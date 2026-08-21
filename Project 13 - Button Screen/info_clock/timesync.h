#pragma once
#include "app_data.h"

// ============================================================================
// timesync.h — NTP va doc gio he thong vao AppData.
// ============================================================================

void timeBegin();                          // cau hinh NTP + mui gio
bool timeWaitValid(uint32_t timeoutMs);     // cho den khi NTP tra ve gio hop le
void timeSync(AppData& d);                  // doc gio he thong vao struct
