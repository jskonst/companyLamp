#include "mqtt.h"
#include "sets.h"

extern "C" {
#include <lwip/tcp.h>
#include <lwip/dns.h>
}

// Неблокирующий TCP: connect() PubSubClient при лежащем брокере иначе держит loop
// на таймауте сокета. DNS и SYN идут с возвратом в loop, кнопка и лента не ждут.
class NbClient : public Client {
 public:
  bool begin(const char* host, uint16_t port) {
    stop();
    if (!host || !host[0] || port == 0) {
      phase = Phase::Failed;
      return false;
    }
    this->port = port;
    strncpy(hostBuf, host, sizeof(hostBuf) - 1);
    hostBuf[sizeof(hostBuf) - 1] = '\0';
    dnsEpoch = epoch;
    phase = Phase::Resolving;
    started = millis();

    IPAddress literal;
    if (literal.fromString(hostBuf)) {
      ip_addr_t addr;
      IP_ADDR4(&addr, literal[0], literal[1], literal[2], literal[3]);
      startTcp(addr);
      return phase == Phase::Connecting || phase == Phase::Up;
    }

    ip_addr_t resolved;
    err_t err = dns_gethostbyname(hostBuf, &resolved, &NbClient::onDns, this);
    if (err == ERR_OK) {
      if (phase == Phase::Resolving) startTcp(resolved);
      return phase == Phase::Connecting || phase == Phase::Up;
    }
    if (err == ERR_INPROGRESS) return true;
    phase = Phase::Failed;
    return false;
  }

  bool isIdle() const { return phase == Phase::Idle; }
  bool isUp() const { return phase == Phase::Up && pcb; }
  bool isPending() const { return phase == Phase::Resolving || phase == Phase::Connecting; }
  bool isFailed() const { return phase == Phase::Failed; }
  bool timedOut() const {
    if (!isPending()) return false;
    uint32_t limit = (phase == Phase::Resolving) ? 500UL : 5000UL;
    return (uint32_t)(millis() - started) > limit;
  }

  // Пока PubSubClient ждёт CONNACK, available() крутится без yield. Отдаём квант
  // кнопке и ленте, но не чаще 10 мс: иначе strip и EncButton получают тысячи тиков.
  void setAppPump(bool on) { pumpApps = on; }

  int connect(IPAddress, uint16_t) override { return isUp() ? 1 : 0; }
  int connect(const char*, uint16_t) override { return isUp() ? 1 : 0; }

  size_t write(uint8_t b) override { return write(&b, 1); }

  size_t write(const uint8_t* data, size_t size) override {
    if (!isUp() || !data || !size) return 0;
    size_t sent = 0;
    uint32_t t0 = millis();
    while (sent < size) {
      size_t space = tcp_sndbuf(pcb);
      if (!space) {
        tcp_output(pcb);
        yield();
        if ((uint32_t)(millis() - t0) > 30) break;
        continue;
      }
      size_t n = size - sent;
      if (n > space) n = space;
      if (tcp_write(pcb, data + sent, n, TCP_WRITE_FLAG_COPY) != ERR_OK) break;
      sent += n;
    }
    if (pcb) tcp_output(pcb);
    return sent;
  }

  int available() override {
    if (pumpApps && !inPump) {
      uint32_t now = millis();
      if ((uint32_t)(now - lastPump) >= 10) {
        lastPump = now;
        inPump = true;
        yield();
        buttonTick();
        renderTick();
        otaTick();
        inPump = false;
      } else {
        yield();
      }
    }
    return (int)rxCount;
  }

  int read() override {
    if (!rxCount) return -1;
    uint8_t b = rx[rxR];
    rxR = (uint16_t)((rxR + 1) % RX_CAP);
    rxCount--;
    return b;
  }

  int read(uint8_t* buf, size_t size) override {
    size_t n = 0;
    while (n < size) {
      int b = read();
      if (b < 0) break;
      buf[n++] = (uint8_t)b;
    }
    return (int)n;
  }

  int peek() override { return rxCount ? rx[rxR] : -1; }

  void flush() override {
    if (!pcb) return;
    uint32_t t0 = millis();
    while (tcp_sndbuf(pcb) < TCP_SND_BUF && (uint32_t)(millis() - t0) < 40) {
      tcp_output(pcb);
      yield();
    }
    tcp_output(pcb);
  }

  void stop() override {
    epoch++;
    pumpApps = false;
    if (pcb) {
      tcp_pcb* dying = pcb;
      pcb = nullptr;
      tcp_arg(dying, nullptr);
      tcp_recv(dying, nullptr);
      tcp_err(dying, nullptr);
      tcp_sent(dying, nullptr);
      if (tcp_close(dying) != ERR_OK) tcp_abort(dying);
    }
    rxR = rxW = 0;
    rxCount = 0;
    phase = Phase::Idle;
  }

  uint8_t connected() override {
    return (phase == Phase::Up && pcb && pcb->state == ESTABLISHED) ? 1 : 0;
  }

  operator bool() override { return connected(); }

 private:
  enum class Phase : uint8_t { Idle, Resolving, Connecting, Up, Failed };

  static constexpr uint16_t RX_CAP = 512;

  tcp_pcb* pcb = nullptr;
  Phase phase = Phase::Idle;
  uint16_t port = 0;
  uint32_t started = 0;
  uint8_t epoch = 1;
  uint8_t dnsEpoch = 0;
  char hostBuf[64] = {};
  uint8_t rx[RX_CAP] = {};
  uint16_t rxR = 0;
  uint16_t rxW = 0;
  uint16_t rxCount = 0;
  bool pumpApps = false;
  bool inPump = false;
  uint32_t lastPump = 0;

  void startTcp(const ip_addr_t& addr) {
    pcb = tcp_new();
    if (!pcb) {
      phase = Phase::Failed;
      return;
    }
    tcp_arg(pcb, this);
    tcp_recv(pcb, &NbClient::onRecv);
    tcp_err(pcb, &NbClient::onErr);
    tcp_nagle_disable(pcb);
    ip_addr_t copy = addr;
    if (tcp_connect(pcb, &copy, port, &NbClient::onConnected) != ERR_OK) {
      tcp_abort(pcb);
      pcb = nullptr;
      phase = Phase::Failed;
      return;
    }
    phase = Phase::Connecting;
    started = millis();
  }

  size_t pushRx(pbuf* p) {
    size_t accepted = 0;
    for (pbuf* q = p; q; q = q->next) {
      auto* data = static_cast<const uint8_t*>(q->payload);
      for (uint16_t i = 0; i < q->len; i++) {
        if (rxCount >= RX_CAP) return accepted;
        rx[rxW] = data[i];
        rxW = (uint16_t)((rxW + 1) % RX_CAP);
        rxCount++;
        accepted++;
      }
    }
    return accepted;
  }

  static void onDns(const char*, const ip_addr_t* ipaddr, void* arg) {
    auto* self = static_cast<NbClient*>(arg);
    if (!self || self->epoch != self->dnsEpoch || self->phase != Phase::Resolving) return;
    if (!ipaddr) {
      self->phase = Phase::Failed;
      return;
    }
    self->startTcp(*ipaddr);
  }

  static err_t onConnected(void* arg, tcp_pcb*, err_t err) {
    auto* self = static_cast<NbClient*>(arg);
    if (!self) return ERR_OK;
    self->phase = (err == ERR_OK) ? Phase::Up : Phase::Failed;
    return ERR_OK;
  }

  static void onErr(void* arg, err_t) {
    auto* self = static_cast<NbClient*>(arg);
    if (!self) return;
    self->pcb = nullptr;
    self->phase = Phase::Failed;
  }

  static err_t onRecv(void* arg, tcp_pcb* tpcb, pbuf* p, err_t) {
    auto* self = static_cast<NbClient*>(arg);
    if (!self) {
      if (p) pbuf_free(p);
      return ERR_OK;
    }
    if (!p) {
      self->phase = Phase::Failed;
      return ERR_OK;
    }
    const size_t total = p->tot_len;
    size_t n = self->pushRx(p);
    if (n) tcp_recved(tpcb, n);
    pbuf_free(p);
    if (n < total) {
      self->phase = Phase::Failed;
      self->pcb = nullptr;
      tcp_abort(tpcb);
      return ERR_ABRT;
    }
    return ERR_OK;
  }
};

static NbClient net;
PubSubClient mqtt(net);

static RemoteLamp remotes[MAX_REMOTE_LAMPS];
static uint8_t    remoteCount = 0;

static char idBuf[8] = {};
static char clientBuf[16] = {};

const char* lampId() {
  if (!idBuf[0]) snprintf(idBuf, sizeof(idBuf), "%06x", ESP.getChipId() & 0xFFFFFFu);
  return idBuf;
}

static const char* mqttClientId() {
  if (!clientBuf[0]) snprintf(clientBuf, sizeof(clientBuf), "lamp-%s", lampId());
  return clientBuf;
}

static bool topicFor(const char* sub, char* out, size_t n) {
  const String group = cfg::group();
  if (!group.length() || !sub || !sub[0]) return false;
  int w = snprintf(out, n, "lamp/%s/%s/%s", group.c_str(), lampId(), sub);
  return w > 0 && (size_t)w < n;
}

static RemoteLamp* findOrCreateRemote(const char* name) {
  for (uint8_t i = 0; i < remoteCount; i++)
    if (strncmp(remotes[i].name, name, sizeof(remotes[i].name)) == 0)
      return &remotes[i];
  if (remoteCount >= MAX_REMOTE_LAMPS) return nullptr;
  RemoteLamp& r = remotes[remoteCount++];
  strncpy(r.name, name, sizeof(r.name) - 1);
  r.name[sizeof(r.name) - 1] = '\0';
  r.lastSeen = 0;
  r.mood = 0;
  r.dnd = false;
  r.pir = false;
  r.presence = Presence::Unknown;
  return &r;
}

String mqttStatusString() {
  if (cfg::offline()) return "Офлайн-режим";
  if (!mqtt.connected()) return "MQTT: нет подключения";
  String s = "Группа: " + cfg::group();
  s += "\nID: ";
  s += lampId();
  if (remoteCount == 0) {
    s += "\n(нет ламп в группе)";
  } else {
    for (uint8_t i = 0; i < remoteCount; i++) {
      s += '\n';
      s += remotes[i].online() ? "\u25cf " : "\u25cb ";
      s += remotes[i].name;
    }
  }
  return s;
}

static char meetingSender[20] = {};
static bool meetingPending    = false;

bool takeMeetingSignal(String& sender) {
  if (!meetingPending) return false;
  meetingPending = false;
  sender = meetingSender;
  return true;
}

// lamp/<group>/<id>/<sub> — отправитель это сегмент id, не текст между первым и последним '/'.
static bool parseLampTopic(const String& topic, String& sender, String& subtopic) {
  if (!topic.startsWith(F("lamp/"))) return false;
  int s1 = topic.indexOf('/');
  int s2 = topic.indexOf('/', s1 + 1);
  int s3 = topic.indexOf('/', s2 + 1);
  if (s1 < 0 || s2 < 0 || s3 < 0) return false;
  String group = topic.substring(s1 + 1, s2);
  if (group != cfg::group()) return false;
  sender = topic.substring(s2 + 1, s3);
  subtopic = topic.substring(s3 + 1);
  return sender.length() && subtopic.length();
}

void callback(char* topic, byte* payload, uint16_t len) {
  if (len >= 128) return;
  char buf[128];
  if (len) memcpy(buf, payload, len);
  buf[len] = '\0';
  String val(buf);

  String sender;
  String subtopic;
  if (!parseLampTopic(String(topic), sender, subtopic)) return;
  if (sender == lampId()) return;

  RemoteLamp* remote = findOrCreateRemote(sender.c_str());
  if (!remote) return;

  if (subtopic == "heartbeat") {
    remote->lastSeen = millis();
    remote->pir = (val == "1");
  } else if (subtopic == "status") {
    if (val == "online") remote->presence = Presence::Online;
    else if (val == "away") remote->presence = Presence::Away;
    else if (val == "offline") remote->presence = Presence::Offline;
  } else if (subtopic == "mood") {
    remote->mood = (uint8_t)constrain(val.toInt(), 0, 120);
  } else if (subtopic == "dnd") {
    remote->dnd = (val == "1");
  } else if (subtopic == "pomodoro") {
    remote->dnd = val.startsWith("work");
  } else if (subtopic == "meeting") {
    strncpy(meetingSender, sender.c_str(), sizeof(meetingSender) - 1);
    meetingSender[sizeof(meetingSender) - 1] = '\0';
    meetingPending = true;
  }
}

static uint16_t qos1PacketId = 1;

// status: retained, QoS 1. PubSubClient::publish шлёт только QoS 0, пакет собирается здесь.
// PUBACK забирает mqtt.loop() и игнорирует (тип 0x40), поток не разъезжается.
static bool publishQos1(const char* topic, const char* payload, bool retain) {
  if (!mqtt.connected() || !topic) return false;
  uint16_t tlen = (uint16_t)strlen(topic);
  uint16_t plen = payload ? (uint16_t)strlen(payload) : 0;
  uint32_t rem = 2u + tlen + 2u + plen;
  if (rem > 240) return false;

  uint8_t pkt[256];
  uint16_t n = 0;
  pkt[n++] = (uint8_t)(0x30 | 0x02 | (retain ? 0x01 : 0));
  if (rem < 128) {
    pkt[n++] = (uint8_t)rem;
  } else {
    pkt[n++] = (uint8_t)((rem & 0x7F) | 0x80);
    pkt[n++] = (uint8_t)(rem >> 7);
  }
  pkt[n++] = (uint8_t)(tlen >> 8);
  pkt[n++] = (uint8_t)(tlen & 0xFF);
  memcpy(pkt + n, topic, tlen);
  n = (uint16_t)(n + tlen);
  uint16_t pid = qos1PacketId++;
  if (qos1PacketId == 0) qos1PacketId = 1;
  pkt[n++] = (uint8_t)(pid >> 8);
  pkt[n++] = (uint8_t)(pid & 0xFF);
  if (plen) {
    memcpy(pkt + n, payload, plen);
    n = (uint16_t)(n + plen);
  }
  return net.write(pkt, n) == n;
}

static bool publishStatus(const char* payload) {
  char topic[128];
  if (!topicFor("status", topic, sizeof(topic))) return false;
  bool ok = publishQos1(topic, payload, true);
  net.flush();
  DEBUG(F("[MQTT] status "));
  DEBUGLN(payload);
  return ok;
}

static void heartbeatTick() {
  static Timer hbTmr(8000);
  if (!hbTmr.period()) return;

  char topic[128];
  if (!topicFor("heartbeat", topic, sizeof(topic))) return;
#if USE_PIR
  const char* payload = pirActive() ? "1" : "0";
#else
  const char* payload = "0";
#endif
  mqtt.publish(topic, payload, true);
  DEBUGLN(F("[MQTT] heartbeat sent"));
}

static uint32_t nextAttempt = 0;
static uint32_t backoffMs = 2000;
static bool awayAnnounced = false;

static void armBackoff() {
  nextAttempt = millis() + backoffMs;
  backoffMs = (backoffMs < 15000UL) ? backoffMs * 2 : 30000UL;
  if (backoffMs > 30000UL) backoffMs = 30000UL;
}

static bool handshake() {
  static char willTopic[128];
  if (!topicFor("status", willTopic, sizeof(willTopic))) return false;

  String host = cfg::host();
  static char hostCopy[64];
  strncpy(hostCopy, host.c_str(), sizeof(hostCopy) - 1);
  hostCopy[sizeof(hostCopy) - 1] = '\0';
  mqtt.setServer(hostCopy, cfg::port());
  mqtt.setSocketTimeout(1);

  String user = cfg::mqttUser();
  String pass = cfg::mqttPass();
  net.setAppPump(true);
  bool ok = user.length()
      ? mqtt.connect(mqttClientId(), user.c_str(), pass.c_str(), willTopic, 1, true, "offline")
      : mqtt.connect(mqttClientId(), willTopic, 1, true, "offline");
  net.setAppPump(false);
  return ok;
}

static void subscribeGroup() {
  String topic = String(F("lamp/")) + cfg::group() + F("/+/#");
  mqtt.subscribe(topic.c_str());
  DEBUG(F("[MQTT] subscribed: "));
  DEBUGLN(topic);
}

void mqttBegin() {
  mqtt.setCallback(callback);
  mqtt.setKeepAlive(15);
  mqtt.setSocketTimeout(1);
  mqtt.setBufferSize(512);
}

void mqttTick() {
  const bool offline = cfg::offline();
  if (!offline) awayAnnounced = false;

  if (WiFi.status() != WL_CONNECTED) {
    // Сессия рвётся без DISCONNECT, чтобы брокер опубликовал LWT offline.
    if (!net.isIdle()) {
      net.stop();
      mqtt.connected();
    }
    return;
  }

  if (offline && awayAnnounced) {
    if (mqtt.connected()) mqtt.disconnect();
    return;
  }

  if (!cfg::group().length() || !cfg::host().length()) return;

  if (mqtt.connected()) {
    if (offline) {
      publishStatus("away");
      mqtt.disconnect();
      awayAnnounced = true;
      return;
    }
    heartbeatTick();
    mqtt.loop();
    return;
  }

  if (net.isPending()) {
    if (net.timedOut()) {
      net.stop();
      armBackoff();
    }
    return;
  }
  if (btn.busy()) return;
  if ((int32_t)(millis() - nextAttempt) < 0) return;

  if (!net.isUp()) {
    if (net.isFailed()) {
      net.stop();
      armBackoff();
      return;
    }
    if (!net.begin(cfg::host().c_str(), cfg::port())) armBackoff();
    return;
  }

  if (!handshake()) {
    DEBUGLN(F("[MQTT] connect failed"));
    net.stop();
    armBackoff();
    return;
  }

  DEBUGLN(F("[MQTT] connected"));
  backoffMs = 2000;
  nextAttempt = 0;
  subscribeGroup();
  if (offline) {
    publishStatus("away");
    mqtt.disconnect();
    awayAnnounced = true;
  } else {
    publishStatus("online");
  }
}

uint8_t mqttRemoteCount() { return remoteCount; }
const RemoteLamp& mqttRemote(uint8_t i) { return remotes[i]; }

static bool publishSub(const char* sub, const char* payload, bool retain) {
  if (!mqtt.connected() || cfg::offline()) return false;
  char topic[128];
  if (!topicFor(sub, topic, sizeof(topic))) return false;
  return mqtt.publish(topic, payload, retain);
}

void publishMood(uint8_t hue) {
  char payload[8];
  snprintf(payload, sizeof(payload), "%u", (unsigned)hue);
  publishSub("mood", payload, true);
}

void publishDND(bool active) {
  publishSub("dnd", active ? "1" : "0", true);
}

void publishMeeting() {
  publishSub("meeting", cfg::local().c_str(), false);
}

void publishPomodoro(const char* state) {
  publishSub("pomodoro", state, true);
}

#if USE_PIR
static uint32_t pirHoldUntil = 0;

bool pirActive() {
  if (digitalRead(PIR_PIN)) pirHoldUntil = millis() + 60000UL;
  return (int32_t)(pirHoldUntil - millis()) > 0;
}
#endif
