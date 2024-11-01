#include <I2C_RTC.h>
#include <Wire.h>

static DS3231 RTC;

uint8_t hms[] = {23, 21, 55};
uint8_t alrm[] = {23, 22, 0};

void setup() {
  Serial.begin(115200);
  RTC.begin();
  RTC.setHourMode(CLOCK_H24);
  RTC.setTime(hms[0],hms[1],hms[2]);
  RTC.startClock();
  RTC.enableAlarmPin();
  RTC.setAlarm1(alrm[0], alrm[1], alrm[2]);
  RTC.enableAlarm1();
  printf("Clock has started.\n");
}

void loop() {
  if(RTC.isRunning()) {
    printf("Time: %d:%d:%d\n\n", RTC.getHours(), RTC.getMinutes(), RTC.getSeconds());
    delay(500);
  }
}
