#include <ESP8266HTTPClient.h>
#include <ArduinoWebsockets.h>
#include <WiFiClientSecureBearSSL.h>

#include <Arduino_JSON.h>
#include <string>

#include <WiFiUdp.h>
#include <NTPClient.h>               
#include <TimeLib.h>

#include <ESP8266WebServer.h>

#include <Arduino.h>
#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WiFiClient.h"
#include "IPAddress.h"
#include "ESP8266WiFiType.h"
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Crypto.h"
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <RTClib.h>

using namespace websockets;

//std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
//std::unique_ptr<BearSSL::WiFiClientSecure> wsClient(new BearSSL::WiFiClientSecure);
const char fingerprint_sams_com [] PROGMEM = "DC:19:DE:0B:D0:79:11:E4:BE:75:BE:4F:BD:FC:B9:DE:D9:0A:E7:4F";
WebsocketsClient websocketClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
WiFiClient wifiClient;
HTTPClient http;
String key = "I3ZbMRjf0lfag1uSJzuDKtz8J";

#ifndef STASSID
#define STASSID "Nhim"
#define STAPSK "1357924680"
#endif


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  delay(10000);
  Serial.println("setup() done connecting to ssid '" STASSID "'");

  if ((WiFi.status() == WL_CONNECTED)) {

    Serial.println("Test before websocket");
    testBeforeWebsocket();

    testBeforeWebsocket();

    testBeforeWebsocket();

    connectWebSocket();

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    bool checkTest = client->setFingerprint(fingerprint_sams_com);

    HTTPClient https;

    if(checkTest){
      Serial.println("Check ok");
    }
    else {
      Serial.println("Check not ok");
    }

    https.begin(*client, "https://sams-project.com/api/Hello");
    int httpCode = https.GET();
    if(httpCode > 0){
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    String payloadData = https.getString();
    
    Serial.println(payloadData);
    https.end();
  }
}

void loop() {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    bool checkTest = client->setFingerprint(fingerprint_sams_com);

    HTTPClient https;

    if(websocketClient.available()) {
      websocketClient.poll();
    }

    https.begin(*client, "https://sams-project.com/api/Student/get-students-by-classId-v2?startPage=1&endPage=1&quantity=2&isModule=true");
    int httpCode = https.GET();
    if(httpCode > 0){
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    String payloadData = https.getString();
    
    Serial.println(payloadData);
    https.end();

    if(websocketClient.available()) {
      websocketClient.poll();
    }
    delay(3000);
}

bool connectWebSocket() {

  // run callback when events are occuring
  websocketClient.onEvent(onEventsCallback);

  // run callback when messages are received
  websocketClient.onMessage([&](WebsocketsMessage message) {
    //Serial.println(static_cast<std::underlying_type<MessageType>::type>(message.type()));

    if(message.isText()){
      // Get data from here
      //ECHOLN("Get message at websocket");
      const char* data = message.c_str();
      JSONVar message = JSON.parse(data);
      String event = message["Event"];
      JSONVar receiveData = message["Data"];
    }
    else if(message.isBinary()){
      const char* data = message.c_str();
      if(strcmp(data, "ping") == 0){
        Serial.println("Receive custom ping, lets send pong");
        //lastReceiveCustomPing = millis();
        String sendMessage = "pong";

        websocketClient.sendBinary(sendMessage.c_str(), sendMessage.length());
        delay(50);
        websocketClient.sendBinary(sendMessage.c_str(), sendMessage.length());
      }
    }
    else{
      Serial.println("Does not found anything");
    }
  });

  // bool check = websocketClient.connect("wss://sams-project.com/ws/module?key=xSJ6nDkyRcjYn81hPy5H9fRZg");
  // if(!check){
  //   Serial.println("Connect websocket failed");
    
  // }

  bool check = websocketClient.connect("ws://sams-project.com:8080/ws/module?key=xSJ6nDkyRcjYn81hPy5H9fRZg");
  return check;
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("[Main][WebsocketEvent] Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("[Main][WebsocketEvent] Connnection Closed");
        CloseReason reason = websocketClient.getCloseReason();
        // Check the close reason and print it
            switch (reason) {
                case CloseReason_None:
                    Serial.println("Close Reason: None");
                    break;
                case CloseReason_NormalClosure:
                    Serial.println("Close Reason: Normal Closure");
                    break;
                case CloseReason_GoingAway:
                    Serial.println("Close Reason: Going Away");
                    break;
                case CloseReason_ProtocolError:
                    Serial.println("Close Reason: Protocol Error");
                    break;
                case CloseReason_UnsupportedData:
                    Serial.println("Close Reason: Unsupported Data");
                    break;
                case CloseReason_NoStatusRcvd:
                    Serial.println("Close Reason: No Status Received");
                    break;
                case CloseReason_AbnormalClosure:
                    Serial.println("Close Reason: Abnormal Closure");
                    break;
                case CloseReason_InvalidPayloadData:
                    Serial.println("Close Reason: Invalid Payload Data");
                    break;
                case CloseReason_PolicyViolation:
                    Serial.println("Close Reason: Policy Violation");
                    break;
                case CloseReason_MessageTooBig:
                    Serial.println("Close Reason: Message Too Big");
                    break;
                case CloseReason_InternalServerError:
                    Serial.println("Close Reason: Internal Server Error");
                    break;
                default:
                    Serial.println("Close Reason: Unknown");
                    break;
            }
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("[Main][WebsocketEvent] Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("[Main][WebsocketEvent] Got a Pong!");
    }
}


void testBeforeWebsocket(){
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    bool checkTest = client->setFingerprint(fingerprint_sams_com);

    HTTPClient https;

    if(checkTest){
      Serial.println("Check ok");
    }
    else {
      Serial.println("Check not ok");
    }

    https.begin(*client, "https://sams-project.com/api/Hello");
    int httpCode = https.GET();
    if(httpCode > 0){
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    String payloadData = https.getString();
    
    Serial.println(payloadData);
    https.end();
}

void testBeforeWebsocket1(){
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  delay(1000);
    bool checkTest = client->setFingerprint(fingerprint_sams_com);
    delay(1000);

    HTTPClient https;
    delay(1000);

    if(checkTest){
      Serial.println("Check ok");
    }
    else {
      Serial.println("Check not ok");
    }

    https.begin(*client, "https://sams-project.com/api/Class");
    int httpCode = https.GET();
    if(httpCode > 0){
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    String payloadData = https.getString();
    
    Serial.println(payloadData);
    https.end();
}

void testBeforeWebsocket2(){
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    bool checkTest = client->setFingerprint(fingerprint_sams_com);

    HTTPClient https;

    if(checkTest){
      Serial.println("Check ok");
    }
    else {
      Serial.println("Check not ok");
    }

    https.begin(*client, "sams-project.com", 443, "/api/Class");
    int httpCode = https.GET();
    if(httpCode > 0){
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    String payloadData = https.getString();
    
    Serial.println(payloadData);
    https.end();
}
