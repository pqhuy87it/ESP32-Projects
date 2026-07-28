/*
 * Claude Code Usage Monitor — Standalone WiFi
 * Supports: M5StickC Plus, M5StickC Plus2, LilyGo T-Display S3, ESP32-C3-OLED,
 *           ideaspark ESP32 1.9" ST7789 (170x320)
 *
 * PIN entry: A cycles the digit, B confirms
 * Dashboard (Clarity): A cycles brightness, B forces a refresh
 * Dashboard (Mango):   A flips the screen, B cycles brightness, A+B force refresh
 * A+B held on boot: factory reset → wipe NVS → re-enter setup
 *
 * ESP32-C3-OLED wiring (both buttons external, active-LOW to GND):
 *   Button A → GPIO 3     Button B → GPIO 7
 *   SDA → GPIO 5          SCL → GPIO 6
 *   GPIO 9 (BO0): download mode only — do NOT wire a button here
 *
 * ideaspark ESP32 1.9" (ESP32-WROOM-32, ST7789 170x320 SPI, 16MB flash):
 *   LCD (đã đi dây sẵn): MOSI 23, SCLK 18, CS 15, DC 2, RST 4, BLK 32
 *   Board không có user button → hàn 2 nút ngoài, active-LOW xuống GND:
 *     Button A → GPIO 27    Button B → GPIO 26
 *   Không có mạch pin → halBatPercent() trả -1, UI ẩn icon battery.
 *   Nếu bật IDEASPARK_SINGLE_BUTTON (chỉ dùng nút BOOT): short = A, long = B,
 *   và factory reset lúc boot bị vô hiệu (giữ GPIO0 khi boot = download mode).
 */

#include "hal.h"
#include <WiFi.h>
#include <Preferences.h>
#include "esp_mac.h"      // esp_efuse_mac_get_default (core 3.x)
#include "esp_random.h"   // esp_fill_random (core 3.x)
#include "config.h"
#include "board_ideaspark19.h"
#include "crypto.h"
#include "provision.h"
#include "api.h"
#include "ui.h"
#ifdef MANGO_UI
#include "status.h"
#endif

static Preferences prefs;
static char        token[256];
static UsageData   usage;
#ifdef MANGO_UI
static ModelStatus modelStatus = {true, true, true, true, false};
#endif
static unsigned long lastFetch = 0;
static int         pollMs     = DEFAULT_POLL_SEC * 1000;
static uint8_t     brightness = DEFAULT_BRIGHTNESS;

// Board này chỉ có 1 nút vật lý → không thể giữ tổ hợp lúc boot
#if defined(BOARD_IDEASPARK_ESP32_19) && defined(IDEASPARK_SINGLE_BUTTON)
  #define NO_BOOT_COMBO 1
#endif

// ── PIN Entry (blocks until 4 digits confirmed) ────────
static void enterPin(char* pinOut, int maxLen) {
    int digits[4] = {0, 0, 0, 0};
    int pos = 0;

    while (pos < 4) {
        uiPinScreen(pos, digits);
        while (true) {
            halUpdate();
            if (halBtnAWasPressed()) { digits[pos] = (digits[pos] + 1) % 10; break; }
            if (halBtnBWasPressed()) { pos++; break; }
            delay(20);
        }
    }
    snprintf(pinOut, maxLen, "%d%d%d%d", digits[0], digits[1], digits[2], digits[3]);
}

// ── WiFi ───────────────────────────────────────────────
static bool connectWiFi(const char* ssid, const char* pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    int ticks = 0;
    while (WiFi.status() != WL_CONNECTED) {
        ticks++;
        uiConnecting(ssid, ticks / 2);
        delay(500);
        if (ticks > WIFI_CONNECT_TIMEOUT_S * 2) return false;
    }
    return true;
}

// ── Sync NTP for reset countdown display ───────────────
static void syncTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm t;
    getLocalTime(&t, 5000);
}

// ── Fetch + draw ───────────────────────────────────────
static void refresh() {
    if (WiFi.status() != WL_CONNECTED) {
        prefs.begin(NVS_NAMESPACE, true);
        connectWiFi(prefs.getString("ssid", "").c_str(),
                    prefs.getString("wifipass", "").c_str());
        prefs.end();
    }
    fetchUsage(token, usage);
#ifdef MANGO_UI
    fetchModelStatus(modelStatus);   // failure keeps last-known state
    uiSetModelStatus(modelStatus);
#endif
    lastFetch = millis();
    uiDashboard(usage, lastFetch, WiFi.RSSI(), halBatPercent());
}

// ── Setup ──────────────────────────────────────────────
void setup() {
    halInit();
    uiInit();

    uiBootProgress(10, "Initializing...");
    delay(300);

    uiBootProgress(30, "Checking config...");
    delay(200);

#ifndef NO_BOOT_COMBO
    // Factory reset: both buttons must be held continuously for 2 seconds.
    // A single snapshot can mis-fire on boards where GPIOs float LOW briefly;
    // repeated sampling over 2 s eliminates false triggers.
    halUpdate();
    if (halBtnAIsPressed() && halBtnBIsPressed()) {
        uiBootProgress(40, "Hold A+B 2s...");
        bool held = true;
        for (int i = 0; i < 20 && held; i++) {
            delay(100);
            halUpdate();
            if (!halBtnAIsPressed() || !halBtnBIsPressed()) held = false;
        }
        if (held) {
            uiBootProgress(50, "Factory reset...");
            prefs.begin(NVS_NAMESPACE, false);
            prefs.clear();
            prefs.end();
            uiError("NVS WIPED", "Rebooting in 2s...");
            delay(2000);
            ESP.restart();
        }
    }
#else
    // Chỉ có nút BOOT: giữ nút lúc cấp nguồn sẽ vào download mode nên không
    // dùng được. Thay bằng: giữ BOOT liên tục 5 s trong 5 s đầu sau khi boot.
    {
        uiBootProgress(40, "Hold BOOT 5s to reset");
        uint32_t t0 = millis(), downSince = 0;
        while (millis() - t0 < 5000) {
            halUpdate();
            bool down = halBtnAIsPressed() || halBtnBIsPressed();
            if (down && !downSince) downSince = millis();
            if (!down) downSince = 0;
            if (downSince && millis() - downSince >= 5000) break;
            delay(20);
        }
        if (downSince && millis() - downSince >= 5000) {
            uiBootProgress(50, "Factory reset...");
            prefs.begin(NVS_NAMESPACE, false);
            prefs.clear();
            prefs.end();
            uiError("NVS WIPED", "Rebooting in 2s...");
            delay(2000);
            ESP.restart();
        }
    }
#endif

    // Check provisioned
    prefs.begin(NVS_NAMESPACE, true);
    bool provisioned = prefs.getBool("provisioned", false);
    prefs.end();

    if (!provisioned) {
        uiBootProgress(50, "No config found");
        delay(400);

        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        char apName[24];
        snprintf(apName, sizeof(apName), "ClaudeMonitor-%02X%02X", mac[4], mac[5]);

#ifdef BOARD_ESP32C3_OLED
        // No readable display during setup — use open AP so password isn't needed
        const char* apPass = "";
        Serial.printf("[SETUP] AP: %s (open)\n", apName);
#else
        // ideaspark 1.9" có màn 320x170 đọc tốt → vẫn dùng AP có mật khẩu
        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        uint8_t rnd[8];
        esp_fill_random(rnd, sizeof(rnd));
        char apPass[9];
        for (int i = 0; i < 8; i++) apPass[i] = alphabet[rnd[i] % (sizeof(alphabet) - 1)];
        apPass[8] = '\0';
        Serial.printf("[SETUP] AP: %s  pass: %s\n", apName, apPass);
#endif
        runProvisioningPortal(apName, apPass);
        return;
    }

    uiBootProgress(50, "Config loaded");
    delay(200);

    // Load NVS
    prefs.begin(NVS_NAMESPACE, true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("wifipass", "");
    EncryptedBlob blob;
    prefs.getBytes("blob", &blob, sizeof(blob));
    pollMs     = prefs.getInt("poll_sec", DEFAULT_POLL_SEC) * 1000;
    brightness = prefs.getInt("brightness", DEFAULT_BRIGHTNESS);
    prefs.end();

    halSetBrightness(brightness);

    uiBootProgress(60, "Enter PIN...");
    delay(300);

    // PIN + decrypt loop
    int attempts = 0;
    while (attempts < MAX_PIN_ATTEMPTS) {
        char pin[9];
        enterPin(pin, sizeof(pin));

        if (decryptToken(blob, pin, token, sizeof(token))) break;

        attempts++;
        if (attempts >= MAX_PIN_ATTEMPTS) {
            uiError("MAX ATTEMPTS", "Wiping credentials...");
            prefs.begin(NVS_NAMESPACE, false);
            prefs.clear();
            prefs.end();
            delay(3000);
            ESP.restart();
        }

        int lockSec = LOCKOUT_BASE_SEC * (1 << (attempts - 1));
        if (lockSec > 3600) lockSec = 3600;
        uiLockout(attempts, MAX_PIN_ATTEMPTS, lockSec);
    }

    uiBootProgress(80, "Connecting WiFi...");

    if (!connectWiFi(ssid.c_str(), pass.c_str())) {
        uiError("WIFI FAILED", ssid.c_str());
        delay(5000);
        ESP.restart();
    }

    uiBootProgress(90, "Syncing time...");
    syncTime();

    uiBootProgress(95, "Fetching usage...");
    refresh();
}

// ── Loop ───────────────────────────────────────────────
void loop() {
    halUpdate();

#if defined(MANGO_UI) && !defined(NO_BOOT_COMBO)
    // A flips the screen 180°, B cycles brightness, A+B together = force refresh
    // (the Clarity Button-B action). A single press only commits after a short
    // window so the other button can still join to form the combo.
    static unsigned long aPressAt = 0, bPressAt = 0;
    const unsigned long comboWindowMs = 350;
    if (halBtnAWasPressed()) aPressAt = millis();
    if (halBtnBWasPressed()) bPressAt = millis();

    if ((aPressAt && (bPressAt || halBtnBIsPressed())) ||
        (bPressAt && halBtnAIsPressed())) {
        aPressAt = bPressAt = 0;
        refresh();
    } else if (aPressAt && millis() - aPressAt > comboWindowMs) {
        aPressAt = 0;
        uiToggleRotation();
        uiDashboard(usage, lastFetch, WiFi.RSSI(), halBatPercent());
    } else if (bPressAt && millis() - bPressAt > comboWindowMs) {
        bPressAt = 0;
        brightness = (brightness + 1) % 4;
        halSetBrightness(brightness);
    }
#elif defined(MANGO_UI)
    // Single-button build: không có tổ hợp A+B → A = xoay màn, B = brightness,
    // force refresh dựa vào poll timer.
    if (halBtnAWasPressed()) {
        uiToggleRotation();
        uiDashboard(usage, lastFetch, WiFi.RSSI(), halBatPercent());
    }
    if (halBtnBWasPressed()) {
        brightness = (brightness + 1) % 4;
        halSetBrightness(brightness);
    }
#else
    if (halBtnAWasPressed()) {
#ifdef BOARD_ESP32C3_OLED
        brightness = (brightness + 1) % 2; // on/off only — contrast change imperceptible
#else
        brightness = (brightness + 1) % 4; // ideaspark: LEDC PWM trên GPIO32
#endif
        halSetBrightness(brightness);
    }

    if (halBtnBWasPressed()) {
        refresh();
    }
#endif

    if (millis() - lastFetch >= (unsigned long)pollMs) {
        refresh();
    }

#ifdef MANGO_UI
    // Healthy mascots blink every 2s (eyes shut for 150ms) to show liveness.
    static unsigned long lastBlink = 0;
    static bool eyesClosed = false;
    if (eyesClosed && millis() - lastBlink > 150) {
        uiBlinkTick(false);
        eyesClosed = false;
    } else if (!eyesClosed && usage.ok && millis() - lastBlink > 2000) {
        uiBlinkTick(true);
        eyesClosed = true;
        lastBlink = millis();
    }
#endif

    static unsigned long lastRedraw = 0;
    if (millis() - lastRedraw > 10000) {
        // Only time passed (not data) — update the clock/countdowns in place; redrawing
        // the whole dashboard here is what made the slow CrowPanel panel flicker.
        uiDashboardClock(usage, lastFetch, WiFi.RSSI());
        lastRedraw = millis();
    }

    delay(20);
}
