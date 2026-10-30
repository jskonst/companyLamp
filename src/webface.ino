void build() {
  GP.BUILD_BEGIN();

  GP.THEME(GP_DARK);
  GP.UPDATE("ledW,ledL,ledR,ledP,sw,br,col", 2000);

  GP.LABEL("STATUS");
  GP.BLOCK_BEGIN();
  GP.LABEL("Wi-Fi:");
  GP.LED_GREEN("ledW", staConnected());
  GP.LABEL(staConnected() ? WiFi.localIP().toString() : String(F("нет связи")));
  GP.BREAK();
  if (apActive) { GP.LABEL(String(F("Точка: ")) + apName() + F(" / 192.168.4.1")); GP.BREAK(); }
  GP.LABEL("MQTT:");
  GP.LED_GREEN("ledL", mqtt.connected());
  GP.LABEL("Remote:");
  GP.LED_GREEN("ledR", !onlineTmr.elapsed());
  GP.BREAK();
  GP.LABEL("Remote PIR:");
  GP.LED_RED("ledP", (!pirTmr.elapsed() && !onlineTmr.elapsed()));
  GP.BLOCK_END();

  GP.LABEL("SETTINGS");
  GP.BLOCK_BEGIN();
  GP.LABEL("Power:");
  GP.SWITCH("sw", data.power);
  GP.BREAK();
  GP.LABEL("Bright:");
  GP.SLIDER("br", data.bright, 0, 255);
  GP.BREAK();
  GP.LABEL("Color:");
  GP.SLIDER("col", data.color, 0, 255);
  GP.BLOCK_END();

  GP.FORM_BEGIN("/save");

  GP.LABEL("WIFI");
  GP.BLOCK_BEGIN();
  GP.TEXT("ssid", "SSID", data.ssid);
  GP.BREAK();
  GP.PASS("pass", "Password", data.pass);
  GP.BLOCK_END();

  GP.LABEL("MQTT");
  GP.BLOCK_BEGIN();
  GP.TEXT("local", "Local Name", data.local);
  GP.BREAK();
  GP.TEXT("remote", "Remote Name", data.remote);
  GP.BREAK();
  GP.TEXT("host", "Host", data.host);
  GP.BREAK();
  GP.NUMBER("port", "Port", data.port);
  GP.BLOCK_END();
  GP.SUBMIT("Save");
  GP.BREAK();
  GP.BUTTON("rec", "Переподключить Wi-Fi");
  GP.BUTTON("ap", "Включить точку доступа");

  GP.FORM_END();

  GP.BUILD_END();
}
