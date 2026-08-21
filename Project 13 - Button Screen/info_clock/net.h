#pragma once
#include <Arduino.h>

// ============================================================================
// net.h — quan ly WiFi bang WiFiManager (tzapu).
//
// Hanh vi:
//   netBegin()  -> thu credential da luu. Khong duoc thi mo AP captive portal,
//                  cho nguoi dung chon WiFi va nhap API key / toa do.
//   Portal tu dong tat sau PORTAL_TIMEOUT_S giay de thiet bi khong treo.
//
// Thu vien: WiFiManager (tzapu) >= 2.0.17  — cai qua Library Manager
// ============================================================================

bool netBegin();               // goi trong setup()
bool netOnline();
bool netStartPortal();         // mo portal theo yeu cau (giu nut 6 giay)
void netForgetWifi();          // xoa credential WiFi da luu
const char* netApSsid();       // ten AP cua portal, de hien tren man hinh
