/*
 * TEST BRING-UP (TFT_eSPI) — ESP32-C3 SuperMini + ST7789 240x240 + 1 switch
 * =========================================================================
 * Chay qua 3 man hinh, bam nut de chuyen (hoac tu chuyen sau 6 giay):
 *   1. MAU      -> kiem tra TFT_RGB_ORDER va TFT_INVERSION_*
 *   2. HINH HOC -> kiem tra do phan giai va offset
 *   3. NUT BAM  -> dem so lan nhan, kiem tra debounce
 *
 * TRUOC KHI NAP:
 *   1. Chep Setup900_C3_ST7789_240x240.h vao
 *      <Arduino>/libraries/TFT_eSPI/User_Setups/
 *   2. Sua <Arduino>/libraries/TFT_eSPI/User_Setup_Select.h:
 *        //#include <User_Setup.h>
 *        #include <User_Setups/Setup900_C3_ST7789_240x240.h>
 *
 * Arduino IDE:
 *   Tools -> Board           : ESP32C3 Dev Module
 *   Tools -> USB CDC On Boot : Enabled      <-- BAT BUOC de thay Serial
 *   Tools -> Flash Size      : 4MB
 *
 * Luu y: font TFT_eSPI khong co dau tieng Viet nen chu tren man viet khong dau.
 */

#include <TFT_eSPI.h>
#include <SPI.h>

static TFT_eSPI tft = TFT_eSPI();

// Chi dinh nghia chan nut o day. Chan man hinh nam trong file Setup900.
static constexpr int PIN_SW = 0;   // switch, chan kia xuong GND

static constexpr uint8_t BACKLIGHT_LEVEL = 200;   // 0..255

// ==================================================================
// Cac kieu du lieu — PHAI khai bao truoc ham dau tien trong file .ino
// ------------------------------------------------------------------
// Arduino builder tu sinh prototype va chen len dau file, ngay truoc
// dinh nghia ham dau tien. Neu enum nam duoi do, prototype se tham
// chieu toi kieu chua ton tai -> loi "variable or field declared void".
// ==================================================================
enum class Stage { Colors, Geometry, Button };

// ==================================================================
// Den nen — dung LEDC de chinh do sang duoc
// ==================================================================
static void backlightBegin(uint8_t level) {
#ifdef TFT_BL
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(TFT_BL, 12000, 8);
    ledcWrite(TFT_BL, level);
  #else
    ledcSetup(0, 12000, 8);
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, level);
  #endif
#endif
}

// ==================================================================
// Doc nut — debounce khong blocking
// ==================================================================
static constexpr uint32_t DEBOUNCE_MS = 30;

static bool buttonWasPressed() {
  static bool     stableDown   = false;
  static bool     lastRawDown  = false;
  static uint32_t lastChangeMs = 0;

  const bool rawDown = (digitalRead(PIN_SW) == LOW);
  const uint32_t now = millis();

  if (rawDown != lastRawDown) {
    lastRawDown  = rawDown;
    lastChangeMs = now;
    return false;
  }
  if (now - lastChangeMs < DEBOUNCE_MS) return false;
  if (rawDown == stableDown)            return false;

  stableDown = rawDown;
  return stableDown;   // chi bao su kien o canh xuong
}

// ==================================================================
// Man 1 — kiem tra mau
// ==================================================================
static void drawColorTest() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("TEST 1: MAU", 120, 6);

  const uint16_t colors[3] = { TFT_RED, TFT_GREEN, TFT_BLUE };
  const char*    labels[3] = { "RED",   "GREEN",   "BLUE"   };

  for (int i = 0; i < 3; i++) {
    const int x = i * 80;
    tft.fillRect(x, 30, 80, 130, colors[i]);
    tft.setTextColor(TFT_BLACK, colors[i]);
    tft.drawString(labels[i], x + 40, 88);
  }

  // Dai trang/den de bat loi am ban
  tft.fillRect(0,   160, 120, 50, TFT_WHITE);
  tft.fillRect(120, 160, 120, 50, TFT_BLACK);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("WHITE", 60, 178);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("BLACK", 180, 178);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Sai mau -> sua TFT_RGB_ORDER", 120, 216);

  Serial.println(F("[TEST 1] 3 thanh phai dung nhan RED / GREEN / BLUE."));
  Serial.println(F("         Sai thu tu -> bo comment TFT_RGB_ORDER TFT_BGR"));
  Serial.println(F("         Am ban     -> bo comment TFT_INVERSION_OFF"));
}

// ==================================================================
// Man 2 — kiem tra hinh hoc va offset
// ==================================================================
static void drawGeometryTest() {
  tft.fillScreen(TFT_BLACK);

  // Vien sat mep: thieu canh nao la offset dang sai
  tft.drawRect(0, 0, 240, 240, TFT_WHITE);
  tft.drawRect(1, 1, 238, 238, TFT_WHITE);

  // O vuong 4 goc
  tft.fillRect(4,   4,   18, 18, TFT_YELLOW);
  tft.fillRect(218, 4,   18, 18, TFT_YELLOW);
  tft.fillRect(4,   218, 18, 18, TFT_YELLOW);
  tft.fillRect(218, 218, 18, 18, TFT_YELLOW);

  // Tam man
  tft.drawFastHLine(90, 120, 60, TFT_CYAN);
  tft.drawFastVLine(120, 90, 60, TFT_CYAN);
  tft.drawCircle(120, 120, 30, TFT_CYAN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("240 x 240", 120, 70);

  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Phai thay du 4 goc vang", 120, 175);
  tft.drawString("va vien trang khep kin", 120, 193);

  Serial.println(F("[TEST 2] Phai thay du 4 o vang + vien trang kin."));
  Serial.printf ("         tft.width()=%d  tft.height()=%d (mong doi 240 x 240)\n",
                 tft.width(), tft.height());
}

// ==================================================================
// Man 3 — kiem tra nut bam
// ==================================================================
static uint32_t pressCount  = 0;
static uint32_t lastPressMs = 0;

static bool pressIsRecent() {
  return (pressCount > 0) && (millis() - lastPressMs < 700);
}

static void drawButtonTest() {
  const bool     recent = pressIsRecent();
  const uint16_t bg     = recent ? tft.color565(20, 90, 50) : TFT_BLACK;

  tft.fillScreen(bg);
  tft.drawRect(0, 0, 240, 240, recent ? TFT_GREEN : TFT_DARKGREY);

  tft.setTextDatum(MC_DATUM);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString("TEST 3: BUTTON", 120, 40);

  if (pressCount == 0) {
    tft.setTextFont(4);
    tft.setTextColor(TFT_DARKGREY, bg);
    tft.drawString("CHUA NHAN", 120, 110);
  } else {
    tft.setTextFont(4);
    tft.setTextColor(recent ? TFT_GREEN : TFT_WHITE, bg);
    tft.drawString("DA NHAN BUTTON", 120, 100);

    tft.setTextFont(7);   // font LED 7 doan, to va ro
    tft.setTextColor(TFT_CYAN, bg);
    tft.drawString(String(pressCount), 120, 158);
  }

  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY, bg);
  tft.drawString("GPIO0 -> GND", 120, 215);
}

// ==================================================================

static Stage    stage          = Stage::Colors;
static uint32_t stageEnteredMs = 0;
static constexpr uint32_t AUTO_ADVANCE_MS = 6000;

static void enterStage(Stage next) {
  stage          = next;
  stageEnteredMs = millis();
  switch (stage) {
    case Stage::Colors:   drawColorTest();    break;
    case Stage::Geometry: drawGeometryTest(); break;
    case Stage::Button:   drawButtonTest();   break;
  }
}

void setup() {
  Serial.begin(115200);
  // Cho USB CDC gan vao host. Neu crash som, log se mat neu delay qua ngan.
  delay(2000);

  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F(" TEST TFT_eSPI — C3 + ST7789 240x240"));
  Serial.println(F("======================================"));
#ifdef USER_SETUP_ID
  Serial.printf("Setup ID trong thu vien : %d (mong doi 900)\n", USER_SETUP_ID);
#else
  Serial.println(F("!! Khong thay USER_SETUP_ID."));
  Serial.println(F("!! User_Setup_Select.h chua tro toi Setup900 — kiem tra lai."));
#endif
  Serial.printf("MOSI=%d SCLK=%d DC=%d RST=%d CS=%d\n",
                TFT_MOSI, TFT_SCLK, TFT_DC, TFT_RST, TFT_CS);

  pinMode(PIN_SW, INPUT_PULLUP);
  Serial.printf("Muc GPIO%d luc khoi dong: %s (mong doi HIGH)\n",
                PIN_SW, digitalRead(PIN_SW) == HIGH ? "HIGH" : "LOW");

  Serial.println(F(">>> Sap goi tft.init() ..."));
  Serial.flush();          // day het log ra truoc, phong khi init crash
  tft.init();
  Serial.println(F("<<< tft.init() da tra ve OK"));
  Serial.flush();

  tft.setRotation(0);
  backlightBegin(BACKLIGHT_LEVEL);
  Serial.println(F("Init xong. Man van den -> kiem tra BLK, VCC, RES."));
  Serial.flush();

  enterStage(Stage::Colors);
}

void loop() {
  const bool pressed = buttonWasPressed();

  if (pressed) {
    Serial.printf("[BUTTON] Nhan luc t=%lu ms\n", (unsigned long)millis());
  }

  switch (stage) {
    case Stage::Colors:
      if (pressed || millis() - stageEnteredMs > AUTO_ADVANCE_MS) {
        enterStage(Stage::Geometry);
      }
      break;

    case Stage::Geometry:
      if (pressed || millis() - stageEnteredMs > AUTO_ADVANCE_MS) {
        enterStage(Stage::Button);
      }
      break;

    case Stage::Button: {
      static bool needsRedraw = false;
      static bool lastRecent  = false;

      if (pressed) {
        pressCount++;
        lastPressMs = millis();
        needsRedraw = true;
      }

      // Ve lai khi vua nhan, va mot lan nua khi het hieu ung nen xanh
      const bool recent = pressIsRecent();
      if (needsRedraw || recent != lastRecent) {
        drawButtonTest();
        needsRedraw = false;
        lastRecent  = recent;
      }
      break;
    }
  }

  delay(10);   // C3 chi co mot nhan — luon nhuong CPU
}
