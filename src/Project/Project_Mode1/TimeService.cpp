#include "TimeService.h"

#include <WiFiUdp.h>
#include <NTPClient.h>               
#include <TimeLib.h>

// DateTime format
const char* dateFormat = "%Y-%m-%d";
const char* timeFormat = "%H:%M:%S";
const char* dateTimeFormat = "%Y-%m-%d %H:%M:%S";

// DateTime information, NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
char Time[] = "TIME:00:00:00";
char Date[] = "DATE:00-00-2000";
char CDateTime[] = "0000-00-00T00:00:00";
byte second_, minute_, hour_, day_, month_;
int year_;

TimeServiceClass::TimeServiceClass(){
}

TimeServiceClass::~TimeServiceClass(){
}

void TimeServiceClass::setupDateTime(){

}

void TimeServiceClass::updateDateTime(){

}

String TimeServiceClass::getCurrentDateTime(){
  
}

