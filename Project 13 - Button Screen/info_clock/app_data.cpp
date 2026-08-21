#include "app_data.h"
#include "config.h"

void appDataInit(AppData& d) {
  memset(&d, 0, sizeof(AppData));
  strlcpy(d.city,      CITY_FALLBACK, sizeof d.city);
  strlcpy(d.condition, "--",          sizeof d.condition);
  d.icon      = WxIcon::Cloudy;
  d.day       = 1;
  d.month     = 1;
  d.show12h   = (USE_12H_CLOCK != 0);
  d.online    = false;
  d.timeValid = false;
}
