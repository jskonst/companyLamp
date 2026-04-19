extern bool serialVisualOverride;
extern uint32_t serialVisualOverrideUntil;
extern void applyLampState();
extern void showLedsSafe();

static void exitSerialVisualOverride() {
  serialVisualOverride = false;
  serialVisualOverrideUntil = 0;
}

static void togglePowerSafe() {
  const bool nextPower = !cfg::power();
  db[kk::power] = nextPower;
  // If we are turning on from zero brightness, restore a visible level.
  if (nextPower && cfg::bright() == 0) {
    db[kk::bright] = 20;
  }
  db.update();
}

static void handleButtonClicks(uint8_t clicks) {
  if (clicks == 1) {
    togglePowerSafe();
  } else if (clicks == 2) {
    db[kk::color] = (uint8_t)(cfg::color() + 32);
    db.update();
  } else if (clicks == 3) {
    DEBUGLN(F("[BTN] TODO: next mode"));
  } else if (clicks >= 4) {
    DEBUGLN(F("[BTN] TODO: prev mode"));
  }
}

void buttonSerialTestTick() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '1' || c == '2' || c == '3') {
      exitSerialVisualOverride();
      handleButtonClicks((uint8_t)(c - '0'));
      applyLampState();
      Serial.printf("SIM click %c | power=%d bright=%u color=%u\n", c, (int)cfg::power(), (unsigned)cfg::bright(), (unsigned)cfg::color());
    } else if (c == 'p' || c == 'P') {
      exitSerialVisualOverride();
      togglePowerSafe();
      applyLampState();
      Serial.printf("SIM power toggle | power=%d bright=%u color=%u\n", (int)cfg::power(), (unsigned)cfg::bright(), (unsigned)cfg::color());
    } else if (c == '+') {
      exitSerialVisualOverride();
      db[kk::bright] = (uint8_t)constrain((int)cfg::bright() + 10, 0, 100);
      db.update();
      applyLampState();
      Serial.printf("SIM bright+ | bright=%u\n", (unsigned)cfg::bright());
    } else if (c == '-') {
      exitSerialVisualOverride();
      db[kk::bright] = (uint8_t)constrain((int)cfg::bright() - 10, 0, 100);
      db.update();
      applyLampState();
      Serial.printf("SIM bright- | bright=%u\n", (unsigned)cfg::bright());
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
      delayMicroseconds(80);
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

void buttonTick() {
  btn.tick();

  if (btn.press())   DEBUGLN(F("[BTN] press"));
  if (btn.release()) DEBUGLN(F("[BTN] release"));

  if (btn.hasClicks()) {
    uint8_t clicks = btn.getClicks();
    DEBUG(F("[BTN] click n=")); DEBUGLN(clicks);
    handleButtonClicks(clicks);
    Serial.printf("[BTN] after: power=%d bright=%u color=%u\n",
      (int)cfg::power(), (unsigned)cfg::bright(), (unsigned)cfg::color());
  }

  static int8_t dir = 10;
  if (btn.step()) {
    int newBright = constrain((int)cfg::bright() + dir, 0, 100);
    db[kk::bright] = (uint8_t)newBright;
    DEBUG(F("[BTN] step dir=")); DEBUG(dir);
    DEBUG(F(" bright=")); DEBUGLN(newBright);
  }
  if (btn.releaseStep()) {
    dir = -dir;
    db.update();
    DEBUG(F("[BTN] releaseStep saved bright=")); DEBUG(cfg::bright());
    DEBUG(F(" nextDir=")); DEBUGLN(dir);
  }
}
