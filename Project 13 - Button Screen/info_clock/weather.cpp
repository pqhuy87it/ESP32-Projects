#include "weather.h"
#include "config.h"
#include "net.h"
#include "settings.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Doi trang thai cua OpenWeatherMap sang enum noi bo.
//
// LUU Y QUAN TRONG: truong "icon" khong du de phan biet moi thu.
// Ca nhom 7xx (mist, haze, fog, dust, SQUALL, TORNADO) deu tra ve icon "50d".
// Muon nhan ra gio giat va loc xoay thi phai doc truong "main".
//
// Ngoai ra API KHONG co trang thai "windy" — gio chi la so lieu wind.speed.
// Nen ta tu quy uoc: troi quang hoac nhieu may ma gio vuot nguong thi hien
// icon gio. Mua, dong va suong mu luon duoc uu tien vi quan trong hon.
static WxIcon iconFromCode(const char* code) {
  if (!code || strlen(code) < 3) return WxIcon::Cloudy;
  const bool night = (code[2] == 'n');

  if (!strncmp(code, "01", 2)) return night ? WxIcon::NightClear : WxIcon::Clear;
  if (!strncmp(code, "02", 2) ||
      !strncmp(code, "03", 2)) return WxIcon::PartlyCloudy;
  if (!strncmp(code, "04", 2)) return WxIcon::Cloudy;
  if (!strncmp(code, "09", 2) ||
      !strncmp(code, "10", 2)) return WxIcon::Rain;
  if (!strncmp(code, "11", 2)) return WxIcon::Storm;
  if (!strncmp(code, "13", 2)) return WxIcon::Snow;
  if (!strncmp(code, "50", 2)) return WxIcon::Fog;
  return WxIcon::Cloudy;
}

static WxIcon mapOwmIcon(const char* main, const char* code, float windMs) {
  // Squall (771) va Tornado (781) deu la hien tuong gio manh, va deu bi API
  // gan icon "50d" giong suong mu. Bat qua "main" truoc.
  if (main && (!strcmp(main, "Squall") || !strcmp(main, "Tornado"))) {
    return WxIcon::Wind;
  }

  const WxIcon base = iconFromCode(code);

  // Dung `if` THUONG chu khong phai `#if`: bo tien xu ly chi tinh duoc bieu
  // thuc so nguyen, gap hang so thuc nhu 8.0f la bao loi
  // "floating constant in preprocessor expression".
  // Dat nguong = 0 thi dieu kien thanh hang so sai, trinh bien dich tu loai bo
  // ca nhanh nay — khong ton mot byte flash nao.
  if (WIND_ICON_THRESHOLD_MS > 0.0f) {
    const bool calmSky = (base == WxIcon::Clear || base == WxIcon::NightClear ||
                          base == WxIcon::PartlyCloudy || base == WxIcon::Cloudy);
    if (calmSky && windMs >= WIND_ICON_THRESHOLD_MS) return WxIcon::Wind;
  }

  return base;
}

bool weatherFetch(AppData& d) {
  if (!netOnline()) {
    d.online = false;
    return false;
  }
  d.online = true;

  if (strlen(S.apiKey) < 8) {
    Serial.println(F("[weather] chua co API key — giu nut 6 giay de mo portal"));
    return false;
  }

  char url[256];
  snprintf(url, sizeof url,
           "http://api.openweathermap.org/data/2.5/weather"
           "?lat=%.4f&lon=%.4f&units=%s&appid=%s",
           S.lat, S.lon, OWM_UNITS, S.apiKey);

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  const int status = http.GET();

  if (status != HTTP_CODE_OK) {
    Serial.printf("[weather] HTTP %d — kiem tra API key va toa do\n", status);
    http.end();
    return false;
  }

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(2048);
#endif
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) {
    Serial.printf("[weather] JSON: %s\n", err.c_str());
    return false;
  }

  strlcpy(d.city,   doc["name"] | CITY_FALLBACK, sizeof d.city);
  strlcpy(d.region, doc["sys"]["country"] | "",  sizeof d.region);
  d.temp        = (int)lroundf(doc["main"]["temp"] | 0.0f);
  d.humidityOut = doc["main"]["humidity"] | 0;
  d.windSpeed   = doc["wind"]["speed"] | 0.0f;

  strlcpy(d.condition, doc["weather"][0]["main"] | "--", sizeof d.condition);
  d.icon = mapOwmIcon(d.condition, doc["weather"][0]["icon"] | "04d", d.windSpeed);

  Serial.printf("[weather] %s %d%c %s  do am %u%%\n",
                d.city, d.temp, TEMP_UNIT, d.condition, d.humidityOut);
  return true;
}

// ============================================================================
// Du bao — MOT lan goi API sinh ra ca hai dang du lieu:
//   d.forecast[]  theo gio, cho man Weather
//   d.daily[]     theo ngay, cho man Forecast
//
// Ban mien phi cua OpenWeatherMap chi co /forecast voi buoc 3 gio. Min/max
// theo ngay phai tu gop lai. Response kha lon so voi RAM cua C3 nen dung
// DeserializationOption::Filter de chi giu dung nhung truong can thiet.
// ============================================================================

// Khoa dinh danh mot ngay, an toan khi vat qua nam moi
static inline uint32_t dayKey(const struct tm& t) {
  return (uint32_t)(t.tm_year) * 512u + (uint32_t)t.tm_yday;
}

static void fillHourly(AppData& d, JsonArray list) {
  const uint8_t picks[FORECAST_SLOTS] =
      { FORECAST_PICK_1, FORECAST_PICK_2, FORECAST_PICK_3 };

  for (uint8_t i = 0; i < FORECAST_SLOTS; i++) {
    const uint8_t idx = (picks[i] < list.size()) ? picks[i]
                                                : (uint8_t)(list.size() - 1);
    JsonObject e = list[idx];

    // dt la unix UTC; localtime_r ap dung mui gio da dat bang configTzTime()
    const time_t ts = (time_t)(e["dt"] | 0);
    struct tm lt;
    localtime_r(&ts, &lt);

    d.forecast[i].isPM   = (lt.tm_hour >= 12);
    d.forecast[i].hour12 = (uint8_t)(lt.tm_hour % 12);
    if (d.forecast[i].hour12 == 0) d.forecast[i].hour12 = 12;

    d.forecast[i].temp = (int)lroundf(e["main"]["temp"] | 0.0f);
    d.forecast[i].icon = mapOwmIcon(e["weather"][0]["main"] | "",
                                    e["weather"][0]["icon"] | "04d",
                                    e["wind"]["speed"] | 0.0f);
  }
  d.hasForecast = true;
}

static void fillDaily(AppData& d, JsonArray list) {
  static const char* const WDAY[7] =
      { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

  // Ngay hom nay, de bo qua va chi lay cac ngay ke tiep
  time_t nowTs = time(nullptr);
  struct tm nowTm;
  localtime_r(&nowTs, &nowTm);
  const uint32_t todayKey = dayKey(nowTm);

  uint32_t keys[DAILY_SLOTS] = {0};
  int      bestHourGap[DAILY_SLOTS];
  uint8_t  found = 0;

  for (uint8_t i = 0; i < DAILY_SLOTS; i++) {
    d.daily[i].valid   = false;
    d.daily[i].tempMin =  999;
    d.daily[i].tempMax = -999;
    bestHourGap[i]     = 99;
  }

  for (JsonObject e : list) {
    const time_t ts = (time_t)(e["dt"] | 0);
    if (ts == 0) continue;

    struct tm lt;
    localtime_r(&ts, &lt);
    const uint32_t k = dayKey(lt);
    if (k <= todayKey) continue;              // bo qua hom nay va qua khu

    // Tim khe da cap cho ngay nay, hoac cap khe moi
    int slot = -1;
    for (uint8_t i = 0; i < found; i++) if (keys[i] == k) { slot = i; break; }
    if (slot < 0) {
      if (found >= DAILY_SLOTS) continue;     // du 3 ngay thi dung
      slot      = found++;
      keys[slot] = k;
      strlcpy(d.daily[slot].wday, WDAY[lt.tm_wday % 7], sizeof d.daily[slot].wday);
      d.daily[slot].valid = true;
    }

    DailyForecast& day = d.daily[slot];

    const int tMin = (int)lroundf(e["main"]["temp_min"] | 99.0f);
    const int tMax = (int)lroundf(e["main"]["temp_max"] | -99.0f);
    if (tMin < day.tempMin) day.tempMin = tMin;
    if (tMax > day.tempMax) day.tempMax = tMax;

    // Moc gan 12 gio trua nhat dai dien cho ca ngay: icon, do am, nhiet do
    const int gap = abs(lt.tm_hour - 12);
    if (gap < bestHourGap[slot]) {
      bestHourGap[slot] = gap;
      day.tempDay  = (int)lroundf(e["main"]["temp"] | 0.0f);
      day.humidity = e["main"]["humidity"] | 0;
      day.icon     = mapOwmIcon(e["weather"][0]["main"] | "",
                                e["weather"][0]["icon"] | "04d",
                                e["wind"]["speed"] | 0.0f);
    }
  }

  d.hasDaily = (found > 0);
}

bool weatherFetchForecast(AppData& d) {
  if (!netOnline() || strlen(S.apiKey) < 8) return false;

  char url[280];
  snprintf(url, sizeof url,
           "http://api.openweathermap.org/data/2.5/forecast"
           "?lat=%.4f&lon=%.4f&units=%s&cnt=%u&appid=%s",
           S.lat, S.lon, OWM_UNITS, FORECAST_FETCH_CNT, S.apiKey);

  HTTPClient http;
  http.setTimeout(12000);
  http.begin(url);
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[forecast] HTTP %d\n", status);
    http.end();
    return false;
  }

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument filter;
  JsonDocument doc;
#else
  StaticJsonDocument<256>  filter;
  DynamicJsonDocument      doc(8192);
#endif
  filter["list"][0]["dt"]                  = true;
  filter["list"][0]["main"]["temp"]        = true;
  filter["list"][0]["main"]["temp_min"]    = true;
  filter["list"][0]["main"]["temp_max"]    = true;
  filter["list"][0]["main"]["humidity"]    = true;
  filter["list"][0]["weather"][0]["main"]  = true;
  filter["list"][0]["weather"][0]["icon"]  = true;
  filter["list"][0]["wind"]["speed"]       = true;

  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("[forecast] JSON: %s\n", err.c_str());
    return false;
  }

  JsonArray list = doc["list"].as<JsonArray>();
  if (list.isNull() || list.size() == 0) return false;

  fillHourly(d, list);
  fillDaily(d, list);

  Serial.printf("[forecast] theo gio: %u%s %u%s %u%s\n",
                d.forecast[0].hour12, d.forecast[0].isPM ? "PM" : "AM",
                d.forecast[1].hour12, d.forecast[1].isPM ? "PM" : "AM",
                d.forecast[2].hour12, d.forecast[2].isPM ? "PM" : "AM");
  for (uint8_t i = 0; i < DAILY_SLOTS; i++) {
    if (!d.daily[i].valid) continue;
    Serial.printf("[forecast] %s  %d%c  min %d  max %d  am %u%%\n",
                  d.daily[i].wday, d.daily[i].tempDay, TEMP_UNIT,
                  d.daily[i].tempMin, d.daily[i].tempMax, d.daily[i].humidity);
  }
  return true;
}
