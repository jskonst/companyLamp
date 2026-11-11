// Неблокирующий Wi-Fi: STA к роутеру и запасная точка. Кнопка и лента не ждут IP.

#define WIFI_CONNECT_TIMEOUT 15000ul
#define WIFI_RETRY_PERIOD 30000ul
#define AP_OFF_DELAY 60000ul

#ifndef LAMP_AP_PASS
#define LAMP_AP_PASS ""
#endif

uint32_t staLostMs = 0;
uint32_t staOkMs = 0;
uint32_t staRetryMs = 0;
bool staWasUp = false;

bool hasCreds() { return data.ssid[0] != '\0'; }

// Пароль из портала важнее флага сборки. Короткий пароль WPA не принимаем.
const char* currentApPass() {
  if (strlen(data.apPass) >= 8) return data.apPass;
  if (strlen(LAMP_AP_PASS) >= 8) return LAMP_AP_PASS;
  return nullptr;
}

bool apIsOpen() { return currentApPass() == nullptr; }

String apName() {
  char buf[24];
  snprintf(buf, sizeof(buf), "companyLamp-%s", lampId[0] ? lampId : "lamp");
  return String(buf);
}

void apStart() {
  if (apActive) return;
  WiFi.mode(hasCreds() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  const char* pass = currentApPass();
  // nullptr у String-перегрузки softAP превращается в пустую строку и сеть остаётся открытой.
  if (pass) WiFi.softAP(apName().c_str(), pass);
  else WiFi.softAP(apName().c_str());
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  apActive = true;
  DEBUG(F("AP up: "));
  DEBUG(apName());
  DEBUG(' ');
  DEBUGLN(WiFi.softAPIP());
}

void apStop() {
  if (!apActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  DEBUGLN(F("AP down"));
}

void apRestart() {
  apStop();
  apStart();
}

void staBegin() {
  if (!hasCreds()) return;
  staRetryMs = millis();
  WiFi.begin(data.ssid, data.pass);
  DEBUG(F("STA connecting to "));
  DEBUGLN(data.ssid);
}

void wifiBegin(bool forceAP) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  if (deviceHost[0]) WiFi.hostname(deviceHost);
  WiFi.mode(WIFI_STA);
  staLostMs = millis();
  if (forceAP || !hasCreds()) apStart();
  staBegin();
}

void wifiReconnect() {
  WiFi.disconnect();
  staWasUp = false;
  staLostMs = millis();
  if (!hasCreds()) {
    apStart();
    return;
  }
  if (apActive) WiFi.mode(WIFI_AP_STA);
  staBegin();
}

bool staConnected() { return WiFi.status() == WL_CONNECTED; }

void wifiTick() {
  if (apActive) dns.processNextRequest();
  uint32_t now = millis();
  bool up = staConnected();

  if (up && !staWasUp) {
    staWasUp = true;
    staOkMs = now;
    mqttOnWifiUp();
    DEBUG(F("STA IP: "));
    DEBUGLN(WiFi.localIP());
    IPAddress ip = WiFi.localIP();
    if (data.ip[0] != ip[0] || data.ip[1] != ip[1] || data.ip[2] != ip[2] || data.ip[3] != ip[3]) {
      for (int i = 0; i < 4; i++) data.ip[i] = ip[i];
      memory.update();
    }
  } else if (!up && staWasUp) {
    staWasUp = false;
    staLostMs = now;
    DEBUGLN(F("STA lost"));
  }

  if (up) {
    if (apActive && now - staOkMs > AP_OFF_DELAY && WiFi.softAPgetStationNum() == 0) apStop();
    return;
  }

  if (!apActive && now - staLostMs > WIFI_CONNECT_TIMEOUT) apStart();

  uint32_t period = (apActive && WiFi.softAPgetStationNum() > 0) ? WIFI_RETRY_PERIOD * 4 : WIFI_RETRY_PERIOD;
  if (hasCreds() && now - staRetryMs > period) staBegin();
}
