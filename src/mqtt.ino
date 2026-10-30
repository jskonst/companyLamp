/*
  Протокол:
  GWL:0,ir  // пакет heartbeat, состояние ИК
  GWL:1,power,color,wink  // пакет данных
  GWL:2     // запрос настроек
*/

// опрашиваем mqtt
void mqttTick() {
  if (WiFi.status() != WL_CONNECTED) return;  // wifi не подключен
  if (!mqtt.connected()) {
    startFlag = 0;
    static uint32_t lastTry = 0;
    // переподключение не чаще раза в 7 с, connect с таймаутом 2 с, без delay
    if (!lastTry || millis() - lastTry > 7000) {
      lastTry = millis();
      connectMQTT();
    }
    return;
  }
  if (!startFlag) {
    startFlag = 1;
    char str[] = MQTT_HEADER "2";  // +2 запрос состояния у удалённой
    mqtt.publish(data.remote, str);
    sendPacket();                  // и публикуем своё после переподключения
  }
  mqtt.loop();
}

void connectMQTT() {
  // задаём случайный ID
  String id("WebLamp-");
  id += String(random(0xffffff), HEX);
  //DEBUGLN(id);
  // подписываемся на своё имя
  if (mqtt.connect(id.c_str())) mqtt.subscribe(data.local);
}

// тут нам прилетел пакет от удалённой лампы
void callback(char* topic, byte* payload, uint16_t len) {
  char buf[48];                // копия с нулём в конце (payload[len] — выход за буфер)
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, payload, len);
  buf[len] = '\0';
  char* str = buf;
  DEBUGLN(str);
  // не наш пакет, выходим
  if (strncmp(str, MQTT_HEADER, hLen)) return;

  str += hLen;   // смещаемся для удобства чтения

  switch (getFromIndex(str, 0)) {
    case 0:   // heartbeat
      if (!IGNORE_PIR && getFromIndex(str, 1)) pirTmr.restart();
      break;

    case 1:   // управление
      data.power = getFromIndex(str, 1);
      data.color = getFromIndex(str, 2);
      if (getFromIndex(str, 3)) wink();  // неблокирующе, см. animation()
      break;

    case 2:   // запрос
      sendPacket();
      break;
  }

  onlineTmr.restart();  // перезапуск таймера онлайна
}

// отправляем пакет
void sendPacket() {
  // GWL:1,power,color,wink
  String s;
  s.reserve(10);
  s += MQTT_HEADER "1,";  // +1,
  s += data.power;
  s += ',';
  s += data.color;
  s += ',';
  s += winkFlag;
  winkFlag = 0;
  if (!mqtt.connected()) return;  // офлайн — только локально
  // отправляем
  mqtt.publish(data.remote, s.c_str());
}

void heartbeat() {
  if (hbTmr.period()) {
    // GWL:0,pir
    char str[hLen + 4] = MQTT_HEADER "0,";  // +0,
    str[hLen + 2] = pirFlag + '0';
    pirFlag = 0;
    if (mqtt.connected()) mqtt.publish(data.remote, str);
  }
}
