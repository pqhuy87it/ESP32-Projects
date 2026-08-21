#pragma once
#include <Arduino.h>

// ============================================================================
// settings.h — tham so nguoi dung LUU TRONG FLASH (NVS).
//
// Khac voi config.h: config.h la gia tri MAC DINH bien dich san, con file nay
// la gia tri thuc te dang dung, sua duoc tu trang cau hinh WiFi ma khong can
// nap lai firmware.
// ============================================================================

struct Settings {
  char  apiKey[40];      // OpenWeatherMap API key
  float lat;
  float lon;
};

extern Settings S;

void settingsLoad();      // doc tu NVS, thieu thi lay mac dinh trong config.h
void settingsSave();      // ghi xuong NVS
void settingsClear();     // xoa, tro ve mac dinh
