# План 07 — Режим 6: Не беспокоить (DND)

## Цель
Пользователь одним кликом включает красный сигнал «занят».
Если онлайн — коллеги видят это на своих лампах (митинг не мигает, DND обозначен).

---

## 7.1 Рендер

```cpp
// src/mode_dnd.ino
void renderDND() {
  static Timer tmr(30);
  static uint8_t breath = 0;
  static int8_t dir = 1;

  if (!tmr.period()) return;

  bool active = cfg::dnd();
  if (!active) {
    // режим неактивен — показываем секцию 6 тускло
    fill_solid(leds, LED_AMOUNT, CRGB::Black);
    for (uint8_t i = 20; i < 24; i++) leds[i] = CRGB(30, 0, 0);
    FastLED.setBrightness(40);
    showLedsSafe();
    return;
  }

  // активен — медленное дыхание красным по всему кольцу
  breath += dir;
  if (breath >= 100) { breath = 100; dir = -1; }
  if (breath <= 30)  { breath = 30;  dir =  1; }

  fill_solid(leds, LED_AMOUNT, CRGB::Red);
  FastLED.setBrightness(breath);
  showLedsSafe();
}
```

---

## 7.2 Действия кнопки

```cpp
void dndClick(uint8_t clicks) {
  if (clicks != 1) return;
  bool next = !cfg::dnd();
  db[kk::dnd] = next;
  db.update();
  if (mqtt.connected() && !cfg::offline()) publishDND(next);
  DEBUG(F("[DND] ")); DEBUGLN(next ? "ON" : "OFF");
}
```

---

## 7.3 Влияние на входящий сигнал митинга

В callback митинга (план 08) проверять:
```cpp
void onMeetingSignal(const String& sender) {
  if (cfg::dnd()) {
    // тихий режим — слабая подсветка без мигания
    showMeetingQuiet();
  } else {
    showMeetingBlink();
  }
}
```

---

## 7.4 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode_dnd.ino` | Создать: renderDND, dndClick |
| `src/sets.h` | `kk::dnd` уже добавлен в плане 06 |
| `src/sets.cpp` | `db.init(kk::dnd, false)` |

---

## Проверка

- [ ] Вход в режим DND (3/4 клика) → секция 6 тускло красная
- [ ] 1 клик в режиме DND → всё кольцо медленно дышит красным
- [ ] Ещё 1 клик → возврат к тусклой секции
- [ ] Состояние сохраняется после перезагрузки
- [ ] (Онлайн) MQTT publish `GWL:<lamp>/dnd` = "1" при включении
- [ ] (Онлайн) Другая лампа получила DND → входящий митинг будет тихим
