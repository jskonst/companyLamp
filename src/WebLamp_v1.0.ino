/*
  WebLamp (cleaned build)
*/

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

#define MQTT_HEADER "GWL:"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "led_driver.h"
#define EB_STEP_TIME 100
#include <EncButton.h>
#include "Timer.h"
#include "sets.h"

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

static void showBlack() {
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  showLedsSafe();
  delay(2);
  showLedsSafe();
}

void applyLampState() {
  static bool initialized = false;
  static bool lastPower = false;
  static uint8_t lastBright = 0;
  static uint8_t lastColor = 255;

  bool power = sett_started() ? cfg::power() : true;
  uint8_t bright = sett_started() ? cfg::bright() : 20;
  uint8_t color = sett_started() ? cfg::color() : 0;

  if (initialized && power == lastPower && bright == lastBright && color == lastColor) return;

  lastPower = power;
  lastBright = bright;
  lastColor = color;
  initialized = true;

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
  delay(1000);
  Serial.begin(115200);
  Serial.println("\n\nStarting WebLamp...");

  Serial.printf("Initializing NeoPixelBus on GPIO%d...\n", LED_PIN);
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, LED_AMOUNT).setCorrection(UncorrectedColor);
  FastLED.setDither(0);
  FastLED.setBrightness(50);
  fill_solid(leds, LED_AMOUNT, CRGB::Green);
  FastLED.show();
  Serial.println("LEDs initialized - should be GREEN now!");

  btn.setClickTimeout(BTN_CLICK_TIMEOUT);
  btn.setDebTimeout(BTN_DEBOUNCE_TIME);
#if SERIAL_BTN_TEST
  Serial.println(F("Serial test: 1/2/3 click, +/- brightness, p toggle, r/g/w/k/t visual"));
#endif

  Timer tmr(2000);
  while (!tmr.period()) {
    loadAnimation(CRGB::Green);
    btn.tick();
  }

  showBlack();
  delay(500);

  sett_begin();
  WiFi.setSleepMode(WIFI_SLEEP_CFG);
  DEBUG(F("WiFi sleep mode: "));
  DEBUGLN((int)WIFI_SLEEP_CFG);

  showBlack();
  delay(200);

  FastLED.setBrightness(100);
  for (int i = 0; i < 3; i++) {
    showBlack();
    delay(800);
    fill_solid(leds, LED_AMOUNT, CRGB::White);
    showLedsSafe();
    delay(800);
  }
  showBlack();

  applyLampState();
}

void loop() {
#if SERIAL_BTN_TEST
  buttonSerialTestTick();
#endif
  buttonTick();
  sett_loop();

#if SERIAL_BTN_TEST
  if (serialVisualOverride && (int32_t)(millis() - serialVisualOverrideUntil) >= 0) {
    serialVisualOverride = false;
  }
  if (!serialVisualOverride) applyLampState();
#else
  applyLampState();
#endif
}
