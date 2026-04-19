#include "sets.h"

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

// ========== build ==========
static void build(sets::Builder& b) {
    {
        sets::Group g(b, "WiFi");
        b.Input(kk::ssid, "SSID");
        b.Pass(kk::pass, "Pass", "");
        b.Input(kk::local, "Локальная лампа", "AG_lamp_1");
        b.Input(kk::remote, "Удалённая лампа", "AG_lamp_2");
        b.Input(kk::host, "Хост", "broker.mqttdashboard.com");
        b.Input(kk::port, "Порт", "1883");
        b.Input(kk::ip, "IP", "0.0.0.0");
        b.Switch(kk::power, "Питание");
        b.Slider(kk::bright, "Яркость", 0,100,1);
        
        if (b.Switch(kk::close_ap, "Закрывать AP")) {   
            WiFiConnector.closeAP(db[kk::close_ap]);
        }
        if (b.Button("Подключить")) {
            WiFiConnector.connect(db[kk::ssid], db[kk::pass]);
        }
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
    db.init(kk::local, "AG_lamp_1");
    db.init(kk::remote, "AG_lamp_2");
    db.init(kk::host,  "broker.mqttdashboard.com");
    db.init(kk::port,  "1883");
    db.init(kk::ip, "0.0.0.0");
    db.init(kk::power, 0);
    db.init(kk::bright, 20);
    db.init(kk::color, 0);
    

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
    WiFiConnector.closeAP(db[kk::close_ap]);
    WiFiConnector.connect(db[kk::ssid], db[kk::pass]);

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

String remote() {
    return String(db[kk::remote]);
}

String host() {
    return String(db[kk::host]);
}

uint16_t port() {
    return (uint16_t)String(db[kk::port]).toInt();
}

String ip() {
    return String(db[kk::ip]);
}

bool close_ap() {
    return (bool)db[kk::close_ap];
}
}  // namespace cfg
