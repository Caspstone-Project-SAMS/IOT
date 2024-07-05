#include "RTCService.h"

RTC_DS1307 rtc;
bool haveRTC = false;
static unsigned long lastUpdate = 0;
static const unsigned long intervalTime = 2073600000; //24days

RTCServiceClass::RTCServiceClass(){
}

RTCServiceClass::~RTCServiceClass(){
}

bool RTCServiceClass::connectDS1307(){
  int reconnectTimes = 0;
  while(reconnectTimes < 4){
    delay(200);
    Serial.println("Connect RTC: " + String(reconnectTimes + 1));
    if(rtc.begin()){
      haveRTC = true;
      return true;
    }
    reconnectTimes++;
  }
  return false;
}

bool RTCServiceClass::setupDS1307DateTime(){
  if(WifiService.checkWifi() != CONNECT_OK){
    Serial.println("Wifi not connected");
    return false;
  }
  if(!haveRTC){
    Serial.println("RTC not connected");
    return false;
  }

  
}

bool RTCServiceClass::getDS1307DateTime(DateTime& dateTime){

}