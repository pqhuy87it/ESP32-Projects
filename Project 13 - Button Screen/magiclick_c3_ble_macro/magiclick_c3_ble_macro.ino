/*
 * MagiClick C3 — nút macro BLE cho macOS
 * ------------------------------------------------------------------
 * Phần cứng : ESP32-C3 SuperMini + màn ST7789 240x240 1.3" + 1 switch cơ
 * Cách hoạt động:
 *   - Nhấn ngắn  -> gửi hotkey "vô hình" qua BLE HID
 *   - Giữ 2 giây -> xoá bond và khởi động lại để ghép nối lại từ đầu
 *
 * Yêu cầu thư viện:
 *   - arduino-esp32 core >= 3.x
 *   - NimBLE-Arduino    >= 2.3.8
 *   - HijelHID_BLEKeyboard
 *   - LovyanGFX
 *
 * Bên phía macOS: bind hotkey MACRO_KEY + MACRO_MODIFIERS trong
 * Raycast / Shortcuts.app / Keyboard Maestro để mở app hoặc chạy workflow.
 */

#include <LovyanGFX.hpp>
#include <HijelHID_BLEKeyboard.h>

// ==================================================================
// Cấu hình — chỉ cần sửa trong khối này
// ==================================================================

// Chân nối, khớp với sơ đồ đấu dây.
// SCLK/MOSI dùng đúng chân IO_MUX của SPI2 trên C3 (FSPICLK = GPIO6,
// FSPID = GPIO7) để có biên an toàn khi chạy SPI tốc độ cao.
// Nếu module có chân CS thì dùng GPIO10 (FSPICS0).
static constexpr int PIN_SCLK = 6;
static constexpr int PIN_MOSI = 7;
static constexpr int PIN_DC   = 5;
static constexpr int PIN_RST  = 4;
static constexpr int PIN_BL   = 3;
static constexpr int PIN_SW   = 0;   // switch cơ, chân còn lại xuống GND
                                     // GPIO0 an toàn trên C3 (chân boot là GPIO9)
                                     // Tránh GPIO2, GPIO8, GPIO9 — strapping pin

// Hotkey gửi sang macOS.
// Hyper = Ctrl + Option + Command: không có app hệ thống nào dùng tổ hợp này.
// LƯU Ý: nếu compile báo KEY_F13 không tồn tại, mở src/BLEHIDKeys.h của thư
// viện để xem danh sách phím thật, rồi đổi sang KEY_M hoặc một phím chữ khác.
#define MACRO_KEY        KEY_F13
#define MACRO_MODIFIERS  (KEY_MOD_LCTRL | KEY_MOD_LALT | KEY_MOD_LGUI)

static const char* DEVICE_NAME = "MagiClick C3";
static const char* MACRO_LABEL = "Open Xcode";   // chữ hiển thị trên màn

// Thời gian (ms)
static constexpr uint32_t DEBOUNCE_MS      = 30;
static constexpr uint32_t LONG_PRESS_MS    = 2000;
static constexpr uint32_t FEEDBACK_MS      = 600;    // hiện "SENT" bao lâu
static constexpr uint32_t DIM_AFTER_MS     = 20000;  // giảm sáng sau 20s
static constexpr uint32_t BLANK_AFTER_MS   = 60000;  // tắt đèn nền sau 60s

static constexpr uint8_t BRIGHT_FULL = 200;
static constexpr uint8_t BRIGHT_DIM  = 40;

// ==================================================================
// Cac kieu du lieu — PHAI khai bao truoc ham dau tien trong file .ino
// Arduino builder chen prototype len dau file; enum nam duoi se gay
// loi "variable or field declared void".
// ==================================================================
enum class Screen      { Advertising, Ready, Sent, Unpairing };
enum class ButtonEvent { None, ShortPress, LongPress };

// ==================================================================
// Driver màn hình
// ==================================================================

class Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI      bus_;
  lgfx::Light_PWM    light_;

public:
  Display() {
    {
      auto cfg = bus_.config();
      cfg.spi_host    = SPI2_HOST;   // C3 chỉ có SPI2 dùng được cho ngoại vi
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;    // chạy ổn rồi hãy nâng lên 80000000
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = true;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_SCLK;
      cfg.pin_mosi    = PIN_MOSI;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = PIN_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs          = -1;      // module 7 chân không có CS
      cfg.pin_rst         = PIN_RST;
      cfg.pin_busy        = -1;
      cfg.panel_width     = 240;
      cfg.panel_height    = 240;
      cfg.offset_x        = 0;       // nếu ảnh lệch ngang, thử 0 hoặc 80
      cfg.offset_y        = 0;       // nếu ảnh lệch dọc, thử 0 hoặc 80
      cfg.offset_rotation = 0;
      cfg.readable        = false;
      cfg.invert          = true;    // ST7789 hầu hết cần đảo màu
      cfg.rgb_order       = false;   // đổi thành true nếu đỏ/xanh bị hoán vị
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl      = PIN_BL;
      cfg.invert      = false;
      cfg.freq        = 12000;
      cfg.pwm_channel = 0;           // C3 chỉ có 6 kênh LEDC (0..5)
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }
};

static Display lcd;
static HijelHID_BLEKeyboard keyboard(DEVICE_NAME, "Huy", 100);

// ==================================================================
// Trạng thái hiển thị
// ==================================================================

static Screen   currentScreen  = Screen::Advertising;
static Screen   renderedScreen = Screen::Unpairing;  // ép vẽ lần đầu
static uint32_t lastActivityMs = 0;
static uint8_t  currentBrightness = 0;

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return lcd.color565(r, g, b); }

static void render(Screen screen) {
  if (screen == renderedScreen) return;
  renderedScreen = screen;

  uint16_t bg, accent;
  const char* title;
  const char* subtitle;

  switch (screen) {
    case Screen::Advertising:
      bg = rgb(24, 24, 28);  accent = rgb(240, 160, 40);
      title = "PAIRING";     subtitle = "Mo Bluetooth tren Mac";
      break;
    case Screen::Ready:
      bg = rgb(18, 22, 30);  accent = rgb(90, 200, 250);
      title = MACRO_LABEL;   subtitle = "San sang";
      break;
    case Screen::Sent:
      bg = rgb(20, 60, 40);  accent = rgb(120, 240, 150);
      title = "SENT";        subtitle = MACRO_LABEL;
      break;
    case Screen::Unpairing:
      bg = rgb(60, 20, 24);  accent = rgb(250, 120, 120);
      title = "RESET";       subtitle = "Da xoa ghep noi";
      break;
  }

  lcd.fillScreen(bg);
  lcd.setTextDatum(middle_center);

  lcd.setTextColor(accent, bg);
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.drawString(title, 120, 108);

  lcd.setTextColor(rgb(150, 150, 160), bg);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.drawString(subtitle, 120, 150);

  // Vòng viền mảnh cho đẹp
  lcd.drawCircle(120, 120, 112, accent);
}

static void setBrightness(uint8_t level) {
  if (level == currentBrightness) return;
  currentBrightness = level;
  lcd.setBrightness(level);
}

static void wakeDisplay() {
  lastActivityMs = millis();
  setBrightness(BRIGHT_FULL);
}

static void updateBacklight() {
  const uint32_t idle = millis() - lastActivityMs;
  if (idle > BLANK_AFTER_MS)      setBrightness(0);
  else if (idle > DIM_AFTER_MS)   setBrightness(BRIGHT_DIM);
  else                            setBrightness(BRIGHT_FULL);
}

// ==================================================================
// Đọc nút — không blocking
// ==================================================================

static ButtonEvent pollButton() {
  static bool     stableDown   = false;
  static bool     lastRawDown  = false;
  static uint32_t lastChangeMs = 0;
  static uint32_t pressedAtMs  = 0;
  static bool     longFired    = false;

  const bool rawDown = (digitalRead(PIN_SW) == LOW);
  const uint32_t now = millis();

  if (rawDown != lastRawDown) {
    lastRawDown  = rawDown;
    lastChangeMs = now;
    return ButtonEvent::None;
  }

  if (now - lastChangeMs < DEBOUNCE_MS) return ButtonEvent::None;
  if (rawDown == stableDown) {
    // Đang giữ: bắn sự kiện long press ngay khi đủ ngưỡng, không đợi nhả tay
    if (stableDown && !longFired && (now - pressedAtMs >= LONG_PRESS_MS)) {
      longFired = true;
      return ButtonEvent::LongPress;
    }
    return ButtonEvent::None;
  }

  stableDown = rawDown;
  if (stableDown) {
    pressedAtMs = now;
    longFired   = false;
    return ButtonEvent::None;
  }

  // Vừa nhả tay
  if (longFired) return ButtonEvent::None;   // long press đã xử lý rồi
  return ButtonEvent::ShortPress;
}

// ==================================================================
// Hành động
// ==================================================================

static uint32_t feedbackUntilMs = 0;

static void sendMacro() {
  if (!keyboard.isPaired()) return;

  keyboard.tap(MACRO_KEY, MACRO_MODIFIERS);
  keyboard.releaseAll();

  feedbackUntilMs = millis() + FEEDBACK_MS;
  currentScreen   = Screen::Sent;
}

static void forgetPairing() {
  currentScreen = Screen::Unpairing;
  render(currentScreen);
  keyboard.clearBonds();
  delay(1200);
  ESP.restart();   // khởi động lại để quảng bá sạch
}

// ==================================================================

void setup() {
  Serial.begin(115200);

  pinMode(PIN_SW, INPUT_PULLUP);

  lcd.init();
  lcd.setRotation(0);
  wakeDisplay();
  render(Screen::Advertising);

  keyboard.setLogLevel(HIDLogLevel::Normal);
  keyboard.setTxPower(3);   // để bàn cạnh Mac thì không cần công suất tối đa
  keyboard.begin();
}

void loop() {
  switch (pollButton()) {
    case ButtonEvent::ShortPress:
      wakeDisplay();
      sendMacro();
      break;
    case ButtonEvent::LongPress:
      wakeDisplay();
      forgetPairing();
      break;
    case ButtonEvent::None:
      break;
  }

  // Giữ màn "SENT" đủ lâu để thấy phản hồi, sau đó bám theo trạng thái BLE
  if (currentScreen == Screen::Sent) {
    if (millis() > feedbackUntilMs) {
      currentScreen = keyboard.isPaired() ? Screen::Ready : Screen::Advertising;
    }
  } else {
    currentScreen = keyboard.isPaired() ? Screen::Ready : Screen::Advertising;
  }

  render(currentScreen);
  updateBacklight();

  delay(10);   // nhường CPU — C3 chỉ có một nhân, đừng bao giờ busy-wait
}
