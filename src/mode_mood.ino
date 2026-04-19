// STUB: Plan 08 заменит этот файл полной реализацией

#include "led_driver.h"

void renderMood() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;
  fill_solid(leds, LED_AMOUNT, CRGB(0, 200, 0));
  FastLED.setBrightness(60);
  showLedsSafe();
}

void moodClick(uint8_t clicks) { (void)clicks; }
void moodStep() {}
void moodReleaseStep() {}
