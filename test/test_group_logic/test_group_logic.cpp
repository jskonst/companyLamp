#include <unity.h>

#include "group_logic.h"

void setUp(void) {}
void tearDown(void) {}

void test_hue_scale_red_yellow_green(void) {
  TEST_ASSERT_EQUAL_UINT8(0, moodToHue(0));
  TEST_ASSERT_EQUAL_UINT8(32, moodToHue(25));
  TEST_ASSERT_EQUAL_UINT8(64, moodToHue(50));
  TEST_ASSERT_EQUAL_UINT8(80, moodToHue(75));
  TEST_ASSERT_EQUAL_UINT8(96, moodToHue(100));
  TEST_ASSERT_EQUAL_UINT8(62, moodToHue(49));
  TEST_ASSERT_EQUAL_UINT8(64, moodToHue(51));
}

void test_hue_stays_on_red_green_scale(void) {
  uint8_t prev = 0;
  for (int mood = 0; mood <= 100; mood++) {
    uint8_t hue = moodToHue((uint8_t)mood);
    TEST_ASSERT_TRUE(hue >= prev);
    TEST_ASSERT_TRUE(hue <= 96);
    prev = hue;
  }
}

void test_mood_steps_wrap(void) {
  TEST_ASSERT_EQUAL_UINT8(25, nextMoodStep(0));
  TEST_ASSERT_EQUAL_UINT8(25, nextMoodStep(10));
  TEST_ASSERT_EQUAL_UINT8(50, nextMoodStep(25));
  TEST_ASSERT_EQUAL_UINT8(75, nextMoodStep(50));
  TEST_ASSERT_EQUAL_UINT8(100, nextMoodStep(75));
  TEST_ASSERT_EQUAL_UINT8(100, nextMoodStep(99));
  TEST_ASSERT_EQUAL_UINT8(0, nextMoodStep(100));
}

void test_red_and_green_average_is_yellow(void) {
  MoodSample samples[2] = {{0, true}, {100, true}};
  uint8_t avg = 0;
  TEST_ASSERT_TRUE(averageMood(samples, 2, 0, false, &avg));
  TEST_ASSERT_EQUAL_UINT8(50, avg);
  TEST_ASSERT_EQUAL_UINT8(64, moodToHue(avg));
}

void test_average_includes_self_and_skips_offline(void) {
  MoodSample samples[3] = {{0, false}, {100, true}, {0, true}};
  uint8_t avg = 0;
  // online: 100 и 0, плюс своя 0 → 100/3 → 33
  TEST_ASSERT_TRUE(averageMood(samples, 3, 0, true, &avg));
  TEST_ASSERT_EQUAL_UINT8(33, avg);

  // только своя, чужие offline
  MoodSample gone[1] = {{80, false}};
  TEST_ASSERT_TRUE(averageMood(gone, 1, 40, true, &avg));
  TEST_ASSERT_EQUAL_UINT8(40, avg);

  TEST_ASSERT_FALSE(averageMood(gone, 1, 40, false, &avg));
}

void test_average_rounds_half_up(void) {
  MoodSample samples[2] = {{0, true}, {1, true}};
  uint8_t avg = 0;
  TEST_ASSERT_TRUE(averageMood(samples, 2, 0, false, &avg));
  TEST_ASSERT_EQUAL_UINT8(1, avg);
}

void test_state_payload_roundtrip(void) {
  char buf[96];
  TEST_ASSERT_EQUAL_INT(23, formatStatePayload(buf, sizeof(buf), 75, "Женя"));
  TEST_ASSERT_EQUAL_STRING("{\"m\":75,\"n\":\"Женя\"}", buf);

  uint8_t mood = 0;
  char name[32];
  TEST_ASSERT_TRUE(parseStatePayload(buf, &mood, name, sizeof(name)));
  TEST_ASSERT_EQUAL_UINT8(75, mood);
  TEST_ASSERT_EQUAL_STRING("Женя", name);

  TEST_ASSERT_GREATER_THAN_INT(0, formatStatePayload(buf, sizeof(buf), 50, ""));
  TEST_ASSERT_EQUAL_STRING("{\"m\":50}", buf);
  name[0] = 'x';
  TEST_ASSERT_TRUE(parseStatePayload(buf, &mood, name, sizeof(name)));
  TEST_ASSERT_EQUAL_UINT8(50, mood);
  TEST_ASSERT_EQUAL_STRING("", name);
}

void test_state_payload_accepts_spaces_and_clamps(void) {
  uint8_t mood = 0;
  char name[32];
  TEST_ASSERT_TRUE(parseStatePayload("{\"n\": \"Ann\", \"m\": 150}", &mood, name, sizeof(name)));
  TEST_ASSERT_EQUAL_UINT8(100, mood);
  TEST_ASSERT_EQUAL_STRING("Ann", name);
  TEST_ASSERT_TRUE(parseStatePayload("{\"m\": -3, \"n\": \"a\\\"b\"}", &mood, name, sizeof(name)));
  TEST_ASSERT_EQUAL_UINT8(0, mood);
  TEST_ASSERT_EQUAL_STRING("a\"b", name);
  TEST_ASSERT_FALSE(parseStatePayload("{\"n\":\"без настроения\"}", &mood, name, sizeof(name)));
}

void test_wink_payload(void) {
  char buf[16];
  TEST_ASSERT_EQUAL_INT(8, formatWinkPayload(buf, sizeof(buf), 96));
  TEST_ASSERT_EQUAL_STRING("{\"h\":96}", buf);
  uint8_t hue = 0;
  TEST_ASSERT_TRUE(parseWinkPayload(buf, &hue));
  TEST_ASSERT_EQUAL_UINT8(96, hue);
  TEST_ASSERT_TRUE(parseWinkPayload("{ \"h\" : 0 }", &hue));
  TEST_ASSERT_EQUAL_UINT8(0, hue);
  TEST_ASSERT_FALSE(parseWinkPayload("{\"h\":256}", &hue));
  TEST_ASSERT_FALSE(parseWinkPayload("{\"h\":-1}", &hue));
  TEST_ASSERT_FALSE(parseWinkPayload("{}", &hue));
}

void test_status_payload(void) {
  TEST_ASSERT_TRUE(parseStatusPayload("online") == PeerStatus::Online);
  TEST_ASSERT_TRUE(parseStatusPayload("  away\n") == PeerStatus::Away);
  TEST_ASSERT_TRUE(parseStatusPayload("offline") == PeerStatus::Offline);
  TEST_ASSERT_TRUE(parseStatusPayload("busy") == PeerStatus::None);
  TEST_ASSERT_TRUE(parseStatusPayload(nullptr) == PeerStatus::None);
}

void test_topics_and_ids(void) {
  char id[7];
  formatChipId(0xA1B2C3u, id);
  TEST_ASSERT_EQUAL_STRING("a1b2c3", id);
  formatChipId(1u, id);
  TEST_ASSERT_EQUAL_STRING("000001", id);
  TEST_ASSERT_TRUE(sameLampId("A1B2C3", "a1b2c3"));
  TEST_ASSERT_FALSE(sameLampId("a1b2c3", "a1b2c4"));
  TEST_ASSERT_FALSE(sameLampId("a1b2c3", "a1b2c30"));

  char topic[48];
  char client[16];
  TEST_ASSERT_GREATER_THAN_INT(0, buildClientId(client, sizeof(client), "a1b2c3"));
  TEST_ASSERT_EQUAL_STRING("lamp-a1b2c3", client);
  TEST_ASSERT_GREATER_THAN_INT(0, buildLampTopic(topic, sizeof(topic), "team", "a1b2c3", "state"));
  TEST_ASSERT_EQUAL_STRING("lamp/team/a1b2c3/state", topic);

  TopicParts parts;
  TEST_ASSERT_TRUE(parseLampTopic("lamp/team-1/A1B2C3/status", &parts));
  TEST_ASSERT_EQUAL_STRING("team-1", parts.group);
  TEST_ASSERT_EQUAL_STRING("a1b2c3", parts.id);
  TEST_ASSERT_EQUAL_STRING("status", parts.leaf);

  TEST_ASSERT_TRUE(parseLampTopic("lamp/team/a1b2c3/wink", &parts));
  TEST_ASSERT_EQUAL_STRING("wink", parts.leaf);
  TEST_ASSERT_FALSE(parseLampTopic("lamp/team/a1b2c3/wink/extra", &parts));
  TEST_ASSERT_FALSE(parseLampTopic("lamp/team/abc/state", &parts));
  TEST_ASSERT_FALSE(parseLampTopic("lamp/команда/a1b2c3/state", &parts));
  TEST_ASSERT_FALSE(parseLampTopic("other/team/a1b2c3/state", &parts));
  TEST_ASSERT_FALSE(parseLampTopic("lamp/team/a1b2c3/nope", &parts));

  char filter[40];
  TEST_ASSERT_GREATER_THAN_INT(0, buildGroupFilter(filter, sizeof(filter), "team", "state"));
  TEST_ASSERT_EQUAL_STRING("lamp/team/+/state", filter);
}

void test_group_name(void) {
  TEST_ASSERT_TRUE(validGroupName("team"));
  TEST_ASSERT_TRUE(validGroupName("Team-1"));
  TEST_ASSERT_FALSE(validGroupName(""));
  TEST_ASSERT_FALSE(validGroupName("a b"));
  TEST_ASSERT_FALSE(validGroupName("группа"));
  TEST_ASSERT_FALSE(validGroupName("123456789012345678901"));
  TEST_ASSERT_TRUE(validGroupName("12345678901234567890"));
}

void test_gestures_match_protocol(void) {
  TEST_ASSERT_EQUAL_UINT8(1, GESTURE_WINK);
  TEST_ASSERT_EQUAL_UINT8(2, GESTURE_MOOD);
  TEST_ASSERT_EQUAL_UINT8(3, GESTURE_POWER);
  TEST_ASSERT_EQUAL_UINT8(4, GESTURE_AVERAGE);
  TEST_ASSERT_EQUAL_UINT8(5, GESTURE_AUTONOMOUS);
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_hue_scale_red_yellow_green);
  RUN_TEST(test_hue_stays_on_red_green_scale);
  RUN_TEST(test_mood_steps_wrap);
  RUN_TEST(test_red_and_green_average_is_yellow);
  RUN_TEST(test_average_includes_self_and_skips_offline);
  RUN_TEST(test_average_rounds_half_up);
  RUN_TEST(test_state_payload_roundtrip);
  RUN_TEST(test_state_payload_accepts_spaces_and_clamps);
  RUN_TEST(test_wink_payload);
  RUN_TEST(test_status_payload);
  RUN_TEST(test_topics_and_ids);
  RUN_TEST(test_group_name);
  RUN_TEST(test_gestures_match_protocol);
  return UNITY_END();
}
