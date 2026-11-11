#include "led_driver.h"
#include "sets.h"
#include "mode.h"

const CRGB MODE_COLORS[CYCLE_COUNT] = {
  CRGB(255, 200, 100),  // LAMP:     тёплый белый
  CRGB(255, 80,  0  ),  // POMODORO: оранжевый
  CRGB(0,   200, 0  ),  // MOOD:     зелёный (заменяется реальным цветом команды)
};

Mode currentMode() {
  return (Mode)(uint8_t)db[kk::mode];
}

void setMode(Mode m) {
  db[kk::mode] = (uint8_t)m;
  db.update();
}

static uint32_t _modeIndUntil = 0;
static bool _modeIndActive = false;

static constexpr uint8_t LEDS_PER_MODE = LED_AMOUNT / CYCLE_COUNT;  // 24/3 = 8

void showCustomIndicator(CRGB color, uint8_t brightness, uint16_t ms) {
  fill_solid(leds, LED_AMOUNT, color);
  FastLED.setBrightness(brightness);
  showLedsSafe();
  _modeIndActive = true;
  _modeIndUntil = millis() + ms;
}

void showModeIndicator(Mode m) {
  if ((uint8_t)m >= MODE_COUNT) return;
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  if (m == Mode::DND) {
    fill_solid(leds, LED_AMOUNT, CRGB::Red);
  } else {
    uint8_t base = (uint8_t)m * LEDS_PER_MODE;
    for (uint8_t i = base; i < base + LEDS_PER_MODE; i++) leds[i] = MODE_COLORS[(uint8_t)m];
  }
  FastLED.setBrightness(80);
  showLedsSafe();
  _modeIndActive = true;
  _modeIndUntil = millis() + 2000;
}

void modeIndicatorTick() {
  if (_modeIndActive && (int32_t)(millis() - _modeIndUntil) >= 0) {
    _modeIndActive = false;
    invalidateLampStateCache();
  }
}

bool isModeIndicatorActive() {
  return _modeIndActive;
}

static uint8_t _prevDbMode = 255;

// ─── global brightness / power ───────────────────────────────────────────────

static int8_t _brightDir = 10;

void togglePowerGlobal() {
  const bool nextPower = !cfg::power();
  db[kk::power] = nextPower;
  if (nextPower && cfg::bright() == 0) db[kk::bright] = BTN_MIN_BRIGHT;
  db.update();
}

uint8_t globalBrightness255() {
  return (uint8_t)map(cfg::bright(), 0, 100, 0, 255);
}

void applyGlobalBrightness(uint8_t effectScale255) {
  uint16_t v = (uint16_t)globalBrightness255() * effectScale255 / 255;
  FastLED.setBrightness((uint8_t)v);
}

void globalBrightStep() {
  int newBright = constrain((int)cfg::bright() + _brightDir, BTN_MIN_BRIGHT, 100);
  db[kk::bright] = (uint8_t)newBright;
  DEBUG(F("[BRIGHT] step=")); DEBUGLN(newBright);
}

void globalBrightReleaseStep() {
  _brightDir = -_brightDir;
  db.update();
  DEBUG(F("[BRIGHT] saved=")); DEBUGLN(cfg::bright());
}

void modeWebTick() {
    uint8_t current = (uint8_t)db[kk::mode];
    if (_prevDbMode != current) {
        if (_prevDbMode != 255) {
            showModeIndicator((Mode)current);
        }
        _prevDbMode = current;
        sett.reload();
    }
}

void renderTick() {
  if (isModeIndicatorActive()) return;
  switch (currentMode()) {
    case Mode::LAMP:     renderLamp();     break;
    case Mode::POMODORO: renderPomodoro(); break;
    case Mode::MOOD:     renderMood();     break;
    case Mode::DND:      renderDND();      break;
  }
}
