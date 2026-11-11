# План 06 — MQTT инфраструктура

## Цель
Подключение к брокеру, heartbeat, приём и отправка сообщений по всем топикам.
Остальные режимы используют эту инфраструктуру.

---

## 6.1 Топики

Структура топика: `GWL/<group>/<lamp>/<subtopic>`

```
GWL/<group>/<lamp>/heartbeat   publish каждые 8 сек
GWL/<group>/<lamp>/mood        publish/subscribe  uint8 (HSV hue 0–120)
GWL/<group>/<lamp>/dnd         publish/subscribe  "0" | "1"
GWL/<group>/<lamp>/meeting     publish/subscribe  имя лампы-инициатора
GWL/<group>/<lamp>/pomodoro    publish/subscribe  "work:<hue>" | "break" | "idle"
```

`<group>` — название группы (например `office`, `family`), задаётся в настройках UI.
Подписка на все лампы группы: `GWL/<group>/+/#` (wildcard по имени лампы).
Своё сообщение отфильтровывается в callback по `sender == cfg::local()`.

> **Будущее:** несколько групп и переключение между ними — см. PLAN_11_GROUPS.

---

## 6.2 Переменная состояния каждой удалённой лампы

Лампы обнаруживаются автоматически: первый heartbeat от нового имени создаёт запись.

```cpp
struct RemoteLamp {
  char name[20];
  uint32_t lastSeen;     // millis() последнего heartbeat
  uint8_t mood;          // HSV hue
  bool dnd;
  bool pir;
  bool online() const { return (uint32_t)(millis() - lastSeen) < 18000UL; }
};

RemoteLamp remotes[20];  // до 20 удалённых ламп в группе
uint8_t remoteCount = 0;

RemoteLamp* findOrCreateRemote(const char* name) {
  for (uint8_t i = 0; i < remoteCount; i++)
    if (strcmp(remotes[i].name, name) == 0) return &remotes[i];
  if (remoteCount >= 20) return nullptr;
  RemoteLamp& r = remotes[remoteCount++];
  strncpy(r.name, name, sizeof(r.name) - 1);
  r.lastSeen = 0; r.mood = 0; r.dnd = false; r.pir = false;
  return &r;
}
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
  String user = cfg::mqttUser();
  String pass = cfg::mqttPass();
  bool ok = user.length()
    ? mqtt.connect(id.c_str(), user.c_str(), pass.c_str())
    : mqtt.connect(id.c_str());
  if (!ok) {
    DEBUG(F("[MQTT] connect failed, rc=")); DEBUGLN(mqtt.state());
    return;
  }
  DEBUGLN(F("[MQTT] connected"));

  // подписываемся на все лампы своей группы
  String topic = String("GWL/") + cfg::group() + "/+/#";
  mqtt.subscribe(topic.c_str());
  DEBUG(F("[MQTT] subscribed: ")); DEBUGLN(topic);
}
```

---

## 6.4 Heartbeat

```cpp
void heartbeatTick() {
  static Timer hbTmr(8000);
  if (!hbTmr.period()) return;

  String topic = String("GWL/") + cfg::group() + "/" + cfg::local() + "/heartbeat";
  String payload = USE_PIR ? (pirActive() ? "1" : "0") : "0";
  mqtt.publish(topic.c_str(), payload.c_str(), true);  // retain=true
}
```

---

## 6.5 Callback — разбор входящих сообщений

```cpp
void callback(char* topic, byte* payload, uint16_t len) {
  payload[len] = '\0';
  String t(topic);    // "GWL/<group>/<name>/<subtopic>"
  String val((char*)payload);

  // разбираем: GWL/<group>/<lamp>/<subtopic>
  // пропускаем "GWL/<group>/"
  int p1 = t.indexOf('/');           // после "GWL"
  int p2 = t.indexOf('/', p1 + 1);  // после group
  int p3 = t.lastIndexOf('/');       // перед subtopic
  if (p1 < 0 || p2 < 0 || p3 <= p2) return;

  String sender   = t.substring(p2 + 1, p3);
  String subtopic = t.substring(p3 + 1);

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
  String topic = String("GWL/") + cfg::group() + "/" + cfg::local() + "/mood";
  mqtt.publish(topic.c_str(), String(hue).c_str(), true);
}

void publishDND(bool active) {
  String topic = String("GWL/") + cfg::group() + "/" + cfg::local() + "/dnd";
  mqtt.publish(topic.c_str(), active ? "1" : "0", true);
}

void publishMeeting() {
  String topic = String("GWL/") + cfg::group() + "/" + cfg::local() + "/meeting";
  mqtt.publish(topic.c_str(), cfg::local().c_str());
}

void publishPomodoro(const char* state) {
  String topic = String("GWL/") + cfg::group() + "/" + cfg::local() + "/pomodoro";
  mqtt.publish(topic.c_str(), state, true);
}
```

---

## 6.7 Статус группы в UI

Кнопка «Обновить» в веб-интерфейсе — по нажатию рендерится текущий `remotes[]`.

```cpp
// в build() секции MQTT
b.Label("", mqttStatusString().c_str());
b.Button("", "Обновить");  // перезагружает страницу, Label перерисуется

String mqttStatusString() {
  if (!mqtt.connected()) return "MQTT: нет подключения";
  String s = "Группа: " + cfg::group() + "\n";
  for (uint8_t i = 0; i < remoteCount; i++) {
    s += remotes[i].online() ? "● " : "○ ";
    s += remotes[i].name;
    s += "\n";
  }
  if (remoteCount == 0) s += "(нет ламп в группе)";
  return s;
}
```

---

## 6.8 Офлайн-режим

Переключатель в веб-интерфейсе: `b.Switch(kk::offline, "Офлайн-режим")`.
При `cfg::offline() == true`:
- `mqttTick()` ничего не делает.
- Входящие сигналы игнорируются.
- Локальные режимы работают.

---

## 6.8 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mqtt.ino` | Переписать: новый протокол, connectMQTT, heartbeat, callback, findOrCreateRemote |
| `src/sets.h` | Добавить `kk::offline`, `kk::group`, `kk::mqtt_user`, `kk::mqtt_pass` в DB_KEYS |
| `src/sets.cpp` | Добавить init и поля в buildWifi(): Input логин, Pass пароль брокера, Input группа, Switch офлайн |
| `src/WebLamp_v1.0.ino` | В loop(): добавить `mqttTick()` |
| `src/sets.cpp` | В build(): добавить Label + кнопку «Обновить» для статуса группы |

---

## Проверка

- [ ] При включённом WiFi и валидных настройках: подключение к брокеру
- [ ] Serial: `[MQTT] connected`, затем `[MQTT] subscribed: GWL/office/+/#`
- [ ] Heartbeat: каждые 8 сек в Serial `[MQTT] heartbeat sent`
- [ ] Два устройства: отправка mood с одного → получение на другом
- [ ] Офлайн-режим: переключатель в UI → MQTT не используется
- [ ] Потеря связи с брокером: автоматическое переподключение через 5 сек
- [ ] Кнопка «Обновить» в UI: показывает список ламп группы с онлайн/оффлайн статусом
