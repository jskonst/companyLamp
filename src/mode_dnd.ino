#include "led_driver.h"
#include "sets.h"

static constexpr uint8_t DND_SECTION_BASE = 18;  // LEDs 18-23 (mode index 3 * 6)
static constexpr uint8_t DND_SECTION_LEN  = 6;

void renderDND() {
  static Timer tmr(30);
  static uint8_t breath = 30;
  static int8_t  dir    = 1;

  if (!tmr.period()) return;

  if (!cfg::dnd()) {
    fill_solid(leds, LED_AMOUNT, CRGB::Black);
    for (uint8_t i = DND_SECTION_BASE; i < DND_SECTION_BASE + DND_SECTION_LEN; i++)
      leds[i] = CRGB(30, 0, 0);
    FastLED.setBrightness(40);
    showLedsSafe();
    return;
  }

  breath += dir;
  if (breath >= 100) { breath = 100; dir = -1; }
  if (breath <= 30)  { breath = 30;  dir =  1; }

  fill_solid(leds, LED_AMOUNT, CRGB::Red);
  FastLED.setBrightness(breath);
  showLedsSafe();
}

void dndClick(uint8_t clicks) {
  if (clicks != 1) return;
  bool next = !cfg::dnd();
  db[kk::dnd] = next;
  db.update();
  if (mqtt.connected() && !cfg::offline()) publishDND(next);
  DEBUG(F("[DND] ")); DEBUGLN(next ? "ON" : "OFF");
}

void dndStep() {}
void dndReleaseStep() {}
