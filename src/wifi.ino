// Неблокирующее управление Wi-Fi: STA к домашней сети + резервная точка доступа.
// Цель: лампа управляется кнопкой и веб-интерфейсом всегда, даже без роутера.

#define WIFI_CONNECT_TIMEOUT 15000ul  // сколько ждём IP до подъёма точки
#define WIFI_RETRY_PERIOD    30000ul  // период повторной попытки STA при поднятой точке
#define AP_OFF_DELAY         60000ul  // сколько STA должен быть стабилен, чтобы погасить точку
#ifndef LAMP_AP_PASS
#define LAMP_AP_PASS ""               // пусто = открытая сеть; иначе >= 8 символов
#endif

uint32_t staLostMs = 0;     // когда пропал/не появился IP
uint32_t staOkMs = 0;       // когда появился IP
uint32_t staRetryMs = 0;    // последняя попытка WiFi.begin
bool staWasUp = false;

bool hasCreds() { return data.ssid[0] != '\0'; }

String apName() {
  char buf[24];
  snprintf(buf, sizeof(buf), "companyLamp-%06X", ESP.getChipId());
  return String(buf);
}

void apStart() {
  if (apActive) return;
  WiFi.mode(hasCreds() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  const char* pass = LAMP_AP_PASS;
  WiFi.softAP(apName(), strlen(pass) >= 8 ? pass : nullptr);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());  // captive portal: любой домен -> лампа
  apActive = true;
  DEBUG(F("AP up: ")); DEBUG(apName()); DEBUG(' '); DEBUGLN(WiFi.softAPIP());
}

void apStop() {
  if (!apActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  DEBUGLN(F("AP down"));
}

void staBegin() {
  if (!hasCreds()) return;
  staRetryMs = millis();
  WiFi.begin(data.ssid, data.pass);
  DEBUG(F("STA connecting to ")); DEBUGLN(data.ssid);
}

void wifiBegin(bool forceAP) {
  WiFi.persistent(false);       // креды храним сами в EEPROM (EEManager)
  WiFi.setAutoReconnect(false); // реконнект делаем сами, чтобы не сбивать канал точки
  WiFi.mode(WIFI_STA);
  staLostMs = millis();
  if (forceAP || !hasCreds()) apStart();
  staBegin();
}

// вызвать после сохранения новых кредов из портала — без перезагрузки
void wifiReconnect() {
  WiFi.disconnect();
  staWasUp = false;
  staLostMs = millis();
  if (!hasCreds()) { apStart(); return; }
  if (apActive) WiFi.mode(WIFI_AP_STA);
  staBegin();
}

bool staConnected() { return WiFi.status() == WL_CONNECTED; }

void wifiTick() {
  if (apActive) dns.processNextRequest();
  uint32_t now = millis();
  bool up = staConnected();

  if (up && !staWasUp) {          // появился IP
    staWasUp = true;
    staOkMs = now;
    startFlag = 0;                // запросить состояние у удалённой лампы
    DEBUG(F("STA IP: ")); DEBUGLN(WiFi.localIP());
    IPAddress ip = WiFi.localIP();
    if (data.ip[0] != ip[0] || data.ip[1] != ip[1] || data.ip[2] != ip[2] || data.ip[3] != ip[3]) {
      for (int i = 0; i < 4; i++) data.ip[i] = ip[i];
      memory.update();
    }
  } else if (!up && staWasUp) {   // потеряли связь
    staWasUp = false;
    staLostMs = now;
    DEBUGLN(F("STA lost"));
  }

  if (up) {
    // STA стабилен, клиентов на точке нет — гасим точку
    if (apActive && now - staOkMs > AP_OFF_DELAY && WiFi.softAPgetStationNum() == 0) apStop();
    return;
  }

  // нет IP
  if (!apActive && now - staLostMs > WIFI_CONNECT_TIMEOUT) apStart();

  // периодически пробуем снова; пока к точке кто-то подключён — реже,
  // т.к. ESP8266 при поиске роутера может сменить канал и уронить клиента
  uint32_t period = (apActive && WiFi.softAPgetStationNum() > 0) ? WIFI_RETRY_PERIOD * 4 : WIFI_RETRY_PERIOD;
  if (hasCreds() && now - staRetryMs > period) staBegin();
}
