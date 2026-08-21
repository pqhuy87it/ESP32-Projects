#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// ============================================================================
// hal.h — lop phan cung: man hinh, den nen, nut bam.
// Moi module khac dung `tft` qua day, khong tu tao doi tuong TFT_eSPI.
// ============================================================================

extern TFT_eSPI tft;

// Su kien duoc phan loai khi NHA TAY, nen moi lan nhan sinh dung MOT su kien.
enum class BtnEvent : uint8_t { None, Short, Long, VeryLong };

void     halBegin();                       // init man hinh + nut + den nen
void     halBacklight(uint8_t level);      // 0..255, vo hieu neu khong co TFT_BL
BtnEvent halPollButton();                  // khong blocking, goi lien tuc
void     halSplash(const char* msg);       // thong bao mot dong
void     halMessage(const char* line1, const char* line2, const char* line3);
