#include "screen_weather.h"
#include "hal.h"
#include "theme.h"
#include "icons.h"
#include "config.h"

// ============================================================================
// Layout (240x240)
//
//        12:41            <- gio lon, Font 8 (75px), can giua
//         FRI             <- thu viet tat, trang
//       16 June           <- ngay + ten thang, xam
//   ~~~~~~~~~~~~~~~~~     <- duong cong vong len
//   [icon] : 9AM 1PM 8PM  <- trai: thoi tiet hien tai
//    79'   :  o   o   o      phai: du bao 3 moc
// ============================================================================

static constexpr int Y_TIME_MID = 46;      // tam hang gio
static constexpr int Y_WDAY_MID = 100;     // "FRI"
static constexpr int Y_DATE_MID = 121;     // "16 June"

static constexpr int ARC_EDGE_Y = 152;     // do cao duong cong o hai mep
static constexpr int ARC_APEX_Y = 137;     // do cao o dinh (giua man)
static constexpr int ARC_THICK  = 2;

static constexpr int BOT_TOP    = 156;
static constexpr int BOT_BOT    = 236;

static constexpr int LEFT_W     = 99;      // cot thoi tiet hien tai
static constexpr int LEFT_CX    = LEFT_W / 2;
static constexpr int SEP_X      = 103;     // vach cham doc

static constexpr int FC_COL_W   = 44;      // be rong mot cot du bao
static constexpr int FC_X0      = 128;     // tam cot du bao dau tien

static constexpr int CUR_ICON   = 34;      // o icon thoi tiet hien tai
static constexpr int CUR_ICON_CY = 174;
static constexpr int CUR_TEMP_CY = 214;

static constexpr int FC_LABEL_CY = 170;
static constexpr int FC_ICON     = 30;
static constexpr int FC_ICON_CY  = 206;

static const char* const WDAY_SHORT[7] = {
  "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

static const char* const MONTH_NAME[12] = {
  "January", "February", "March",     "April",   "May",      "June",
  "July",    "August",   "September", "October", "November", "December"
};

// ---------------------------------------------------------------------------

// Duong cong parabol vong len. Ve tung cot doc nen khong bi rang cua.
static void drawArc(uint16_t color) {
  const float half = SCR_W / 2.0f;
  const float k    = (float)(ARC_EDGE_Y - ARC_APEX_Y) / (half * half);

  for (int x = 0; x < SCR_W; x++) {
    const float dx = x - half;
    const int   y  = ARC_APEX_Y + (int)(k * dx * dx);
    tft.fillRect(x, y, 1, ARC_THICK, color);
  }
}

// Vach doc dang cham, ngan thoi tiet hien tai voi du bao
static void drawDottedSeparator(uint16_t color) {
  for (int y = BOT_TOP + 4; y < BOT_BOT - 4; y += 7) {
    tft.fillRect(SEP_X, y, 2, 3, color);
  }
}

// ---------------------------------------------------------------------------

static void drawTimeRow(const AppData& d) {
  tft.fillRect(0, 6, SCR_W, 80, P.bg);

  char buf[8];
  if (d.show12h) snprintf(buf, sizeof buf, "%d:%02d",   d.hourShown, d.minute);
  else           snprintf(buf, sizeof buf, "%02d:%02d", d.hourShown, d.minute);

  tft.setTextFont(8);                 // 75px — chi chu so va dau ':'
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.line, P.bg);     // trang, giong anh mau
  tft.drawString(buf, SCR_W / 2, Y_TIME_MID);
}

static void drawDateRows(const AppData& d) {
  tft.fillRect(0, 90, SCR_W, 42, P.bg);

  tft.setTextDatum(MC_DATUM);

  tft.setTextFont(2);
  tft.setTextColor(P.line, P.bg);                 // thu: trang
  tft.drawString(WDAY_SHORT[d.weekday % 7], SCR_W / 2, Y_WDAY_MID);

  char buf[24];
  snprintf(buf, sizeof buf, "%u %s", d.day, MONTH_NAME[(d.month - 1) % 12]);
  tft.setTextFont(4);
  tft.setTextColor(P.muted, P.bg);                // ngay thang: xam
  tft.drawString(buf, SCR_W / 2, Y_DATE_MID);
}

static void drawCurrentWeather(const AppData& d) {
  tft.fillRect(0, BOT_TOP, LEFT_W, BOT_BOT - BOT_TOP, P.bg);

  iconWeather(d.icon, LEFT_CX - CUR_ICON / 2, CUR_ICON_CY - CUR_ICON / 2,
              CUR_ICON, P.value);

  char num[8];
  snprintf(num, sizeof num, "%d", d.temp);

  tft.setTextFont(6);                 // 48px
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.line, P.bg);

  // Can giua cum "so + vong tron do" chu khong chi rieng con so
  const int wNum  = tft.textWidth(num);
  const int total = wNum + 12;
  const int left  = LEFT_CX - total / 2;

  tft.setTextDatum(ML_DATUM);
  tft.drawString(num, left, CUR_TEMP_CY);
  iconDegreeRing(left + wNum + 6, CUR_TEMP_CY - tft.fontHeight() / 2 + 7,
                 4, P.line);
}

static void drawForecast(const AppData& d) {
  tft.fillRect(SEP_X + 4, BOT_TOP, SCR_W - SEP_X - 4, BOT_BOT - BOT_TOP, P.bg);

  if (!d.hasForecast) {
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(P.dim, P.bg);
    tft.drawString("no forecast", (SEP_X + SCR_W) / 2, (BOT_TOP + BOT_BOT) / 2);
    return;
  }

  for (uint8_t i = 0; i < FORECAST_SLOTS; i++) {
    const int cx = FC_X0 + i * FC_COL_W;

    char label[8];
    snprintf(label, sizeof label, "%u%s",
             d.forecast[i].hour12, d.forecast[i].isPM ? "PM" : "AM");

    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(P.muted, P.bg);
    tft.drawString(label, cx, FC_LABEL_CY);

    iconWeather(d.forecast[i].icon, cx - FC_ICON / 2, FC_ICON_CY - FC_ICON / 2,
                FC_ICON, P.value);
  }
}

// ---------------------------------------------------------------------------

static bool forecastChanged(const AppData& a, const AppData& b) {
  if (a.hasForecast != b.hasForecast) return true;
  for (uint8_t i = 0; i < FORECAST_SLOTS; i++) {
    if (a.forecast[i].hour12 != b.forecast[i].hour12 ||
        a.forecast[i].isPM   != b.forecast[i].isPM   ||
        a.forecast[i].icon   != b.forecast[i].icon) return true;
  }
  return false;
}

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) {
    tft.fillScreen(P.bg);
    drawArc(P.line);
    drawDottedSeparator(P.dim);
  }

  if (full || cur.hourShown != prev.hourShown ||
      cur.minute != prev.minute || cur.isPM != prev.isPM) drawTimeRow(cur);

  if (full || cur.day != prev.day || cur.month != prev.month ||
      cur.weekday != prev.weekday)                        drawDateRows(cur);

  if (full || cur.temp != prev.temp || cur.icon != prev.icon) drawCurrentWeather(cur);

  if (full || forecastChanged(cur, prev))                    drawForecast(cur);
}

const ScreenDef SCREEN_WEATHER = { "Weather", render };
