# План 06 — MQTT инфраструктура

## Цель
Подключение к брокеру, heartbeat, приём и отправка сообщений по всем топикам.
Остальные режимы используют эту инфраструктуру.

---

## 6.1 Топики

```
GWL:<lamp>/heartbeat   publish каждые 8 сек
GWL:<lamp>/mood        publish/subscribe  uint8 (HSV hue 0–120)
GWL:<lamp>/dnd         publish/subscribe  "0" | "1"
GWL:<lamp>/meeting     publish/subscribe  имя лампы-инициатора
GWL:<lamp>/pomodoro    publish/subscribe  "work:<hue>" | "break" | "idle"
GWL:<lamp>/pir         publish каждые 8 сек вместе с heartbeat
```

Подписка: на все топики всех удалённых ламп: `GWL:<remote>/#`
(или по одной если брокер не поддерживает wildcard).

---

## 6.2 Переменная состояния каждой удалённой лампы

```cpp
struct RemoteLamp {
  char name[20];
  uint32_t lastSeen;     // millis() последнего heartbeat
  uint8_t mood;          // HSV hue
  bool dnd;
  bool pir;
  bool online() const { return (uint32_t)(millis() - lastSeen) < 18000UL; }
};

RemoteLamp remotes[4];   // до 4 удалённых ламп
uint8_t remoteCount = 0;
```

---

## 6.3 Подключение и переподключение

```cpp
// src/mqtt.ino
void mqttTick() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (cfg::offline()) return;  // режим офлайн

  if (!mqtt.connected()) {
    connectMQTT();
    return;
  }
  heartbeatTick();
  mqtt.loop();
}

void connectMQTT() {
  static uint32_t lastAttempt = 0;
  if ((uint32_t)(millis() - lastAttempt) < 5000) return;  // не чаще раза в 5 сек
  lastAttempt = millis();

  String id = "WebLamp-" + String(random(0xffffff), HEX);
  if (!mqtt.connect(id.c_str())) {
    DEBUG(F("[MQTT] connect failed, rc=")); DEBUGLN(mqtt.state());
    return;
  }
  DEBUGLN(F("[MQTT] connected"));

  // подписываемся на топики удалённых ламп
  for (uint8_t i = 0; i < remoteCount; i++) {
    String topic = String("GWL:") + remotes[i].name + "/#";
    mqtt.subscribe(topic.c_str());
    DEBUG(F("[MQTT] subscribed: ")); DEBUGLN(topic);
  }
}
```

---

## 6.4 Heartbeat

```cpp
void heartbeatTick() {
  static Timer hbTmr(8000);
  if (!hbTmr.period()) return;

  String topic = String("GWL:") + cfg::local() + "/heartbeat";
  String payload = USE_PIR ? (pirActive() ? "1" : "0") : "0";
  mqtt.publish(topic.c_str(), payload.c_str(), true);  // retain=true
}
```

---

## 6.5 Callback — разбор входящих сообщений

```cpp
void callback(char* topic, byte* payload, uint16_t len) {
  payload[len] = '\0';
  String t(topic);    // "GWL:<name>/<subtopic>"
  String val((char*)payload);

  // найти отправителя
  int colon = t.indexOf(':');
  int slash  = t.lastIndexOf('/');
  if (colon < 0 || slash < 0) return;

  String sender  = t.substring(colon + 1, slash);
  String subtopic = t.substring(slash + 1);

  // не обрабатывать свои сообщения
  if (sender == cfg::local()) return;

  RemoteLamp* remote = findOrCreateRemote(sender.c_str());
  if (!remote) return;

  if (subtopic == "heartbeat") {
    remote->lastSeen = millis();
    remote->pir = (val == "1");

  } else if (subtopic == "mood") {
    remote->mood = (uint8_t)val.toInt();
    onRemoteMoodUpdate();

  } else if (subtopic == "dnd") {
    remote->dnd = (val == "1");

  } else if (subtopic == "meeting") {
    onMeetingSignal(sender);

  } else if (subtopic == "pomodoro") {
    // используется для DND-сигнала
    remote->dnd = val.startsWith("work");
  }
}
```

---

## 6.6 Публикация состояния

```cpp
void publishMood(uint8_t hue) {
  String topic = String("GWL:") + cfg::local() + "/mood";
  mqtt.publish(topic.c_str(), String(hue).c_str(), true);
}

void publishDND(bool active) {
  String topic = String("GWL:") + cfg::local() + "/dnd";
  mqtt.publish(topic.c_str(), active ? "1" : "0", true);
}

void publishMeeting() {
  String topic = String("GWL:") + cfg::local() + "/meeting";
  mqtt.publish(topic.c_str(), cfg::local().c_str());
}

void publishPomodoro(const char* state) {
  String topic = String("GWL:") + cfg::local() + "/pomodoro";
  mqtt.publish(topic.c_str(), state, true);
}
```

---

## 6.7 Офлайн-режим

Переключатель в веб-интерфейсе: `b.Switch(kk::offline, "Офлайн-режим")`.
При `cfg::offline() == true`:
- `mqttTick()` ничего не делает.
- Входящие сигналы игнорируются.
- Локальные режимы работают.

---

## 6.8 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mqtt.ino` | Переписать: новый протокол, connectMQTT, heartbeat, callback |
| `src/sets.h` | Добавить `kk::offline` в DB_KEYS |
| `src/sets.cpp` | `db.init(kk::offline, false)`, добавить Switch в build() |
| `src/WebLamp_v1.0.ino` | В loop(): добавить `mqttTick()` |

---

## Проверка

- [ ] При включённом WiFi и валидных настройках: подключение к брокеру
- [ ] Serial: `[MQTT] connected`, затем `[MQTT] subscribed: GWL:lamp_2/#`
- [ ] Heartbeat: каждые 8 сек в Serial `[MQTT] heartbeat sent`
- [ ] Два устройства: отправка mood с одного → получение на другом
- [ ] Офлайн-режим: переключатель в UI → MQTT не используется
- [ ] Потеря связи с брокером: автоматическое переподключение через 5 сек
