// SDS011 Air Quality Monitor
// --------------------------
// Optionally, used in conjunction with PC Server/plotter application at (c) vwlowen.co.uk
//
// Based on SDS011 Sensor libray by R. Zschiegner (rz@madavi.de).

#include <SDS011.h>                           //  https://platformio.org/lib/show/1563/SDS011%20sensor%20Library

#include <Adafruit_GFX.h>                     //  https://github.com/adafruit/Adafruit-GFX-Library
#include <Fonts/FreeSansBold18pt7b.h  >
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <EEPROM.h>
#include "Adafruit_ST7796S_kbv.h"

const char* ssid = "MyHome1stFloor_2.4G";             // Your WiFi SSID.
const char* password = "Nh@cuatoi726";           // Your WiFi Password.

String serverIP = "192.168.1.3:8802";          // The server IP address and Port number set up in the PC Server/plotter Application.
String deviceId = "air_quality";               // The device ID that the PC server Application will recognize.


//#define TFT_RST                // (Not connected. Pull TFT RST HIGH with 10k resistor)   
//#define TFT_SCLK  D5           // SCLK is explicit and must be connected to D5 (GPIO14)
//#define TFT_MOSI  D7           //  MOSI is explicit and must be connected to D7 (GPIO13)

#define TFT_CS      D3           // GPIO0                
#define TFT_DC      D2           // GPIO4  

#define SAVE_COUNTER 12          // Data is saved to EEPROM every SAVE_COUNTER * SAMPLE_INTERVAL minutes. (1 hour).

#define SAMPLE_INTERVAL  5       // Take air sample every SAMPLE_INTERVAL minutes

#define SAMPLE_SECS  30          // Run fan for SAMPLE_SECONDS, then take air sample

#define SDS_TX       D1          // SDS011 Tx Pin  GPIO 5
#define SDS_RX       D6          // SDS011 Rx Pin  GPIO 12 (Unused IO - **Do Not Use**)

#define SDS011_PWR   D8          // Power control to SDS011 Sensor.
// #define SAVE_DATA    D4          //

// Define colours used by UK Defra to specify pollutant bands.
#define LIGHT_GREEN  0x9FF3                               
#define MID_GREEN    0x37E0
#define DARK_GREEN   0x3660
#define LIGHT_YELLOW 0xFFE0
#define MID_YELLOW   0xFE60
#define ORANGE       0xFCC0
#define LIGHT_RED    0xFB2C
#define MID_RED      0xF800
#define DARK_RED     0x9800
#define PURPLE       0xC99F
// Color definitions
#define BLACK 0x0000       ///<   0,   0,   0
#define WHITE 0xffff
#define NAVY 0x000F        ///<   0,   0, 123
#define DARKGREEN 0x03E0   ///<   0, 125,   0
#define DARKCYAN 0x03EF    ///<   0, 125, 123
#define MAROON 0x7800      ///< 123,   0,   0
#define PURPLE 0x780F      ///< 123,   0, 123
#define OLIVE 0x7BE0       ///< 123, 125,   0
#define LIGHTGREY 0xC618   ///< 198, 195, 198
#define DARKGREY 0x7BEF    ///< 123, 125, 123
#define BLUE 0x001F        ///<   0,   0, 255
#define GREEN 0x07E0       ///<   0, 255,   0
#define CYAN 0x07FF        ///<   0, 255, 255
#define RED 0xF800         ///< 255,   0,   0
#define MAGENTA 0xF81F     ///< 255,   0, 255
#define YELLOW 0xFFE0      ///< 255, 255,   0
#define WHITE 0xFFFF       ///< 255, 255, 255
#define LIGHT_ORANGE 0xFD20      ///< 255, 165,   0
#define GREENYELLOW 0xAFE5 ///< 173, 255,  41
#define PINK 0xFC18        ///< 255, 130, 198

int loopCount = 0;

Adafruit_ST7796S_kbv tft = Adafruit_ST7796S_kbv(TFT_CS, TFT_DC);

SDS011 sdsSensor;                                         // Sensor library - create an instance of the sensor.

String quality;                                           // Define PM2.5 value as LOW, MEDIUM etc (UK Defra scale).
int colour;                                               // Define PM2.5 value as colour (UK Defra scale)
short pm25Array[320];                                     // Array to hold sensor values for the histogram.

float p10, p25;                                           // Variabled for PM10 and MP2.5 data from sensor.
int error;                                                // Confirms valid data from sensor. 0 = error.

short arrayPointer = 15;                                  // Array element currently being written to (16-bit integer)
int yPos;                                                 // Vertical marker for bar chart.

int sleepSeconds;

void saveData() {
  
  EEPROM.put(0, arrayPointer);
  
  for (int i= 15; i<=319; i++ ) {
     EEPROM.put(i*2, pm25Array[i]);
  }  
  EEPROM.commit();
  
  tft.setCursor(245, 10);
  tft.setTextColor(BLUE, BLACK);
  tft.print("SAVED");
  // while (digitalRead(SAVE_DATA) == LOW);
  delay(250);
} 


void plotHistogram() {                                    // Function to re-draw the histogram.
   tft.fillRect(15,200, 319, 90, BLACK);          // Clear plotting area

   byte line;
   for (int i = 15; i <= 319;  i++){
      getTextData25(pm25Array[i] / 10);                   // Get colour corrsponding to each air quality level. Value is stored
                                                          // multiplied by 10 so divide by 10 here to get true value.                        
      line = constrain(sqrt(pm25Array[i]*40), 0, 105);    // Calculate length of line to plot. sqrt compresses higher values.
     tft.drawFastVLine(i, 305 - line, line,  colour);     // Draw vertical line in chosen colour.
   }
}


// UK air pollution bands for PM2.5 and PM10 Particles.  
// https://uk-air.defra.gov.uk/air-pollution/daqi?view=more-info&pollutant=pm25#pollutant

int getTextData25(int value) {                                                         // Function sets three global variables: 'Ypos'
  switch (value) {                                                                     // (vertical cursor position), 'colour' &
    case 0 ... 11 : yPos = 100; colour = LIGHT_GREEN; quality = "1 LOW"; break;        // 'quality' and returns half the length of the
    case 12 ... 23 : yPos = 90; colour = MID_GREEN; quality = "2 LOW"; break;          // text string 'quality' whose value is used to
    case 24 ... 35 : yPos = 80; colour = DARK_GREEN; quality = "3 LOW"; break;         // centre justify the text on the display.
    case 36 ... 41 : yPos = 70; colour = LIGHT_YELLOW; quality = "4 MODERATE"; break;
    case 42 ... 47 : yPos = 60; colour = MID_YELLOW; quality = "5 MODERATE"; break;
    case 48 ... 53 : yPos = 50; colour = ORANGE; quality = "6 MODERATE";  break;
    case 54 ... 58 : yPos = 40; colour = LIGHT_RED;  quality = "7 HIGH"; break;
    case 59 ... 64 : yPos = 30; colour = MID_RED;  quality = "8 HIGH"; break;
    case 65 ... 70 : yPos = 20; colour = DARK_RED;  quality = "9 HIGH"; break;  
    case 71 ... 9999: yPos = 10; colour = PURPLE;  quality = "10 VERY HIGH"; break;   
    default: yPos = 10; colour = MAGENTA;  quality = "HAZARDOUS"; break;
  }  
  return (quality.length() / 2) * 6;
}                                         


int getTextDataPM10(int value) {
  switch (value) {
    case 0 ... 16 : colour = LIGHT_GREEN; quality = "1 LOW"; break;
    case 17 ... 33 : colour = MID_GREEN; quality = "2 LOW"; break;
    case 34 ... 50 :  colour = DARK_GREEN; quality = "3 LOW"; break;
    case 51 ... 58 : colour = LIGHT_YELLOW; quality = "4 MODERATE"; break;
    case 59 ... 66 : colour = MID_YELLOW; quality = "5 MODERATE"; break;
    case 67 ... 75 : colour = ORANGE; quality = "6 MODERATE";  break;
    case 76 ... 83 : colour = LIGHT_RED;  quality = "7 HIGH"; break;
    case 84 ... 91 : colour = MID_RED;  quality = "8 HIGH"; break;
    case 92 ... 100 : colour = DARK_RED;  quality = "9 HIGH"; break;  
    case 101 ... 9999: colour = PURPLE;  quality = "10 VERY HIGH"; break;   
    default: colour = MAGENTA;  quality = "HAZARDOUS"; break;
  }
   return (quality.length() / 2) * 6;
}

void setup_st7796s() {
  tft.begin(); ;

  tft.setRotation(1); 
  tft.setTextWrap(true);

  tft.fillScreen(BLACK);
  tft.setTextSize(1);
}

void setup_EEPROM() {
  EEPROM.begin(1000);
  

  //-- The following block of code tests if the EEPROM has been 'prepared'  ----
  //-- with all zeroes and clears it if necessary.------------------------------ 
   
  bool eraseFlag = false;
  for (int i = 2; i< 30; i++ ) {
    if (EEPROM.read(i) != 0) {
      eraseFlag = true;
      break;
    }
  }
      
   if (eraseFlag) {
     for (int i=0; i< 640; i++){
       EEPROM.write(i, (byte) 0);                      // Reset EEPROM addresses to zero
     }
     EEPROM.put(0, (short) 15);                         // Reset array Pointer to start address (15)
     EEPROM.commit();
   }

 //-------------------------------------------------------------------------------

  EEPROM.get(0, arrayPointer);
  for (int i=15; i<319; i++) {
    EEPROM.get(i*2, pm25Array[i]);
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);                                   // Connect WiFi to server running on PC to
                                                                // plot PM10 and PM2.5 values.                                              
  int wifi_timeout = 0;

  tft.setCursor(0, 10);
  tft.print("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED)  {
    delay(10);
    tft.setCursor(0, 20);
    tft.fillRect(0, 20, 100, 10, BLACK);
    tft.print(wifi_timeout);
    wifi_timeout++;
    if(wifi_timeout > 1000) {
      tft.setCursor(0, 35);
      tft.println("Failed to connect to Wifi");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.setCursor(0, 20);
    tft.print("Connected to ");
    tft.println(WiFi.SSID());
    tft.print("IP address: ");
    tft.println(WiFi.localIP());
    delay(5000);
  }
}

void setup() {

  pinMode(SDS011_PWR, OUTPUT); 
  // pinMode(SAVE_DATA, INPUT_PULLUP);
  pinMode(A0, INPUT); 

  setup_EEPROM();
  
  setup_st7796s();
 
  sleepSeconds = (SAMPLE_INTERVAL * 60) - SAMPLE_SECS;          // Calculate sleep time in seconds.

  setup_wifi();
 
  tft.fillScreen(BLACK);
 
  tft.setCursor(15, tft.height() -20);
 
  tft.setTextSize(1);                                         // Print static labels and headers on display.
  tft.setCursor(150, 10);
  tft.println("PM 2.5");
  tft.setCursor(150, 17);
  tft.print("ug/m3");

  tft.setCursor(5, 10);
  tft.print("PM 10"); 
  tft.setCursor(5, 17);
  tft.print("ug/m3"); 

  tft.fillRect(472, 10, 6, 10, PURPLE);                        // Print a colour key for the Defra pollutant bands.
  tft.fillRect(472, 20, 6, 10, DARK_RED);
  tft.fillRect(472, 30, 6, 10, MID_RED);
  tft.fillRect(472, 40, 6, 10, LIGHT_RED);
  tft.fillRect(472, 50, 6, 10, ORANGE);
  tft.fillRect(472, 60, 6, 10, MID_YELLOW);
  tft.fillRect(472, 70, 6, 10, LIGHT_YELLOW);
  tft.fillRect(472, 80, 6, 10, DARK_GREEN);
  tft.fillRect(472, 90, 6, 10, MID_GREEN);
  tft.fillRect(472, 100, 6, 10, LIGHT_GREEN);
    
  tft.drawFastVLine(13, 200, 108, BLUE);               // Draw histogram vertical axis
  tft.setTextColor(BLUE);
 
  tft.setCursor(0, 200);
  tft.print(" ^");
  tft.setCursor(0, 210);
  tft.print("50"); 
  tft.setCursor(0, 250);
  tft.print("10"); 
  tft.setCursor(0, 280);
  tft.print(" 1");

  tft.drawFastHLine(12, tft.height() - 14, tft.width()-1, BLUE);     // Draw histogram horizontal axis

  for (int x = 319; x > 15; x-=12) {
     tft.drawFastVLine(x, 303, 3, BLUE);                             // Draw 1-hour ticks on horizontal axis.
  }

  tft.setTextColor(BLUE);
  tft.setCursor(105, 312); 
  tft.print("Air Quality Monitor");

   int rssi = WiFi.RSSI();

   tft.setCursor(0, 312); 
   tft.fillRect(0, 312, 50, 8, BLACK);
   tft.setTextSize(1);

   if (WiFi.status() == WL_CONNECTED) {
     tft.setTextColor(BLUE);
     tft.print("RSSI " + String(rssi) + " dB");
   } else {
     tft.setTextColor(RED);
     tft.print("No WiFi");    
   }
   
  tft.setTextColor(GREEN);
 
  tft.setTextSize(3);
  
  Serial.begin(9600);
  sdsSensor.begin(SDS_TX, SDS_RX);    // Begin sensor and define Tx and Rx pins.

  plotHistogram();
                                    
}



void loop() {

  digitalWrite(SDS011_PWR, HIGH);                         // Turn on SDS011 Sensor power

  tft.setTextSize(1);                                     //
  tft.setCursor(408, 312);   
  tft.setTextColor(GREEN, BLACK);
  tft.print("SAMPLING ");  

  for (int i = SAMPLE_SECS; i>=0; i--) {                // Run fan for 30 seconds to ensure new air
    
    tft.setCursor(460,  312);                         
     if (i < 10) {
       tft.print("0");
     }
     tft.print(i);

    //  if (digitalRead(SAVE_DATA) == LOW) {
    //     saveData();
    //  }   
          
     delay(1000);
  }
  
  tft.fillRect(248, 232, 70, 10, BLACK);
 
  tft.fillRect(85, 105, 65, 8, BLACK);
    
	error = sdsSensor.read(&p25,&p10);                   // Read PM2.5 and PM10 values from sensor.

	if (! error) {
   Serial.print("P2.5: ");
   Serial.println(p25);
	 Serial.print("P10:  ");
   Serial.println(p10);
    

   int x = getTextData25(p25);                                          // Function retuns (width of text)/2 so we can
                                                                        // centre-justify it on the display. It also sets
   tft.setTextColor(colour);                                            // the text colour appropriate to the PM2.5 value as
                                                                        // defined by the UK Defra documentation.
                                                                        
   tft.fillRect(465, 10, 5, 105, BLACK);                        // Clear old triangle
   tft.fillTriangle(465, yPos, 468, yPos+5, 465, yPos+10, colour);      // Plot new position of triangle on colour scale
   
   tft.fillRect(100, 40, 110, 8, BLACK);                        // Clear display areas where new text will                
                                                                        // be drawn. (Graphical fonts don't overwrite
   tft.fillRect(0, 36, 285, 77, BLACK);                         // previous text.
   
   tft.setCursor(165 - x, 40);                                          // Set cursor to centre of display area.
   tft.print(quality);

   tft.setTextSize(2);
   tft.setFont(&FreeSansBold18pt7b);                                    // Change to new font.

   String sp25 = String(p25);                                           // Convert PM2.5 value to text because the
                                                                        // 'getTextBounds' function needs text.
   int16_t x1, y1;
   uint16_t w, h;
   tft.getTextBounds(sp25, 0,0, &x1, &y1, &w, &h);                      // We mainly want the width of the text that
                                                                        // we're about to print so we can centre-justify it.   
   tft.setCursor(183-(w/2), 110);
   tft.print(p25, 1);
   
   tft.setFont();                                                       // Revert to standard font.
   tft.setTextSize(1);
   
   tft.setTextColor(colour, BLACK);                             // PM10 data is less-used so just print it
   tft.fillRect(0, 30, 100, 10, BLACK);                         // in the top left corner of the display.
   tft.setCursor(0, 40);
   tft.print(quality);
   tft.setTextSize(2);
   tft.fillRect(0, 55, 60, 20, BLACK);
 
   tft.setCursor(2, 55);
   tft.print(p10, 1);

   tft.setTextSize(1); 
   tft.setTextColor(GREEN);
   tft.fillRect(80, 10, 50, 10, BLACK);
   tft.fillRect(245, 10, 30, 10, BLACK);


// ====== plot histogram (bar graph) ==============

   if (arrayPointer >= 319) {                              // If array has been filled, move all values down one.
     for (int i = 15; i <= 319; i++) {
       pm25Array[i] = pm25Array[i+1];
     }
   }
 
   pm25Array[arrayPointer] = (short) (p25 * 10);           // Multiply float value by 10 to make short integer.

   plotHistogram();
   
   if (arrayPointer < 319) arrayPointer++;                 // Increment the pointer to store the next value.



   delay(100);

 
 //======= end plot ====================


   int rssi = WiFi.RSSI();                                   // Get the WiFi signal strength and print on the display.

   tft.setCursor(0, 312); 
   tft.fillRect(0, 312, 50, 8, BLACK);
   tft.setTextSize(1);
   
   if (WiFi.status() == WL_CONNECTED) {
     tft.setTextColor(BLUE);
     tft.print(" RSSI " + String(rssi) + " dB");
   } else {
     tft.setTextColor(RED);
     tft.print(" No WiFi");    
   }
 

   if (WiFi.status() == WL_CONNECTED)  { 
     HTTPClient http;
 
    // Specify request destination, including your GET variables 
     
     String http_request = "";

     http_request = "http://" + serverIP  + "/apage?";      // Build the text string for the HTTP GET request to the PC server.
     http_request += "id=" + deviceId;
     http_request += "&leftaxis=" + sp25;
     http_request += "&rightaxis=" + String(p10);
     http_request += "&rssi=" + String(rssi);
    //  http_request += "&volts=" + String(volts);
    
     Serial.println("Making HTTP request...");
     Serial.println(http_request);

    //  http.begin(http_request);  
     
    // Send the request
     int httpCode = http.GET();

    // Check the returning HTTP code
     if (httpCode > 0) {
      // Get a response back from the server
       String payload = http.getString();
      // Print the response
       Serial.println("HTTP Response: ");
       Serial.println(payload);
     }

    // Close the HTTP connection
      http.end(); 

    } 
	}
 
	digitalWrite(SDS011_PWR, LOW);                        // Turn off SDS011 Power.

  delay(1000);
  tft.setTextSize(1);

  tft.setCursor(408, 312); 
  tft.setTextColor(BLUE, BLACK);
  tft.print("SLEEP   ");  

  int secs;
  int mins; 

 for (int i = sleepSeconds; i>0; i--) {                 // Sleep for the sample interval (less the 30 seconds warmup time)
     tft.setCursor(445, 312);                          //
     secs = i;
     tft.print(secs / 60);                              // Print minutes remaining.
     tft.print(":");
     
     if (secs % 60 < 10) {
       tft.print("0");
     }
     tft.print(secs % 60);                              // Print seconds remaining.
     tft.print(" ");

    //  if (digitalRead(SAVE_DATA) == LOW) {               // Manually save histogram data to EEPROM - don't wait for the
    //     saveData();                                     // auto-save after one hour to expire.
    //  }   

     delay(1000);
  }

  loopCount++;
  if (loopCount >= SAVE_COUNTER) {                      // Each loop takes 5 minutes. 12 loops = 5 * 12 = 60 minutes
    loopCount = 0;
    saveData();
  }

  Serial.print("Heap size at end of loop ");
  Serial.println(system_get_free_heap_size() );         // Free  memory check!

  
}
