#pragma once
#include "app_data.h"

// ============================================================================
// ui.h — quan ly nhieu man hinh.
//
// ~~~~~~~~~~~~~~~~ CACH THEM MOT MAN HINH MOI ~~~~~~~~~~~~~~~~
// 1. Tao screen_xxx.h:
//        #pragma once
//        #include "ui.h"
//        extern const ScreenDef SCREEN_XXX;
//
// 2. Tao screen_xxx.cpp:
//        static void render(const AppData& cur, const AppData& prev, bool full) {
//          if (full) tft.fillScreen(P.bg);
//          if (full || cur.minute != prev.minute) { ... }
//        }
//        const ScreenDef SCREEN_XXX = { "Xxx", render };
//
// 3. Them vao bang SCREENS[] trong ui.cpp — MOT dong.
//
// Nut bam se tu dong xoay vong qua man hinh moi.
// ============================================================================

struct ScreenDef {
  const char* name;
  // full = true  -> ve lai tat ca, ke ca nen (khi vua vao man hoac doi mau)
  // full = false -> chi ve lai phan khac giua cur va prev
  void (*render)(const AppData& cur, const AppData& prev, bool full);
};

void        uiBegin();
void        uiNextScreen();          // chuyen sang man ke tiep, ve lai tu dau
void        uiForceRedraw();         // danh dau can ve lai toan bo o lan sau
void        uiRender(const AppData& cur);
const char* uiCurrentName();
uint8_t     uiScreenCount();

// Kich thuoc man hinh, dung chung cho moi screen
static constexpr int SCR_W  = 240;
static constexpr int SCR_H  = 240;
static constexpr int MARGIN = 10;
