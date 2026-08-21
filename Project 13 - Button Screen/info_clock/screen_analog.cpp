#include "screen_analog.h"
#include "hal.h"
#include "theme.h"
#include "config.h"
#include <math.h>

// ============================================================================
// Mat dong ho kim
//
//        \ | | | /
//           12
//           PM
//     9      +      3          <- kim gio, kim phut, kim giay do
//        16 June
//            6
//        / | | | \
//
// ----------------------------------------------------------------------------
// VAN DE: kim giay phai ve lai moi giay, ma ST7789 khong co framebuffer.
// Sprite 240x240 ton 115KB nen khong dung duoc khi WiFi dang chay.
//
// CACH GIAI: xoa kim cu bang CHINH HINH DANG cua no, ve bang mau nen.
// Chinh xac tung pixel, khong phai xoa ca vung, nen khong nhap nhay.
//
// Thu tu moi lan cap nhat:
//   1. Xoa ba kim cu   (ve hinh cu bang P.bg — phai xoa CA BA truoc khi ve,
//                      neu khong cho hai kim giao nhau se bi khuyet)
//   2. Ve lai so 12/3/6/9, PM va ngay (kim co the da di qua chung)
//   3. Ve ba kim moi + truc giua
//
// Vanh vach chia nam ngoai tam quet cua kim nen khong bao gio bi anh huong,
// chi ve mot lan khi vao man.
// ============================================================================

static constexpr float CX = SCR_W / 2.0f;
static constexpr float CY = SCR_H / 2.0f;

// --- Vanh vach chia: superellipse, giong man Minimal nhung mau xam ---
static constexpr int   TICK_COUNT    = 60;
static constexpr float TICK_SQUIRCLE = 3.2f;
static constexpr int   TICK_LEN      = 9;
static constexpr int   TICK_LEN_MAJ  = 14;     // moc 5 phut dai hon
static constexpr int   TICK_MARGIN   = 5;

// --- So gio ---
static constexpr int NUM_RADIUS = 90;

// --- Kim: ban kinh (rBack la doi trong phia sau truc) ---
static constexpr int HOUR_TIP   = 52, HOUR_BACK   = 15, HOUR_W_BASE   = 12, HOUR_W_TIP   = 7;
static constexpr int MIN_TIP    = 76, MIN_BACK    = 17, MIN_W_BASE    = 11, MIN_W_TIP    = 6;
static constexpr int SEC_TIP    = 86, SEC_BACK    = 22, SEC_W         = 3;

static constexpr int HUB_R = 5;

// --- Chu ---
static constexpr int PM_CY   = 72;
static constexpr int DATE_CY = 158;

static const char* const MONTH_NAME[12] = {
  "January", "February", "March",     "April",   "May",      "June",
  "July",    "August",   "September", "October", "November", "December"
};

// Goc cua ba kim o lan ve truoc, dung de xoa chinh xac
static float prevHourAng = 0, prevMinAng = 0, prevSecAng = 0;
static bool  havePrev    = false;

// ---------------------------------------------------------------------------
// Hinh hoc kim
// ---------------------------------------------------------------------------

// Kim la mot tu giac thuon: rong wBase o goc, thu lai con wTip o mui.
// Goc tinh theo chieu kim dong ho, 0 = huong 12 gio.
static void handShape(float ang, int rBack, int rTip,
                      int wBase, int wTip, uint16_t c) {
  const float dx =  sinf(ang), dy = -cosf(ang);   // huong ra mui kim
  const float px =  cosf(ang), py =  sinf(ang);   // vuong goc voi kim

  const float bx = CX - dx * rBack, by = CY - dy * rBack;
  const float tx = CX + dx * rTip,  ty = CY + dy * rTip;

  const float hb = wBase / 2.0f, ht = wTip / 2.0f;

  const int b1x = (int)lroundf(bx + px * hb), b1y = (int)lroundf(by + py * hb);
  const int b2x = (int)lroundf(bx - px * hb), b2y = (int)lroundf(by - py * hb);
  const int t1x = (int)lroundf(tx + px * ht), t1y = (int)lroundf(ty + py * ht);
  const int t2x = (int)lroundf(tx - px * ht), t2y = (int)lroundf(ty - py * ht);

  tft.fillTriangle(b1x, b1y, b2x, b2y, t2x, t2y, c);
  tft.fillTriangle(b1x, b1y, t2x, t2y, t1x, t1y, c);
}

// Kim gio/phut kieu vien: khoi dac roi khoet long bang mau nen
static void drawOutlinedHand(float ang, int rBack, int rTip,
                             int wBase, int wTip, uint16_t c) {
  handShape(ang, rBack, rTip, wBase, wTip, c);
  const int iBase = wBase - 6;
  const int iTip  = wTip  - 4;
  if (iBase > 1 && iTip > 0) {
    handShape(ang, rBack - 4, rTip - 6, iBase, iTip, P.bg);
  }
}

// Xoa: ve khoi dac (khong khoet long) bang mau nen
static void eraseHand(float ang, int rBack, int rTip, int wBase, int wTip) {
  handShape(ang, rBack + 1, rTip + 2, wBase + 2, wTip + 2, P.bg);
}

static void eraseSecondHand(float ang) {
  handShape(ang, SEC_BACK + 1, SEC_TIP + 2, SEC_W + 2, SEC_W + 2, P.bg);
}

// ---------------------------------------------------------------------------
// Cac phan tinh
// ---------------------------------------------------------------------------

static void drawTickRing() {
  const float a = SCR_W / 2.0f - TICK_MARGIN;

  for (int i = 0; i < TICK_COUNT; i++) {
    const float th = (2.0f * (float)M_PI * i) / TICK_COUNT - (float)M_PI / 2.0f;
    const float ct = cosf(th), st = sinf(th);

    const float denom = powf(powf(fabsf(ct), TICK_SQUIRCLE) +
                             powf(fabsf(st), TICK_SQUIRCLE),
                             1.0f / TICK_SQUIRCLE);
    const float d   = a / denom;
    const int   len = (i % 5 == 0) ? TICK_LEN_MAJ : TICK_LEN;

    const int x1 = (int)lroundf(CX + ct * (d - len));
    const int y1 = (int)lroundf(CY + st * (d - len));
    const int x2 = (int)lroundf(CX + ct * d);
    const int y2 = (int)lroundf(CY + st * d);

    tft.drawLine(x1, y1, x2, y2, P.dim);
    // Day them 1px theo phuong vuong goc voi vach
    tft.drawLine(x1 + (int)lroundf(-st), y1 + (int)lroundf(ct),
                 x2 + (int)lroundf(-st), y2 + (int)lroundf(ct), P.dim);
  }
}

static void drawNumerals() {
  tft.setTextFont(4);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.line, P.bg);

  tft.drawString("12", (int)CX,              (int)CY - NUM_RADIUS);
  tft.drawString("3",  (int)CX + NUM_RADIUS, (int)CY);
  tft.drawString("6",  (int)CX,              (int)CY + NUM_RADIUS);
  tft.drawString("9",  (int)CX - NUM_RADIUS, (int)CY);
}

static void drawLabels(const AppData& d) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);

  // AM / PM — chi co nghia o che do 12 gio
  tft.setTextFont(2);
  tft.setTextColor(P.muted, P.bg);
  if (d.show12h) tft.drawString(d.isPM ? "PM" : "AM", (int)CX, PM_CY);
  else           tft.drawString("    ", (int)CX, PM_CY);

  char buf[24];
  snprintf(buf, sizeof buf, "%u %s", d.day, MONTH_NAME[(d.month - 1) % 12]);
  tft.setTextFont(4);
  tft.setTextColor(P.muted, P.bg);
  tft.drawString(buf, (int)CX, DATE_CY);
}

// ---------------------------------------------------------------------------

static void drawHands(const AppData& d) {
  const float deg2rad = (float)M_PI / 180.0f;

  // Kim gio chay muot theo phut, khong nhay tung buoc 30 do
  const float hourAng = ((d.hour24 % 12) * 30.0f + d.minute * 0.5f) * deg2rad;
  const float minAng  = (d.minute * 6.0f + d.second * 0.1f) * deg2rad;
  const float secAng  = (d.second * 6.0f) * deg2rad;

  // --- 1. Xoa ba kim cu. Phai xoa het truoc khi ve, neu khong cho hai kim
  //        giao nhau se bi khuyet mot manh.
  if (havePrev) {
    eraseHand(prevHourAng, HOUR_BACK, HOUR_TIP, HOUR_W_BASE, HOUR_W_TIP);
    eraseHand(prevMinAng,  MIN_BACK,  MIN_TIP,  MIN_W_BASE,  MIN_W_TIP);
#if ANALOG_SHOW_SECONDS
    eraseSecondHand(prevSecAng);
#endif
    tft.fillCircle((int)CX, (int)CY, HUB_R + 2, P.bg);
  }

  // --- 2. Ve lai cac phan tinh ma kim vua di qua
  drawNumerals();
  drawLabels(d);

  // --- 3. Ve ba kim moi, tu ngan den dai
  drawOutlinedHand(hourAng, HOUR_BACK, HOUR_TIP, HOUR_W_BASE, HOUR_W_TIP, P.line);
  drawOutlinedHand(minAng,  MIN_BACK,  MIN_TIP,  MIN_W_BASE,  MIN_W_TIP,  P.line);

#if ANALOG_SHOW_SECONDS
  uint8_t sr = 176, sg = 62, sb = 56;
  { const uint8_t rgbv[3] = { ANALOG_SECOND_RGB }; sr = rgbv[0]; sg = rgbv[1]; sb = rgbv[2]; }
  if (themeIsNight()) { sr /= 2; sg /= 2; sb /= 2; }
  const uint16_t secCol = rgb565(sr, sg, sb);

  handShape(secAng, SEC_BACK, SEC_TIP, SEC_W, SEC_W, secCol);
  tft.fillCircle((int)CX, (int)CY, HUB_R,     secCol);
  tft.fillCircle((int)CX, (int)CY, HUB_R - 3, P.bg);
#else
  tft.fillCircle((int)CX, (int)CY, HUB_R - 1, P.line);
#endif

  prevHourAng = hourAng;
  prevMinAng  = minAng;
  prevSecAng  = secAng;
  havePrev    = true;
}

// ---------------------------------------------------------------------------

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) {
    tft.fillScreen(P.bg);
    drawTickRing();          // ngoai tam quet cua kim, chi ve mot lan
    havePrev = false;        // khong con kim cu tren man de xoa
  }

  const bool timeMoved =
#if ANALOG_SHOW_SECONDS
      (cur.second != prev.second) ||
#endif
      (cur.minute != prev.minute) || (cur.hour24 != prev.hour24);

  if (full || timeMoved || cur.day != prev.day || cur.month != prev.month) {
    drawHands(cur);
  }
}

const ScreenDef SCREEN_ANALOG = { "Analog", render };
