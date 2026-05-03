#include "led_driver.h"
#include "sets.h"
#include "mqtt.h"

static uint8_t computeTeamAvg() {
    uint16_t sum   = cfg::mood();
    uint8_t  count = 1;
    for (uint8_t i = 0; i < mqttRemoteCount(); i++) {
        const RemoteLamp& r = mqttRemote(i);
        if (r.online()) { sum += r.mood; count++; }
    }
    return (uint8_t)(sum / count);
}

static uint8_t moodToHue(uint8_t mood) {
    return (uint8_t)map(constrain(mood, 0, 120), 0, 120, 0, 85);
}

// ─── 2 клика: отправить вручную ───────────────────────────────────────────────

void moodClick(uint8_t clicks) {
    if (clicks != 2) return;
    if (mqtt.connected() && !cfg::offline()) publishMood(cfg::mood());
    DEBUGLN(F("[MOOD] sent"));
}

// ─── рендер ───────────────────────────────────────────────────────────────────

void renderMood() {
    if (!cfg::power()) {
        fill_solid(leds, LED_AMOUNT, CRGB::Black);
        FastLED.setBrightness(0);
        showLedsSafe();
        return;
    }

    uint8_t hue = moodToHue(computeTeamAvg());
    fill_solid(leds, LED_AMOUNT, CHSV(hue, 200, 255));
    applyGlobalBrightness();
    showLedsSafe();
}
