#include <WiFi.h>
#include <WiFiManager.h>
#include "net.h"
#include "config.h"
#include "settings.h"
#include "hal.h"

static WiFiManager wm;
static char apSsid[32] = {0};

const char* netApSsid() { return apSsid; }

// Ten AP kem 4 ky tu cuoi cua MAC de phan biet khi co nhieu thiet bi
static void buildApSsid() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apSsid, sizeof apSsid, "%s-%02X%02X",
           AP_SSID_PREFIX, mac[4], mac[5]);
}

// Hien thong tin portal len man hinh — nguoi dung khong can cam Serial
static void onPortalStart(WiFiManager* /*self*/) {
  Serial.printf("[net] portal: SSID=%s  ->  http://192.168.4.1\n", apSsid);
  halMessage("WiFi Setup", apSsid, "192.168.4.1");
}

static void onSaveParams() {
  strlcpy(S.apiKey, wm.server->arg("apikey").c_str(), sizeof S.apiKey);

  const String latStr = wm.server->arg("lat");
  const String lonStr = wm.server->arg("lon");
  if (latStr.length()) S.lat = latStr.toFloat();
  if (lonStr.length()) S.lon = lonStr.toFloat();

  settingsSave();
}

// Ba o nhap them vao trang cau hinh, canh duoi phan chon WiFi
static void addCustomParams() {
  static char latBuf[16], lonBuf[16];
  snprintf(latBuf, sizeof latBuf, "%.4f", S.lat);
  snprintf(lonBuf, sizeof lonBuf, "%.4f", S.lon);

  static WiFiManagerParameter pApiKey("apikey", "OpenWeatherMap API key",
                                      S.apiKey, sizeof(S.apiKey) - 1);
  static WiFiManagerParameter pLat("lat", "Latitude",  latBuf, 15);
  static WiFiManagerParameter pLon("lon", "Longitude", lonBuf, 15);

  wm.addParameter(&pApiKey);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
}

static void configureWm() {
  buildApSsid();

  wm.setAPCallback(onPortalStart);
  wm.setSaveParamsCallback(onSaveParams);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);   // tranh treo vo han
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  wm.setDarkMode(true);
  wm.setTitle("InfoClock");
  wm.setBreakAfterConfig(true);                  // luu xong la thoat portal

  // Chi hien nhung muc can thiet, bo bot menu cho gon tren dien thoai
  const char* menu[] = { "wifi", "param", "info", "sep", "erase", "restart" };
  wm.setMenu(menu, 6);

  addCustomParams();
}

bool netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);              // tranh tre khi goi API

  configureWm();

  const bool ok = wm.autoConnect(apSsid, AP_PASSWORD);
  Serial.printf("[net] %s\n", ok ? WiFi.localIP().toString().c_str()
                                 : "khong ket noi duoc");
  return ok;
}

bool netStartPortal() {
  Serial.println(F("[net] mo portal theo yeu cau"));
  const bool ok = wm.startConfigPortal(apSsid, AP_PASSWORD);
  Serial.printf("[net] portal dong, %s\n", ok ? "da ket noi" : "chua ket noi");
  return ok;
}

void netForgetWifi() {
  Serial.println(F("[net] xoa credential WiFi"));
  wm.resetSettings();
}

bool netOnline() {
  return WiFi.status() == WL_CONNECTED;
}
