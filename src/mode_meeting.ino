// STUB: Plan 09 заменит этот файл полной реализацией.
// meetingQuickCall() — уже рабочая (визуальная часть), MQTT добавит Plan 06+.

#include "led_driver.h"

void renderMeeting() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;
  fill_solid(leds, LED_AMOUNT, CRGB(0, 200, 255));
  FastLED.setBrightness(60);
  showLedsSafe();
}

void meetingClick(uint8_t clicks) { (void)clicks; }

static void showMeetingFlash() {
  for (uint8_t n = 0; n < 3; n++) {
    fill_solid(leds, LED_AMOUNT, CRGB(0, 180, 255));
    FastLED.setBrightness(100);
    showLedsSafe();
    delay(100);
    fill_solid(leds, LED_AMOUNT, CRGB::Black);
    FastLED.setBrightness(0);
    showLedsSafe();
    delay(100);
  }
}

void meetingQuickCall() {
  // TODO (Plan 06+): проверить !cfg::offline() && mqtt.connected() перед отправкой
  DEBUGLN(F("[MTG] quick-call"));
  showMeetingFlash();
}
