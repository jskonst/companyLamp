# План 04 — Система режимов и индикация на кольце

## Цель
Реализовать переключение между 6 режимами через 3/4 клика кнопки
и кратковременный показ текущего режима на кольце.

---

## 4.1 Перечень режимов

```cpp
enum class Mode : uint8_t {
  LAMP     = 0,   // базовая лампа
  POMODORO = 1,   // помодоро
  MOOD     = 2,   // личное настроение
  TEAM     = 3,   // настроение команды
  MEETING  = 4,   // позвать на митинг
  DND      = 5,   // не беспокоить
};
constexpr uint8_t MODE_COUNT = 6;
```

Хранится в `db[kk::mode]`, восстанавливается после перезагрузки.

---

## 4.2 Цвета секций (фиксированные)

| Режим | Секция (LED) | Цвет индикатора |
|---|---|---|
| LAMP | 0–3 | Тёплый белый |
| POMODORO | 4–7 | Оранжевый |
| MOOD | 8–11 | Зелёный (текущее настроение) |
| TEAM | 12–15 | Синий (среднее настроения) |
| MEETING | 16–19 | Голубой |
| DND | 20–23 | Красный |

```cpp
const CRGB MODE_COLORS[MODE_COUNT] = {
  CRGB(255, 200, 100),  // LAMP: тёплый белый
  CRGB(255, 80,  0  ),  // POMODORO: оранжевый
  CRGB(0,   255, 0  ),  // MOOD: зелёный (заменяется реальным цветом)
  CRGB(0,   100, 255),  // TEAM: синий
  CRGB(0,   200, 255),  // MEETING: голубой
  CRGB(255, 0,   0  ),  // DND: красный
};
```

---

## 4.3 Отображение индикатора режима

При переключении режима (3 или 4 клика):
1. Всё кольцо гаснет.
2. Загорается только секция текущего режима (LED `mode*4` … `mode*4 + 3`).
3. Через 2 секунды — возврат к отображению активного режима.

```cpp
// новый файл: src/mode.ino

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
    // рендер вернётся к активному режиму через applyModeRender()
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
} else if (clicks >= 4) {
  uint8_t prev = ((uint8_t)currentMode() + MODE_COUNT - 1) % MODE_COUNT;
  setMode((Mode)prev);
  showModeIndicator((Mode)prev);
}
```

---

## 4.5 Диспетчер рендера

В `loop()` добавить вызов диспетчера рендера, который передаёт управление
нужному режиму:

```cpp
void renderTick() {
  if (isModeIndicatorActive()) return;  // показываем индикатор, не рендерим

  switch (currentMode()) {
    case Mode::LAMP:     renderLamp();     break;
    case Mode::POMODORO: renderPomodoro(); break;
    case Mode::MOOD:     renderMood();     break;
    case Mode::TEAM:     renderTeam();     break;
    case Mode::MEETING:  renderMeeting();  break;
    case Mode::DND:      renderDND();      break;
  }
}
```

Каждый `render*()` — это неблокирующая функция, вызываемая каждый `loop()`.
На этапе плана 04 все кроме LAMP — заглушки (просто заполняют кольцо цветом режима).

---

## 4.6 Диспетчер действий кнопки по режиму

Функция `handleButtonClicks()` должна делегировать 1/2 клика активному режиму:

```cpp
static void handleButtonClicks(uint8_t clicks) {
  if (clicks == 3) { /* next mode */ return; }
  if (clicks >= 4) { /* prev mode */ return; }

  switch (currentMode()) {
    case Mode::LAMP:     lampClick(clicks);     break;
    case Mode::POMODORO: pomodoroClick(clicks); break;
    case Mode::MOOD:     moodClick(clicks);     break;
    case Mode::TEAM:     /* read-only */        break;
    case Mode::MEETING:  meetingClick(clicks);  break;
    case Mode::DND:      dndClick(clicks);      break;
  }
}
```

Аналогично `handleHold()` и `handleReleaseStep()` для удержания.

---

## 4.7 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode.ino` | Создать: Mode enum, showModeIndicator, renderTick, диспетчеры |
| `src/sets.h` | Добавить `kk::mode` в DB_KEYS |
| `src/sets.cpp` | `db.init(kk::mode, 0)` |
| `src/button.ino` | handleButtonClicks: 3/4 клика → смена режима; 1/2 → делегация |
| `src/WebLamp_v1.0.ino` | loop(): `applyLampState()` → `renderTick()` |

---

## 4.8 Архитектурное правило: button.ino только диспатчит

По мере роста режимов есть риск, что вся логика уйдёт в `button.ino`.
Чтобы этого не случилось — жёсткое правило:

**`button.ino` не знает ни о каком состоянии лампы. Только вызывает функции.**

Конкретные точки, которые нужно соблюсти при реализации 4.6:

1. `togglePowerSafe()` переезжает в `lamp.ino` и называется `lampClick(1)` —
   логика "восстановить яркость до 20" — это поведение режима LAMP, не кнопки.

2. Переменная `static int8_t dir` переезжает в `lamp.ino`
   (или в состояние режима LAMP) — чтобы можно было сбросить направление
   при выключении/смене режима. В `button.ino` остаётся только вызов
   `lampStepBright()` / `lampReleaseStep()`.

3. `handleHold()` и `handleReleaseStep()` из 4.6 делегируют так же, как клики —
   через switch по `currentMode()`.

Итог: `button.ino` = тонкий диспетчер. Вся логика — в `lamp.ino`, `dnd.ino` и т.д.

---

## Проверка

- [ ] 3 клика → на 2 сек загорается секция следующего режима → гаснет → работает режим
- [ ] 4 клика → аналогично, в обратную сторону
- [ ] Режим сохраняется в памяти и восстанавливается после перезагрузки
- [ ] В режиме LAMP: 1 клик = вкл/выкл, 2 клика = цвет, удержание = яркость
- [ ] В остальных режимах (заглушки): кольцо горит цветом режима
- [ ] Последовательное переключение: LAMP→POMODORO→MOOD→TEAM→MEETING→DND→LAMP
