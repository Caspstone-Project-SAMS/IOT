#include <Adafruit_Fingerprint.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoWebsockets.h>

#include <Arduino_JSON.h>
#include <string>

#include <RTClib.h>

#include <WiFiUdp.h>
#include <NTPClient.h>               
#include <TimeLib.h>

#include <ESP8266WebServer.h>
#include "EEPRomService.h"
#include "AppDebug.h"
#include "AppConfig.h"
#include "WifiService.h"
#include "HttpServerH.h"
#include "LCDService.h"


using namespace websockets;

//=========================Class definition=========================
class FingerData{
  public:
    std::string fingerprintTemplate;
    std::string Content;

    FingerData(std::string _fingerprintTemplate, std::string _Content){
      fingerprintTemplate = _fingerprintTemplate;
      Content = _Content;
    }
};
//===================================================================



//=========================Macro define=============================
#define SERVER_IP "34.81.224.196"
#define WEBSOCKETS_SERVER_HOST "34.81.224.196"
#define WEBSOCKETS_SERVER_PORT 80
#define WEBSOCKETS_PROTOCOL "ws"

#ifndef STASSID
#define STASSID "Nhim" //"FPTU_Library" //Nhim
#define STAPSK "1357924680" //"12345678" //1357924680
// #define STASSID "Nhim"
// #define STAPSK "1357924680"
// #define STASSID "Garage Coffee"
// #define STAPSK "garageopen24h"
#endif

#define Finger_Rx 0 //D3 in ESP8266 is GPIO0
#define Finger_Tx 2 //D4 is GPIO2
//====================================================================



//===========================Memory variables declaration here===========================
// DateTime format
const char* dateFormat = "%Y-%m-%d";
const char* timeFormat = "%H:%M:%S";
const char* dateTimeFormat = "%Y-%m-%d %H:%M:%S";


// Fingerprint sesnor
SoftwareSerial mySerial(Finger_Rx, Finger_Tx);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
int template_buf_size = 512;
//==================================


// Websocket
WebsocketsClient websocketClient;
//==================================


// DateTime information, NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
char Time[] = "TIME:00:00:00";
char Date[] = "DATE:00-00-2000 ";
char CDateTime[] = "0000-00-00T00:00:00";
byte second_, minute_, hour_, day_, month_;
int year_;
//================================


// DS1307 real-time
RTC_DS1307 rtc;
bool haveRTC = false;
static unsigned long lastUpdate = 0;
static const unsigned long intervalTime = 2073600000; //24days
//================================


// Http Client
WiFiClient wifiClient;
HTTPClient http;
//================================


// Is registering fingerprint template?
bool isRegisteringFingerprintTemplate = false;
String content;
//================================


// Mode Management
ESP8266WebServer* server = nullptr;
int appMode = NORMAL_MODE;
unsigned long settingTimeout = 0;
unsigned long toggleTimeout = 0;
//================================


// Button
const int BUTTON_PIN = D5; // The ESP8266 pin D5 connected to button
int prev_button_state = LOW; // the previous state from the input pin
int button_state;    // the current reading from the input pin
// LOW = Not pushed - HIGH = Pushed
//================================


//========================Set up code==================================

void connectButton(){
  pinMode(BUTTON_PIN, INPUT_PULLDOWN_16);
}

void connectFingerprintSensor() {
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment");
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);
}

void conenctWifi() {
  delay(100);
  Serial.println("\n\n\n================================");
  Serial.println("Connecting to Wifi");

  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected! IP address: "); Serial.println(WiFi.localIP());
}

void connectDS1307() {
  bool connectToRTC = false;
  while(!connectToRTC){
    delay(500);
    if(rtc.begin()){
      connectToRTC = true;
      haveRTC = true;
    }
  }
}

void connectWebSocket() {

  // run callback when events are occuring
  websocketClient.onEvent(onEventsCallback);

  // run callback when messages are received
  websocketClient.onMessage([&](WebsocketsMessage message) {
    Serial.print("Got Message: ");
    //Serial.println(static_cast<std::underlying_type<MessageType>::type>(message.type()));

    if(message.isText()){
      // Get data from here
      const char* data = message.c_str();
      JSONVar message = JSON.parse(data);
      String event = message["Event"];
      String sendData = message["Data"];
      if(event == "RegisterFingerprint"){
        isRegisteringFingerprintTemplate = true;
        content = sendData;
      }
    }
  });

  bool connected = websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws?isRegisterModule=true");
  if(!connected){
    while(1){
      delay(500);
    }
  }
  printTextLCD("Connected", 1);
}
//===================================================================

void setup() {
  Serial.begin(9600);
  delay(100);

  //Connect button
  connectButton();

  // Connect I2C LCD
  connectLCD();
  delay(2000);

  // Connect to DS1307 (RTC object)
  clearLCD();
  printTextLCD("Connect RTC", 0);
  connectDS1307();
  delay(2000);

  // Connect to R308 fingerprint sensor
  clearLCD();
  printTextLCD("Connect Fingerprint Sensor", 0);
  connectFingerprintSensor();
  delay(2000);

  // Connet to wifi
  clearLCD();
  printTextLCD("Connect Wifi", 0);
  conenctWifi();
  delay(2000);

  // Connect to ntp server;
  clearLCD();
  printTextLCD("Connect NTP server", 0);
  timeClient.begin();
  delay(2000);

  // Connect to websockets
  clearLCD();
  printTextLCD("Connect Websockets", 0);
  connectWebSocket();
  delay(2000);

  // Prepare data
  clearLCD();
  printTextLCD("Prepare data", 0);
  delay(1000);
  // Setup datetime for module
  clearLCD();
  printTextLCD("Setup DateTime", 0);
  delay(1000);
  if(haveRTC){
    clearLCD();
    printTextLCD("RTC found", 0);
    printTextLCD("Setup RTC", 0);
    setupDS1307DateTime();
  }
  else{
    clearLCD();
    printTextLCD("RTC not found", 0);
    while(1);
  }

  //Reset data
  resetData();
  
  clearLCD();
  printTextLCD("Setup done!!!", 0);
  delay(2000);
}

void loop() {
  if(appMode == NORMAL_MODE) {
    handleNormalMode();
  }
  else {
    handleSetUpMode();
  }
  checkModeReset();
}

void handleNormalMode(){
  int checkUpdateDateTimeStatus = checkUpdateDateTime();
  if(checkUpdateDateTimeStatus == 1){
    setupDS1307DateTime();
  }

  // UpdateTime continuously
  getDS1307DateTime();

  if(isRegisteringFingerprintTemplate){
    // Scanning to register fingerprint template
    clearLCD();
    printTextLCD(content, 0);
    printTextLCD("Registering fingerprint", 1);
    bool getFingerprintSuccessfully = false;
    while(!getFingerprintSuccessfully){
      if(appMode == SERVER_MODE) return;
      getFingerprintSuccessfully = getFingerprintEnroll();
      if(getFingerprintSuccessfully){
        String fingerprintTemplate = getFingerprintTemplate();
        uploadFingerprintTemplate(fingerprintTemplate);
      }
    }
  }
  else{
    //print datetime get from RTC
    parseDateTimeToString();
    printTextLCD(Date, 0);
    printTextLCD(Time, 1);
  }

  // let the websockets client check for incoming messages
  if(websocketClient.available()) {
    websocketClient.poll();
  }
}

void handleSetUpMode(){
  if(server) {
    server->handleClient();
  }
}

//====================================================================
void resetData(){

  // Empty sensor
  finger.emptyDatabase();
  clearLCD();
  printTextLCD("Empty database", 0);
}

// Set DateTime of DS1307
void setupDS1307DateTime() {
  int checkwifi = checkWifi();
  if(checkWifi == 0){
    clearLCD();
    printTextLCD("Cannot setup datetime", 0);
    printTextLCD("Wifi not connected", 1);
    while(1);
  }

  timeClient.update();
  unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
  second_ = second(unix_epoch);
  minute_ = minute(unix_epoch);
  hour_   = hour(unix_epoch);
  day_    = day(unix_epoch);
  month_  = month(unix_epoch);
  year_   = year(unix_epoch);

  rtc.adjust(DateTime(year_, month_, day_, hour_, minute_, second_));
}

// Get DateTime of DS1307
void getDS1307DateTime(){
  DateTime now = rtc.now();

  second_ = now.second();
  minute_ = now.minute();
  hour_   = now.hour();
  day_    = now.day();
  month_  = now.month();
  year_   = now.year();
}

void parseDateTimeToString(){
  Time[12] = second_ % 10 + 48;
  Time[11] = second_ / 10 + 48;
  Time[9]  = minute_ % 10 + 48;
  Time[8]  = minute_ / 10 + 48;
  Time[6]  = hour_   % 10 + 48;
  Time[5]  = hour_   / 10 + 48;

  Date[5]  = day_   / 10 + 48;
  Date[6]  = day_   % 10 + 48;
  Date[8]  = month_  / 10 + 48;
  Date[9]  = month_  % 10 + 48;
  Date[13] = (year_   / 10) % 10 + 48;
  Date[14] = year_   % 10 % 10 + 48;

  CDateTime[0] = (year_ / 10 / 10 / 10) % 10 + 48;
  CDateTime[1] = (year_ / 10 / 10) % 10 + 48;
  CDateTime[2] = (year_ / 10) % 10 + 48;
  CDateTime[3] = year_ % 10 % 10 + 48;
  CDateTime[5] = month_  / 10 + 48;
  CDateTime[6] = month_  % 10 + 48;
  CDateTime[8] = day_  / 10 + 48;
  CDateTime[9] = day_  % 10 + 48;
  CDateTime[11] = hour_  / 10 + 48;
  CDateTime[12] = hour_  % 10 + 48;
  CDateTime[14] = minute_ / 10 + 48;
  CDateTime[15] = minute_ % 10 + 48;
  CDateTime[17] = second_ / 10 + 48;
  CDateTime[18] = second_ % 10 + 48;
}

int checkWifi(){
  if(WiFi.status() == WL_CONNECTED){
    return 1;
  }
  return 0;
}

int checkUpdateDateTime() {
  unsigned long now = millis();
  if(now < lastUpdate){
    lastUpdate = 0;
    return 0;
  }
  if(lastUpdate >= 2*intervalTime){
    return 0;
  }
  else if(now > (lastUpdate + intervalTime)){
    lastUpdate = now;
    return 1;
  }
  else{
    return 0;
  }
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.println("Waiting for valid finger to enroll");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.println("Creating model for fingerprint");
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.println("Created successfully");
  delay(2000);

  return true;
}

String getFingerprintTemplate() {
  Serial.print("Attempting to get fingerprint template...\n");
  uint8_t p = finger.getModel();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template "); Serial.println(" transferring:");
      break;
    default:
      Serial.print("Unknown error "); Serial.println(p);
      return "";
  }

  // one data packet is 139 bytes. in one data packet, 11 bytes are 'usesless' :D
  uint8_t bytesReceived[556]; // 4 data packets
  memset(bytesReceived, 0xff, 556);
  uint32_t starttime = millis();
  int byteIndex = 0;
  while (byteIndex < 556 && (millis() - starttime) < 5000) {
    if (mySerial.available()) {
      bytesReceived[byteIndex++] = mySerial.read();
    }
  }
  Serial.print(byteIndex); Serial.println(" bytes read.");
  Serial.println("Decoding packet...");

  uint8_t fingerTemplate[512]; // the real template
  memset(fingerTemplate, 0xff, 512);

  for(int m=0;m<4;m++){ //filtering data packets
    uint8_t stat=bytesReceived[(m*(128+11))+6];
    if( stat!= FINGERPRINT_DATAPACKET && stat!= FINGERPRINT_ENDDATAPACKET){
      Serial.println("Bad fingerprint_packet");
      while(1){
        delay(1);
      }
    }
    memcpy(fingerTemplate + (m*128), bytesReceived + (m*(128+11))+9, 128); 
  }

  // // filtering only the data packets
  // int uindx = 9, index = 0;
  // memcpy(fingerTemplate + index, bytesReceived + uindx, 128);   // first 256 bytes
  // uindx += 256;       // skip data
  // uindx += 2;         // skip checksum
  // uindx += 9;         // skip next header
  // index += 256;       // advance pointer
  // memcpy(fingerTemplate + index, bytesReceived + uindx, 256);   // second 256 bytes

  for(int i = 0; i < 512; i++)
  {
    Serial.print(fingerTemplate[i]); Serial.print(" ");
  }

  // Fingerprint template is presented in Hexa format
  String fingerprintTemplate = "";
  for (int i = 0; i < 512; ++i) {
    char tmp[16];
    char format[128];
    sprintf(format, "%%.%dX", 2);
    sprintf(tmp, format, fingerTemplate[i]);
    fingerprintTemplate.concat(tmp);
  }
  Serial.println();
  Serial.print("Fingerprint template: " ); Serial.println(fingerprintTemplate);
  return fingerprintTemplate;
}

uint8_t deleteFingerprint(uint8_t id) {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Delete successfully!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
  }

  return p;
}

void uploadFingerprintTemplate(String fingerprintTemplate){
  int checkWifiStatus =  checkWifi();
  if(checkWifiStatus == 0) {
    clearLCD();
    printTextLCD("Connection lost", 0);
    return;
  }

  FingerData fingerData(fingerprintTemplate.c_str(), content.c_str());
  JSONVar fingerDataObject;
  fingerDataObject["fingerprintTemplate"] = fingerData.fingerprintTemplate.c_str();
  fingerDataObject["Content"] = fingerData.Content.c_str();
  String payload = JSON.stringify(fingerDataObject);

  http.begin(wifiClient, "http://" SERVER_IP "/api/Hello/fingerprint");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);
  String payloadData = http.getString();
  http.end();

  if(httpCode <= 0){
    clearLCD();
    printTextLCD("GET failed: " + http.errorToString(httpCode), 0);
    delay(1000);
    return;
  }

  if (httpCode != HTTP_CODE_OK){
    clearLCD();
    printTextLCD("GET failed!!!", 0);
    printTextLCD("Status code: " + httpCode, 1);
    delay(1000);
    return;
  }

  clearLCD();
  printTextLCD("Registered successfully", 0);
  printTextLCD(payloadData, 1);
  delay(3000);

  isRegisteringFingerprintTemplate = false;
  content = "";
}

void checkModeReset(){
  button_state = digitalRead(BUTTON_PIN);
  if(button_state == HIGH){
    settingTimeout = millis();
    do{
      if((settingTimeout + SETTING_HOLD_TIME) <= millis()){
        // Change mode
        if(appMode == SERVER_MODE){
          appMode = NORMAL_MODE;
          printTextLCD("Changing to", 0);
          printTextLCD("normal mode...", 1);
        }
        else{
          appMode = SERVER_MODE;
          printTextLCD("Changing to", 0);
          printTextLCD("setup mode...", 1);


          // Start server and AP wifi
          startConfigServer();
          WifiService.setupAP();

          clearLCD();
          printTextLCD("Setup mode started", 0);
        }
        return;
      }
      button_state = digitalRead(BUTTON_PIN);
    }
    while(button_state == HIGH);
  }
}


void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}
