#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

// Присутствие коллеги. Пока status не приходил — запасной признак это heartbeat < 18 с.
enum class Presence : uint8_t { Unknown, Online, Away, Offline };

struct RemoteLamp {
    char     name[20];
    uint32_t lastSeen;
    uint8_t  mood;
    bool     dnd;
    bool     pir;
    Presence presence;
    bool online() const {
        if (presence == Presence::Online) return true;
        if (presence == Presence::Away || presence == Presence::Offline) return false;
        return lastSeen != 0 && (uint32_t)(millis() - lastSeen) < 18000UL;
    }
};

extern PubSubClient mqtt;

// 6 hex-символов ESP.getChipId(). Им же назван client id `lamp-<id>` и сегмент топика.
const char* lampId();

uint8_t            mqttRemoteCount();
const RemoteLamp&  mqttRemote(uint8_t i);
