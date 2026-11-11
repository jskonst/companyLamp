#include "led_driver.h"
#include "sets.h"

static void showMeetingFlash(bool allowWhenOff) {
  uint8_t color = cfg::color();
  CRGB c = (color == 0) ? CRGB::White : (CRGB)CHSV(color, 255, 255);

  uint8_t restore = cfg::power() ? globalBrightness255() : 0;
  uint8_t base = restore;
  if (!cfg::power() && allowWhenOff) {
    base = (uint8_t)max((uint16_t)globalBrightness255(), (uint16_t)map(BTN_MIN_BRIGHT, 0, 100, 0, 255));
  }
  if (base == 0) return;

  uint8_t peak = (uint8_t)min((uint16_t)base + 26u, 255u);

  fill_solid(leds, LED_AMOUNT, c);
  FastLED.setBrightness(base);
  showLedsSafe();

  for (uint8_t n = 0; n < 5; n++) {
    brightLoop(base, peak, 2);
    brightLoop(peak, base, 2);
  }

  FastLED.setBrightness(restore);
  showLedsSafe();
}

void meetingQuickCall() {
  DEBUGLN(F("[MTG] quick-call"));
  showMeetingFlash(true);
  if (!cfg::offline() && mqtt.connected()) {
    publishMeeting();
    DEBUGLN(F("[MTG] published"));
  }
}

void meetingTick() {
  String sender;
  if (takeMeetingSignal(sender)) {
    DEBUG(F("[MTG] incoming from ")); DEBUGLN(sender);
    if (!cfg::dndWink()) {
      DEBUGLN(F("[MTG] wink ignored by settings"));
      return;
    }
    showMeetingFlash(true);
  }
}
