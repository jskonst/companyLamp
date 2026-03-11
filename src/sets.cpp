#include "sets.h"

#include <LittleFS.h>
#include <WiFiConnector.h>

GyverDBFile db(&LittleFS, "/data.db");
SettingsGyver sett(PROJECT_NAME, &db);

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
    

    // wifi
    WiFiConnector.onConnect([]() {
        Serial.print("Connected: ");
        Serial.println(WiFi.localIP());
        //TODO - save local IP address and show it in name of AP
    });
    WiFiConnector.onError([]() {
        Serial.print("Error. Start AP: ");
        Serial.println(WiFi.softAPIP());
    });

    WiFiConnector.setName(PROJECT_NAME);
    WiFiConnector.closeAP(db[kk::close_ap]);
    WiFiConnector.connect(db[kk::ssid], db[kk::pass]);

    // settings
    sett.begin();
    sett.onBuild(build);
    sett.onUpdate(update);
}

// ========== loop ==========
void sett_loop() {
    WiFiConnector.tick();
    sett.tick();
}