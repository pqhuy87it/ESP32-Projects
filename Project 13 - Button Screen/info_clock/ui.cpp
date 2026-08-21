#include "ui.h"
#include "hal.h"
#include "theme.h"

#include "screen_info.h"
#include "screen_calendar.h"
#include "screen_weather.h"
#include "screen_lcd.h"
#include "screen_minimal.h"
#include "screen_colorful.h"
#include "screen_forecast.h"
#include "screen_analog.h"

// ============================================================================
// BANG DANG KY MAN HINH — them man hinh moi o day
// ============================================================================
static const ScreenDef* const SCREENS[] = {
  &SCREEN_INFO,
  &SCREEN_CALENDAR,
  &SCREEN_WEATHER,
  &SCREEN_LCD,
  &SCREEN_MINIMAL,
  &SCREEN_COLORFUL,
  &SCREEN_FORECAST,
  &SCREEN_ANALOG,
  // &SCREEN_XXX,        <-- man hinh tiep theo cua ban
};
static constexpr uint8_t SCREEN_N = sizeof(SCREENS) / sizeof(SCREENS[0]);

static uint8_t current  = 0;
static AppData prev;
static bool    needFull = true;

void uiBegin() {
  appDataInit(prev);
  current  = 0;
  needFull = true;
}

void uiNextScreen() {
  current  = (uint8_t)((current + 1) % SCREEN_N);
  needFull = true;
  Serial.printf("[ui] -> %s\n", SCREENS[current]->name);
}

void uiForceRedraw() { needFull = true; }

void uiRender(const AppData& cur) {
  const ScreenDef* s = SCREENS[current];

  // setTextSize dinh trang thai trong TFT_eSPI. Reset truoc moi lan ve de
  // man nay khong lam hong man khac.
  tft.setTextSize(1);

  s->render(cur, prev, needFull);

  prev     = cur;
  needFull = false;
}

const char* uiCurrentName() { return SCREENS[current]->name; }
uint8_t     uiScreenCount() { return SCREEN_N; }
