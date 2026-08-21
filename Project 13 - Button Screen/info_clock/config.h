#pragma once
// ============================================================================
// config.h — TOAN BO tham so nguoi dung. Khong sua file khac de cau hinh.
// ============================================================================

// ---------- WiFi ----------
// KHONG con hardcode SSID/mat khau. WiFiManager mo AP captive portal cho ban
// chon mang tu dien thoai, roi luu vao flash.
//   - Lan dau bat may: tim WiFi ten AP_SSID_PREFIX-XXXX, mat khau AP_PASSWORD
//   - Mo http://192.168.4.1 (thuong tu bat), chon WiFi, nhap API key
//   - Doi cau hinh sau nay: GIU NUT 6 GIAY
#define AP_SSID_PREFIX          "InfoClock"
#define AP_PASSWORD             "12345678"     // toi thieu 8 ky tu
#define PORTAL_TIMEOUT_S        180            // portal tu tat sau 3 phut
#define WIFI_CONNECT_TIMEOUT_S  20

// ---------- OpenWeatherMap ----------
// Lay API key mien phi: https://openweathermap.org/api  (key moi can ~10 phut)
// Ba gia tri duoi day chi la MAC DINH cho lan chay dau tien. Sau do chung
// duoc luu trong NVS va sua duoc tu trang cau hinh, xem settings.h.
#define OWM_API_KEY         "28c29099075601342f371617f43b2878"            // de trong, nhap tu portal
#define OWM_LAT             21.0285f      // Ha Noi
#define OWM_LON             105.8542f
#define OWM_UNITS           "metric"      // "metric" = C, "imperial" = F
#define TEMP_UNIT           'C'           // doi thanh 'F' neu dung imperial
#define CITY_FALLBACK       "Hanoi"       // hien khi chua goi duoc API

// ---------- Mui gio (chuoi POSIX) ----------
// Ha Noi khong co gio mua he. Mot so vi du khac:
//   Tokyo      "JST-9"
//   Singapore  "SGT-8"
//   New York   "EST5EDT,M3.2.0,M11.1.0"
#define TZ_POSIX            "ICT-7"
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.google.com"
#define NTP_SERVER_3        "time.cloudflare.com"

// ---------- Chu ky cap nhat ----------
#define WEATHER_PERIOD_MS   (10UL * 60UL * 1000UL)   // 10 phut
#define FORECAST_PERIOD_MS  (30UL * 60UL * 1000UL)   // du bao it doi, 30 phut

// OpenWeatherMap tra du bao theo buoc 3 gio. Chon 3 moc trong danh sach do.
// {1,3,5} = khoang +6h, +12h, +18h — trai deu trong ngay.
#define FORECAST_PICK_1     1
#define FORECAST_PICK_2     3
#define FORECAST_PICK_3     5

// So moc 3 gio tai ve. 32 = 4 ngay, du de gop ra min/max cho 3 ngay toi.
// Giam xuong neu C3 bao het RAM khi parse JSON.
#define FORECAST_FETCH_CNT  32
#define CLOCK_PERIOD_MS     1000UL

// ---------- Giao dien ----------
#define USE_12H_CLOCK       1        // 0 = dong ho 24 gio, badge AM/PM tu an
#define BACKLIGHT_LEVEL     210      // 0..255, chi co tac dung khi Setup dinh nghia TFT_BL

// ---------- Icon gio ----------
// OpenWeatherMap KHONG co trang thai "windy" — gio chi la so lieu wind.speed.
// Nen ta tu quyet dinh: khi troi quang hoac nhieu may MA gio manh hon nguong
// nay thi hien icon gio thay vi icon may.
// Mua/dong/suong mu luon duoc uu tien hon vi quan trong hon.
// Dat 0 de tat hoan toan, khi do icon gio chi hien khi API bao "Squall".
//   5 m/s = gio nhe   |  8 m/s = gio kha manh  |  11 m/s = gio manh
// LUU Y: day la hang so THUC. Dung dat no vao `#if` — bo tien xu ly chi tinh
// duoc bieu thuc so nguyen. Trong code hay dung `if` thuong, trinh bien dich
// se tu loai bo nhanh chet khi gia tri bang 0.
#define WIND_ICON_THRESHOLD_MS   8.0f

// ---------- Man hinh "Analog" ----------
// 1 = co kim giay (ve lai moi giay). 0 = chi kim gio va phut, nhe hon nhieu.
#define ANALOG_SHOW_SECONDS   1
#define ANALOG_SECOND_RGB     176,  62,  56      // do tham cho kim giay

// ---------- Man hinh "Colorful": mau cua 4 chu so ----------
// Thu tu: gio-chuc, gio-donvi, phut-chuc, phut-donvi  (o luoi 2x2)
// Moi bo ba so la R, G, B. Doi thoai mai theo so thich.
#define DIGIT_1_RGB      245, 240, 225      // kem
#define DIGIT_2_RGB      190, 115,  85      // dat nung
#define DIGIT_3_RGB      220, 195, 105      // vang mu tat
#define DIGIT_4_RGB      246, 241, 224      // kem
#define DIGIT_GHOST_RGB   26,  27,  26      // chu so khong lo o nen

// ---------- Nut bam ----------
// Chan man hinh nam trong TFT_eSPI/User_Setups/Setup900_C3_ST7789_240x240.h
#define PIN_BUTTON          0
#define BTN_DEBOUNCE_MS     30
#define BTN_LONG_MS         2000     // giu 2 giay  -> che do dem
#define BTN_VERYLONG_MS     6000     // giu 6 giay  -> mo portal cau hinh WiFi

// ---------- Phat trien ----------
// 1 = dung du lieu gia, khong can WiFi. Rat tien khi lam giao dien moi.
#define USE_DEMO_DATA       0
