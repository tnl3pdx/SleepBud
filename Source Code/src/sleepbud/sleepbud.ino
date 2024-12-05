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
void reattachInts();
void normalLoop();
void optionLoop(int select);
void genSetup();
void wifiNTP();
void setDigitLED(int num, uint8_t hue, uint8_t sat, uint8_t val, uint8_t select);
void timeDisplay(uint8_t hue, uint8_t sat);
void setAuxLED(bool type, uint8_t hue, uint8_t sat, uint8_t val);
void isrPIR();

// Debug Functions
void testLEDs();

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
uint8_t ledRef[10][7] = {
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
uint8_t brightVal;
uint8_t sleepTime[2];
uint8_t utcOffset;
int defDispBright = 125;
int debounceDelay = 5000;
volatile int pirIntDelay = 0;
volatile int disableLEDs = 0;
volatile int menuSelect = 0;

//* WiFi Credentials
const char *ssid = WIFI_SSID;
const char *pswrd = WIFI_PASSWORD;

//***** Main Program *****//
void setup() {
  Serial.begin(115200);
  genSetup();

  if (timeUpdate == 1) {
    wifiNTP();
  }

  attachInterrupt(digitalPinToInterrupt(PIRPIN), isrPIR, RISING);
}

void loop() {

  reattachInts();

  /* Loop Selection */

  // Normal Mode (Colon is white (One Red LED if alarm is on))
  if (menuSelect == 0) {
    normalLoop();
  // Set Manual Time (Colon is all yellow)
  } else if (menuSelect == 1) {
    optionLoop(0);
  // Set Alarm Time (Colon is all red)
  } else if (menuSelect == 2) {
    optionLoop(1);
  // Enable Alarm (Colon is all orange)
  } else if (menuSelect == 3) {
    optionLoop(2);
  // Set UTC Offset (Colon is all purple)
  } else if (menuSelect == 4) {
    optionLoop(3);
  // Enable WIFI boot (Colon is all blue)
  } else if (menuSelect == 5) {
    optionLoop(4);
  }

}

void reattachInts() {
  /* Reattach Interrupts */

  // PIR Interrupt
  if ((millis() - pirIntDelay > debounceDelay) && (!digitalRead(PIRPIN))) {
    attachInterrupt(digitalPinToInterrupt(PIRPIN), isrPIR, RISING);
  }

  // Button 1 Interrupt

  // Button 2 Interrupt

  // Button 3 Interrupt

  // Button 4 Interrupt
}

void normalLoop() {

  if(RTC.isRunning()) {
    #ifdef DEBUG
    Serial.printf("Current Time: %d:%d:%d\n", RTC.getHours(), RTC.getMinutes(), RTC.getSeconds());
    #endif
    timeDisplay(0, 0);
    setAuxLED(0, 0, 0, defDispBright);
    setAuxLED(1, 0, 0, 0);
    if (disableLEDs == 1) {
      FastLED.clear();
    }
    FastLED.show();
    Serial.printf("Current State of disableLEDs: %d\n\n", disableLEDs);
  }

  // Reattach Interrupt
  if ((millis() - pirIntDelay > debounceDelay) && (!digitalRead(PIRPIN))) {
    attachInterrupt(digitalPinToInterrupt(PIRPIN), isrPIR, RISING);
  }
}

void optionLoop(int select) {
  return;

}

void genSetup() {
  #ifdef DEBUG
  Serial.printf("Starting general setup.\n");
  #endif
  
  //** Set pins
  Wire.setPins(SDAPIN, SCLPIN);

  // Setup OE for logic level shifter
  pinMode(TXSPIN, OUTPUT);
  digitalWrite(TXSPIN, HIGH);

  // Setup PIR pin
  pinMode(PIRPIN, INPUT_PULLDOWN);


  //** Fetch parameters from non-volatile memory
  
  if (!nvsObj.begin("config", false)) {
    Serial.printf("nvsObj object failed to start, looping...\n");
    while(1);
  }

  if (nvsObj.isKey("configInit") != 1) {
    #ifdef DEBUG
    Serial.printf("Config namespace has not be initialized, creating new namespace.\n");
    #endif

    // Load config namespace with parameters
    nvsObj.putBool("timeUpdate", 1);
    nvsObj.putInt("brightness", 50);
    nvsObj.putInt("sleepHR", 0);
    nvsObj.putInt("sleepMIN", 0);
    nvsObj.putInt("utcOffset", 0);

    nvsObj.putBool("configInit", 1);        // Set "already init" status for namespace
  }

  #ifdef DEBUG
  Serial.printf("Config namespace has already been created, obtaining values.\n");
  #endif

  // Obtain values from namespace
  timeUpdate = nvsObj.getBool("timeUpdate");
  brightVal = nvsObj.getInt("brightness");
  sleepTime[0] = nvsObj.getInt("sleepHR");
  sleepTime[1] = nvsObj.getInt("sleepMIN");
  utcOffset = nvsObj.getInt("utcOffset");

  // TEMPORARY
  brightVal = defDispBright;

  #ifdef DEBUG
  Serial.printf("timeUpdate obtained is: %d\n", timeUpdate);
  Serial.printf("brightVal obtained is: %d\n", brightVal);
  Serial.printf("sleepTime obtained is: %d:%d\n", sleepTime[0], sleepTime[1]);
  Serial.printf("utcOffset obtained is: %d\n", utcOffset);
  #endif

  //** FastLED Setup

  // Initialize digit LEDs
  FastLED.addLeds<NEOPIXEL, DIGIPIN0>(digiLEDS[0], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN1>(digiLEDS[1], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN2>(digiLEDS[2], NDIGILEDS);
  FastLED.addLeds<NEOPIXEL, DIGIPIN3>(digiLEDS[3], NDIGILEDS);

  // Initialize other LEDs
  FastLED.addLeds<NEOPIXEL, MODEPIN>(modeLEDS, NMODELEDS);
  FastLED.addLeds<NEOPIXEL, LAMPPIN>(lampLEDS, NLAMPLEDS);

  // Reset pixel data
  FastLED.clear();
  FastLED.show();

  //** RTC Setup
  if (!RTC.begin()) {
    Serial.printf("RTC object failed to start, looping...\n");
    while(1);
  }
  RTC.setHourMode(CLOCK_H24);
  RTC.enableAlarmPin();

  #ifdef DEBUG
  Serial.printf("genSetup has finished successfully.\n");
  #endif
}

void wifiNTP() {
  String formatTime;
  char* tkn;
  uint8_t pos = 0;


  #ifdef DEBUG
  Serial.printf("Wifi and NTP needs to be setup.\n");
  Serial.printf("Attempting WIFI connection: connecting to %s\n", ssid);
  #endif

  //** Start WiFi connection
  if (!WiFi.begin(ssid, pswrd)) {
    Serial.printf("WiFi object failed to start, looping...\n");
    while(1);
  }

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
  timeClient.setTimeOffset(utcOffset);     // Set time offset from saved parameters

  #ifdef DEBUG
  Serial.printf("timeClient started! Time offset set is: %d\n", utcOffset);
  Serial.printf("Stopping RTC clock for configuration\n");
  #endif

  if(RTC.isRunning()) {  // Stop clock for configuration 
    RTC.stopClock();
  }

  // Attempt to obtain NTP packet
  while (!timeClient.isTimeSet()) {
    if (timeClient.update() == 0) {
      #ifdef DEBUG
      Serial.printf("NTP update Failed, Trying again.\n");
      #endif
    }
  }

  // Obtain new data from RTC module
  formatTime = timeClient.getFormattedTime();

  // Turn off time and WiFi
  timeClient.end();
  WiFi.disconnect();

  #ifdef DEBUG
  Serial.printf("Data received from NTP server: ");
  Serial.println(formatTime);
  Serial.printf("\nWiFi and NTP setup successful.\n\n");
  #endif

  // Parse string to get hour, minute, and second data

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
  Serial.printf("Setting RTC module with newly obtained time and starting up RTC again.\n");
  #endif 

  RTC.setTime(hms[0], hms[1], hms[2]);
  RTC.setEpoch((time_t)timeClient.getEpochTime(), 0);     // Set epoch from NTP using non-unix epoch
  RTC.startClock();

}

void setDigitLED(int num, uint8_t hue, uint8_t sat, uint8_t val, uint8_t select) {
  uint8_t j;
  if ((num >= 0 && num <= 9) && (select <= 3)) {   // Configure Digit Display
    for (j = 0; j < NDIGILEDS; j++) {
      if (ledRef[num][j] == 0) {
        digiLEDS[select][j].setHSV(0, 0 ,0);
      } else {
        digiLEDS[select][j].setHSV(hue, sat, val);
      }
    }
  } 
}

void setAuxLED(bool type, uint8_t hue, uint8_t sat, uint8_t val) {
  if (type == 0) {       // Configure Mode Color
    fill_solid(modeLEDS, NMODELEDS, CHSV(hue, sat, val));
  } else if (type == 1) {       // Configure Lamp Color
    fill_solid(lampLEDS, NLAMPLEDS, CHSV(hue, sat, val));
  } 
}

void timeDisplay(uint8_t hue, uint8_t sat) {
  // Parse hour and minute data from RTC
  uint8_t hour = RTC.getHours();
  uint8_t min = RTC.getMinutes();
  uint8_t partH[2] = {(uint8_t)(hour/10), (uint8_t)(hour%10)};
  uint8_t partM[2] = {(uint8_t)(min/10), (uint8_t)(min%10)};;

  // Set LEDs on for 7 segment displays
  for (uint8_t i = 0; i < 2; i++) {
    setDigitLED(partH[i], hue, sat, 127, i);
    setDigitLED(partM[i], hue, sat, 127, i + 2);
  }
}

void isrPIR() {
  pirIntDelay = millis();

  if (disableLEDs == 0) {
    disableLEDs = 1;
  } else {
    disableLEDs = 0;
  }

  detachInterrupt(digitalPinToInterrupt(PIRPIN));
}

void testLEDs() {

  int i = 0;
  int j = 0;
  int k = 0;
  float percent;

  for (k = 10; k > 0; k--) {
    percent = 1.00 - (0.05 * k);
    Serial.printf("Percentage is %f\n\n", percent);
    for (i = 0; i < 4; i++) {
      for (j = 0; j < NDIGILEDS; j++) {
        digiLEDS[i][j].setHSV(0, 0, (int)(255.0 * percent));
      }
    }
  setAuxLED(0, 0, 0, (int)(255.0 * percent));
  setAuxLED(1, 0, 0, (int)(255.0 * percent));

  FastLED.show();
  delay(7000);
  }

}
