#include "sets.h"

void brightLoop(uint8_t from, uint8_t to, uint8_t step) {
  int val = from;
  while (true) {
    FastLED.setBrightness((uint8_t)constrain(val, 0, 255));
    showLedsSafe();
    delay(10);
    if (from > to) {
      val -= step;
      if (val <= (int)to) return;
    } else {
      val += step;
      if (val >= (int)to) return;
    }
  }
}

void lampWink() {
  if (!cfg::power()) return;
  const uint8_t bright = globalBrightness255();
  brightLoop(bright, 0, 20);
  brightLoop(0, 255, 20);
  brightLoop(255, 0, 20);
  brightLoop(0, 255, 20);
  brightLoop(255, 0, 20);
  brightLoop(0, bright, 20);
}

void renderLamp() {
  static Timer tmr(30);
  static CRGB currentColor = CRGB::Black;

  if (!tmr.period()) return;

  bool power = cfg::power();
  uint8_t color = cfg::color();

  CRGB target;
  if (!power)          target = CRGB::Black;
  else if (color == 0) target = CRGB::White;
  else                 target = CHSV(color, 255, 255);

  if (currentColor != target) {
    currentColor.r += (int16_t)(target.r - currentColor.r) * 17 / 255;
    currentColor.g += (int16_t)(target.g - currentColor.g) * 17 / 255;
    currentColor.b += (int16_t)(target.b - currentColor.b) * 17 / 255;
    if (abs((int)currentColor.r - target.r) <= 1) currentColor.r = target.r;
    if (abs((int)currentColor.g - target.g) <= 1) currentColor.g = target.g;
    if (abs((int)currentColor.b - target.b) <= 1) currentColor.b = target.b;
  }

  fill_solid(leds, LED_AMOUNT, currentColor);
  applyGlobalBrightness();
  showLedsSafe();
}

void lampClick(uint8_t clicks) {
  if (clicks == 2) {
    db[kk::color] = (uint8_t)(cfg::color() + 32);
    db.update();
  }
  // clicks 3/4/5 перехватываются глобально в button.ino (смена режима)
  // lampWink() доступна для вызова из web UI
}
