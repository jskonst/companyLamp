/*
  companyLamp — группа ламп на ESP8266 (Wemos D1 mini).
  Протокол: docs/protocol.md (топики lamp/<group>/..., настроение, среднее, away).

  Пины:
    лента WS2812 — GPIO14 / D5
    кнопка       — GPIO16 / D0, BTN_LEVEL 1, внешний подтяг к GND
    PIR          — D6 / GPIO12, USE_PIR 0. В групповом протоколе PIR не передаётся.

  Жесты кнопки — константы GESTURE_* в include/group_logic.h.
*/

#define LED_PIN 14
#define BTN_PIN 16
#define PIR_PIN D6
#define LED_AMOUNT 24
#define BTN_LEVEL 1
#define USE_PIR 0

// Сколько чужих ламп помним. Вместе с собой это 20 — потолок группы.
#define PEER_MAX 19

// Пауза перед повтором MQTT и потолок. TCP к мёртвому брокеру не блокирует loop
// (см. комментарий в mqtt.ino); эти паузы лишь не дают долбить брокер.
#define MQTT_BACKOFF_START_MS 2000u
#define MQTT_BACKOFF_MAX_MS 30000u

#ifndef LAMP_OTA_PASS
#define LAMP_OTA_PASS "companylamp"
#endif
static_assert(sizeof(LAMP_OTA_PASS) > 1, "LAMP_OTA_PASS не должен быть пустым");

#ifndef MQTT_DNS_TIMEOUT_MS
#define MQTT_DNS_TIMEOUT_MS 500
#endif

#include <ArduinoOTA.h>
#include <AsyncMqttClient.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <EEManager.h>
#include <ESP8266WiFi.h>
#include <EncButton.h>
#include <FastLED.h>
#include <GyverPortal.h>

#include "Timer.h"
#include "group_logic.h"

#define DEBUG(x) Serial.print(x)
#define DEBUGLN(x) Serial.println(x)

// Образ EEPROM. Ключ EEManager ('b' в startup) не совпадает со старой парой —
// при прошивке поверх ключа 'a' настройки сбрасываются на эти значения.
// Поле добавили или поменяли местами — поднимите ключ, иначе EEManager прочитает мусор.
struct LampData {
  char ssid[32] = "";
  char pass[32] = "";
  char group[21] = "";       // пусто = только локально
  char host[32] = "";        // пусто = брокер не настроен, публичного брокера больше нет
  uint16_t port = 1883;
  char mqttUser[32] = "";
  char mqttPass[32] = "";
  char owner[32] = "";       // необязательное имя в state.n
  char apPass[32] = "";      // пароль точки; пусто = взять LAMP_AP_PASS или открытую сеть
  uint8_t ip[4] = {0, 0, 0, 0};
  bool power = true;
  uint8_t bright = 50;
  uint8_t mood = 50;         // 0 красный … 50 жёлтый … 100 зелёный
  bool average = false;      // false = мой цвет, true = среднее группы
  bool autonomous = false;
};

struct Peer {
  char id[7];
  char name[32];
  uint8_t mood;
  PeerStatus status;
  bool hasMood;
};

LampData data;
EEManager memory(data);
// ButtonT, не EncButtonT: у EncButtonT три параметра — это пины энкодера и кнопки,
// EncButtonT<16, 0, 0> читал бы GPIO0. Кнопка лампы — GPIO16, уровень 1, без внутренней подтяжки.
ButtonT<BTN_PIN> btn(INPUT, BTN_LEVEL);
CRGB leds[LED_AMOUNT];
GyverPortal portal;
DNSServer dns;
AsyncMqttClient mqtt;

char lampId[7];
char clientId[16];
char deviceHost[24];
char topicStatus[48];
char topicState[48];
char topicWink[48];
char subStatus[40];
char subState[40];
char subWink[40];

Peer peers[PEER_MAX];
int peerCount = 0;
char peerText[640];
int onlineCount = 0;
bool peersDirty = true;

bool apActive = false;
char portalNote[120];

uint32_t winkStartMs = 0;
uint8_t winkHue = 0;
uint32_t maxFlashMs = 0;

bool mqttConnecting = false;
bool mqttHasIP = false;
bool awayAnnounced = false;
bool awayWait = false;
uint16_t awayPkt = 0;
uint32_t awayWaitSince = 0;
uint32_t mqttConnectStarted = 0;
uint32_t mqttNextTry = 0;
uint16_t mqttBackoff = MQTT_BACKOFF_START_MS;
uint8_t mqttFailStreak = 0;
bool mqttHoldBackoff = false;
uint32_t mqttResolvedAt = 0;
IPAddress mqttIP;

void setup() {
  startup();
}

void loop() {
  ArduinoOTA.handle();
  wifiTick();
  memory.tick();
  animation();
  buttonTick();
  mqttTick();
  portal.tick();
  checkPortal();
}
