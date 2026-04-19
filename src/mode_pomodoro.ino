// STUB: Plan 10 заменит этот файл полной реализацией

#include "led_driver.h"

void renderPomodoro() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;
  fill_solid(leds, LED_AMOUNT, CRGB(255, 80, 0));
  FastLED.setBrightness(60);
  showLedsSafe();
}

void pomodoroClick(uint8_t clicks) { (void)clicks; }
void pomodoroStep() {}
void pomodoroReleaseStep() {}
