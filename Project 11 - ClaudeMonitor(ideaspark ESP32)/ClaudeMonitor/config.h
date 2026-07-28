#pragma once

// ── Board selection (Arduino IDE — thay cho build_flags trong platformio.ini) ──
// Trên PlatformIO các macro này đặt qua build_flags. Arduino IDE không có cơ chế đó,
// nên ta define trực tiếp ở đây. Vì gần như mọi file đều #include "config.h",
// macro sẽ có mặt ở mọi translation unit.
#define BOARD_IDEASPARK_ESP32_19

// ideaspark ESP32 1.9" dùng đúng panel 170x320 (landscape 320x170) như T-Display S3,
// nên define luôn BOARD_TDISPLAY_S3 để TÁI DÙNG toàn bộ layout Mango trong ui.cpp
// (SCREEN_W/H/ROT, hàng 4 mascot, geometry...) — không phải thêm nhánh #ifdef mới.
// Khác biệt thật sự chỉ nằm ở hal.cpp: chân LCD, chân nút, backlight, pin.
// => Trong hal.cpp, nhánh BOARD_IDEASPARK_ESP32_19 phải được kiểm TRƯỚC
//    nhánh BOARD_TDISPLAY_S3, vì cả hai macro đều được define.
#ifdef BOARD_IDEASPARK_ESP32_19
  #define BOARD_TDISPLAY_S3
  #define HAS_BATTERY 0     // board không có mạch sạc / chia áp pin
  #include "board_ideaspark19.h"
#endif

#define MANGO_UI          // T-Display S3 (320x170) đủ chỗ cho Mango dashboard.
                          // Bỏ comment dòng này (xóa nó) nếu muốn UI "Clarity" tối giản.

// ── Firmware version ─────────────────────────────────────
#define FW_VERSION              "2.1.1"  // Mango — shown on the Mango boot screen

// ── Polling ──────────────────────────────────────────────
#define DEFAULT_POLL_SEC        120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC            300

// ── Security ─────────────────────────────────────────────
#define MAX_PIN_ATTEMPTS        10
#define LOCKOUT_BASE_SEC        60       // doubles each failure
#define KDF_ROUNDS              10000

// ── Display ──────────────────────────────────────────────
#ifdef BOARD_ESP32C3_OLED
  #define SCREEN_W              72
  #define SCREEN_H              40
  // No SCREEN_ROT — U8g2 uses U8G2_R0
#elif defined(BOARD_TDISPLAY_S3)
  #define SCREEN_W              320
  #define SCREEN_H              170
  #define SCREEN_ROT            1
#elif defined(BOARD_TDISPLAY_S3_AMOLED)
  #define SCREEN_W              536
  #define SCREEN_H              240
  #define SCREEN_ROT            0
#elif defined(BOARD_TDISPLAY_ESP32)
  #define SCREEN_W              240
  #define SCREEN_H              135
  #define SCREEN_ROT            3
#elif defined(BOARD_T8_S2)
  #define SCREEN_W              240
  #define SCREEN_H              135
  #define SCREEN_ROT            3
#elif defined(BOARD_CROWPANEL_ADV_35)
  #define SCREEN_W              480
  #define SCREEN_H              320
  #define SCREEN_ROT            3
#else
  #define SCREEN_W              240
  #define SCREEN_H              135
  #define SCREEN_ROT            3
#endif
#define DEFAULT_BRIGHTNESS      2        // 0=off 1=dim 2=normal 3=bright

// ── Network ──────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_S  20
#define API_TIMEOUT_MS          15000
#define MESSAGES_ENDPOINT       "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION       "2023-06-01"
#define PROBE_MODEL             "claude-haiku-4-5-20251001"
// status.anthropic.com redirects here — query the canonical host directly
#define STATUS_ENDPOINT         "https://status.claude.com/api/v2/incidents/unresolved.json"

// ── NVS ──────────────────────────────────────────────────
#define NVS_NAMESPACE           "claude"

// ── Feature flags ────────────────────────────────────────
// MANGO_UI — the "Mango" dashboard: model-status mascots from status.claude.com,
//   battery + WiFi-signal icons in the header, dashboard-styled PIN screen, and
//   Button A = flip screen / Button B = brightness. Enabled on boards whose
//   panels have vertical room: BOARD_TDISPLAY_S3 (320x170) and
//   BOARD_M5STICK_C_PLUS (240x135). Mango-specific geometry branches on the board.
