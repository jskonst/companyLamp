# План 10 — Режим 2: Помодоро

## Цель
Таймер Помодоро с визуализацией прогресса на кольце, звуковым (световым) сигналом
по завершению, автоматическим DND-сигналом во время работы.

---

## 10.1 Машина состояний

```
IDLE ──1клик──► WORK ──истёк──► DONE_WORK ──1клик──► DONE_CONFIRMED
                  ▲                                         │1клик
                  └─────────────────────────────────────────┘
                              (запуск следующего интервала)
DONE_CONFIRMED ──после long break──► IDLE

Из любого состояния: 2 клика = пропустить к следующему интервалу
Из WORK/BREAK: 1 клик = пауза/продолжить
```

```cpp
enum class PomoState {
  IDLE,
  WORK,
  BREAK_SHORT,
  BREAK_LONG,
  DONE,       // интервал завершён, мигаем
  CONFIRMED,  // мигание остановлено, ждём запуска следующего
  PAUSED,
};
```

---

## 10.2 Переменные состояния

```cpp
// src/mode_pomodoro.ino
static PomoState pomoState = PomoState::IDLE;
static uint32_t pomoStart = 0;       // millis() начала текущего интервала
static uint32_t pomoDuration = 0;    // длительность текущего интервала в мс
static uint8_t  pomoRound = 0;       // текущий номер помодоро (1-based)
static uint32_t pomoPauseAt = 0;     // millis() момента паузы
```

---

## 10.3 Визуализация прогресса на кольце

```cpp
void renderPomodoro() {
  static Timer tmr(100);
  if (!tmr.period()) return;

  switch (pomoState) {
    case PomoState::IDLE:
      renderPomoIdle();
      break;
    case PomoState::WORK:
    case PomoState::BREAK_SHORT:
    case PomoState::BREAK_LONG:
      renderPomoProgress();
      break;
    case PomoState::DONE:
      renderPomoDone();
      break;
    case PomoState::CONFIRMED:
      renderPomoConfirmed();
      break;
    case PomoState::PAUSED:
      renderPomoPaused();
      break;
  }
}
```

### Прогресс (заполнение кольца)
```cpp
void renderPomoProgress() {
  uint32_t elapsed = millis() - pomoStart;
  uint8_t filled = (uint8_t)((uint32_t)LED_AMOUNT * elapsed / pomoDuration);
  if (filled > LED_AMOUNT) filled = LED_AMOUNT;

  bool isWork = (pomoState == PomoState::WORK);
  CRGB fillColor   = isWork ? CHSV(cfg::pomo_color(), 255, 255) : CRGB::Green;
  CRGB emptyColor  = CRGB(10, 10, 10);  // очень тускло

  for (uint8_t i = 0; i < LED_AMOUNT; i++) {
    leds[i] = (i < filled) ? fillColor : emptyColor;
  }
  FastLED.setBrightness(80);
  showLedsSafe();
}
```

### Завершение (мигание)
```cpp
void renderPomoDone() {
  bool on = (millis() / 400) % 2;
  fill_solid(leds, LED_AMOUNT, on ? CRGB::White : CRGB::Black);
  FastLED.setBrightness(on ? 100 : 0);
  showLedsSafe();
}
```

### Подтверждено (ждём старта)
```cpp
void renderPomoConfirmed() {
  // секция 2 мигает медленно оранжевым
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  bool on = (millis() / 1000) % 2;
  CRGB c = on ? CRGB(255, 80, 0) : CRGB(40, 12, 0);
  for (uint8_t i = 4; i < 8; i++) leds[i] = c;
  FastLED.setBrightness(60);
  showLedsSafe();
}
```

### Пауза
```cpp
void renderPomoPaused() {
  // прогресс фиксирован, мигает медленно
  uint32_t elapsed = pomoPauseAt - pomoStart;
  uint8_t filled = (uint8_t)((uint32_t)LED_AMOUNT * elapsed / pomoDuration);

  bool on = (millis() / 800) % 2;
  CRGB fillColor = on ? CHSV(cfg::pomo_color(), 255, 255) : CRGB(20, 5, 0);

  for (uint8_t i = 0; i < LED_AMOUNT; i++) {
    leds[i] = (i < filled) ? fillColor : CRGB(10, 10, 10);
  }
  FastLED.setBrightness(60);
  showLedsSafe();
}
```

---

## 10.4 Тик таймера и переходы состояний

```cpp
void pomodoroTick() {
  if (pomoState != PomoState::WORK &&
      pomoState != PomoState::BREAK_SHORT &&
      pomoState != PomoState::BREAK_LONG) return;

  if ((uint32_t)(millis() - pomoStart) >= pomoDuration) {
    pomoState = PomoState::DONE;
    onPomoDone();
  }
}

void onPomoDone() {
  // снять DND если был рабочий интервал
  if (mqtt.connected() && !cfg::offline()) {
    publishPomodoro("idle");
    publishDND(false);
  }
}
```

---

## 10.5 Кнопка

Поведение 2 кликов зависит от состояния:
- **IDLE** → смена базового цвета (+32 по HSV, те же 8 цветов что у лампы)
- **Все остальные** → пропустить к следующему интервалу

```cpp
void pomodoroClick(uint8_t clicks) {
  if (clicks == 1) {
    switch (pomoState) {
      case PomoState::IDLE:
        startPomoWork();
        break;
      case PomoState::WORK:
      case PomoState::BREAK_SHORT:
      case PomoState::BREAK_LONG:
        // пауза
        pomoPauseAt = millis();
        pomoState = PomoState::PAUSED;
        break;
      case PomoState::PAUSED:
        // продолжить
        pomoStart += (millis() - pomoPauseAt);
        pomoState = PomoState::WORK;
        break;
      case PomoState::DONE:
        pomoState = PomoState::CONFIRMED;
        break;
      case PomoState::CONFIRMED:
        startNextInterval();
        break;
    }
  } else if (clicks == 2) {
    if (pomoState == PomoState::IDLE) {
      // смена базового цвета — как 2 клика в режиме лампы
      db[kk::pomo_color] = (uint8_t)(cfg::pomo_color() + 32);
      db.update();
    } else {
      // пропустить к следующему интервалу
      startNextInterval();
    }
  }
}
```

---

## 10.6 Переходы интервалов

```cpp
void startPomoWork() {
  pomoRound++;
  pomoStart = millis();
  pomoDuration = (uint32_t)cfg::pomo_work() * 60000UL;
  pomoState = PomoState::WORK;

  if (mqtt.connected() && !cfg::offline()) {
    String payload = "work:" + String(cfg::pomo_color());
    publishPomodoro(payload.c_str());
    publishDND(true);
  }
}

void startNextInterval() {
  bool isLongBreak = (pomoRound % cfg::pomo_count() == 0);

  if (pomoState == PomoState::WORK || pomoState == PomoState::CONFIRMED) {
    // переход к перерыву
    pomoStart = millis();
    if (isLongBreak) {
      pomoDuration = (uint32_t)cfg::pomo_long() * 60000UL;
      pomoState = PomoState::BREAK_LONG;
    } else {
      pomoDuration = (uint32_t)cfg::pomo_short() * 60000UL;
      pomoState = PomoState::BREAK_SHORT;
    }
    if (mqtt.connected() && !cfg::offline()) {
      publishPomodoro("break");
      publishDND(false);
    }
  } else {
    // из перерыва → к работе
    startPomoWork();
  }
}
```

---

## 10.7 Настройки в веб-интерфейсе

```cpp
// в build(), группа "Помодоро"
sets::Group g(b, "Помодоро");
b.Input(kk::pomo_work,  "Работа (мин)",           25);
b.Input(kk::pomo_short, "Короткий перерыв (мин)", 5);
b.Input(kk::pomo_long,  "Длинный перерыв (мин)",  15);
b.Input(kk::pomo_count, "Помодоро до длинного",   4);
b.Slider(kk::pomo_color, "Цвет работы (DND)", 0, 255, 1);
```

---

## 10.8 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode_pomodoro.ino` | Создать: всё выше |
| `src/sets.h` | Добавить `pomo_work`, `pomo_short`, `pomo_long`, `pomo_count`, `pomo_color` |
| `src/sets.cpp` | `db.init(...)` для всех, добавить группу в build() |
| `src/WebLamp_v1.0.ino` | loop(): добавить `pomodoroTick()` |

---

## Проверка

- [ ] Вход в режим POMODORO → кольцо пустое (IDLE)
- [ ] 2 клика в IDLE → цвет кольца меняется (+32), 8 шагов по кругу
- [ ] Web-слайдер и кнопка меняют один и тот же `pomo_color`
- [ ] 1 клик → начало работы, кольцо заполняется выбранным цветом слева направо
- [ ] Прогресс точный: за 25 мин — 24 LED заполнены
- [ ] По истечению → мигание белым
- [ ] 1 клик → мигание стоп, секция 2 мигает медленно (CONFIRMED)
- [ ] 1 клик → перерыв, кольцо заполняется зелёным
- [ ] 2 клика в любой момент → пропуск к следующему интервалу
- [ ] 1 клик во время работы → пауза (кольцо мигает медленно)
- [ ] (Онлайн) Во время работы: MQTT `pomodoro = "work:X"` и `dnd = "1"`
- [ ] (Онлайн) Перерыв: MQTT `pomodoro = "break"` и `dnd = "0"`
- [ ] После 4 помодоро → длинный перерыв
