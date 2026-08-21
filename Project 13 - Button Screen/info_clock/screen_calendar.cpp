#include "screen_calendar.h"
#include "hal.h"
#include "theme.h"

// ============================================================================
// Man lich — nen den thuan, KHONG duong ke (theo anh mau).
//
// Font 1 (GLCD) la font pixel 5x7. setTextSize(N) ve moi pixel thanh mot o
// vuong NxN, nen phong to len se ra dung phong cach LCD retro.
// ============================================================================
static constexpr int CAL_TOP       = 8;
static constexpr int CAL_BOT       = SCR_H - 8;
static constexpr int CAL_SPLIT_X   = 120;      // ranh 50/50, khong ve
static constexpr int CAL_ROW_H     = 32;       // chieu cao mot dong thu
static constexpr int CAL_MID_Y     = SCR_H / 2;

static constexpr int WK_TEXT_SIZE  = 3;        // nhan thu : 5x7 * 3 = 15x21
static constexpr int NUM_TEXT_SIZE = 9;        // so lon   : 5x7 * 9 = 45x63

static const char* const WEEKDAY_SHORT[7] = {
  "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
};

// tm_wday dung 0 = Sunday, con cot cua ta bat dau tu Monday.
// Bo qua buoc doi nay thi highlight lech dung mot ngay — loi rat kho thay.
static uint8_t weekdayToRow(uint8_t tmWday) {
  return (uint8_t)((tmWday + 6) % 7);
}

static void drawWeekdayColumn(const AppData& d) {
  tft.fillRect(0, CAL_TOP, CAL_SPLIT_X, CAL_BOT - CAL_TOP, P.bg);

  const uint8_t todayRow = weekdayToRow(d.weekday);
  const int     colMidX  = CAL_SPLIT_X / 2;

  tft.setTextFont(1);                   // font pixel 5x7
  tft.setTextSize(WK_TEXT_SIZE);
  tft.setTextDatum(MC_DATUM);

  for (int row = 0; row < 7; row++) {
    const int  yMid    = CAL_TOP + row * CAL_ROW_H + CAL_ROW_H / 2;
    const bool isToday = (row == todayRow);

    // Hom nay trang sang; cac ngay khac chay gradient tu tren xuong duoi
    const uint16_t c = isToday ? P.line
                               : lerp565(P.wkTop, P.wkBot, (float)row / 6.0f);
    tft.setTextColor(c, P.bg);
    tft.drawString(WEEKDAY_SHORT[row], colMidX, yMid);
  }

  tft.setTextSize(1);                   // setTextSize dinh trang thai
}

static void drawBigNumber(uint8_t number, int x, int y, int w, int h,
                          uint16_t color) {
  tft.fillRect(x, y, w, h, P.bg);

  char buf[4];
  snprintf(buf, sizeof buf, "%02u", number);

  tft.setTextFont(1);
  tft.setTextSize(NUM_TEXT_SIZE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, P.bg);
  tft.drawString(buf, x + w / 2, y + h / 2);

  tft.setTextSize(1);
}

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) tft.fillScreen(P.bg);

  if (full || cur.weekday != prev.weekday) drawWeekdayColumn(cur);

  if (full || cur.day != prev.day) {
    drawBigNumber(cur.day, CAL_SPLIT_X, CAL_TOP,
                  SCR_W - CAL_SPLIT_X, CAL_MID_Y - CAL_TOP, P.calDay);
  }
  if (full || cur.month != prev.month) {
    drawBigNumber(cur.month, CAL_SPLIT_X, CAL_MID_Y,
                  SCR_W - CAL_SPLIT_X, CAL_BOT - CAL_MID_Y, P.calMonth);
  }
}

const ScreenDef SCREEN_CALENDAR = { "Calendar", render };
