#pragma once
#include "config.h"
#ifdef MANGO_UI
#include <stdint.h>

struct MoonData {
    float  ageDays;        // tuổi trăng (ngày, có phần thập phân — vd 21.8)
    int    imageIndex;     // 0..30 — chọn ảnh m-phase-N
    int    illumPct;       // % chiếu sáng 0..100
    const char* phaseName; // tên pha (New Moon, Waxing Crescent, ...)
    // Giờ mọc/lặn trăng (giờ địa phương). valid=false nếu không xảy ra trong ngày.
    int    riseH, riseM;   bool riseValid;
    int    setH,  setM;    bool setValid;
    // Ngày hiện tại (giờ địa phương) để hiển thị "April 9, 2026".
    int    year, month, day;
};

// Tính pha trăng + mọc/lặn cho thời điểm epoch (giây, UTC), với múi giờ offset (giây).
void computeMoon(long epoch, int tzOffsetSec, MoonData& out);

#endif // MANGO_UI
