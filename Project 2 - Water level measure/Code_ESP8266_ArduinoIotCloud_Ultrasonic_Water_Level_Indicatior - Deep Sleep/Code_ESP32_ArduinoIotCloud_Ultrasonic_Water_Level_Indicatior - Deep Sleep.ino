/**********************************************************************************
 * TITLE: IoT-based Water Level Indicator (DEEP SLEEP VERSION)
 * Logic: Wake up -> Connect WiFi/Cloud -> Measure -> Upload -> Sleep 1 hour
 * PINS: TRIG=11, ECHO=12, DHT=10
 **********************************************************************************/

#include <WiFi.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

// ------------------- CẤU HÌNH DEEP SLEEP -------------------
#define uS_TO_S_FACTOR 1000000ULL  /* Chuyển đổi micro giây sang giây */
#define TIME_TO_SLEEP  900        /* Thời gian ngủ (giây) = 1 giờ */
#define WIFI_TIMEOUT_MS 2000000      /* Timeout WiFi 20s */

// ------------------- THÔNG TIN WIFI & CLOUD -------------------
const char SSID[]       = "MyHouse_2.4G"; 
const char PASS[]       = "Nh@cuatoi303"; 

const char DEVICE_ID[]  = "486c4f22-54c1-4436-a4f2-ba603253381d";
const char SECRET_KEY[] = "X7NSV2aG?slUzaDlRiGMcu@KI";

// ------------------- BIẾN CLOUD -------------------
int waterLevelPercentage;
int dht11_humidity;
int dht11_temperature;
String waterDistance;

// ------------------- CẤU HÌNH PHẦN CỨNG -------------------
// Màn hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// GPIO Pins (THEO YÊU CẦU CỦA BẠN)
// LƯU Ý: Nếu dùng ESP32 WROOM chuẩn, các chân 6-11 dính tới Flash có thể gây crash.
// Nếu bạn dùng ESP32-S2/S3 hoặc board thiết kế riêng thì OK.
#define TRIGPIN    11  
#define ECHOPIN    12  
#define DHTPIN     10  
#define DHTTYPE    DHT11

// Khoảng cách bể nước (cm)
const int emptyTankDistance = 100;
const int fullTankDistance  = 20;

// Biến nội bộ
float duration;
float distance;
DHT dht(DHTPIN, DHTTYPE);

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

// Khai báo hàm
void initProperties();
void measureWaterLevel();
void measureHumidityAndTemperature();
void updateOledDisplay();
void goToDeepSleep();

void setup() {
  Serial.begin(115200);
  delay(100); 

  // 1. Khởi tạo GPIO & Cảm biến
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  dht.begin(); 

  // 2. Khởi động OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 failed"));
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Waking up...");
  display.display();

  // 3. Khởi tạo Cloud
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
  
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  // 4. Đo đạc dữ liệu NGAY LẬP TỨC
  measureWaterLevel();
  measureHumidityAndTemperature();
  updateOledDisplay(); 

  // 5. Chờ kết nối Cloud (Có Timeout)
  Serial.print("Connecting to Cloud");
  unsigned long startAttemptTime = millis();
  bool connected = false;

  while (millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    ArduinoCloud.update(); 
    if (ArduinoCloud.connected()) {
      connected = true;
      break;
    }
    Serial.print(".");
    delay(500);
  }

  // 6. Xử lý đồng bộ dữ liệu
  if (connected) {
    Serial.println("\nConnected! Syncing...");
    
    // Gọi update liên tục trong khoảng 2s để đẩy dữ liệu đi
    for(int i=0; i<10; i++) {
       ArduinoCloud.update();
       delay(200); 
    }
    
    Serial.println("Data synced.");
    display.setCursor(0, 55);
    display.print("Synced: OK");
    display.display();
    delay(1000); 
    
  } else {
    Serial.println("\nTimeout! No WiFi.");
    display.setCursor(0, 55);
    display.print("Err: No WiFi");
    display.display();
    delay(2000);
  }

  // 7. Đi ngủ
  goToDeepSleep();
}

void loop() {
  // Không làm gì trong loop
}

// ------------------- HÀM PHỤ TRỢ -------------------

void initProperties(){
  ArduinoCloud.setBoardId(DEVICE_ID);
  ArduinoCloud.setSecretDeviceKey(SECRET_KEY);
  ArduinoCloud.addProperty(waterLevelPercentage, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(waterDistance, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(dht11_humidity, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(dht11_temperature, READ, ON_CHANGE, NULL);
}

void goToDeepSleep(){
  Serial.println("Sleep " + String(TIME_TO_SLEEP) + "s");
  
  // Tắt màn hình
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF); 
  
  // Setup timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.flush(); 
  esp_deep_sleep_start();
}

void measureWaterLevel() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  
  duration = pulseIn(ECHOPIN, HIGH, 30000); // Timeout 30ms
  
  if (duration == 0) distance = 0;
  else distance = (duration / 2) * 0.0343;

  if (distance >= emptyTankDistance) waterLevelPercentage = 0;
  else if (distance <= fullTankDistance) waterLevelPercentage = 100;
  else waterLevelPercentage = map((long)distance, emptyTankDistance, fullTankDistance, 0, 100);
  
  waterLevelPercentage = constrain(waterLevelPercentage, 0, 100);
  waterDistance = String(distance, 1) + " cm";
}

void measureHumidityAndTemperature() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    dht11_humidity = 0;
    dht11_temperature = 0;
  } else {
    dht11_humidity = (int)h;
    dht11_temperature = (int)t;
  }
}

void updateOledDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Lvl: "); display.print(waterLevelPercentage); display.println("%");
  display.print("Dst: "); display.println(waterDistance);
  display.println("-------------");
  display.print("Temp: "); display.print(dht11_temperature); display.println("C");
  display.print("Hum : "); display.print(dht11_humidity); display.println("%");
  display.display();
}