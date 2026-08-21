#include "demo.h"
#include "config.h"

void demoInit(AppData& d) {
  strlcpy(d.city,      "Hanoi",  sizeof d.city);
  strlcpy(d.region,    "VN",     sizeof d.region);
  strlcpy(d.condition, "Clouds", sizeof d.condition);
  d.temp          = 31;
  d.icon          = WxIcon::PartlyCloudy;
  d.humidityOut   = 74;
  d.windSpeed     = 3.4f;
  d.humidityIn    = 56;
  d.hasHumidityIn = true;

  d.hour24    = 13;
  d.second    = 0;
  d.hourShown = 11;
  d.minute    = 16;
  d.isPM      = true;
  d.show12h   = (USE_12H_CLOCK != 0);
  d.day       = 22;
  d.month     = 8;
  d.year2     = 26;
  d.weekday   = 6;        // Saturday
  d.online    = true;
  d.timeValid = true;

  const WxIcon fc[FORECAST_SLOTS] = { WxIcon::Clear, WxIcon::PartlyCloudy, WxIcon::Rain };
  const uint8_t hh[FORECAST_SLOTS] = { 9, 1, 8 };
  const bool    pm[FORECAST_SLOTS] = { false, true, true };
  for (uint8_t i = 0; i < FORECAST_SLOTS; i++) {
    d.forecast[i].hour12 = hh[i];
    d.forecast[i].isPM   = pm[i];
    d.forecast[i].icon   = fc[i];
    d.forecast[i].temp   = 28 + i;
  }
  d.hasForecast = true;

  const char*   dw[DAILY_SLOTS]  = { "Fri", "Sat", "Sun" };
  const WxIcon  di[DAILY_SLOTS]  = { WxIcon::Clear, WxIcon::Rain, WxIcon::Cloudy };
  const int     dd[DAILY_SLOTS]  = { 17, 15, 14 };
  const int     dn[DAILY_SLOTS]  = { 11,  9,  9 };
  const int     dx[DAILY_SLOTS]  = { 23, 22, 20 };
  const uint8_t dh[DAILY_SLOTS]  = { 23, 45, 42 };
  for (uint8_t i = 0; i < DAILY_SLOTS; i++) {
    strlcpy(d.daily[i].wday, dw[i], sizeof d.daily[i].wday);
    d.daily[i].icon     = di[i];
    d.daily[i].tempDay  = dd[i];
    d.daily[i].tempMin  = dn[i];
    d.daily[i].tempMax  = dx[i];
    d.daily[i].humidity = dh[i];
    d.daily[i].valid    = true;
  }
  d.hasDaily = true;
}

void demoTick(AppData& d) {
  static uint32_t lastMinute = 0;
  static uint32_t lastIcon   = 0;
  static uint32_t lastDay    = 0;
  const  uint32_t now        = millis();

  // Giay chay that de xem kim giay quay tren man Analog
  d.second = (uint8_t)((now / 1000) % 60);

  if (now - lastMinute >= 2000) {            // 1 "phut" moi 2 giay
    lastMinute = now;
    d.hour24 = (uint8_t)((d.hour24 + ((d.minute + 1 >= 60) ? 1 : 0)) % 24);
    if (++d.minute >= 60) {
      d.minute = 0;
      if (d.show12h) {
        if (++d.hourShown > 12) { d.hourShown = 1; d.isPM = !d.isPM; }
      } else {
        if (++d.hourShown > 23) d.hourShown = 0;
      }
    }
  }

  if (now - lastIcon >= 4000) {
    lastIcon = now;
    static const WxIcon CYCLE[8] = {
      WxIcon::Clear, WxIcon::PartlyCloudy, WxIcon::Cloudy, WxIcon::Rain,
      WxIcon::Storm, WxIcon::Wind, WxIcon::Fog, WxIcon::NightClear
    };
    static const char* const NAMES[8] = {
      "Clear", "Partly Cloud", "Clouds", "Rain",
      "Storm", "Squall", "Fog", "Clear Night"
    };
    static uint8_t i = 0;
    i = (uint8_t)((i + 1) % 8);
    d.icon = CYCLE[i];
    strlcpy(d.condition, NAMES[i], sizeof d.condition);
    d.temp = 24 + (int)i;
    // Xoay ca du bao de kiem tra viec ve lai cot ben phai
    for (uint8_t k = 0; k < FORECAST_SLOTS; k++) {
      d.forecast[k].icon = CYCLE[(i + k * 2) % 8];
    }
    // Doi ca mau the du bao theo ngay de xem het cac to mau
    for (uint8_t k = 0; k < DAILY_SLOTS; k++) {
      d.daily[k].icon = CYCLE[(i + k * 3) % 8];
    }
  }

  if (now - lastDay >= 6000) {               // doi ngay/thu de kiem tra man lich
    lastDay = now;
    d.day     = (uint8_t)(d.day % 28 + 1);
    d.weekday = (uint8_t)((d.weekday + 1) % 7);
  }
}
