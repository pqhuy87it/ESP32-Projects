#include "screen_colorful.h"
#include "hal.h"
#include "theme.h"
#include "icons.h"
#include "config.h"

// ============================================================================
// Man hinh "Colorful"
//
//        Sat 16 June          <- ngay, xam
//         1  4                <- gio: 2 chu so tren
//         3  2                <- phut: 2 chu so duoi, moi so mot mau
//         (o) 21'             <- thoi tiet, xam
//
// Nen phia sau co hai chu so KHONG LO mau rat toi, tran ra khoi khung —
// day la lop trang tri, khong phai du lieu them. No hien chu so PHUT nen
// doi cung nhip voi cac lop khac, khong gay nhap nhay them.
//
// Ve theo thu tu: nen -> chu so khong lo -> 4 chu so mau.
// ============================================================================

static constexpr int DATE_CY = 38;
static constexpr int DATE_X  = 20, DATE_W = SCR_W - 40, DATE_H = 30;

// Khoi luoi 2x2. Ca lop nen va lop mau deu nam trong o nay va duoc
// xoa/ve lai cung nhau, vi chung de len nhau.
static constexpr int GRID_Y = 58, GRID_H = 146;
static constexpr int ROW_T_CY = 104;
static constexpr int ROW_B_CY = 165;
static constexpr int DIGIT_GAP = 3;

// Chu so khong lo: Font 8 phong 2 lan = 150px cao
static constexpr int GHOST_SIZE = 2;
static constexpr int GHOST_CY   = 132;
static constexpr int GHOST_L_CX = 76;
static constexpr int GHOST_R_CX = 166;

static constexpr int WX_CY = 222;
static constexpr int WX_X  = 24, WX_W = SCR_W - 48, WX_H = 32;
static constexpr int WX_ICON_BOX = 24;

static const char* const WDAY_SHORT[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* const MONTH_NAME[12] = {
  "January", "February", "March",     "April",   "May",      "June",
  "July",    "August",   "September", "October", "November", "December"
};

// Mau chu so lay tu config.h. Che do dem lam toi ca bon de dong bo voi
// cac man khac, vi day la mau tu do nen khong nam trong Palette.
static uint16_t warm(uint8_t r, uint8_t g, uint8_t b) {
  if (themeIsNight()) { r /= 2; g /= 2; b /= 2; }
  return rgb565(r, g, b);
}

// ---------------------------------------------------------------------------

static void drawDateRow(const AppData& d) {
  tft.fillRect(DATE_X, DATE_CY - DATE_H / 2, DATE_W, DATE_H, P.bg);

  char buf[28];
  snprintf(buf, sizeof buf, "%s %u %s",
           WDAY_SHORT[d.weekday % 7], d.day, MONTH_NAME[(d.month - 1) % 12]);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.muted, P.bg);
  tft.setTextFont(4);
  if (tft.textWidth(buf) > DATE_W) tft.setTextFont(2);
  tft.drawString(buf, SCR_W / 2, DATE_CY);
}

static void drawDigitGrid(const AppData& d) {
  tft.fillRect(0, GRID_Y, SCR_W, GRID_H, P.bg);

  char hh[4], mm[4];
  snprintf(hh, sizeof hh, "%02u", d.hourShown);   // luon 2 chu so de luoi 2x2 can doi
  snprintf(mm, sizeof mm, "%02u", d.minute);

  // --- Lop 1: hai chu so khong lo, mau rat toi ---
  const uint16_t ghost = warm(DIGIT_GHOST_RGB);
  tft.setTextFont(8);
  tft.setTextSize(GHOST_SIZE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ghost, P.bg);

  char one[2] = { mm[0], '\0' };
  tft.drawString(one, GHOST_L_CX, GHOST_CY);
  one[0] = mm[1];
  tft.drawString(one, GHOST_R_CX, GHOST_CY);

  tft.setTextSize(1);

  // --- Lop 2: bon chu so mau ---
  // Do be rong chu so de tu can giua khoi 2x2, khong hardcode toa do cot
  const int dw   = tft.textWidth("0");
  const int colL = SCR_W / 2 - dw / 2 - DIGIT_GAP / 2;
  const int colR = SCR_W / 2 + dw / 2 + DIGIT_GAP / 2;

  struct Cell { char ch; int cx; int cy; uint16_t color; };
  const Cell cells[4] = {
    { hh[0], colL, ROW_T_CY, warm(DIGIT_1_RGB) },
    { hh[1], colR, ROW_T_CY, warm(DIGIT_2_RGB) },
    { mm[0], colL, ROW_B_CY, warm(DIGIT_3_RGB) },
    { mm[1], colR, ROW_B_CY, warm(DIGIT_4_RGB) },
  };

  // Nen trong suot de chu so mau khong xoa mat chu so khong lo phia sau
  for (const Cell& c : cells) {
    const char s[2] = { c.ch, '\0' };
    tft.setTextColor(c.color);
    tft.drawString(s, c.cx, c.cy);
  }

  tft.setTextColor(P.line, P.bg);      // tra lai che do nen dac
}

static void drawWeatherRow(const AppData& d) {
  tft.fillRect(WX_X, WX_CY - WX_H / 2, WX_W, WX_H, P.bg);

  char num[8];
  snprintf(num, sizeof num, "%d", d.temp);

  tft.setTextFont(4);
  tft.setTextColor(P.muted, P.bg);

  const int wNum  = tft.textWidth(num);
  const int total = WX_ICON_BOX + 8 + wNum + 11;
  int x = (SCR_W - total) / 2;

  iconWeather(d.icon, x, WX_CY - WX_ICON_BOX / 2, WX_ICON_BOX, P.muted);
  x += WX_ICON_BOX + 8;

  tft.setTextDatum(ML_DATUM);
  tft.drawString(num, x, WX_CY);
  iconDegreeRing(x + wNum + 5, WX_CY - tft.fontHeight() / 2 + 5, 3, P.muted);
}

// ---------------------------------------------------------------------------

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) tft.fillScreen(P.bg);

  if (full || cur.day != prev.day || cur.month != prev.month ||
      cur.weekday != prev.weekday)                            drawDateRow(cur);

  if (full || cur.hourShown != prev.hourShown ||
      cur.minute != prev.minute)                              drawDigitGrid(cur);

  if (full || cur.temp != prev.temp || cur.icon != prev.icon) drawWeatherRow(cur);
}

const ScreenDef SCREEN_COLORFUL = { "Colorful", render };
