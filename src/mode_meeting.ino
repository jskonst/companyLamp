#include "led_driver.h"
#include "sets.h"

static void showMeetingFlash() {
  uint8_t color = cfg::color();
  CRGB c = (color == 0) ? CRGB::White : (CRGB)CHSV(color, 255, 255);

  uint8_t base = cfg::power() ? map(cfg::bright(), 0, 100, 0, 255) : 0;
  uint8_t peak = (uint8_t)min((uint16_t)base + 26u, 255u);  // +10% от 255

  fill_solid(leds, LED_AMOUNT, c);
  FastLED.setBrightness(base);
  showLedsSafe();

  for (uint8_t n = 0; n < 5; n++) {
    brightLoop(base, peak, 2);
    brightLoop(peak, base, 2);
  }

  FastLED.setBrightness(base);
  showLedsSafe();
}

void meetingQuickCall() {
  DEBUGLN(F("[MTG] quick-call"));
  showMeetingFlash();
  if (!cfg::offline() && mqtt.connected()) {
    publishMeeting();
    DEBUGLN(F("[MTG] published"));
  }
}

void meetingTick() {
  String sender;
  if (takeMeetingSignal(sender)) {
    DEBUG(F("[MTG] incoming from ")); DEBUGLN(sender);
    showMeetingFlash();
  }
}
