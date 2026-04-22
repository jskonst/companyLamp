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
String host();
uint16_t port();
String ip();
bool close_ap();
String group();
String mqttUser();
String mqttPass();
bool offline();
bool dnd();
uint8_t pomo_work();
uint8_t pomo_short();
uint8_t pomo_long();
uint8_t pomo_count();
uint8_t pomo_color();
uint8_t pomo_break_color();
uint8_t pomo_bg();
}

String mqttStatusString();
String pomoStateString();
const char* pomoActionLabel();
const char* pomoSkipLabel();
void pomodoroWebTick();
void pomodoroClick(uint8_t clicks);
void pomodoroWebAction();
void pomodoroWebSkip();

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
    mode,
    group,
    mqtt_user,
    mqtt_pass,
    offline,
    dnd,
    pomo_work,
    pomo_short,
    pomo_long,
    pomo_count,
    pomo_color,
    pomo_break_color,
    pomo_bg
);
