void startup() {
  delay(1000);
  Serial.begin(115200);
  portal.attachBuild(build);

  EEPROM.begin(sizeof(LampData) + 8);
  // 'b' — группа. Старый ключ 'a' был парой AG_lamp_1/2 и публичным брокером.
  // EEManager при несовпадении записывает значения по умолчанию из LampData.
  uint8_t mem = memory.begin(0, 'b');
  if (mem == 1) DEBUGLN(F("EEPROM: новый ключ, настройки сброшены"));
  else if (mem == 2) DEBUGLN(F("EEPROM: не хватило места"));

  mqttBegin();

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_AMOUNT).setCorrection(TypicalLEDStrip);
  // 24 WS2812 от boost 5 В с одной 18650. Полный белый был бы ~1.4 А — режем до 800 мА.
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 800);
  FastLED.setBrightness(50);
  FastLED.show();

  // Между кликами ~0.8 с, иначе 4 и 5 кликов не успеть. Шаг яркости ~100 мс.
  btn.setClickTimeout(800);
  btn.setStepTimeout(100);
  if (USE_PIR) pinMode(PIR_PIN, INPUT);

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

  wifiBegin(forceAP);

  portal.start(deviceHost);
  portal.enableOTA(F("lamp"), F(LAMP_OTA_PASS));

  ArduinoOTA.setHostname(deviceHost);
  ArduinoOTA.setPassword(LAMP_OTA_PASS);
  ArduinoOTA.onStart([]() { DEBUGLN(F("OTA start")); });
  ArduinoOTA.onEnd([]() { DEBUGLN(F("OTA end")); });
  ArduinoOTA.onError([](ota_error_t err) {
    DEBUG(F("OTA err "));
    DEBUGLN((int)err);
  });
  ArduinoOTA.begin();

  FastLED.setBrightness(data.bright);
  DEBUG(F("host "));
  DEBUG(deviceHost);
  DEBUG(F(" group "));
  DEBUGLN(data.group[0] ? data.group : "(локально)");
}
