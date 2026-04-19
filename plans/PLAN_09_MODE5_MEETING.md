# План 09 — Режим 5: Позвать на митинг

## Цель
Одним нажатием отправить сигнал всем лампам. Получатели мигают,
подтверждают участие нажатием. DND смягчает сигнал.

---

## 9.1 Состояния

```
IDLE         → ничего не происходит, секция 5 горит тускло синим
INCOMING     → мигаем (получили сигнал от коллеги)
WAITING      → отправили сигнал, ждём подтверждений
```

---

## 9.2 Рендер

```cpp
// src/mode_meeting.ino
enum class MeetingState { IDLE, INCOMING, WAITING };
static MeetingState meetingState = MeetingState::IDLE;
static uint32_t meetingUntil = 0;

void renderMeeting() {
  static Timer tmr(100);
  if (!tmr.period()) return;

  uint32_t now = millis();

  // таймаут
  if (meetingState != MeetingState::IDLE &&
      (int32_t)(now - meetingUntil) >= 0) {
    meetingState = MeetingState::IDLE;
  }

  switch (meetingState) {
    case MeetingState::IDLE: {
      fill_solid(leds, LED_AMOUNT, CRGB::Black);
      for (uint8_t i = 16; i < 20; i++) leds[i] = CRGB(0, 60, 80);
      FastLED.setBrightness(40);
      break;
    }
    case MeetingState::INCOMING: {
      // мигание: 500мс вкл, 500мс выкл
      bool on = (now / 500) % 2;
      fill_solid(leds, LED_AMOUNT, on ? CRGB(0, 150, 255) : CRGB::Black);
      FastLED.setBrightness(on ? 100 : 0);
      break;
    }
    case MeetingState::WAITING: {
      // медленный пульс — ждём ответа
      uint8_t phase = (now / 1000) % 2 ? 80 : 30;
      fill_solid(leds, LED_AMOUNT, CRGB(0, 100, 200));
      FastLED.setBrightness(phase);
      break;
    }
  }
  showLedsSafe();
}
```

---

## 9.3 Входящий сигнал

```cpp
void onMeetingSignal(const String& sender) {
  DEBUG(F("[MTG] incoming from ")); DEBUGLN(sender);

  if (cfg::dnd()) {
    // DND активен — тихий сигнал (не переключаем режим, просто пульс на секции)
    showMeetingQuiet();
    return;
  }

  meetingState = MeetingState::INCOMING;
  meetingUntil = millis() + 30000;  // 30 сек
  // переключить в режим MEETING для видимости
  setMode(Mode::MEETING);
  showModeIndicator(Mode::MEETING);
}

void showMeetingQuiet() {
  // слабый пульс секции 5, без переключения режима
  for (uint8_t i = 16; i < 20; i++) leds[i] = CRGB(0, 40, 60);
  showLedsSafe();
}
```

---

## 9.4 Действия кнопки

```cpp
void meetingClick(uint8_t clicks) {
  if (clicks == 1) {
    if (meetingState == MeetingState::INCOMING) {
      // подтвердить участие
      meetingState = MeetingState::IDLE;
      DEBUGLN(F("[MTG] confirmed"));
    } else {
      // отправить сигнал митинга
      if (mqtt.connected() && !cfg::offline()) publishMeeting();
      meetingState = MeetingState::WAITING;
      meetingUntil = millis() + 30000;
      DEBUGLN(F("[MTG] sent"));
    }
  } else if (clicks == 2) {
    // отменить / подтвердить
    meetingState = MeetingState::IDLE;
  }
}
```

---

## 9.5 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode_meeting.ino` | Создать: renderMeeting, onMeetingSignal, meetingClick |
| `src/mqtt.ino` | callback: subtopic == "meeting" → `onMeetingSignal(sender)` |

---

## Проверка

- [ ] Вход в режим MEETING → секция 5 тускло голубая
- [ ] 1 клик → отправка сигнала, режим переходит в WAITING (пульс)
- [ ] На другой лампе: кольцо мигает синим, автоматически переключается в режим MEETING
- [ ] 1 клик на получателе → подтверждение, IDLE
- [ ] Таймаут 30 сек без подтверждения → автоматически IDLE
- [ ] Получатель в DND → тихий пульс секции 5 без переключения режима
