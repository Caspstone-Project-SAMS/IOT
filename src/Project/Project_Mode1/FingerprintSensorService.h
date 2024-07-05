#ifndef FINGERPRINT_SENSOR_SERVICE_H_
#define FINGERPRINT_SENSOR_SERVICE_H_

#include <Adafruit_Fingerprint.h>
#include "LCDService.h"
#include "AppDebug.h"

#define FINGER_RX 0 //D3 in ESP8266 is GPIO0
#define FINGER_TX 2 //D4 is GPIO2
#define TEMPLATE_BUF_SIZE 512

class FingerprintSensorClass
{
  public:
    FingerprintSensorClass();
    ~FingerprintSensorClass();
    bool connectFingerprintSensor();
    uint8_t getFingerprintEnroll();
    String getFingerprintTemplate();
    uint8_t deleteFingerprint(uint8_t id);
};

extern FingerprintSensorClass FINGERPSensor;

#endif