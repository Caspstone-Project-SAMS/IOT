#include <ESP8266HTTPClient.h>
#include <ArduinoWebsockets.h>

#include <Arduino_JSON.h>
#include <string>

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
#include "FingerprintSensorService.h"
#include "RTCService.h"


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

class RegisteringSession{
  public:
    std::string user;
    std::string studentCode;
    std::string studentID;
    uint8_t mode;
    int8_t currentFingerNumber;
    uint16_t sessionID;
    uint8_t durationInMin;
    unsigned long endTimeStamp;
    bool normalTimeStampCase;

    RegisteringSession(){}

    RegisteringSession(std::string StudentCode, std::string StudentID, int Mode ,int SessionID){
      studentCode = StudentCode;
      studentID = StudentID;
      mode = Mode;
      sessionID = SessionID;
    }

    RegisteringSession(int SessionId, std::string User, uint8_t DurationInMin, unsigned long StartTimeStamp){
      sessionID = SessionId;
      user = User;
      durationInMin = DurationInMin;
      endTimeStamp = StartTimeStamp + (durationInMin * 60 * 1000);
      if(endTimeStamp > StartTimeStamp) normalTimeStampCase = true;
      else normalTimeStampCase = false;
    }
};
//===================================================================



//=========================Macro define=============================
#define SERVER_IP "34.81.224.196"
#define WEBSOCKETS_SERVER_HOST "34.81.224.196"
#define WEBSOCKETS_SERVER_PORT 80
#define WEBSOCKETS_PROTOCOL "ws"

// For upload fingerprint template state
#define CONNECTION_LOST 1
#define UPLOAD_FAIL 2
#define UPLOAD_SUCCESS 3
#define INVALID_DATA 4
//====================================================================



//===========================Memory variables declaration here===========================

// Websocket
WebsocketsClient websocketClient;
//==================================


// DateTime information, NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
char Time[] = "TIME:00:00:00";
char Date[] = "DATE:00-00-2000";
char CDateTime[] = "0000-00-00T00:00:00";
byte second_, minute_, hour_, day_, month_;
int year_;
//================================


// Check update DateTime
static unsigned long lastUpdate = 0;
static const unsigned long intervalTime = 2073600000; //24days
//================================


// Http Client
WiFiClient wifiClient;
HTTPClient http;
//================================


// Fingerprint template registration session
String studentCode;
String studentID;
int registrationNumber = 0;
RegisteringSession* session = nullptr;
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



String key = "I3ZbMRjf0lfag1uSJzuDKtz8J";

bool printConnectingMode = false;



//========================Set up code==================================
void connectButton(){
  pinMode(BUTTON_PIN, INPUT_PULLDOWN_16);
}

bool connectWebSocket() {

  // run callback when events are occuring
  websocketClient.onEvent(onEventsCallback);

  // run callback when messages are received
  websocketClient.onMessage([&](WebsocketsMessage message) {
    ECHO("Got Message: ");
    //Serial.println(static_cast<std::underlying_type<MessageType>::type>(message.type()));

    if(message.isText()){
      ECHOLN("Is text");
      // Get data from here
      const char* data = message.c_str();
      JSONVar message = JSON.parse(data);
      String event = message["Event"];
      JSONVar receiveData = message["Data"];
      if(event == "RegisterFingerprint"){
        if(session){
          uint16_t sessionId = receiveData["SessionID"];
          if(session->sessionID == sessionId){
            session->studentCode = (const char*)receiveData["StudentCode"];
            session->studentID = (const char*)receiveData["StudentID"];
            session->mode = receiveData["Mode"];

            if(session->mode == 1 || session->mode == 3){
              session->currentFingerNumber = 1;
            }
            else if(session->mode == 2){
              session->currentFingerNumber = 2;
            }

            appMode = REGISTRATION_MODE;

            websocketClient.send(String("Fingerprint registration ") + String(sessionId));
            delay(50);
            websocketClient.send(String("Fingerprint registration ") + String(sessionId));
          }
        }
      }
      else if(event == "ConnectModule"){
        if(session){
          websocketClient.send("Connected by other");
          delay(50);
          websocketClient.send("Connected by other");
        }

        uint16_t sessionId = receiveData["SessionID"];
        std::string user = (const char*)receiveData["User"];
        uint8_t durationInMin = receiveData["DurationInMin"];
        unsigned long now = millis();
        //unsigned long endTimestamp = now + (durationInMin * 60 * 1000);
        session = new RegisteringSession(sessionId, user, durationInMin, now);
        appMode = CONNECT_MODULE;
        printConnectingMode = true;
        String sendMessage = "Connected " + String(sessionId);

        websocketClient.send(sendMessage.c_str());
        delay(50);
        websocketClient.send(sendMessage.c_str());
      }
      else if(event == "CancelSession"){
        uint16_t sessionId = receiveData["SessionID"];
        if(session){
          if(session->sessionID == sessionId){
            session = nullptr;
            appMode = NORMAL_MODE;

            websocketClient.send(String("Cancel session ") + String(sessionId));
            delay(50);
            websocketClient.send(String("Cancel session ") + String(sessionId));
          }
        }
      }
    }
    else if(message.isBinary()){
      ECHOLN("Is binary");
      const char* data = message.c_str();
      if(strcmp(data, "ping") == 0){
        ECHOLN("Receive custom ping, lets send pong");
        String sendMessage = "pong";

        websocketClient.sendBinary(sendMessage.c_str(), sendMessage.length());
        delay(50);
        websocketClient.sendBinary(sendMessage.c_str(), sendMessage.length());
      }
    }
    else{
      ECHOLN("Does not found anything");
    }
  });

  return websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws/module?key=" + key);
}

void resetData(){
  // Empty fingerprint sensor
  FINGERPSensor.emptyDatabase();
}

void setupDateTime(){
  if(WifiService.checkWifi() == false){
    return;
  }
  bool updateTime = timeClient.update();
  if(updateTime){
    unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
    second_ = second(unix_epoch);
    minute_ = minute(unix_epoch);
    hour_   = hour(unix_epoch);
    day_    = day(unix_epoch);
    month_  = month(unix_epoch);
    year_   = year(unix_epoch);

    RTCService.setupDS1307DateTime(DateTime(year_, month_, day_, hour_, minute_, second_));
  }
}
//===================================================================

void setup() {
  Serial.begin(9600);
  Serial.println("Start");
  delay(100);

  unsigned long lcdTimeout = 0;
  bool check = false;

  connectButton();
  delay(50);
  connectLCD();
  delay(50);

  clearLCD();
  printTextLCD("Setting up...", 0);

  // Connect to RTC
  if(RTCService.connectDS1307()){
    printTextLCD("RTC connected", 1);
  }
  else{
    printTextLCD("RTC not found", 1);
  }
  lcdTimeout = millis();

  // Connect to R308 fingerprint sensor
  check = FINGERPSensor.connectFingerprintSensor();
  while((lcdTimeout + 2000) > millis()){
    delay(800);
  }
  if(check){
    printTextLCD("Fingerprint Sensor connected", 1);
    lcdTimeout = millis();
  }
  else{
    while (1) { 
      printTextLCD("Fingerprint Sensor not found", 1);
      delay(500); 
    }
  }

  // Connect to wifi
  //Serial.print("Address of WiFi object (1): "); Serial.println(reinterpret_cast<uintptr_t>(&WiFi), HEX);
  WifiService.setupWiFi(WiFi);
  int wifiStatus = WifiService.connect();
  while((lcdTimeout + 2000) > millis()){
    delay(800);
  }
  if(wifiStatus == CONNECT_OK){
    printTextLCD("Wifi connected", 1);
  }
  else if(wifiStatus == CONNECT_TIMEOUT){
    printTextLCD("Wifi connection timed out", 1);
  }
  else if(wifiStatus == CONNECT_SUSPENDED){
    printTextLCD("Suspended wifi", 1);
  }
  lcdTimeout = millis();

  if(WifiService.checkWifi()){
    // Connect to NTP Server
    while((lcdTimeout + 2000) > millis()){
      delay(800);
    }
    timeClient.begin();
    printTextLCD("NTP server connected", 1);
    lcdTimeout = millis();

    // Connect to websockets
    check = connectWebSocket();
    while((lcdTimeout + 2000) > millis()){
      delay(800);
    }
    if(check){
      printTextLCD("Websockets connected", 1);
    }
    else{
      printTextLCD("Websocket connection failed", 1);
    }
    lcdTimeout = millis();
  }

  setupDateTime();
  resetData();
  while((lcdTimeout + 2000) > millis()){
    delay(800);
  }
  printTextLCD("Setup done!!!", 1);
  delay(2000);
  
  clearLCD();
}

void loop() {
  if(appMode == SERVER_MODE) {
    handleSetUpMode();
  }
  else if(appMode == REGISTRATION_MODE){
    handleRegistrationMode();
  }
  else if(appMode == NORMAL_MODE){
    handleNormalMode();
  }
  else if(appMode == CONNECT_MODULE){
    handleConnectModuleMode();
  }
  checkModeReset();
}

void handleConnectModuleMode(){
  if(session){
    if(printConnectingMode){
      clearLCD();
      printTextNoResetLCD("Module connected", 0);
      printTextNoResetLCD(String("User: ") + session->user.c_str(), 1);
      printConnectingMode = false;
    }
  }

  if(session->normalTimeStampCase){
    if(millis() > session->endTimeStamp){
      appMode = NORMAL_MODE;
      session = nullptr;
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
      delay(50);
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
    }
  }
  else{
    if(millis() > session->endTimeStamp && millis() < 4294907290){
      appMode = NORMAL_MODE;
      session = nullptr;
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
      delay(50);
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
    }
  }

  if(websocketClient.available()) {
      websocketClient.poll();
    }
}

void handleSetUpMode(){
  if(server) {
    server->handleClient();
  }
  printTextNoResetLCD(String(WIFI_AP_SSID) + ":" + String(WIFI_AP_PASSWORD), 1);
}

void handleNormalMode(){
  if(!WifiService.checkWifi()){
    printTextLCD("Connection failed", 0);
    printTextLCD("Connect new wifi", 1);
    while(appMode != SERVER_MODE){
      checkModeReset();
      delay(50);
    }
    return;
  }

  if(!websocketClient.available()){
    printTextLCD("Websocket falied", 0);
    printTextLCD("Reconnecting...", 1);
    delay(1000);
    uint8_t c = 1;
    while(c <= 5){
      printTextLCD("Reconnect: " + String(c), 1);
      unsigned long lcdTimeout = millis();
      if(websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws/module?key=" + key)){
        while((lcdTimeout + 1500) > millis()){
          delay(800);
        }
        printTextLCD("Connect websocket successfully", 1);
        break;
      }
      c++;
    }
    return;
  }

  if(checkUpdateDateTime()){
    setupDateTime();
  }

  //print current datetime
  if(getCurrentDateTime()){
    printTextNoResetLCD(Date, 0);
    printTextNoResetLCD(Time, 1);
  }
  else{
    printTextLCD("Failed to get", 0);
    printTextLCD("current datetime", 1);
    delay(2000);
  }  

  // let the websockets client check for incoming messages
  if(websocketClient.available()) {
    websocketClient.poll();
  }
}

void handleRegistrationMode(){
  if(!WifiService.checkWifi()){
    printTextLCD("Connection failed", 0);
    printTextLCD("Connect new wifi", 1);
    while(appMode != SERVER_MODE){
      checkModeReset();
      delay(50);
    }
    return;
  }

  if(!websocketClient.available()){
    printTextLCD("Websocket falied", 0);
    printTextLCD("Reconnecting...", 1);
    delay(1000);
    uint8_t c = 1;
    while(c <= 5){
      printTextLCD("Reconnect: " + String(c), 1);
      unsigned long lcdTimeout = millis();
      if(websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws/module?key=" + key)){
        while((lcdTimeout + 1500) > millis()){
          delay(800);
        }
        printTextLCD("Connect websocket successfully", 1);
        break;
      }
      c++;
    }
    return;
  }

  printTextLCD(session->studentCode.c_str(), 0);

  // Scan
  printTextLCD("Place finger", 1);
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = FINGERPSensor.scanFinger();
    checkModeReset();
    if(websocketClient.available()) {
      websocketClient.poll();
    }
    delay(1);
  }

  // Get image 1
  p = FINGERPSensor.getImage1();
  if(p != FINGERPRINT_OK){
    printTextLCD("Try again", 1);
    delay(2000);
    return;
  }

  // Remove finger
  printTextLCD("Remove finger", 1);
  p = 0;
  while(p != FINGERPRINT_NOFINGER){
    p = FINGERPSensor.scanFinger();
    checkModeReset();
    if(websocketClient.available()) {
      websocketClient.poll();
    }
    delay(1);
  }

  // Place finger again
  printTextLCD("Place same finger again", 1);
  p = -1;
  while (p != FINGERPRINT_OK){
    p = FINGERPSensor.scanFinger();
    checkModeReset();
    if(websocketClient.available()) {
      websocketClient.poll();
    }
    delay(1);
  }

  // Get image 2
  p = FINGERPSensor.getImage2();
  if(p != FINGERPRINT_OK){
    printTextLCD("Try again", 1);
    delay(2000);
    return;
  }

  // Create model
  p = FINGERPSensor.createModel();
  if(p == FINGERPRINT_OK){
    printTextLCD("Creating...", 1);
  }
  else if(p == FINGERPRINT_ENROLLMISMATCH){
    printTextLCD("Finger does not match", 1);
    delay(2000);
    return;
  }
  else{
    printTextLCD("Error: try again", 1);
    delay(2000);
    return;
  }

  String fingerprintTemplate = FINGERPSensor.getFingerprintTemplate();
  if(fingerprintTemplate == ""){
    printTextLCD("Created failed", 1);
    delay(2000);
    return;
  }

  int uploadResult = uploadFingerprintTemplate(fingerprintTemplate);
  switch(uploadResult){
    case CONNECTION_LOST:
      printTextLCD("Connection lost", 1);
      break;
    case UPLOAD_FAIL:
      printTextLCD("Upload failed", 1);
      break;
    case UPLOAD_SUCCESS:
      if(session->mode == 3){
        if(session->currentFingerNumber == 2){
          appMode = NORMAL_MODE;
          session = nullptr;
        }
        else if(session->currentFingerNumber == 1){
          session->currentFingerNumber = 2;
        }
      }
      else if(session->mode == 1 || session->mode == 2){
        appMode = NORMAL_MODE;
        session = nullptr;
      }
      printTextLCD("Upload successfully", 1);
      break;
    case INVALID_DATA:
      printTextLCD("Invalid data", 1);
      break;
    default:
      printTextLCD("Unkonw error", 1);
      break;
  }
  delay(2000);
  clearLCD();
}

//==========================================================================================
bool getCurrentDateTime(){
  DateTime now;
  if(RTCService.getDS1307DateTime(now)){
    second_ = now.second();
    minute_ = now.minute();
    hour_   = now.hour();
    day_    = now.day();
    month_  = now.month();
    year_   = now.year();
  }
  else{
    if(WifiService.checkWifi() != CONNECT_OK){
      return false;
    }
    bool updateTime = timeClient.update();
    if(!updateTime){
      return false;
    }
    unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
    second_ = second(unix_epoch);
    minute_ = minute(unix_epoch);
    hour_   = hour(unix_epoch);
    day_    = day(unix_epoch);
    month_  = month(unix_epoch);
    year_   = year(unix_epoch);
  }

  parseDateTimeToString();
  return true;
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

bool checkUpdateDateTime() {
  unsigned long now = millis();
  if(now < lastUpdate){
    lastUpdate = 0;
    return false;
  }
  if(lastUpdate >= 2*intervalTime){
    return false;
  }
  else if(now > (lastUpdate + intervalTime)){
    lastUpdate = now;
    return true;
  }
  else{
    return false;
  }
}

void checkModeReset(){
  button_state = digitalRead(BUTTON_PIN);
  if(button_state == HIGH){
    settingTimeout = millis();
    while(button_state == HIGH)
    {
      if((settingTimeout + SETTING_HOLD_TIME) <= millis()){
        // Change mode
        if(appMode == SERVER_MODE){
          appMode = NORMAL_MODE;
          clearLCD();
          printTextLCD("Changing to", 0);
          printTextLCD("normal mode...", 1);

          delay(1500);

          // Stop server
          stopServer();
          
          // Setup normal mode before start (all everythings related to wifi)
          setupNormalMode();
          delay(2000);

          clearLCD();
        }
        else if(appMode == NORMAL_MODE){
          unsigned long lcdTimeout = 0;

          clearLCD();
          printTextLCD("Changing to", 0);
          printTextLCD("setup mode...", 1);
          lcdTimeout = millis();

          // Reset all data of normal mode
          resetNormalMode();

          while((lcdTimeout + 1500) > millis()){
            delay(800);
          }
          clearLCD();
          printTextLCD("Setting up", 0);

          // Start server and AP wifi
          if(!startConfigServer()){
            printTextLCD("Server not found", 1);
            delay(2000);
            printTextLCD("Error when", 0);
            printTextLCD("setting up", 1);
            while(1){
              delay(50);
            }
          }
          else{
            printTextLCD("Server started", 1);
          }
          lcdTimeout = millis();

          WifiService.setupAP();

          appMode = SERVER_MODE;

          while((lcdTimeout + 2000) > millis()){
            delay(800);
          }
          clearLCD();
          printTextLCD("Mode: setup", 0);
          printTextLCD(String(WIFI_AP_SSID) + ":" + String(WIFI_AP_PASSWORD), 1);
        }
        break;
      }
      button_state = digitalRead(BUTTON_PIN);
      delay(50);
    }
  }
}

void setupNormalMode(){
  unsigned long lcdTimeout = 0;
  printTextLCD("Setting up", 0);

  int wifiStatus = WifiService.connect();
  if(wifiStatus == CONNECT_OK){
    printTextLCD("Wifi connected", 1);
    lcdTimeout = millis();

    timeClient.begin();
    while((lcdTimeout + 1500) > millis()){
      delay(800);
    }

    printTextLCD("NTP server connected", 1);
    lcdTimeout = millis();

    bool checkWebSocket = websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws/module?key=" + key);
    while((lcdTimeout + 1500) > millis()){
      delay(800);
    }

    if(checkWebSocket){
      printTextLCD("Websockets connected", 1);
    }
    else{
      printTextLCD("Websocket connection failed", 1);
    }
  }
  else if(wifiStatus == CONNECT_TIMEOUT){
    printTextLCD("Wifi connection timed out", 1);
  }
  else if(wifiStatus == CONNECT_SUSPENDED){
    printTextLCD("Suspended wifi", 1);
  }
  setupDateTime();
}

void resetNormalMode(){
  if(websocketClient.available()){
    websocketClient.close();
  }
  timeClient.end();
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        ECHOLN("[Main][WebsocketEvent] Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        ECHOLN("[Main][WebsocketEvent] Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        ECHOLN("[Main][WebsocketEvent] Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        ECHOLN("[Main][WebsocketEvent] Got a Pong!");
    }
}

int uploadFingerprintTemplate(String fingerprintTemplate){
  if(!WifiService.checkWifi()) {
    return CONNECTION_LOST;
  }

  if(fingerprintTemplate == "" || session->studentID == "" || session->sessionID <= 0){
    return INVALID_DATA;
  }

  ECHOLN("[Main][UploadTemplate] Uploading fingerprint template...");
  JSONVar fingerDataObject;
  fingerDataObject["FingerprintTemplate"] = fingerprintTemplate.c_str();
  fingerDataObject["StudentID"] = session->studentID.c_str();
  fingerDataObject["SessionID"] = String(session->sessionID).c_str(); 
  fingerDataObject["FingerNumber"] = String(session->currentFingerNumber).c_str();
  String payload = JSON.stringify(fingerDataObject);

  http.begin(wifiClient, "http://" SERVER_IP "/api/Fingerprint");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);
  String payloadData = http.getString();
  http.end();

  ECHO("[Main][UploadTemplate] Receive paylod data: "); ECHOLN(payloadData);
  ECHO("[Main][UploadTemplate] Upload status: "); ECHOLN(httpCode);

  if (httpCode != HTTP_CODE_OK){
    return UPLOAD_FAIL;
  }

  return UPLOAD_SUCCESS;
}