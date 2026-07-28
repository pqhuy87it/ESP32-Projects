// ─────────────────────────────────────────────────────────────
// board_ideaspark19.h
// ideaspark ESP32 Dev Board + 1.9" ST7789 170x320
// (ESP32-WROOM-32, 16MB flash, CH340, USB-C)
//
// Được config.h include khi BOARD_IDEASPARK_ESP32_19 được define.
// ─────────────────────────────────────────────────────────────
#pragma once

// ── Display: ST7789 170x320, SPI, đã đi dây cứng trên board ──
// Các chân này chỉ để tham chiếu / dùng cho backlight. TFT_eSPI lấy chân
// từ User_Setup_ideaspark19.h chứ không đọc từ đây.
#define LCD_MOSI        23
#define LCD_SCLK        18
#define LCD_CS          15
#define LCD_DC           2
#define LCD_RST          4
#define LCD_BLK         32      // backlight — hal.cpp điều khiển bằng LEDC PWM

// ── Buttons ─────────────────────────────────────────────────
// Board KHÔNG có user button, chỉ có BOOT (GPIO0) và EN/RST.
//
// Mặc định: 2 nút ngoài, active-LOW nối thẳng xuống GND, dùng pull-up nội bộ.
//   Button A → GPIO 27      Button B → GPIO 26
//
// Không muốn hàn nút? Bỏ comment IDEASPARK_SINGLE_BUTTON để dùng nút BOOT:
//   short press = Button A, long press (≥600ms) = Button B.
//   Đánh đổi: mất tổ hợp A+B (force refresh của Mango) và không thể giữ
//   A+B lúc cấp nguồn để factory reset — giữ GPIO0 khi boot là download mode.
//   Bù lại, main.cpp đổi sang "giữ BOOT 5 s trong 5 s đầu sau khi boot".
//
#define IDEASPARK_SINGLE_BUTTON

#ifdef IDEASPARK_SINGLE_BUTTON
  #define PIN_BTN_BOOT   0
  #define LONGPRESS_MS   600
#else
  #define PIN_BTN_A     27
  #define PIN_BTN_B     26
#endif

// ── Backlight PWM ───────────────────────────────────────────
#define BL_PWM_FREQ   5000
#define BL_PWM_BITS      8
#define BL_PWM_CHANNEL   0      // chỉ dùng trên Arduino-ESP32 core 2.x

// ── GPIO map ────────────────────────────────────────────────
// Đã dùng / phải tránh:
//   2, 4, 15, 18, 23, 32   → LCD
//   0, 12                  → strapping pin (BOOT / MTDI)
//   6..11                  → SPI flash nội bộ
//   34, 35, 36, 39         → input-only, KHÔNG có pull-up nội bộ
// Còn rảnh và an toàn cho nút / I2C:
//   13, 14, 16, 17, 19, 21, 22, 25, 26, 27, 33
