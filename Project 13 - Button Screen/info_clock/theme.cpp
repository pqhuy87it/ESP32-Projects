#include "theme.h"

static constexpr Palette PAL_BRIGHT = {
  rgb565(  0,   0,   0),      // bg
  rgb565(245, 230,  70),      // title    : vang
  rgb565( 90, 240, 100),      // label    : xanh la sang
  rgb565(110, 210, 255),      // value    : xanh lam nhat
  rgb565(255, 255, 255),      // line     : trang
  rgb565(120, 120, 126),      // muted    : xam nhat
  rgb565( 70,  74,  78),      // dim      : xam toi
  rgb565( 80, 225,  95),      // calDay   : xanh la
  rgb565( 70, 210, 205),      // calMonth : xanh ngoc
  rgb565( 78,  92, 118),      // wkTop
  rgb565( 66, 108, 104),      // wkBot
  rgb565( 30,  32,  30),      // lcdBezel
  rgb565(168, 178, 158),      // lcdBg   : xanh xam kieu man LCD
  rgb565( 38,  44,  38),      // lcdOn
  rgb565(132, 142, 124),      // lcdOff
  rgb565(126, 128, 224),      // tickA : cham tim sang
  rgb565( 66,  68, 138)       // tickB : cham tim toi
};

// Cung tong mau, giam sang khoang mot nua — dung ban dem
static constexpr Palette PAL_NIGHT = {
  rgb565(  0,   0,   0),
  rgb565(122, 115,  35),
  rgb565( 45, 120,  50),
  rgb565( 55, 105, 128),
  rgb565(150, 150, 150),
  rgb565( 58,  58,  62),
  rgb565( 36,  38,  40),
  rgb565( 40, 112,  48),
  rgb565( 35, 105, 102),
  rgb565( 39,  46,  59),
  rgb565( 33,  54,  52),
  rgb565( 10,  11,  10),
  rgb565( 62,  68,  58),
  rgb565( 16,  18,  16),
  rgb565( 46,  51,  43),
  rgb565( 62,  63, 112),
  rgb565( 33,  34,  69)
};

Palette P = PAL_BRIGHT;
static bool nightMode = false;

void themeSetNight(bool night) {
  nightMode = night;
  P         = night ? PAL_NIGHT : PAL_BRIGHT;
}

bool themeIsNight() { return nightMode; }

uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  const int ar = (a >> 8) & 0xF8, ag = (a >> 3) & 0xFC, ab = (a << 3) & 0xF8;
  const int br = (b >> 8) & 0xF8, bg = (b >> 3) & 0xFC, bb = (b << 3) & 0xF8;
  return rgb565((uint8_t)(ar + (br - ar) * t),
                (uint8_t)(ag + (bg - ag) * t),
                (uint8_t)(ab + (bb - ab) * t));
}
