#pragma once
#include "app_data.h"

// ============================================================================
// weather.h — lay thoi tiet tu OpenWeatherMap theo toa do lat/lon.
// ============================================================================

bool weatherFetch(AppData& d);          // thoi tiet hien tai
bool weatherFetchForecast(AppData& d);  // du bao theo gio (3 moc)
