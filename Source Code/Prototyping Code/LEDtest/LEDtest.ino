// Libraries
#include <FastLED.h>

// Defines
#define LEDPIN 14
#define NUMOFLEDS 7

// LED Objects
CRGB leds[NUMOFLEDS];

// Function Prototypes
void setLED(int num, int Rval, int Bval, int Gval);

int ledRef[10][7] = {
  {1, 1, 1, 1, 1, 1, 0},    // 0
  {0, 0, 0, 0, 1, 1, 0},    // 1
  {1, 0, 1, 1, 0, 1, 1},    // 2
  {1, 0, 0, 1, 1, 1, 1},    // 3
  {0, 1, 0, 0, 1, 1, 1},    // 4
  {1, 1, 0, 1, 1, 0, 1},    // 5
  {1, 1, 1, 1, 1, 0, 1},    // 6
  {1, 0, 0, 0, 1, 1, 0},    // 7
  {1, 1, 1, 1, 1, 1, 1},    // 8
  {1, 1, 0, 1, 1, 1, 1},    // 9
};

// Program
void setup() {
  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, LEDPIN>(leds, NUMOFLEDS);
  FastLED.clear();
  printf("Initialization Done.\n");
}


void loop() {
  int i;
  for (i = 0; i < 9; i++) {
    printf("Displaying %d on LEDs.\n", i);
    setLED(i, 252, 3, 207);
    delay(3000);
  }
}

void setLED(int num, int Rval, int Gval, int Bval) {
  int j;
  if (num >= 0 && num <= 9) {   // Handles Numbers
    for (j = 0; j < NUMOFLEDS; j++) {
      if (ledRef[num][j] == 0) {
        leds[j].r = 0;
        leds[j].g = 0;
        leds[j].b = 0;
      } else {
        leds[j].r = Rval;
        leds[j].g = Gval;
        leds[j].b = Bval;
      }
    }
  } 
  
  FastLED.setBrightness(50);
  FastLED.show();

}
