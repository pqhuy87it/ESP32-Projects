// Copyright (c) 2024 Bastian Brumbi
// Google Calendar epaper

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
#include <HTTPSRedirect.h>  // https://github.com/electronicsguy/ESP8266/tree/master/HTTPSRedirect
#include <Adafruit_GFX.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// E-Paper Display Pins
#define CS_PIN D4
#define DC_PIN D2
#define RST_PIN D1
#define BUSY_PIN D3

const int MAX_ENTRIES = 4;
struct Time { //Zeit
  String day;
  int date;
  int hour;
  int min;
};
struct Entry { //Kalenderevents
  String title;
  Time start;
  String startDate;
  Time end;
  String endDate;
};
Entry entries[MAX_ENTRIES]; //Liste der Events


int offset = 0; //verschiebung bei Zeilenumbruch

char const * const dstHost = "script.google.com";
char const * const dstPath = "/macros/s/AKfycbyeHcivREL2A0Ksj_KFHN5XMF007YWSeG7a6QZqQv6t9zU23IpP3M1m-FFwpj2SUulN9w/exec"; // script path including key
int const dstPort = 443;

//Hier Wlan Zugangsdaten eintragen
String SSID = "MyHouse_2.4G";
String Password = "Nh@cuatoi303";

WiFiClient client;
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN)); // DEPG0213BN 122x250, SSD1680
//GxEPD2_3C<GxEPD2_290c, GxEPD2_290c::HEIGHT> display(GxEPD2_290c( 2, 4, 5, 0)); // GDEW029Z10 128x296, UC8151 (IL0373)
//GxEPD2_3C<GxEPD2_290_C90c, MAX_HEIGHT(GxEPD2_290_C90c)> display(GxEPD2_290_C90c( 2, 4, 5, 0));
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c( 2, 4, 5, 0)); // GDEH029Z13 128x296, UC8151D
//GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c( 2, 4, 5, 0)); // GDEM029C90 128x296, SSD1680

#define TITELCOL GxEPD_BLACK
#define NAMECOL GxEPD_BLACK
#define TIMECOL GxEPD_BLACK
String title = "Upcoming Events";

void extractData(const String& str);
String getValue(String data, char separator, int index);
void readCalendar();
void displayEvent(int i, int y);
void convTime();
void convDayName();
void errorCode();

void setup() {
  Serial.begin(115200);

  pinMode(15, OUTPUT); //Status LED

  digitalWrite(15, HIGH);
  display.init(115200);
  digitalWrite(15, LOW);
  display.setRotation(3);
  display.setFullWindow();
}
 
void loop() {
  pinMode(15, HIGH);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, Password);

  Serial.println("Connecting to Wi-Fi...");

  long t_start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(15, LOW);
    delay(500);
    digitalWrite(15, HIGH);
    delay(250);
    if(millis() - t_start > 20000) ESP.restart(); //nach 20 sek keine Verbingung
  }

  Serial.println("Connected to Wi-Fi");
  Serial.println(WiFi.localIP());

  display.firstPage();
  do
  {
    readCalendar();
    convTime();
    convDayName();

    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(20, 15);
    display.setTextColor(TITELCOL);
    display.print(title);

    display.drawLine(0, 20, display.width(), 20, GxEPD_BLACK);

    displayEvent(0, 40);
    displayEvent(1, 60);
    displayEvent(2, 80);
    displayEvent(3, 100);

  }
  while (display.nextPage());
  digitalWrite(15, LOW);
  display.powerOff();
  WiFi.disconnect();

  delay(1000 * 60 * 60 * 1);
}

void readCalendar() {
  Serial.println("Reading calendar data...");
  HTTPSRedirect* client = nullptr;

  //Verbindung mit Server aufbauen
  client = new HTTPSRedirect(dstPort);
  client->setInsecure();
  client->setPrintResponseBody(false);
  client->setContentTypeHeader("application/json");

  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(dstHost, dstPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else {
      errorCode();
      Serial.println("Get calendar data error!");
    }
  }

  if (!flag) {
    Serial.println("Get calendar data error!");
    errorCode();
    delete client;
    client = nullptr;
    return;
  }

  //Antwort des Servers speichern
  client->GET(dstPath, dstHost);
  String googleCalData = client->getResponseBody();

  Serial.println("Get calendar data successed!");
  Serial.println(googleCalData);

  //Daten aus String Extrahieren
  extractData(googleCalData);

  delete client;
  client = nullptr;
}

void extractData(const String& str) {
  String temp = str;
  int index = 0;
  int entryCount = 0;

  while (temp.length() > 0 && entryCount < MAX_ENTRIES) {
    //Position des ersten Semikolons
    int pos = temp.indexOf(';');
    if (pos == -1) break;

    //gefundene Daten zwischenspeichern und aus Kette löschen
    String token = temp.substring(0, pos);
    temp = temp.substring(pos + 1);

    //Über Index identifizierung welches Element vorliegt
    //dann in Liste speichern
    if (index % 3 == 0) {  // Startdatum
      entries[entryCount].startDate = token;
    } else if (index % 3 == 1) {  // Titel
      entries[entryCount].title = token;
    } else {  // Enddatum
      entries[entryCount].endDate = token;
      entryCount++;
    }
    index++;
  }
}

void convTime() {
  for(int i = 0; i<MAX_ENTRIES; i++) {
    String datetimeStr = entries[i].startDate;
    //Wochentag
    int idx = datetimeStr.indexOf(' ');
    entries[i].start.day = datetimeStr.substring(0, idx);
    datetimeStr = datetimeStr.substring(idx + 1);
    //Monat
    idx = datetimeStr.indexOf(' ');
    datetimeStr.substring(0, idx);
    datetimeStr = datetimeStr.substring(idx + 1);
    //Tag
    idx = datetimeStr.indexOf(' ');
    entries[i].start.date = datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Jahr
    idx = datetimeStr.indexOf(' ');
    datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Stunde
    idx = datetimeStr.indexOf(':');
    entries[i].start.hour = datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Minute
    idx = datetimeStr.indexOf(':');
    entries[i].start.min = datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Sekunde
    idx = datetimeStr.indexOf(' ');
    datetimeStr.substring(0, idx).toInt();
  }
  for(int i = 0; i<MAX_ENTRIES; i++) {
    String datetimeStr = entries[i].endDate;
    //Wochentag
    int idx = datetimeStr.indexOf(' ');
    entries[i].end.day = datetimeStr.substring(0, idx);
    datetimeStr = datetimeStr.substring(idx + 1);
    //Monat
    idx = datetimeStr.indexOf(' ');
    datetimeStr.substring(0, idx);
    datetimeStr = datetimeStr.substring(idx + 1);
    //Tag
    idx = datetimeStr.indexOf(' ');
    entries[i].end.date = datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Jahr
    idx = datetimeStr.indexOf(' ');
    datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Stunde
    idx = datetimeStr.indexOf(':');
    entries[i].end.hour = datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Minute
    idx = datetimeStr.indexOf(':');
    entries[i].end.min = datetimeStr.substring(0, idx).toInt();
    datetimeStr = datetimeStr.substring(idx + 1);
    //Sekunde
    idx = datetimeStr.indexOf(' ');
    datetimeStr.substring(0, idx).toInt();
  }
  
}

void convDayName() {
  for(int i = 0; i<MAX_ENTRIES; i++) {
    if(entries[i].start.day == "Mon")  entries[i].start.day = "Mo";
    else if(entries[i].start.day == "Tue")  entries[i].start.day = "Di";
    else if(entries[i].start.day == "Wed")  entries[i].start.day = "Mi";
    else if(entries[i].start.day == "Thu")  entries[i].start.day = "Do";
    else if(entries[i].start.day == "Fri")  entries[i].start.day = "Fr";
    else if(entries[i].start.day == "Sat")  entries[i].start.day = "Sa";
    else if(entries[i].start.day == "Sun")  entries[i].start.day = "So";
    if(entries[i].end.day == "Mon")  entries[i].end.day = "Mo";
    else if(entries[i].end.day == "Tue")  entries[i].end.day = "Di";
    else if(entries[i].end.day == "Wed")  entries[i].end.day = "Mi";
    else if(entries[i].end.day == "Thu")  entries[i].end.day = "Do";
    else if(entries[i].end.day == "Fri")  entries[i].end.day = "Fr";
    else if(entries[i].end.day == "Sat")  entries[i].end.day = "Sa";
    else if(entries[i].end.day == "Sun")  entries[i].end.day = "So";
  }
}

void displayEvent(int i, int y) {
  display.setFont(&FreeMonoBold9pt7b);display.setTextColor(NAMECOL);
  display.setCursor(5, y + offset);
  display.print(entries[i].title.substring(0, 12));
  if(entries[i].title.substring(12).length() > 0 && (offset == 0 || (offset == 20 && entries[3].start.day.length() == 0) || (offset == 40 && entries[2].start.day.length() == 0))) {
    offset += 20;
    display.setCursor(5, y + offset - 5);
    display.print(entries[i].title.substring(12, 24));
    display.setTextColor(TIMECOL);display.setFont();
    display.setCursor(145, y + offset - 15);
    display.printf("%s;%d %d:%d - %s;%d %d:%d", entries[i].start.day.c_str(), entries[i].start.date, entries[i].start.hour, entries[i].start.min, entries[i].end.day.c_str(), entries[i].end.date, entries[i].end.hour, entries[i].end.min);
  }
  else if(entries[i].start.day.length() > 0) {
    display.setTextColor(TIMECOL);display.setFont();
    display.setCursor(145, y-8 + offset);
    display.printf("%s;%d %d:%d - %s;%d %d:%d", entries[i].start.day.c_str(), entries[i].start.date, entries[i].start.hour, entries[i].start.min, entries[i].end.day.c_str(), entries[i].end.date, entries[i].end.hour, entries[i].end.min);
  }
}

void errorCode() {
  for(int i = 0; i<3; i++) {
    digitalWrite(15, LOW);
    delay(500);
    digitalWrite(15, HIGH);
    delay(150);
  }
}
