#include "sets.h"
#include "mode.h"

#include <LittleFS.h>
#include <WiFiConnector.h>

GyverDBFile db(&LittleFS, "/data.db");
SettingsGyver sett(PROJECT_NAME, &db);
static bool settings_started = false;

static String makeAPName() {
    String ip = String(db[kk::ip]);
    if (!ip.length() || ip == "0.0.0.0") return String(PROJECT_NAME);

    String name(PROJECT_NAME);
    name += ' ';
    name += ip;
    return name;
}

// ========== mode sections ==========
static void buildLamp(sets::Builder& b) {
    sets::Group g(b, "Лампа");
    b.Switch(kk::power, "Питание");
    b.Slider(kk::bright, "Яркость", 0, 100, 1);
    b.Slider(kk::color,  "Цвет",    0, 255, 1);
}

static void buildPomodoro(sets::Builder& b) {
    {
        sets::Group g(b, "Управление");
        b.Label(pomoStateString().c_str());
        if (b.Button(pomoActionLabel())) {
            pomodoroWebAction();
            b.reload();
        }
        if (pomoSkipLabel() && b.Button(pomoSkipLabel())) {
            pomodoroWebSkip();
            b.reload();
        }
    }
    {
        sets::Group g(b, "Настройки");
        b.Input(kk::pomo_work,  "Работа (мин)");
        b.Input(kk::pomo_short, "Короткий перерыв (мин)");
        b.Input(kk::pomo_long,  "Длинный перерыв (мин)");
        b.Input(kk::pomo_count, "Помодоро до длинного");
        b.Slider(kk::pomo_color,       "Цвет работы",        0, 255, 1);
        b.Slider(kk::pomo_break_color, "Цвет перерыва",       0, 255, 1);
        b.Slider(kk::pomo_bg,          "Цвет фона",          0, 255, 1);
    }
}

static void buildMood(sets::Builder& b) {
    sets::Group g(b, "Настроение");
    b.Switch(kk::power, "Питание");
    b.Slider(kk::bright, "Яркость", 0, 100, 1);
    b.Slider(kk::mood, "Настроение (0=красный, 120=зелёный)", 0, 120, 1);
}

static void buildDND(sets::Builder& b) {
    sets::Group g(b, "Не беспокоить");
    if (b.Switch(kk::dnd, "Не беспокоить")) {
        toggleDND();
        b.reload();
    }
    b.Switch(kk::dnd_wink, "Принимать wink-сигналы");
}

static void buildMeeting(sets::Builder& b) {
    sets::Group g(b, "Митинг");
    b.Label("Реализуется в Plan 09");
}

static void buildWifi(sets::Builder& b) {
    {
        sets::Group g(b, "WiFi");
        b.Input(kk::ssid,  "SSID");
        b.Pass(kk::pass,   "Пароль WiFi", "");
        b.Input(kk::local, "Имя лампы", "lamp_1");
        if (b.Switch(kk::close_ap, "Скрыть AP")) {
            WiFiConnector.closeAP(db[kk::close_ap]);
        }
        if (b.Button("Подключиться")) {
            WiFiConnector.connect(db[kk::ssid], db[kk::pass]);
        }
    }
    {
        sets::Group g(b, "MQTT");
        b.Input(kk::host,      "Хост",            "broker.hivemq.com");
        b.Number(kk::port,     "Порт", nullptr, 1, 65535);
        b.Input(kk::group,     "Группа",           "office");
        b.Input(kk::mqtt_user, "Логин брокера",    "");
        b.Pass(kk::mqtt_pass,  "Пароль брокера",   "");
        b.Switch(kk::offline,  "Офлайн-режим");
        b.Switch(kk::dnd_wink, "Принимать wink-сигналы");
        b.Label(mqttStatusString().c_str());
        if (b.Button("Обновить")) b.reload();
    }
}

// ========== build ==========
static void build(sets::Builder& b) {
    if ((uint8_t)db[kk::mode] == (uint8_t)Mode::DND) {
        buildDND(b);
    } else {
        if (b.Tabs(kk::mode, "LAMP;POMODORO;MOOD")) {
            b.reload();
        }
        switch ((uint8_t)db[kk::mode]) {
            case 0: buildLamp(b);     break;
            case 1: buildPomodoro(b); break;
            case 2: buildMood(b);     break;
        }
    }

    {
        sets::Menu wifi(b, "WiFi");
        buildWifi(b);
    }
}

// ========== update ==========
static void update(sets::Updater& u) {
}

// ========== begin ==========
void sett_begin() {
    if (settings_started) return;

    // fs
#ifdef ESP32
    LittleFS.begin(true);
#else
    LittleFS.begin();
#endif

    // database
    db.begin();
    db.init(kk::ssid, "");
    db.init(kk::pass, "");
    db.init(kk::close_ap, true);
    db.init(kk::local,     "lamp_1");
    db.init(kk::remote,    "");
    db.init(kk::host,      "broker.hivemq.com");
    db.init(kk::port,      (uint16_t)1883);
    db.init(kk::group,     "office");
    db.init(kk::mqtt_user, "");
    db.init(kk::mqtt_pass, "");
    db.init(kk::offline,   false);
    db.init(kk::ip, "0.0.0.0");
    db.init(kk::power, 1);
    db.init(kk::bright, 20);
    db.init(kk::color, 0);
    db.init(kk::mode, 0);
    if ((uint8_t)db[kk::mode] >= MODE_COUNT) { db[kk::mode] = (uint8_t)0; db.update(); }
    db.init(kk::dnd, false);
    db.init(kk::dnd_wink, true);
    db.init(kk::pomo_work,  (uint8_t)25);
    db.init(kk::pomo_short, (uint8_t)5);
    db.init(kk::pomo_long,  (uint8_t)15);
    db.init(kk::pomo_count, (uint8_t)4);
    db.init(kk::pomo_color,       (uint8_t)0);
    db.init(kk::pomo_break_color, (uint8_t)160);
    db.init(kk::pomo_bg,          (uint8_t)0);
    db.init(kk::mood,             (uint8_t)120);
    db.init(kk::mood_date,        (uint32_t)0);

    // wifi
    WiFiConnector.onConnect([]() {
        const String ip = WiFi.localIP().toString();

        Serial.print("Connected: ");
        Serial.println(ip);

        if (String(db[kk::ip]) != ip) {
            db[kk::ip] = ip;
            db.update();
        }
        WiFiConnector.setName(makeAPName());
    });
    WiFiConnector.onError([]() {
        Serial.print("Error. Start AP: ");
        Serial.println(WiFi.softAPIP());
    });

    WiFiConnector.setName(makeAPName());
    WiFiConnector.setTimeout(15);
    WiFiConnector.closeAP(db[kk::close_ap]);

    // settings
    sett.begin();
    sett.onBuild(build);
    sett.onUpdate(update);

    settings_started = true;
}

// ========== loop ==========
void sett_loop() {
    if (!settings_started) return;
    WiFiConnector.tick();
    sett.tick();
}

bool sett_started() {
    return settings_started;
}

namespace cfg {
uint8_t bright() {
    return (uint8_t)db[kk::bright];
}

bool power() {
    return (bool)db[kk::power];
}

uint8_t color() {
    return (uint8_t)db[kk::color];
}

String ssid() {
    return String(db[kk::ssid]);
}

String pass() {
    return String(db[kk::pass]);
}

String local() {
    return String(db[kk::local]);
}

String host() {
    return String(db[kk::host]);
}

uint16_t port() {
    return (uint16_t)db[kk::port];
}

String ip() {
    return String(db[kk::ip]);
}

bool close_ap() {
    return (bool)db[kk::close_ap];
}

String group() {
    return String(db[kk::group]);
}

String mqttUser() {
    return String(db[kk::mqtt_user]);
}

String mqttPass() {
    return String(db[kk::mqtt_pass]);
}

bool offline() {
    return (bool)db[kk::offline];
}

bool dnd() {
    return (bool)db[kk::dnd];
}

bool dndWink() {
    return (bool)db[kk::dnd_wink];
}

uint8_t pomo_work() {
    return (uint8_t)db[kk::pomo_work];
}

uint8_t pomo_short() {
    return (uint8_t)db[kk::pomo_short];
}

uint8_t pomo_long() {
    return (uint8_t)db[kk::pomo_long];
}

uint8_t pomo_count() {
    return (uint8_t)db[kk::pomo_count];
}

uint8_t pomo_color() {
    return (uint8_t)db[kk::pomo_color];
}

uint8_t pomo_break_color() {
    return (uint8_t)db[kk::pomo_break_color];
}

uint8_t pomo_bg() {
    return (uint8_t)db[kk::pomo_bg];
}

uint8_t mood() {
    return (uint8_t)db[kk::mood];
}
}  // namespace cfg
