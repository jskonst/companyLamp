// STUB: Plan 07 заменит этот файл полной реализацией

#include "led_driver.h"

void renderDND() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;
  fill_solid(leds, LED_AMOUNT, CRGB(200, 0, 0));
  FastLED.setBrightness(50);
  showLedsSafe();
}

void dndClick(uint8_t clicks) { (void)clicks; }
void dndStep() {}
void dndReleaseStep() {}
