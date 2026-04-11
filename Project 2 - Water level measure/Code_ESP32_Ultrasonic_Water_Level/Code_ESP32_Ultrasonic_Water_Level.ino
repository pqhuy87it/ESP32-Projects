/**********************************************************************************
 * TITLE: IoT-based Water Level Indicator (MINIMAL DEEP SLEEP VERSION)
 * Logic: Wake up -> Connect WiFi/Cloud -> Measure Water -> Upload -> Sleep
 * PINS: TRIG=11, ECHO=12
 * REMOVED: OLED, DHT Sensor
 **********************************************************************************/

#include <WiFi.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

// ------------------- CẤU HÌNH DEEP SLEEP -------------------
#define uS_TO_S_FACTOR 1000000ULL  /* Chuyển đổi micro giây sang giây */
#define TIME_TO_SLEEP  600        /* Thời gian ngủ (giây) = 1 giờ */
#define WIFI_TIMEOUT_MS 20000      /* Timeout WiFi 20s */

// ------------------- THÔNG TIN WIFI & CLOUD -------------------
const char SSID[]       = "MyHouse_2.4G"; 
const char PASS[]       = "Nh@cuatoi303"; 

const char DEVICE_ID[]  = "486c4f22-54c1-4436-a4f2-ba603253381d";
const char SECRET_KEY[] = "X7NSV2aG?slUzaDlRiGMcu@KI";

// ------------------- BIẾN CLOUD -------------------
int waterLevelPercentage;
String waterDistance;

// ------------------- CẤU HÌNH PHẦN CỨNG -------------------
// GPIO Pins (Giữ nguyên theo yêu cầu)
// LƯU Ý: Vẫn cảnh báo cũ, Pin 11, 12 trên ESP32 thường có thể gây lỗi Flash.
#define TRIGPIN    11  
#define ECHOPIN    12  

// Khoảng cách bể nước (cm)
const int emptyTankDistance = 100;
const int fullTankDistance  = 20;

// Biến nội bộ
float duration;
float distance;

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

// Khai báo hàm
void initProperties();
void measureWaterLevel();
void goToDeepSleep();

void setup() {
  Serial.begin(115200);
  delay(100); 

  // 1. Khởi tạo GPIO
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);

  // 2. Khởi tạo Cloud
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
  
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  // 3. Đo mức nước NGAY LẬP TỨC
  measureWaterLevel();
  Serial.print("Current Water Level: ");
  Serial.print(waterLevelPercentage);
  Serial.println("%");

  // 4. Chờ kết nối Cloud (Có Timeout)
  Serial.print("Connecting to Cloud");
  unsigned long startAttemptTime = millis();
  bool connected = false;

  while (!ArduinoCloud.connected()) {
    ArduinoCloud.update(); 
    if (ArduinoCloud.connected()) {
      connected = true;
      break;
    }
    Serial.print(".");
    delay(500);
  }

  // 5. Xử lý đồng bộ dữ liệu
  if (connected) {
    Serial.println("\nConnected! Syncing...");
    
    // Gọi update liên tục trong khoảng 2s để đảm bảo dữ liệu được đẩy đi
    for(int i=0; i<10; i++) {
       ArduinoCloud.update();
       delay(200); 
    }
    Serial.println("Data synced successfully.");
    
  } else {
    Serial.println("\nTimeout! Could not connect to WiFi/Cloud.");
  }

  // 6. Đi ngủ
  goToDeepSleep();
}

void loop() {
  // Không làm gì trong loop vì dùng Deep Sleep
}

// ------------------- HÀM PHỤ TRỢ -------------------

void initProperties(){
  ArduinoCloud.setBoardId(DEVICE_ID);
  ArduinoCloud.setSecretDeviceKey(SECRET_KEY);
  // Chỉ còn lại các biến liên quan đến nước
  ArduinoCloud.addProperty(waterLevelPercentage, READ, ON_CHANGE, NULL);
}

void goToDeepSleep(){
  Serial.println("Going to sleep for " + String(TIME_TO_SLEEP) + "s");
  Serial.flush(); // Đợi in hết Serial rồi mới ngắt
  
  // Setup timer và ngủ
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void measureWaterLevel() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  
  // Timeout 30ms (30000uS) để tránh treo nếu mất cảm biến
  duration = pulseIn(ECHOPIN, HIGH, 30000); 
  
  if (duration == 0) {
    distance = 0; // Không nhận được phản hồi
  } else {
    distance = (duration / 2) * 0.0343;
  }

  // Map giá trị ra phần trăm
  if (distance >= emptyTankDistance) {
    waterLevelPercentage = 0;
  } else if (distance <= fullTankDistance) {
    waterLevelPercentage = 100;
  } else {
    waterLevelPercentage = map((long)distance, emptyTankDistance, fullTankDistance, 0, 100);
  }
  
  waterLevelPercentage = constrain(waterLevelPercentage, 0, 100);
  waterDistance = String(distance, 1) + " cm";
}