#pragma once
#include <Arduino.h>
#include "app_data.h"

// ============================================================================
// icons.h — ve icon. Uu tien bitmap tu weather_icons.h neu file do ton tai,
// khong thi ve bang code (fillCircle / fillRect / fillTriangle).
//
// Sinh weather_icons.h bang:
//   python3 png_to_rgb565.py --size 40 --out weather_icons.h icon_*.png
// ============================================================================

// Ve icon tren nen P.bg
void iconWeather(WxIcon icon, int x, int y, int box, uint16_t color);

// Ve icon tren mot nen BAT KY — bat buoc dung ham nay khi o icon nam tren
// the mau hoac khung sang, neu khong bitmap se keo theo mot o den quanh no.
void iconWeatherOn(WxIcon icon, int x, int y, int box, uint16_t color, uint16_t bg);
void iconDroplet(int cx, int cy, int w, uint16_t color);
void iconDegreeRing(int cx, int cy, int r, uint16_t color);

// Chan doan: tra ve 0 neu bo icon tuong ung khong duoc nap
int  iconsBigSize();
int  iconsSmallSize();
bool iconsUsingBitmaps();
