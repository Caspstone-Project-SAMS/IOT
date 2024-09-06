#ifndef RTC_SERVICE_H
#define RTC_SERVICE_H

#include <RTClib.h>
#include "LCDService.h"
#include "AppDebug.h"
#include "WifiService.h"

class RTCServiceClass{
  public:
    RTCServiceClass();
    ~RTCServiceClass();
    bool connectDS1307();
    bool setupDS1307DateTime(DateTime dateTime);
    bool getDS1307DateTime(DateTime& dateTime);
};

extern RTCServiceClass RTCService;

#endif