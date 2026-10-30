/*
  Скетч к проекту "WebLamp"
  - Страница проекта (схемы, описания): https://alexgyver.ru/weblamp/
  - Исходники на GitHub: https://github.com/AlexGyver/WebLamp
  Проблемы с загрузкой? Читай гайд для новичков: https://alexgyver.ru/arduino-first/
  AlexGyver, AlexGyver Technologies, 2022

  1.0 - релиз
*/

#define LED_PIN 14    // пин ленты
#define BTN_PIN 16    // пин кнопки
#define PIR_PIN D5    // пин PIR (ИК датчика)
#define LED_AMOUNT 24 // кол-вл светодиодов
#define BTN_LEVEL 1   // 1 - кнопка подключает VCC, 0 - подключает GND
#define USE_PIR 0     // 1 - использовать PIR (ИК датчик) на этой лампе
#define IGNORE_PIR 0  // 1 - игнорировать сигнал PIR (ИК датчика) с удалённой лампы

/*
  Запуск (не блокирует — лампа управляется всегда):
  - Есть сохранённая сеть: подключаемся в фоне. Если за 15 с нет IP
    или связь пропала — поднимается точка доступа WLamp-XXXXXX (192.168.4.1).
  - Нет сохранённой сети: точка доступа сразу.
  - Клик в первые 2 с после включения: принудительно включить точку доступа.
  - Пока работает точка, раз в 30 с пробуем снова подключиться к роутеру.
    Через 60 с стабильного STA без клиентов точка выключается.

  Анимация при старте: 2 с мигает зелёным (окно для клика).

  Работа:
  1 клик: вкл/выкл
  2 клика: сменить цвет
  3 клика: подмигнуть
  Удержание: сменить яркость
*/

// ============= ВСЯКОЕ =============
#define MQTT_HEADER "GWL:"  // заголовок пакета данных

// ============= БИБЛЫ =============
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <GyverPortal.h>
#include <EEManager.h>
#include <PubSubClient.h>
#include <FastLED.h>
#define EB_STEP 100   // период step шага кнопки
#include <EncButton.h>
#include "Timer.h"


// ============= ДАННЫЕ =============
#if 1
#define DEBUG(x) Serial.print(x)
#define DEBUGLN(x) Serial.println(x)
#else
#define DEBUG(x)
#define DEBUGLN(x)
#endif

struct LampData {
  char ssid[32] = "";
  char pass[32] = "";
  char local[20] = "AG_lamp_1";
  char remote[20] = "AG_lamp_2";
  char host[32] = "broker.mqttdashboard.com";
  uint16_t port = 1883;
  uint8_t ip[4] = {0, 0, 0, 0};

  bool power = 1;
  uint8_t bright = 50;
  uint8_t color = 0;
};

LampData data;
EncButtonT<BTN_PIN, 0, 0> btn;
CRGB leds[LED_AMOUNT];
WiFiClient espClient;
PubSubClient mqtt(espClient);
GyverPortal portal;
EEManager memory(data);

DNSServer dns;          // captive DNS для точки доступа
bool apActive = false;  // поднята ли точка доступа
bool pirFlag = 0;
bool winkFlag = 0;
bool startFlag = 0;
const uint8_t hLen = strlen(MQTT_HEADER);

Timer onlineTmr(18000, false);  // 18 секунд таймаут онлайна
Timer pirTmr(60000, false);     // 1 минута таймаут пира
Timer hbTmr(8000);              // 8 секунд период отправки пакета

void setup() {
  startup();      // запускаем всё
}

void loop() {
  if (USE_PIR && digitalRead(PIR_PIN)) pirFlag = 1;  // опрос ИК датчика
  
  heartbeat();    // отправляем пакет что мы онлайн
  wifiTick();     // STA реконнект + управление точкой доступа (не блокирует)
  memory.tick();  // проверяем обновление настроек
  animation();    // эффект ленты
  buttonTick();   // действия кнопки
  mqttTick();     // проверяем входящие
  portal.tick();  // веб-интерфейс работает и по STA, и по точке доступа
  checkPortal();  // проверяем действия
}
