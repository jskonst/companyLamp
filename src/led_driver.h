#pragma once

#include <Arduino.h>
#include <NeoPixelBus.h>

struct ColorCorrectionTag {};
extern const ColorCorrectionTag TypicalLEDStrip;
extern const ColorCorrectionTag UncorrectedColor;

struct NEOPIXEL {};
struct WS2812 {};
struct WS2812B {};
constexpr uint8_t GRB = 0;

struct CRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  constexpr CRGB() : r(0), g(0), b(0) {}
  constexpr CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

  bool operator!=(const CRGB& other) const {
    return r != other.r || g != other.g || b != other.b;
  }

  static const CRGB Black;
  static const CRGB White;
  static const CRGB Red;
  static const CRGB Green;
  static const CRGB Blue;
};

CRGB CHSV(uint8_t h, uint8_t s, uint8_t v);
void fill_solid(CRGB* arr, uint16_t count, const CRGB& color);

class FastLEDClass {
 public:
  FastLEDClass();
  ~FastLEDClass();

  template <typename CHIPSET, uint8_t PIN>
  FastLEDClass& addLeds(CRGB* arr, uint16_t count) {
    init(arr, count, PIN);
    return *this;
  }

  template <typename CHIPSET, uint8_t PIN, uint8_t ORDER>
  FastLEDClass& addLeds(CRGB* arr, uint16_t count) {
    (void)ORDER;
    init(arr, count, PIN);
    return *this;
  }

  FastLEDClass& setCorrection(const ColorCorrectionTag&) {
    return *this;
  }

  void setDither(uint8_t) {}
  void setBrightness(uint8_t value);
  uint8_t getBrightness() const;
  void clear();
  void show();

 private:
  void init(CRGB* arr, uint16_t count, uint8_t pin);

  using StripType = NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod>;
  StripType* strip;
  CRGB* buffer;
  uint16_t ledCount;
  uint8_t brightness;
};

extern FastLEDClass FastLED;

void showCustomIndicator(CRGB color, uint8_t brightness, uint16_t ms);

