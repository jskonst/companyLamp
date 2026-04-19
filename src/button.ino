#include "mode.h"

#if SERIAL_BTN_TEST
extern bool serialVisualOverride;
extern uint32_t serialVisualOverrideUntil;
extern void showLedsSafe();
#endif

static void handleButtonClicks(uint8_t clicks) {
  if (clicks >= 5) {
    meetingQuickCall();
    return;
  }
  if (clicks == 4) {
    uint8_t prev = ((uint8_t)currentMode() + MODE_COUNT - 1) % MODE_COUNT;
    setMode((Mode)prev);
    showModeIndicator((Mode)prev);
    DEBUG(F("[BTN] mode prev -> ")); DEBUGLN(prev);
    return;
  }
  if (clicks == 3) {
    uint8_t next = ((uint8_t)currentMode() + 1) % MODE_COUNT;
    setMode((Mode)next);
    showModeIndicator((Mode)next);
    DEBUG(F("[BTN] mode next -> ")); DEBUGLN(next);
    return;
  }
  switch (currentMode()) {
    case Mode::LAMP:     lampClick(clicks);     break;
    case Mode::POMODORO: pomodoroClick(clicks); break;
    case Mode::MOOD:     moodClick(clicks);     break;
    case Mode::MEETING:  meetingClick(clicks);  break;
    case Mode::DND:      dndClick(clicks);      break;
  }
}

#if SERIAL_BTN_TEST
void buttonSerialTestTick() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '1' || c == '2' || c == '3' || c == '4' || c == '5') {
      serialVisualOverride = false;
      serialVisualOverrideUntil = 0;
      handleButtonClicks((uint8_t)(c - '0'));
      renderTick();
      Serial.printf("SIM click %c | mode=%u power=%d bright=%u color=%u\n",
        c, (unsigned)currentMode(), (int)cfg::power(), (unsigned)cfg::bright(), (unsigned)cfg::color());
    } else if (c == 'r' || c == 'R') {
      serialVisualOverride = true;
      serialVisualOverrideUntil = millis() + 5000;
      fill_solid(leds, LED_AMOUNT, CRGB::Red);
      FastLED.setBrightness(96);
      showLedsSafe();
      Serial.println(F("SIM direct RED all LEDs"));
    } else if (c == 'g' || c == 'G') {
      serialVisualOverride = true;
      serialVisualOverrideUntil = millis() + 5000;
      fill_solid(leds, LED_AMOUNT, CRGB::Green);
      FastLED.setBrightness(96);
      showLedsSafe();
      Serial.println(F("SIM direct GREEN all LEDs"));
    } else if (c == 'w' || c == 'W') {
      serialVisualOverride = true;
      serialVisualOverrideUntil = millis() + 5000;
      fill_solid(leds, LED_AMOUNT, CRGB::White);
      FastLED.setBrightness(96);
      showLedsSafe();
      Serial.println(F("SIM direct WHITE all LEDs"));
    } else if (c == 'k' || c == 'K' || c == '0') {
      serialVisualOverride = true;
      serialVisualOverrideUntil = millis() + 5000;
      fill_solid(leds, LED_AMOUNT, CRGB::Black);
      FastLED.setBrightness(0);
      showLedsSafe();
      delay(2);
      showLedsSafe();
      Serial.println(F("SIM direct BLACK all LEDs"));
    } else if (c == 't' || c == 'T') {
      serialVisualOverride = true;
      serialVisualOverrideUntil = millis() + (uint32_t)LED_AMOUNT * 120 + 1500;
      FastLED.setBrightness(96);
      fill_solid(leds, LED_AMOUNT, CRGB::Black);
      showLedsSafe();
      for (uint8_t i = 0; i < LED_AMOUNT; i++) {
        fill_solid(leds, LED_AMOUNT, CRGB::Black);
        leds[i] = CRGB::Blue;
        showLedsSafe();
        delay(120);
      }
      Serial.println(F("SIM direct CHASE done"));
    }
  }
}
#endif

void buttonTick() {
  btn.tick();

  if (btn.press())   DEBUGLN(F("[BTN] press"));
  if (btn.release()) DEBUGLN(F("[BTN] release"));

  if (btn.hasClicks()) {
    uint8_t clicks = btn.getClicks();
    DEBUG(F("[BTN] click n=")); DEBUGLN(clicks);
    handleButtonClicks(clicks);
  }

  if (btn.step()) {
    switch (currentMode()) {
      case Mode::LAMP: lampStep(); break;
      case Mode::MOOD: moodStep(); break;
      default: break;
    }
  }
  if (btn.releaseStep()) {
    switch (currentMode()) {
      case Mode::LAMP: lampReleaseStep(); break;
      case Mode::MOOD: moodReleaseStep(); break;
      default: break;
    }
  }
}
