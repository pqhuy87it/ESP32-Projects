#include<FastLED.h>
#include<MegunoLink.h>
#include<Filter.h>

// define necessary parameters
#define N_PIXELS  16
#define MIC_PIN   26
#define LED_PIN   27
// the following parameters can be tweaked according to your audio levels
#define NOISE 550
#define TOP   (N_PIXELS+2) // allow the max level to be slightly off scale
#define LED_TYPE  WS2811
#define BRIGHTNESS  24     // a little dim for recording purposes
#define COLOR_ORDER GRB

// declare the LED array
CRGB leds[N_PIXELS];

// define the variables needed for the audio levels
int lvl = 0, minLvl = 0, maxLvl = 300; // tweak the min and max as needed

// instantiate the filter class for smoothing the raw audio signal
ExponentialFilter<long> ADCFilter(5,0);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // initialize the LED object
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds,N_PIXELS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // put your main code here, to run repeatedly:
  // read the audio signal and filter it
  int n, height;
  n = analogRead(MIC_PIN);
  // remove the MX9614 bias of 1.25VDC
  n = abs(1023 - n);
  // hard limit noise/hum
  n = (n <= NOISE) ? 0 : abs(n - NOISE);
  // apply the exponential filter to smooth the raw signal
  ADCFilter.Filter(n);
  lvl = ADCFilter.Current();
//  // plot the raw versus filtered signals
//Serial.print(n);
//Serial.print(" ");
//Serial.println(lvl);
  // calculate the number of pixels as a percentage of the range
  // TO-DO: can be done dynamically by using a running average of min/max audio levels
  height = TOP * (lvl - minLvl) / (long)(maxLvl - minLvl);
  if(height < 0L) height = 0;
  else if(height > TOP) height = TOP;
  // turn the LEDs corresponding to the level on/off
  for(uint8_t i = 0; i < N_PIXELS; i++) {
    // turn off LEDs above the current level
    if(i >= height) leds[i] = CRGB(0,0,0);
    // otherwise, turn them on!
    else leds[i] = Wheel( map( i, 0, N_PIXELS-1, 30, 150 ) );
  }
  FastLED.show();
}

CRGB Wheel(byte WheelPos) {
  // return a color value based on an input value between 0 and 255
  if(WheelPos < 85)
    return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}