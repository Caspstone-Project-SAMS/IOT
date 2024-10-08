#ifndef FINGERPRINT_SENSOR_SERVICE_H_
#define FINGERPRINT_SENSOR_SERVICE_H_

#include <Adafruit_Fingerprint.h>
#include "AppDebug.h"
#include <string>
#include <Arduino_JSON.h>

#define FINGER_RX 0 //D3 in ESP8266 is GPIO0
#define FINGER_TX 2 //D4 is GPIO2
#define TEMPLATE_BUF_SIZE 512

class FingerprintSensorClass
{
  public:
    FingerprintSensorClass();
    ~FingerprintSensorClass();
    bool connectFingerprintSensor();
    uint8_t scanFinger();
    uint8_t image2Tz();
    uint8_t seachFinger();
    uint16_t getFingerID();
    //String getFingerprintTemplate();
    bool uploadFingerprintTemplate(const char* fingerData, uint16_t& storeModelID);
    uint8_t getFingerTemplateCount();
    uint8_t deleteFingerprint(uint8_t id);
    void emptyDatabase();
  private:
    uint8_t convert_hex_to_binary(std::string& hexString);
};

extern FingerprintSensorClass FINGERPSensor;

#endif