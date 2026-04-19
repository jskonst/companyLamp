#pragma once

#include <Arduino.h>

#define PROJECT_NAME "WebLamp"

#include <GyverDBFile.h>
#include <SettingsGyver.h>

extern GyverDBFile db;
extern SettingsGyver sett;

void sett_begin();
void sett_loop();
bool sett_started();

namespace cfg {
uint8_t bright();
bool power();
uint8_t color();
String ssid();
String pass();
String local();
String remote();
String host();
uint16_t port();
String ip();
bool close_ap();
}

DB_KEYS(
    kk,
    ssid,
    pass,
    close_ap,
    local,
    remote,
    host,
    port,
    ip,
    power,
    bright,
    color,
    mode
);
