#pragma once
// Чистая логика группы ламп. Без Arduino, чтобы её гонял native-тест.
// Договорённости — docs/protocol.md. Здесь их нельзя «улучшать» молча:
// шкала настроения, формула оттенка, шаги двойного клика, топики, JSON.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Жесты кнопки: число кликов подряд. Источник — docs/protocol.md.
// Чтобы сменить раскладку, правьте константы, таблицу в README
// и тест test_gestures_match_protocol.
static const uint8_t GESTURE_WINK = 1;        // подмигнуть остальным
static const uint8_t GESTURE_MOOD = 2;        // следующий шаг настроения
static const uint8_t GESTURE_POWER = 3;       // вкл/выкл
static const uint8_t GESTURE_AVERAGE = 4;     // мой цвет / среднее
static const uint8_t GESTURE_AUTONOMOUS = 5;  // выйти из группы / вернуться

static const uint8_t MOOD_MIN = 0;
static const uint8_t MOOD_MAX = 100;
static const int MOOD_STEP_COUNT = 5;

enum class PeerStatus : uint8_t {
  None = 0,
  Online = 1,
  Away = 2,
  Offline = 3
};

struct MoodSample {
  uint8_t mood;
  bool online;
};

struct TopicParts {
  char group[21];
  char id[7];
  char leaf[8];
  bool ok;
};

inline uint8_t clampMood(int mood) {
  if (mood < MOOD_MIN) return MOOD_MIN;
  if (mood > MOOD_MAX) return MOOD_MAX;
  return (uint8_t)mood;
}

// hue(m) = m <= 50 ? m * 64 / 50 : 64 + (m - 50) * 32 / 50
// Целочисленное деление, как в протоколе: 0 красный, 50/64 жёлтый, 100/96 зелёный.
inline uint8_t moodToHue(uint8_t mood) {
  if (mood > MOOD_MAX) mood = MOOD_MAX;
  if (mood <= 50) return (uint8_t)((uint16_t)mood * 64 / 50);
  return (uint8_t)(64 + (uint16_t)(mood - 50) * 32 / 50);
}

// Двойной клик: 0 → 25 → 50 → 75 → 100 → 0.
// Значение между шагами (ползунок) прыгает на ближайший больший шаг.
inline uint8_t nextMoodStep(uint8_t mood) {
  static const uint8_t steps[MOOD_STEP_COUNT] = {0, 25, 50, 75, 100};
  if (mood > MOOD_MAX) mood = MOOD_MAX;
  for (int i = 0; i < MOOD_STEP_COUNT; i++) {
    if (steps[i] > mood) return steps[i];
  }
  return 0;
}

// Среднее арифметическое m (не оттенка по кругу).
// Красный 0 и зелёный 100 дают 50, а hue(50) = 64 — жёлтый, не синий.
// Округление до ближайшего, .5 вверх. includeSelf — «плюс своя лампа».
inline bool averageMood(const MoodSample* samples, int count, uint8_t selfMood,
                        bool includeSelf, uint8_t* avgOut) {
  if (!avgOut) return false;
  uint32_t sum = 0;
  uint16_t n = 0;
  if (samples) {
    for (int i = 0; i < count; i++) {
      if (!samples[i].online) continue;
      sum += samples[i].mood;
      n++;
    }
  }
  if (includeSelf) {
    sum += selfMood;
    n++;
  }
  if (!n) return false;
  *avgOut = (uint8_t)((sum + n / 2) / n);
  return true;
}

inline bool validGroupName(const char* name) {
  if (!name || !name[0]) return false;
  size_t len = strlen(name);
  if (len > 20) return false;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-';
    if (!ok) return false;
  }
  return true;
}

inline void formatChipId(uint32_t chipId, char out[7]) {
  snprintf(out, 7, "%06x", (unsigned)(chipId & 0xFFFFFFu));
}

inline bool sameLampId(const char* a, const char* b) {
  if (!a || !b) return false;
  for (int i = 0; i < 6; i++) {
    unsigned char ca = (unsigned char)a[i];
    unsigned char cb = (unsigned char)b[i];
    if (!ca || !cb) return false;
    if (tolower(ca) != tolower(cb)) return false;
  }
  return a[6] == '\0' && b[6] == '\0';
}

inline void canonLampId(char out[7], const char* id) {
  for (int i = 0; i < 6; i++) {
    unsigned char c = id ? (unsigned char)id[i] : 0;
    if (c >= 'A' && c <= 'F') c = (unsigned char)(c - 'A' + 'a');
    out[i] = (char)c;
  }
  out[6] = '\0';
}

// lamp/<group>/<id>/<leaf>
inline int buildLampTopic(char* dst, size_t dstLen, const char* group,
                          const char* id, const char* leaf) {
  if (!dst || !dstLen) return -1;
  return snprintf(dst, dstLen, "lamp/%s/%s/%s", group ? group : "", id ? id : "",
                  leaf ? leaf : "");
}

// lamp/<group>/+/<leaf>
inline int buildGroupFilter(char* dst, size_t dstLen, const char* group,
                            const char* leaf) {
  if (!dst || !dstLen) return -1;
  return snprintf(dst, dstLen, "lamp/%s/+/%s", group ? group : "", leaf ? leaf : "");
}

inline int buildClientId(char* dst, size_t dstLen, const char* id) {
  if (!dst || !dstLen) return -1;
  return snprintf(dst, dstLen, "lamp-%s", id ? id : "");
}

inline bool parseLampTopic(const char* topic, TopicParts* out) {
  if (out) memset(out, 0, sizeof(*out));
  if (!topic || !out || strncmp(topic, "lamp/", 5) != 0) return false;

  const char* group = topic + 5;
  const char* slash1 = strchr(group, '/');
  if (!slash1) return false;
  size_t groupLen = (size_t)(slash1 - group);
  if (groupLen == 0 || groupLen > 20) return false;
  memcpy(out->group, group, groupLen);
  out->group[groupLen] = '\0';
  if (!validGroupName(out->group)) return false;

  const char* id = slash1 + 1;
  const char* slash2 = strchr(id, '/');
  if (!slash2) return false;
  if ((size_t)(slash2 - id) != 6) return false;
  for (int i = 0; i < 6; i++) {
    unsigned char c = (unsigned char)id[i];
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hex) return false;
  }
  canonLampId(out->id, id);

  const char* leaf = slash2 + 1;
  if (!leaf[0] || strchr(leaf, '/')) return false;
  if (strcmp(leaf, "status") != 0 && strcmp(leaf, "state") != 0 && strcmp(leaf, "wink") != 0) {
    return false;
  }
  strncpy(out->leaf, leaf, sizeof(out->leaf) - 1);
  out->ok = true;
  return true;
}

inline PeerStatus parseStatusPayload(const char* text) {
  if (!text) return PeerStatus::None;
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
  char buf[16];
  size_t i = 0;
  while (text[i] && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n' &&
         i + 1 < sizeof(buf)) {
    buf[i] = text[i];
    i++;
  }
  buf[i] = '\0';
  if (strcmp(buf, "online") == 0) return PeerStatus::Online;
  if (strcmp(buf, "away") == 0) return PeerStatus::Away;
  if (strcmp(buf, "offline") == 0) return PeerStatus::Offline;
  return PeerStatus::None;
}

inline const char* jsonFindKey(const char* json, const char* key) {
  if (!json || !key) return nullptr;
  size_t keyLen = strlen(key);
  bool inString = false;
  bool escaped = false;
  for (const char* p = json; *p; p++) {
    if (inString) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (*p == '\\') {
        escaped = true;
        continue;
      }
      if (*p == '"') inString = false;
      continue;
    }
    if (*p != '"') continue;
    if (strncmp(p + 1, key, keyLen) == 0 && p[1 + keyLen] == '"') {
      const char* value = p + 2 + keyLen;
      while (*value == ' ' || *value == '\t') value++;
      if (*value == ':') {
        value++;
        while (*value == ' ' || *value == '\t') value++;
        return value;
      }
    }
    inString = true;
  }
  return nullptr;
}

inline bool parseJsonInt(const char* text, int* out) {
  if (!text || !out) return false;
  bool neg = false;
  if (*text == '-') {
    neg = true;
    text++;
  }
  if (*text < '0' || *text > '9') return false;
  long value = 0;
  while (*text >= '0' && *text <= '9') {
    value = value * 10 + (*text - '0');
    if (value > 100000L) return false;
    text++;
  }
  if (neg) value = -value;
  *out = (int)value;
  return true;
}

// Имя в state. Управляющие символы выкидываем: в списке портала они ломают строку.
inline bool parseJsonString(const char* text, char* dst, size_t dstLen) {
  if (!text || !dst || !dstLen || *text != '"') return false;
  text++;
  size_t written = 0;
  while (*text && *text != '"') {
    char ch = *text++;
    if (ch == '\\') {
      if (!*text) return false;
      char esc = *text++;
      if (esc == '"' || esc == '\\' || esc == '/') ch = esc;
      else if (esc == 'n') ch = '\n';
      else if (esc == 'r') ch = '\r';
      else if (esc == 't') ch = '\t';
      else if (esc == 'u') {
        for (int i = 0; i < 4 && *text; i++) text++;
        ch = '?';
      } else ch = esc;
    }
    if ((unsigned char)ch < 0x20) continue;
    if (written + 1 < dstLen) dst[written++] = ch;
  }
  if (*text != '"') return false;
  dst[written] = '\0';
  return true;
}

// m обязателен. n необязателен: нет поля или пустая строка — имени нет.
inline bool parseStatePayload(const char* json, uint8_t* mood, char* name, size_t nameLen) {
  if (name && nameLen) name[0] = '\0';
  int raw = 0;
  if (!parseJsonInt(jsonFindKey(json, "m"), &raw)) return false;
  if (mood) *mood = clampMood(raw);
  const char* nameValue = jsonFindKey(json, "n");
  if (nameValue && *nameValue == '"' && name && nameLen) {
    parseJsonString(nameValue, name, nameLen);
  }
  return true;
}

inline bool parseWinkPayload(const char* json, uint8_t* hue) {
  int raw = 0;
  if (!parseJsonInt(jsonFindKey(json, "h"), &raw)) return false;
  if (raw < 0 || raw > 255) return false;
  if (hue) *hue = (uint8_t)raw;
  return true;
}

// {"m":75} или {"m":75,"n":"Женя"} — без пробелов, как в примере протокола.
// Возвращает длину или -1, если буфер мал.
inline int formatStatePayload(char* dst, size_t dstLen, uint8_t mood, const char* name) {
  if (!dst || !dstLen) return -1;
  mood = clampMood(mood);
  if (!name || !name[0]) return snprintf(dst, dstLen, "{\"m\":%u}", mood);

  char escaped[96];
  size_t j = 0;
  for (size_t i = 0; name[i] && j + 2 < sizeof(escaped); i++) {
    unsigned char c = (unsigned char)name[i];
    if (c < 0x20) continue;
    if (c == '"' || c == '\\') escaped[j++] = '\\';
    escaped[j++] = (char)c;
  }
  escaped[j] = '\0';
  if (!escaped[0]) return snprintf(dst, dstLen, "{\"m\":%u}", mood);

  int wrote = snprintf(dst, dstLen, "{\"m\":%u,\"n\":\"%s\"}", mood, escaped);
  if (wrote < 0 || (size_t)wrote >= dstLen) return -1;
  return wrote;
}

inline int formatWinkPayload(char* dst, size_t dstLen, uint8_t hue) {
  if (!dst || !dstLen) return -1;
  int wrote = snprintf(dst, dstLen, "{\"h\":%u}", hue);
  if (wrote < 0 || (size_t)wrote >= dstLen) return -1;
  return wrote;
}
