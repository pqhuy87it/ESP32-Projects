#pragma once
#include <Arduino.h>

// ============================================================================
// app_data.h — mo hinh du lieu dung chung cho moi man hinh.
// Man hinh CHI doc struct nay, khong tu goi API. Nho vay them man hinh moi
// khong can biet gi ve WiFi hay JSON.
// ============================================================================

enum class WxIcon : uint8_t {
  Clear, NightClear, PartlyCloudy, Cloudy, Rain, Storm, Snow, Fog, Wind
};

// Mot moc du bao theo gio
struct ForecastSlot {
  uint8_t hour12;      // 1..12
  bool    isPM;
  WxIcon  icon;
  int     temp;
};

static constexpr uint8_t FORECAST_SLOTS = 3;

// Mot ngay du bao. Ban mien phi cua OpenWeatherMap khong co endpoint theo
// ngay, nen cac gia tri nay duoc gop tu cac buoc 3 gio cua /forecast.
struct DailyForecast {
  char    wday[4];      // "Fri"
  WxIcon  icon;         // lay tu moc gan giua ngay nhat
  int     tempDay;      // nhiet do dai dien (moc gan giua ngay)
  int     tempMin;      // thap nhat trong ngay
  int     tempMax;      // cao nhat trong ngay
  uint8_t humidity;
  bool    valid;
};

static constexpr uint8_t DAILY_SLOTS = 3;

struct AppData {
  // --- Thoi tiet ---
  char    city[26];
  char    region[5];        // ma quoc gia: "VN", "US"...
  int     temp;
  char    condition[18];    // "Clouds", "Rain"...
  WxIcon  icon;
  uint8_t humidityOut;
  float   windSpeed;        // m/s, dung de quyet dinh co hien icon gio khong
  ForecastSlot  forecast[FORECAST_SLOTS];
  bool          hasForecast;
  DailyForecast daily[DAILY_SLOTS];
  bool          hasDaily;

  // --- Cam bien trong nha (tuy chon) ---
  uint8_t humidityIn;
  bool    hasHumidityIn;

  // --- Thoi gian ---
  uint8_t hourShown;        // 1..12 khi 12h, 0..23 khi 24h
  uint8_t hour24;           // 0..23, man dong ho kim can gia tri that
  uint8_t minute;
  uint8_t second;           // chi man analog dung; cac man khac bo qua
  bool    isPM;
  bool    show12h;
  uint8_t day, month, year2;
  uint8_t weekday;          // 0 = Sunday, theo tm_wday

  // --- Trang thai he thong ---
  bool    online;
  bool    timeValid;
};

// Dat gia tri mac dinh an toan truoc khi co du lieu that
void appDataInit(AppData& d);
