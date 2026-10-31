void startup() {
  delay(1000);
  Serial.begin(115200);         // запускаем сериал для отладки
  portal.attachBuild(build);  // подключаем интерфейс

  EEPROM.begin(sizeof(data) + 1); // +1 на ключ
  memory.begin(0, 'a');           // запускаем менеджер памяти

  // запускаем ленту
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_AMOUNT).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(50);
  FastLED.show();

  // настраиваем уровень кнопки
  btn.setButtonLevel(BTN_LEVEL);

  // 2 секунды, чтобы юзер успел кликнуть и принудительно включить точку доступа
  bool forceAP = false;
  Timer tmr(2000);
  while (!tmr.period()) {
    loadAnimation(CRGB::Green);
    btn.tick();
    if (btn.click()) forceAP = true;
  }
  btn.reset();

  FastLED.clear();
  FastLED.show();

  mqtt.setServer(data.host, data.port);
  mqtt.setCallback(callback);
  mqtt.setSocketTimeout(2);   // не висеть долго, если брокер недоступен
  espClient.setTimeout(2000);
  randomSeed(micros());

  // Wi-Fi стартует в фоне; дальше всё в wifiTick() без блокировок
  wifiBegin(forceAP);

  // портал работает на любом интерфейсе (STA и AP) на порту 80
  portal.start("companylamp");  // mDNS: http://companylamp.local

  FastLED.setBrightness(data.bright);
}
