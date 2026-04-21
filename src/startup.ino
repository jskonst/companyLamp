#include <WiFiConnector.h>

extern void showLedsSafe();

// ===== startup timing =====
constexpr uint32_t STARTUP_BOOT_DELAY_MS    = 1000;
constexpr uint32_t STARTUP_WARMUP_MS        = 2000;
constexpr uint32_t STARTUP_AP_IGNORE_BTN_MS = 700;

// ===== startup WiFi animation tuning =====
constexpr uint8_t  WIFI_CONNECTED_BLINK_COUNT   = 2;
constexpr uint8_t  WIFI_CONNECTED_BRIGHT_MIN    = 16;
constexpr uint8_t  WIFI_CONNECTED_BRIGHT_MAX    = 78;
constexpr uint8_t  WIFI_CONNECTED_BRIGHT_STEP   = 2;
constexpr uint16_t WIFI_CONNECTED_STEP_DELAY_MS = 14;
constexpr uint16_t WIFI_CONNECTED_END_PAUSE_MS  = 120;

constexpr uint8_t  WIFI_WAIT_BRIGHT_MIN      = 14;
constexpr uint8_t  WIFI_WAIT_BRIGHT_MAX      = 62;
constexpr uint8_t  WIFI_WAIT_BRIGHT_FLOOR    = 20;
constexpr uint8_t  WIFI_WAIT_BRIGHT_STEP     = 2;
constexpr uint16_t WIFI_WAIT_STEP_DELAY_MS   = 18;
constexpr uint32_t WIFI_WAIT_PULSE_PERIOD_MS = 5000ul;

// ===== startup state machine =====
enum class StartupState : uint8_t {
  INIT,
  WARMUP,
  WIFI_CONNECT,
  AP_MODE,
  FINALIZE,
  DONE
};

// ===== animations =====

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

// ===== utilities =====

static bool startupButtonClicked() {
  btn.tick();
  if (btn.hasClicks()) {
    btn.getClicks();
    return true;
  }
  return false;
}

static void startupLogState(const __FlashStringHelper* tag) {
  DEBUG(F("[STARTUP] "));
  DEBUGLN(tag);
}

// ===== phases =====

static void startupInitHardware() {
  startupLogState(F("INIT"));
  delay(STARTUP_BOOT_DELAY_MS);
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
}

// Returns true if button clicked -> go to AP
static bool startupWarmupWindow() {
  startupLogState(F("WARMUP"));
  Timer openAptmr(STARTUP_WARMUP_MS);
  while (!openAptmr.period()) {
    loadAnimation(CRGB::Green);
    if (startupButtonClicked()) return true;
  }
  return false;
}

// Returns true if WiFi connected
static bool startupTryWifiConnect() {
  startupLogState(F("WIFI_CONNECT"));
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

    if (startupButtonClicked()) return false;
  }

  return WiFiConnector.connected();
}

static void startupRunApMode() {
  startupLogState(F("AP_MODE"));
  WiFiConnector.connect("", "");
  const uint32_t ignoreClicksUntil = millis() + STARTUP_AP_IGNORE_BTN_MS;

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

static void startupFinalize() {
  startupLogState(F("FINALIZE"));
  startupShowBlack();
  applyLampState();
}

// ===== orchestration =====

void startup() {
  StartupState state = StartupState::INIT;

  while (state != StartupState::DONE) {
    switch (state) {
      case StartupState::INIT:
        startupInitHardware();
        state = StartupState::WARMUP;
        break;

      case StartupState::WARMUP:
        state = startupWarmupWindow() ? StartupState::AP_MODE : StartupState::WIFI_CONNECT;
        break;

      case StartupState::WIFI_CONNECT:
        if (startupTryWifiConnect()) {
          showWifiConnectedBlink();
          state = StartupState::FINALIZE;
        } else {
          state = StartupState::AP_MODE;
        }
        break;

      case StartupState::AP_MODE:
        startupRunApMode();
        state = StartupState::FINALIZE;
        break;

      case StartupState::FINALIZE:
        startupFinalize();
        state = StartupState::DONE;
        break;

      case StartupState::DONE:
        break;
    }
  }

  startupLogState(F("DONE"));
}
