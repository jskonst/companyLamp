// Раскладка кликов — GESTURE_* в group_logic.h. Удержание яркости здесь не задаётся числом.

void buttonTick() {
  btn.tick();

  if (btn.hasClicks()) {
    uint8_t clicks = btn.getClicks();
    if (clicks == GESTURE_WINK) {
      publishWink();
    } else if (clicks == GESTURE_MOOD) {
      data.mood = nextMoodStep(data.mood);
      memory.update();
      publishState();
    } else if (clicks == GESTURE_POWER) {
      data.power = !data.power;
      memory.update();
    } else if (clicks == GESTURE_AVERAGE) {
      data.average = !data.average;
      memory.update();
      peersDirty = true;
    } else if (clicks == GESTURE_AUTONOMOUS) {
      setAutonomous(!data.autonomous);
    }
  }

  static int8_t dir = 10;
  if (btn.step()) {
    data.bright = constrain(data.bright + dir, 0, 255);
    if (data.bright == 255) maxFlashMs = millis();
  }
  if (btn.releaseStep()) {
    dir = -dir;
    memory.update();
  }
}
