#include "settings.h"
#include "config.h"
#include <Preferences.h>

static const char* NVS_NAMESPACE = "infoclock";

Settings S;

void settingsLoad() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);            // true = chi doc

  String key = prefs.getString("apikey", OWM_API_KEY);
  strlcpy(S.apiKey, key.c_str(), sizeof S.apiKey);
  S.lat = prefs.getFloat("lat", OWM_LAT);
  S.lon = prefs.getFloat("lon", OWM_LON);

  prefs.end();

  Serial.printf("[settings] lat=%.4f lon=%.4f apikey=%s\n",
                S.lat, S.lon,
                strlen(S.apiKey) > 8 ? "da co" : "CHUA DAT");
}

void settingsSave() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("apikey", S.apiKey);
  prefs.putFloat("lat", S.lat);
  prefs.putFloat("lon", S.lon);
  prefs.end();
  Serial.println(F("[settings] da luu vao NVS"));
}

void settingsClear() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  settingsLoad();
}
