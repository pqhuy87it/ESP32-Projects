#include "hal.h"
#include <Arduino.h>

TFT_eSPI lcd;

// ════════════════════════════════════════════════════════════
// LƯU Ý VỀ THỨ TỰ #ifdef
// config.h define CẢ BOARD_IDEASPARK_ESP32_19 và BOARD_TDISPLAY_S3 (để ui.cpp
// tái dùng layout Mango 320x170). Vì vậy nhánh ideaspark PHẢI đứng trước.
// ════════════════════════════════════════════════════════════

#if defined(BOARD_IDEASPARK_ESP32_19)
// ── ideaspark ESP32 1.9" (ESP32-WROOM-32, ST7789 320x170, SPI) ──
//
// KHÁC BIỆT QUAN TRỌNG so với T-Display S3:
//  • KHÔNG có chân power-enable. Trên S3 đó là GPIO15, nhưng ở board này
//    GPIO15 = LCD_CS → kéo HIGH sẽ ghim CS deselect và màn không hiện gì.
//  • Màn nối SPI (23/18/15/2/4) chứ không phải bus 8-bit song song.
//  • Backlight ở GPIO32, không phải GPIO38.
//  • Không có mạch đo pin → halBatPercent() trả -1.
//  • Nút là tactile switch hàn ngoài nên PHẢI debounce (nút trên S3 đỡ nảy hơn,
//    thiếu debounce thì màn nhập PIN sẽ nhảy 2-3 số mỗi lần bấm).

static const uint32_t DEBOUNCE_MS = 25;

struct Btn {
    uint8_t  pin;
    bool     rawLast;     // mức thô lần trước (true = đang nhấn)
    bool     stable;      // trạng thái sau debounce
    uint32_t tEdge;       // thời điểm mức thô đổi
    uint32_t tPressed;    // thời điểm bắt đầu nhấn (cho long press)
    bool     evt;         // latch cho *WasPressed()
};

static void btnBegin(Btn& b, uint8_t pin) {
    b = {pin, false, false, 0, 0, false};
    pinMode(pin, INPUT_PULLUP);
}

// true đúng một lần tại cạnh "bắt đầu nhấn" đã debounce
static bool btnPoll(Btn& b) {
    bool raw = (digitalRead(b.pin) == LOW);   // active-LOW
    uint32_t now = millis();

    if (raw != b.rawLast) { b.rawLast = raw; b.tEdge = now; }

    if (now - b.tEdge >= DEBOUNCE_MS && raw != b.stable) {
        b.stable = raw;
        if (raw) { b.tPressed = now; return true; }
    }
    return false;
}

static void blBegin() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(LCD_BLK, BL_PWM_FREQ, BL_PWM_BITS);
#else
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_BITS);
    ledcAttachPin(LCD_BLK, BL_PWM_CHANNEL);
#endif
}

// Clamp level: brightness đọc từ NVS (prefs.getInt) có thể là giá trị rác/cũ,
// bản S3 index thẳng vào vals[] nên sẽ đọc ngoài mảng.
void halSetBrightness(uint8_t level) {
    static const uint8_t vals[4] = {0, 60, 160, 255};   // 0=off 1=dim 2=normal 3=bright
    if (level > 3) level = 3;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(LCD_BLK, vals[level]);
#else
    ledcWrite(BL_PWM_CHANNEL, vals[level]);
#endif
}

static void panelBegin() {
    blBegin();
    halSetBrightness(0);          // tắt đèn trong lúc init để không nháy trắng
    lcd.init();
    // KHÔNG gọi invertDisplay() ở đây: TFT_eSPI đã gửi INVON trong ST7789 init.
    // Nếu màn ra âm bản thì bật TFT_INVERSION_OFF trong User_Setup, đừng
    // invert lần hai ở đây (hai lần invert = trở về như cũ, dễ gây nhầm lẫn).
    lcd.setRotation(SCREEN_ROT);  // 1 → 320x170 landscape
    lcd.fillScreen(TFT_BLACK);
    halSetBrightness(DEFAULT_BRIGHTNESS);
}

#ifndef IDEASPARK_SINGLE_BUTTON
// ── 2 nút ngoài: A = GPIO27, B = GPIO26 ─────────────────────
static Btn s_a, s_b;

void halInit() {
    Serial.begin(115200);
    btnBegin(s_a, PIN_BTN_A);
    btnBegin(s_b, PIN_BTN_B);
    panelBegin();
}

void halUpdate() {
    // Latch sticky: bản S3 gán tapA = (a && !prevA) nên nếu halUpdate() chạy
    // hai lần trước khi WasPressed() được đọc thì event bị mất. Ở đây event
    // chỉ bị xóa khi đã có người đọc.
    if (btnPoll(s_a)) s_a.evt = true;
    if (btnPoll(s_b)) s_b.evt = true;
}

bool halBtnAWasPressed() { bool r = s_a.evt; s_a.evt = false; return r; }
bool halBtnBWasPressed() { bool r = s_b.evt; s_b.evt = false; return r; }
bool halBtnAIsPressed()  { return s_a.stable; }   // dùng state đã debounce,
bool halBtnBIsPressed()  { return s_b.stable; }   // không digitalRead thô

#else
// ── Chỉ nút BOOT: short press = A, long press = B ────────────
static Btn  s_boot;
static bool s_evtA = false, s_evtB = false;
static bool s_longFired = false;

void halInit() {
    Serial.begin(115200);
    btnBegin(s_boot, PIN_BTN_BOOT);
    panelBegin();
}

void halUpdate() {
    bool wasDown = s_boot.stable;
    if (btnPoll(s_boot)) s_longFired = false;   // vừa bắt đầu nhấn

    // long press: bắn ngay khi đủ ngưỡng, không đợi nhả tay
    if (s_boot.stable && !s_longFired &&
        millis() - s_boot.tPressed >= LONGPRESS_MS) {
        s_evtB = true;
        s_longFired = true;
    }
    // short press: nhả tay trước khi đạt ngưỡng long
    if (wasDown && !s_boot.stable && !s_longFired) s_evtA = true;
}

bool halBtnAWasPressed() { bool r = s_evtA; s_evtA = false; return r; }
bool halBtnBWasPressed() { bool r = s_evtB; s_evtB = false; return r; }
bool halBtnAIsPressed()  { return s_boot.stable && !s_longFired; }
bool halBtnBIsPressed()  { return s_boot.stable &&  s_longFired; }

#endif // IDEASPARK_SINGLE_BUTTON

int halBatPercent() {
    return -1;   // không có mạch đo pin → ui.cpp ẩn icon battery
}

#else
// ── LilyGo T-Display S3 (1.9" ST7789 320x170, 8-bit parallel) ──
// Giữ nguyên bản gốc.

#define BTN_A_PIN  0
#define BTN_B_PIN  14
#define BAT_ADC    4
#define BL_PIN     38
#define PWR_EN     15

static bool prevA = false, prevB = false;
static bool tapA = false, tapB = false;

void halInit() {
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, HIGH);
    lcd.init();
    lcd.invertDisplay(true);
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Core 3.x: ledcSetup + ledcAttachPin gộp thành ledcAttach(pin, freq, res);
    // ledcWrite giờ nhận số PIN thay vì channel.
    ledcAttach(BL_PIN, 5000, 8);
    ledcWrite(BL_PIN, 200);
#else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(BL_PIN, 0);
    ledcWrite(0, 200);
#endif
}

void halUpdate() {
    bool a = !digitalRead(BTN_A_PIN);
    bool b = !digitalRead(BTN_B_PIN);
    tapA = (a && !prevA);
    tapB = (b && !prevB);
    prevA = a;
    prevB = b;
}

bool halBtnAWasPressed() { bool r = tapA; tapA = false; return r; }
bool halBtnBWasPressed() { bool r = tapB; tapB = false; return r; }
bool halBtnAIsPressed()  { return !digitalRead(BTN_A_PIN); }
bool halBtnBIsPressed()  { return !digitalRead(BTN_B_PIN); }

int halBatPercent() {
    uint16_t raw = analogRead(BAT_ADC);
    float v = (raw / 4095.0f) * 3.3f * 2.0f;
    return constrain((int)((v - 3.3f) / 0.85f * 100), 0, 100);
}

void halSetBrightness(uint8_t level) {
    static const uint8_t vals[] = {0, 60, 160, 255};
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(BL_PIN, vals[level]);   // core 3.x: theo số pin
#else
    ledcWrite(0, vals[level]);        // core 2.x: theo channel
#endif
}

#endif // BOARD_IDEASPARK_ESP32_19

// ── Dùng chung: ST7789 vẽ trực tiếp, không có framebuffer ───
void halFlush() { }

void halClear(uint16_t color) { lcd.fillScreen(color); }
