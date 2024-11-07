// Libraries
#include <FastLED.h>

// Pin Defines
#define DIGIPIN0 14
#define DIGIPIN1 32
#define DIGIPIN2 15
#define DIGIPIN3 33
#define MODEPIN  27
#define LAMPPIN  12

// Defines
#define NDIGILEDS 7
#define NMODELEDS 2
#define NLAMPLEDS 40

// LED Objects
CRGB digiLEDS[4][NDIGILEDS];
CRGB modeLEDS[NMODELEDS];
CRGB lampLEDS[NLAMPLEDS];

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
  
  // Initialize digit LEDs
  FastLED.addLeds<NEOPIXEL, DIGIPIN0>(digiLEDS[0], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN1>(digiLEDS[1], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN2>(digiLEDS[2], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN3>(digiLEDS[3], NDIGILEDS);

  // Initialize other LEDs
  FastLED.addLeds<NEOPIXEL, MODEPIN>(modeLEDS, NMODELEDS);
  FastLED.addLeds<NEOPIXEL, LAMPPIN>(lampLEDS, NLAMPLEDS);

  FastLED.clear();
  printf("Initialization Done.\n");
}


void loop() {
  int i;
  for (i = 0; i < 9; i++) {
    printf("Displaying %d on LEDs.\n", i);
    setLED(i, 252, 3, 207, 0);
    setLED(i, 3, 252, 207, 1);
    setLED(i, 0, 3, 252, 2);
    setLED(i, 252, 207, 3, 3);
    setLED(-1, 255, 255, 255, -1);
    setLED(-2, 0, 255, 255, -1);
    delay(500);
  }
}

void setLED(int num, int Rval, int Gval, int Bval, int select) {
  int j;
  if ((num >= 0 && num <= 9) && (select >= 0 && select <= 3)) {   // Configure Digit Display
    for (j = 0; j < NDIGILEDS; j++) {
      if (ledRef[num][j] == 0) {
        digiLEDS[select][j].setRGB(0, 0 ,0);
      } else {
        digiLEDS[select][j].setRGB(Rval, Gval, Bval);
      }
    }
  } else if (num == -1) {       // Configure Mode Color
    fill_solid(modeLEDS, NMODELEDS, CRGB(Rval, Gval, Bval));
  } else if (num == -2) {       // Configure Lamp Color
    fill_solid(lampLEDS, NLAMPLEDS, CRGB(Rval, Gval, Bval));
  } else {
    return;
  }
  
  FastLED.setBrightness(50);
  FastLED.show();

}
