#include "mqtt.h"

static RemoteLamp remotes[MAX_REMOTE_LAMPS];
static uint8_t    remoteCount = 0;

static RemoteLamp* findOrCreateRemote(const char* name) {
    for (uint8_t i = 0; i < remoteCount; i++)
        if (strncmp(remotes[i].name, name, sizeof(remotes[i].name)) == 0)
            return &remotes[i];
    if (remoteCount >= MAX_REMOTE_LAMPS) return nullptr;
    RemoteLamp& r = remotes[remoteCount++];
    strncpy(r.name, name, sizeof(r.name) - 1);
    r.name[sizeof(r.name) - 1] = '\0';
    r.lastSeen = 0; r.mood = 0; r.dnd = false; r.pir = false;
    return &r;
}

String mqttStatusString() {
    if (cfg::offline()) return "Офлайн-режим";
    if (!mqtt.connected()) return "MQTT: нет подключения";
    String s = "Группа: " + cfg::group();
    if (remoteCount == 0) {
        s += "\n(нет ламп в группе)";
    } else {
        for (uint8_t i = 0; i < remoteCount; i++) {
            s += '\n';
            s += remotes[i].online() ? "\u25cf " : "\u25cb ";
            s += remotes[i].name;
        }
    }
    return s;
}

// ─── callback ────────────────────────────────────────────────────────────────

static char meetingSender[20] = {};
static bool meetingPending    = false;

// Вызывается из mode_meeting.ino (план 09): возвращает имя инициатора и сбрасывает флаг.
bool takeMeetingSignal(String& sender) {
    if (!meetingPending) return false;
    meetingPending = false;
    sender = meetingSender;
    return true;
}

void callback(char* topic, byte* payload, uint16_t len) {
    if (len == 0 || len >= 256) return;
    String t(topic);
    char buf[256];
    memcpy(buf, payload, len);
    buf[len] = '\0';
    String val(buf);

    // parse <group>/<lamp>/<subtopic>
    int p1 = t.indexOf('/');
    int p2 = t.lastIndexOf('/');
    if (p1 < 0 || p2 <= p1) return;

    String sender   = t.substring(p1 + 1, p2);
    String subtopic = t.substring(p2 + 1);

    if (sender == cfg::local()) return;

    RemoteLamp* remote = findOrCreateRemote(sender.c_str());
    if (!remote) return;

    if (subtopic == "heartbeat") {
        remote->lastSeen = millis();
        remote->pir = (val == "1");
    } else if (subtopic == "mood") {
        remote->mood = (uint8_t)val.toInt();
        // mode_mood читает remotes[].mood на каждом тике — дополнительный callback не нужен
    } else if (subtopic == "dnd") {
        remote->dnd = (val == "1");
    } else if (subtopic == "pomodoro") {
        remote->dnd = val.startsWith("work");
    } else if (subtopic == "meeting") {
        strncpy(meetingSender, sender.c_str(), sizeof(meetingSender) - 1);
        meetingSender[sizeof(meetingSender) - 1] = '\0';
        meetingPending = true;
    }
}

// ─── heartbeat ───────────────────────────────────────────────────────────────

static void heartbeatTick() {
    static Timer hbTmr(8000);
    if (!hbTmr.period()) return;

    String base = cfg::group() + "/" + cfg::local() + "/";
#if USE_PIR
    String payload = pirActive() ? "1" : "0";
#else
    String payload = "0";
#endif
    mqtt.publish((base + "heartbeat").c_str(), payload.c_str(), true);
    DEBUGLN(F("[MQTT] heartbeat sent"));
}

// ─── connect ─────────────────────────────────────────────────────────────────

static void connectMQTT() {
    static uint32_t lastAttempt = 0;
    if ((uint32_t)(millis() - lastAttempt) < 5000) return;
    lastAttempt = millis();

    static char hostBuf[64] = {};
    strncpy(hostBuf, cfg::host().c_str(), sizeof(hostBuf) - 1);
    mqtt.setServer(hostBuf, cfg::port());

    String id = "WebLamp-" + String(random(0xffffff), HEX);
    String user = cfg::mqttUser();
    String pass = cfg::mqttPass();

    bool ok = user.length()
        ? mqtt.connect(id.c_str(), user.c_str(), pass.c_str())
        : mqtt.connect(id.c_str());

    if (!ok) {
        DEBUG(F("[MQTT] connect failed, rc="));
        DEBUGLN(mqtt.state());
        return;
    }
    DEBUGLN(F("[MQTT] connected"));

    String topic = cfg::group() + "/+/#";
    mqtt.subscribe(topic.c_str());
    DEBUG(F("[MQTT] subscribed: "));
    DEBUGLN(topic);
}

// ─── remote access ───────────────────────────────────────────────────────────

uint8_t mqttRemoteCount() { return remoteCount; }
const RemoteLamp& mqttRemote(uint8_t i) { return remotes[i]; }

// ─── public API ──────────────────────────────────────────────────────────────

void mqttBegin() {
    mqtt.setCallback(callback);
}

void mqttTick() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (cfg::offline()) return;

    if (!mqtt.connected()) {
        connectMQTT();
        return;
    }
    heartbeatTick();
    mqtt.loop();
}

void publishMood(uint8_t hue) {
    String base = cfg::group() + "/" + cfg::local() + "/";
    mqtt.publish((base + "mood").c_str(), String(hue).c_str(), true);
}

void publishDND(bool active) {
    String base = cfg::group() + "/" + cfg::local() + "/";
    mqtt.publish((base + "dnd").c_str(), active ? "1" : "0", true);
}

void publishMeeting() {
    String base = cfg::group() + "/" + cfg::local() + "/";
    mqtt.publish((base + "meeting").c_str(), cfg::local().c_str());
}

void publishPomodoro(const char* state) {
    String base = cfg::group() + "/" + cfg::local() + "/";
    mqtt.publish((base + "pomodoro").c_str(), state, true);
}
