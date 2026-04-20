#include <WiFiConnector.h>

extern void showLedsSafe();

// ===== startup WiFi animation tuning =====
constexpr uint8_t WIFI_CONNECTED_BLINK_COUNT = 2;
constexpr uint8_t WIFI_CONNECTED_BRIGHT_MIN = 16;
constexpr uint8_t WIFI_CONNECTED_BRIGHT_MAX = 78;
constexpr uint8_t WIFI_CONNECTED_BRIGHT_STEP = 2;
constexpr uint16_t WIFI_CONNECTED_STEP_DELAY_MS = 14;
constexpr uint16_t WIFI_CONNECTED_END_PAUSE_MS = 120;

constexpr uint8_t WIFI_WAIT_BRIGHT_MIN = 14;
constexpr uint8_t WIFI_WAIT_BRIGHT_MAX = 62;
constexpr uint8_t WIFI_WAIT_BRIGHT_FLOOR = 20;
constexpr uint8_t WIFI_WAIT_BRIGHT_STEP = 2;
constexpr uint16_t WIFI_WAIT_STEP_DELAY_MS = 18;
constexpr uint32_t WIFI_WAIT_PULSE_PERIOD_MS = 5000ul;

static void startupShowBlack() {
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  showLedsSafe();
  delay(2);
  showLedsSafe();
}

static void showWifiConnectedBlink() {
  fill_solid(leds, LED_AMOUNT, CRGB::Green);
  for (uint8_t i = 0; i < WIFI_CONNECTED_BLINK_COUNT; i++) {
    for (uint8_t b = WIFI_CONNECTED_BRIGHT_MIN; b <= WIFI_CONNECTED_BRIGHT_MAX; b += WIFI_CONNECTED_BRIGHT_STEP) {
      FastLED.setBrightness(b);
      showLedsSafe();
      delay(WIFI_CONNECTED_STEP_DELAY_MS);
    }
    for (int b = WIFI_CONNECTED_BRIGHT_MAX;
         b >= (int)WIFI_CONNECTED_BRIGHT_MIN + WIFI_CONNECTED_BRIGHT_STEP;
         b -= WIFI_CONNECTED_BRIGHT_STEP) {
      FastLED.setBrightness((uint8_t)b);
      showLedsSafe();
      delay(WIFI_CONNECTED_STEP_DELAY_MS);
    }
  }
  delay(WIFI_CONNECTED_END_PAUSE_MS);
}

static void showWifiWaitPulse() {
  fill_solid(leds, LED_AMOUNT, CRGB::Green);
  for (uint8_t b = WIFI_WAIT_BRIGHT_MIN; b <= WIFI_WAIT_BRIGHT_MAX; b += WIFI_WAIT_BRIGHT_STEP) {
    FastLED.setBrightness(b);
    showLedsSafe();
    delay(WIFI_WAIT_STEP_DELAY_MS);
  }
  for (int b = WIFI_WAIT_BRIGHT_MAX; b >= WIFI_WAIT_BRIGHT_FLOOR; b -= WIFI_WAIT_BRIGHT_STEP) {
    FastLED.setBrightness((uint8_t)b);
    showLedsSafe();
    delay(WIFI_WAIT_STEP_DELAY_MS);
  }
}

static bool startupButtonClicked() {
  btn.tick();
  if (btn.hasClicks()) {
    btn.getClicks();
    return true;
  }
  return false;
}

static void enterAP() {
  WiFiConnector.connect("", "");
  DEBUGLN(F("[STARTUP] AP mode"));
  const uint32_t ignoreClicksUntil = millis() + 700;

  while (!WiFiConnector.connected()) {
    sett_loop();
    loadAnimation(CRGB::Blue);

    if ((int32_t)(millis() - ignoreClicksUntil) >= 0 && startupButtonClicked()) {
      DEBUGLN(F("[STARTUP] AP exit by button -> restart"));
      ESP.restart();
    }
    yield();
  }

  DEBUGLN(F("[STARTUP] Connected from AP mode"));
  showWifiConnectedBlink();
}

void startup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("\n\nStarting WebLamp...");

  Serial.printf("Initializing NeoPixelBus on GPIO%d...\n", LED_PIN);
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, LED_AMOUNT).setCorrection(UncorrectedColor);
  FastLED.setDither(0);
  FastLED.setBrightness(50);
  fill_solid(leds, LED_AMOUNT, CRGB::Green);
  showLedsSafe();
  Serial.println("LEDs initialized - should be GREEN now!");

  btn.setClickTimeout(BTN_CLICK_TIMEOUT);
  btn.setDebTimeout(BTN_DEBOUNCE_TIME);
#if SERIAL_BTN_TEST
  Serial.println(F("Serial test: 1/2/3 click, +/- brightness, p toggle, r/g/w/k/t visual"));
#endif

  sett_begin();
  WiFi.setSleepMode(WIFI_SLEEP_CFG);
  DEBUG(F("WiFi sleep mode: "));
  DEBUGLN((int)WIFI_SLEEP_CFG);

  Timer openAptmr(2000);
  while (!openAptmr.period()) {
    loadAnimation(CRGB::Green);
    if (startupButtonClicked()) {
      enterAP();
      startupShowBlack();
      applyLampState();
      return;
    }
  }

  const String ssid = cfg::ssid();
  DEBUG(F("[STARTUP] Try WiFi SSID: "));
  DEBUGLN(ssid.length() ? ssid : F("<empty>"));
  WiFiConnector.connect(ssid, cfg::pass());

  uint32_t pulseTmr = millis();
  while (WiFiConnector.connecting()) {
    sett_loop();
    loadAnimation(CRGB::Green);

    if ((uint32_t)(millis() - pulseTmr) >= WIFI_WAIT_PULSE_PERIOD_MS) {
      pulseTmr = millis();
      showWifiWaitPulse();
    }

    if (startupButtonClicked()) {
      enterAP();
      startupShowBlack();
      applyLampState();
      return;
    }
  }

  if (WiFiConnector.connected()) {
    showWifiConnectedBlink();
  } else {
    enterAP();
  }

  startupShowBlack();
  applyLampState();
}
