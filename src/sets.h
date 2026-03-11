#pragma once

#define PROJECT_NAME "Test"

#include <GyverDBFile.h>
#include <SettingsGyver.h>

extern GyverDBFile db;
extern SettingsGyver sett;

void sett_begin();
void sett_loop();

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
    color
);


// struct LampData {
//   char ssid[32] = "";
//   char pass[32] = "";
//   char local[20] = "AG_lamp_1";
//   char remote[20] = "AG_lamp_2";
//   char host[32] = "broker.mqttdashboard.com";
//   uint16_t port = 1883;
//   uint8_t ip[4] = {0, 0, 0, 0};

//   bool power = 1;
//   uint8_t bright = 50;
//   uint8_t color = 0;
// };