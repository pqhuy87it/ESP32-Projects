#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSerif12pt7b.h>

#define TFT_DC 4
#define TFT_CS 15
#define TFT_RST 2
#define STATE_TIMEOUT 5000
#define WEATHER_TIMEOUT 10000

#define TEXT 0xffff

// YOUR WIFI CREDENTIALS **IMPORTANT**
const char* ssid = "MyHome1stFloor_2.4G";
const char* password = "Nh@cuatoi726";

// Latitude and Longitude of your desired location
double lat = 21.01695; 
double lon = 105.80407;

// YOUR API KEY **IMPORTANT**
String API_KEY = "28c29099075601342f371617f43b2878";

String URL = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat) + "&lon=" + String(lon) + "&units=metric" + "&appid=" + API_KEY;

// Display 
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Local Time Variables
String currentTime;
String meridiem;

// Weather Variables
String location = "London";
String description = "Cloudy";
int high = 0;
int low = 0;
int temp = 0;

String jsonBuffer;

// NTPClient initialization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "in.pool.ntp.org");

void fetchDateTime();
void progressBar(int x, int y, int w, int h, int value);

enum STATES
{
  WEATHER = 0,
  DATE = 1,
  TIME = 2
};

STATES currentState = STATES::DATE;

unsigned long previousMillis = 0;
unsigned long weatherMillis = 0;

//Week Days
String weekDays[7]={"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

//Month names
String months[12]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void setup() {
  Serial.begin(9600);

  tft.init(240, 240);

  // Set display rotation
  tft.setRotation(2);
  
  // Clear display
  tft.fillScreen(0);

  tft.setFont(&FreeSerif12pt7b);

  tft.setTextSize(1);

  tft.setCursor(100, 100);

  tft.println("Macintosh");

  int progX = 0;

  // Connecting to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    progressBar(20, 120, 200, 20, progX);
    progX += 5;
    Serial.println(progX);
    delay(500);
  }
  
  timeClient.begin();
  timeClient.setTimeOffset(25200);

  if(WiFi.status() == WL_CONNECTED)
  {
      fetchWeather();
  }

  tft.fillScreen(0);
  tft.setFont();
  tft.setTextSize(2);
}

void loop() {
  timeClient.update();
  stateManager();
  displayManager();
  unsigned long currentWeatherMillis = millis();
  if(currentWeatherMillis - weatherMillis >= WEATHER_TIMEOUT)
  {
    weatherMillis = currentWeatherMillis;
    fetchWeather();
  }
}

void progressBar(int x, int y, int w, int h, int value)
{
  if(value >= 100) value = 100;
  int progress = map(value, 0, 100, x + 5, w - 10);
  tft.drawRect(x, y, w, h, TEXT);
  tft.fillRect(x + 5, y + 5, progress, h - 10, TEXT);
}

void stateManager()
{
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= STATE_TIMEOUT)
  {
    previousMillis = currentMillis;
    if(currentState == STATES::DATE)
    {
      fetchDateTime();
      currentState = STATES::TIME;
    }
    else if(currentState == STATES::TIME)
    {
      currentState = STATES::WEATHER;
    }
    else
    {
      currentState = STATES::DATE;
    }
    tft.fillScreen(0);
  }
}

void displayManager()
{
  if(currentState == STATES::TIME)
  {
    time();
  }
  else if(currentState == STATES::WEATHER)
  {
    weather();
  }
  else
  {
    date();
  }
}

void fetchWeather()
{
  if(WiFi.status() == WL_CONNECTED)
  {
    String response = httpGETRequest(URL.c_str());

    if(response != "")
    {
      JSONVar myObject = JSON.parse(response);

      if(JSON.typeof_(myObject) == "undefined")
      {
        Serial.println("parsing input faileed");
        return;
      }

      temp = (int)myObject["main"]["temp"];
      high = (int)myObject["main"]["temp_max"];
      low = (int)myObject["main"]["temp_min"];
      location = String(myObject["name"]);
      description = String(myObject["weather"][0]["main"]);
    }
  }
}

// Fetching and Formatting Time
void fetchDateTime() {
  String hours = String(timeClient.getHours());
  String minutes = String(timeClient.getMinutes());
  if (hours.toInt() >= 12) {
    meridiem = "PM";
  } else {
    meridiem = "AM";
  }
  if (hours.toInt() > 12) {
    hours = String(map(hours.toInt(), 13, 24, 1, 12));
  }
  if (hours.toInt() < 10) {
    hours = "0" + hours;
  }
  if (minutes.toInt() < 10) {
    minutes = "0" + minutes;
  }
  currentTime = hours + ":" + minutes;
  Serial.println(currentTime);
}

void time()
{
  tft.drawRect(10, 10 , 220, 220, TEXT);
  tft.setFont();
  tft.setTextSize(6);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(20, 100);
  tft.print(currentTime);
  tft.setTextSize(2);
  tft.setCursor(195, 100);
  tft.print(meridiem);
}

String padZero(int number)
{
  if(number < 10)
  {
    return "0" + String(number);
  }
  return String(number);
}

void weather()
{

  tft.drawRect(10, 10 , 220, 220, TEXT);

  tft.setFont();
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setCursor(85, 60);
  tft.println(location);

  tft.setTextSize(8);
  tft.setCursor(80, 88);
  tft.print(padZero(temp));


  //tft.setTextSize(2);
  //tft.setCursor(170, 80);
  //tft.println("0");

  tft.setTextSize(2);
  tft.setCursor(90, 152);
  tft.println(description);

  String HL = "H: " + padZero(high) + " L: " + padZero(low);
  tft.setTextSize(2);
  tft.setCursor(60, 180);
  tft.print(HL);
}

void date()
{
  tft.setFont();
  time_t epochTime = timeClient.getEpochTime();

  struct tm *ptm = gmtime ((time_t *)&epochTime);
  
  String currentMonthName = months[ptm->tm_mon];
  String currentDay = weekDays[timeClient.getDay()];

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.drawRect(10, 10 , 220, 220, TEXT);

  tft.setTextSize(10);
  tft.setCursor(30, 40);
  tft.println(currentDay);

  tft.setTextSize(4);
  tft.setCursor(20, 130);
  tft.print(ptm->tm_mday);
  tft.print(" ");
  tft.println(currentMonthName);

  tft.setTextSize(4);
  tft.setCursor(130, 180);
  tft.println(ptm->tm_year+1900);
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    return "";
  }
  // Free resources
  http.end();

  return payload;
}