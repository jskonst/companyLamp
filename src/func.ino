// Подмигнуть чужим цветом ~1.2 с (две вспышки). 0 в winkStartMs значит «сейчас не мигаем».
void winkFrom(uint8_t hue) {
  winkHue = hue;
  winkStartMs = millis() | 1;
}

int winkBright() {
  if (!winkStartMs) return -1;
  uint32_t t = millis() - winkStartMs;
  if (t > 1200) {
    winkStartMs = 0;
    return -1;
  }
  uint32_t ph = t % 600;
  return ph < 300 ? 255 - ph * 255 / 300 : (ph - 300) * 255 / 300;
}

// В автономном режиме среднее не показываем: чужие состояния не обновляются.
uint8_t ringMood() {
  if (!data.average || data.autonomous) return data.mood;
  MoodSample samples[PEER_MAX];
  int n = 0;
  for (int i = 0; i < peerCount; i++) {
    if (!peers[i].hasMood || peers[i].status != PeerStatus::Online) continue;
    samples[n].mood = peers[i].mood;
    samples[n].online = true;
    n++;
  }
  uint8_t avg = data.mood;
  // Своя лампа online, пока мы не в автономном режиме — её m входит в среднее.
  if (!averageMood(samples, n, data.mood, true, &avg)) return data.mood;
  return avg;
}

bool othersOnline() {
  if (data.autonomous) return false;
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].status == PeerStatus::Online) return true;
  }
  return false;
}

void animation() {
  static Timer tmr(30);
  static bool breath = true;
  static uint8_t count = 0;
  static CRGB shown;

  if (!tmr.period()) return;

  count++;
  if (othersOnline()) {
    if (count % 30 == 0) breath = !breath;
  } else {
    breath = true;
  }

  int wb = winkBright();
  CRGB target;
  uint8_t br;
  if (wb >= 0) {
    // Чужой цвет только на время вспышки, своё настроение не трогаем.
    target = CHSV(winkHue, 255, 255);
    shown = target;
    br = (uint8_t)wb;
  } else if (!data.power) {
    target = CRGB::Black;
    shown = target;
    br = 0;
  } else {
    uint8_t breathBr = breath ? 255 : 210;
    target = CHSV(moodToHue(ringMood()), 255, breathBr);
    br = data.bright;
    if (shown != target) shown = blend(shown, target, 17);
  }

  fill_solid(leds, LED_AMOUNT, shown);
  if (apActive && ((millis() / 500) % 2)) leds[0] = CRGB::Blue;
  // Автономный режим: последний светодиод тускло-белый, даже если кольцо выключено.
  if (data.autonomous) {
    leds[LED_AMOUNT - 1] = CRGB(80, 80, 80);
    if (wb < 0 && br < 40) br = 40;
  }
  if (wb < 0 && maxFlashMs && millis() - maxFlashMs < 150) br = 0;
  FastLED.setBrightness(br);
  FastLED.show();
}

void loadAnimation(CRGB color) {
  static int8_t dir = 1;
  static uint8_t val = 0;
  static Timer tmr(20);
  if (tmr.period()) {
    val += dir;
    if (val >= 100 || val <= 0) dir = -dir;
    fill_solid(leds, LED_AMOUNT, color);
    FastLED.setBrightness(val);
    FastLED.show();
  }
  yield();
}
