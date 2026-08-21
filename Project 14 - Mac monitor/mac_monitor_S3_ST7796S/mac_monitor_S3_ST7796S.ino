// ==================================================================
// Mac System Monitor - ESP32-S3 + ST7796S (480x320) + TFT_eSPI
// Nhận dữ liệu qua BLE, vẽ 4 biểu đồ (CPU / RAM / PWR / GPU)
//
// Format chuỗi BLE mong đợi:  C:12.3,G:45.6,R:78.9,W:23.4
//
// Cấu hình màn hình: xem Setup_ST7796S_ESP32S3.h
// ==================================================================

#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// ==================================================================
// LAYOUT (rotation = 1 -> 480 x 320)
// ==================================================================
#define SCR_W       480
#define SCR_H       320

#define BAR_H        20                 // thanh trạng thái trên cùng
#define CELL_W      (SCR_W / 2)         // 240
#define CELL_H     ((SCR_H - BAR_H) / 2) // 150

// Biểu đồ trong mỗi ô
#define GRAPH_POINTS 74                 // 74 điểm * 3px = 222px
#define BAR_STEP      3
#define BAR_WIDTH     2

// Màu phụ
#define COL_BORDER  0x2965              // xám đậm
#define COL_GRID    0x18E3              // xám rất đậm
#define COL_LABEL   0x8410              // xám nhạt

// ==================================================================
// ĐỐI TƯỢNG MÀN HÌNH
// ==================================================================
TFT_eSPI  tft    = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);  // 240x150, dùng lại cho cả 4 ô
TFT_eSprite topbar = TFT_eSprite(&tft);  // 480x20

// ==================================================================
// DỮ LIỆU
// ==================================================================
float cpuHist[GRAPH_POINTS]  = {0};
float ramHist[GRAPH_POINTS]  = {0};
float gpuHist[GRAPH_POINTS]  = {0};
float wattHist[GRAPH_POINTS] = {0};

String cpuVal  = "--";
String ramVal  = "--";
String gpuVal  = "--";
String wattVal = "--";

volatile bool     needRedraw   = false;   // BLE task set -> loop() vẽ
volatile bool     bleConnected = false;
volatile uint32_t lastRxMs     = 0;

// Mutex bảo vệ mảng history giữa BLE task và main loop
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

void updateDisplay();
void drawTopBar();

// ==================================================================
// BLE
// ==================================================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static void pushSample(float* hist, float v) {
  for (int i = 0; i < GRAPH_POINTS - 1; i++) hist[i] = hist[i + 1];
  hist[GRAPH_POINTS - 1] = v;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override { bleConnected = true; }
  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    BLEDevice::startAdvertising();   // cho phép Mac kết nối lại
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String rx = pCharacteristic->getValue().c_str();
    if (rx.length() == 0) return;

    int cIndex = rx.indexOf("C:");
    int gIndex = rx.indexOf(",G:");
    int rIndex = rx.indexOf(",R:");
    int wIndex = rx.indexOf(",W:");
    if (cIndex < 0 || gIndex < 0 || rIndex < 0 || wIndex < 0) return;

    String c = rx.substring(cIndex + 2, gIndex);
    String g = rx.substring(gIndex + 3, rIndex);
    String r = rx.substring(rIndex + 3, wIndex);
    String w = rx.substring(wIndex + 3);

    // Chỉ cập nhật dữ liệu ở đây, KHÔNG vẽ SPI trong BLE task
    // (stack của BLE task nhỏ, vẽ ở đây dễ tràn / xung đột bus)
    taskENTER_CRITICAL(&dataMux);
    cpuVal = c;  gpuVal = g;  ramVal = r;  wattVal = w;
    pushSample(cpuHist,  c.toFloat());
    pushSample(gpuHist,  g.toFloat());
    pushSample(ramHist,  r.toFloat());
    pushSample(wattHist, w.toFloat());
    taskEXIT_CRITICAL(&dataMux);

    lastRxMs   = millis();
    needRedraw = true;
  }
};

// ==================================================================
// VẼ 1 Ô BIỂU ĐỒ
// ==================================================================
void drawQuadrant(int x, int y, const char* title, const String& valStr,
                  const char* unit, float* hist, uint16_t color, float maxScale) {
  canvas.fillSprite(TFT_BLACK);
  canvas.drawRect(0, 0, CELL_W, CELL_H, COL_BORDER);

  // --- Tên thông số ---
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(COL_LABEL, TFT_BLACK);
  canvas.drawString(title, 10, 9, 2);

  // --- Giá trị hiện tại (canh phải) ---
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(color, TFT_BLACK);
  int uw = canvas.textWidth(unit, 2);
  canvas.drawString(unit,   CELL_W - 10,          16, 2);
  canvas.drawString(valStr, CELL_W - 12 - uw,      4, 4);

  // --- Khu vực biểu đồ ---
  const int gX = 10;
  const int gY = 46;
  const int gW = GRAPH_POINTS * BAR_STEP;   // 222
  const int gH = 94;

  // Lưới ngang 25 / 50 / 75%
  for (int p = 25; p < 100; p += 25) {
    int ly = gY + gH - (gH * p) / 100;
    for (int lx = gX; lx < gX + gW; lx += 6) canvas.drawPixel(lx, ly, COL_GRID);
  }
  // Đường đáy
  canvas.drawFastHLine(gX, gY + gH, gW, COL_BORDER);

  // Nhãn thang đo (góc trên phải khu biểu đồ)
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(COL_GRID, TFT_BLACK);
  canvas.drawString(String((int)maxScale) + unit, gX + gW, gY - 12, 1);

  // --- Các cột dữ liệu ---
  float peak = 0;
  for (int i = 0; i < GRAPH_POINTS; i++) {
    float v = hist[i];
    if (v > peak) peak = v;
    if (v <= 0) continue;

    int h = (int)((v / maxScale) * gH);
    if (h > gH) h = gH;
    if (h < 1)  h = 1;

    canvas.fillRect(gX + i * BAR_STEP, gY + gH - h, BAR_WIDTH, h, color);
  }

  // --- Đường peak ---
  if (peak > 0) {
    int ph = (int)((peak / maxScale) * gH);
    if (ph > gH) ph = gH;
    int py = gY + gH - ph;
    for (int lx = gX; lx < gX + gW; lx += 4) canvas.drawPixel(lx, py, color);

    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(COL_LABEL, TFT_BLACK);
    canvas.drawString("peak " + String(peak, 1), gX, gY - 12, 1);
  }

  canvas.pushSprite(x, y);
}

// ==================================================================
// CẬP NHẬT TOÀN BỘ
// ==================================================================
void updateDisplay() {
  // Snapshot dữ liệu để không giữ critical section suốt lúc vẽ SPI
  static float c[GRAPH_POINTS], r[GRAPH_POINTS], g[GRAPH_POINTS], w[GRAPH_POINTS];
  String cS, rS, gS, wS;

  taskENTER_CRITICAL(&dataMux);
  memcpy(c, cpuHist,  sizeof(c));
  memcpy(r, ramHist,  sizeof(r));
  memcpy(g, gpuHist,  sizeof(g));
  memcpy(w, wattHist, sizeof(w));
  cS = cpuVal; rS = ramVal; gS = gpuVal; wS = wattVal;
  taskEXIT_CRITICAL(&dataMux);

  const int y0 = BAR_H;
  const int y1 = BAR_H + CELL_H;

  drawQuadrant(0,      y0, "CPU", cS, "%", c, TFT_GREEN,   100.0f);
  drawQuadrant(CELL_W, y0, "RAM", rS, "%", r, TFT_YELLOW,  100.0f);
  drawQuadrant(0,      y1, "PWR", wS, "W", w, TFT_ORANGE,   60.0f);
  drawQuadrant(CELL_W, y1, "GPU", gS, "%", g, TFT_MAGENTA, 100.0f);
}

// ==================================================================
// THANH TRẠNG THÁI
// ==================================================================
void drawTopBar() {
  topbar.fillSprite(TFT_BLACK);
  topbar.drawFastHLine(0, BAR_H - 1, SCR_W, COL_BORDER);

  topbar.setTextDatum(TL_DATUM);
  topbar.setTextColor(TFT_CYAN, TFT_BLACK);
  topbar.drawString("MAC MONITOR", 10, 3, 2);

  bool stale = (millis() - lastRxMs > 5000);
  uint16_t dot;
  const char* txt;
  if (!bleConnected)      { dot = TFT_RED;    txt = "BLE: disconnected"; }
  else if (stale)         { dot = TFT_ORANGE; txt = "BLE: no data"; }
  else                    { dot = TFT_GREEN;  txt = "BLE: streaming"; }

  topbar.setTextDatum(TR_DATUM);
  topbar.setTextColor(COL_LABEL, TFT_BLACK);
  topbar.drawString(txt, SCR_W - 22, 3, 2);
  topbar.fillCircle(SCR_W - 11, BAR_H / 2 - 1, 4, dot);

  topbar.pushSprite(0, 0);
}

// ==================================================================
// SETUP / LOOP
// ==================================================================
void setup() {
  Serial.begin(115200);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

  tft.init();
  tft.setRotation(1);            // 480 x 320 ngang
  tft.fillScreen(TFT_BLACK);
  // tft.invertDisplay(true);    // bỏ comment nếu màu bị âm bản

  // 240*150*2 = 72KB  +  480*20*2 = 19KB  -> vừa RAM nội ESP32-S3
  canvas.setColorDepth(16);
  canvas.createSprite(CELL_W, CELL_H);
  topbar.setColorDepth(16);
  topbar.createSprite(SCR_W, BAR_H);

  if (!canvas.created() || !topbar.created()) {
    Serial.println("Sprite alloc failed!");
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Waiting for Mac...", SCR_W / 2, SCR_H / 2, 4);

  // ---------------- BLE ----------------
  BLEDevice::init("MacMonitor");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic* pChar = pService->createCharacteristic(
      CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  pChar->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  pAdv->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  drawTopBar();
}

void loop() {
  static bool     firstFrame = true;
  static uint32_t lastBar    = 0;

  if (needRedraw) {
    needRedraw = false;
    if (firstFrame) { tft.fillScreen(TFT_BLACK); firstFrame = false; }
    updateDisplay();
  }

  if (millis() - lastBar > 500) {
    lastBar = millis();
    drawTopBar();
  }

  delay(10);
}
