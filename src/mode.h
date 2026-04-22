#pragma once

#include <Arduino.h>

enum class Mode : uint8_t {
  LAMP     = 0,
  POMODORO = 1,
  MOOD     = 2,
  DND      = 3,
};
constexpr uint8_t MODE_COUNT = 4;

// Minimum brightness (%) — keeps background colors visible at lowest hold setting
#define BTN_MIN_BRIGHT 20

void togglePowerGlobal();
uint8_t globalBrightness255();
void applyGlobalBrightness(uint8_t effectScale255 = 255);
void globalBrightStep();
void globalBrightReleaseStep();
