void build() {
  rebuildPeerText();
  String ipText = staConnected() ? WiFi.localIP().toString() : String(F("нет связи"));
  if (!staConnected() && apActive) ipText = F("192.168.4.1");

  GP.BUILD_BEGIN();
  GP.THEME(GP_DARK);
  GP.UPDATE("ledW,ledM,onl,peers,sw,br,mood,avg,aut", 2000);

  GP.LABEL(String(F("Лампа ")) + lampId + F(" · ") + deviceHost + F(".local"));
  if (portalNote[0]) {
    GP.LABEL(portalNote);
    GP.BREAK();
  }

  GP.LABEL("СВЯЗЬ");
  GP.BLOCK_BEGIN();
  GP.LABEL("Wi-Fi:");
  GP.LED_GREEN("ledW", staConnected());
  GP.LABEL(ipText);
  GP.BREAK();
  if (apActive) {
    GP.LABEL(String(F("Точка: ")) + apName() + F(" / 192.168.4.1"));
    GP.BREAK();
  }
  GP.LABEL("MQTT:");
  GP.LED_GREEN("ledM", mqtt.connected() && !data.autonomous);
  GP.LABEL(brokerConfigured() ? String(data.host) : String(F("не настроен")));
  GP.BREAK();
  GP.LABEL("Онлайн:");
  GP.LABEL(String(onlineCount), "onl");
  GP.BREAK();
  GP.AREA("peers", 8, peerText);
  GP.BLOCK_END();

  GP.LABEL("ЛАМПА");
  GP.BLOCK_BEGIN();
  GP.LABEL("Питание:");
  GP.SWITCH("sw", data.power);
  GP.BREAK();
  GP.LABEL("Яркость:");
  GP.SLIDER("br", data.bright, 0, 255);
  GP.BREAK();
  GP.LABEL("Настроение 0 красный — 100 зелёный:");
  GP.SLIDER("mood", data.mood, 0, 100);
  GP.BREAK();
  GP.LABEL("Среднее группы:");
  GP.SWITCH("avg", data.average);
  GP.BREAK();
  GP.LABEL("Автономный режим:");
  GP.SWITCH("aut", data.autonomous);
  GP.BLOCK_END();

  GP.FORM_BEGIN("/save");

  GP.LABEL("WI-FI");
  GP.BLOCK_BEGIN();
  GP.TEXT("ssid", "SSID", data.ssid, "", 31);
  GP.BREAK();
  // Пароль в value не кладём: иначе он уедет в HTML страницы.
  GP.PASS("pass", "Пароль, пусто = не менять", "", "", 31);
  GP.BREAK();
  GP.LABEL(data.pass[0] ? String(F("Пароль Wi-Fi задан")) : String(F("Пароль Wi-Fi не задан")));
  GP.BREAK();
  GP.PASS("apPass", "Пароль точки, пусто = не менять, от 8 символов", "", "", 31);
  GP.BREAK();
  GP.LABEL(apIsOpen() ? String(F("Точка доступа открытая")) : String(F("Точка доступа с паролем")));
  GP.BLOCK_END();

  GP.LABEL("MQTT");
  GP.BLOCK_BEGIN();
  GP.TEXT("group", "Группа: латиница, цифры, дефис", data.group, "", 20);
  GP.BREAK();
  GP.TEXT("host", "Хост брокера, пусто = только эта лампа", data.host, "", 31);
  GP.BREAK();
  GP.NUMBER("port", "Порт", data.port);
  GP.BREAK();
  GP.TEXT("mqttUser", "Логин брокера", data.mqttUser, "", 31);
  GP.BREAK();
  GP.PASS("mqttPass", "Пароль брокера, пусто = не менять", "", "", 31);
  GP.BREAK();
  GP.LABEL(data.mqttPass[0] ? String(F("Пароль брокера задан")) : String(F("Пароль брокера не задан")));
  GP.BREAK();
  GP.TEXT("owner", "Имя на кольце у других", data.owner, "", 31);
  GP.BLOCK_END();

  GP.SUBMIT("Сохранить");
  GP.BREAK();
  GP.BUTTON("rec", "Переподключить Wi-Fi");
  GP.BUTTON("ap", "Включить точку доступа");
  GP.BUTTON("apclr", "Сбросить пароль точки");
  GP.FORM_END();

  GP.LABEL("ПРОШИВКА");
  GP.BLOCK_BEGIN();
  GP.LABEL(String(F("Браузер: http://")) + ipText + F("/ota_update  логин lamp"));
  GP.BREAK();
  GP.LABEL(F("Пароль OTA задаётся при сборке (LAMP_OTA_PASS), в страницу не выводится."));
  GP.BREAK();
  GP.LABEL(F("Кнопка: 1 подмигнуть, 2 настроение, 3 питание, 4 среднее, 5 автономный, удержание — яркость."));
  GP.BLOCK_END();

  GP.BUILD_END();
}
