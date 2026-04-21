/*
  WebLamp (cleaned build)
*/

#include "mode.h"          // Mode enum must be visible before auto-generated prototypes

#define LED_PIN 5          // strip data pin (D1 / GPIO5)
#define BTN_PIN 14         // button pin (D5 / GPIO14)
#define PIR_PIN D6         // PIR pin (GPIO12)
#define LED_AMOUNT 24      // number of LEDs
#define BTN_LEVEL 1        // 1: button to VCC, 0: button to GND
#define SERIAL_BTN_TEST 0  // 1: enable Serial simulation in buttonSerialTestTick()
#define WIFI_SLEEP_CFG WIFI_MODEM_SLEEP  // WIFI_NONE_SLEEP / WIFI_MODEM_SLEEP / WIFI_LIGHT_SLEEP
#define BTN_CLICK_TIMEOUT 650            // ms window for multi-click detection
#define BTN_DEBOUNCE_TIME 25             // ms debounce for mechanical button
#define USE_PIR 0
#define IGNORE_PIR 0

// Максимум ламп в группе. RemoteLamp ≈ 28 байт, итого N×28 байт статической RAM.
// При увеличении сверх 20 проверьте свободную память (сейчас ~50 KB свободно).
#define MAX_REMOTE_LAMPS 20

#define MQTT_HEADER "GWL:"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "led_driver.h"
#define EB_STEP_TIME 100
#include <EncButton.h>
#include "Timer.h"
#include "sets.h"
#include "mqtt.h"

#define DEBUG_ENABLE 1
#if DEBUG_ENABLE
  #define DEBUG(x) Serial.print(x)
  #define DEBUGLN(x) Serial.println(x)
#else
  #define DEBUG(x)
  #define DEBUGLN(x)
#endif

// GPIO16 on ESP8266 supports dedicated pulldown mode.
// For BTN_LEVEL=1 (button to VCC) this keeps input stable without external resistor.
#if (BTN_PIN == 16) && (BTN_LEVEL == 1)
constexpr uint8_t BTN_PIN_MODE = INPUT_PULLDOWN_16;
#elif (BTN_LEVEL == 0)
constexpr uint8_t BTN_PIN_MODE = INPUT_PULLUP;
#else
constexpr uint8_t BTN_PIN_MODE = INPUT;
#endif
ButtonT<BTN_PIN> btn(BTN_PIN_MODE, BTN_LEVEL);
CRGB leds[LED_AMOUNT];
WiFiClient espClient;
PubSubClient mqtt(espClient);

#if SERIAL_BTN_TEST
bool serialVisualOverride = false;
uint32_t serialVisualOverrideUntil = 0;
#endif

void showLedsSafe() {
  noInterrupts();
  FastLED.show();
  interrupts();
}

static bool lampStateCacheInitialized = false;
static bool lampStateLastPower = false;
static uint8_t lampStateLastBright = 0;
static uint8_t lampStateLastColor = 255;

void invalidateLampStateCache() {
  lampStateCacheInitialized = false;
}

void applyLampState() {
  bool power = sett_started() ? cfg::power() : true;
  uint8_t bright = sett_started() ? cfg::bright() : 20;
  uint8_t color = sett_started() ? cfg::color() : 0;

  if (lampStateCacheInitialized &&
      power == lampStateLastPower &&
      bright == lampStateLastBright &&
      color == lampStateLastColor) {
    return;
  }

  lampStateLastPower = power;
  lampStateLastBright = bright;
  lampStateLastColor = color;
  lampStateCacheInitialized = true;

  CRGB c;
  if (!power) c = CRGB::Black;
  else if (color == 0) c = CRGB::White;
  else c = CHSV(color, 255, 255);

  fill_solid(leds, LED_AMOUNT, c);
  if (!power) {
    FastLED.setBrightness(0);
    showLedsSafe();
    delayMicroseconds(80);
    showLedsSafe();
  } else {
    FastLED.setBrightness(map(bright, 0, 100, 0, 255));
    showLedsSafe();
  }
}

void setup() {
  startup();
  mqttBegin();
}

void loop() {
#if SERIAL_BTN_TEST
  buttonSerialTestTick();
#endif
  buttonTick();
  sett_loop();
  mqttTick();
  meetingTick();
  modeWebTick();
  modeIndicatorTick();

#if SERIAL_BTN_TEST
  if (serialVisualOverride && (int32_t)(millis() - serialVisualOverrideUntil) >= 0) {
    serialVisualOverride = false;
  }
  if (!serialVisualOverride) renderTick();
#else
  renderTick();
#endif
}
