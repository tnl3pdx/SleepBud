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

// Global Variables
volatile bool modeButtonPressed = false;
volatile bool minusButtonPressed = false;
volatile bool plusButtonPressed = false;
volatile bool selectButtonPressed = false;
int modeCounter = 0; // Modes: 0 - 3
bool alarmSet = false; // Alarm state (off/on)
bool alarmActive = false; // Alarm currently ringing
uint8_t brightness = 50; // Default brightness (mode 0)
uint8_t setDisplayTime[4] = {0, 0, 0, 0}; //displayed time // This is the global time switch to another global time????
uint8_t alarmTime[4] = {0, 0, 0, 0}; // Alarm time (HH:MM)
uint8_t utcOffset[4] = {0, 0, 0, 0}; // UTC Offset (HH:MM)
uint8_t currentField = 0; // Field selector for modes 1 and 3

// Timer for Debounce
unsigned long lastPressTime = 0; // Last button press time
const unsigned long debounceInterval = 600; // 200ms debounce interval

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
void ISR_modeButton();
void ISR_minusButton();
void ISR_plusButton();
void ISR_selectButton();
void updateModeIndicator();
void handleMode0();
void handleMode1();
void handleMode2();
void handleMode3();
void handleMode4();


// Debug Functions
void testLEDs();

//***** Objects *****//

// // LED Definitions
// #define NUM_LEDS 4
// #define LED_PIN 5
// CRGB leds[NUM_LEDS];

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

  // Initialize Buttons
  pinMode(MODEBUTTON, INPUT_PULLUP);
  pinMode(DOWNBUTTON, INPUT_PULLUP);
  pinMode(UPBUTTON, INPUT_PULLUP);
  pinMode(SWITCHBUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(MODEBUTTON), ISR_modeButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(DOWNBUTTON), ISR_minusButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(UPBUTTON), ISR_plusButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(SWITCHBUTTON), ISR_selectButton, FALLING);

  // Where is this for you?
  // // Initialize LEDs
  // FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  // FastLED.clear();
  // FastLED.show();

  genSetup();

  if (timeUpdate == 1) {
    wifiNTP();
  }

  attachInterrupt(digitalPinToInterrupt(PIRPIN), isrPIR, RISING);
  Serial.println("Setup complete");
}

void loop() {


  // Handle mode button press
  if (modeButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    modeButtonPressed = false;
    modeCounter = (modeCounter + 1) % 5; // Cycle through modes 0-4
    updateModeIndicator();
    Serial.printf("Mode changed to: %d\n", modeCounter);
  }

  // Handle modes
  switch (modeCounter) {
    case 0:
      handleMode0();
      break;
    case 1:
      handleMode1();
      break;
    case 2:
      handleMode2();
      break;
    case 3:
      handleMode3();
      break;
    case 4:
      handleMode4();
      break;
  }

// Should this be added into mode 0????

  reattachInts();

  /* Loop Selection */
  normalLoop();

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
    setAuxLED(0, 0, 0, 204);
    setAuxLED(1, 0, 0, 204);
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


// ISR Functions
void ISR_modeButton() { modeButtonPressed = true; }
void ISR_minusButton() { minusButtonPressed = true; }
void ISR_plusButton() { plusButtonPressed = true; }
void ISR_selectButton() { selectButtonPressed = true; }


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
    nvsObj.putInt("brightness", 50); //DOES THIS CHANGE THE BRIGHTNESS?
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
    setDigitLED(partH[i], hue, sat, 64, i);
    setDigitLED(partM[i], hue, sat, 64, i + 2);
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


// Update Mode Indicator
void updateModeIndicator() {
  FastLED.clear();
  CRGB color;
  switch (modeCounter) {
    case 0: color = CRGB::Black; break;  // No color
    case 1: color = CRGB::Red; break;    // Alarm set mode
    case 2: color = CRGB::Blue; break;   // Alarm active mode
    case 3: color = CRGB::Green; break;  // UTC offset mode
  }
  modeLEDS[0] = color; // Use the first LED to indicate the mode
  modeLEDS[1] = color; // Two mode LEDS
  FastLED.show();
}

// Mode Handlers
void handleMode0() {
  if (minusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    minusButtonPressed = false;
    brightness = max(brightness - 10, 0);
    FastLED.setBrightness(brightness);
    FastLED.show();
    Serial.printf("Brightness decreased to: %d\n", brightness);
  }

  if (plusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    plusButtonPressed = false;
    brightness = min(brightness + 10, 255);
    FastLED.setBrightness(brightness);
    FastLED.show();
    Serial.printf("Brightness increased to: %d\n", brightness);
  }

  if (selectButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    selectButtonPressed = false;
    alarmActive = false;
    Serial.println("Alarm snoozed");
    Serial.printf("Time adjusted: %02d:%02d\n", setDisplayTime[0] * 10 + setDisplayTime[1], setDisplayTime[2] * 10 + setDisplayTime[3]);
  }
}

void handleMode1() {
  if (minusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    minusButtonPressed = false;
    adjustTimeField(alarmTime, false);
  }

  if (plusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    plusButtonPressed = false;
    adjustTimeField(alarmTime, true);
  }

  if (selectButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    selectButtonPressed = false;
    currentField = (currentField + 1) % 4;
    Serial.printf("Selected field: %d\n", currentField);
  }
}

void handleMode2() {
  if (selectButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    selectButtonPressed = false;
    alarmSet = !alarmSet;
    Serial.printf("Alarm set: %s\n", alarmSet ? "ON" : "OFF");
    modeLEDS[0] = alarmSet ? CRGB::White : CRGB::Black; // Indicate alarm set with a white LED
    modeLEDS[1] = alarmSet ? CRGB::White : CRGB::Black; // Indicate alarm set with a white LED
    FastLED.show();
  }
}

// red is 0 so negative
// neutral so 1 is white 
// green is 2 so positive


//short UTC_offset_array[38][3] = {-12:00, -11:00, -10:00, -09:30, -09:00, -08:00, -07:00, -06:00, -05:00, -04:00, -03:30, -03:00, -02:00, -01:00, 00:00, +01:00, +02:00, +03:00, +03:30, +04:00, +04:30, +05:00, +05:30, +05:45, +06:00, +06:30, +07:00, +08:00, +08:45, +09:00, +09:30, +10:00, +10:30, +11:00, +12:00, +12:45, +13:00, +14:00 };
short UTC_offset_array[38][3] = 
  {
    {12, 0, 0},   {11, 0, 0},   {10, 0, 0},   {9, 30, 0}, 
    {9, 0, 0},    {8, 0, 0},    {7, 0, 0},    {6, 0, 0}, 
    {5, 0, 0},    {4, 0, 0},    {3, 30, 0},   {3, 0, 0}, 
    {2, 0, 0},    {1, 0, 0},    {0, 0, 2},    {1, 0, 2}, 
    {2, 0, 2},    {3, 0, 2},    {3, 30, 2},   {4, 0, 2}, 
    {4, 30, 2},   {5, 0, 2},    {5, 30, 2},   {5, 45, 2}, 
    {6, 0, 2},    {6, 30, 2},   {7, 0, 2},    {8, 0, 2}, 
    {8, 45, 2},   {9, 0, 2},    {9, 30, 2},   {10, 0, 2}, 
    {10, 30, 2},  {11, 0, 2},   {12, 0, 2},   {12, 45, 2}, 
    {13, 0, 2},   {14, 0, 2}
};

void handleMode3() {
// This should be an integer value that is either positive or negative or zero
// This requires a HH:MM to a int seconds value to pass to utcoffset func

  int start_UTC_array= 15; //start at 0

  if (minusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    minusButtonPressed = false;
    //UTC_offset_array[start_UTC_array]

    adjustTimeField(utcOffset, false);
  }

  if (plusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    plusButtonPressed = false;
    adjustTimeField(utcOffset, true);
  }

  if (selectButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    selectButtonPressed = false;
    currentField = (currentField + 1) % 4;
    Serial.printf("Selected field: %d\n", currentField);
  }
}

void handleMode4() {
  // Decrease selected field value
  if (minusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    minusButtonPressed = false;
    adjustTimeField(setDisplayTime, false); // Decrease selected field
    Serial.println("Minus button pressed: Time adjusted");
  }

  // Increase selected field value
  if (plusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    plusButtonPressed = false;
    adjustTimeField(setDisplayTime, true); // Increase selected field
    Serial.println("Plus button pressed: Time adjusted");
  }

  // Move to the next field
  if (selectButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    selectButtonPressed = false;
    currentField = (currentField + 1) % 4; // Cycle through fields 0 to 3
    Serial.printf("Field selected: %d\n", currentField);
  }

  // Provide LED feedback for the selected field
  FastLED.clear();
  leds[currentField] = CRGB::Orange; // Highlight the current field in Orange
  FastLED.show();
}

// Adjust Time Field Helper Function
void adjustTimeField(uint8_t* timeArray, bool increase) {
  if (currentField == 0) timeArray[currentField] = increase ? (timeArray[currentField] == 1 ? 0 : 1) : (timeArray[currentField] == 0 ? 1 : 0);
  else if (currentField == 2) timeArray[currentField] = increase ? (timeArray[currentField] + 1) % 6 : (timeArray[currentField] == 0 ? 5 : timeArray[currentField] - 1);
  else timeArray[currentField] = increase ? (timeArray[currentField] + 1) % 10 : (timeArray[currentField] == 0 ? 9 : timeArray[currentField] - 1);
  Serial.printf("Time adjusted: %02d:%02d\n", timeArray[0] * 10 + timeArray[1], timeArray[2] * 10 + timeArray[3]);
}


// Things that need to be adjusted

// Include the LEDs to change depending on mode (and if alarm on or off)
// Ensure brightness can be adjusted for the lamp ( I use fast led lib)
// Does switching modes work properly and does it interupt the time in main loop
// Does the UTC actually work as expected?
// Does the alarm work at the set time
// Does the snooze work (  RTC.enableAlarmPin(); ) where it says and ensure it works
// Check that the time is actually changed after setting it manually

// Comprehensive test
// does mode 0 adjust brightness and snooze correctly

// Does mode 1 adjust the time correctly (go back to mode 0 and check)

// Does mode 2 turn on the alarm correctly for the set alarm time (check mode 0 if mode lights are on and if buzzer goes off at that time)

// Does mode 3 adjust the UTC offset correctly (check mode 0 for corrected time)

// Does mode 4 manually set the time correctly (check mode 0 for time changed)

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

