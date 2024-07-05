#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include "RTCService.h"
#include "LCDService.h"
#include "AppDebug.h"
#include "WifiService.h"

class TimeServiceClass(){
  public:
    TimeServiceClass();
    ~TimeServiceClass();
    void setupDateTime();
    void updateDateTime();
    String getCurrentDateTime();
}

extern TimeServiceClass TimeService;


#endif