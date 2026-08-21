/* ------------------------------------------------------------------------
   Stopwatch  --  ESP32-C3 SuperMini + ST7789 240x240 + 1 switch
   Port of MakerM0 MagiClick CircuitPython app/stopwatch.py to Arduino.

   UI: 1 column, 8 lap rows, lap number on the left.

   ------------------------------------------------------------------------
   CONTROLS (single switch on GPIO0 -> GND)

     tap        (< 600 ms)   start  /  record lap  /  resume
     hold  0.6 .. 1.8 s      pause  /  resume
     hold  > 1.8 s           reset

   The action fires on RELEASE, and the hint line under the big clock tells
   you which tier you are in while holding, so you can let go at the right
   moment. The lap timestamp is captured on button-DOWN, so a lap is never
   delayed by hold detection.

   ------------------------------------------------------------------------
   BEFORE FLASHING

     Uses your existing library config:
       <Arduino>/libraries/TFT_eSPI/User_Setups/Setup900_C3_ST7789_240x240.h
     with User_Setup_Select.h pointing at it.

     Arduino IDE:
       Tools -> Board            : ESP32C3 Dev Module
       Tools -> USB CDC On Boot  : Enabled
       Tools -> Flash Size       : 4MB
   ------------------------------------------------------------------------ */

#include <TFT_eSPI.h>
#include <SPI.h>

#if !defined(LOAD_GLCD)
  #error "This sketch draws with font 1 -- add #define LOAD_GLCD to Setup900."
#endif

// ---------------------------------------------------------------- pins ----
static constexpr int PIN_SW = 0;            // switch, other leg to GND

static constexpr uint8_t BACKLIGHT_LEVEL = 200;   // 0..255

// -------------------------------------------------------------- options ----
#define LAP_ROWS       8
#define SHOW_SPLIT     1    // 1 = show delta vs previous lap on the right
#define SCROLL_LAPS    1    // 1 = keep newest 8 laps, 0 = stop after 8

static constexpr uint32_t DEBOUNCE_MS  = 30;
static constexpr uint32_t TAP_MAX_MS   = 600;    // below this = tap
static constexpr uint32_t RESET_MIN_MS = 1800;   // above this = reset

// --------------------------------------------------------------- layout ----
#define SCR_W    240
#define SCR_H    240
#define HDR_H     60        // header band, redrawn from a sprite
#define DIV_Y     62
#define ROW_TOP   64
#define ROW_H     22        // 64 + 8*22 = 240

#define X_INDEX   28        // right edge of the lap-number gutter
#define X_TIME    34
#define X_SPLIT  150

// --------------------------------------------------------------- colors ----
#define C_RUN     TFT_WHITE
#define C_PAUSE   TFT_RED
#define C_IDLE    0x7BEF    // grey
#define C_LAP     0x07E0    // green
#define C_INDEX   0x03EF    // dim teal
#define C_SPLIT   0x4208    // dim grey
#define C_EMPTY   0x2124    // very dim grey (placeholder dashes)
#define C_HINT    0x05BF    // cyan-ish
#define C_HINT_HI TFT_YELLOW
#define C_DIV     0x2965

static TFT_eSPI    tft = TFT_eSPI();
static TFT_eSprite hdr = TFT_eSprite(&tft);

// ====================================================== stopwatch state ====
struct Stopwatch {
  uint32_t startMs   = 0;     // millis() at last start/resume
  uint32_t elapsedMs = 0;     // accumulated time from earlier run segments
  bool     running   = false;

  uint32_t now() const {
    return running ? elapsedMs + (millis() - startMs) : elapsedMs;
  }
  void start() {
    if (running) return;
    startMs = millis();
    running = true;
  }
  void pause() {
    if (!running) return;
    elapsedMs += millis() - startMs;
    running = false;
  }
  void reset() {
    startMs = elapsedMs = 0;
    running = false;
  }
  bool isPaused() const { return !running && elapsedMs > 0; }
};

static Stopwatch sw;

static uint32_t lapMs[LAP_ROWS];    // displayed lap times
static uint16_t lapNo[LAP_ROWS];    // their 1-based lap numbers
static uint8_t  lapCount = 0;       // rows currently filled
static uint16_t lapTotal = 0;       // laps recorded since reset

// ============================================================= backlight ===
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

// ========================================================= button helper ===
// Non-blocking debounce. Reports the DOWN edge, and the UP edge together
// with how long the switch was held.
struct OneButton {
  bool     stable      = true;    // true = released (active LOW)
  bool     lastRaw     = true;
  uint32_t lastChange  = 0;
  uint32_t pressedAt   = 0;

  bool     down    = false;       // events, cleared on every update()
  bool     up      = false;
  uint32_t heldMs  = 0;

  void begin() { pinMode(PIN_SW, INPUT_PULLUP); }

  // how long the switch has been held right now, 0 if released
  uint32_t holding() const { return stable ? 0 : (millis() - pressedAt); }

  void update() {
    down = up = false;

    const bool     raw = (digitalRead(PIN_SW) == LOW) ? false : true;  // LOW = pressed
    const uint32_t now = millis();

    if (raw != lastRaw) { lastRaw = raw; lastChange = now; return; }
    if (now - lastChange < DEBOUNCE_MS) return;
    if (raw == stable)                  return;

    stable = raw;
    if (!stable) {                      // pressed
      pressedAt = now;
      down      = true;
    } else {                            // released
      heldMs = now - pressedAt;
      up     = true;
    }
  }
};

static OneButton btn;

// lap timestamp captured on button-down
static uint32_t pendingLap   = 0;
static bool     pendingValid = false;

// ============================================================= rendering ===
static void fmtTime(uint32_t ms, char *out) {
  snprintf(out, 10, "%02u:%02u:%03u",
           (unsigned)(ms / 60000u),
           (unsigned)((ms / 1000u) % 60u),
           (unsigned)(ms % 1000u));
}

// Text under the big clock. Depends on state and on the current hold tier.
static const char *hintText(uint16_t &color) {
  const uint32_t held = btn.holding();

  if (held >= RESET_MIN_MS) { color = C_HINT_HI; return "RELEASE = RESET"; }
  if (held >= TAP_MAX_MS) {
    color = C_HINT_HI;
    return sw.running ? "RELEASE = PAUSE" : "RELEASE = RESUME";
  }

  color = C_HINT;
  if (sw.running)     return "TAP = LAP    HOLD = PAUSE";
  if (sw.isPaused())  return "TAP = RESUME    HOLD = RESET";
  return "TAP = START";
}

static void drawHeader(bool force = false) {
  static uint32_t lastShown = 0xFFFFFFFF;
  static uint16_t lastColor = 0;
  static const char *lastHint = nullptr;

  const uint32_t t = sw.now();
  const uint16_t c = sw.running ? C_RUN : (sw.isPaused() ? C_PAUSE : C_IDLE);

  uint16_t hintColor;
  const char *hint = hintText(hintColor);

  if (!force && t == lastShown && c == lastColor && hint == lastHint) return;
  lastShown = t;
  lastColor = c;
  lastHint  = hint;

  char buf[10];
  fmtTime(t, buf);

  hdr.fillSprite(TFT_BLACK);

  hdr.setTextFont(1);
  hdr.setTextDatum(TL_DATUM);
  hdr.setTextSize(4);                       // 24 x 32 px per char -> 216 wide
  hdr.setTextColor(c, TFT_BLACK);
  hdr.drawString(buf, (SCR_W - 9 * 24) / 2, 6);

  hdr.setTextSize(1);                       // 6 x 8 px per char
  hdr.setTextDatum(TC_DATUM);
  hdr.setTextColor(hintColor, TFT_BLACK);
  hdr.drawString(hint, SCR_W / 2, 46);

  hdr.pushSprite(0, 0);
}

static void drawRow(uint8_t i) {
  const int y  = ROW_TOP + i * ROW_H;
  const int ty = y + (ROW_H - 16) / 2;      // font 1 size 2 -> 16 px tall

  tft.fillRect(0, y, SCR_W, ROW_H, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);

  const bool filled = (i < lapCount);

  // ---- lap number, right-aligned in the left gutter
  char num[4];
  snprintf(num, sizeof(num), "%u", filled ? lapNo[i] : (unsigned)(i + 1));
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(filled ? C_INDEX : C_EMPTY, TFT_BLACK);
  tft.drawString(num, X_INDEX, ty);

  // ---- lap time
  tft.setTextDatum(TL_DATUM);
  if (filled) {
    char buf[10];
    fmtTime(lapMs[i], buf);
    tft.setTextColor(C_LAP, TFT_BLACK);
    tft.drawString(buf, X_TIME, ty);
  } else {
    tft.setTextColor(C_EMPTY, TFT_BLACK);
    tft.drawString("__:__:___", X_TIME, ty);
  }

#if SHOW_SPLIT
  // ---- delta vs the previous lap. After scrolling, the top visible row has
  //      no predecessor on screen, so it is left blank.
  if (filled && (i > 0 || lapNo[i] == 1)) {
    const uint32_t prev  = (i == 0) ? 0 : lapMs[i - 1];
    const uint32_t delta = (lapMs[i] >= prev) ? (lapMs[i] - prev) : 0;
    char d[10];
    snprintf(d, sizeof(d), "+%u.%03u",
             (unsigned)(delta / 1000u), (unsigned)(delta % 1000u));
    tft.setTextColor(C_SPLIT, TFT_BLACK);
    tft.drawString(d, X_SPLIT, ty);
  }
#endif
}

static void drawAllRows() {
  for (uint8_t i = 0; i < LAP_ROWS; i++) drawRow(i);
}

static void drawStatic() {
  tft.fillScreen(TFT_BLACK);
  tft.drawFastHLine(0, DIV_Y, SCR_W, C_DIV);
}

// =============================================================== actions ===
static void recordLap(uint32_t stamp) {
  if (lapCount < LAP_ROWS) {
    lapMs[lapCount] = stamp;
    lapNo[lapCount] = ++lapTotal;
    drawRow(lapCount);
    lapCount++;
  } else {
#if SCROLL_LAPS
    for (uint8_t i = 0; i < LAP_ROWS - 1; i++) {
      lapMs[i] = lapMs[i + 1];
      lapNo[i] = lapNo[i + 1];
    }
    lapMs[LAP_ROWS - 1] = stamp;
    lapNo[LAP_ROWS - 1] = ++lapTotal;
    drawAllRows();
#endif
  }
  Serial.printf("[LAP %u] %lu ms\n", (unsigned)lapTotal, (unsigned long)stamp);
}

static void doReset() {
  sw.reset();
  lapCount     = 0;
  lapTotal     = 0;
  pendingValid = false;
  drawAllRows();
  drawHeader(true);
  Serial.println(F("[RESET]"));
}

// ================================================================== setup ==
void setup() {
  Serial.begin(115200);
  delay(1500);                  // let USB CDC attach so early logs survive

  Serial.println();
  Serial.println(F("=== Stopwatch -- C3 + ST7789 240x240 ==="));
#ifdef USER_SETUP_ID
  Serial.printf("Setup ID: %d\n", USER_SETUP_ID);
#else
  Serial.println(F("!! No USER_SETUP_ID -- check User_Setup_Select.h"));
#endif

  btn.begin();
  Serial.printf("GPIO%d at boot: %s (expect HIGH)\n",
                PIN_SW, digitalRead(PIN_SW) == HIGH ? "HIGH" : "LOW");

  tft.init();
  tft.setRotation(0);
  backlightBegin(BACKLIGHT_LEVEL);
  drawStatic();

  hdr.setColorDepth(16);
  if (!hdr.createSprite(SCR_W, HDR_H)) {    // 240 x 60 x 2 = 28.8 kB
    Serial.println(F("!! Sprite alloc failed -- lower HDR_H or use 8-bit depth"));
  }

  drawAllRows();
  drawHeader(true);
  Serial.println(F("Ready. Tap = start."));
}

// =================================================================== loop ==
void loop() {
  btn.update();

  // capture the instant the switch goes down, so the lap is exact
  if (btn.down && sw.running) {
    pendingLap   = sw.now();
    pendingValid = true;
  }

  if (btn.up) {
    if (btn.heldMs >= RESET_MIN_MS) {
      doReset();
    } else if (btn.heldMs >= TAP_MAX_MS) {
      if (sw.running)          sw.pause();
      else if (sw.isPaused())  sw.start();
      pendingValid = false;
    } else {                                  // tap
      if (sw.running) {
        if (pendingValid) { recordLap(pendingLap); pendingValid = false; }
      } else {
        sw.start();                           // fresh start or resume
      }
    }
  }

  // refresh the big clock at ~40 Hz
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw >= 25) {
    lastDraw = millis();
    drawHeader();
  }

  delay(2);       // single core -- always yield
}
