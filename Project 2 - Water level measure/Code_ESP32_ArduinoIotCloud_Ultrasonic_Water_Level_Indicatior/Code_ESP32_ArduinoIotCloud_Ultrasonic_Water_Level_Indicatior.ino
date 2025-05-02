/**********************************************************************************
 *  TITLE: IoT-based Water Level Indicator using ESP32, Ultrasonic Sensor & Arduino IoT Cloud with 0.96" OLED
 *  Click on the following links to learn more. 
 *  YouTube Video: https://youtu.be/dqB6Vfq2Xcw
 *  Related Blog : https://iotcircuithub.com/esp32-projects/
 *  
 *  This code is provided free for project purpose and fair use only.
 *  Please do mail us to techstudycell@gmail.com if you want to use it commercially.
 *  Copyrighted © by Tech StudyCell
 *  
 *  Preferences--> Aditional boards Manager URLs : 
 *  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  
 *  Download Board ESP32 (3.0.5) : https://github.com/espressif/arduino-esp32
 *
 *  Download the libraries 
 *  ArduinoIoTCloud Library (Version 2.2.0) with all the dependencies: https://github.com/arduino-libraries/ArduinoIoTCloud
 *  Adafruit_SSD1306 Library (2.5.13): https://github.com/adafruit/Adafruit_SSD1306

 **********************************************************************************/

#include <WiFi.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

// WiFi credentials
const char SSID[]     = "MyHouse_2.4G";        // Replace with your WiFi SSID
const char PASS[]     = "Nh@cuatoi303";    // Replace with your WiFi password

// Arduino IoT Cloud Device credentials
const char DEVICE_ID[] = "486c4f22-54c1-4436-a4f2-ba603253381d";  // Replace with your device ID
const char SECRET_KEY[] = "X7NSV2aG?slUzaDlRiGMcu@KI";// Replace with your secret key

// Variables to be synced with the cloud
int waterLevelPercentage;
int dht11_humidity;
int dht11_temperature;
int cnt = 0;
String waterDistance;

//----------------------------------------Variable for millis/timer.
unsigned long prevMill = 0;
const long intervalMill = 1000;
//----------------------------------------

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

// Defines the time interval for updating weather data.
#define INTERVAL_UPDATING_WATER_LEVEL 30 //--> 60 seconds (1 minutes). Water level is updated every 1 minutes.

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cloud connection handler
WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

// GPIO Pins
#define TRIGPIN    11
#define ECHOPIN    12

#define DHTPIN     10      // Set the pin connected to the DHT11 data pin
#define DHTTYPE    DHT11  // DHT 11

// Tank distance thresholds (in cm)
const int emptyTankDistance = 100;
const int fullTankDistance = 20;

float duration;
float distance;

// Variables for button debounce
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // 50ms debounce delay

DHT dht(DHTPIN, DHTTYPE);

void initProperties(){

  // Arduino IoT Cloud setup
  ArduinoCloud.setBoardId(DEVICE_ID);
  ArduinoCloud.setSecretDeviceKey(SECRET_KEY);
  ArduinoCloud.addProperty(waterLevelPercentage, READ, 2 * SECONDS, onWaterLevelPercentageChange);
  ArduinoCloud.addProperty(waterDistance, READ, 2 * SECONDS, NULL);
  ArduinoCloud.addProperty(dht11_humidity, READ, 2 * SECONDS, NULL);
  ArduinoCloud.addProperty(dht11_temperature, READ, 2 * SECONDS, NULL);
}

void connectToArduinoCloud() {
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::CONNECT, doThisOnConnect);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::SYNC, doThisOnSync);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::DISCONNECT, doThisOnDisconnect);
}

void doThisOnConnect(){
  /* add your custom code here */
  Serial.println("Board successfully connected to Arduino IoT Cloud");
  // digitalWrite(wifiLed, HIGH); //Turn off WiFi LED

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Connected to Ardunio IoT Cloud.");
  display.display();
}
void doThisOnSync(){
  /* add your custom code here */
  Serial.println("Thing Properties synchronised");
}

void doThisOnDisconnect(){
  /* add your custom code here */
  Serial.println("Board disconnected from Arduino IoT Cloud");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Disconnected to Ardunio IoT Cloud.");
  display.display();
}

void onWaterLevelPercentageChange() {
  Serial.print("Water Level Changed to: ");
  Serial.println(waterLevelPercentage);
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);

  // Pin configurations
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);

  // OLED Display initialization
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setCursor(10, 5);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Setting...");
  display.display();

  delay(1000);  

  initProperties();

  connectToArduinoCloud();
}

void loop() {
  //----------------------------------------Timer/Millis to update time and date.
  // This Timer/Millis also functions to update weather data.
  unsigned long currentMill = millis();

  if (currentMill - prevMill >= intervalMill) {
    prevMill = currentMill;

    cnt++;
    if (cnt > INTERVAL_UPDATING_WATER_LEVEL) {
      cnt = 0;
      
      // Call measure water level.
      measureWaterLevel();

      // Call measure temperature and humidity
      measureHumidityAndTemperature();

      // Update OLED display
      updateOledDisplay();

      // Update cloud variables
      ArduinoCloud.update();
    }
  }
}

void measureWaterLevel() {
  // Trigger the ultrasonic sensor
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);

  // Measure the duration of the echo pulse
  duration = pulseIn(ECHOPIN, HIGH);
  distance = ((duration / 2) * 0.343) / 10;

  if (distance < fullTankDistance) {
    waterLevelPercentage = 100;
  } else if (distance > emptyTankDistance) {
    waterLevelPercentage = 0;
  } else {
    // Calculate water level percentage
    waterLevelPercentage = map((int)distance, emptyTankDistance, fullTankDistance, 0, 100);
    waterDistance = String(distance) + " cm";
  }
}

void updateOledDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("Water level: ");
  display.print(waterLevelPercentage);
  display.println(" %");
  display.print("Temperature: ");
  display.print(dht11_temperature);
  display.println(" C");
  display.print("Humidity: ");
  display.print(dht11_humidity);
  display.println(" %");
  display.display();
}

void measureHumidityAndTemperature() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  dht11_humidity = dht.readHumidity();
  dht11_temperature = dht.readTemperature();
}