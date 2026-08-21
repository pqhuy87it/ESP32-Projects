#include "screen_info.h"
#include "hal.h"
#include "theme.h"
#include "icons.h"
#include "config.h"

// ============================================================================
// Layout — moi hang mot vung doc lap, chi ve lai khi du lieu cua no doi.
// ============================================================================
static constexpr int Y_CITY_TOP = 4,   Y_CITY_H = 32;
static constexpr int Y_WX_TOP   = 40,  Y_WX_H   = 46;
static constexpr int Y_DIV1     = 91;
static constexpr int Y_TIME_TOP = 96,  Y_TIME_H = 58;
static constexpr int Y_DIV2     = 158;
static constexpr int Y_DATE_TOP = 163, Y_DATE_H = 28;
static constexpr int Y_HUM_TOP  = 196, Y_HUM_H  = 40;

static constexpr int BADGE_W   = 44, BADGE_H = 26;
static constexpr int DIV_THICK = 3;

static const char* const WEEKDAY_FULL[7] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// ---------------------------------------------------------------------------

static void clearRow(int y, int h) {
  tft.fillRect(0, y, SCR_W, h, P.bg);
}

static void badge(int x, int y, int w, int h, const char* text, uint16_t fill) {
  tft.fillRoundRect(x, y, w, h, 5, fill);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(P.bg, fill);
  tft.drawString(text, x + w / 2, y + h / 2);
}

// Ve chu, tu ha co font neu qua rong
static void fitted(const char* text, int x, int yMid, int maxW,
                   uint16_t color, uint8_t bigFont, uint8_t smallFont) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, P.bg);
  tft.setTextFont(bigFont);
  if (tft.textWidth(text) > maxW) tft.setTextFont(smallFont);
  tft.drawString(text, x, yMid);
}

// ---------------------------------------------------------------------------

static void drawCityRow(const AppData& d) {
  clearRow(Y_CITY_TOP, Y_CITY_H);
  const int yMid = Y_CITY_TOP + Y_CITY_H / 2;

  const bool hasRegion = d.region[0] != '\0';
  const int  badgeX    = SCR_W - MARGIN - BADGE_W;
  const int  maxTextW  = (hasRegion ? badgeX - 8 : SCR_W - MARGIN) - MARGIN;

  fitted(d.city, MARGIN, yMid, maxTextW, d.online ? P.label : P.dim, 4, 2);

  if (hasRegion) {
    badge(badgeX, yMid - BADGE_H / 2, BADGE_W, BADGE_H, d.region, P.label);
  }
}

static void drawWeatherRow(const AppData& d) {
  clearRow(Y_WX_TOP, Y_WX_H);
  const int yMid    = Y_WX_TOP + Y_WX_H / 2;
  const int iconBox = 42;
  const int iconX   = SCR_W - MARGIN - iconBox;

  char num[8];
  snprintf(num, sizeof num, "%d", d.temp);

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(P.value, P.bg);
  tft.drawString(num, MARGIN, yMid);

  int cursor = MARGIN + tft.textWidth(num) + 5;
  iconDegreeRing(cursor + 3, yMid - tft.fontHeight() / 2 + 5, 3, P.value);
  cursor += 10;

  const char unit[2] = { TEMP_UNIT, '\0' };
  tft.drawString(unit, cursor, yMid);
  cursor += tft.textWidth(unit) + 10;

  tft.setTextColor(P.label, P.bg);
  if (cursor + tft.textWidth(d.condition) > iconX - 6) tft.setTextFont(2);
  tft.drawString(d.condition, cursor, yMid);

  iconWeather(d.icon, iconX, Y_WX_TOP + 2, iconBox, P.value);
}

static void drawTimeRow(const AppData& d) {
  clearRow(Y_TIME_TOP, Y_TIME_H);
  const int yMid   = Y_TIME_TOP + Y_TIME_H / 2;
  const int badgeX = SCR_W - MARGIN - BADGE_W;

  char clockStr[8];
  if (d.show12h) snprintf(clockStr, sizeof clockStr, "%d:%02d",   d.hourShown, d.minute);
  else           snprintf(clockStr, sizeof clockStr, "%02d:%02d", d.hourShown, d.minute);

  tft.setTextFont(6);                  // 48px, co chu so va dau ':'
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(P.title, P.bg);
  tft.drawString(clockStr, MARGIN, yMid);

  if (d.show12h) {
    badge(badgeX, yMid - BADGE_H / 2, BADGE_W, BADGE_H,
          d.isPM ? "PM" : "AM", P.title);
  }
}

static void drawDateRow(const AppData& d) {
  clearRow(Y_DATE_TOP, Y_DATE_H);
  const int yMid = Y_DATE_TOP + Y_DATE_H / 2;

  char dateStr[14];
  snprintf(dateStr, sizeof dateStr, "%d/%d/%02d", d.day, d.month, d.year2);
  const char* weekday = WEEKDAY_FULL[d.weekday % 7];

  tft.setTextFont(4);
  if (tft.textWidth(dateStr) + 14 + tft.textWidth(weekday) > SCR_W - 2 * MARGIN) {
    tft.setTextFont(2);
  }
  tft.setTextDatum(ML_DATUM);

  tft.setTextColor(P.value, P.bg);
  tft.drawString(dateStr, MARGIN, yMid);
  const int cursor = MARGIN + tft.textWidth(dateStr) + 14;

  tft.setTextColor(P.label, P.bg);
  tft.drawString(weekday, cursor, yMid);
}

static void drawHumidityRow(const AppData& d) {
  clearRow(Y_HUM_TOP, Y_HUM_H);
  const int yMid = Y_HUM_TOP + Y_HUM_H / 2;

  tft.fillRect(MARGIN + 4,         Y_HUM_TOP + 4, 2, Y_HUM_H - 8, P.dim);
  tft.fillRect(SCR_W - MARGIN - 6, Y_HUM_TOP + 4, 2, Y_HUM_H - 8, P.dim);

  char outVal[8], inVal[8];
  snprintf(outVal, sizeof outVal, "%u%%", d.humidityOut);
  snprintf(inVal,  sizeof inVal,  "%u%%", d.humidityIn);

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);

  const int wOutLbl = tft.textWidth("O: ");
  const int wOutVal = tft.textWidth(outVal);
  const int dropGap = 34;

  if (!d.hasHumidityIn) {
    int x = (SCR_W - (22 + wOutLbl + wOutVal)) / 2;
    iconDroplet(x + 8, yMid, 13, P.value);
    x += 22;
    tft.setTextColor(P.label, P.bg);
    tft.drawString("O: ", x, yMid);
    tft.setTextColor(P.value, P.bg);
    tft.drawString(outVal, x + wOutLbl, yMid);
    return;
  }

  const int wInLbl = tft.textWidth("I: ");
  const int wInVal = tft.textWidth(inVal);
  int x = (SCR_W - (wOutLbl + wOutVal + dropGap + wInLbl + wInVal)) / 2;

  tft.setTextColor(P.label, P.bg);
  tft.drawString("O: ", x, yMid);
  x += wOutLbl;
  tft.setTextColor(P.value, P.bg);
  tft.drawString(outVal, x, yMid);
  x += wOutVal;

  iconDroplet(x + dropGap / 2, yMid, 13, P.value);
  x += dropGap;

  tft.setTextColor(P.label, P.bg);
  tft.drawString("I: ", x, yMid);
  x += wInLbl;
  tft.setTextColor(P.value, P.bg);
  tft.drawString(inVal, x, yMid);
}

// ---------------------------------------------------------------------------

static void render(const AppData& cur, const AppData& prev, bool full) {
  if (full) {
    tft.fillScreen(P.bg);
    tft.fillRect(MARGIN, Y_DIV1, SCR_W - 2 * MARGIN, DIV_THICK, P.line);
    tft.fillRect(MARGIN, Y_DIV2, SCR_W - 2 * MARGIN, DIV_THICK, P.line);
  }

  if (full || cur.online != prev.online ||
      strcmp(cur.city, prev.city) != 0 ||
      strcmp(cur.region, prev.region) != 0)          drawCityRow(cur);

  if (full || cur.temp != prev.temp || cur.icon != prev.icon ||
      strcmp(cur.condition, prev.condition) != 0)    drawWeatherRow(cur);

  if (full || cur.hourShown != prev.hourShown ||
      cur.minute != prev.minute || cur.isPM != prev.isPM) drawTimeRow(cur);

  if (full || cur.day != prev.day || cur.month != prev.month ||
      cur.year2 != prev.year2 || cur.weekday != prev.weekday) drawDateRow(cur);

  if (full || cur.humidityOut != prev.humidityOut ||
      cur.humidityIn != prev.humidityIn ||
      cur.hasHumidityIn != prev.hasHumidityIn)       drawHumidityRow(cur);
}

const ScreenDef SCREEN_INFO = { "Info", render };
