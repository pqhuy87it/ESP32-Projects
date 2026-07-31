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
#include <time.h>
#include "esp_mac.h"      // esp_efuse_mac_get_default (core 3.x)
#include "config.h"
#include "crypto.h"
#include "provision.h"
#include "api.h"
#include "ui.h"
#ifdef MANGO_UI
#include "status.h"
#include "weather.h"
#include "moon.h"
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
static WeatherData weather = {};   // zero-init toàn bộ field (ok=false)
static MoonData    moon = {};
static bool        weatherShowMoon = false;   // false=forecast, true=moon (nút A lật khi ở Weather)
static bool        clockShowCalendar = false; // false=đồng hồ, true=lịch (nút A lật khi ở Clock)
static unsigned long lastWeatherFetch = 0;
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

#ifdef MANGO_UI
// Ba chế độ màn hình, xoay vòng bằng nút B: Dashboard → Clock → Weather → …
enum ViewMode { VIEW_DASH = 0, VIEW_CLOCK, VIEW_WEATHER, VIEW_COUNT };
static ViewMode viewMode = VIEW_DASH;

// Vẽ lại màn hình ứng với chế độ hiện tại.
static void drawCurrentView() {
    switch (viewMode) {
        case VIEW_CLOCK:
            if (clockShowCalendar) uiCalendarScreen(WiFi.RSSI());
            else                   uiClockScreen(lastFetch, WiFi.RSSI());
            break;
        case VIEW_WEATHER:
            if (weatherShowMoon) {
                computeMoon((long)time(nullptr), OWM_TZ_OFFSET_SEC, moon);
                uiMoonScreen(moon, WiFi.RSSI());
            } else {
                uiWeatherScreen(weather, WiFi.RSSI());
            }
            break;
        default:           uiDashboard(usage, lastFetch, WiFi.RSSI(), halBatPercent()); break;
    }
}
#endif


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
    // A flips the screen 180°, B xoay vòng chế độ (Dashboard→Clock→Weather),
    // A+B together = force refresh. Một lần nhấn chỉ chốt sau một cửa sổ ngắn
    // để nút kia còn kịp ghép thành combo.
    static unsigned long aPressAt = 0, bPressAt = 0;
    const unsigned long comboWindowMs = 350;
    if (halBtnAWasPressed()) aPressAt = millis();
    if (halBtnBWasPressed()) bPressAt = millis();

    if ((aPressAt && (bPressAt || halBtnBIsPressed())) ||
        (bPressAt && halBtnAIsPressed())) {
        aPressAt = bPressAt = 0;
        refresh();
        drawCurrentView();
    } else if (aPressAt && millis() - aPressAt > comboWindowMs) {
        aPressAt = 0;
        if (viewMode == VIEW_WEATHER) {
            weatherShowMoon = !weatherShowMoon;   // lật Forecast <-> Moon
        } else if (viewMode == VIEW_CLOCK) {
            clockShowCalendar = !clockShowCalendar;  // lật Đồng hồ <-> Lịch
        } else {
            uiToggleRotation();                    // Dashboard: xoay màn hình
        }
        drawCurrentView();
    } else if (bPressAt && millis() - bPressAt > comboWindowMs) {
        bPressAt = 0;
        viewMode = (ViewMode)((viewMode + 1) % VIEW_COUNT);   // xoay vòng chế độ
        weatherShowMoon = false;                 // vào Weather luôn bắt đầu ở Forecast
        clockShowCalendar = false;               // vào Clock luôn bắt đầu ở Đồng hồ
        // Vào weather mode mà chưa có dữ liệu → fetch ngay để có gì đó hiển thị.
        if (viewMode == VIEW_WEATHER && !weather.ok) {
            fetchWeather(weather);
            lastWeatherFetch = millis();
        }
        drawCurrentView();
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

    // Fetch usage định kỳ — chỉ khi đang ở Dashboard. Ở Clock/Weather thì hoãn
    // (fetchUsage/fetchModelStatus là HTTPS blocking, làm đứng UI vài giây).
    // lastFetch không đổi khi hoãn, nên vừa quay lại Dashboard là fetch ngay.
    bool atDashboard = true;
#ifdef MANGO_UI
    atDashboard = (viewMode == VIEW_DASH);
#endif
    if (atDashboard && millis() - lastFetch >= (unsigned long)pollMs) {
        refresh();
    }

#ifdef MANGO_UI
    // Fetch thời tiết định kỳ khi đang ở Weather mode (mặc định 15'/lần).
    if (viewMode == VIEW_WEATHER &&
        millis() - lastWeatherFetch >= (unsigned long)WEATHER_REFRESH_SEC * 1000) {
        fetchWeather(weather);
        lastWeatherFetch = millis();
        if (!weatherShowMoon) uiWeatherScreen(weather, WiFi.RSSI());  // không đè khi đang xem moon
    }

    // NTP: chỉ sync 1 lần lúc boot, sau đó sync lại khi sang ngày mới (qua 0h).
    // Chỉ khi ở Dashboard để không block Clock/Weather.
    if (viewMode == VIEW_DASH && WiFi.status() == WL_CONNECTED) {
        struct tm nt;
        if (getLocalTime(&nt, 0) && nt.tm_yday != lastSyncYday) {
            syncTime();
        }
    }
#endif

#ifdef MANGO_UI
    // Healthy mascots blink every 2s (eyes shut for 150ms) — chỉ ở Dashboard.
    static unsigned long lastBlink = 0;
    static bool eyesClosed = false;
    if (viewMode == VIEW_DASH) {
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
    if (viewMode == VIEW_CLOCK) {
        // Chỉ tick khi đang xem đồng hồ; xem lịch thì không cần vẽ lại mỗi giây.
        if (!clockShowCalendar && millis() - lastRedraw > 1000) {
            uiClockScreen(lastFetch, WiFi.RSSI(), false);
            lastRedraw = millis();
        }
    } else if (viewMode == VIEW_WEATHER) {
        // Weather: không cần vẽ lại thường xuyên; fetch định kỳ ở trên đã lo cập nhật.
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
