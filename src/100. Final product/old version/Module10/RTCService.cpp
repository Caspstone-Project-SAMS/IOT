#include "RTCService.h"

RTC_DS1307 rtc;
bool haveRTC = false;
static unsigned long lastUpdate = 0;
static const unsigned long intervalTime = 2073600000; //24days

// DateTime format
// const char* dateFormat = "%Y-%m-%d";
// const char* timeFormat = "%H:%M:%S";
// const char* dateTimeFormat = "%Y-%m-%d %H:%M:%S";

RTCServiceClass::RTCServiceClass(){
}

RTCServiceClass::~RTCServiceClass(){
}

bool RTCServiceClass::connectDS1307(){
  int reconnectTimes = 0;
  while(reconnectTimes < 4){
    delay(200);
    //Serial.println("Connect RTC: " + String(reconnectTimes + 1));
    if(rtc.begin()){
      haveRTC = true;
      return true;
    }
    reconnectTimes++;
  }
  return false;
}

bool RTCServiceClass::setupDS1307DateTime(DateTime& dateTime){
  if(haveRTC){
    rtc.adjust(dateTime);
    return true;
  }
  return false;
}

bool RTCServiceClass::getDS1307DateTime(DateTime& dateTime){
  if(haveRTC){
    dateTime = rtc.now();
    return true;
  }
  return false;
}

RTCServiceClass RTCService;

