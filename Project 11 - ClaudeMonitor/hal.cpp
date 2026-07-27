#include "hal.h"
#include <Arduino.h>

// ── LilyGo T-Display S3 (1.9" ST7789 320x170, 8-bit parallel) ──

TFT_eSPI lcd;

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

void halFlush() {}

void halClear(uint16_t color) { lcd.fillScreen(color); }
