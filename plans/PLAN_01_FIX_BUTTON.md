# План 01 — Исправление кнопки

## Проблема

Текущий код использует `btn.click()` + ручной агрегатор кликов.
Но `btn.click()` в EncButton v3 уже **ждёт таймаут** и возвращает `true` один раз
после завершения последовательности. Ручной агрегатор поверх него конфликтует:
он пытается накопить клики, но к моменту первого `btn.click()` окно уже истекло —
и никогда не набирает больше одного клика.

Оригинал (WebLamp_v1.0) использовал `btn.hasClicks()` из EncButton v2.
В EncButton v3 аналог — `btn.getClicks()` вызывается сразу после `btn.click()`.

## Решение

В `button.ino` заменить ручной агрегатор на:
```cpp
if (btn.click()) {
  handleButtonClicks(btn.getClicks());
}
```

Аналогично поправить логику удержания — в оригинале `dir` флипается при `releaseStep`,
что давало ожидаемое поведение (каждое удержание меняет направление).
Текущая правка (флип на границах) — тоже допустима, решить окончательно по тесту.

## Изменения

**Файл:** `src/button.ino`

### До (удалить):
```cpp
static uint8_t pendingClicks = 0;
static uint32_t clickDeadline = 0;

if (btn.click()) {
  if (pendingClicks < 3) pendingClicks++;
  clickDeadline = millis() + BTN_CLICK_TIMEOUT;
  DEBUG(F("[BTN] click registered, pending=")); DEBUGLN(pendingClicks);
}

if (pendingClicks && (int32_t)(millis() - clickDeadline) >= 0) {
  DEBUG(F("[BTN] firing clicks=")); DEBUGLN(pendingClicks);
  handleButtonClicks(pendingClicks);
  Serial.printf("[BTN] after action: ...\n");
  pendingClicks = 0;
}
```

### После (заменить на):
```cpp
if (btn.click()) {
  uint8_t clicks = btn.getClicks();
  DEBUG(F("[BTN] click n=")); DEBUGLN(clicks);
  handleButtonClicks(clicks);
  Serial.printf("[BTN] after: power=%d bright=%u color=%u\n",
    (int)cfg::power(), (unsigned)cfg::bright(), (unsigned)cfg::color());
}
```

Также в `handleButtonClicks` для кликов ≥ 4:
```cpp
// 3/4 клика — смена режима (будет в плане 04)
// пока — заглушка с логом
} else if (clicks == 3) {
  DEBUGLN(F("[BTN] TODO: next mode"));
} else if (clicks >= 4) {
  DEBUGLN(F("[BTN] TODO: prev mode"));
}
```

Также убрать `#define BTN_CLICK_TIMEOUT` из WebLamp_v1.0.ino (он больше не нужен
для агрегатора, но EncButton использует свой таймаут через `btn.setClickTimeout()`).

## Проверка

### Serial лог при 1 клике:
```
[BTN] press
[BTN] release
[BTN] click n=1
[BTN] after: power=1 bright=20 color=0
```

### Serial лог при 2 кликах:
```
[BTN] press
[BTN] release
[BTN] press
[BTN] release
[BTN] click n=2
[BTN] after: power=1 bright=20 color=32
```

### Визуальная проверка:
- [ ] 1 клик → лампа включается / выключается
- [ ] 2 клика → цвет меняется (8 шагов, каждый +32 по HSV)
- [ ] Удержание → яркость плавно меняется
- [ ] Отпустить → яркость зафиксирована
- [ ] Нет ложных срабатываний при медленных одиночных кликах
