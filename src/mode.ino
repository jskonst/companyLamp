#include "led_driver.h"
#include "sets.h"
#include "mode.h"

const CRGB MODE_COLORS[MODE_COUNT] = {
  CRGB(255, 200, 100),  // LAMP:     тёплый белый
  CRGB(255, 80,  0  ),  // POMODORO: оранжевый
  CRGB(0,   200, 0  ),  // MOOD:     зелёный (заменяется реальным цветом команды)
  CRGB(0,   200, 255),  // MEETING:  голубой
  CRGB(200, 0,   0  ),  // DND:      красный
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

void showModeIndicator(Mode m) {
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  uint8_t base = (uint8_t)m * 4;
  for (uint8_t i = base; i < base + 4; i++) leds[i] = MODE_COLORS[(uint8_t)m];
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
    case Mode::MEETING:  renderMeeting();  break;
    case Mode::DND:      renderDND();      break;
  }
}
