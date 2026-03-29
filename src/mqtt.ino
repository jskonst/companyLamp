/*
  Протокол:
  GWL:0,ir  // пакет heartbeat, состояние ИК
  GWL:1,power,color,wink  // пакет данных
  GWL:2     // запрос настроек
*/

// опрашиваем mqtt
void mqttTick() {
  // if (WiFi.status() != WL_CONNECTED) return;  // wifi не подключен
  // if (!mqtt.connected()) connectMQTT();
  // else {
  //   if (!startFlag) {
  //     startFlag = 1;
  //     char str[] = MQTT_HEADER "2";  // +2
  //     mqtt.publish(data.remote, str);
  //   }
  // }
  // mqtt.loop();
}

// void connectMQTT() {
//   // задаём случайный ID
//   String id("WebLamp-");
//   id += String(random(0xffffff), HEX);
//   //DEBUGLN(id);
//   // подписываемся на своё имя
//   if (mqtt.connect(id.c_str())) mqtt.subscribe(data.local);
//   delay(1000);
// }

// тут нам прилетел пакет от удалённой лампы
void callback(char* topic, byte* payload, uint16_t len) {
  // MQTT callback stub - functionality currently disabled
  (void)topic;
  (void)payload;
  (void)len;
}

// // отправляем пакет
// void sendPacket() {
//   // GWL:1,power,color,wink
//   String s;
//   s.reserve(10);
//   s += MQTT_HEADER "1,";  // +1,
//   s += data.power;
//   s += ',';
//   s += data.color;
//   s += ',';
//   s += winkFlag;
//   winkFlag = 0;
//   // отправляем
//   mqtt.publish(data.remote, s.c_str());
// }

// void heartbeat() {
//   if (hbTmr.period()) {
//     // GWL:0,pir
//     char str[hLen + 4] = MQTT_HEADER "0,";  // +0,
//     str[hLen + 2] = pirFlag + '0';
//     pirFlag = 0;
//     mqtt.publish(data.remote, str);
//   }
// }
