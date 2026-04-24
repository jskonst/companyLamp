#include "led_driver.h"
#include "sets.h"
#include "mqtt.h"

static uint8_t moodDraft  = 120;
static bool    moodEditing = false;

static uint8_t computeTeamAvg() {
    uint16_t sum   = cfg::mood();
    uint8_t  count = 1;
    for (uint8_t i = 0; i < mqttRemoteCount(); i++) {
        const RemoteLamp& r = mqttRemote(i);
        if (r.online()) { sum += r.mood; count++; }
    }
    return (uint8_t)(sum / count);
}

// ─── hold: прокрутка цвета ────────────────────────────────────────────────────

void moodStep() {
    if (!moodEditing) {
        moodEditing = true;
        moodDraft   = cfg::mood();
    }
    moodDraft = (moodDraft >= 2) ? moodDraft - 2 : 120;
    db[kk::mood] = moodDraft;

    fill_solid(leds, LED_AMOUNT, CHSV(moodDraft, 255, 255));
    applyGlobalBrightness();
    showLedsSafe();
}

void moodReleaseStep() {
    if (!moodEditing) return;
    moodEditing = false;
    db.update();
    if (mqtt.connected() && !cfg::offline()) publishMood(cfg::mood());
    DEBUGLN(F("[MOOD] saved & sent"));
}

// ─── 2 клика: отправить вручную ───────────────────────────────────────────────

void moodClick(uint8_t clicks) {
    if (clicks != 2) return;
    if (mqtt.connected() && !cfg::offline()) publishMood(cfg::mood());
    DEBUGLN(F("[MOOD] sent"));
}

// ─── рендер ───────────────────────────────────────────────────────────────────

void renderMood() {
    if (moodEditing) return;  // step уже показывает цвет

    uint8_t hue = computeTeamAvg();
    fill_solid(leds, LED_AMOUNT, CHSV(hue, 200, 255));
    applyGlobalBrightness();
    showLedsSafe();
}
