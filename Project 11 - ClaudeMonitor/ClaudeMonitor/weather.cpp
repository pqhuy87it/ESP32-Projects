#include "config.h"     // phải đặt TRƯỚC #ifdef để MANGO_UI được định nghĩa
#ifdef MANGO_UI

#include "weather.h"
#include "certs.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ── Map thời tiết → tên file .bmp (dùng chung cho cả bộ 50x50 và 28x28) ──
// Bộ icon của bạn:
//   clear-day, clear-night, cloudy, drizzle, extreme-rain, fog, light-rain,
//   n_a, partly-cloudy-day, partly-cloudy-night, rain, sleet, snow,
//   thunderstorm, unknown
//
// OWM phân biệt mức độ chi tiết qua weather[0].id (mã 3 chữ số), còn trường
// weather[0].icon chỉ cho biết ngày/đêm (hậu tố d/n). Ta map theo id để tận
// dụng light-rain/extreme-rain/sleet, và ghép d/n cho clear & mây-ít.
//   2xx thunderstorm | 3xx drizzle | 5xx rain (theo cấp) | 6xx snow/sleet
//   7xx fog | 800 clear | 801-802 partly-cloudy | 803-804 cloudy
static const char* mapIcon(int id, const char* icon) {
    bool night = (icon[0] && icon[strlen(icon) - 1] == 'n');

    if (id >= 200 && id < 300) return "thunderstorm";
    if (id >= 300 && id < 400) return "drizzle";
    if (id >= 500 && id < 600) {
        if (id == 500)               return "light-rain";
        if (id == 511)               return "sleet";         // freezing rain
        if (id >= 502 && id <= 531)  return "extreme-rain";  // heavy/shower
        return "rain";                                       // 501 moderate + còn lại
    }
    if (id >= 600 && id < 700) {
        if (id >= 611 && id <= 616)  return "sleet";
        return "snow";
    }
    if (id >= 700 && id < 800) return "fog";                 // mist/haze/fog...
    if (id == 800)             return night ? "clear-night" : "clear-day";
    if (id == 801 || id == 802)
        return night ? "partly-cloudy-night" : "partly-cloudy-day";
    if (id >= 803 && id <= 804) return "cloudy";
    return "unknown";
}

// ── Trích string "key":"value" trong đoạn con [from, to) ────────────────
static bool jsonStrIn(const String& body, int from, int to,
                      const char* key, char* out, size_t outLen) {
    String pat = String("\"") + key + "\":\"";
    int i = body.indexOf(pat, from);
    if (i < 0 || i >= to) return false;
    i += pat.length();
    int j = body.indexOf('"', i);
    if (j < 0 || j > to) return false;
    strlcpy(out, body.substring(i, j).c_str(), outLen);
    return true;
}

// ── Trích số "key":value trong đoạn con [from, to) ──────────────────────
static bool jsonNumIn(const String& body, int from, int to,
                      const char* key, float& out) {
    String pat = String("\"") + key + "\":";
    int i = body.indexOf(pat, from);
    if (i < 0 || i >= to) return false;
    i += pat.length();
    int j = i;
    while (j < to) {
        char c = body[j];
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+') j++;
        else break;
    }
    if (j == i) return false;
    out = body.substring(i, j).toFloat();
    return true;
}

// Chuyển epoch (UTC) + offset → giờ trong ngày 0..23.
static int epochToHour(long dt) {
    long local = dt + OWM_TZ_OFFSET_SEC;
    return (int)(((local / 3600) % 24 + 24) % 24);
}

bool fetchWeather(WeatherData& out) {
    WiFiClientSecure client;
    // OpenWeatherMap dùng chứng chỉ Sectigo — không nằm trong CA_BUNDLE của thiết bị.
    // Thời tiết là dữ liệu công khai (không kèm token/secret) nên bỏ kiểm tra cert
    // ở riêng kết nối này là chấp nhận được. Các kết nối tới api.anthropic.com vẫn
    // xác thực đầy đủ bằng CA_BUNDLE như cũ.
    client.setInsecure();

    int cnt = 1 + FORECAST_SLOTS;   // 1 hiện tại + N cột forecast
    String url = String(WEATHER_ENDPOINT) +
                 "?lat=" + OWM_LAT +
                 "&lon=" + OWM_LON +
                 "&units=" + OWM_UNITS +
                 "&cnt=" + String(cnt) +
                 "&appid=" + OWM_API_KEY;

    HTTPClient https;
    if (!https.begin(client, url)) {
        strlcpy(out.error, "https_init", sizeof(out.error));
        return false;
    }
    https.addHeader("User-Agent", "claude-usage-stick/1.0");
    https.setTimeout(API_TIMEOUT_MS);

    Serial.println("[WX] GET forecast");
    int code = https.GET();
    Serial.printf("[WX] HTTP %d\n", code);

    if (code != 200) {
        https.end();
        if (code == 401) strlcpy(out.error, "bad_api_key", sizeof(out.error));
        else snprintf(out.error, sizeof(out.error), "http_%d", code);
        return false;   // giữ dữ liệu cũ
    }

    int len = https.getSize();
    if (len > 16384) {
        Serial.printf("[WX] body too large (%d)\n", len);
        https.end();
        strlcpy(out.error, "too_large", sizeof(out.error));
        return false;
    }

    String body = https.getString();
    https.end();

    // Tên thành phố trong object "city":{...,"name":"..."} ở cuối.
    int cityPos = body.indexOf("\"city\"");
    if (cityPos < 0) cityPos = 0;
    jsonStrIn(body, cityPos, body.length(), "name", out.city, sizeof(out.city));

    // Duyệt từng phần tử list qua các mốc "dt": liên tiếp.
    // slot = -1 → phần tử đầu = hiện tại (gần đúng, lệch <3h); slot 0.. = các cột.
    int searchFrom = 0;
    for (int slot = -1; slot < FORECAST_SLOTS; slot++) {
        int dtPos = body.indexOf("\"dt\":", searchFrom);
        if (dtPos < 0) break;
        int nextDt = body.indexOf("\"dt\":", dtPos + 5);
        int segEnd = (nextDt < 0) ? (int)body.length() : nextDt;

        float dtF = 0, tF = 0, idF = 0;
        char icon[8] = {0};
        jsonNumIn(body, dtPos, segEnd, "dt", dtF);
        bool okT = jsonNumIn(body, dtPos, segEnd, "temp", tF);
        jsonStrIn(body, dtPos, segEnd, "icon", icon, sizeof(icon));
        jsonNumIn(body, dtPos, segEnd, "id", idF);      // weather[0].id (phân loại chi tiết)
        int wid = (int)idF;

        if (slot < 0) {
            if (!okT || icon[0] == 0) {
                strlcpy(out.error, "parse_fail", sizeof(out.error));
                return false;
            }
            out.temp = tF;
            out.feelsLike = tF;
            float humF = 0; jsonNumIn(body, dtPos, segEnd, "humidity", humF);
            out.humidity = (int)humF;
            float popF = 0; jsonNumIn(body, dtPos, segEnd, "pop", popF);
            out.pop = (int)lroundf(popF * 100.0f);   // 0.0–1.0 → %
            strlcpy(out.iconName, mapIcon(wid, icon), sizeof(out.iconName));
            jsonStrIn(body, dtPos, segEnd, "description", out.desc, sizeof(out.desc));
        } else {
            ForecastSlot& fs = out.slots[slot];
            if (okT && icon[0]) {
                fs.hour  = epochToHour((long)dtF);
                fs.temp  = (int)lroundf(tF);
                strlcpy(fs.iconName, mapIcon(wid, icon), sizeof(fs.iconName));
                fs.valid = true;
            } else {
                fs.valid = false;
            }
        }
        searchFrom = dtPos + 5;
    }

    out.ok = true;
    out.error[0] = 0;
    Serial.printf("[WX] %s %.1f %s | slots %d\n",
                  out.city, out.temp, out.desc, FORECAST_SLOTS);
    return true;
}

#endif // MANGO_UI
