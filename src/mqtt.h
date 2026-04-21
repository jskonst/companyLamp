#pragma once
#include <Arduino.h>

struct RemoteLamp {
    char     name[20];
    uint32_t lastSeen;
    uint8_t  mood;
    bool     dnd;
    bool     pir;
    bool online() const { return (uint32_t)(millis() - lastSeen) < 18000UL; }
};
