# План 08 — Режим MOOD: Настроение (личное + команда)

> **Изменение архитектуры (Plan 04 rev):** бывшие режимы 3 (личное) и 4 (команда)
> объединены в один режим MOOD = 2. Всего режимов стало 5.

## Цель
Один режим MOOD: по умолчанию показывает среднее настроение команды.
Личное настроение выставляется удержанием кнопки или web-слайдером и участвует в среднем.

---

## 8.1 Режим 3 — Личное настроение

### Рендер
```cpp
// src/mode_mood.ino
void renderMood() {
  uint8_t hue = cfg::mood();  // 0=красный, 120=зелёный в HSV
  CRGB color = CHSV(hue, 255, 255);
  fill_solid(leds, LED_AMOUNT, color);
  FastLED.setBrightness(60);
  showLedsSafe();
}
```

### Удержание — выбор цвета
```cpp
static uint8_t moodDraft = 120;  // рабочее значение при выборе
static bool moodEditing = false;

void moodHold() {
  // прокрутка от зелёного (120) к красному (0) и обратно
  if (!moodEditing) {
    moodEditing = true;
    moodDraft = cfg::mood();
  }
  // step -2 при удержании (полный круг за ~2 сек)
  moodDraft = (moodDraft >= 2) ? moodDraft - 2 : 120;
  db[kk::mood] = moodDraft;  // обновляем без записи на флеш

  // показываем текущий выбор
  CRGB color = CHSV(moodDraft, 255, 255);
  fill_solid(leds, LED_AMOUNT, color);
  FastLED.setBrightness(60);
  showLedsSafe();
}

void moodReleaseHold() {
  moodEditing = false;
  db.update();  // сохраняем на флеш
}
```

### 1 клик — подтвердить и отправить
```cpp
void moodClick(uint8_t clicks) {
  if (clicks != 1) return;
  db.update();
  if (mqtt.connected() && !cfg::offline()) publishMood(cfg::mood());
  DEBUGLN(F("[MOOD] sent"));
}
```

### Сброс ежедневно
```cpp
void moodDailyReset() {
  // вызывается в loop() при смене даты (NTP) или при перезагрузке на следующий день
  uint32_t today = getUnixDay();  // секунды / 86400
  if ((uint32_t)db[kk::mood_date] != today) {
    db[kk::mood] = 120;  // сброс к зелёному
    db[kk::mood_date] = today;
    db.update();
  }
}
```

**Примечание:** для `getUnixDay()` нужен NTP (план 09). До его реализации —
сброс только при перезагрузке если нет сохранённой даты.

---

## 8.2 Режим 4 — Настроение команды

### Хранение данных команды
```cpp
// in mode_mood.ino
static uint8_t teamMoods[4] = {120, 120, 120, 120};  // mood каждой лампы
static uint8_t teamCount = 0;

void onRemoteMoodUpdate() {
  // пересчитать среднее
  // вызывается из mqtt callback при получении mood
  teamCount = 0;
  uint16_t sum = 0;
  for (uint8_t i = 0; i < remoteCount; i++) {
    if (remotes[i].online()) {
      sum += remotes[i].mood;
      teamCount++;
    }
  }
  // добавить своё настроение
  sum += cfg::mood();
  teamCount++;
  teamAvgMood = (uint8_t)(sum / teamCount);
}

static uint8_t teamAvgMood = 120;
```

### Рендер
```cpp
void renderTeam() {
  if (teamCount == 0) {
    // нет данных — тусклый белый
    fill_solid(leds, LED_AMOUNT, CRGB::White);
    FastLED.setBrightness(15);
    showLedsSafe();
    return;
  }

  CRGB color = CHSV(teamAvgMood, 200, 255);
  fill_solid(leds, LED_AMOUNT, color);
  FastLED.setBrightness(60);
  showLedsSafe();
}
```

---

## 8.3 Изменения файлов

| Файл | Действие |
|---|---|
| `src/mode_mood.ino` | Создать: renderMood, moodHold, moodClick, renderTeam, onRemoteMoodUpdate |
| `src/sets.h` | Добавить `kk::mood`, `kk::mood_date` в DB_KEYS |
| `src/sets.cpp` | `db.init(kk::mood, 120)`, `db.init(kk::mood_date, 0)` |
| `src/sets.cpp` | В build(): добавить слайдер настроения |
| `src/mqtt.ino` | callback: при получении mood → `onRemoteMoodUpdate()` |

---

## Проверка

### Режим 3 (Настроение):
- [ ] Удержание: кольцо меняет цвет от зелёного к красному и обратно
- [ ] Отпустить → цвет фиксируется и сохраняется
- [ ] 1 клик → отправка mood по MQTT
- [ ] Другая лампа получила обновление (в её режиме 4 изменился цвет)
- [ ] После перезагрузки → цвет настроения восстановлен

### Режим 4 (Команда):
- [ ] При онлайн-коллегах → кольцо цвета среднего настроения
- [ ] При отсутствии данных → тусклый белый
- [ ] Настроение обновляется в реальном времени при изменении у коллег
- [ ] Ежедневный сброс: после смены дня настроение возвращается к зелёному
