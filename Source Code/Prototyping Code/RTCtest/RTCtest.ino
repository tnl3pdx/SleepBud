// RTC Clock Library & Dependencies
#include <I2C_RTC.h>
#include <Wire.h>

// ESP32 Wifi and NTP Libraries
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Misc Header files
#include <creds.h>

#define DEBUG


// WiFi Credentials
const char *ssid = WIFI_SSID;
const char *pswrd = WIFI_PASSWORD;


static DS3231 RTC;
WiFiUDP ntpPort;
NTPClient timeClient(ntpPort, "time.nist.gov");

uint8_t hms[3];
uint8_t alrm[] = {23, 22, 0};
int maxTries = 5;

void setup() {
  Serial.begin(115200);

  // Start and Configure RTC Clock
  RTC.begin();
  RTC.setHourMode(CLOCK_H24);
  RTC.enableAlarmPin();

  // WIFI Connection Setup

  #ifdef DEBUG
  Serial.printf("Attempting WIFI connection: connecting to %s\n", ssid);
  #endif

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

  // Start NTP Communication to fetch current time
  timeClient.begin();

  #ifdef DEBUG
  Serial.printf("timeClient started!\n");
  Serial.printf("Test time fetch from time.nist.gov\n");
  #endif

  timeClient.update();

  #ifdef DEBUG
  Serial.printf("Data received from NTP server: %s\n", timeClient.getFormattedTime());
  #endif

  // Parse string data from NTP to store in RTC as integers
  String formatTime = timeClient.getFormattedTime();
  char* tkn;
  int pos = 0;

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
  
  // Setup RTC clock with parsed time data and set alarm for testing
  RTC.setTime(hms[0], hms[1], hms[2]);
  RTC.startClock();
  
  RTC.setAlarm1(hms[0], hms[1], hms[2] + 5);
  RTC.enableAlarm1();
  
  #ifdef DEBUG
  Serial.printf("Configuration done, ending UDP connection to time server.\n");
  #endif

  timeClient.end();
}

void loop() {
  if(RTC.isRunning()) {
    Serial.printf("Current Time: %d:%d:%d\n\n", RTC.getHours(), RTC.getMinutes(), RTC.getSeconds());
    delay(10);
  }
}
