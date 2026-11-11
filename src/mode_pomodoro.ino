#include "led_driver.h"
#include "sets.h"
#include "mqtt.h"

enum class PomoState {
  IDLE,
  WORK,
  BREAK_SHORT,
  BREAK_LONG,
  DONE,       // интервал завершён, мигаем
  CONFIRMED,  // мигание остановлено, ждём запуска следующего
  PAUSED,
};

static PomoState pomoState     = PomoState::IDLE;
static PomoState pomoPrevState = PomoState::WORK;  // состояние до паузы / до DONE
static uint32_t  pomoStart     = 0;
static uint32_t  pomoDuration  = 0;
static uint8_t   pomoRound     = 0;
static uint32_t  pomoPauseAt   = 0;
static uint32_t  pomoDoneAt    = 0;

// ─── helpers ─────────────────────────────────────────────────────────────────

static void startPomoWork() {
  pomoRound++;
  pomoStart    = millis();
  pomoDuration = (uint32_t)cfg::pomo_work() * 60000UL;
  pomoState    = PomoState::WORK;

  if (mqtt.connected() && !cfg::offline()) {
    String payload = "work:" + String(cfg::pomo_color());
    publishPomodoro(payload.c_str());
    publishDND(true);
  }
}

static void startNextInterval() {
  uint8_t count = cfg::pomo_count();
  bool isLongBreak = count > 0 && (pomoRound % count == 0);

  bool fromWork = (pomoState == PomoState::WORK) ||
                  ((pomoState == PomoState::PAUSED ||
                    pomoState == PomoState::DONE   ||
                    pomoState == PomoState::CONFIRMED) &&
                   pomoPrevState == PomoState::WORK);

  if (fromWork) {
    pomoStart = millis();
    if (isLongBreak) {
      pomoDuration = (uint32_t)cfg::pomo_long() * 60000UL;
      pomoState    = PomoState::BREAK_LONG;
    } else {
      pomoDuration = (uint32_t)cfg::pomo_short() * 60000UL;
      pomoState    = PomoState::BREAK_SHORT;
    }
    if (mqtt.connected() && !cfg::offline()) {
      publishPomodoro("break");
      publishDND(false);
    }
  } else {
    startPomoWork();
  }
}

// ─── render ──────────────────────────────────────────────────────────────────

static CRGB bgColor() {
  uint8_t h = cfg::pomo_bg();
  return h == 0 ? CRGB(10, 10, 10) : CHSV(h, 200, 15);
}

static void renderPomoIdle() {
  fill_solid(leds, LED_AMOUNT, bgColor());
  applyGlobalBrightness();
  showLedsSafe();
}

static void renderPomoProgress() {
  uint32_t elapsed = millis() - pomoStart;
  uint8_t  filled  = (uint8_t)((uint32_t)LED_AMOUNT * elapsed / pomoDuration);
  if (filled > LED_AMOUNT) filled = LED_AMOUNT;

  bool isWork    = (pomoState == PomoState::WORK);
  CRGB fillColor = isWork ? CHSV(cfg::pomo_color(), 255, 255) : CHSV(cfg::pomo_break_color(), 255, 255);

  for (uint8_t i = 0; i < LED_AMOUNT; i++)
    leds[i] = (i < filled) ? fillColor : bgColor();
  applyGlobalBrightness();
  showLedsSafe();
}

static void renderPomoDone() {
  static const uint8_t  BLINKS  = 5;
  static const uint32_t HALF_MS = 400;

  uint32_t elapsed = millis() - pomoDoneAt;
  if (elapsed >= (uint32_t)BLINKS * HALF_MS * 2) {
    pomoState = PomoState::CONFIRMED;
    return;
  }

  bool on = (elapsed / HALF_MS) % 2 == 0;
  fill_solid(leds, LED_AMOUNT, CRGB::White);
  applyGlobalBrightness(on ? 255 : 0);
  showLedsSafe();
}

static void renderPomoConfirmed() {
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  bool on = (millis() / 1000) % 2;
  CRGB c = on ? CRGB(255, 80, 0) : CRGB(40, 12, 0);
  for (uint8_t i = 4; i < 8; i++) leds[i] = c;
  applyGlobalBrightness();
  showLedsSafe();
}

static void renderPomoPaused() {
  uint32_t elapsed = pomoPauseAt - pomoStart;
  uint8_t  filled  = (uint8_t)((uint32_t)LED_AMOUNT * elapsed / pomoDuration);
  if (filled > LED_AMOUNT) filled = LED_AMOUNT;

  bool    on        = (millis() / 800) % 2;
  // Use work or break color depending on what state was paused
  uint8_t pc        = (pomoPrevState == PomoState::WORK) ? cfg::pomo_color() : cfg::pomo_break_color();
  CRGB    fillColor = CHSV(pc, 255, 255);

  for (uint8_t i = 0; i < LED_AMOUNT; i++)
    leds[i] = (i < filled) ? fillColor : bgColor();
  applyGlobalBrightness(on ? 255 : 40);
  showLedsSafe();
}

void renderPomodoro() {
  static Timer tmr(100);
  if (!tmr.period()) return;

  if (!cfg::power()) {
    fill_solid(leds, LED_AMOUNT, CRGB::Black);
    FastLED.setBrightness(0);
    showLedsSafe();
    return;
  }

  switch (pomoState) {
    case PomoState::IDLE:        renderPomoIdle();      break;
    case PomoState::WORK:
    case PomoState::BREAK_SHORT:
    case PomoState::BREAK_LONG:  renderPomoProgress();  break;
    case PomoState::DONE:        renderPomoDone();      break;
    case PomoState::CONFIRMED:   renderPomoConfirmed(); break;
    case PomoState::PAUSED:      renderPomoPaused();    break;
  }
}

// ─── tick ────────────────────────────────────────────────────────────────────

void pomodoroTick() {
  switch (pomoState) {
    case PomoState::WORK:        pomoDuration = (uint32_t)cfg::pomo_work()  * 60000UL; break;
    case PomoState::BREAK_SHORT: pomoDuration = (uint32_t)cfg::pomo_short() * 60000UL; break;
    case PomoState::BREAK_LONG:  pomoDuration = (uint32_t)cfg::pomo_long()  * 60000UL; break;
    default: return;
  }

  if ((uint32_t)(millis() - pomoStart) >= pomoDuration) {
    pomoPrevState = pomoState;
    pomoDoneAt    = millis();
    pomoState     = PomoState::DONE;
    if (mqtt.connected() && !cfg::offline()) {
      publishPomodoro("idle");
      publishDND(false);
    }
  }
}

// ─── button ──────────────────────────────────────────────────────────────────

static void pomoSessionAdvance() {
  switch (pomoState) {
    case PomoState::IDLE:
      pomoRound = 0;
      startPomoWork();
      break;
    case PomoState::WORK:
    case PomoState::BREAK_SHORT:
    case PomoState::BREAK_LONG:
      pomoPrevState = pomoState;
      pomoPauseAt   = millis();
      pomoState     = PomoState::PAUSED;
      break;
    case PomoState::PAUSED:
      pomoStart += (millis() - pomoPauseAt);
      pomoState  = pomoPrevState;
      break;
    case PomoState::DONE:
    case PomoState::CONFIRMED:
      startNextInterval();
      break;
  }
}

void pomodoroClick(uint8_t clicks) {
  if (clicks == 2) pomoSessionAdvance();
}

// Called from web UI buttons
void pomodoroWebAction() { pomoSessionAdvance(); }
void pomodoroWebSkip()   { if (pomoState != PomoState::IDLE) startNextInterval(); }

void pomodoroStep()        { globalBrightStep(); }
void pomodoroReleaseStep() { globalBrightReleaseStep(); }

// ─── web sync ────────────────────────────────────────────────────────────────

String pomoStateString() {
  switch (pomoState) {
    case PomoState::IDLE:        return "Ожидание";
    case PomoState::WORK:        return "Работа";
    case PomoState::BREAK_SHORT: return "Короткий перерыв";
    case PomoState::BREAK_LONG:  return "Длинный перерыв";
    case PomoState::PAUSED:      return "Пауза";
    case PomoState::DONE:        return "Завершено — подтвердите";
    case PomoState::CONFIRMED:   return "Готов к следующему";
  }
  return "";
}

const char* pomoSkipLabel() {
  return (pomoState == PomoState::IDLE) ? nullptr : "Пропустить";
}

const char* pomoActionLabel() {
  switch (pomoState) {
    case PomoState::IDLE:        return "Старт";
    case PomoState::WORK:
    case PomoState::BREAK_SHORT:
    case PomoState::BREAK_LONG:  return "Пауза";
    case PomoState::PAUSED:      return "Продолжить";
    case PomoState::DONE:
    case PomoState::CONFIRMED:   return "Подтвердить";
  }
  return "Старт";
}

void pomodoroWebTick() {
  static PomoState prevState = PomoState::IDLE;
  if (prevState == pomoState) return;
  prevState = pomoState;
  if (currentMode() == Mode::POMODORO) sett.reload();
}
