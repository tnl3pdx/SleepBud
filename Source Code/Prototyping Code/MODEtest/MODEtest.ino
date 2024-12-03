// This is for the modes and functionaity test
// Ameer Melli 12/2/24
#include <FastLED.h>

// Pin Definitions
const byte modeButton = 21;  // Button 0: Mode button
const byte minusButton = 14; // Button 1: Minus button
const byte plusButton = 32;  // Button 2: Plus button
const byte selectButton = 15; // Button 3: Select button

// LED Definitions
#define NUM_LEDS 4
#define LED_PIN 5
CRGB leds[NUM_LEDS];

// Global Variables
volatile bool modeButtonPressed = false;
volatile bool minusButtonPressed = false;
volatile bool plusButtonPressed = false;
volatile bool selectButtonPressed = false;

int modeCounter = 0; // Modes: 0 - 3
bool alarmSet = false; // Alarm state (off/on)
bool alarmActive = false; // Alarm currently ringing
uint8_t brightness = 50; // Default brightness (mode 0)
uint8_t alarmTime[4] = {0, 0, 0, 0}; // Alarm time (HH:MM)
uint8_t utcOffset[4] = {0, 0, 0, 0}; // UTC Offset (HH:MM)
uint8_t currentField = 0; // Field selector for modes 1 and 3

// Timer for Debounce
unsigned long lastPressTime = 0; // Last button press time
const unsigned long debounceInterval = 600; // 200ms debounce interval

// Function Prototypes
void ISR_modeButton();
void ISR_minusButton();
void ISR_plusButton();
void ISR_selectButton();
void updateModeIndicator();
void handleMode0();
void handleMode1();
void handleMode2();
void handleMode3();

void setup() {
  Serial.begin(115200);

  // Initialize Buttons
  pinMode(modeButton, INPUT_PULLUP);
  pinMode(minusButton, INPUT_PULLUP);
  pinMode(plusButton, INPUT_PULLUP);
  pinMode(selectButton, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(modeButton), ISR_modeButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(minusButton), ISR_minusButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(plusButton), ISR_plusButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(selectButton), ISR_selectButton, FALLING);

  // Initialize LEDs
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  Serial.println("Setup complete");
}

void loop() {
  // Handle mode button press
  if (modeButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    modeButtonPressed = false;
    modeCounter = (modeCounter + 1) % 4; // Cycle through modes 0-3
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
  }
}

// ISR Functions
void ISR_modeButton() { modeButtonPressed = true; }
void ISR_minusButton() { minusButtonPressed = true; }
void ISR_plusButton() { plusButtonPressed = true; }
void ISR_selectButton() { selectButtonPressed = true; }

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
  leds[0] = color; // Use the first LED to indicate the mode
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
    leds[1] = alarmSet ? CRGB::White : CRGB::Black; // Indicate alarm set with a white LED
    FastLED.show();
  }
}

void handleMode3() {
  if (minusButtonPressed && millis() - lastPressTime > debounceInterval) {
    lastPressTime = millis();
    minusButtonPressed = false;
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

// Adjust Time Field Helper Function
void adjustTimeField(uint8_t* timeArray, bool increase) {
  if (currentField == 0) timeArray[currentField] = increase ? (timeArray[currentField] == 1 ? 0 : 1) : (timeArray[currentField] == 0 ? 1 : 0);
  else if (currentField == 2) timeArray[currentField] = increase ? (timeArray[currentField] + 1) % 6 : (timeArray[currentField] == 0 ? 5 : timeArray[currentField] - 1);
  else timeArray[currentField] = increase ? (timeArray[currentField] + 1) % 10 : (timeArray[currentField] == 0 ? 9 : timeArray[currentField] - 1);
  Serial.printf("Time adjusted: %02d:%02d\n", timeArray[0] * 10 + timeArray[1], timeArray[2] * 10 + timeArray[3]);
}