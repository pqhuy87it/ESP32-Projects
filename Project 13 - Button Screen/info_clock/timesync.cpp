#include "timesync.h"
#include "config.h"
#include <time.h>

void timeBegin() {
  // configTzTime nhan chuoi mui gio POSIX — xu ly gio mua he dung hon
  // so voi configTime() truyen offset bang so giay.
  configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
}

bool timeWaitValid(uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  struct tm t;
  while (millis() < deadline) {
    if (getLocalTime(&t, 200)) return true;
    delay(200);
  }
  return false;
}

void timeSync(AppData& d) {
  struct tm now;
  if (!getLocalTime(&now, 50)) return;      // chua co gio, giu nguyen gia tri cu

  d.timeValid = true;
  d.show12h   = (USE_12H_CLOCK != 0);

  const int h24 = now.tm_hour;
  if (d.show12h) {
    d.isPM      = (h24 >= 12);
    d.hourShown = h24 % 12;
    if (d.hourShown == 0) d.hourShown = 12;
  } else {
    d.isPM      = false;
    d.hourShown = (uint8_t)h24;
  }

  d.hour24  = (uint8_t)h24;
  d.minute  = (uint8_t)now.tm_min;
  d.second  = (uint8_t)now.tm_sec;
  d.day     = (uint8_t)now.tm_mday;
  d.month   = (uint8_t)(now.tm_mon + 1);
  d.year2   = (uint8_t)(now.tm_year % 100);
  d.weekday = (uint8_t)now.tm_wday;         // 0 = Sunday
}
