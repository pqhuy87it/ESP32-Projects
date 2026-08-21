#pragma once
#include "app_data.h"

// ============================================================================
// demo.h — du lieu gia de lam giao dien khi khong co WiFi.
// Bat bang USE_DEMO_DATA 1 trong config.h.
// Phut tu tang, icon xoay vong qua ca 8 loai, ngay tu nhay de kiem tra
// viec ve lai vung dirty.
// ============================================================================

void demoInit(AppData& d);
void demoTick(AppData& d);
