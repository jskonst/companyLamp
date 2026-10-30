// Сеть: режим WIFI_AP_STA. STA подключается в фоне; если SSID пуст или за 15 с
// нет подключения — поднимаем AP "companyLamp-XXXX" (открытая, 192.168.4.1) с
// captive-порталом. Лампа и кнопка работают всегда, ничего не блокирует и не ресетит.
static bool apOn = false;
static uint32_t staStartMs = 0;
static DNSServer captiveDns;  // GyverPortal включает DNS только если AP поднята до start(), а у нас AP динамическая

void apStart() {
  if (apOn) return;
  char name[24];
  snprintf(name, sizeof(name), "companyLamp-%04X", (unsigned)(ESP.getChipId() & 0xFFFF));
  WiFi.mode(strlen(data.ssid) ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(name);
  captiveDns.start(53, "*", WiFi.softAPIP());
  apOn = true;
  Serial.print(F("AP "));
  Serial.print(name);
  Serial.print(F(" IP "));
  Serial.println(WiFi.softAPIP());
}

void apStop() {
  if (!apOn) return;
  captiveDns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apOn = false;
  Serial.println(F("AP off"));
}

void wifiBegin() {
  if (strlen(data.ssid)) {
    WiFi.mode(apOn ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(data.ssid, data.pass);
    staStartMs = millis();
    Serial.print(F("Connecting to "));
    Serial.println(data.ssid);
  } else {
    apStart();
  }
}

void wifiTick() {
  if (apOn) captiveDns.processNextRequest();
  static bool wasConnected = false;
  bool conn = WiFi.status() == WL_CONNECTED;
  if (conn && !wasConnected) {
    Serial.print(F("Connected! IP: "));
    Serial.println(WiFi.localIP());
    IPAddress ip = WiFi.localIP();
    if (data.ip[0] != ip[0] || data.ip[1] != ip[1] || data.ip[2] != ip[2] || data.ip[3] != ip[3]) {
      for (int i = 0; i < 4; i++) data.ip[i] = ip[i];
      memory.update();
    }
  }
  if (!conn && wasConnected) staStartMs = millis();  // роутер пропал — снова отсчёт 15 с
  wasConnected = conn;

  // нет роутера дольше 15 с -> AP (STA продолжает переподключаться в фоне)
  if (!conn && !apOn && millis() - staStartMs > 15000) apStart();
  // роутер вернулся и к AP никто не подключён -> гасим AP
  if (conn && apOn && WiFi.softAPgetStationNum() == 0) apStop();
}

bool apActive() { return apOn; }
