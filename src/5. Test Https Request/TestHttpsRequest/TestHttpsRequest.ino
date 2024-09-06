/**
   BasicHTTPSClient.ino

    Created on: 20.08.2018

*/

//#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "certs.h"

#ifndef STASSID
#define STASSID "Nhim"
#define STAPSK "1357924680"
#endif

//ESP8266WiFiMulti WiFiMulti;

const char fingerprint_sams_com [] PROGMEM = "81:81:F7:68:CB:64:60:41:D7:37:11:4D:DE:53:B2:6C:2D:FC:9D:46";

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println("OKKKKKKKkkk");

  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  //WiFiMulti.addAP(STASSID, STAPSK);
  WiFi.begin(STASSID, STAPSK);
  Serial.println("setup() done connecting to ssid '" STASSID "'");
}

void loop() {
  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

    client->setFingerprint(fingerprint_sams_com);
    // Or, if you happy to ignore the SSL certificate, then use the following line instead:
    // client->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "34.81.223.233", 443, "/api/Hello")) {  // HTTPS

      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  }

  Serial.println("Wait 10s before next round...");
  delay(10000);
}