#pragma once

#include <Arduino.h>

enum class Mode : uint8_t {
  LAMP     = 0,
  POMODORO = 1,
  MOOD     = 2,
  MEETING  = 3,
  DND      = 4,
};
constexpr uint8_t MODE_COUNT = 5;
