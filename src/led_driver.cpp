#include "led_driver.h"

const ColorCorrectionTag TypicalLEDStrip = {};
const ColorCorrectionTag UncorrectedColor = {};

const CRGB CRGB::Black = CRGB(0, 0, 0);
const CRGB CRGB::White = CRGB(255, 255, 255);
const CRGB CRGB::Red = CRGB(255, 0, 0);
const CRGB CRGB::Green = CRGB(0, 255, 0);
const CRGB CRGB::Blue = CRGB(0, 0, 255);

FastLEDClass FastLED;

static uint8_t scale8(uint8_t value, uint8_t scale) {
  return (uint16_t)value * (uint16_t)scale / 255;
}

CRGB CHSV(uint8_t h, uint8_t s, uint8_t v) {
  if (s == 0) return CRGB(v, v, v);

  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;

  uint8_t p = (uint16_t)v * (255 - s) / 255;
  uint8_t q = (uint16_t)v * (255 - ((uint16_t)s * remainder / 255)) / 255;
  uint8_t t = (uint16_t)v * (255 - ((uint16_t)s * (255 - remainder) / 255)) / 255;

  switch (region) {
    case 0: return CRGB(v, t, p);
    case 1: return CRGB(q, v, p);
    case 2: return CRGB(p, v, t);
    case 3: return CRGB(p, q, v);
    case 4: return CRGB(t, p, v);
    default: return CRGB(v, p, q);
  }
}

void fill_solid(CRGB* arr, uint16_t count, const CRGB& color) {
  for (uint16_t i = 0; i < count; i++) arr[i] = color;
}

FastLEDClass::FastLEDClass() : strip(nullptr), buffer(nullptr), ledCount(0), brightness(255) {}

FastLEDClass::~FastLEDClass() {
  if (strip) delete strip;
}

void FastLEDClass::init(CRGB* arr, uint16_t count, uint8_t pin) {
  buffer = arr;
  ledCount = count;

  if (strip) {
    delete strip;
    strip = nullptr;
  }

  strip = new StripType(ledCount, pin);
  strip->Begin();
  strip->Show();
}

void FastLEDClass::setBrightness(uint8_t value) {
  brightness = value;
}

uint8_t FastLEDClass::getBrightness() const {
  return brightness;
}

void FastLEDClass::clear() {
  if (!buffer) return;
  fill_solid(buffer, ledCount, CRGB::Black);
}

// WS2812 ≈ 60 мА на полный белый (R+G+B = 765). Кадр не должен просить больше 800 мА при 5 В.
// NeoPixelBus сам ток не режет: если сумма каналов после яркости выше порога, кадр масштабируется.
static constexpr uint32_t kMilliampLimit = 800;
static constexpr uint32_t kMilliampFullWhite = 60;
static constexpr uint32_t kChannelSumFullWhite = 255ul * 3ul;
static constexpr uint32_t kLimitChannelSum =
    kMilliampLimit * kChannelSumFullWhite / kMilliampFullWhite;  // 10200

void FastLEDClass::show() {
  if (!strip || !buffer) return;

  uint32_t sum = 0;
  for (uint16_t i = 0; i < ledCount; i++) {
    const CRGB& c = buffer[i];
    sum += scale8(c.r, brightness);
    sum += scale8(c.g, brightness);
    sum += scale8(c.b, brightness);
  }
  const bool limit = sum > kLimitChannelSum;

  for (uint16_t i = 0; i < ledCount; i++) {
    const CRGB& c = buffer[i];
    uint32_t r = scale8(c.r, brightness);
    uint32_t g = scale8(c.g, brightness);
    uint32_t b = scale8(c.b, brightness);
    if (limit) {
      r = r * kLimitChannelSum / sum;
      g = g * kLimitChannelSum / sum;
      b = b * kLimitChannelSum / sum;
    }
    strip->SetPixelColor(i, RgbColor((uint8_t)r, (uint8_t)g, (uint8_t)b));
  }
  strip->Show();
}

