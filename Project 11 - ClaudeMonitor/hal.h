#pragma once
#include "config.h"     // Để BOARD_* macro có sẵn trước khi chọn thư viện.
                        // (Trong main.cpp, hal.h được include trước config.h, nên
                        //  nếu không có dòng này, BOARD_TDISPLAY_S3 sẽ chưa được define
                        //  khi khối #ifdef bên dưới được đánh giá.)
#include <stdint.h>

#include <TFT_eSPI.h>
extern TFT_eSPI lcd;

void halInit();
void halUpdate();
bool halBtnAWasPressed();
bool halBtnBWasPressed();
bool halBtnAIsPressed();
bool halBtnBIsPressed();
int  halBatPercent();
void halSetBrightness(uint8_t level);
void halFlush();
void halClear(uint16_t color);
