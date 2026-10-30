bool checkPortal() {
  // клики
  if (portal.click()) {
    if (portal.click("rec")) wifiReconnect();
    if (portal.click("ap")) apStart();
    if (portal.click("br")) data.bright = portal.getInt("br");
    if (portal.click("sw")) {
      data.power = portal.getCheck("sw");
      sendPacket();
    }
    if (portal.click("col")) {
      data.color = portal.getInt("col");
      sendPacket();
    }
    if (portal.click()) memory.update();
  }

  // обновления
  if (portal.update()) {
    if (portal.update("ledL")) portal.answer(mqtt.connected());
    if (portal.update("ledW")) portal.answer(staConnected());
    if (portal.update("ledR")) portal.answer(!onlineTmr.elapsed());
    if (portal.update("ledP")) portal.answer((!pirTmr.elapsed() && !onlineTmr.elapsed()));
    if (portal.update("br")) portal.answer(data.bright);
    if (portal.update("sw")) portal.answer(data.power);
    if (portal.update("col")) portal.answer(data.color);
  }

  // формы
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
      mqtt.disconnect();
      mqtt.setServer(data.host, data.port);
      wifiReconnect();  // переподключаемся без перезагрузки, точка/портал остаются
      return 1;
    }
  }
  return 0;
}
