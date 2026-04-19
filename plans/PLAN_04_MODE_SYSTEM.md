# План 04 — Система режимов и индикация на кольце

## Цель
Реализовать переключение между 5 режимами через 3/4 клика кнопки **или web-интерфейс**,
кратковременный показ текущего режима на кольце, и быстрый вызов митинга из **любого** режима.

---

## 4.1 Перечень режимов

```cpp
enum class Mode : uint8_t {
  LAMP     = 0,   // базовая лампа
  POMODORO = 1,   // помодоро
  MOOD     = 2,   // настроение: показывает среднее команды, личное настраивается
  MEETING  = 3,   // входящий / исходящий сигнал митинга
  DND      = 4,   // не беспокоить
};
constexpr uint8_t MODE_COUNT = 5;
```

> **Изменение vs оригинала:** режимы MOOD (личное) и TEAM (команда) объединены
> в один режим MOOD. Всего режимов — 5 вместо 6.

Хранится в `db[kk::mode]`, восстанавливается после перезагрузки.

---

## 4.2 Цвета секций (фиксированные)

| Режим | Секция (LED) | Цвет индикатора |
|---|---|---|
| LAMP | 0–3 | Тёплый белый |
| POMODORO | 4–7 | Оранжевый |
| MOOD | 8–11 | Зелёный (заменяется реальным средним командным) |
| MEETING | 12–15 | Голубой |
| DND | 16–19 | Красный |

LED 20–23 зарезервированы (будущий общий индикатор статуса / онлайн).

```cpp
const CRGB MODE_COLORS[MODE_COUNT] = {
  CRGB(255, 200, 100),  // LAMP
  CRGB(255, 80,  0  ),  // POMODORO
  CRGB(0,   255, 0  ),  // MOOD (заменяется реальным цветом)
  CRGB(0,   200, 255),  // MEETING
  CRGB(255, 0,   0  ),  // DND
};
```

---

## 4.3 Отображение индикатора режима

При переключении режима (3 или 4 клика, или web):
1. Всё кольцо гаснет.
2. Загорается только секция текущего режима (LED `mode*4` … `mode*4 + 3`).
3. Через 2 секунды — возврат к отображению активного режима.

```cpp
// src/mode.ino

static uint32_t modeIndicatorUntil = 0;
static bool modeIndicatorActive = false;

void showModeIndicator(Mode m) {
  fill_solid(leds, LED_AMOUNT, CRGB::Black);
  uint8_t base = (uint8_t)m * 4;
  CRGB color = MODE_COLORS[(uint8_t)m];
  for (uint8_t i = base; i < base + 4; i++) leds[i] = color;
  FastLED.setBrightness(80);
  showLedsSafe();
  modeIndicatorActive = true;
  modeIndicatorUntil = millis() + 2000;
}

void modeIndicatorTick() {
  if (modeIndicatorActive && (int32_t)(millis() - modeIndicatorUntil) >= 0) {
    modeIndicatorActive = false;
  }
}

bool isModeIndicatorActive() { return modeIndicatorActive; }
```

---

## 4.4 Смена режима из кнопки

В `button.ino`, функция `handleButtonClicks()`:
```cpp
} else if (clicks == 3) {
  uint8_t next = ((uint8_t)currentMode() + 1) % MODE_COUNT;
  setMode((Mode)next);
  showModeIndicator((Mode)next);
} else if (clicks == 4) {
  uint8_t prev = ((uint8_t)currentMode() + MODE_COUNT - 1) % MODE_COUNT;
  setMode((Mode)prev);
  showModeIndicator((Mode)prev);
}
```

---

## 4.5 Быстрый вызов митинга (cross-mode action)

**5 кликов из любого режима** отправляют сигнал митинга — без смены режима.
Условие: онлайн не отключён (`!cfg::offline()`) и MQTT подключён.

```cpp
} else if (clicks >= 5) {
  if (!cfg::offline() && mqtt.connected()) {
    publishMeeting();
    showMeetingFlash();   // вспышка голубым, затем возврат — режим не меняется
    DEBUGLN(F("[MTG] quick-call"));
  }
}
```

```cpp
// src/mode_meeting.ino
void showMeetingFlash() {
  // 3 мигания голубым за ~600 мс, потом автовозврат через modeIndicatorActive = false
  for (uint8_t n = 0; n < 3; n++) {
    fill_solid(leds, LED_AMOUNT, CRGB(0, 180, 255));
    FastLED.setBrightness(100); showLedsSafe(); delay(100);
    fill_solid(leds, LED_AMOUNT, CRGB::Black);
    FastLED.setBrightness(0);   showLedsSafe(); delay(100);
  }
  // после этого renderTick() сам восстановит текущий режим
}
```

> **Логика онлайн-блокировки:** если `cfg::offline()` == true — 5 кликов игнорируются
> (можно добавить короткий сигнал "нельзя" — одна красная вспышка).

---

## 4.6 Режим MOOD — объединённое настроение

Режим MOOD объединяет бывший личный MOOD и командный TEAM:

- **Рендер по умолчанию** — цвет **среднего настроения команды** (включая своё)
- **Установка личного настроения** — удержание кнопки (прокрутка цвета) или web-слайдер
- **1 клик** — отправить текущее личное настроение в команду
- При редактировании удержанием — кольцо показывает личный цвет как превью;
  после отпускания возвращается к среднему командному

Подробная реализация — в плане 08 (обновлённом).

---

## 4.7 Web-интерфейс — переключение режимов

### Архитектура: табы на главной странице

Одна страница (`/`) с панелью вкладок вверху. Каждый таб:
- активирует соответствующий режим на лампе (POST/WebSocket)
- показывает controls для этого режима (яркость/цвет/слайдер настроения и т.д.)

```
┌────────────────────────────────────────┐
│ [LAMP] [POMODORO] [MOOD] [MEET] [DND] │  ← таб-бар
├────────────────────────────────────────┤
│  controls текущего режима              │
└────────────────────────────────────────┘
```

Таб-бар делается минимальным JS (показать/скрыть `<div>`), без перегрузки страницы.
Активный таб автоматически соответствует `db[kk::mode]` при загрузке страницы.

### API

| Действие | Endpoint |
|---|---|
| Получить текущий режим | `GET /api/mode` → `{"mode":2}` |
| Установить режим | `POST /api/mode` body `mode=2` |
| Получить mood | `GET /api/mood` → `{"mood":90}` |
| Установить mood | `POST /api/mood` body `mood=90` |

Все endpoints возвращают JSON. Реализация — в планах 05–10 по каждому режиму.

### Примечание по смене режима через web

Смена режима из web выполняет `setMode()` + `showModeIndicator()` — те же функции,
что и кнопка. `button.ino` не затрагивается.

---

## 4.8 Диспетчер рендера

```cpp
void renderTick() {
  if (isModeIndicatorActive()) return;

  switch (currentMode()) {
    case Mode::LAMP:     renderLamp();     break;
    case Mode::POMODORO: renderPomodoro(); break;
    case Mode::MOOD:     renderMood();     break;
    case Mode::MEETING:  renderMeeting();  break;
    case Mode::DND:      renderDND();      break;
  }
}
```

---

## 4.9 Диспетчер действий кнопки по режиму

```cpp
static void handleButtonClicks(uint8_t clicks) {
  if (clicks >= 5) { meetingQuickCall(); return; }   // cross-mode, любой режим
  if (clicks == 4) { /* prev mode */ return; }
  if (clicks == 3) { /* next mode */ return; }

  switch (currentMode()) {
    case Mode::LAMP:     lampClick(clicks);     break;
    case Mode::POMODORO: pomodoroClick(clicks); break;
    case Mode::MOOD:     moodClick(clicks);     break;
    case Mode::MEETING:  meetingClick(clicks);  break;
    case Mode::DND:      dndClick(clicks);      break;
  }
}
```

`meetingQuickCall()` — в `mode_meeting.ino`, проверяет offline, вызывает `publishMeeting()` + `showMeetingFlash()`.

---

## 4.10 Архитектурное правило: button.ino только диспатчит

**`button.ino` не знает ни о каком состоянии лампы. Только вызывает функции.**

1. `togglePowerSafe()` → `lampClick(1)` в `lamp.ino`
2. `static int8_t dir` → в `lamp.ino`
3. `handleHold()` и `handleReleaseStep()` делегируют через switch по `currentMode()`

---

## 4.11 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode.ino` | Создать: Mode enum (5), showModeIndicator, renderTick, диспетчеры |
| `src/mode_meeting.ino` | Добавить: meetingQuickCall, showMeetingFlash |
| `src/sets.h` | Добавить `kk::mode` в DB_KEYS |
| `src/sets.cpp` | `db.init(kk::mode, 0)` |
| `src/button.ino` | 5 кликов → meetingQuickCall; 3/4 → режим; 1/2 → делегация |
| `src/WebLamp_v1.0.ino` | loop(): `applyLampState()` → `renderTick()` |
| `src/web.ino` | Создать: таб-бар, GET/POST `/api/mode`, `/api/mood` |

---

## Проверка

- [ ] 3 клика → секция следующего режима 2 сек → режим активен
- [ ] 4 клика → аналогично, назад
- [ ] 5 кликов в режиме LAMP → голубая вспышка, режим LAMP сохранился
- [ ] 5 кликов при `offline=true` → нет отправки (красная вспышка или тишина)
- [ ] Web: активный таб соответствует текущему режиму при загрузке
- [ ] Web: клик на таб → лампа переключила режим, секция мигнула
- [ ] Режим MOOD: кольцо цвета среднего команды; удержание → превью личного цвета
- [ ] Web слайдер mood → обновляет личное настроение и отправляет
- [ ] Режим сохраняется после перезагрузки
- [ ] Последовательность: LAMP→POMODORO→MOOD→MEETING→DND→LAMP
