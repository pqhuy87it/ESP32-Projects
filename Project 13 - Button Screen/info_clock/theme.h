#pragma once
#include <Arduino.h>

// ============================================================================
// theme.h — bang mau dung chung. Mau duoc gan theo VAI TRO thong tin,
// khong theo vi tri, nen man hinh moi chi viec chon dung vai tro.
// ============================================================================

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct Palette {
  uint16_t bg;        // nen den
  uint16_t title;     // VANG    — thong tin quan trong nhat
  uint16_t label;     // XANH LA — nhan, chu mo ta
  uint16_t value;     // XANH LAM— con so, gia tri do duoc
  uint16_t line;      // TRANG   — thanh ngan khu vuc, muc duoc chon
  uint16_t muted;     // XAM NHAT— muc khong duoc chon
  uint16_t dim;       // xam toi — vach phu, vien mo

  // Rieng cho man lich
  uint16_t calDay;    // so ngay  — xanh la
  uint16_t calMonth;  // so thang — xanh ngoc
  uint16_t wkTop;     // gradient nhan thu: mut tren
  uint16_t wkBot;     // gradient nhan thu: mut duoi

  // Rieng cho man LCD segment (screen_lcd)
  uint16_t lcdBezel;  // nen quanh cac khung
  uint16_t lcdBg;     // nen trong khung LCD
  uint16_t lcdOn;     // doan dang bat — dam
  uint16_t lcdOff;    // doan chua bat — mo, tao cam giac LCD that

  // Rieng cho man toi gian co vanh vach chia (screen_minimal)
  uint16_t tickA;     // mau vach o phia tren
  uint16_t tickB;     // mau vach o phia duoi
};

extern Palette P;                     // bang mau dang dung

void     themeSetNight(bool night);
bool     themeIsNight();
uint16_t lerp565(uint16_t a, uint16_t b, float t);   // noi suy mau, t = 0..1
