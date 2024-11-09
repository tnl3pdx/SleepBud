//***** Libraries *****//

//* RTC Clock Library & Dependencies
#include <I2C_RTC.h>
#include <Wire.h>

//* ESP32 Wifi and NTP Libraries
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

//* LED Control
#include <FastLED.h>

//* NVS Intefacing Libaries
#include <Preferences.h>

//* Misc. Libraries
#include "creds.h"
#include "config.h"

//***** Defines *****//
#define DEBUG


//***** Function Prototypes *****//
void genSetup();
void wifiNTP();
void rtcSet();
void setLED(int num, int Rval, int Bval, int Gval, int select);
void displayLED(int bright);


//***** Objects *****//

//* LED Objects
CRGB digiLEDS[4][NDIGILEDS];
CRGB modeLEDS[NMODELEDS];
CRGB lampLEDS[NLAMPLEDS];

//* RTC and WiFi Objects
static DS3231 RTC;
WiFiUDP ntpPort;
NTPClient timeClient(ntpPort, "time.nist.gov");

//* NVS Objects
Preferences nvsObj;

//***** Global Variables *****//
uint8_t hms[3];
uint8_t alrm[] = {23, 22, 0};
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

//* Configuration Variables
bool timeUpdate;
int brightVal;
int sleepTime[2];
int utcOffset;

//***** Main Program *****//
void setup() {
  genSetup();

  if (timeUpdate == 1) {
    wifiNTP();
    rtcSet(1);
  } else {
    rtcSet(0);
  }

}


void loop() {




}


void genSetup() {
  #ifdef DEBUG
  Serial.printf("Starting general setup.\n");
  #endif
  
  //** Fetch parameters from non-volatile memory
  nvsObj.begin("config", false);

  if (nvsObj.isKey("config") != 1) {
    #ifdef DEBUG
    Serial.printf("Config namespace has not be initialized, creating new namespace.\n");
    #endif

    // Load config namespace with parameters
    nvsObj.putBool("timeUpdate", 1);
    nvsObj.putInt("brightness", 50);
    nvsObj.putInt("sleepHR", 0);
    nvsObj.putInt("sleepMIN", 0);
    nvsObj.putInt("utcOffset", 0);

    // Initialize Global Variables
    timeUpdate = 1;
    brightVal = 50;
    sleepTime[0] = 0;
    sleepTime[1] = 0;
    utcOffset = 0;

  } else {
    // Obtain values from namespace
    timeUpdate = nvsObj.getBool("timeUpdate");
    brightVal = nvsObj.getInt("brightness");
    sleepTime[0] = nvsObj.getInt("sleepHR");
    sleepTime[1] = nvsObj.getInt("sleepMIN");
    utcOffset = nvsObj.getInt("utcOffset");
  }

  //** FastLED Setup

  // Initialize digit LEDs
  FastLED.addLeds<NEOPIXEL, DIGIPIN0>(digiLEDS[0], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN1>(digiLEDS[1], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN2>(digiLEDS[2], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN3>(digiLEDS[3], NDIGILEDS);

  // Initialize other LEDs
  FastLED.addLeds<NEOPIXEL, MODEPIN>(modeLEDS, NMODELEDS);
  FastLED.addLeds<NEOPIXEL, LAMPPIN>(lampLEDS, NLAMPLEDS);

  //** RTC Setup
  RTC.begin();
  RTC.setHourMode(CLOCK_H24);
  RTC.enableAlarmPin();

  #ifdef DEBUG
  Serial.printf("genSetup has finished successfully.\n\n");
  #endif
}


void wifiNTP() {
  #ifdef DEBUG
  Serial.printf("Wifi and NTP needs to be setup.\n");
  Serial.printf("Attempting WIFI connection: connecting to %s\n", ssid);
  #endif

  //** Start WiFi connection
  WiFi.begin(ssid, pswrd);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);

    #ifdef DEBUG
    Serial.printf(".");
    #endif
  }

  #ifdef DEBUG
  Serial.printf("\nWifi successfully connected!\n");
  #endif

  //** Start NTP Communication to fetch current time
  timeClient.begin();

  #ifdef DEBUG
  Serial.printf("timeClient started!\n");
  #endif

  // Attempt to obtain NTP packet
  while (!isTimeSet()) {
    timeClient.update();
  }

  #ifdef DEBUG
  Serial.printf("Data received from NTP server: %s\n", timeClient.getFormattedTime());
  Serial.printf("WiFi and NTP setup successful.\n\n")
  #endif
}


void rtcSet(bool getRTC) {
  String formatTime;
  char* tkn;
  int pos = 0;

  if(RTC.isRunning()) {  // Stop clock for configuration 
    RTC.stopClock();
  }

  if (getRTC) {   // Update RTC with NTP data from WiFi
    #ifdef DEBUG
    Serial.printf("Time fetch from time.nist.gov\n");
    #endif

    while (!isTimeSet()) {
      timeClient.update();
    }
  }

  // Obtain new data from RTC module
  formatTime = timeClient.getFormattedTime();

  tkn = strtok(strdup(formatTime.c_str()), ":");

  while (tkn != NULL && !(pos >= 3)) {
    hms[pos] = atoi(tkn);

    #ifdef DEBUG
    Serial.printf("Obtained token #%d: %d\n", pos, hms[pos]);
    #endif

    tkn = strtok(NULL, ":");
    pos++;
  }

  #ifdef DEBUG
  Serial.printf("HMS obtained from parsing function: %d:%d:%d\n", hms[0], hms[1], hms[2]);
  #endif 

  RTC.setTime(hms[0], hms[1], hms[2]);
  RTC.startClock();

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

void displayLED(int bright) {
  FastLED.setBrightness(bright);
  FastLED.show();
}



