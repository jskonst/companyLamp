#pragma once

#include <Arduino.h>

enum class Mode : uint8_t {
  LAMP     = 0,
  POMODORO = 1,
  MOOD     = 2,
  DND      = 3,
};
constexpr uint8_t MODE_COUNT  = 4;  // total modes
constexpr uint8_t CYCLE_COUNT = 3;  // modes reachable via 3-click cycling

// Minimum brightness (%) — keeps background colors visible at lowest hold setting
#define BTN_MIN_BRIGHT 20

void toggleDND();
void togglePowerGlobal();
uint8_t globalBrightness255();
void applyGlobalBrightness(uint8_t effectScale255 = 255);
void globalBrightStep();
void globalBrightReleaseStep();
