static void notePortal(const char* text) {
  strncpy(portalNote, text, sizeof(portalNote) - 1);
  portalNote[sizeof(portalNote) - 1] = '\0';
}

static void copyBounded(const String& src, char* dst, size_t len) {
  src.toCharArray(dst, len);
}

// Имя попадёт в JSON и в HTML списка. Кавычки, угловые скобки и управляющие — мимо.
static void sanitizeOwner(char* text) {
  char* write = text;
  for (char* read = text; *read; read++) {
    unsigned char c = (unsigned char)*read;
    bool ok = c >= 0x80 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' || c == '.';
    if (ok) *write++ = (char)c;
  }
  *write = '\0';
}

static bool saveSecret(const char* field, char* dst, size_t len, uint8_t minLen) {
  String value = portal.getString(field);
  if (!value.length()) return true;  // пусто = не менять, в HTML пароля нет
  if (minLen && value.length() < minLen) return false;
  if (value.length() >= len) return false;
  copyBounded(value, dst, len);
  return true;
}

bool checkPortal() {
  if (portal.click()) {
    if (portal.click("rec")) wifiReconnect();
    if (portal.click("ap")) apStart();
    if (portal.click("apclr")) {
      data.apPass[0] = '\0';
      memory.updateNow();
      if (apActive) apRestart();
      notePortal("Пароль точки сброшен");
    }
    if (portal.click("br")) {
      data.bright = constrain(portal.getInt("br"), 0, 255);
      memory.update();
    }
    if (portal.click("sw")) {
      data.power = portal.getCheck("sw");
      memory.update();
    }
    if (portal.click("mood")) {
      data.mood = clampMood(portal.getInt("mood"));
      memory.update();
      publishState();
    }
    if (portal.click("avg")) {
      data.average = portal.getCheck("avg");
      memory.update();
      peersDirty = true;
    }
    if (portal.click("aut")) setAutonomous(portal.getCheck("aut"));
  }

  if (portal.update()) {
    if (peersDirty) rebuildPeerText();
    if (portal.update("ledW")) portal.answer(staConnected());
    if (portal.update("ledM")) portal.answer(mqtt.connected() && !data.autonomous);
    if (portal.update("onl")) portal.answer(onlineCount);
    if (portal.update("peers")) portal.answer(peerText);
    if (portal.update("sw")) portal.answer(data.power);
    if (portal.update("br")) portal.answer(data.bright);
    if (portal.update("mood")) portal.answer(data.mood);
    if (portal.update("avg")) portal.answer(data.average);
    if (portal.update("aut")) portal.answer(data.autonomous);
  }

  if (portal.form()) {
    if (portal.form("/save")) {
      portalNote[0] = '\0';
      copyBounded(portal.getString("ssid"), data.ssid, sizeof(data.ssid));

      if (!saveSecret("pass", data.pass, sizeof(data.pass), 0)) {
        notePortal("Пароль Wi-Fi не сохранён");
      }

      String group = portal.getString("group");
      group.trim();
      if (!group.length()) {
        data.group[0] = '\0';
      } else if (group.length() <= 20 && validGroupName(group.c_str())) {
        copyBounded(group, data.group, sizeof(data.group));
      } else {
        notePortal("Группа: латиница, цифры и дефис, до 20 символов");
      }

      String host = portal.getString("host");
      host.trim();
      if (host.length() >= sizeof(data.host) || host.indexOf(' ') >= 0 || host.indexOf('/') >= 0) {
        notePortal("Некорректный хост брокера");
      } else {
        copyBounded(host, data.host, sizeof(data.host));
      }

      int port = portal.getInt("port");
      data.port = (port > 0 && port <= 65535) ? (uint16_t)port : 1883;

      copyBounded(portal.getString("mqttUser"), data.mqttUser, sizeof(data.mqttUser));
      if (!saveSecret("mqttPass", data.mqttPass, sizeof(data.mqttPass), 0)) {
        notePortal("Пароль брокера слишком длинный");
      }

      copyBounded(portal.getString("owner"), data.owner, sizeof(data.owner));
      sanitizeOwner(data.owner);

      bool apPassChanged = portal.getString("apPass").length() > 0;
      if (!saveSecret("apPass", data.apPass, sizeof(data.apPass), 8)) {
        notePortal("Пароль точки: минимум 8 символов, или оставьте поле пустым");
        apPassChanged = false;
      }

      memory.updateNow();
      mqttRestart();
      // Переподнимать точку на каждом Save нельзя: оборвётся телефон, с которого сохраняли.
      if (apPassChanged && apActive) apRestart();
      wifiReconnect();
      if (!portalNote[0]) notePortal("Сохранено");
      return true;
    }
  }
  return false;
}
