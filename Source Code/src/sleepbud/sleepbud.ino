

/* Notes:

Include the LEDs to change depending on mode (and if alarm on or off)
Ensure brightness can be adjusted for the lamp ( I use fast led lib)
Does switching modes work properly and does it interupt the time in main loop
Does the UTC actually work as expected?
Does the alarm work at the set time
Does the snooze work (  RTC.enableAlarmPin(); ) where it says and ensure it works
Check that the time is actually changed after setting it manually

Comprehensive test:
does mode 0 adjust brightness and snooze correctly

Does mode 1 adjust the time correctly (go back to mode 0 and check)

Does mode 2 turn on the alarm correctly for the set alarm time (check mode 0 if mode lights are on and if buzzer goes off at that time)

Does mode 3 adjust the UTC offset correctly (check mode 0 for corrected time)

Does mode 4 manually set the time correctly (check mode 0 for time changed)
*/

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
#define TOTALBUTTONS 4
#define NUMMODES  6
#define TOTALCOLORS 6

//***** Function Prototypes *****//

// Menu UI Functions
void pollButtons();
void pollPIR();
void updateModeIndicator();
void ui_normalLoop();
void ui_configTime();
void ui_enableAlarm();
void ui_configAlarm();
void ui_selectUTC();
void ui_enableNTPUpdate();

// General Setup Functions
void genSetup();
void wifiNTP();

// Helper Functions
void setDigitLED(int num, uint8_t hue, uint8_t sat, uint8_t val, uint8_t select);
void timeDisplay(uint8_t hue, uint8_t sat, uint8_t mode);
void setAuxLED(bool type, uint8_t hue, uint8_t sat, uint8_t val);
long UTCToSeconds(short utcOffsetTriplet[]);

// ISR Functions
void alarmISR();
void isrPIR();

// Debug Functions
void testLEDs();
void testDigits();


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

/* UTC Offset Data: 
	{HOUR, MINUTE, TYPE}

	NEGATIVE	-	RED 	(0)
	NEUTRAL		- WHITE (1)
	POSITIVE	- GREEN (2)
*/
short utcOffsetArray[38][3] = {
	{12, 0, 0},   {11, 0, 0},   {10, 0, 0},   {9, 30, 0}, 
	{9, 0, 0},    {8, 0, 0},    {7, 0, 0},    {6, 0, 0}, 
	{5, 0, 0},    {4, 0, 0},    {3, 30, 0},   {3, 0, 0}, 
	{2, 0, 0},    {1, 0, 0},    {0, 0, 1},    {1, 0, 2}, 
	{2, 0, 2},    {3, 0, 2},    {3, 30, 2},   {4, 0, 2}, 
	{4, 30, 2},   {5, 0, 2},    {5, 30, 2},   {5, 45, 2}, 
	{6, 0, 2},    {6, 30, 2},   {7, 0, 2},    {8, 0, 2}, 
	{8, 45, 2},   {9, 0, 2},    {9, 30, 2},   {10, 0, 2}, 
	{10, 30, 2},  {11, 0, 2},   {12, 0, 2},   {12, 45, 2}, 
	{13, 0, 2},   {14, 0, 2}
};

//* 7 Segment LED Reference Data
uint8_t ledRef[10][7] = {
	{1, 1, 1, 1, 1, 1, 0},    // 0
	{0, 1, 1, 0, 0, 0, 0},    // 1
	{1, 0, 1, 1, 0, 1, 1},    // 2
	{1, 1, 1, 1, 0, 0, 1},    // 3
	{0, 1, 1, 0, 1, 0, 1},    // 4
	{1, 1, 0, 1, 1, 0, 1},    // 5
	{1, 1, 0, 1, 1, 1, 1},    // 6
  {0, 1, 1, 1, 0, 0, 0},    // 7
  {1, 1, 1, 1, 1, 1, 1},    // 8
  {1, 1, 1, 1, 1, 0, 1}    // 9
};

//* Color table that holds hue/sat for mode select
int colorTable[TOTALCOLORS][2] = {
    {0, 0},       // White
    {0, 255},     // Red
    {85, 255},    // Green
    {170, 255},   // Blue
    {42, 255},    // Yellow
    {212, 255}    // Purple
};

//* Button States
volatile bool minusPressed  = 0;
volatile bool plusPressed   = 0;
volatile bool selectPressed = 0;
volatile bool pressed[TOTALBUTTONS] = {0};

//* Menu UI Variables
int     modeCounter     = 0;  // Modes: 0 - 3
bool    alarmSet        = 0;  // Alarm state (off/on)
bool    alarmActive     = 0;  // Alarm currently ringing
uint8_t currentField    = 0;  // Field select or for modes 1 and 3

//* Timer for Debounce
unsigned long       lastPressTime     = 0;    // Last button press time
const unsigned int  debounceInterval  = 40;   // Debounce interval

//* Timer for PIR
unsigned long       lastPIRTrigger  = 0;
const unsigned int  pirDelay        = 3000;

//* Timer for blink
unsigned long       lastBlink       = 0;
const unsigned int  blinkDelay      = 500;
bool blinkToggle = 0;

//* Data Values for Clock
uint8_t hms[3];
uint8_t rtcAlarm[2];

//* Configuration Variables
bool      timeUpdate;
uint8_t   lampBrightVal;
uint8_t   utcOffset;

//* WiFi Credentials
const char *ssid = WIFI_SSID;
const char *pswrd = WIFI_PASSWORD;

//* Setting Variables
volatile bool lampEN = 1;
bool uiDoOnce = 0;
bool resetNVS = 0;
int selected_digit = 0;
uint8_t dispBrightVal = 50;   // Default display brightness
uint8_t maxLampBright = 204;  // 80% of max brightness
uint8_t colorPick = 0;


//***** Main Program *****//
void setup() {
  Serial.begin(115200);

  genSetup();

  if (timeUpdate == 1) {
    wifiNTP();
  }

  setAuxLED(0, 100, 127, dispBrightVal);
  if (lampEN == 1) {
    setAuxLED(1, 0, 0, lampBrightVal);
  }

  attachInterrupt(digitalPinToInterrupt(PIRPIN), isrPIR, RISING);
  Serial.println("Setup complete");
  
}

void loop() {
	
  // Reattach PIR Interrupt
  if ((millis() - lastPIRTrigger > pirDelay) && (!digitalRead(PIRPIN))) {
    attachInterrupt(digitalPinToInterrupt(PIRPIN), isrPIR, RISING);
  }

  if (lampEN == 1) {
    setAuxLED(1, colorTable[colorPick][0], colorTable[colorPick][1], lampBrightVal);
  } else {
    setAuxLED(1, 0, 0, 0);
  }

	pollButtons();

  // Handle modes
  switch (modeCounter) {
    case 0:
      ui_normalLoop();
      break;
    case 1:
      ui_configTime();
      break;
    case 2:
      ui_enableAlarm();
      break;
    case 3:
      ui_configAlarm();
      break;
    case 4:
      ui_selectUTC();
      break;
    case 5:
      ui_enableNTPUpdate();
      break;  
  }

  FastLED.show();

}

void pollButtons() {

  // Reset pressed state for mode button
  if (digitalRead(MODEBUTTON) == 1) {
    pressed[0] = 0;
  }

    // Reset pressed state for UP button
  if (digitalRead(UPBUTTON) == 1) {
    pressed[1] = 0;
  }

    // Reset pressed state for DOWN button
  if (digitalRead(DOWNBUTTON) == 1) {
    pressed[2] = 0;
  }
  
  // Reset pressed state for select button
  if (digitalRead(SWITCHBUTTON) == 1) {
    pressed[3] = 0;
  }
  
	// Check if there has been a button pressed recently
  if (millis() - lastPressTime > debounceInterval) {
		if (digitalRead(MODEBUTTON) == 0) {
      // Check if mode button has been released (one press -> one action only)
      if (!pressed[0]) {

        // we just left mode 1 and this sets the time
        if (modeCounter == 1) {
          // This is for after all digits are selected correctly
          if(RTC.isRunning()) {  // Stop clock for configuration 
            RTC.stopClock();
            RTC.setTime(hms[0], hms[1], hms[2]);
            RTC.startClock();
          }
        } else if (modeCounter == 3) {
            if(RTC.isRunning()) {
              RTC.setAlarm1(rtcAlarm[0], rtcAlarm[1], 0);
            }
            nvsObj.begin("config", false);
            nvsObj.putInt("alarmHr", rtcAlarm[0]);
            nvsObj.putInt("alarmMin", rtcAlarm[1]);
            nvsObj.end();
        } else if (modeCounter == 4) {
            nvsObj.begin("config", false);
            nvsObj.putInt("utcOffset", utcOffset);
            nvsObj.end();
            #ifdef DEBUG
            Serial.printf("UTC changed to: %d\n", utcOffset);
            #endif
        }

        modeCounter = (modeCounter + 1) % NUMMODES;

        #ifdef DEBUG
        Serial.printf("Mode changed to: %d\n", modeCounter);
        #endif

        updateModeIndicator();

        selected_digit = 0;
        uiDoOnce = 0;
        pressed[0] = 1;
      }
    } else if (digitalRead(UPBUTTON) == 0) {
      if (!pressed[1]) {
        plusPressed = 1;
        #ifdef DEBUG
        Serial.printf("Plus Pressed\n");
        #endif
        pressed[1] = 1;
      }
    } else if (digitalRead(DOWNBUTTON) == 0) {
      if (!pressed[2]) {
        minusPressed = 1;
        #ifdef DEBUG
        Serial.printf("Minus Pressed\n");
        #endif
        pressed[2] = 1;
      }
    } else if (digitalRead(SWITCHBUTTON) == 0) {
      if (!pressed[3]) {
        selectPressed = 1;
        #ifdef DEBUG
        Serial.printf("Select Pressed\n");
        #endif
        pressed[3] = 1;
      }
    }
		lastPressTime = millis();
	}

}

void pollPIR() {
  if (millis() - lastPIRTrigger > pirDelay) {
    if (digitalRead(PIRPIN) == 1) {
      if (lampEN) {
        lampEN = 0;
      } else {
        lampEN = 1;
      }
    }

    lastPIRTrigger = millis();
  }
}

// Update Mode Indicator
void updateModeIndicator() {
  uint8_t hue, sat;

  switch (modeCounter) {
    case 0: hue = 100; sat = 127;     break;    // Teal
    case 1: hue = 0; sat = 255;       break;    // Red
    case 2: hue = 85; sat = 255;      break;    // Green
    case 3: hue = 170; sat = 255;     break;    // Purple
    case 4: hue = 42; sat = 255;      break;    // Yellow
    case 5: hue = 212; sat = 255;     break;    // Pink
  }

  setAuxLED(0, hue, sat, dispBrightVal);
}

// Mode Handlers
void ui_normalLoop() {

  // Display Time
  if(RTC.isRunning()) {
    #ifdef DEBUG
    //Serial.printf("Current Time: %d:%d:%d\n", RTC.getHours(), RTC.getMinutes(), RTC.getSeconds());
    #endif

    if (RTC.getHours() > 12) {
      timeDisplay(0, 0, 0);
    } else {
      timeDisplay(35, 167, 0);
    }
  }

  if (plusPressed) {
    lampBrightVal = constrain(lampBrightVal + 10, 0, maxLampBright);

    Serial.printf("Brightness increased to: %d\n", lampBrightVal);

    nvsObj.begin("config", false);
    nvsObj.putInt("lampBrightVal", lampBrightVal);
    nvsObj.end();

    plusPressed = 0;
  } else if (minusPressed) {
    lampBrightVal = constrain(lampBrightVal - 10, 0, maxLampBright);

    Serial.printf("Brightness decreased to: %d\n", lampBrightVal);

    nvsObj.begin("config", false);
    nvsObj.putInt("lampBrightVal", lampBrightVal);
    nvsObj.end();

    minusPressed = 0;
  } else if (selectPressed) {
    if (alarmActive) {
      RTC.clearAlarm1();
      RTC.disableAlarm1();
      digitalWrite(BUZZPIN, LOW);
      setAuxLED(0, 100, 127, dispBrightVal);
      FastLED.show();
      alarmActive = 0;
      alarmSet = 0;

    } else {
      colorPick = (colorPick + 1) % TOTALCOLORS;

      Serial.printf("Color selected is: %d\n", colorPick);
    }
  selectPressed = 0; 
  }
}

void ui_configTime() {
  // Obtain RTC Data to put as initial values
  if (uiDoOnce == 0) {
    hms[0] = RTC.getHours();
    hms[1] = RTC.getMinutes();
    hms[2] = 0;
    uiDoOnce = 1;
  }

  // 0 - 24 (hours) and 0 - 59 (minutes) feild
  if (selectPressed) {
    if (selected_digit){
      selected_digit = 0;
    }else if (selected_digit == 0)
    {
      selected_digit = 1;
    }
    selectPressed = 0;
  }

  //Chnaging first and second digits (hours)
  if (selected_digit == 0) {
    if (plusPressed) {
      hms[selected_digit] = constrain(hms[selected_digit] + 1, 0, 24); // only setting 1 or zero for hour
      plusPressed = 0;
    }
    if (minusPressed) {
      hms[selected_digit] = constrain(hms[selected_digit] - 1, 0, 24); // only setting 1 or zero for hour
      minusPressed = 0;
    }
    timeDisplay(0, 0, 1);
  }

  //Chnaging second and third digits (minutes)
  if (selected_digit == 1) {
    if (plusPressed){
      hms[selected_digit] = constrain(hms[selected_digit] + 1, 0, 59); // only setting 1 or zero for hour
      plusPressed = 0;
    }
    if (minusPressed){
      hms[selected_digit] = constrain(hms[selected_digit] - 1, 0, 59); // only setting 1 or zero for hour
      minusPressed = 0;
    }
    timeDisplay(0, 0, 1);
  }

}

void ui_enableAlarm() {
  
  hms[0] = 0;
  hms[1] = (uint8_t)alarmSet;

  timeDisplay(0, 0, 1);

  if (plusPressed) {
    alarmSet = 1;

    #ifdef DEBUG
    Serial.printf("alarmSet is: %d\n", alarmSet);
    #endif

    RTC.enableAlarm1();

    hms[1] = alarmSet;
    plusPressed = 0;
  } else if (minusPressed) {
    alarmSet = 0;

    #ifdef DEBUG
    Serial.printf("alarmSet is: %d\n", alarmSet);
    #endif

    RTC.disableAlarm1();
    hms[1] = alarmSet;
    minusPressed = 0;
  } else if (selectPressed) {
    selectPressed = 0; 
  }
}

void ui_configAlarm() {

  // 0 - 24 (hours) and 0 - 59 (minutes) field
  if (selectPressed) {
    if (selected_digit){
      selected_digit = 0;
    } else if (selected_digit == 0)
    {
      selected_digit = 1;
    }
    selectPressed = 0;
  }

  //Chnaging first and second digits (hours)
  if (selected_digit == 0) {
    if (plusPressed) {
      rtcAlarm[selected_digit] = constrain(rtcAlarm[selected_digit] + 1, 0, 24); // only setting 1 or zero for hour
      plusPressed = 0;
    }
    if (minusPressed) {
      rtcAlarm[selected_digit] = constrain(rtcAlarm[selected_digit] - 1, 0, 24); // only setting 1 or zero for hour
      minusPressed = 0;
    }
    timeDisplay(0, 0, 2);
  }

  //Chnaging second and third digits (minutes)
  if (selected_digit == 1) {
    if (plusPressed){
      rtcAlarm[selected_digit] = constrain(rtcAlarm[selected_digit] + 1, 0, 59); // only setting 1 or zero for hour
      plusPressed = 0;
    }
    if (minusPressed){
      rtcAlarm[selected_digit] = constrain(rtcAlarm[selected_digit] - 1, 0, 59); // only setting 1 or zero for hour
      minusPressed = 0;
    }
    timeDisplay(0, 0, 2);
  }
}

void ui_selectUTC() {

  hms[0] = utcOffsetArray[utcOffset][0];
  hms[1] = utcOffsetArray[utcOffset][1];


  if (UTCToSeconds(utcOffsetArray[utcOffset]) < 0) {
    timeDisplay(0, 255, 1);
  } else if (UTCToSeconds(utcOffsetArray[utcOffset]) > 0) {
    timeDisplay(85, 255, 1);
  } else {
    timeDisplay(0, 0, 1);
  }
  

  if (plusPressed) {
    utcOffset = constrain(utcOffset + 1, 0, 37);

    #ifdef DEBUG
    Serial.printf("UTC Array Selection: %d\n", utcOffset);
    #endif

    plusPressed = 0;
  } else if (minusPressed) {
    utcOffset = constrain(utcOffset - 1, 0, 37);

    #ifdef DEBUG
    Serial.printf("UTC Array Selection: %d\n", utcOffset);
    #endif

    minusPressed = 0;
  } else if (selectPressed) {
    selectPressed = 0; 
  }

}

void ui_enableNTPUpdate() {

  hms[0] = 0;
  hms[1] = (uint8_t)timeUpdate;

  timeDisplay(0, 0, 1);

  if (plusPressed) {
    timeUpdate = 1;

    #ifdef DEBUG
    Serial.printf("timeUpdate is: %d\n", timeUpdate);
    #endif

    nvsObj.begin("config", false);
    nvsObj.putBool("timeUpdate", timeUpdate);
    nvsObj.end();

    hms[1] = timeUpdate;
    plusPressed = 0;
  } else if (minusPressed) {
    timeUpdate = 0;

    #ifdef DEBUG
    Serial.printf("timeUpdate is: %d\n", timeUpdate);
    #endif

    nvsObj.begin("config", false);
    nvsObj.putBool("timeUpdate", timeUpdate);
    nvsObj.end();

    hms[1] = timeUpdate;
    minusPressed = 0;
  } else if (selectPressed) {
    selectPressed = 0; 
  }
}

void genSetup() {
  #ifdef DEBUG
  Serial.printf("Starting general setup.\n");
  #endif
  
  // Set pins for I2C
  Wire.setPins(SDAPIN, SCLPIN);

  // Setup OE for logic level shifter
  pinMode(TXSPIN, OUTPUT);
  digitalWrite(TXSPIN, HIGH);

  // Setup PIR pin
  pinMode(PIRPIN, INPUT_PULLDOWN);

  // Setup Buzzer Pin
  pinMode(BUZZPIN, OUTPUT);
  digitalWrite(BUZZPIN, LOW);

  // Setup RTC Interrupt Pin
  pinMode(DSINT, INPUT);
  attachInterrupt(digitalPinToInterrupt(DSINT), alarmISR, FALLING);

  // Initialize Buttons
  pinMode(MODEBUTTON, INPUT_PULLUP);
  pinMode(DOWNBUTTON, INPUT_PULLUP);
  pinMode(UPBUTTON, INPUT_PULLUP);
  pinMode(SWITCHBUTTON, INPUT_PULLUP);

  //** Fetch parameters from non-volatile memory
  
  if (!nvsObj.begin("config", false)) {
    Serial.printf("nvsObj object failed to start, looping...\n");
    while(1);
  }

  if (nvsObj.isKey("configInit") != 1 || resetNVS == 1) {
    #ifdef DEBUG
    Serial.printf("Config namespace has not be initialized, creating new namespace.\n");
    #endif

    // Load config namespace with parameters
    nvsObj.putBool("timeUpdate", 1);
    nvsObj.putInt("lampBrightVal", 50);
    nvsObj.putInt("utcOffset", 14);
    nvsObj.putInt("alarmHr", 0);
    nvsObj.putInt("alarmMin", 0);

    nvsObj.putBool("configInit", 1);        // Set "already init" status for namespace
  }

  #ifdef DEBUG
  Serial.printf("Config namespace has already been created, obtaining values.\n");
  #endif

  // Obtain values from namespace
  timeUpdate = nvsObj.getBool("timeUpdate");
  lampBrightVal = nvsObj.getInt("lampBrightVal");
  utcOffset = nvsObj.getLong("utcOffset");
  rtcAlarm[0] = nvsObj.getInt("alarmHr");
  rtcAlarm[1] = nvsObj.getInt("alarmMin");

  nvsObj.end();

  #ifdef DEBUG
  Serial.printf("timeUpdate obtained is: %d\n", timeUpdate);
  Serial.printf("lampBrightVal obtained is: %d\n", lampBrightVal);
  Serial.printf("utcOffset obtained is: %d\n", utcOffset);
  Serial.printf("rtcAlarm[0] obtained is: %d\n", rtcAlarm[0]);
  Serial.printf("rtcAlarm[1] obtained is: %d\n", rtcAlarm[1]);
  Serial.printf("alarmSet obtained is: %d\n", alarmSet);
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
  if (!RTC.isConnected()) {
    Serial.printf("RTC object failed to start, looping...\n");
    while(1);
  }
  RTC.setHourMode(CLOCK_H24);
  RTC.enableAlarmPin();
  alarmSet = RTC.isAlarm1Enabled();

  #ifdef DEBUG
  Serial.printf("alarmSet obtained is: %d\n", alarmSet);
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
    if (millis() - lastBlink > blinkDelay) {
      if (blinkToggle == 0) {
        digiLEDS[3][0].setHSV(0, 0, dispBrightVal);
        blinkToggle = 1;
      } else {
        digiLEDS[3][0].setHSV(0, 0, 0);
        blinkToggle = 0;
      }
      #ifdef DEBUG
      Serial.printf(".");
      #endif

      FastLED.show();
      lastBlink = millis();
    }
  }

  blinkToggle = 1;

  FastLED.clear();
  FastLED.show();

  #ifdef DEBUG
  Serial.printf("\nWifi successfully connected!\n");
  #endif

  //** Start NTP Communication to fetch current time
  timeClient.begin();
  timeClient.setTimeOffset(UTCToSeconds(utcOffsetArray[utcOffset]));     // Set time offset from saved parameters

  #ifdef DEBUG
  Serial.printf("timeClient started!\n");
  Serial.printf("Time offset is: %d:%d\n", utcOffsetArray[utcOffset][0], utcOffsetArray[utcOffset][1]);
  Serial.printf("Time offset in seconds is: %ld\n", UTCToSeconds(utcOffsetArray[utcOffset]));
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

void timeDisplay(uint8_t hue, uint8_t sat, uint8_t mode) {
  // Mode 0: Normal time fetch from RTC
  // Mode 1: Custom Numbers on display from HMS array

  // Parse hour and minute data from RTC
  uint8_t hour;
  uint8_t min;
  uint8_t partH[2];
  uint8_t partM[2];

  if (mode == 0) {
    hour = RTC.getHours();
    min = RTC.getMinutes();
  } else if (mode == 1) {
    hour = hms[0];
    min = hms[1];
  } else if (mode == 2) {
    hour = rtcAlarm[0];
    min = rtcAlarm[1];
  }

  //partH[2] = {(uint8_t)(hour/10), (uint8_t)(hour%10)};
  //partM[2] = {(uint8_t)(min/10), (uint8_t)(min%10)};;

  partH[0] = (uint8_t)(hour/10);
  partH[1] = (uint8_t)(hour%10);

  partM[0] = (uint8_t)(min/10);
  partM[1] = (uint8_t)(min%10);

  // Set LEDs on for 7 segment displays
  for (uint8_t i = 0; i < 2; i++) {
    setDigitLED(partH[i], hue, sat, dispBrightVal, i);
    setDigitLED(partM[i], hue, sat, dispBrightVal, i + 2);
  }

}

/*  Helper Functions  */

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
  // Configure Mode Color
  if (type == 0) {              
    if (alarmSet == 1 && modeCounter == 0) {
      modeLEDS[0].setHSV(170, 255, val);
    } else {
      modeLEDS[0].setHSV(hue, sat, val);
    }
    modeLEDS[1].setHSV(hue, sat, val);
  } else if (type == 1) {       // Configure Lamp Color
    fill_solid(lampLEDS, NLAMPLEDS, CHSV(hue, sat, val));
  } 
}

// UTCToSeconds() - used to convert utcOffset array data into signed seconds
long UTCToSeconds(short utcOffsetTriplet[]) {
    short hours = utcOffsetTriplet[0];
    short minutes = utcOffsetTriplet[1];
    short is_pos = utcOffsetTriplet[2];
    if (is_pos == 0) { //0 means negative 1 means 0 and 2 means positive
      return -1 * ((hours * 3600) + (minutes * 60));
    }
    return (hours * 3600) + (minutes * 60);
}

/*  ISR Functions  */

void alarmISR() {
  digitalWrite(BUZZPIN, HIGH);
  alarmActive = 1;
}

void isrPIR() {
  lastPIRTrigger = millis();

  if (lampEN == 0) {
    lampEN = 1;
  } else {
    lampEN = 0;
  }

  detachInterrupt(digitalPinToInterrupt(PIRPIN));
}

/*  Debugging Functions   */
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

void testDigits() {
  int i = 0;
  int j = 0;

  for (i = 0; i < 10; i++) {
    for (j = 0; j < 4; j++) {
      setDigitLED(i, 0, 0, 50, j);
      setAuxLED(0, 0, 0, 50);
      setAuxLED(1, 0, 0, 50);
      FastLED.show();
    }
  }
  
  delay(5000);
}