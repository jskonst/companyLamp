# План 05 — Режим 1: Лампа

## Цель
Довести базовый режим лампы до полного соответствия спецификации:
плавная смена цвета, дыхание (если коллега онлайн), wink-эффект.

---

## 5.1 Рендер лампы

Заменить текущий `applyLampState()` (мгновенное переключение) на анимированный рендер
с плавными переходами, как в оригинале.

```cpp
// src/mode_lamp.ino
void renderLamp() {
  static Timer tmr(30);
  static CRGB currentColor = CRGB::Black;

  if (!tmr.period()) return;

  bool power = cfg::power();
  uint8_t bright = power ? map(cfg::bright(), 0, 100, 0, 255) : 0;
  uint8_t color  = cfg::color();

  // целевой цвет
  CRGB target;
  if (!power)       target = CRGB::Black;
  else if (color == 0) target = CRGB::White;
  else              target = CHSV(color, 255, 255);

  // плавный переход
  if (currentColor != target) {
    currentColor.r += (int16_t)(target.r - currentColor.r) * 17 / 255;
    currentColor.g += (int16_t)(target.g - currentColor.g) * 17 / 255;
    currentColor.b += (int16_t)(target.b - currentColor.b) * 17 / 255;
    // snap к цели если очень близко
    if (abs((int)currentColor.r - target.r) <= 1) currentColor.r = target.r;
    if (abs((int)currentColor.g - target.g) <= 1) currentColor.g = target.g;
    if (abs((int)currentColor.b - target.b) <= 1) currentColor.b = target.b;
  }

  fill_solid(leds, LED_AMOUNT, currentColor);
  FastLED.setBrightness(bright);
  showLedsSafe();
}
```

---

## 5.2 Wink-эффект (3 клика)

Восстановить из оригинала. Работает только если лампа включена.

```cpp
// src/mode_lamp.ino
void lampWink() {
  if (!cfg::power()) return;
  const uint8_t bright = map(cfg::bright(), 0, 100, 0, 255);
  brightLoop(bright, 0, 20);
  brightLoop(0, 255, 20);
  brightLoop(255, 0, 20);
  brightLoop(0, 255, 20);
  brightLoop(255, 0, 20);
  brightLoop(0, bright, 20);
}

void brightLoop(uint8_t from, uint8_t to, uint8_t step) {
  int val = from;
  while (true) {
    FastLED.setBrightness(val);
    showLedsSafe();
    delay(10);
    if (from > to) {
      val -= step;
      if (val <= (int)to) return;
    } else {
      val += step;
      if (val >= (int)to) return;
    }
  }
}
```

Вызов из диспетчера кнопки:
```cpp
void lampClick(uint8_t clicks) {
  if (clicks == 1) togglePowerSafe();
  else if (clicks == 2) { db[kk::color] = cfg::color() + 32; db.update(); }
  else if (clicks == 3) lampWink();  // был winkFlag=1, теперь напрямую
}
```

---

## 5.3 Удержание в режиме лампы

```cpp
void lampStep(int8_t dir) {
  int newBright = (int)cfg::bright() + dir;
  if (newBright >= 100) { newBright = 100; /* dir flip handled outside */ }
  if (newBright <= 0)   { newBright = 0; }
  db[kk::bright] = (uint8_t)newBright;
}
```

---

## 5.4 Дыхание (online-режим, MQTT)

Дыхание добавляется **после** реализации MQTT (план 07).
Здесь — только заглушка: если `onlineTmr.elapsed()` → дыхание выключено (яркость фиксирована).

---

## 5.5 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode_lamp.ino` | Создать: renderLamp, lampWink, brightLoop, lampClick, lampStep |
| `src/WebLamp_v1.0.ino` | Удалить `applyLampState()`, убрать `winkFlag` |
| `src/button.ino` | Убрать `winkFlag = 1`, вызывать через диспетчер |

---

## Проверка

- [ ] Включение/выключение: плавное затухание/появление (~0.5 сек)
- [ ] Смена цвета двойным кликом: плавный переход между 8 цветами
- [ ] Яркость удержанием: плавная регулировка, фиксация при отпускании
- [ ] 3 клика: подмигивание (двойная вспышка) если лампа включена
- [ ] 3 клика при выключенной лампе: нет эффекта
