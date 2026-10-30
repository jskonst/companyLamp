void startup() {
  Serial.begin(115200);
  EEPROM.begin(sizeof(data) + 1);  // +1 на ключ
  memory.begin(0, 'a');            // настройки (Wi-Fi, MQTT, яркость, цвет) в EEPROM

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_AMOUNT).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(50);
  FastLED.show();
  btn.setButtonLevel(BTN_LEVEL);

  // Wi-Fi без блокирующих циклов: STA в фоне + точка доступа, если роутера нет.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.hostname("companylamp");
  wifiBegin();

  mqtt.setServer(data.host, data.port);
  mqtt.setCallback(callback);
  mqtt.setSocketTimeout(2);
  espClient.setTimeout(2000);
  randomSeed(micros());

  portal.attachBuild(build);
  portal.start("companylamp");  // веб-интерфейс + mDNS companylamp.local; captive DNS — свой, в portal.ino

  FastLED.setBrightness(data.bright);
}
