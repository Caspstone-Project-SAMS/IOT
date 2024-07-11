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



//======================================Macro define=====================================
#define SERVER_IP "34.81.224.196"
#define WEBSOCKETS_SERVER_HOST "34.81.224.196"
#define WEBSOCKETS_SERVER_PORT 80
#define WEBSOCKETS_PROTOCOL "ws"

// For getting schedules
#define CONNECTION_LOST 1
#define GET_FAIL 2
#define GET_SUCCESS 3
#define INVALID_DATA 4
#define UNDEFINED_ERROR 5
//=======================================================================================



//=====================================Class definition==================================
class Attendance {
  public:
    uint16_t storedFingerID;
    uint16_t scheduleID;
    std::string userID;
    std::string studentName;
    std::string studentCode;
    DateTime attendanceTime;
    bool attended;

    Attendance(uint16_t storedFingerId, uint16_t scheduleId, std::string userId, std::string StudentCode) {
      storedFingerID = storedFingerId;
      scheduleID = scheduleId;
      userID = userId;
      studentCode = StudentCode;
      attended = false;
    }

    Attendance(){}
};

class Schedule {
  public:
    uint16_t scheduleID;
    uint16_t classID;
    std::string date;
    uint8_t slotNumber;
    std::string classCode;
    std::string subjectCode;
    std::string roomName;
    std::string startTime;
    std::string endTime;
    struct tm dateStruct;
    struct tm startTimeStruct;
    struct tm endTimeStruct;

    std::vector<Attendance> attendances;
};
//=======================================================================================



//===========================Memory variables declaration here===========================
// Other datas
String content;
String lecturerId = "a829c0b5-78dc-4194-a424-08dc8640e68a";
String semesterId = "2";
//==================================


// DateTime format
const char* dateFormat = "%Y-%m-%d";
const char* timeFormat = "%H:%M:%S";
const char* dateTimeFormat = "%Y-%m-%d %H:%M:%S";
//==================================


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

// List of schedules, classes and attendances
std::vector<Schedule> schedules;
//================================

// Whether if update to server or not
bool attedancesUpdated = true;
//================================

bool displayAttendanceSession = true;

// On-going schedule
//ScheduleData* onGoingSchedule = nullptr;
//================================

// Check store fingerprint template or not
bool fingerprintIsStored = false;
//================================



//=======================================================================================



//=====================================Set up code=======================================
void connectButton(){
  pinMode(BUTTON_PIN, INPUT_PULLDOWN_16);
}

bool connectWebSocket() {

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
      String receiveData = message["Data"];
      if(event == "RegisterFingerprint"){
        content = receiveData;
      }
    }
  });

  return websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws?isRegisterModule=true");
}

void resetData(){
  // Empty fingerprint sensor
  FINGERPSensor.emptyDatabase();

  // Emty data in RAM
  schedules.clear();
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
//=======================================================================================



//=====================================Main code=========================================
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

  // If wifi connection is ok
  if(WifiService.checkWifi()){
    // Connect to NTP Server
    timeClient.begin();
    while((lcdTimeout + 2000) > millis()){
      delay(800);
    }
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

  resetData();  
  setupDateTime();

  while((lcdTimeout + 2000) > millis()){
    delay(800);
  }
  printTextLCD("Loading datas", 1);
  lcdTimeout = millis();

  uint8_t total = 0;
  uint8_t loaded = 0;
  int loadingStatus = loadingData(total, loaded);
  while((lcdTimeout + 2000) > millis()){
    delay(800);
  }
  switch(loadingStatus){
    case CONNECTION_LOST:
      printTextLCD("Connection lost", 1);
      break;
    case GET_SUCCESS:
      printTextLCD("Get " + String(loaded) +  "/" + String(total) + " schedule", 1);
      break;
    case UNDEFINED_ERROR:
      printTextLCD("Undefined error", 1);
      break;
    default:
      printTextLCD("Undefined error", 1);
      break;
  }

  delay(2000);
  printTextLCD("Setup done!!!", 1);
  delay(2000);
  
  clearLCD();
}

void loop() {
  if(appMode == SERVER_MODE) {
    handleSetUpMode();
  }
  else if(appMode == ATTENDANCE_MODE){
    handleAttendanceMode();
  }
  else if(appMode == NORMAL_MODE){
    handleNormalMode();
  }
  checkModeReset();
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
      if(websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws?isRegisterModule=true")){
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

void handleAttendanceMode(){

}
//=======================================================================================



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

String getCurrentDate(){
  char CurrenrDate[] = "0000-00-00";
  CurrenrDate[0] = (year_ / 10 / 10 / 10) % 10 + 48;
  CurrenrDate[1] = (year_ / 10 / 10) % 10 + 48;
  CurrenrDate[2] = (year_ / 10) % 10 + 48;
  CurrenrDate[3] = year_ % 10 % 10 + 48;
  CurrenrDate[5] = month_  / 10 + 48;
  CurrenrDate[6] = month_  % 10 + 48;
  CurrenrDate[8] = day_  / 10 + 48;
  CurrenrDate[9] = day_  % 10 + 48;
  return CurrenrDate;
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

    bool checkWebSocket = websocketClient.connect(WEBSOCKETS_SERVER_HOST, WEBSOCKETS_SERVER_PORT, "/ws?isRegisterModule=true");
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

int loadingData(uint8_t& total, uint8_t& loaded){
  if(!WifiService.checkWifi()) {
    return CONNECTION_LOST;
  }

  int getSchedulesStatus = getSchedules(total);
  switch(getSchedulesStatus){
    case GET_FAIL:
      return GET_FAIL;
      break;
    case INVALID_DATA:
      return GET_FAIL;
      break;
    case GET_SUCCESS:
      break;
    default:
      return UNDEFINED_ERROR;
      break;
  }

  uint16_t storeModelID = 1;
  for(Schedule& schedule : schedules){
    if(getScheduleInformation(schedule, storeModelID) == GET_SUCCESS){
      loaded++;
    }
  }

  // Make a notification about the status of receiving schedules
  // Call notification API here
  return GET_SUCCESS;
}

int getSchedules(uint8_t& totalSchedules) {
  String currentDate = getCurrentDate();
  String url = "http://" + String(SERVER_IP) + "/api/Schedule?lecturerId=" + lecturerId + "&semesterId=" + semesterId + "&startDate=" + currentDate + "&endDate=" + currentDate + "&quantity=10";
  //http://34.81.224.196/api/Schedule?lecturerId=a829c0b5-78dc-4194-a424-08dc8640e68a&quantity=10&semesterId=2&startDate=2024-07-07&endDate=2024-07-07

  http.begin(wifiClient, url);
  http.end();

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK){
    return GET_FAIL;
  }

  String payload = http.getString();
  JSONVar scheduleDataArray = JSON.parse(payload);
  // Check whether if data is in correct format
  if(JSON.typeof(scheduleDataArray)!="array"){
    return INVALID_DATA;
  }

  // Load schedules information
  for(int i = 0; i < scheduleDataArray.length(); i++) {
    if(JSON.typeof(scheduleDataArray[i])=="object"){
      Schedule schedule;
      schedule.scheduleID = scheduleDataArray[i]["scheduleID"];
      schedule.classID = scheduleDataArray[i]["classID"];
      schedule.date = (const char*)scheduleDataArray[i]["date"];
      schedule.slotNumber = scheduleDataArray[i]["slotNumber"];
      schedule.classCode = (const char*)scheduleDataArray[i]["classCode"];
      schedule.subjectCode = (const char*)scheduleDataArray[i]["subjectCode"];
      schedule.roomName = (const char*)scheduleDataArray[i]["roomName"];
      schedule.startTime = (const char*)scheduleDataArray[i]["startTime"];
      schedule.endTime = (const char*)scheduleDataArray[i]["endTime"];
    
      // Create struct for date, start time and end time
      strptime(schedule.date.c_str(), dateFormat, &schedule.dateStruct);
      strptime(schedule.startTime.c_str(), timeFormat, &schedule.startTimeStruct);
      strptime(schedule.endTime.c_str(), timeFormat, &schedule.endTimeStruct);

      schedules.push_back(schedule);

      ++totalSchedules;
    }
  }

  return GET_SUCCESS;
}

int getScheduleInformation(Schedule& schedule, uint16_t& storeModelID){
  int page = 1;
  String baseUrl = "http://" + String(SERVER_IP) + "/api/Student/get-students-by-classId?isModule=true&quantity=3&classID=" + String(schedule.classID);
  //http://34.81.224.196/api/Student/get-students-by-classId?classID=2&startPage=1&endPage=1&quantity=3&isModule=true

  while(true){
    String calledUrl = baseUrl + "&startPage=" + String(page) + "&endPage=" + String(page);
    http.begin(wifiClient, calledUrl);
    http.end();

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK){
      return GET_FAIL;
    }

    String payload = http.getString();
    JSONVar students = JSON.parse(payload);

    if(JSON.typeof(students)!="array"){
      return INVALID_DATA;
    }

    if(students.length() == 0){
      break;
    }

    for(int i = 0; i < students.length(); i++) {
      Attendance attendance;
      attendance.scheduleID = schedule.scheduleID;
      attendance.userID = (const char*)students[i]["userID"];
      attendance.studentName = (const char*)students[i]["studentName"];
      attendance.studentCode = (const char*)students[i]["studentCode"];
      if(JSON.typeof(students[i]["fingerprintTemplateData"])=="array"){
        for(uint8_t fingerIndex = 0; fingerIndex < students[i]["fingerprintTemplateData"].length(); fingerIndex++){
          bool uploadFingerStatus = FINGERPSensor.uploadFingerprintTemplate((const char*)students[i]["fingerprintTemplateData"][fingerIndex], storeModelID);
          if(uploadFingerStatus){
            attendance.storedFingerID = storeModelID;
            ++storeModelID;
            schedule.attendances.push_back(attendance);
          }
        }
      }
    }

    page++;
    delay(10);
  }

  return GET_SUCCESS;
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