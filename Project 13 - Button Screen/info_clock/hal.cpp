#include "hal.h"
#include "config.h"
#include "theme.h"

TFT_eSPI tft = TFT_eSPI();

void halBacklight(uint8_t level) {
#ifdef TFT_BL
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    static bool attached = false;
    if (!attached) { ledcAttach(TFT_BL, 12000, 8); attached = true; }
    ledcWrite(TFT_BL, level);
  #else
    static bool attached = false;
    if (!attached) { ledcSetup(0, 12000, 8); ledcAttachPin(TFT_BL, 0); attached = true; }
    ledcWrite(0, level);
  #endif
#else
  (void)level;      // BLK noi thang 3V3 — khong dieu khien duoc
#endif
}

void halBegin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);      // can cho pushImage. Mau icon sai -> doi false
  tft.fillScreen(P.bg);

  halBacklight(BACKLIGHT_LEVEL);
}

BtnEvent halPollButton() {
  static bool     stableDown   = false;
  static bool     lastRawDown  = false;
  static uint32_t lastChangeMs = 0;
  static uint32_t pressedAtMs  = 0;

  const bool     rawDown = (digitalRead(PIN_BUTTON) == LOW);
  const uint32_t now     = millis();

  if (rawDown != lastRawDown) {
    lastRawDown  = rawDown;
    lastChangeMs = now;
    return BtnEvent::None;
  }
  if (now - lastChangeMs < BTN_DEBOUNCE_MS) return BtnEvent::None;
  if (rawDown == stableDown)                return BtnEvent::None;

  stableDown = rawDown;

  if (stableDown) {                 // vua nhan xuong
    pressedAtMs = now;
    return BtnEvent::None;
  }

  // Vua nha tay — phan loai theo thoi gian giu.
  // Phan loai khi nha (khong phai khi du nguong) de mot lan nhan chi sinh
  // dung mot su kien, tranh viec giu 6 giay lam kich ca Long lan VeryLong.
  const uint32_t held = now - pressedAtMs;
  if (held >= BTN_VERYLONG_MS) return BtnEvent::VeryLong;
  if (held >= BTN_LONG_MS)     return BtnEvent::Long;
  return BtnEvent::Short;
}

void halSplash(const char* msg) {
  tft.fillScreen(P.bg);
  tft.setTextFont(4);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.label, P.bg);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
}

void halMessage(const char* line1, const char* line2, const char* line3) {
  tft.fillScreen(P.bg);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);

  const int cx = tft.width() / 2;

  tft.setTextFont(4);
  tft.setTextColor(P.title, P.bg);
  tft.drawString(line1, cx, 70);

  tft.setTextFont(4);
  tft.setTextColor(P.label, P.bg);
  tft.drawString(line2, cx, 120);

  tft.setTextFont(2);
  tft.setTextColor(P.value, P.bg);
  tft.drawString(line3, cx, 165);
}
