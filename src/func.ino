#include "led_driver.h"
#include "Timer.h"

extern void showLedsSafe();

void loadAnimation(CRGB color) {
  static int16_t val = 0;
  static int8_t dir = 1;
  static Timer tmr(20);

  if (tmr.period()) {
    val += dir;
    if (val >= 100) {
      val = 100;
      dir = -1;
    } else if (val <= 0) {
      val = 0;
      dir = 1;
    }

    fill_solid(leds, LED_AMOUNT, color);
    FastLED.setBrightness((uint8_t)val);
    showLedsSafe();
  }

  yield();
}
