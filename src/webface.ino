bool checkPortal() {
  if (portal.click()) {
    if (portal.click("br")) data.bright = portal.getInt("br");
    if (portal.click("sw")) {
      data.power = portal.getCheck("sw");
      sendPacket();
    }
    if (portal.click("col")) {
      data.color = portal.getInt("col");
      sendPacket();
    }
    if (portal.click("wink")) {
      winkFlag = 1;
      sendPacket();
    }
    memory.update();
  }

  if (portal.update()) {
    if (portal.update("ledW")) portal.answer(WiFi.status() == WL_CONNECTED);
    if (portal.update("ledL")) portal.answer(mqtt.connected());
    if (portal.update("ledR")) portal.answer(!onlineTmr.elapsed());
    if (portal.update("ledP")) portal.answer((!pirTmr.elapsed() && !onlineTmr.elapsed()));
    if (portal.update("br")) portal.answer(data.bright);
    if (portal.update("sw")) portal.answer(data.power);
    if (portal.update("col")) portal.answer(data.color);
  }

  if (portal.form()) {
    if (portal.form("/save")) {
      portal.copyStr("ssid", data.ssid);
      String p = portal.getString("pass");
      if (p.length()) p.toCharArray(data.pass, sizeof(data.pass));  // пусто = не менять
      portal.copyStr("local", data.local);
      portal.copyStr("remote", data.remote);
      portal.copyStr("host", data.host);
      data.port = portal.getInt("port");
      memory.updateNow();
      // без перезагрузки: переподключаем Wi-Fi и MQTT
      mqtt.disconnect();
      mqtt.setServer(data.host, data.port);
      wifiBegin();
      return 1;
    }
  }
  return 0;
}

void build() {
  GP.BUILD_BEGIN();
  GP.THEME(GP_DARK);
  GP.UPDATE("ledW,ledL,ledR,ledP,sw,br,col", 2000);

  GP.TITLE("companyLamp");
  GP.BLOCK_BEGIN();
  GP.LABEL("Wi-Fi:");  GP.LED_GREEN("ledW", WiFi.status() == WL_CONNECTED);
  GP.LABEL("MQTT:");   GP.LED_GREEN("ledL", mqtt.connected());
  GP.BREAK();
  GP.LABEL("Remote:"); GP.LED_GREEN("ledR", !onlineTmr.elapsed());
  GP.LABEL("Remote PIR:"); GP.LED_RED("ledP", (!pirTmr.elapsed() && !onlineTmr.elapsed()));
  GP.BLOCK_END();

  GP.BLOCK_BEGIN();
  GP.LABEL("Power:"); GP.SWITCH("sw", data.power);
  GP.BREAK();
  GP.LABEL("Bright:"); GP.SLIDER("br", data.bright, 0, 255);
  GP.BREAK();
  GP.LABEL("Color:"); GP.SLIDER("col", data.color, 0, 255);
  GP.BREAK();
  GP.BUTTON("wink", "Wink");
  GP.BLOCK_END();

  GP.FORM_BEGIN("/save");
  GP.BLOCK_BEGIN();
  GP.LABEL("WIFI");
  GP.TEXT("ssid", "SSID", data.ssid);
  GP.BREAK();
  GP.PASS("pass", "Password (empty = keep)", "");
  GP.BLOCK_END();
  GP.BLOCK_BEGIN();
  GP.LABEL("MQTT");
  GP.TEXT("local", "Local Name", data.local);
  GP.BREAK();
  GP.TEXT("remote", "Remote Name", data.remote);
  GP.BREAK();
  GP.TEXT("host", "Broker host", data.host);
  GP.BREAK();
  GP.NUMBER("port", "Port", data.port);
  GP.BLOCK_END();
  GP.SUBMIT("Save");
  GP.FORM_END();
  GP.BUILD_END();
}
