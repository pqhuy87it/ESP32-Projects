#include "screen_forecast.h"
#include "hal.h"
#include "theme.h"
#include "icons.h"
#include "config.h"

// ============================================================================
// Man hinh du bao 3 ngay
//
//   05:13 AM              Fri 4 Oct     <- thanh tren: gio | ngay
//   +--------------------------------+
//   | Fri                    23% (o) |  <- ten thu | do am
//   | (*) 17'    v11'      ^23'       |  <- icon+nhiet do | thap nhat | cao nhat
//   +--------------------------------+
//   | Sat                    45% (o) |
//   | (~) 15'     v9'      ^22'       |
//   +--------------------------------+
//   | Sun                    42% (o) |
//   | (c) 14'     v9'      ^20'       |
//   +--------------------------------+
//
// Mau nen cua the doi theo tinh trang thoi tiet — nang thi am, mua thi
// tim tham, nhieu may thi xam xanh. Nho vay nhin mot cai la biet ngay.
//
// Mui ten len/xuong khong co trong font cua TFT_eSPI nen duoc ve bang
// fillTriangle + fillRect.
// ============================================================================

static constexpr int BAR_CY   = 15;                  // thanh gio/ngay
static constexpr int BAR_H    = 24;

static constexpr int CARD_X   = 6;
static constexpr int CARD_W   = SCR_W - 2 * CARD_X;  // 228
static constexpr int CARD_H   = 64;
static constexpr int CARD_GAP = 7;
static constexpr int CARD_Y0  = 30;
static constexpr int CARD_R   = 10;

static constexpr int PAD_L    = 12;                  // le trong the
static constexpr int ROW_A_DY = 18;                  // tam hang tren, tinh tu dinh the
static constexpr int ROW_B_DY = 45;                  // tam hang duoi

static constexpr int WX_ICON_BOX = 22;
static constexpr int DROP_W      = 12;
static constexpr int ARROW_H     = 13;

static const char* const WDAY_SHORT[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* const MONTH_ABBR[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// ---------------------------------------------------------------------------
// Mau nen the theo tinh trang thoi tiet
// ---------------------------------------------------------------------------
static uint16_t cardColor(WxIcon icon) {
  uint8_t r, g, b;
  switch (icon) {
    case WxIcon::Clear:        r = 168; g = 120; b =  58; break;  // am, nang
    case WxIcon::PartlyCloudy: r = 132; g = 112; b =  78; break;  // am nhat
    case WxIcon::Cloudy:       r =  80; g =  90; b =  98; break;  // xam xanh
    case WxIcon::Rain:         r =  46; g =  40; b =  86; break;  // tim tham
    case WxIcon::Storm:        r =  38; g =  32; b =  70; break;  // tim rat tham
    case WxIcon::Snow:         r =  96; g = 108; b = 124; break;  // xanh lanh
    case WxIcon::Fog:          r =  72; g =  76; b =  78; break;  // xam duc
    case WxIcon::NightClear:   r =  38; g =  44; b =  74; break;  // xanh dem
    case WxIcon::Wind:         r =  56; g =  92; b = 100; break;  // xanh loc
    default:                   r =  70; g =  74; b =  80; break;
  }
  if (themeIsNight()) { r /= 2; g /= 2; b /= 2; }
  return rgb565(r, g, b);
}

// ---------------------------------------------------------------------------
// Mui ten — font TFT_eSPI khong co ky tu nay
// ---------------------------------------------------------------------------
static void drawArrowDown(int cx, int cy, int h, uint16_t c) {
  const int half = h / 2;
  const int w    = h / 2;
  tft.fillRect(cx - 1, cy - half, 3, h - w, c);                    // than
  tft.fillTriangle(cx - w / 2 - 1, cy + half - w,
                   cx + w / 2 + 1, cy + half - w,
                   cx,             cy + half, c);                  // dau
}

static void drawArrowUp(int cx, int cy, int h, uint16_t c) {
  const int half = h / 2;
  const int w    = h / 2;
  tft.fillRect(cx - 1, cy - half + w, 3, h - w, c);
  tft.fillTriangle(cx - w / 2 - 1, cy - half + w,
                   cx + w / 2 + 1, cy - half + w,
                   cx,             cy - half, c);
}

// Ve "<so>'" voi vong tron do, tra ve be rong da chiem
static int drawTemp(int x, int yMid, int value, uint16_t fg, uint16_t bg) {
  char buf[8];
  snprintf(buf, sizeof buf, "%d", value);

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(buf, x, yMid);

  const int w = tft.textWidth(buf);
  iconDegreeRing(x + w + 5, yMid - tft.fontHeight() / 2 + 5, 3, fg);
  return w + 11;
}

static int tempWidth(int value) {
  char buf[8];
  snprintf(buf, sizeof buf, "%d", value);
  tft.setTextFont(4);
  return tft.textWidth(buf) + 11;
}

// ---------------------------------------------------------------------------

static void drawTopBar(const AppData& d) {
  tft.fillRect(0, BAR_CY - BAR_H / 2, SCR_W, BAR_H, P.bg);

  char timeStr[12];
  if (d.show12h) {
    snprintf(timeStr, sizeof timeStr, "%02u:%02u %s",
             d.hourShown, d.minute, d.isPM ? "PM" : "AM");
  } else {
    snprintf(timeStr, sizeof timeStr, "%02u:%02u", d.hourShown, d.minute);
  }

  char dateStr[20];
  snprintf(dateStr, sizeof dateStr, "%s %u %s",
           WDAY_SHORT[d.weekday % 7], d.day, MONTH_ABBR[(d.month - 1) % 12]);

  tft.setTextFont(2);
  tft.setTextColor(P.line, P.bg);

  tft.setTextDatum(ML_DATUM);
  tft.drawString(timeStr, CARD_X + 2, BAR_CY);

  tft.setTextDatum(MR_DATUM);
  tft.drawString(dateStr, SCR_W - CARD_X - 2, BAR_CY);
}

static void drawCard(const DailyForecast& day, int y) {
  if (!day.valid) {
    tft.fillRoundRect(CARD_X, y, CARD_W, CARD_H, CARD_R, P.dim);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(P.muted, P.dim);
    tft.drawString("no data", SCR_W / 2, y + CARD_H / 2);
    return;
  }

  const uint16_t bg = cardColor(day.icon);
  const uint16_t fg = P.line;                    // chu trang tren moi mau the

  tft.fillRoundRect(CARD_X, y, CARD_W, CARD_H, CARD_R, bg);

  // ----- Hang tren: ten thu (trai) | do am + giot nuoc (phai) -----
  const int yA = y + ROW_A_DY;

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(day.wday, CARD_X + PAD_L, yA);

  char hum[8];
  snprintf(hum, sizeof hum, "%u%%", day.humidity);
  const int dropCx = CARD_X + CARD_W - PAD_L - DROP_W / 2;
  iconDroplet(dropCx, yA, DROP_W, fg);

  tft.setTextDatum(MR_DATUM);
  tft.drawString(hum, dropCx - DROP_W / 2 - 4, yA);

  // ----- Hang duoi: icon + nhiet do | thap nhat | cao nhat -----
  const int yB = y + ROW_B_DY;

  // The co nen mau -> phai truyen mau nen vao, neu khong bitmap se keo theo
  // mot o den quanh icon
  iconWeatherOn(day.icon, CARD_X + PAD_L - 2, yB - WX_ICON_BOX / 2,
                WX_ICON_BOX, fg, bg);
  drawTemp(CARD_X + PAD_L + WX_ICON_BOX + 2, yB, day.tempDay, fg, bg);

  // Cao nhat: neo phai
  const int wMax = tempWidth(day.tempMax);
  const int xMax = CARD_X + CARD_W - PAD_L - wMax;
  drawArrowUp(xMax - 8, yB, ARROW_H, fg);
  drawTemp(xMax, yB, day.tempMax, fg, bg);

  // Thap nhat: giua hai cum trai va phai
  const int wMin  = tempWidth(day.tempMin);
  const int xMin  = CARD_X + CARD_W / 2 - wMin / 2 + 4;
  drawArrowDown(xMin - 8, yB, ARROW_H, fg);
  drawTemp(xMin, yB, day.tempMin, fg, bg);
}

// ---------------------------------------------------------------------------

static bool dailyChanged(const AppData& a, const AppData& b) {
  if (a.hasDaily != b.hasDaily) return true;
  for (uint8_t i = 0; i < DAILY_SLOTS; i++) {
    const DailyForecast& x = a.daily[i];
    const DailyForecast& y = b.daily[i];
    if (x.valid    != y.valid    || x.icon     != y.icon     ||
        x.tempDay  != y.tempDay  || x.tempMin  != y.tempMin  ||
        x.tempMax  != y.tempMax  || x.humidity != y.humidity ||
        strcmp(x.wday, y.wday) != 0) return true;
  }
  return false;
}

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) tft.fillScreen(P.bg);

  if (full || cur.hourShown != prev.hourShown || cur.minute != prev.minute ||
      cur.isPM != prev.isPM || cur.day != prev.day ||
      cur.month != prev.month || cur.weekday != prev.weekday) drawTopBar(cur);

  if (full || dailyChanged(cur, prev)) {
    for (uint8_t i = 0; i < DAILY_SLOTS; i++) {
      drawCard(cur.daily[i], CARD_Y0 + i * (CARD_H + CARD_GAP));
    }
  }
}

const ScreenDef SCREEN_FORECAST = { "Forecast", render };
