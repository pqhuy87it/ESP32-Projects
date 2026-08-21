/*
 * InfoClock — ESP32-C3 SuperMini + ST7789 240x240
 * ============================================================================
 * Dong ho / thoi tiet nhieu man hinh. WiFi cau hinh qua captive portal,
 * khong hardcode SSID trong code.
 *
 * CAU TRUC PROJECT
 *   InfoClock.ino        setup() / loop() — chi dieu phoi, khong ve gi
 *   config.h             gia tri MAC DINH bien dich san
 *   settings.h/.cpp      tham so luu trong NVS, sua duoc tu portal
 *   app_data.h/.cpp      struct AppData — mo hinh du lieu dung chung
 *   theme.h/.cpp         bang mau theo vai tro thong tin
 *   hal.h/.cpp           man hinh, den nen, nut bam 3 muc
 *   net.h/.cpp           WiFiManager: captive portal + luu credential
 *   timesync.h/.cpp      NTP + doc gio he thong
 *   weather.h/.cpp       goi OpenWeatherMap theo lat/lon trong settings
 *   icons.h/.cpp         ve icon (bitmap neu co weather_icons.h, khong thi code)
 *   demo.h/.cpp          du lieu gia de lam UI khi khong co WiFi
 *   ui.h/.cpp            quan ly nhieu man hinh + bang dang ky SCREENS[]
 *   screen_info.cpp      man hinh 1
 *   screen_calendar.cpp  man hinh 2
 *   screen_weather.cpp   man hinh 3 — gio lon + du bao theo gio
 *   screen_lcd.cpp       man hinh 4 — phong cach LCD segment kieu Casio
 *   screen_minimal.cpp   man hinh 5 — toi gian, vanh vach chia quanh vien
 *   screen_colorful.cpp  man hinh 6 — 4 chu so mau xep luoi 2x2
 *   screen_forecast.cpp  man hinh 7 — du bao 3 ngay, the mau theo thoi tiet
 *   screen_analog.cpp    man hinh 8 — mat dong ho kim, co kim giay
 *
 * THEM MAN HINH MOI: xem huong dan trong ui.h — chi 3 buoc.
 *
 * NUT BAM (GPIO0) — phan loai khi nha tay
 *   Nhan ngan   : chuyen giao dien, xoay vong qua 5 man
 *   Giu 2 giay  : bat/tat che do dem
 *   Giu 6 giay  : mo portal cau hinh WiFi / API key / toa do
 *
 * LAN DAU SU DUNG
 *   1. Nap firmware, man hinh hien "WiFi Setup" kem ten AP
 *   2. Dien thoai ket noi vao AP do (mat khau trong config.h)
 *   3. Trang cau hinh thuong tu bat. Neu khong, mo http://192.168.4.1
 *   4. Chon WiFi, sang tab Setup nhap OpenWeatherMap API key + toa do
 *
 * YEU CAU
 *   - TFT_eSPI voi User_Setups/Setup900_C3_ST7789_240x240.h
 *   - WiFiManager (tzapu) >= 2.0.17
 *   - ArduinoJson
 *   - Tools -> USB CDC On Boot   : Enabled
 *   - Tools -> Partition Scheme  : Huge APP (3MB No OTA/1MB SPIFFS)
 *         ^^^ BAT BUOC. Phan vung mac dinh chi cho 1.25MB, khong du.
 *   - Tools -> Core Debug Level  : None
 * ============================================================================
 */

#include "config.h"
#include "settings.h"
#include "app_data.h"
#include "theme.h"
#include "hal.h"
#include "ui.h"
#include "net.h"
#include "timesync.h"
#include "weather.h"
#include "icons.h"

#if USE_DEMO_DATA
  #include "demo.h"
#endif

static AppData app;

// Sau khi portal dong, ket noi va du lieu co the da doi hoan toan
static void refreshEverything() {
  app.online = netOnline();
  if (app.online) {
    timeBegin();
    timeWaitValid(10000);
    timeSync(app);
    weatherFetch(app);
    weatherFetchForecast(app);
  } else {
    strlcpy(app.city, "No WiFi", sizeof app.city);
  }
  uiForceRedraw();
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println(F("\n=============================="));
  Serial.println(F(" InfoClock — ESP32-C3 + ST7789"));
  Serial.println(F("=============================="));
  // Bao ro tung bo icon va o nao dung duoc bo nao — de khong phai doan
  Serial.printf("Icon lon    : %s (%d px)\n",
                iconsBigSize()   ? "co" : "KHONG", iconsBigSize());
  Serial.printf("Icon nho    : %s (%d px)\n",
                iconsSmallSize() ? "co" : "KHONG", iconsSmallSize());
  {
    // In ro moi o icon se lay tu bo bitmap nao. "22<-40" nghia la o 22px
    // duoc thu nho tu bitmap 40px.
    const int boxes[] = { 42, 34, 30, 26, 24, 22 };
    Serial.print(F("O icon      : "));
    for (int b : boxes) {
      int use = 0;
      if (iconsSmallSize() >= b)  use = iconsSmallSize();
      else if (iconsBigSize())    use = iconsBigSize();
      else                        use = iconsSmallSize();

      if (use) Serial.printf("%d<-%d  ", b, use);
      else     Serial.printf("%d=code  ", b);
    }
    Serial.println();
  }

  appDataInit(app);
  settingsLoad();
  halBegin();
  uiBegin();
  Serial.printf("So man hinh : %u\n", uiScreenCount());

#if USE_DEMO_DATA
  Serial.println(F("Che do DEMO — khong dung WiFi"));
  demoInit(app);
#else
  halSplash("Connecting WiFi");
  netBegin();            // tu mo portal neu chua co credential
  refreshEverything();
#endif

  uiForceRedraw();
  uiRender(app);
}

void loop() {
  switch (halPollButton()) {
    case BtnEvent::Short:
      uiNextScreen();
      break;

    case BtnEvent::Long:
      themeSetNight(!themeIsNight());
      uiForceRedraw();
      break;

    case BtnEvent::VeryLong:
#if !USE_DEMO_DATA
      // startConfigPortal() la blocking — thiet bi dung yen trong luc ban
      // cau hinh tren dien thoai, va tu thoat sau PORTAL_TIMEOUT_S giay.
      netStartPortal();
      refreshEverything();
#endif
      break;

    case BtnEvent::None:
      break;
  }

#if USE_DEMO_DATA
  demoTick(app);
#else
  static uint32_t lastClock   = 0;
  static uint32_t lastWeather = 0;
  const  uint32_t now         = millis();

  if (now - lastClock >= CLOCK_PERIOD_MS) {
    lastClock = now;
    timeSync(app);
  }

  if (now - lastWeather >= WEATHER_PERIOD_MS) {
    lastWeather = now;
    weatherFetch(app);
  }

  static uint32_t lastForecast = 0;
  if (now - lastForecast >= FORECAST_PERIOD_MS) {
    lastForecast = now;
    weatherFetchForecast(app);
  }
#endif

  // TODO do am trong nha: gan SHT31 hoac BME280 qua I2C.
  // Con trong GPIO 8, 9, 10, 20, 21. Sau khi doc duoc thi dat:
  //   app.humidityIn    = (uint8_t)lroundf(sensor.readHumidity());
  //   app.hasHumidityIn = true;

  uiRender(app);
  delay(30);        // C3 mot nhan — luon nhuong CPU cho idle task
}
