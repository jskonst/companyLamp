# План 04b — Web UI: переключение режимов

## Цель
Добавить переключение режимов через web-интерфейс: таб-бар вверху страницы,
контролы активного режима под ним, визуальная обратная связь на кольце.

**Зависимость:** Plan 04 (режимы в firmware уже реализованы).

---

## 04b.1 Таб-бар режимов

SettingsGyver имеет готовый виджет `b.Tabs()` (builder.h:605).
Он рендерит строку вкладок, читает и записывает индекс в DB-ключ.

```cpp
// в build(), самый первый элемент
b.Tabs(kk::mode, "LAMP;POMODORO;MOOD;MEETING;DND");
```

При клике на вкладку:
- SettingsGyver записывает новый индекс в `db[kk::mode]`
- Страница перестраивается — показываются контролы нового режима
- `modeWebTick()` в firmware обнаруживает смену и вызывает `showModeIndicator()`

---

## 04b.2 Контролы по режимам

`build()` читает `db[kk::mode]` и показывает соответствующую группу:

```cpp
static void build(sets::Builder& b) {
    b.Tabs(kk::mode, "LAMP;POMODORO;MOOD;MEETING;DND");

    switch ((uint8_t)db[kk::mode]) {
        case 0: buildLamp(b);     break;
        case 1: buildPomodoro(b); break;
        case 2: buildMood(b);     break;
        case 3: buildMeeting(b);  break;
        case 4: buildDND(b);      break;
    }

    {
        sets::Menu wifi(b, "WiFi");
        buildWifi(b);
    }
}
```

### LAMP (Plan 04b реализует полностью)

```cpp
static void buildLamp(sets::Builder& b) {
    sets::Group g(b, "Лампа");
    b.Switch(kk::power, "Питание");
    b.Slider(kk::bright, "Яркость", 0, 100, 1);
    b.Slider(kk::color,  "Цвет",    0, 255, 1);
}
```

### Остальные режимы — заглушки (будут заменены в планах 05–10)

```cpp
static void buildPomodoro(sets::Builder& b) {
    sets::Group g(b, "Помодоро");
    b.Label("Реализуется в Plan 10");
}

static void buildMood(sets::Builder& b) {
    sets::Group g(b, "Настроение");
    b.Label("Реализуется в Plan 08");
}

static void buildMeeting(sets::Builder& b) {
    sets::Group g(b, "Митинг");
    b.Label("Реализуется в Plan 09");
}

static void buildDND(sets::Builder& b) {
    sets::Group g(b, "Не беспокоить");
    b.Label("Реализуется в Plan 07");
}
```

### WiFi — выносится в Menu

```cpp
static void buildWifi(sets::Builder& b) {
    b.Input(kk::ssid,   "SSID");
    b.Pass(kk::pass,    "Пароль", "");
    b.Input(kk::local,  "Имя лампы",    "AG_lamp_1");
    b.Input(kk::remote, "Имя коллеги",  "AG_lamp_2");
    b.Input(kk::host,   "MQTT хост",    "broker.mqttdashboard.com");
    b.Number(kk::port,  "Порт", nullptr, 1, 65535);
    if (b.Switch(kk::close_ap, "Скрыть точку доступа")) {
        WiFiConnector.closeAP(db[kk::close_ap]);
    }
    if (b.Button("Подключиться")) {
        WiFiConnector.connect(db[kk::ssid], db[kk::pass]);
    }
}
```

---

## 04b.3 Индикатор на кольце при web-смене режима

При клике на таб кольцо должно мигнуть секцией нового режима — как при смене кнопкой.

Реализуется через polling в `mode.ino`: сравниваем `db[kk::mode]` с предыдущим значением.

```cpp
// src/mode.ino — добавить

static uint8_t _prevDbMode = 255;

void modeWebTick() {
    uint8_t current = (uint8_t)db[kk::mode];
    if (_prevDbMode != current) {
        if (_prevDbMode != 255) {
            showModeIndicator((Mode)current);
        }
        _prevDbMode = current;
    }
}
```

`_prevDbMode = 255` при старте: при первом вызове индикатор не показывается
(инициализация, а не реальная смена).

Вызов в `loop()`:
```cpp
// src/WebLamp_v1.0.ino — в loop()
modeWebTick();
modeIndicatorTick();
```

> **Примечание:** при смене через кнопку `setMode()` вызывает `db.update()`,
> что меняет `db[kk::mode]` — `modeWebTick()` тоже сработает. Это безвредно:
> `showModeIndicator()` просто сбросит таймер на 2 сек (индикатор уже показан кнопкой).

---

## 04b.4 Изменения файлов

| Файл | Действие |
|---|---|
| `src/sets.cpp` | `build()` → таб-бар + dispatch на buildX(); `buildWifi()` в Menu |
| `src/mode.ino` | Добавить `modeWebTick()` |
| `src/WebLamp_v1.0.ino` | loop(): добавить `modeWebTick()` |

---

## Проверка

- [ ] Web: открыть страницу — активный таб соответствует текущему режиму
- [ ] Web: клик на таб → режим меняется, кольцо показывает секцию нового режима 2 сек
- [ ] Web: режим LAMP → показываются контролы Питание / Яркость / Цвет
- [ ] Web: POMODORO/MOOD/MEETING/DND → лейбл "Реализуется в Plan XX"
- [ ] Web: Menu "WiFi" → раскрывается настройки WiFi
- [ ] Web + кнопка: кнопка меняет режим → при следующем открытии страницы активный таб обновился
- [ ] Перезагрузка: режим восстановлен, таб правильный
