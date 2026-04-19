#include "led_driver.h"
#include "Timer.h"

// // Ð¿Ð¾Ð´Ð¼Ð¸Ð³Ð½ÑƒÑ‚ÑŒ
// void wink() {
//   if (data.power) {
//     brightLoop(data.bright, 0, 20);
//     brightLoop(0, 255, 20);
//     brightLoop(255, 0, 20);
//     brightLoop(0, 255, 20);
//     brightLoop(255, 0, 20);
//     brightLoop(0, data.bright, 20);
//   }
// }

// // ÐºÐ¾ÑÑ‚Ñ‹Ð»ÑŒÐ½Ð¾, Ð½Ð¾ ÐºÑƒÐ´Ð° Ð´ÐµÐ²Ð°Ñ‚ÑŒÑÑ
// void brightLoop(int from, int to, int step) {
//   int val = from;
//   for (;;) {
//     FastLED.setBrightness(val);
//     FastLED.show();
//     delay(10);
//     if (from > to) {
//       val -= step;
//       if (val < to) return;
//     } else {
//       val += step;
//       if (val > to) return;
//     }
//   }
// }

// // Ð²Ñ‹Ð²Ð¾Ð´Ð¸Ð¼ ÑÑ„Ñ„ÐµÐºÑ‚ Ð½Ð° Ð»ÐµÐ½Ñ‚Ñƒ
// void animation() {
//   static Timer tmr(30);
//   static bool breath;   // Ð·Ð´ÐµÑÑŒ Ð¾Ñ‚Ð²ÐµÑ‡Ð°ÐµÑ‚ Ð·Ð° Ð¿Ð¾Ð³Ð°ÑˆÐµÐ½Ð¸Ðµ ÑÑ€ÐºÐ¾ÑÑ‚Ð¸ Ð´Ð»Ñ Ð´Ñ‹Ñ…Ð°Ð½Ð¸Ñ
//   static uint8_t count; // ÑÑ‡Ñ‘Ñ‚Ñ‡Ð¸Ðº-Ð¿Ñ€Ð¾Ð¿ÑƒÑÐº Ð¿ÐµÑ€Ð¸Ð¾Ð´Ð¾Ð²

//   if (tmr.period()) {
//     // Ð¿ÐµÑ€ÐµÐºÐ»ÑŽÑ‡Ð°ÐµÐ¼ Ð»Ð¾ÐºÐ°Ð»ÑŒÐ½ÑƒÑŽ ÑÑ€ÐºÐ¾ÑÑ‚ÑŒ Ð´Ð»Ñ "Ð´Ñ‹Ñ…Ð°Ð½Ð¸Ñ"
//     count++;
//     if (!onlineTmr.elapsed()) {   // ÑƒÐ´Ð°Ð»Ñ‘Ð½Ð½Ð°Ñ Ð»Ð°Ð¼Ð¿Ð° Ð¾Ð½Ð»Ð°Ð¹Ð½
//       if (!pirTmr.elapsed()) {    // ÑÑ€Ð°Ð±Ð¾Ñ‚Ð°Ð» Ð˜Ðš Ð½Ð° ÑƒÐ´Ð°Ð»Ñ‘Ð½Ð½Ð¾Ð¹
//         if (count % 10 == 0) breath = !breath;
//       } else {
//         if (count % 30 == 0) breath = !breath;
//       }
//     } else {
//       breath = 1;
//     }    
//     uint8_t curBr = data.power ? (breath ? 255 : 210) : 0;

//     // Ð·Ð´ÐµÑÑŒ Ð´ÐµÐ»Ð°ÐµÐ¼ Ð¿Ð»Ð°Ð²Ð½Ñ‹Ðµ Ð¿ÐµÑ€ÐµÑ…Ð¾Ð´Ñ‹ Ð¼ÐµÐ¶Ð´Ñƒ Ñ†Ð²ÐµÑ‚Ð°Ð¼Ð¸
//     CRGB ncol = CHSV(data.color, 255, curBr);
//     CRGB ccol = leds[0];
//     if (ccol != ncol) ccol = blend(ccol, ncol, 17);

//     // Ð²Ñ‹Ð²Ð¾Ð´Ð¸Ð¼ Ð½Ð° Ð»ÐµÐ½Ñ‚Ñƒ
//     fill_solid(leds, LED_AMOUNT, ccol);
//     FastLED.setBrightness(data.bright);
//     FastLED.show();
//   }
// }

// // Ð»Ð¾ÐºÐ°Ð»ÑŒÐ½Ñ‹Ð¹ Ð·Ð°Ð¿ÑƒÑÐº Ð¿Ð¾Ñ€Ñ‚Ð°Ð»Ð°. ÐŸÑ€Ð¸ Ð»ÑŽÐ±Ð¾Ð¼ Ð¸ÑÑ…Ð¾Ð´Ðµ Ð·Ð°ÐºÐ°Ð½Ñ‡Ð¸Ð²Ð°ÐµÑ‚ÑÑ Ñ€ÐµÑÐµÑ‚Ð¾Ð¼ Ð¿Ð»Ð°Ñ‚Ñ‹
// // void localPortal(IPAddress ip) {
// //   // ÑÐ¾Ð·Ð´Ð°Ñ‘Ð¼ Ñ‚Ð¾Ñ‡ÐºÑƒ Ñ Ð¸Ð¼ÐµÐ½ÐµÐ¼ WLamp Ð¸ Ð¿Ñ€ÐµÐ´Ñ‹Ð´ÑƒÑ‰Ð¸Ð¼ ÑƒÑÐ¿ÐµÑˆÐ½Ñ‹Ð¼ IP
// //   Serial.println(F("Create AP"));
// //   WiFi.mode(WIFI_AP);
// //   String s(F("WLamp "));
// //   s += ip.toString();
// //   WiFi.softAP(s);

// //   portal.start("WLamp");    // Ð·Ð°Ð¿ÑƒÑÐºÐ°ÐµÐ¼ Ð¿Ð¾Ñ€Ñ‚Ð°Ð»
// //   while (portal.tick()) {   // Ð¿Ð¾Ñ€Ñ‚Ð°Ð» Ñ€Ð°Ð±Ð¾Ñ‚Ð°ÐµÑ‚
// //     loadAnimation(CRGB::Blue);      // Ð¼Ð¸Ð³Ð°ÐµÐ¼ ÑÐ¸Ð½Ð¸Ð¼
// //     btn.tick();
// //     // ÐµÑÐ»Ð¸ Ð½Ð°Ð¶Ð°Ð»Ð¸ ÑÐ¾Ñ…Ñ€Ð°Ð½Ð¸Ñ‚ÑŒ Ð½Ð°ÑÑ‚Ñ€Ð¾Ð¹ÐºÐ¸ Ð¸Ð»Ð¸ ÐºÐ»Ð¸ÐºÐ½ÑƒÐ»Ð¸ Ð¿Ð¾ ÐºÐ½Ð¾Ð¿ÐºÐµ
// //     // Ð¿ÐµÑ€ÐµÐ·Ð°Ð³Ñ€ÑƒÐ¶Ð°ÐµÐ¼ ESP
// //     if (checkPortal() || btn.click()) ESP.reset();
// //   }
// // }

extern void showLedsSafe();

// Ð°Ð½Ð¸Ð¼Ð°Ñ†Ð¸Ñ Ñ€Ð°Ð±Ð¾Ñ‚Ñ‹ Ð»Ð¾ÐºÐ°Ð» Ð¿Ð¾Ñ€Ñ‚Ð°Ð»Ð°
void loadAnimation(CRGB color) {
  static int16_t val = 0;
  static int8_t dir = 1;
  static Timer tmr(20);
  if (tmr.period()) {
    val += dir;
    if (val >= 100) {
      val = 100;
      dir = -1;
    } else if (val <= 0) {
      val = 0;
      dir = 1;
    }
    fill_solid(leds, LED_AMOUNT, color);
    FastLED.setBrightness((uint8_t)val);
    showLedsSafe();
  }
  yield();
}

// // Ñ†Ñ‹Ð³Ð°Ð½ÑÐºÐ¸Ð¹ Ð¿Ð°Ñ€ÑÐµÑ€ Ð¸Ð½Ñ‚Ð° Ð¸Ð· ÑƒÐºÐ°Ð·Ð°Ð½Ð½Ð¾Ð³Ð¾ Ð¸Ð½Ð´ÐµÐºÑÐ° Ð² ÑÑ‚Ñ€Ð¾ÐºÐµ
// int getFromIndex(char* str, int idx, char div = ',') {
//   int val = 0;
//   uint16_t i = 0;
//   int count = 0;
//   bool sign = 0;
//   while (str[i]) {
//     if (idx == count) {
//       if (str[i] == div) break;
//       if (str[i] == '-') sign = -1;
//       else {
//         val *= 10L;
//         val += str[i] - '0';
//       }
//     } else if (str[i] == div) count++;
//     i++;
//   }
//   return sign ? -val : val;
// }
