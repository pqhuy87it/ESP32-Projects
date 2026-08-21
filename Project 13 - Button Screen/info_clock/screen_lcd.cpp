#include "screen_lcd.h"
#include "hal.h"
#include "theme.h"
#include "icons.h"
#include "config.h"

// ============================================================================
// Man hinh LCD segment (kieu dong ho de ban Casio)
//
//   +--------------------------------+
//   |   M  T  W  T  F  S  S          |  <- khung 1: thu trong tuan
//   +--------------------------------+
//   | AM                             |
//   | PM    12:12                    |  <- khung 2: gio, chu so 7 doan
//   +--------------------------------+
//   | (o) 21' |  27 NOV              |  <- khung 3: thoi tiet | ngay
//   +--------------------------------+
//
// ----------------------------------------------------------------------------
// HIEU UNG "DOAN MO" — CHI AP DUNG O HAI CHO
//
// Man LCD vat ly luon hien mo cac doan chua bat. O day ta chi tai tao hieu
// ung do tai nhung noi co Y NGHIA la mot lua chon dang bat/tat:
//
//   Khung 1 — bay chu cai thu : sau thu con lai mau lcdOff, hom nay lcdOn
//   Khung 2 — AM / PM         : in san ca hai, chi mot cai duoc "bat"
//
// Chu so gio (khung 2) va toan bo khung 3 KHONG dung hieu ung nay — chung la
// gia tri doc duoc chu khong phai lua chon, nen ve thang mot lop cho ro.
// ============================================================================

static constexpr int PANEL_X = 8;
static constexpr int PANEL_W = SCR_W - 2 * PANEL_X;      // 224
static constexpr int PANEL_R = 10;                        // ban kinh bo goc

static constexpr int P1_Y = 8,   P1_H = 50;               // thu trong tuan
static constexpr int P2_Y = 64,  P2_H = 92;               // gio
static constexpr int P3_Y = 162, P3_H = 70;               // thoi tiet + ngay

static constexpr int WDAY_COL_W  = PANEL_W / 7;           // 32
static constexpr int WDAY_MARK_Y = P1_Y + 10;             // vach tren chu
static constexpr int WDAY_TEXT_Y = P1_Y + 28;
static constexpr int WDAY_BAR_Y  = P1_Y + 42;             // thanh tien do tuan

static constexpr int AMPM_X   = PANEL_X + 26;
static constexpr int AM_Y     = P2_Y + 28;
static constexpr int PM_Y     = P2_Y + 62;
static constexpr int TIME_CX  = PANEL_X + 140;
static constexpr int TIME_CY  = P2_Y + P2_H / 2;

static constexpr int P3_SPLIT_X = 100;                    // vach doc trong khung 3
static constexpr int P3_CY      = P3_Y + P3_H / 2;
static constexpr int WX_ICON_CX = PANEL_X + 24;
static constexpr int WX_ICON_BOX = 26;
static constexpr int WX_TEMP_X  = PANEL_X + 42;
static constexpr int DATE_CX    = (P3_SPLIT_X + SCR_W - PANEL_X) / 2;

// Chu cai thu, bat dau tu thu Hai. Co hai chu T va hai chu S nen bat buoc
// phai co vach danh dau o tren moi biet dang la ngay nao.
static const char* const WDAY_LETTER[7] = { "M", "T", "W", "T", "F", "S", "S" };

static const char* const MONTH_ABBR[12] = {
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

// tm_wday dung 0 = Chu nhat, con hang chu cai bat dau tu thu Hai
static uint8_t weekdayToCol(uint8_t tmWday) {
  return (uint8_t)((tmWday + 6) % 7);
}

// ---------------------------------------------------------------------------

static void drawPanels() {
  tft.fillScreen(P.lcdBezel);
  tft.fillRoundRect(PANEL_X, P1_Y, PANEL_W, P1_H, PANEL_R, P.lcdBg);
  tft.fillRoundRect(PANEL_X, P2_Y, PANEL_W, P2_H, PANEL_R, P.lcdBg);
  tft.fillRoundRect(PANEL_X, P3_Y, PANEL_W, P3_H, PANEL_R, P.lcdBg);
  // Vach doc chia khung 3
  tft.fillRect(P3_SPLIT_X, P3_Y + 10, 2, P3_H - 20, P.lcdOff);
}

static void drawWeekdayPanel(const AppData& d) {
  tft.fillRoundRect(PANEL_X, P1_Y, PANEL_W, P1_H, PANEL_R, P.lcdBg);

  const uint8_t today = weekdayToCol(d.weekday);

  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM);

  for (uint8_t i = 0; i < 7; i++) {
    const int cx = PANEL_X + WDAY_COL_W / 2 + i * WDAY_COL_W;
    const bool isToday = (i == today);

    tft.setTextColor(isToday ? P.lcdOn : P.lcdOff, P.lcdBg);
    tft.drawString(WDAY_LETTER[i], cx, WDAY_TEXT_Y);

    // Vach danh dau phia tren chu cai cua hom nay
    if (isToday) {
      tft.fillRect(cx - 9, WDAY_MARK_Y, 18, 3, P.lcdOn);
    }
  }

  // Thanh tien do tuan: day dan tu thu Hai den Chu nhat
  const int barX = PANEL_X + 10;
  const int barW = PANEL_W - 20;
  tft.fillRect(barX, WDAY_BAR_Y, barW, 2, P.lcdOff);
  tft.fillRect(barX, WDAY_BAR_Y, barW * (today + 1) / 7, 2, P.lcdOn);
}

static void drawTimePanel(const AppData& d) {
  tft.fillRoundRect(PANEL_X, P2_Y, PANEL_W, P2_H, PANEL_R, P.lcdBg);

  // AM / PM in san ca hai, chi mot cai duoc "bat"
  if (d.show12h) {
    tft.setTextFont(4);
    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(d.isPM ? P.lcdOff : P.lcdOn, P.lcdBg);
    tft.drawString("AM", AMPM_X, AM_Y);

    tft.setTextColor(d.isPM ? P.lcdOn : P.lcdOff, P.lcdBg);
    tft.drawString("PM", AMPM_X, PM_Y);
  }

  char buf[8];
  if (d.show12h) snprintf(buf, sizeof buf, "%2d:%02d", d.hourShown, d.minute);
  else           snprintf(buf, sizeof buf, "%02d:%02d", d.hourShown, d.minute);

  // Chu so gio KHONG co lop doan mo — chi ve gio that.
  tft.setTextFont(7);                 // font 7 doan, 48px
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.lcdOn, P.lcdBg);
  tft.drawString(buf, TIME_CX, TIME_CY);
}

static void drawWeatherCell(const AppData& d) {
  tft.fillRect(PANEL_X + 6, P3_Y + 8, P3_SPLIT_X - PANEL_X - 12, P3_H - 16, P.lcdBg);

  // Khung LCD co nen sang -> truyen P.lcdBg lam nen ghep alpha
  iconWeatherOn(d.icon, WX_ICON_CX - WX_ICON_BOX / 2, P3_CY - WX_ICON_BOX / 2,
                WX_ICON_BOX, P.lcdOn, P.lcdBg);

  char num[8];
  snprintf(num, sizeof num, "%d", d.temp);

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(P.lcdOn, P.lcdBg);
  tft.drawString(num, WX_TEMP_X, P3_CY);

  iconDegreeRing(WX_TEMP_X + tft.textWidth(num) + 6,
                 P3_CY - tft.fontHeight() / 2 + 5, 3, P.lcdOn);
}

static void drawDateCell(const AppData& d) {
  tft.fillRect(P3_SPLIT_X + 6, P3_Y + 8,
               SCR_W - PANEL_X - P3_SPLIT_X - 12, P3_H - 16, P.lcdBg);

  char dayStr[4];
  snprintf(dayStr, sizeof dayStr, "%02u", d.day);
  const char* mon = MONTH_ABBR[(d.month - 1) % 12];

  // Do be rong de can giua ca cum "27" + "NOV"
  tft.setTextFont(7);
  const int wDay = tft.textWidth(dayStr);
  tft.setTextFont(1);
  tft.setTextSize(3);                 // font pixel 5x7, phong 3 lan
  const int wMon = tft.textWidth(mon);
  tft.setTextSize(1);

  const int total = wDay + 8 + wMon;
  int x = DATE_CX - total / 2;

  // Chu so ngay: font 7 doan, KHONG co lop doan mo
  tft.setTextFont(7);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(P.lcdOn, P.lcdBg);
  tft.drawString(dayStr, x, P3_CY);
  x += wDay + 8;

  // Ten thang: font pixel de hop tong the LCD (font 7 doan khong co chu cai)
  tft.setTextFont(1);
  tft.setTextSize(3);
  tft.setTextColor(P.lcdOn, P.lcdBg);
  tft.drawString(mon, x, P3_CY);
  tft.setTextSize(1);
}

// ---------------------------------------------------------------------------

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) drawPanels();

  if (full || cur.weekday != prev.weekday) drawWeekdayPanel(cur);

  if (full || cur.hourShown != prev.hourShown ||
      cur.minute != prev.minute || cur.isPM != prev.isPM) drawTimePanel(cur);

  if (full || cur.temp != prev.temp || cur.icon != prev.icon) drawWeatherCell(cur);

  if (full || cur.day != prev.day || cur.month != prev.month) drawDateCell(cur);
}

const ScreenDef SCREEN_LCD = { "LCD", render };
