#include "sets.h"

extern void applyLampState();

#define BTN_MIN_BRIGHT 5

static int8_t _lampBrightDir = 10;

static void togglePowerSafe() {
  const bool nextPower = !cfg::power();
  db[kk::power] = nextPower;
  if (nextPower && cfg::bright() == 0) db[kk::bright] = 20;
  db.update();
}

void renderLamp() {
  applyLampState();
}

void lampClick(uint8_t clicks) {
  if (clicks == 1) {
    togglePowerSafe();
  } else if (clicks == 2) {
    db[kk::color] = (uint8_t)(cfg::color() + 32);
    db.update();
  }
  // clicks == 3: wink (Plan 05)
}

void lampStep() {
  int newBright = constrain((int)cfg::bright() + _lampBrightDir, BTN_MIN_BRIGHT, 100);
  db[kk::bright] = (uint8_t)newBright;
  DEBUG(F("[LAMP] step bright=")); DEBUGLN(newBright);
}

void lampReleaseStep() {
  _lampBrightDir = -_lampBrightDir;
  db.update();
  DEBUG(F("[LAMP] releaseStep saved bright=")); DEBUGLN(cfg::bright());
}
