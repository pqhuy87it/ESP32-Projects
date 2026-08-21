#include "screen_minimal.h"
#include "hal.h"
#include "theme.h"
#include "icons.h"
#include "config.h"
#include <math.h>

// ============================================================================
// Man hinh toi gian
//
//   \ | | | | | /
//   -  Mon 16 June  -
//   -    14:32      -        <- gio lon, dam, trang
//   -    (o) 21'    -
//   / | | | | | \
//
// VANH VACH CHIA
// Cac vach bam theo duong bo cua HINH VUONG chu khong phai hinh tron, nen
// khong the dung ban kinh co dinh. Ta dung duong superellipse:
//
//     |x/a|^n + |y/a|^n = 1
//
// Voi n = 2 la hinh tron, n -> vo cung la hinh vuong. n khoang 3.2 cho ra
// hinh vuong bo goc giong anh mau. Ban kinh theo huong theta:
//
//     d = a / (|cos|^n + |sin|^n)^(1/n)
//
// Moi vach con duoc nghieng thêm TICK_SKEW_DEG so voi phuong xuyen tam —
// day la chi tiet lam cho vanh trong "co thiet ke" thay vi chi la tia toa.
// ============================================================================

static constexpr int   TICK_COUNT     = 60;
static constexpr float TICK_SQUIRCLE  = 3.2f;   // 2 = tron, lon hon = vuong hon
static constexpr float TICK_SKEW_DEG  = 18.0f;  // 0 = vach xuyen tam
static constexpr int   TICK_LEN       = 13;
static constexpr int   TICK_MARGIN    = 5;      // cach mep man hinh

// Vung xoa cua tung hang. Goc cua nhung o chu nhat nay phai nam BEN TRONG
// mep trong cua vanh vach, neu khong moi lan cap nhat gio se an mat vai vach.
// Hang gio nam gan tam nen duoc phep rong hon hai hang con lai.
static constexpr int DATE_CY = 70,  DATE_X = 34, DATE_W = 172, DATE_H = 32;
static constexpr int TIME_CY = 122, TIME_X = 26, TIME_W = 188, TIME_H = 80;
static constexpr int WX_CY   = 176, WX_X   = 34, WX_W   = 172, WX_H   = 32;

static constexpr int WX_ICON_BOX = 24;

static const char* const WDAY_SHORT[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* const MONTH_NAME[12] = {
  "January", "February", "March",     "April",   "May",      "June",
  "July",    "August",   "September", "October", "November", "December"
};

// ---------------------------------------------------------------------------

static void drawTickRing() {
  const float cx = SCR_W / 2.0f;
  const float cy = SCR_H / 2.0f;
  const float a  = SCR_W / 2.0f - TICK_MARGIN;
  const float skew = TICK_SKEW_DEG * (float)M_PI / 180.0f;

  for (int i = 0; i < TICK_COUNT; i++) {
    const float th = (2.0f * (float)M_PI * i) / TICK_COUNT - (float)M_PI / 2.0f;
    const float ct = cosf(th), st = sinf(th);

    // Ban kinh cua superellipse theo huong th
    const float denom = powf(powf(fabsf(ct), TICK_SQUIRCLE) +
                             powf(fabsf(st), TICK_SQUIRCLE),
                             1.0f / TICK_SQUIRCLE);
    const float d = a / denom;

    // Tam cua vach, nam giua doan trong va doan ngoai
    const float mr = d - TICK_LEN / 2.0f;
    const float mx = cx + ct * mr;
    const float my = cy + st * mr;

    // Huong cua vach = phuong xuyen tam xoay them mot goc nghieng
    const float ux = cosf(th + skew);
    const float uy = sinf(th + skew);
    const float h  = TICK_LEN / 2.0f;

    const int x1 = (int)lroundf(mx - ux * h);
    const int y1 = (int)lroundf(my - uy * h);
    const int x2 = (int)lroundf(mx + ux * h);
    const int y2 = (int)lroundf(my + uy * h);

    // Vach phia tren sang hon, phia duoi toi dan
    const uint16_t c = lerp565(P.tickA, P.tickB, (1.0f + st) * 0.5f);

    // Day 2px: ve them mot duong lech theo phuong VUONG GOC voi vach,
    // neu lech theo phuong doc thi vach ngang se khong day len duoc.
    const int px = (int)lroundf(-uy);
    const int py = (int)lroundf(ux);
    tft.drawLine(x1,      y1,      x2,      y2,      c);
    tft.drawLine(x1 + px, y1 + py, x2 + px, y2 + py, c);
  }
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

static void drawTimeRow(const AppData& d) {
  tft.fillRect(TIME_X, TIME_CY - TIME_H / 2, TIME_W, TIME_H, P.bg);

  char buf[8];
  if (d.show12h) snprintf(buf, sizeof buf, "%d:%02d",   d.hourShown, d.minute);
  else           snprintf(buf, sizeof buf, "%02d:%02d", d.hourShown, d.minute);

  tft.setTextFont(8);                 // 75px — chi chu so va dau ':'
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.line, P.bg);     // trang
  tft.drawString(buf, SCR_W / 2, TIME_CY);
}

static void drawWeatherRow(const AppData& d) {
  tft.fillRect(WX_X, WX_CY - WX_H / 2, WX_W, WX_H, P.bg);

  char num[8];
  snprintf(num, sizeof num, "%d", d.temp);

  tft.setTextFont(4);
  tft.setTextColor(P.muted, P.bg);

  // Can giua ca cum: icon + khoang cach + so + vong tron do
  const int wNum   = tft.textWidth(num);
  const int total  = WX_ICON_BOX + 8 + wNum + 11;
  int x = (SCR_W - total) / 2;

  iconWeather(d.icon, x, WX_CY - WX_ICON_BOX / 2, WX_ICON_BOX, P.muted);
  x += WX_ICON_BOX + 8;

  tft.setTextDatum(ML_DATUM);
  tft.drawString(num, x, WX_CY);
  iconDegreeRing(x + wNum + 5, WX_CY - tft.fontHeight() / 2 + 5, 3, P.muted);
}

// ---------------------------------------------------------------------------

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) {
    tft.fillScreen(P.bg);
    drawTickRing();          // vanh khong bao gio doi, chi ve khi ve lai toan bo
  }

  if (full || cur.day != prev.day || cur.month != prev.month ||
      cur.weekday != prev.weekday)                              drawDateRow(cur);

  if (full || cur.hourShown != prev.hourShown ||
      cur.minute != prev.minute || cur.isPM != prev.isPM)        drawTimeRow(cur);

  if (full || cur.temp != prev.temp || cur.icon != prev.icon)    drawWeatherRow(cur);
}

const ScreenDef SCREEN_MINIMAL = { "Minimal", render };
