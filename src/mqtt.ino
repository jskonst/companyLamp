/*
  Групповой MQTT. Форматы — docs/protocol.md, байт в байт.

  Топики лампы (id — 6 hex-символов chip id):
    lamp/<group>/<id>/status   retained, QoS 1: online | away | offline
    lamp/<group>/<id>/state    retained, QoS 1: {"m":75,"n":"Женя"}
    lamp/<group>/<id>/wink     не retained, QoS 0: {"h":96}

  Подписки: lamp/<group>/+/status|state|wink. Свой id игнорируем.
  Client id lamp-<id>, clean session, keepalive 15 с.
  Last Will: offline, retained, QoS 1 — его публикует брокер, своего heartbeat нет.

  Почему не PubSubClient. WiFiClient::connect() в ядре 3.1.2 только шлёт SYN и
  сразу возвращается. Дальше PubSubClient::connect() ждёт CONNACK до
  socketTimeout целых секунд (меньше секунды библиотека задать не умеет).
  Пока брокер недоступен, данных нет, и весь loop стоит это время: кнопка
  теряет серию кликов, лента замирает. AsyncMqttClient::connect() возвращается
  сразу, таймаут TCP живёт в стеке. Единственная короткая блокировка — DNS,
  если адрес ещё не закэширован и кнопка в этот момент свободна
  (MQTT_DNS_TIMEOUT_MS, по умолчанию 500 мс). Повтор — с удвоением паузы до 30 с.
*/

static const char* STATUS_ONLINE = "online";
static const char* STATUS_AWAY = "away";
static const char* STATUS_OFFLINE = "offline";

bool brokerConfigured() {
  return validGroupName(data.group) && data.host[0] != '\0';
}

void rebuildTopics() {
  buildLampTopic(topicStatus, sizeof(topicStatus), data.group, lampId, "status");
  buildLampTopic(topicState, sizeof(topicState), data.group, lampId, "state");
  buildLampTopic(topicWink, sizeof(topicWink), data.group, lampId, "wink");
  buildGroupFilter(subStatus, sizeof(subStatus), data.group, "status");
  buildGroupFilter(subState, sizeof(subState), data.group, "state");
  buildGroupFilter(subWink, sizeof(subWink), data.group, "wink");
}

void peersClear() {
  memset(peers, 0, sizeof(peers));
  peerCount = 0;
  peersDirty = true;
}

void peersDropOnline() {
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].status == PeerStatus::Online) peers[i].status = PeerStatus::Offline;
  }
  peersDirty = true;
}

void rebuildPeerText() {
  int others = 0;
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].status == PeerStatus::Online) others++;
  }
  bool selfOn = !data.autonomous && mqtt.connected();
  onlineCount = others + (selfOn ? 1 : 0);

  const char* selfWord = selfOn ? STATUS_ONLINE : (data.autonomous ? STATUS_AWAY : STATUS_OFFLINE);
  char* cursor = peerText;
  size_t left = sizeof(peerText);
  int wrote = snprintf(cursor, left, "онлайн %d (мы %s)\n", onlineCount, selfWord);
  if (wrote < 0 || (size_t)wrote >= left) {
    peerText[sizeof(peerText) - 1] = '\0';
    peersDirty = false;
    return;
  }
  cursor += wrote;
  left -= (size_t)wrote;

  for (int i = 0; i < peerCount && left > 1; i++) {
    const char* word = STATUS_OFFLINE;
    if (peers[i].status == PeerStatus::Online) word = STATUS_ONLINE;
    else if (peers[i].status == PeerStatus::Away) word = STATUS_AWAY;
    else if (peers[i].status == PeerStatus::None) word = "?";
    const char* name = peers[i].name[0] ? peers[i].name : "-";
    if (peers[i].hasMood) {
      wrote = snprintf(cursor, left, "%s %s m=%u %s\n", peers[i].id, name, peers[i].mood, word);
    } else {
      wrote = snprintf(cursor, left, "%s %s %s\n", peers[i].id, name, word);
    }
    if (wrote < 0 || (size_t)wrote >= left) break;
    cursor += wrote;
    left -= (size_t)wrote;
  }
  peersDirty = false;
}

int upsertPeer(const char* id) {
  char canon[7];
  canonLampId(canon, id);
  for (int i = 0; i < peerCount; i++) {
    if (sameLampId(peers[i].id, canon)) return i;
  }
  int slot = -1;
  if (peerCount < PEER_MAX) {
    slot = peerCount++;
  } else {
    for (int i = 0; i < PEER_MAX; i++) {
      if (peers[i].status != PeerStatus::Online) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) return -1;
  memset(&peers[slot], 0, sizeof(Peer));
  memcpy(peers[slot].id, canon, 7);
  peers[slot].status = PeerStatus::None;
  return slot;
}

void rememberStatus(const char* id, PeerStatus status) {
  if (status == PeerStatus::None) return;
  int slot = upsertPeer(id);
  if (slot < 0) return;
  peers[slot].status = status;
  peersDirty = true;
}

void rememberState(const char* id, uint8_t mood, const char* name) {
  int slot = upsertPeer(id);
  if (slot < 0) return;
  peers[slot].mood = mood;
  peers[slot].hasMood = true;
  if (name) {
    strncpy(peers[slot].name, name, sizeof(peers[slot].name) - 1);
    peers[slot].name[sizeof(peers[slot].name) - 1] = '\0';
  }
  peersDirty = true;
}

bool publishText(const char* topic, uint8_t qos, bool retain, const char* payload) {
  if (!mqtt.connected() || !payload) return false;
  uint16_t id = mqtt.publish(topic, qos, retain, payload);
  return id != 0 || qos == 0;
}

void publishOnline() {
  publishText(topicStatus, 1, true, STATUS_ONLINE);
}

void publishState() {
  if (data.autonomous || !mqtt.connected()) return;
  char body[96];
  if (formatStatePayload(body, sizeof(body), data.mood, data.owner) < 0) return;
  publishText(topicState, 1, true, body);
}

void publishWink() {
  // Автономная лампа не шлёт подмигивания. Своё кольцо тоже не мигает:
  // вспышка — сигнал остальным, их цвет она не меняет.
  if (data.autonomous || !mqtt.connected()) return;
  char body[16];
  if (formatWinkPayload(body, sizeof(body), moodToHue(data.mood)) < 0) return;
  publishText(topicWink, 0, false, body);
}

void beginAway() {
  uint16_t id = mqtt.publish(topicStatus, 1, true, STATUS_AWAY);
  if (!id) {
    mqtt.disconnect(true);
    awayAnnounced = false;
    awayWait = false;
    return;
  }
  awayPkt = id;
  awayWait = true;
  awayWaitSince = millis();
}

void setAutonomous(bool on) {
  if (data.autonomous == on) {
    memory.update();
    return;
  }
  data.autonomous = on;
  memory.update();
  awayAnnounced = false;
  awayWait = false;
  mqttNextTry = millis();
  mqttBackoff = MQTT_BACKOFF_START_MS;
  peersDirty = true;
  if (on) {
    if (mqtt.connected()) beginAway();
    return;
  }
  // Выход: если сокет ещё жив (не успели уйти), сразу online и state.
  if (mqtt.connected()) {
    mqtt.subscribe(subStatus, 1);
    mqtt.subscribe(subState, 1);
    mqtt.subscribe(subWink, 0);
    publishOnline();
    publishState();
  }
}

void armMqtt() {
  uint16_t port = data.port ? data.port : 1883;
  const char* user = data.mqttUser[0] ? data.mqttUser : nullptr;
  const char* pass = (user && data.mqttPass[0]) ? data.mqttPass : nullptr;
  mqtt.setServer(mqttIP, port);
  mqtt.setClientId(clientId);
  mqtt.setCredentials(user, pass);
  mqtt.setWill(topicStatus, 1, true, STATUS_OFFLINE);
}

void scheduleMqttBackoff() {
  mqttNextTry = millis() + mqttBackoff;
  if (mqttBackoff < MQTT_BACKOFF_MAX_MS) {
    uint32_t next = (uint32_t)mqttBackoff * 2;
    if (next > MQTT_BACKOFF_MAX_MS) next = MQTT_BACKOFF_MAX_MS;
    mqttBackoff = (uint16_t)next;
  }
  if (mqttFailStreak < 250) mqttFailStreak++;
  if (mqttFailStreak >= 3) mqttHasIP = false;  // вдруг брокер переехал
}

// 1 — адрес есть, 0 — DNS не ответил, 2 — кнопка в серии кликов, DNS не трогаем.
int ensureMqttIP() {
  IPAddress ip;
  if (ip.fromString(data.host)) {
    mqttIP = ip;
    mqttHasIP = true;
    return 1;
  }
  if (mqttHasIP && (uint32_t)(millis() - mqttResolvedAt) < 10ul * 60ul * 1000ul) return 1;
  if (btn.busy() || btn.waiting() || btn.pressing()) return 2;
  IPAddress resolved;
  if (!WiFi.hostByName(data.host, resolved, MQTT_DNS_TIMEOUT_MS)) {
    DEBUG(F("DNS fail "));
    DEBUGLN(data.host);
    return 0;
  }
  mqttIP = resolved;
  mqttHasIP = true;
  mqttResolvedAt = millis();
  DEBUG(F("MQTT ip "));
  DEBUGLN(mqttIP);
  return 1;
}

void mqttOnWifiUp() {
  mqttNextTry = millis();
  mqttBackoff = MQTT_BACKOFF_START_MS;
}

void mqttRestart() {
  rebuildTopics();
  mqttHasIP = false;
  mqttResolvedAt = 0;
  awayAnnounced = false;
  awayWait = false;
  mqttFailStreak = 0;
  mqttBackoff = MQTT_BACKOFF_START_MS;
  mqttNextTry = millis();
  peersClear();
  // Грязный обрыв: брокер сам поставит offline (Last Will) на старом топике.
  if (mqtt.connected() || mqttConnecting) {
    mqttConnecting = false;
    mqttHoldBackoff = true;
    mqtt.disconnect(true);
    mqttHoldBackoff = false;
  }
}

void onMqttConnect(bool sessionPresent) {
  (void)sessionPresent;
  mqttConnecting = false;
  mqttFailStreak = 0;
  mqttBackoff = MQTT_BACKOFF_START_MS;
  DEBUG(F("MQTT connected, heap "));
  DEBUGLN(ESP.getFreeHeap());
  if (data.autonomous) {
    beginAway();
    return;
  }
  mqtt.subscribe(subStatus, 1);
  mqtt.subscribe(subState, 1);
  mqtt.subscribe(subWink, 0);
  publishOnline();
  publishState();
  peersDirty = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  mqttConnecting = false;
  awayWait = false;
  DEBUG(F("MQTT down "));
  DEBUGLN((int)reason);
  // Ожидаемый уход в away: чужие статусы не обнуляем, просто больше не слушаем.
  if (data.autonomous && awayAnnounced) {
    peersDirty = true;
    return;
  }
  peersDropOnline();
  if (mqttHoldBackoff) return;
  scheduleMqttBackoff();
}

void onMqttPublish(uint16_t packetId) {
  if (!awayWait || packetId != awayPkt) return;
  awayWait = false;
  if (!data.autonomous) return;
  // PUBACK на retained away: теперь чистое DISCONNECT, чтобы Last Will
  // не затёр away словом offline.
  awayAnnounced = true;
  mqtt.disconnect(false);
  peersDirty = true;
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  (void)properties;
  if (index != 0 || len != total || total >= 160 || !topic) return;

  char top[80];
  strncpy(top, topic, sizeof(top) - 1);
  top[sizeof(top) - 1] = '\0';

  TopicParts parts;
  if (!parseLampTopic(top, &parts)) return;
  if (strcmp(parts.group, data.group) != 0) return;
  if (sameLampId(parts.id, lampId)) return;

  char body[160];
  memcpy(body, payload, len);
  body[len] = '\0';

  if (strcmp(parts.leaf, "status") == 0) {
    rememberStatus(parts.id, parseStatusPayload(body));
  } else if (strcmp(parts.leaf, "state") == 0) {
    uint8_t mood = 0;
    char name[32];
    if (!parseStatePayload(body, &mood, name, sizeof(name))) return;
    rememberState(parts.id, mood, name);
  } else if (strcmp(parts.leaf, "wink") == 0) {
    if (data.autonomous) return;
    uint8_t hue = 0;
    if (!parseWinkPayload(body, &hue)) return;
    winkFrom(hue);
  }
}

void mqttBegin() {
  formatChipId(ESP.getChipId(), lampId);
  buildClientId(clientId, sizeof(clientId), lampId);
  snprintf(deviceHost, sizeof(deviceHost), "companylamp-%s", lampId);
  rebuildTopics();

  mqtt.setCleanSession(true);
  mqtt.setKeepAlive(15);
  mqtt.setMaxTopicLength(80);
  mqtt.onConnect(onMqttConnect);
  mqtt.onDisconnect(onMqttDisconnect);
  mqtt.onMessage(onMqttMessage);
  mqtt.onPublish(onMqttPublish);

  // Уже были автономны до выключения: после Wi-Fi ещё раз скажем away.
  awayAnnounced = false;
  DEBUG(F("id "));
  DEBUGLN(lampId);
}

void mqttTick() {
  static uint32_t peerShown = 0;
  if (peersDirty || (uint32_t)(millis() - peerShown) > 1000) {
    rebuildPeerText();
    peerShown = millis();
  }

  if (!brokerConfigured()) {
    if (mqtt.connected() || mqttConnecting) {
      mqttConnecting = false;
      mqtt.disconnect(true);
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (mqtt.connected() || mqttConnecting) {
      mqttConnecting = false;
      mqtt.disconnect(true);
    }
    return;
  }

  if (awayWait && (uint32_t)(millis() - awayWaitSince) > 3000) {
    // PUBACK не пришёл. Пакет уже стоял в очереди перед DISCONNECT,
    // брокер обычно успевает записать retained away.
    awayWait = false;
    if (data.autonomous) {
      awayAnnounced = true;
      mqtt.disconnect(false);
    }
    return;
  }

  if (data.autonomous && awayAnnounced && !mqtt.connected() && !mqttConnecting) return;

  if (mqttConnecting && (uint32_t)(millis() - mqttConnectStarted) > 20000) {
    mqtt.disconnect(true);
    mqttConnecting = false;
    return;
  }

  if (mqtt.connected() || mqttConnecting || awayWait) return;
  if ((int32_t)(millis() - mqttNextTry) < 0) return;

  int resolved = ensureMqttIP();
  if (resolved == 2) {
    mqttNextTry = millis() + 250;
    return;
  }
  if (resolved == 0) {
    scheduleMqttBackoff();
    return;
  }

  armMqtt();
  mqttConnecting = true;
  mqttConnectStarted = millis();
  DEBUG(F("MQTT connect "));
  DEBUGLN(data.host);
  mqtt.connect();
}
