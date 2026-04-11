/* WEMOS D1 MINI - WATER LEVEL ONLY - DEEP SLEEP OPTIMIZED
 * Đã bỏ: OLED, DHT11
 * Chức năng: Thức dậy -> Đo mức nước -> Gửi Cloud -> Ngủ 1 tiếng
 */

#include <ESP8266WiFi.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

// --- CẤU HÌNH ---
#define TIME_TO_SLEEP  60  // Thời gian ngủ (Giây) = 1 tiếng
#define WIFI_TIMEOUT   20000 // 20 giây timeout (nếu mất mạng thì đi ngủ luôn để bảo vệ pin)

// WiFi & Cloud Credentials
const char SSID[]     = "MyHouse_2.4G";
const char PASS[]     = "Nh@cuatoi303";
const char DEVICE_ID[] = "f7ae600c-3541-4e0f-b635-407d5ae46a1e";
const char SECRET_KEY[] = "Pi6?Aw1TQ8k2lGlStcQlCGMiW";

// Biến Cloud (Chỉ giữ lại biến mực nước)
int waterLevelPercentage;
String waterDistance;

// Cấu hình Chân (Wemos D1 Mini)
#define TRIG_PIN  D6  // GPIO 12
#define ECHO_PIN  D7  // GPIO 13

// Ngưỡng đo bể nước (cm)
const int emptyTankDistance = 100;
const int fullTankDistance = 20;

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

void initProperties(){
  ArduinoCloud.setBoardId(DEVICE_ID);
  ArduinoCloud.setSecretDeviceKey(SECRET_KEY);
  // Chỉ đồng bộ dữ liệu mức nước
  ArduinoCloud.addProperty(waterLevelPercentage, READ, ON_CHANGE, NULL);
}

void doThisOnConnect(){
  /* add your custom code here */
  Serial.println("Board successfully connected to Arduino IoT Cloud");
}

void doThisOnSync(){
  /* add your custom code here */
  Serial.println("Thing Properties synchronised");
}

void doThisOnDisconnect(){
  /* add your custom code here */
  Serial.println("Board disconnected from Arduino IoT Cloud");
}

void connectToArduinoCloud() {
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::CONNECT, doThisOnConnect);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::SYNC, doThisOnSync);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::DISCONNECT, doThisOnDisconnect);
}

void setup() {
  // Serial tốc độ 74880 giúp xem được cả thông tin boot log của ESP8266
  Serial.begin(74880); 

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  delay(1000);

  initProperties();

  connectToArduinoCloud();
  
  // --- QUY TRÌNH KẾT NỐI (CÓ TIMEOUT) ---
  Serial.println("Waking up... Connecting to Cloud...");
  
  // Chờ kết nối tối đa 20s
  while (!ArduinoCloud.connected()) {
    ArduinoCloud.update();
    delay(500);
  }

  // Nếu kết nối thành công thì đo và gửi
  if (ArduinoCloud.connected()) {
    Serial.println("Connected!");
    measureAndSync();
  }

  // --- ĐI NGỦ ---
  Serial.println("Going to deep sleep...");
  // ESP8266 yêu cầu nối D0 vào RST để lệnh này hoạt động
  ESP.deepSleep(TIME_TO_SLEEP * 1000000ULL); 
}

void loop() {
  // Deep sleep mode không dùng loop
}

void measureAndSync() {
  // 1. Đo siêu âm
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  float duration = pulseIn(ECHO_PIN, HIGH);
  // Tính khoảng cách (cm)
  float distance = (duration / 2) * 0.0343;

  // 2. Xử lý logic phần trăm
  if (distance < fullTankDistance) {
    waterLevelPercentage = 100;
  } else if (distance > emptyTankDistance) {
    waterLevelPercentage = 0;
  } else {
    waterLevelPercentage = map((int)distance, emptyTankDistance, fullTankDistance, 0, 100);
  }
  
  waterDistance = String(distance, 1) + " cm";
  Serial.print("Level: "); Serial.print(waterLevelPercentage); Serial.println("%");
  delay(1000);

  // 3. Gửi lên Cloud
  // Gọi update vài lần + delay để đảm bảo dữ liệu đi hết qua Wifi
  ArduinoCloud.update();
  delay(1000);
  ArduinoCloud.update();
  delay(1000);
}