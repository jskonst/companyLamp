#include <WiFiConnector.h>
#include <ArduinoOTA.h>

#ifndef LAMP_OTA_PASS
#define LAMP_OTA_PASS "companylamp"
#endif
static_assert(sizeof(LAMP_OTA_PASS) > 1, "LAMP_OTA_PASS must not be empty");

extern void showLedsSafe();

void otaBegin();

static bool staPaused = false;
static uint32_t staRetryAt = 0;
static bool otaStarted = false;

void wifiPauseSta() { staPaused = true; }

void wifiResumeSta() {
  staPaused = false;
  staRetryAt = millis();
}

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
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setSleepMode(WIFI_SLEEP_CFG);
  DEBUG(F("WiFi sleep mode: "));
  DEBUGLN((int)WIFI_SLEEP_CFG);

#if USE_PIR
  pinMode(PIR_PIN, INPUT);
#endif

  otaBegin();
}

void otaBegin() {
  if (otaStarted) return;
  char host[32];
  snprintf(host, sizeof(host), "companyLamp-%s", lampId());
  WiFi.hostname(host);
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(LAMP_OTA_PASS);
  ArduinoOTA.onStart([]() { DEBUGLN(F("[OTA] start")); });
  ArduinoOTA.onEnd([]() { DEBUGLN(F("[OTA] end")); });
  ArduinoOTA.onError([](ota_error_t err) {
    DEBUG(F("[OTA] err "));
    DEBUGLN((int)err);
  });
  ArduinoOTA.begin();
  otaStarted = true;
  DEBUG(F("[OTA] host "));
  DEBUGLN(host);
}

void otaTick() {
  if (!otaStarted) otaBegin();
  if (otaStarted) ArduinoOTA.handle();
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

    if (startupButtonClicked()) {
      wifiPauseSta();
      return false;
    }
    otaTick();
  }

  return WiFiConnector.connected();
}

// AP остаётся поднятой, но лампа не ждёт роутер: после короткой синей индикации
// работают кнопка и режимы. Повтор STA — в wifiMaintain(), без блокировки loop.
static void startupRunApMode(bool keepSta) {
  startupLogState(F("AP_MODE"));
  if (!keepSta) {
    WiFiConnector.connect("", "");
  } else if (!WiFiConnector.connecting() && WiFi.status() != WL_CONNECTED) {
    if (cfg::ssid().length()) WiFiConnector.connect(cfg::ssid(), cfg::pass());
    else WiFiConnector.connect("", "");
  }

  const uint32_t until = millis() + 2000;
  const uint32_t ignoreClicksUntil = millis() + STARTUP_AP_IGNORE_BTN_MS;
  while ((int32_t)(millis() - until) < 0) {
    sett_loop();
    otaTick();
    loadAnimation(CRGB::Blue);
    if ((int32_t)(millis() - ignoreClicksUntil) >= 0) startupButtonClicked();
    yield();
  }
  staRetryAt = millis();
}

void wifiMaintain() {
  if (!sett_started()) return;

  const bool up = WiFi.status() == WL_CONNECTED;
  static bool wasUp = false;
  if (up && !wasUp) noteStaConnected();
  wasUp = up;

  WiFiMode_t mode = WiFi.getMode();
  const bool apUp = mode == WIFI_AP || mode == WIFI_AP_STA;
  if (up && cfg::close_ap() && WiFi.softAPgetStationNum() == 0 && apUp) {
    WiFi.softAPdisconnect(true);
    if (mode == WIFI_AP_STA) WiFi.mode(WIFI_STA);
    return;
  }
  if (up) return;

  const String ssid = cfg::ssid();
  if (staPaused || !ssid.length()) {
    if (!apUp) WiFiConnector.connect("", "");
    return;
  }
  if (WiFiConnector.connecting()) return;

  const uint32_t now = millis();
  const uint8_t stations = apUp ? WiFi.softAPgetStationNum() : 0;
  const uint32_t period = stations ? 120000UL : 30000UL;
  if ((uint32_t)(now - staRetryAt) < period) return;
  staRetryAt = now;

  // Пока к точке кто-то подключён, softAP заново не поднимаем: телефон на портале не отвалится.
  if (stations) {
    if (mode != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), cfg::pass().c_str());
    return;
  }
  WiFiConnector.connect(ssid, cfg::pass());
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
        if (startupWarmupWindow()) {
          wifiPauseSta();
          state = StartupState::AP_MODE;
        } else {
          state = StartupState::WIFI_CONNECT;
        }
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
        startupRunApMode(!staPaused);
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
