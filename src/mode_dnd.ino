#include "led_driver.h"
#include "sets.h"

static uint8_t _preDNDMode = 0;

void toggleDND() {
  if (currentMode() == Mode::DND) {
    db[kk::mode] = _preDNDMode;
    db[kk::dnd]  = false;
    db.update();
    showModeIndicator((Mode)_preDNDMode);
    DEBUG(F("[DND] off -> mode ")); DEBUGLN(_preDNDMode);
    if (!cfg::offline() && mqtt.connected()) publishDND(false);
  } else {
    _preDNDMode = (uint8_t)currentMode();
    db[kk::mode] = (uint8_t)Mode::DND;
    db[kk::dnd]  = true;
    db.update();
    showModeIndicator(Mode::DND);
    DEBUG(F("[DND] on, saved mode ")); DEBUGLN(_preDNDMode);
    if (!cfg::offline() && mqtt.connected()) publishDND(true);
  }
}

void renderDND() {
  static Timer tmr(30);
  static uint8_t breath = 30;
  static int8_t  dir    = 1;

  if (!tmr.period()) return;

  if (!cfg::power()) {
    fill_solid(leds, LED_AMOUNT, CRGB::Black);
    FastLED.setBrightness(0);
    showLedsSafe();
    return;
  }

  breath += dir;
  if (breath >= 100) { breath = 100; dir = -1; }
  if (breath <= 30)  { breath = 30;  dir =  1; }

  fill_solid(leds, LED_AMOUNT, CRGB::Red);
  applyGlobalBrightness((uint8_t)map(breath, 0, 100, 0, 255));
  showLedsSafe();
}
