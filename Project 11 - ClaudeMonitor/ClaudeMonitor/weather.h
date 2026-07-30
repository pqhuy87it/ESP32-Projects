#pragma once
#include "config.h"
#ifdef MANGO_UI
#include <stdint.h>

#define FORECAST_SLOTS 6        // số cột dự báo (bước 3 tiếng)

struct ForecastSlot {
    int    hour;                // giờ trong ngày 0..23 (đã +múi giờ)
    int    temp;               // nhiệt độ làm tròn
    char   iconName[16];       // tên icon nhỏ (khớp tên file bộ 28x28)
    bool   valid;
};

struct WeatherData {
    // Hiện tại
    float  temp;               // nhiệt độ (đơn vị theo OWM_UNITS)
    float  feelsLike;
    int    humidity;           // %
    int    pop;                // tỉ lệ mưa % (probability of precipitation)
    char   iconName[16];       // icon lớn hiện tại (khớp tên file bộ 50x50)
    char   desc[32];           // mô tả ngắn (vd "clear sky")
    char   city[32];
    // Dự báo các mốc kế tiếp
    ForecastSlot slots[FORECAST_SLOTS];
    bool   ok;                 // true khi có dữ liệu hợp lệ ít nhất 1 lần
    char   error[48];
};

bool fetchWeather(WeatherData& out);

#endif // MANGO_UI
