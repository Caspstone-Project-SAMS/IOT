// Date and time functions using a DS1307 RTC connected via I2C and Wire lib
#include <RTClib.h>


// UNCHANGABLE PARAMATERS
#define SUNDAY    0
#define MONDAY    1
#define TUESDAY   2
#define WEDNESDAY 3
#define THURSDAY  4
#define FRIDAY    5
#define SATURDAY  6


// event on Monday, from 13:50 to 14:10
uint8_t WEEKLY_EVENT_DAY      = MONDAY;
uint8_t WEEKLY_EVENT_START_HH = 13; // event start time: hour
uint8_t WEEKLY_EVENT_START_MM = 50; // event start time: minute
uint8_t WEEKLY_EVENT_END_HH   = 14; // event end time: hour
uint8_t WEEKLY_EVENT_END_MM   = 10; // event end time: minute
const int sensorPin = A0;
float sensorValue;
float voltageOut;


float temperatureC;
RTC_DS1307 rtc;


char daysOfTheWeek[7][12] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};


void setup () {
  Serial.begin(9600);


  // SETUP RTC MODULE
  bool ok = false;
  while(!ok){
    if (! rtc.begin()) {
      Serial.println("Couldn't find RTC");
      delay(500);
    }
    else{
      ok = true;
    }
  }

  // sets the RTC to the date & time on PC this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));


  // sets the RTC with an explicit date & time, for example to set
  // January 21, 2021 at 3am you would call:
  // rtc.adjust(DateTime(2021, 1, 21, 3, 0, 0));


}


void loop () {
  sensorValue = analogRead(sensorPin);
  voltageOut = (sensorValue * 5000) / 1024;


  // calculate temperature for LM35 (LM35DZ)


  temperatureC = voltageOut / 10;
  DateTime now = rtc.now();




  printTime(now);
}


void printTime(DateTime time) {
  Serial.print("TIME: ");
  Serial.print(time.year(), DEC);
  Serial.print('/');
  Serial.print(time.month(), DEC);
  Serial.print('/');
  Serial.print(time.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[time.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(time.hour(), DEC);
  Serial.print(':');
  Serial.print(time.minute(), DEC);
  Serial.print(':');
  Serial.println(time.second(), DEC);


  Serial.print("temperature: ");
  Serial.println(temperatureC);
  delay(5000);
}


