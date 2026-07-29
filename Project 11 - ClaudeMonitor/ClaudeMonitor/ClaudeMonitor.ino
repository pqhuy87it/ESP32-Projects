/*
 * Claude Code Usage Monitor — Standalone WiFi
 * Supports: M5StickC Plus, M5StickC Plus2, LilyGo T-Display S3, ESP32-C3-OLED
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
 */

#include "hal.h"
#include <WiFi.h>
#include <Preferences.h>
#include "esp_mac.h"      // esp_efuse_mac_get_default (core 3.x)
#include "config.h"
#include "crypto.h"
#include "provision.h"
#include "api.h"
#include "ui.h"
#ifdef MANGO_UI
#include "status.h"
#endif

// ── PIN cố định (bỏ qua màn hình nhập lúc boot) ──────────
// PHẢI trùng đúng PIN bạn nhập khi setup lần đầu qua web (/provision).
// Nếu đã lỡ setup với PIN khác: giữ A+B lúc boot 2s để factory reset,
// rồi setup lại với PIN "0000" (hoặc đổi giá trị dưới đây cho khớp).
#define FIXED_PIN "0000"

static Preferences prefs;
static char        token[256];
static UsageData   usage;
#ifdef MANGO_UI
static ModelStatus modelStatus = {true, true, true, true, false};
#endif
static unsigned long lastFetch = 0;
static int         pollMs     = DEFAULT_POLL_SEC * 1000;
static uint8_t     brightness = DEFAULT_BRIGHTNESS;
static int         lastSyncYday = -1;   // tm_yday của lần NTP sync gần nhất (để sync lại khi qua 0h)

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
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // UTC+7 (giờ VN)
    struct tm t;
    if (getLocalTime(&t, 5000)) {
        lastSyncYday = t.tm_yday;   // ghi lại ngày trong năm đã sync
    }
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
        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        uint8_t rnd[8];
        esp_fill_random(rnd, sizeof(rnd));
        char apPass[9];
        for (int i = 0; i < 8; i++) apPass[i] = alphabet[rnd[i] % (sizeof(alphabet) - 1)];
        apPass[8] = '\0';
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

    uiBootProgress(60, "Unlocking...");
    delay(200);

    // Giải mã token bằng PIN cố định — bỏ qua màn hình nhập.
    if (!decryptToken(blob, FIXED_PIN, token, sizeof(token))) {
        // Sai PIN cố định so với lúc provisioning, hoặc blob hỏng.
        // Không tự wipe NVS để bạn còn cơ hội sửa FIXED_PIN cho khớp.
        uiError("UNLOCK FAILED", "Check FIXED_PIN / re-setup");
        delay(5000);
        ESP.restart();
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

#ifdef MANGO_UI
    // A flips the screen 180°, B toggles clock mode, A+B together = force refresh.
    // A single press only commits after a short window so the other button can
    // still join to form the combo.
    static unsigned long aPressAt = 0, bPressAt = 0;
    static bool clockMode = false;
    const unsigned long comboWindowMs = 350;
    if (halBtnAWasPressed()) aPressAt = millis();
    if (halBtnBWasPressed()) bPressAt = millis();

    if ((aPressAt && (bPressAt || halBtnBIsPressed())) ||
        (bPressAt && halBtnAIsPressed())) {
        aPressAt = bPressAt = 0;
        refresh();
        if (clockMode) uiClockScreen(lastFetch, WiFi.RSSI());
    } else if (aPressAt && millis() - aPressAt > comboWindowMs) {
        aPressAt = 0;
        uiToggleRotation();
        if (clockMode) uiClockScreen(lastFetch, WiFi.RSSI());
        else           uiDashboard(usage, lastFetch, WiFi.RSSI(), halBatPercent());
    } else if (bPressAt && millis() - bPressAt > comboWindowMs) {
        bPressAt = 0;
        clockMode = !clockMode;   // chuyển đổi dashboard <-> đồng hồ
        if (clockMode) uiClockScreen(lastFetch, WiFi.RSSI());
        else           uiDashboard(usage, lastFetch, WiFi.RSSI(), halBatPercent());
    }
#else
    if (halBtnAWasPressed()) {
#ifdef BOARD_ESP32C3_OLED
        brightness = (brightness + 1) % 2; // on/off only — contrast change imperceptible
#else
        brightness = (brightness + 1) % 4;
#endif
        halSetBrightness(brightness);
    }

    if (halBtnBWasPressed()) {
        refresh();
    }
#endif

    // Fetch định kỳ — nhưng TẠM DỪNG khi đang ở clock mode để giờ chạy mượt
    // (fetchUsage/fetchModelStatus là HTTPS blocking, sẽ làm đứng giây vài giây).
    // lastFetch không đổi khi hoãn, nên vừa thoát clock mode là fetch ngay.
    bool inClockMode = false;
#ifdef MANGO_UI
    inClockMode = clockMode;
#endif
    if (!inClockMode && millis() - lastFetch >= (unsigned long)pollMs) {
        refresh();
    }

#ifdef MANGO_UI
    // NTP: chỉ sync 1 lần lúc boot, sau đó sync lại khi sang ngày mới (qua 0h).
    // Không sync khi đang ở clock mode (tránh block giây); để lần thoát mode xử lý.
    if (!clockMode && WiFi.status() == WL_CONNECTED) {
        struct tm nt;
        if (getLocalTime(&nt, 0) && nt.tm_yday != lastSyncYday) {
            syncTime();
        }
    }
#endif

#ifdef MANGO_UI
    // Healthy mascots blink every 2s (eyes shut for 150ms) to show liveness.
    // Chỉ chạy ở dashboard — clock mode không có mascot.
    static unsigned long lastBlink = 0;
    static bool eyesClosed = false;
    if (!clockMode) {
        if (eyesClosed && millis() - lastBlink > 150) {
            uiBlinkTick(false);
            eyesClosed = false;
        } else if (!eyesClosed && usage.ok && millis() - lastBlink > 2000) {
            uiBlinkTick(true);
            eyesClosed = true;
            lastBlink = millis();
        }
    }
#endif

    static unsigned long lastRedraw = 0;
#ifdef MANGO_UI
    if (clockMode) {
        // Clock mode: cập nhật mỗi giây để giây chạy mượt. full=false = vẽ đè, không nháy.
        if (millis() - lastRedraw > 1000) {
            uiClockScreen(lastFetch, WiFi.RSSI(), false);
            lastRedraw = millis();
        }
    } else
#endif
    if (millis() - lastRedraw > 10000) {
        // Only time passed (not data) — update the clock/countdowns in place; redrawing
        // the whole dashboard here is what made the slow CrowPanel panel flicker.
        uiDashboardClock(usage, lastFetch, WiFi.RSSI());
        lastRedraw = millis();
    }

    delay(20);
}
