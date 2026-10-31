// подмигнуть: неблокирующе, анимацию ведёт animation() (было: brightLoop с delay ~1 с)
uint32_t winkStartMs = 0;
void wink() {
  if (data.power) winkStartMs = millis() | 1;
}

// яркость подмигивания: 2 вспышки за 1.2 с; -1 = не подмигиваем
int winkBright() {
  if (!winkStartMs) return -1;
  uint32_t t = millis() - winkStartMs;
  if (t > 1200) { winkStartMs = 0; return -1; }
  uint32_t ph = t % 600;
  return ph < 300 ? 255 - ph * 255 / 300 : (ph - 300) * 255 / 300;
}

// выводим эффект на ленту
void animation() {
  static Timer tmr(30);
  static bool breath;   // здесь отвечает за погашение яркости для дыхания
  static uint8_t count; // счётчик-пропуск периодов

  if (tmr.period()) {
    // переключаем локальную яркость для "дыхания"
    count++;
    if (!onlineTmr.elapsed()) {   // удалённая лампа онлайн
      if (!pirTmr.elapsed()) {    // сработал ИК на удалённой
        if (count % 10 == 0) breath = !breath;
      } else {
        if (count % 30 == 0) breath = !breath;
      }
    } else {
      breath = 1;
    }    
    uint8_t curBr = data.power ? (breath ? 255 : 210) : 0;

    // здесь делаем плавные переходы между цветами
    CRGB ncol = CHSV(data.color, 255, curBr);
    CRGB ccol = leds[LED_AMOUNT - 1];  // leds[0] может быть индикатором AP
    if (ccol != ncol) ccol = blend(ccol, ncol, 17);

    // выводим на ленту
    fill_solid(leds, LED_AMOUNT, ccol);
    // индикатор точки доступа: первый светодиод мигает синим
    if (apActive && (millis() / 500) % 2) leds[0] = CRGB::Blue;
    extern uint32_t maxFlashMs;
    int wb = winkBright();
    uint8_t br = data.bright;
    if (wb >= 0) br = wb;
    else if (maxFlashMs && millis() - maxFlashMs < 150) br = 0;
    FastLED.setBrightness(br);
    FastLED.show();
  }
}

// локальный запуск портала. При любом исходе заканчивается ресетом платы
// void localPortal(IPAddress ip) {
//   // создаём точку с именем WLamp и предыдущим успешным IP
//   Serial.println(F("Create AP"));
//   WiFi.mode(WIFI_AP);
//   String s(F("WLamp "));
//   s += ip.toString();
//   WiFi.softAP(s);

//   portal.start("WLamp");    // запускаем портал
//   while (portal.tick()) {   // портал работает
//     loadAnimation(CRGB::Blue);      // мигаем синим
//     btn.tick();
//     // если нажали сохранить настройки или кликнули по кнопке
//     // перезагружаем ESP
//     if (checkPortal() || btn.click()) ESP.reset();
//   }
// }


// анимация работы локал портала
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

// цыганский парсер инта из указанного индекса в строке
int getFromIndex(char* str, int idx, char div = ',') {
  int val = 0;
  uint16_t i = 0;
  int count = 0;
  bool sign = 0;
  while (str[i]) {
    if (idx == count) {
      if (str[i] == div) break;
      if (str[i] == '-') sign = -1;
      else {
        val *= 10L;
        val += str[i] - '0';
      }
    } else if (str[i] == div) count++;
    i++;
  }
  return sign ? -val : val;
}
