#include "ui.h"
#include "config.h"
#include "hal.h"
#include <Arduino.h>
#include <time.h>
#include <math.h>
#include <string.h>
#ifdef MANGO_UI
#include "weather.h"
#include "weather_icons.h"      // bộ icon lớn 50x50 (prefix ICON) — bmp2icons.py big/ ICON
#include "weather_icons_sm.h"   // bộ icon nhỏ 28x28 (prefix ICONSM) — bmp2icons.py small/ ICONSM
#include "moon.h"
#include "moon_icons.h"         // bộ ảnh trăng lớn 75x75 (prefix MOON) — bmp2icons.py moon/ MOON
#include "moon_icons_sm.h"      // bộ ảnh trăng nhỏ 40x40 (prefix MOONSM) — bmp2icons.py moon-sm/ MOONSM
#endif

// Shared helper — no display calls, safe before any #ifdef
static void fmtCountdown(uint32_t epoch, char* out, size_t len) {
    if (epoch == 0) {
        strlcpy(out, "--", len);
        return;
    }
    time_t now;
    time(&now);
    int32_t diff = (int32_t)epoch - (int32_t)now;
    if (diff <= 0) {
        strlcpy(out, "now", len);
        return;
    }
    int d = diff / 86400;
    int h = (diff % 86400) / 3600;
    int m = (diff % 3600) / 60;
    if (d > 0) snprintf(out, len, "%dd%dh", d, h);
    else if (h > 0) snprintf(out, len, "%dh%02dm", h, m);
    else snprintf(out, len, "%dm", m);
}

// ════════════════════════════════════════════════════════════
// TFT implementation — LilyGo T-Display S3 (Mango UI)
// (Khối OLED của BOARD_ESP32C3_OLED đã được lược bỏ cho bản build này.
//  Các nhánh #ifdef của board khác bên dưới không được compile vì chỉ
//  BOARD_TDISPLAY_S3 được define trong config.h.)
// ════════════════════════════════════════════════════════════


// ── Colors (RGB565) ──────────────────────────────────────
#define C_BG      TFT_BLACK
#define C_TEXT    TFT_WHITE
#define C_DIM     0x7BEF
#define C_BAR_BG  0x2104
#define C_OK      0x07E0
#define C_WARN    0xFD20
#define C_CRIT    0xF800
#define C_HEAD    0xEB87   // Claude orange
#define C_ACCENT  0xEB87
#define C_CYAN    0xF50A   // light warm orange
#define C_HEAD_DK 0xA244   // dimmed Claude orange — empty wifi bars, hairline dividers

// The base layout is designed for the ~240x135 LCD. Larger panels scale the
// coordinates and font up so text stays readable and the layout fills the screen.
// Coordinates are expressed in base units and mapped through these macros at draw time.
#if defined(BOARD_TDISPLAY_S3_AMOLED)
  // 536x240 panel — uniform 2x of the base layout.
  #define TS(n) ((n) * 2)
  #define SX(n) ((n) * 2)
  #define SY(n) ((n) * 2)
#elif defined(BOARD_CROWPANEL_ADV_35)
  // 480x320 panel — 2x font, with coordinates stretched to fill the whole screen
  // (~2x across, ~2.37x down) so the dashboard spreads over the full height
  // instead of bunching up at the top.
  #define TS(n) ((n) * 2)
  #define SX(n) ((int)((n) * (SCREEN_W / 240.0f)))
  #define SY(n) ((int)((n) * (SCREEN_H / 135.0f)))
#else
  #define TS(n) (n)
  #define SX(n) (n)
  #define SY(n) (n)
#endif

#ifdef BOARD_CROWPANEL_ADV_35
// The CrowPanel's ILI9488 is too slow to clear-then-redraw on screen without flicker.
// The dashboard is therefore rendered into an off-screen sprite (PSRAM) and pushed in a
// single transfer — no flicker. Other screens draw straight to the panel so touch input
// (PIN entry) stays snappy. TFT_eSprite hides (not overrides virtually) the TFT_eSPI
// drawing methods, so dashboard drawing must use a TFT_eSprite-typed target (see drawBar
// being a template that binds to the concrete type at compile time).
static TFT_eSprite s_dash = TFT_eSprite(&lcd);
static bool        s_dashReady = false;
static TFT_eSprite& dashTarget() {
    if (!s_dashReady) { s_dash.setColorDepth(16); s_dash.createSprite(SCREEN_W, SCREEN_H); s_dashReady = true; }
    return s_dash;
}
  #define UI_PUSH_DASH() (s_dash.pushSprite(0, 0))
#else
  #define UI_PUSH_DASH() halFlush()
#endif

static uint16_t barColor(float) {
    return C_TEXT;
}

// Right-aligned reset countdown that rides on a usage bar's label row (the
// M5StickC Plus 240x135 layout). A fixed-width slot is cleared first so a shorter
// string (e.g. "59m" replacing "1h59m") leaves no trailing pixels when the 10s
// clock tick repaints it in place. Only ever called on the 240px panel, so the
// slot width is tuned for it. Templated like drawBar to bind to the real target.
static const int RESET_SLOT_W = 42;
template <class GFX>
static void drawResetSlot(GFX& g, int barX, int barW, int y, const char* reset) {
    g.fillRect(barX + barW - RESET_SLOT_W, y, RESET_SLOT_W, 8, C_BG);
    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(1);
    g.setCursor(barX + barW - (int)strlen(reset) * 6, y);
    g.print(reset);
}

// Templated so it binds to the concrete target type (TFT_eSPI panel or TFT_eSprite
// buffer) — those share method names but are not virtual, so a base reference would
// dispatch to the wrong (panel) implementation.
// When `reset` is given (M5StickC Plus), the countdown rides at the right of the
// label row and the % is tucked just left of it; otherwise the % is flush-right
// and every other board renders exactly as before.
template <class GFX>
static void drawBar(GFX& g, int x, int y, int w, int h, float pct, const char* label,
                    const char* reset = nullptr) {
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(TS(1));
    g.setCursor(x, y);
    g.print(label);

    int pctRight = x + w;
    if (reset) {
        drawResetSlot(g, x, w, y, reset);
        pctRight = x + w - RESET_SLOT_W - TS(4);
    }

    char ps[8];
    snprintf(ps, sizeof(ps), "%.0f%%", pct);
    g.setCursor(pctRight - (int)strlen(ps) * TS(6), y);
    g.setTextColor(barColor(pct), C_BG);
    g.print(ps);

    int by = y + SY(12);
    g.fillRect(x, by, w, h, C_BAR_BG);
    int fw = constrain((int)(w * pct / 100.0f), 0, w);
    if (fw > 0) g.fillRect(x, by, fw, h, barColor(pct));
}

#ifdef MANGO_UI
static ModelStatus s_modelStatus = {true, true, true, true, false};

void uiSetModelStatus(const ModelStatus& s) {
    s_modelStatus = s;
}

void uiToggleRotation() {
    static bool flipped = false;
    flipped = !flipped;
    // 1 and 3 are the two landscape orientations; XOR-2 toggles between them
    // regardless of which one is this board's default (S3 = 1, M5StickC Plus = 3).
    lcd.setRotation(flipped ? (SCREEN_ROT ^ 2) : SCREEN_ROT);
    halClear(C_BG);
}

// Clawd, 18x5 px (MSB = leftmost column). The row-1 gaps at cols 5/12 are the eyes.
// Shared by the S3 mascot row and the M5StickC Plus status-panel mascot.
static const uint32_t CLAWD_ROWS[5] = {
    0b000111111111111000,
    0b000110111111011000,
    0b011111111111111110,
    0b000111111111111000,
    0b000010100001010000,
};
static const uint32_t CLAWD_DEAD_ROW1 = 0b000111111111111000;   // eyes filled solid

// Bitmap columns of Clawd's two eyes (the row-1 gaps) — used by both blink ticks.
static const int CLAWD_EYE_COLS[2] = {5, 12};

// Left edge (in px from the mascot's x) of bitmap column c when Clawd is W px
// wide. Rounding to nearest lets W be a non-multiple of 18 (fractional cell
// widths) while keeping the edges symmetric, so both eyes render the same width.
static inline int mascotEdge(int c, int W) { return (c * W + 9) / 18; }

// W = total width; rh = row height, ~2x the cell width (W/18) — terminal quadrant
// cells are about twice as tall as wide, and square cells squash him.
static void drawMascot(TFT_eSPI& g, int x, int y, int W, int rh, uint16_t color, bool dead) {
    for (int r = 0; r < 5; r++) {
        uint32_t row = (dead && r == 1) ? CLAWD_DEAD_ROW1 : CLAWD_ROWS[r];
        for (int c = 0; c < 18; c++)
            if (row & (1UL << (17 - c)))
                g.fillRect(x + mascotEdge(c, W), y + r * rh,
                           mascotEdge(c + 1, W) - mascotEdge(c, W), rh, color);
    }
    if (dead) {
        for (int e = 0; e < 2; e++) {
            int cx = x + (mascotEdge(CLAWD_EYE_COLS[e], W) +
                          mascotEdge(CLAWD_EYE_COLS[e] + 1, W)) / 2;
            int cy = y + rh + rh / 2;
            g.drawLine(cx - 3, cy - 4, cx + 3, cy + 4, C_BG);
            g.drawLine(cx - 2, cy - 4, cx + 4, cy + 4, C_BG);
            g.drawLine(cx + 3, cy - 4, cx - 3, cy + 4, C_BG);
            g.drawLine(cx + 4, cy - 4, cx - 2, cy + 4, C_BG);
        }
    }
}

// ── Model-health panel ──────────────────────────────────────────────────────
// The space below the bars differs by screen size:
//   • T-Display S3 (320x170): the reset countdowns sit on their own size-2 row
//     below the bars (the Clarity layout — easier to read), then a full row of
//     four labelled, blinking Clawds — no divider, the mascots speak for themselves.
//   • M5StickC Plus (240x135): reset rides on the bar rows (no room below);
//     a "── MODELS ──" divider, one overall-health Clawd + a 2x2 "NAME STATUS" grid.
// drawStatusPanel() is the board-specific entry point either way, so uiDashboard
// just calls it.

#ifdef BOARD_TDISPLAY_S3
// ── T-Display S3: reset row below the bars + four labelled Clawds ───────────
// Reset countdowns get their own row under the bars; the "MODELS" divider and
// the four mascots — each named, each blinking when healthy — fill what's left.
#define RESET_CAP_Y     80
#define RESET_VAL_Y     92
#define MASCOT_W        44                // fractional ~2.4px cells via mascotEdge
#define MASCOT_RH       5                 // row height → 25px tall
#define MASCOT_Y        122
#define MASCOT_SPACING  80
#define MASCOT_CX0      40
#define MASCOT_NAME_Y   156
#define MASCOT_CX(i) (MASCOT_CX0 + (i) * MASCOT_SPACING)
#define MASCOT_X(i)  (MASCOT_CX(i) - MASCOT_W / 2)

// Size-2 countdown values — padded, opaque print overwrites in place so the
// 10s clock tick can repaint them without clearing first.
static void drawResetValues(TFT_eSPI& g, const char* h5rst, const char* d7rst) {
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(2);
    g.setCursor(10, RESET_VAL_Y);
    g.printf("%-8s", h5rst);
    g.setCursor(SCREEN_W / 2 + 10, RESET_VAL_Y);
    g.printf("%-8s", d7rst);
}

static void drawResetRow(TFT_eSPI& g, const char* h5rst, const char* d7rst) {
    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(1);
    g.setCursor(10, RESET_CAP_Y);
    g.print("5H RESET");
    g.setCursor(SCREEN_W / 2 + 10, RESET_CAP_Y);
    g.print("7D RESET");
    drawResetValues(g, h5rst, d7rst);
}

static void drawStatusPanel(TFT_eSPI& g) {
    static const char* names[4] = {"HAIKU", "SONNET", "OPUS", "FABLE"};
    bool up[4] = {s_modelStatus.haikuUp, s_modelStatus.sonnetUp,
                  s_modelStatus.opusUp,  s_modelStatus.fableUp};
    for (int i = 0; i < 4; i++) {
        int cx = MASCOT_CX(i);
        // Unknown (status never fetched) renders gray without X eyes, so a
        // status-page outage is never mistaken for a model outage.
        bool dead = s_modelStatus.ok && !up[i];
        uint16_t col = (!s_modelStatus.ok || dead) ? C_DIM : C_HEAD;
        drawMascot(g, MASCOT_X(i), MASCOT_Y, MASCOT_W, MASCOT_RH, col, dead);
        g.setTextColor(C_DIM, C_BG);
        g.setTextSize(1);
        g.setCursor(cx - (int)strlen(names[i]) * 3, MASCOT_NAME_Y);
        g.print(names[i]);
    }
}

// Repaint only the eye cells of the healthy mascots — drawn straight to the panel,
// so the 2s "I'm alive" blink costs no full redraw.
void uiBlinkTick(bool closed) {
    bool up[4] = {s_modelStatus.haikuUp, s_modelStatus.sonnetUp,
                  s_modelStatus.opusUp,  s_modelStatus.fableUp};
    int ey = MASCOT_Y + MASCOT_RH;   // eye row 1
    for (int i = 0; i < 4; i++) {
        if (!s_modelStatus.ok || !up[i]) continue;   // dead/unknown don't blink
        for (int e = 0; e < 2; e++) {
            int ex = MASCOT_X(i) + mascotEdge(CLAWD_EYE_COLS[e], MASCOT_W);
            int ew = mascotEdge(CLAWD_EYE_COLS[e] + 1, MASCOT_W) -
                     mascotEdge(CLAWD_EYE_COLS[e], MASCOT_W);
            if (closed) {
                lcd.fillRect(ex, ey, ew, MASCOT_RH, C_HEAD);                  // lid down
                lcd.fillRect(ex, ey + MASCOT_RH / 2 - 1, ew, 2, C_BG);        // shut line
            } else {
                lcd.fillRect(ex, ey, ew, MASCOT_RH, C_BG);                    // eye open
            }
        }
    }
}

#else
// ── M5StickC Plus 240x135: one overall-health Clawd + a 2x2 text grid ───────
#define PANEL_MASCOT_S  2
#define PANEL_MASCOT_X  22
#define PANEL_MASCOT_Y  100
#define PANEL_CAP_Y     80
#define PANEL_LINE_Y    84
#define PANEL_COL0_X    76
#define PANEL_COL1_X    164
#define PANEL_ROW0_Y    98
#define PANEL_ROW1_Y    114

static void drawModelsDivider(TFT_eSPI& g, int capY, int lineY) {
    const char* cap = "MODELS";
    int capW = (int)strlen(cap) * 6;
    int cx   = SCREEN_W / 2;
    g.setTextSize(1);
    g.setTextColor(C_DIM, C_BG);
    g.setCursor(cx - capW / 2, capY);
    g.print(cap);
    g.drawFastHLine(14, lineY, (cx - capW / 2 - 6) - 14, C_HEAD_DK);
    g.drawFastHLine(cx + capW / 2 + 6, lineY,
                    (SCREEN_W - 14) - (cx + capW / 2 + 6), C_HEAD_DK);
}

static void drawStatusPanel(TFT_eSPI& g) {
    static const char* names[4] = {"HAIKU", "SONNET", "OPUS", "FABLE"};
    bool up[4] = {s_modelStatus.haikuUp, s_modelStatus.sonnetUp,
                  s_modelStatus.opusUp,  s_modelStatus.fableUp};
    drawModelsDivider(g, PANEL_CAP_Y, PANEL_LINE_Y);

    // One Clawd to the left of the grid reflects overall health: Claude orange when
    // every model is up, gray with X-eyes if any is down, gray (no X) until fetched.
    bool anyDown = false;
    for (int i = 0; i < 4; i++) anyDown = anyDown || !up[i];
    bool dead = s_modelStatus.ok && anyDown;
    drawMascot(g, PANEL_MASCOT_X, PANEL_MASCOT_Y, 18 * PANEL_MASCOT_S, PANEL_MASCOT_S * 2,
               (!s_modelStatus.ok || dead) ? C_DIM : C_HEAD, dead);

    const int colX[2] = {PANEL_COL0_X, PANEL_COL1_X};
    const int rowY[2] = {PANEL_ROW0_Y, PANEL_ROW1_Y};
    g.setTextSize(1);
    for (int i = 0; i < 4; i++) {
        const char* st;
        uint16_t col;
        if (!s_modelStatus.ok) { st = "?";    col = C_DIM;  }   // status never fetched
        else if (up[i])        { st = "UP";   col = C_HEAD; }   // Claude orange = healthy
        else                   { st = "DOWN"; col = C_DIM;  }   // gray = down
        char buf[16];
        snprintf(buf, sizeof(buf), "%-6s %s", names[i], st);   // padded name aligns the status column
        g.setTextColor(col, C_BG);
        g.setCursor(colX[i % 2], rowY[i / 2]);
        g.print(buf);
    }
}

// Blink the panel mascot's eyes — only when he's the healthy (orange) Clawd; the
// gray "something's down"/unknown Clawd stays still. The 2s liveness blink.
void uiBlinkTick(bool closed) {
    bool up[4] = {s_modelStatus.haikuUp, s_modelStatus.sonnetUp,
                  s_modelStatus.opusUp,  s_modelStatus.fableUp};
    bool anyDown = false;
    for (int i = 0; i < 4; i++) anyDown = anyDown || !up[i];
    if (!s_modelStatus.ok || anyDown) return;

    int ch = PANEL_MASCOT_S * 2;
    int ey = PANEL_MASCOT_Y + ch;   // eye row 1
    for (int e = 0; e < 2; e++) {
        int ex = PANEL_MASCOT_X + CLAWD_EYE_COLS[e] * PANEL_MASCOT_S;
        if (closed) {
            lcd.fillRect(ex, ey, PANEL_MASCOT_S, ch, C_HEAD);           // lid down
            lcd.fillRect(ex, ey + ch / 2 - 1, PANEL_MASCOT_S, 2, C_BG); // shut line
        } else {
            lcd.fillRect(ex, ey, PANEL_MASCOT_S, ch, C_BG);            // eye open
        }
    }
}
#endif // BOARD_TDISPLAY_S3 four-mascot row vs M5StickC Plus status panel

static void drawWifiIcon(TFT_eSPI& g, int x, int rssi) {
    int level = (rssi >= -55) ? 4 : (rssi >= -65) ? 3 : (rssi >= -75) ? 2 : (rssi >= -85) ? 1 : 0;
    for (int i = 0; i < 4; i++) {
        int h = 3 * (i + 1);
        g.fillRect(x + i * 4, 14 - h, 3, h, (level > i) ? C_TEXT : C_HEAD_DK);
    }
}

// Right side of the header, anchored to the right edge: [ago] [wifi] [battery+pct].
// Repainted whole by uiDashboardClock every 10s, so everything here must be
// derivable from its arguments.
static void drawHeaderRight(TFT_eSPI& g, int rssi, unsigned long ago, int batPct,
                            bool showAgo = true) {
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(1);

    char ps[8];
    snprintf(ps, sizeof(ps), "%d%%", batPct);
    int x = SCREEN_W - 4 - (int)strlen(ps) * 6;
    g.setCursor(x, 5);
    g.print(ps);

    x -= 24;   // battery: 18 body + 2 nub + 4 gap before the text
    g.drawRect(x, 4, 18, 10, C_TEXT);
    g.fillRect(x + 18, 7, 2, 4, C_TEXT);
    int fw = 14 * constrain(batPct, 0, 100) / 100;
    if (fw > 0) g.fillRect(x + 2, 6, fw, 6, C_TEXT);

    x -= 21;   // wifi: 15 wide + 6 gap
    drawWifiIcon(g, x, rssi);

    if (showAgo) {
        char as[12];
        snprintf(as, sizeof(as), "%lus", ago);
        g.setCursor(x - 6 - (int)strlen(as) * 6, 5);
        g.print(as);
    }
}

// ── Clock mode (full-screen) ─────────────────────────────
// Header giống dashboard; giờ:phút:giây cỡ lớn giữa màn; hàng dưới chia đôi:
// trái = DD/MM/YYYY, phải = thứ (Mon/Tue...). Vẽ trực tiếp lên panel; các board
// dùng sprite (CrowPanel) không bật MANGO clock nên không cần nhánh sprite ở đây.
void uiClockScreen(unsigned long lastFetchMs, int rssi, bool full) {
    auto& g = lcd;
    if (full) halClear(C_BG);   // tick mỗi giây (full=false) không clear → không nháy

    // Header (dải cam) — dùng lại đúng layout của uiDashboard.
    g.fillRect(0, 0, SCREEN_W, SY(18), C_HEAD);
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(TS(1));
    g.setCursor(SX(4), SY(5));
    g.print("CLAUDE USAGE");
    unsigned long ago = (millis() - lastFetchMs) / 1000;
    drawHeaderRight(g, rssi, ago, halBatPercent(), false);   // false = ẩn bộ đếm giây ở clock mode

    // Lấy giờ hệ thống (đã sync NTP trong setup). Nếu chưa có giờ hợp lệ
    // (getLocalTime thất bại), báo nhẹ thay vì in giờ rác.
    struct tm t;
    if (!getLocalTime(&t, 100)) {
        g.setTextColor(C_DIM, C_BG);
        g.setTextSize(TS(2));
        g.setCursor(SX(10), SY(70));
        g.print("SYNCING TIME...");
        UI_PUSH_DASH();
        return;
    }

    // Tên tháng & thứ đầy đủ (định dạng "29 July 2026" / "Wednesday").
    static const char* const MON[] = {"January","February","March","April","May","June",
                                      "July","August","September","October","November","December"};
    static const char* const MON_ABBR[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                           "Jul","Aug","Sep","Oct","Nov","Dec"};
    static const char* const WDAY[] = {"Sunday","Monday","Tuesday","Wednesday",
                                       "Thursday","Friday","Saturday"};
    char dstrFull[24], dstrShort[16];
    snprintf(dstrFull,  sizeof(dstrFull),  "%d %s %d",
             t.tm_mday, MON[t.tm_mon % 12],      t.tm_year + 1900);
    snprintf(dstrShort, sizeof(dstrShort), "%d %s %d",
             t.tm_mday, MON_ABBR[t.tm_mon % 12], t.tm_year + 1900);
    const char* wstr = WDAY[t.tm_wday % 7];

#ifdef BOARD_TDISPLAY_S3
    // Panel 320x170, header cao 18 → vùng còn lại 18..170.
    // Giờ:phút:giây cỡ 6 (mỗi ký tự 36x48), căn giữa.
    char hms[9];
    snprintf(hms, sizeof(hms), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    g.setTextDatum(MC_DATUM);
    g.setTextColor(C_CYAN, C_BG);
    g.setTextSize(6);
    g.drawString(hms, SCREEN_W / 2, 82);   // tâm khối giờ, chừa chỗ cho line + hàng dưới
    g.setTextDatum(TL_DATUM);

    // Đường kẻ cam ngăn giữa giờ và hàng ngày/thứ.
    int lineY = 128;
    g.drawFastHLine(SX(8), lineY, SCREEN_W - SX(16), C_HEAD);

    // Hàng dưới: chia đôi màn. Trái = ngày (căn giữa nửa trái, rút gọn nếu tràn),
    // phải = thứ (căn giữa nửa phải). Vạch cam dọc ở chính giữa.
    // Xoá nền cả dải trước để khi sang ngày (chuỗi đổi độ dài) không sót pixel cũ.
    int rowY = 146;
    g.fillRect(0, lineY + 2, SCREEN_W, SCREEN_H - (lineY + 2), C_BG);
    g.setTextSize(2);

    const int cxL = SCREEN_W / 4;        // tâm nửa trái  (80)
    const int cxR = SCREEN_W * 3 / 4;    // tâm nửa phải  (240)
    const int halfMax = SCREEN_W / 2 - SX(12);   // bề rộng tối đa mỗi nửa (chừa lề + vạch)

    // Chọn ngày đầy đủ nếu vừa, không thì rút gọn ("31 Dec 2026").
    const char* dstr = (g.textWidth(dstrFull) <= halfMax) ? dstrFull : dstrShort;

    g.setTextDatum(MC_DATUM);
    g.setTextColor(C_TEXT, C_BG);
    g.drawString(dstr, cxL, rowY + 7);                     // trái, căn giữa

    g.drawFastVLine(SCREEN_W / 2, rowY - 2, 22, C_HEAD);   // vạch ngăn dọc

    g.setTextColor(C_HEAD, C_BG);
    g.drawString(wstr, cxR, rowY + 7);                     // phải, căn giữa
    g.setTextDatum(TL_DATUM);
#else
    // Board MANGO khác (vd M5StickC Plus 240x135): layout thu nhỏ.
    char hms[9];
    snprintf(hms, sizeof(hms), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    g.setTextDatum(MC_DATUM);
    g.setTextColor(C_CYAN, C_BG);
    g.setTextSize(TS(3));
    g.drawString(hms, SCREEN_W / 2, SY(60));
    g.setTextDatum(TL_DATUM);

    int lineY = SY(84);
    g.drawFastHLine(SX(6), lineY, SCREEN_W - SX(12), C_HEAD);

    int rowY = SCREEN_H - SY(14);
    g.setTextSize(TS(1));

    const int cxL = SCREEN_W / 4;
    const int cxR = SCREEN_W * 3 / 4;
    const int halfMax = SCREEN_W / 2 - SX(8);
    const char* dstr = (g.textWidth(dstrFull) <= halfMax) ? dstrFull : dstrShort;

    g.setTextDatum(MC_DATUM);
    g.setTextColor(C_TEXT, C_BG);
    g.drawString(dstr, cxL, rowY + SY(4));
    g.drawFastVLine(SCREEN_W / 2, rowY - 1, SY(12), C_HEAD);
    g.setTextColor(C_HEAD, C_BG);
    g.drawString(wstr, cxR, rowY + SY(4));
    g.setTextDatum(TL_DATUM);
#endif

    UI_PUSH_DASH();
}

// ── Weather mode (full-screen) ───────────────────────────
// Trái: icon lớn + (thành phố / nhiệt độ / mô tả). Phải: các cột forecast
// (giờ trên; nhiệt độ + icon nhỏ dưới). Layout mô phỏng đồng hồ thời tiết.
void uiWeatherScreen(const WeatherData& wx, int rssi) {
    auto& g = lcd;
    halClear(C_BG);
    // Icon là RGB565 big-endian (do bmp2icons.py sinh). TFT_eSPI mặc định đọc
    // little-endian → cần bật swap bytes, nếu không màu sẽ loạn/nhiễu hạt.
    g.setSwapBytes(true);

    // Header cam.
    g.fillRect(0, 0, SCREEN_W, SY(18), C_HEAD);
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(TS(1));
    g.setCursor(SX(4), SY(5));
    g.print("WEATHER");
    drawHeaderRight(g, rssi, 0, halBatPercent(), false);

    if (!wx.ok) {
        g.setTextColor(C_DIM, C_BG);
        g.setTextSize(TS(2));
        g.setTextDatum(MC_DATUM);
        g.drawString(wx.error[0] ? wx.error : "NO DATA", SCREEN_W / 2, SCREEN_H / 2);
        g.setTextDatum(TL_DATUM);
        UI_PUSH_DASH();
        return;
    }

#ifdef BOARD_TDISPLAY_S3
    const int headerH = 18;
    const int midY    = 94;                     // vạch cam ngang chia trên/dưới

    // ══ PHẦN TRÊN: thời tiết hiện tại — 3 khu đều nhau ══
    const int zoneW = SCREEN_W / 3;             // ~106px mỗi khu
    const int z1cx  = zoneW / 2;                // tâm khu 1 (~53)
    const int z2cx  = zoneW + zoneW / 2;        // tâm khu 2 (~160)
    const int z3cx  = 2 * zoneW + zoneW / 2;    // tâm khu 3 (~266)
    const int topMid = (headerH + midY) / 2;    // tâm dọc vùng trên (~56)

    // Khu 1: icon lớn 50x50, căn giữa khu.
    const uint16_t* icon = weatherIconByName(wx.iconName);
    int iconX = z1cx - ICON_W / 2;
    int iconY = topMid - ICON_H / 2;
    if (icon) {
        g.pushImage(iconX, iconY, ICON_W, ICON_H, icon);
    } else {
        g.drawRect(iconX, iconY, ICON_W, ICON_H, C_HEAD);
        g.setTextColor(C_HEAD, C_BG); g.setTextDatum(MC_DATUM); g.setTextSize(3);
        g.drawString("?", z1cx, topMid);
        g.setTextDatum(TL_DATUM);
    }

    // Khu 2: nhiệt độ lớn + dấu độ + đơn vị, căn giữa khu.
    const char* unit = (strcmp(OWM_UNITS, "imperial") == 0) ? "F" : "C";
    char ts[8];
    snprintf(ts, sizeof(ts), "%d", (int)lroundf(wx.temp));
    // Đo bề rộng cụm số (size 5) + dấu độ (vòng tròn) + đơn vị (size 2) để căn giữa.
    g.setTextSize(5);
    int numW = g.textWidth(ts);
    int degR = 4;                               // bán kính dấu độ cho cụm lớn
    g.setTextSize(2);
    int unitW = g.textWidth(unit);
    int gap = 4;
    int clusterW = numW + gap + degR * 2 + unitW;
    int startX = z2cx - clusterW / 2;
    // Số.
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(5);
    g.setTextDatum(TL_DATUM);
    int numY = topMid - 20;                     // size5 cao ~40 → top = tâm-20
    g.drawString(ts, startX, numY);
    // Dấu độ (vòng tròn) ở góc trên phải của số.
    int degCx = startX + numW + gap + degR;
    g.drawCircle(degCx, numY + 6, degR, C_TEXT);
    // Đơn vị C/F, canh chân số.
    g.setTextSize(2);
    g.drawString(unit, degCx + degR + 2, numY + 22);

    // Khu 3: info 3 dòng (mô tả / mưa / độ ẩm), cả khối căn giữa khu theo chiều dọc.
    char descLine[18];
    strlcpy(descLine, wx.desc, sizeof(descLine));
    if (descLine[0] >= 'a' && descLine[0] <= 'z') descLine[0] -= 32;
    char l2[18], l3[18];
    snprintf(l2, sizeof(l2), "Rain: %d%%", wx.pop);
    snprintf(l3, sizeof(l3), "Humid: %d%%", wx.humidity);
    g.setTextSize(1);
    g.setTextDatum(MC_DATUM);
    int lh = 16;                                // khoảng cách dòng
    int block0 = topMid - lh;                   // 3 dòng: -lh, 0, +lh quanh tâm
    g.setTextColor(C_TEXT, C_BG);
    g.drawString(descLine, z3cx, block0);
    g.setTextColor(C_DIM, C_BG);
    g.drawString(l2, z3cx, block0 + lh);
    g.drawString(l3, z3cx, block0 + 2 * lh);
    g.setTextDatum(TL_DATUM);

    // Vạch cam ngang chia trên/dưới.
    g.drawFastHLine(8, midY, SCREEN_W - 16, C_HEAD);

    // ══ PHẦN DƯỚI: 6 cột forecast (giờ / icon / nhiệt độ) ══
    int colW = SCREEN_W / FORECAST_SLOTS;
    for (int i = 0; i < FORECAST_SLOTS; i++) {
        const ForecastSlot& fs = wx.slots[i];
        int cx = i * colW + colW / 2;

        // Vạch cam dọc phân cách giữa các cột (trước mỗi cột, trừ cột đầu).
        if (i > 0)
            g.drawFastVLine(i * colW, midY + 6, SCREEN_H - midY - 12, C_HEAD);

        if (!fs.valid) continue;

        // Giờ (trên).
        char hh[6]; snprintf(hh, sizeof(hh), "%02d:00", fs.hour);
        g.setTextColor(C_HEAD, C_BG);
        g.setTextSize(1);
        g.setTextDatum(MC_DATUM);
        g.drawString(hh, cx, midY + 12);

        // Icon nhỏ 28x28 (giữa).
        const uint16_t* sic = weatherIconSmByName(fs.iconName);
        int sx = cx - ICONSM_W / 2;
        int sy = midY + 22;
        if (sic) {
            g.pushImage(sx, sy, ICONSM_W, ICONSM_H, sic);
        } else {
            g.drawRect(sx, sy, ICONSM_W, ICONSM_H, C_HEAD_DK);
        }

        // Nhiệt độ (dưới) — số + dấu độ vẽ bằng vòng tròn nhỏ (đẹp hơn chữ 'o').
        char ft[5]; snprintf(ft, sizeof(ft), "%d", fs.temp);
        g.setTextColor(C_TEXT, C_BG);
        g.setTextSize(1);
        int ftw = g.textWidth(ft);
        int fDegR = 2;                               // bán kính dấu độ (cột nhỏ)
        int totalW = ftw + fDegR * 2 + 2;            // bề rộng cả cụm "22°"
        int startX = cx - totalW / 2;                // căn giữa cụm quanh tâm cột
        int ty = midY + 60;
        g.setTextDatum(TL_DATUM);
        g.drawString(ft, startX, ty - 3);            // TL: bù nửa chiều cao chữ (~7px)
        // Vòng tròn dấu độ, đặt hơi cao ngang đỉnh số.
        g.drawCircle(startX + ftw + fDegR + 1, ty - 3, fDegR, C_TEXT);
        g.setTextDatum(MC_DATUM);
    }
    g.setTextDatum(TL_DATUM);
#else
    // Board MANGO nhỏ: chỉ hiện tại (không đủ chỗ cho cột forecast).
    const uint16_t* icon = weatherIconByName(wx.iconName);
    if (icon) g.pushImage(SX(6), SY(24), ICON_W, ICON_H, icon);
    char ts[10];
    snprintf(ts, sizeof(ts), "%d%c", (int)lroundf(wx.temp), 'o');
    g.setTextColor(C_CYAN, C_BG);
    g.setTextSize(TS(3));
    g.setTextDatum(TL_DATUM);
    g.drawString(ts, SX(6) + ICON_W + SX(6), SY(30));
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(TS(1));
    g.drawString(wx.desc, SX(6), SCREEN_H - SY(16));
#endif

    g.setSwapBytes(false);   // trả lại mặc định cho các màn khác
    UI_PUSH_DASH();
}

// ── Moon phase (sub-view của Weather) ────────────────────
void uiMoonScreen(const MoonData& moon, int rssi) {
    auto& g = lcd;
    halClear(C_BG);
    g.setSwapBytes(true);

    // Header cam.
    g.fillRect(0, 0, SCREEN_W, SY(18), C_HEAD);
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(TS(1));
    g.setCursor(SX(4), SY(5));
    g.print("MOON PHASE");
    drawHeaderRight(g, rssi, 0, halBatPercent(), false);

#ifdef BOARD_TDISPLAY_S3
    const int headerH = 18;
    const int midY    = 120;                    // ranh giới 2/3 trên - 1/3 dưới
    const int zoneW   = SCREEN_W / 3;

    // ══ PHẦN TRÊN 2/3: 3 khu đều nhau ══
    const int topMid = (headerH + midY) / 2;    // tâm dọc vùng trên (~69)

    // Khu 1: ảnh trăng 75x75 căn giữa khu.
    char fname[16];
    snprintf(fname, sizeof(fname), "m-phase-%d", moon.imageIndex);
    const uint16_t* img = moonByName(fname);
    int imgX = zoneW / 2 - MOON_W / 2;
    int imgY = topMid - MOON_H / 2;
    if (img) {
        g.pushImage(imgX, imgY, MOON_W, MOON_H, img);
    } else {
        g.drawCircle(zoneW / 2, topMid, MOON_W / 2, C_HEAD);
    }

    // Khu 2: ngày size 3 (giả bold), tự rút gọn tháng nếu tràn; tên pha size 1.
    static const char* const MON[] = {"January","February","March","April","May","June",
        "July","August","September","October","November","December"};
    static const char* const MON_ABBR[] = {"Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"};
    int z2x = zoneW + 6;
    int z2max = zoneW - 12;                      // bề rộng tối đa khu 2 (chừa lề)
    char dline[20];
    snprintf(dline, sizeof(dline), "%s %d", MON[(moon.month - 1) % 12], moon.day);
    g.setTextSize(2);
    if (g.textWidth(dline) > z2max)              // tràn → dùng tháng viết tắt
        snprintf(dline, sizeof(dline), "%s %d", MON_ABBR[(moon.month - 1) % 12], moon.day);

    // Vẽ ngày size 2, giả bold bằng cách vẽ đè lệch 1px.
    g.setTextDatum(TL_DATUM);
    g.setTextColor(C_TEXT, C_BG);
    int dY = topMid - 12;
    g.drawString(dline, z2x, dY);
    g.drawString(dline, z2x + 1, dY);            // lệch 1px → nét đậm hơn

    // Tên pha size 1.
    g.setTextSize(1);
    g.setTextColor(C_CYAN, C_BG);
    g.drawString(moon.phaseName, z2x, dY + 22);

    // Khu 3: nhãn (trái) + giá trị (thẳng cột). Illum/Age trắng, Rise/Set cam.
    int z3x = 2 * zoneW + 6;
    int valX = z3x + 46;                          // cột giá trị căn thẳng hàng
    char ilV[10], agV[10], mrV[10], msV[10];
    snprintf(ilV, sizeof(ilV), "%d%%", moon.illumPct);
    snprintf(agV, sizeof(agV), "%.1fd", moon.ageDays);
    if (moon.riseValid) snprintf(mrV, sizeof(mrV), "%02d:%02d", moon.riseH, moon.riseM);
    else                strlcpy(mrV, "--:--", sizeof(mrV));
    if (moon.setValid)  snprintf(msV, sizeof(msV), "%02d:%02d", moon.setH, moon.setM);
    else                strlcpy(msV, "--:--", sizeof(msV));

    g.setTextSize(1);
    g.setTextDatum(TL_DATUM);
    int r0 = topMid - 26, rh = 16;
    // Illum + Age: trắng.
    g.setTextColor(C_TEXT, C_BG);
    g.drawString("Illum", z3x, r0);        g.drawString(ilV, valX, r0);
    g.drawString("Age",   z3x, r0 + rh);   g.drawString(agV, valX, r0 + rh);
    // Rise + Set: cam.
    g.setTextColor(C_HEAD, C_BG);
    g.drawString("Rise",  z3x, r0 + 2 * rh); g.drawString(mrV, valX, r0 + 2 * rh);
    g.drawString("Set",   z3x, r0 + 3 * rh); g.drawString(msV, valX, r0 + 3 * rh);

    // Vạch cam ngang chia trên/dưới.
    g.drawFastHLine(8, midY, SCREEN_W - 16, C_HEAD);

    // ══ PHẦN DƯỚI 1/3: dải 7 pha (ô giữa = hiện tại, có khung cam) ══
    const int N = 7;
    int cellW = SCREEN_W / N;
    int smY = midY + (SCREEN_H - midY) / 2 - MOONSM_H / 2;
    for (int i = 0; i < N; i++) {
        int off = i - N / 2;                    // -3..+3 quanh pha hiện tại
        int pidx = ((moon.imageIndex + off) % 31 + 31) % 31;
        int ccx = i * cellW + cellW / 2;
        int sx = ccx - MOONSM_W / 2;

        char sf[16];
        snprintf(sf, sizeof(sf), "m-phase-%d", pidx);
        const uint16_t* sim = moonsmByName(sf);
        if (sim) g.pushImage(sx, smY, MOONSM_W, MOONSM_H, sim);
        else     g.drawCircle(ccx, smY + MOONSM_H / 2, MOONSM_W / 2, C_HEAD_DK);

        // Khung cam cho ô hiện tại (giữa).
        if (off == 0) {
            g.drawRect(sx - 2, smY - 2, MOONSM_W + 4, MOONSM_H + 4, C_HEAD);
            g.drawRect(sx - 3, smY - 3, MOONSM_W + 6, MOONSM_H + 6, C_HEAD);
        }
    }
#else
    // Board nhỏ: ảnh trăng + tên pha.
    char fname[16];
    snprintf(fname, sizeof(fname), "m-phase-%d", moon.imageIndex);
    const uint16_t* img = moonByName(fname);
    if (img) g.pushImage(SX(6), SY(24), MOON_W, MOON_H, img);
    g.setTextColor(C_CYAN, C_BG);
    g.setTextSize(TS(1));
    g.setTextDatum(TL_DATUM);
    g.drawString(moon.phaseName, SX(6), SCREEN_H - SY(16));
#endif

    g.setSwapBytes(false);
    UI_PUSH_DASH();
}
#endif // MANGO_UI

void uiInit() {
    lcd.setRotation(SCREEN_ROT);
    halClear(C_BG);
    halFlush();
}

void uiBootProgress(int percent, const char* label) {
    halClear(C_BG);

    lcd.setTextColor(C_ACCENT, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(30), SY(20));
    lcd.print("Claude Usage");

    int bx = SX(20), by = SY(60), bw = SCREEN_W - SX(40), bh = SY(14);
    lcd.fillRect(bx, by, bw, bh, C_BAR_BG);
    int fill = constrain((int)(bw * percent / 100.0f), 0, bw);
    lcd.fillRect(bx, by, fill, bh, C_ACCENT);

    char ps[8];
    snprintf(ps, sizeof(ps), "%d%%", percent);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(TS(1));
    lcd.setCursor(bx + bw / 2 - strlen(ps) * TS(3), by + bh + SY(6));
    lcd.print(ps);

    lcd.setCursor(SX(20), SY(100));
    lcd.print(label);

#ifdef MANGO_UI
    lcd.setCursor(SCREEN_W - SX(4) - (int)strlen("v" FW_VERSION) * TS(6), SCREEN_H - SY(12));
    lcd.print("v" FW_VERSION);
#endif
    halFlush();
}

void uiSetupScreen(const char* apName, const char* apPass) {
    halClear(C_BG);

    lcd.fillRect(0, 0, SCREEN_W, SY(18), C_ACCENT);
    lcd.setTextColor(C_TEXT, C_ACCENT);
    lcd.setTextSize(TS(1));
    lcd.setCursor(SX(6), SY(5));
    lcd.print("SETUP MODE");

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(TS(1));
    lcd.setCursor(SX(10), SY(24));
    lcd.print("1. Connect to WiFi:");

    lcd.setTextColor(C_CYAN, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(10), SY(36));
    lcd.print(apName);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(TS(1));
    lcd.setCursor(SX(10), SY(56));
    lcd.print("Password:");
    lcd.setTextColor(C_CYAN, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(10), SY(68));
    lcd.print(apPass);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(TS(1));
    lcd.setCursor(SX(10), SY(92));
    lcd.print("2. Open in browser:");

    lcd.setTextColor(C_CYAN, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(10), SY(104));
    lcd.print("192.168.4.1");
    halFlush();
}

void uiPinScreen(int pos, const int digits[4]) {
#ifdef BOARD_CROWPANEL_ADV_35
    auto& g = dashTarget();
    g.fillSprite(C_BG);
#else
    auto& g = lcd;
    halClear(C_BG);
#endif

    int boxW = SX(30), boxH = SY(36), gap = SX(12);
    int startX = (SCREEN_W - (4 * boxW + 3 * gap)) / 2;
#ifdef MANGO_UI
    int boxY = (SCREEN_H - boxH) / 2;   // dead-center of the screen
#else
    int boxY = SY(40);
#endif

#ifdef MANGO_UI
    // Same dress code as the dashboard: orange header band + Clawd standing guard.
    // Label/hint/note positions hang off boxY so the block stays centered on both
    // the 320x170 (S3) and 240x135 (M5StickC Plus) panels.
    g.fillRect(0, 0, SCREEN_W, 18, C_HEAD);
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(1);
    g.setCursor(4, 5);
    g.print("CLAUDE USAGE");
    g.setCursor(SCREEN_W - 4 - 6 * 6, 5);
    g.print("LOCKED");
    g.setTextColor(C_DIM, C_BG);
    g.setCursor((SCREEN_W - 10 * 6) / 2, boxY - 18);
    g.print("UNLOCK PIN");
#else
    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(TS(1));
    g.setCursor(SX(70), SY(15));
    g.print("UNLOCK PIN");
#endif

    for (int i = 0; i < 4; i++) {
        int x = startX + i * (boxW + gap);
        uint16_t borderCol = (i == pos) ? C_CYAN : C_DIM;

        g.drawRect(x, boxY, boxW, boxH, borderCol);
        if (i == pos) g.drawRect(x + 1, boxY + 1, boxW - 2, boxH - 2, borderCol);

        g.setTextSize(TS(3));
        if (i < pos) {
            g.setTextColor(C_ACCENT, C_BG);
            g.setCursor(x + SX(9), boxY + SY(7));
            g.print("*");
        } else if (i == pos) {
            g.setTextColor(C_TEXT, C_BG);
            g.setCursor(x + SX(9), boxY + SY(7));
            g.print(digits[i]);
        }
    }

#ifdef MANGO_UI
    const int hintY = boxY + boxH + 12;   // just below the centered boxes
#else
    const int hintY = SY(95);
#endif
    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(TS(1));
    g.setCursor(SX(20), hintY);
#ifdef BOARD_T8_S2
    // Single BOOT button: short tap cycles the digit, long press confirms.
    g.print("[tap] cycle digit");
    g.setCursor(SX(148), SY(95));
    g.print("[hold] confirm");

    g.setCursor(SX(35), SY(118));
    g.setTextColor(0x4A49, C_BG);
    g.print("short tap = A    long press = B");
    halFlush();
#elif defined(BOARD_CROWPANEL_ADV_35)
    // Touch HMI — stack the hints vertically; the side-by-side layout overflows at this scale.
    g.print("tap LEFT  = next digit");
    g.setCursor(SX(20), SY(110));
    g.print("tap RIGHT = confirm");
    g.setCursor(SX(20), SY(124));
    g.setTextColor(0x4A49, C_BG);
    g.print("re-flash to factory reset");

    // Push the full frame on entry (all digits empty at pos 0); afterwards push only the
    // digit-box band — a full-width region is contiguous in the sprite buffer, so each tap
    // updates quickly and without the flicker a direct clear-and-redraw would cause.
    if (pos == 0 && digits[0] == 0 && digits[1] == 0 && digits[2] == 0 && digits[3] == 0) {
        s_dash.pushSprite(0, 0);
    } else {
        int bandY = SY(38), bandH = SY(42);
        lcd.pushImage(0, bandY, SCREEN_W, bandH,
                      (uint16_t*)s_dash.getPointer() + (size_t)bandY * SCREEN_W);
    }
#else
#ifdef MANGO_UI
    static const char* hint = "[A] cycle digit   [B] confirm";
    g.setCursor((SCREEN_W - (int)strlen(hint) * 6) / 2, hintY);
    g.print(hint);

    static const char* note = "Hold A+B on boot = factory reset";
    g.setTextColor(0x4A49, C_BG);
    g.setCursor((SCREEN_W - (int)strlen(note) * 6) / 2, boxY + boxH + 32);
    g.print(note);
#else
    g.print("[A] cycle digit");
    g.setCursor(SX(148), hintY);
    g.print("[B] confirm");

    g.setCursor(SX(35), SY(118));
    g.setTextColor(0x4A49, C_BG);
    g.print("Hold A+B on boot = factory reset");
#endif
    halFlush();
#endif
}

void uiConnecting(const char* ssid, int attempt) {
    halClear(C_BG);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(TS(1));
    lcd.setCursor(SX(10), SY(40));
    lcd.print("Connecting to WiFi...");

    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(10), SY(58));
    lcd.print(ssid);

    if (attempt > 0) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(TS(1));
        lcd.setCursor(SX(10), SY(90));
        lcd.printf("Attempt %d", attempt);
    }
    halFlush();
}

void uiDashboard(const UsageData& data, unsigned long lastFetchMs, int rssi, int batPct) {
#ifdef BOARD_CROWPANEL_ADV_35
    auto& g = dashTarget();
    g.fillSprite(C_BG);
#else
    auto& g = lcd;   // deduces the board's real surface type (panel, sprite, or M5 LCD)
    halClear(C_BG);
#endif

    // Header
    g.fillRect(0, 0, SCREEN_W, SY(18), C_HEAD);
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(TS(1));
    g.setCursor(SX(4), SY(5));
    g.print("CLAUDE USAGE");

    unsigned long ago = (millis() - lastFetchMs) / 1000;
#ifdef MANGO_UI
    drawHeaderRight(g, rssi, ago, batPct);
#else
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%ddBm %lus", rssi, ago);
    g.setCursor(SCREEN_W - strlen(hdr) * TS(6) - SX(4), SY(5));
    g.print(hdr);
#endif

    if (!data.ok) {
        g.setTextColor(C_CRIT, C_BG);
        g.setTextSize(TS(2));
        g.setCursor(SX(10), SY(35));
        g.print("ERROR");
        g.setTextSize(TS(1));
        g.setTextColor(C_DIM, C_BG);
        g.setCursor(SX(10), SY(60));
        g.print(data.error);
        g.setCursor(SX(10), SY(80));
#ifdef MANGO_UI
        g.print("retrying automatically...");   // B is brightness on this board
#else
        g.print("[B] retry now");
#endif
        UI_PUSH_DASH();
        return;
    }

    int barW = SCREEN_W - SX(20);

    char h5rst[16], d7rst[16];
    fmtCountdown(data.h5ResetEpoch, h5rst, sizeof(h5rst));
    fmtCountdown(data.d7ResetEpoch, d7rst, sizeof(d7rst));

#ifdef MANGO_UI
#ifdef BOARD_TDISPLAY_S3
    // Tier L: % flush-right on the bar rows; the countdowns get their own
    // size-2 row below the bars.
    drawBar(g, SX(10), SY(24), barW, SY(10), data.h5, "5-HOUR");
    drawBar(g, SX(10), SY(52), barW, SY(10), data.d7, "7-DAY");
    drawResetRow(g, h5rst, d7rst);
#else
    // Tier S: each reset countdown rides on its bar's label row — no room below.
    drawBar(g, SX(10), SY(24), barW, SY(10), data.h5, "5-HOUR", h5rst);
    drawBar(g, SX(10), SY(52), barW, SY(10), data.d7, "7-DAY",  d7rst);
#endif
    drawStatusPanel(g);
#else
    drawBar(g, SX(10), SY(24), barW, SY(10), data.h5, "5-HOUR WINDOW");
    drawBar(g, SX(10), SY(52), barW, SY(10), data.d7, "7-DAY WINDOW");

    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(TS(1));
    g.setCursor(SX(10), SY(80));
    g.print("5H RST");
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(TS(2));
    g.setCursor(SX(10), SY(92));
    g.printf("%-8s", h5rst);

    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(TS(1));
    g.setCursor(SCREEN_W / 2 + SX(10), SY(80));
    g.print("7D RST");
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(TS(2));
    g.setCursor(SCREEN_W / 2 + SX(10), SY(92));
    g.printf("%-8s", d7rst);

    g.setTextColor(C_DIM, C_BG);
    g.setTextSize(TS(1));
    g.setCursor(SCREEN_W - SX(48), SY(120));
    g.printf("BAT %d%%", batPct);
#endif
    UI_PUSH_DASH();
}

void uiDashboardClock(const UsageData& data, unsigned long lastFetchMs, int rssi) {
    if (!data.ok) return;   // error layout is owned by the full uiDashboard
#ifdef BOARD_CROWPANEL_ADV_35
    auto& g = dashTarget();   // update the retained sprite, then push it once
#else
    auto& g = lcd;
#endif

    // Header "rssi / ago": repaint the header band over its own colour.
    unsigned long ago = (millis() - lastFetchMs) / 1000;
    g.fillRect(SCREEN_W / 2, 0, SCREEN_W / 2, SY(18), C_HEAD);
    g.setTextColor(C_TEXT, C_HEAD);
    g.setTextSize(TS(1));
#ifdef MANGO_UI
    drawHeaderRight(g, rssi, ago, halBatPercent());
#else
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%ddBm %lus", rssi, ago);
    g.setCursor(SCREEN_W - strlen(hdr) * TS(6) - SX(4), SY(5));
    g.print(hdr);
#endif

    // Reset countdowns: repaint in place.
    char h5rst[16], d7rst[16];
    fmtCountdown(data.h5ResetEpoch, h5rst, sizeof(h5rst));
    fmtCountdown(data.d7ResetEpoch, d7rst, sizeof(d7rst));
#ifdef MANGO_UI
#ifdef BOARD_TDISPLAY_S3
    // Tier L: the countdowns live on their own row below the bars.
    drawResetValues(g, h5rst, d7rst);
#else
    // Tier S: the countdowns live on the bar rows; refresh just those slots.
    int barW = SCREEN_W - SX(20);
    drawResetSlot(g, SX(10), barW, SY(24), h5rst);
    drawResetSlot(g, SX(10), barW, SY(52), d7rst);
#endif
#else
    g.setTextColor(C_TEXT, C_BG);
    g.setTextSize(TS(2));
    g.setCursor(SX(10), SY(92));
    g.printf("%-8s", h5rst);
    g.setCursor(SCREEN_W / 2 + SX(10), SY(92));
    g.printf("%-8s", d7rst);
#endif

    UI_PUSH_DASH();
}

void uiError(const char* title, const char* detail) {
    halClear(C_BG);
    lcd.setTextColor(C_CRIT, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(10), SY(30));
    lcd.print(title);
    if (detail) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(TS(1));
        lcd.setCursor(SX(10), SY(60));
        lcd.print(detail);
    }
    halFlush();
}

void uiLockout(int attempts, int maxAttempts, int lockoutSec) {
    halClear(C_BG);
    lcd.setTextColor(C_CRIT, C_BG);
    lcd.setTextSize(TS(2));
    lcd.setCursor(SX(10), SY(25));
    lcd.print("WRONG PIN");

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(TS(1));
    lcd.setCursor(SX(10), SY(55));
    lcd.printf("Attempt %d of %d", attempts, maxAttempts);
    lcd.setCursor(SX(10), SY(75));
    lcd.printf("Locked for %d seconds", lockoutSec);

    for (int s = lockoutSec; s > 0; s--) {
        lcd.fillRect(SX(10), SY(95), SX(200), SY(20), C_BG);
        lcd.setTextColor(C_WARN, C_BG);
        lcd.setTextSize(TS(2));
        lcd.setCursor(SX(10), SY(95));
        lcd.printf("%ds", s);
        halFlush();
        delay(1000);
    }
}

