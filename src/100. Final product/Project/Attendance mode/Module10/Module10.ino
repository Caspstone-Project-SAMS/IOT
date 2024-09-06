#include <ESP8266HTTPClient.h>
#include <ArduinoWebsockets.h>
#include <WiFiClientSecureBearSSL.h>

#include <Arduino_JSON.h>
#include <ArduinoJson.h>
#include <string>
#include <unordered_map>

#include <WiFiUdp.h>
#include <NTPClient.h>               
#include <TimeLib.h>

#include <avr/pgmspace.h>

//#include <StreamUtils.h>

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
#define SERVER_IP "34.81.223.233"
#define WEBSOCKETS_SERVER_HOST "34.81.223.233"
#define WEBSOCKETS_SERVER_PORT 80
#define WEBSOCKETS_PROTOCOL "ws"

#define DOMAIN "sams-project.com"
#define SSL_PORT 443

// For getting schedules
#define CONNECTION_LOST 1
#define GET_FAIL 2
#define GET_SUCCESS 3
#define INVALID_DATA 4
#define UNDEFINED_ERROR 5
//=======================================================================================



//=====================================Class definition==================================
class Attended{
  public:
    uint16_t scheduleID;
    std::string userID;
    DateTime attendanceTime;

    Attended(uint16_t ScheduleID, std::string UserID, DateTime AttendanceTime){
      scheduleID = ScheduleID;
      userID = UserID,
      attendanceTime = AttendanceTime;
    }
};

class Attendance {
  public:
    std::vector<uint16_t> storedFingerID;
    uint16_t scheduleID;
    std::string userID;
    std::string studentName;
    std::string studentCode;

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
    uint8_t order;
    bool isStop;

    std::vector<Attendance> attendances;
};

class StoredFingerprint {
  public:
    std::vector<uint16_t> storedFingerID;
    std::string userID;

    StoredFingerprint(std::vector<uint16_t> StoredFingerID, std::string UserID){
      userID = UserID;
      storedFingerID = StoredFingerID;
    }

    StoredFingerprint(){}
};

class PreparingAttendanceSession{
  public:
    std::string prepareDate;
    uint16_t scheduleID; 
    uint16_t sessionID;
    std::string user;
    uint8_t durationSeconds;
    unsigned long endTimeStamp;
    bool normalTimeStampCase;

    PreparingAttendanceSession(){}

    PreparingAttendanceSession(uint16_t SessionId, uint16_t scheduleId, std::string User){
      sessionID = SessionId;
      scheduleID = scheduleId;
      user = User;
    }

    PreparingAttendanceSession(uint16_t SessionId, std::string User, uint8_t DurationSeconds, unsigned long StartTimeStamp){
      sessionID = SessionId;
      user = User;
      durationSeconds = DurationSeconds;
      endTimeStamp = StartTimeStamp + (DurationSeconds * 1000);
      if(endTimeStamp > StartTimeStamp) normalTimeStampCase = true;
      else normalTimeStampCase = false;
    }
};
//=======================================================================================



//===========================Memory variables declaration here===========================
// Other datas
String content;
String lecturerId = "a829c0b5-78dc-4194-a424-08dc8640e68a";
String semesterId = "5";
//==================================


// DateTime format
const char dateFormat[] = "%Y-%m-%d";
const char timeFormat[] = "%H:%M:%S";
//const char dateTimeFormat[]  = "%Y-%m-%d %H:%M:%S";
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


// Check websocket
static unsigned long lastUpdateWebsocket = 0;
static const unsigned long intervalTimeCheckWebsocket = 10000; // 10 seconds
//================================


// Http Client
//HTTPClient http;
//================================

//Session management
PreparingAttendanceSession* session = nullptr;
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

// Active Buzzer
const unsigned char ACTIVE_BUZZER_PIN = 12;
//================================

// Data to work with
std::vector<Schedule> schedules;
std::vector<Attended> uploadedAttendedList;
std::vector<Attended> unUploadedAttendedList;
std::vector<StoredFingerprint> storedFingerprints;
Schedule* onGoingSchedule = nullptr;
bool printScheduleInformation = true;
static unsigned long lastUpdateAttendanceStatus = 0;
static const unsigned long updateAttendanceStatusIntervalTime = 15000; //15 seconds
//================================
//=======================================================================================


const char key[] PROGMEM = "gzqzg4cbIp0dUAEHsVSvRDtgg";
//const char key[] PROGMEM = "xSJ6nDkyRcjYn81hPy5H9fRZg";

bool printConnectingMode = false;

//unsigned long limitRange = 4294907290;

//std::vector<char*> errorCalledGetScheduleInformationUrl;


//============================ Configuration =============================
bool connectionSound = true;
uint16_t connectionSoundDurationMs = 400;
uint8_t connectionLifeTimeSeconds = 20;

bool attendanceSound = false;
uint16_t attendanceSoundDurationMs = 400;

uint8_t attendanceDurationMinutes = 0;

//======================================== TLS/SSL ==================================
const char fingerprint_sams_com [] PROGMEM = "DC:19:DE:0B:D0:79:11:E4:BE:75:BE:4F:BD:FC:B9:DE:D9:0A:E7:4F";
// std::unique_ptr<BearSSL::WiFiClientSecure> secureWifiClient(new BearSSL::WiFiClientSecure);
// HTTPClient httpss;



//=====================================Set up code=======================================
void connectButton(){
  pinMode(BUTTON_PIN, INPUT_PULLDOWN_16);
}

void connectBuzzer(){
  pinMode(ACTIVE_BUZZER_PIN, OUTPUT) ;
}

bool connectWebSocket() {

  websocketClient.onEvent(onEventsCallback);

  websocketClient.onMessage([&](WebsocketsMessage message) {
    if(message.isText()){
      const char* data = message.c_str();
      JSONVar message = JSON.parse(data);
      String event = message["Event"];
      JSONVar receiveData = message["Data"];
      
      if(event == "ConnectModule"){
        if(session){
          websocketClient.send("Connected by other");
          delay(50);
          websocketClient.send("Connected by other");
        }
        else{
          uint16_t sessionId = receiveData["SessionID"];
          std::string user = (const char*)receiveData["User"];
          unsigned long now = millis();
          session = new PreparingAttendanceSession(sessionId, user, connectionLifeTimeSeconds, now);
          appMode = CONNECT_MODULE;
          printConnectingMode = true;
          String sendMessage = "Connected " + String(sessionId);

          websocketClient.send(sendMessage.c_str());
          delay(50);
          websocketClient.send(sendMessage.c_str());

          if(connectionSound){
            digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
            delay(connectionSoundDurationMs);
            digitalWrite(ACTIVE_BUZZER_PIN, LOW);
          }
        }
      }
      else if(event == "PrepareAttendance"){
        if(session){
          uint16_t sessionId = receiveData["SessionID"];
          if(session->sessionID == sessionId){
            session->scheduleID = receiveData["ScheduleID"];

            appMode = PREPARE_ATTENDANCE_MODE;

            websocketClient.send(String("Prepare attendance ") + String(sessionId));
            delay(50);
            websocketClient.send(String("Prepare attendance ") + String(sessionId));
          }
        }
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

            clearLCD();
          }
        }
      }
      else if(event == "StopAttendance"){
        uint16_t scheduleID = receiveData["ScheduleID"];
        if(onGoingSchedule){
          if(onGoingSchedule->scheduleID == scheduleID){
            onGoingSchedule->isStop = true;
            // Stop attendance
            endAttendanceSession();
          }
        }
        websocketClient.send(String("Stop attendance ") + String(scheduleID));
        delay(50);
        websocketClient.send(String("Stop attendance ") + String(scheduleID));
      }
      else if(event == "StartAttendance"){
        uint16_t scheduleID = receiveData["ScheduleID"];
        auto schedule_check = [scheduleID](const auto& obj){
          return obj.scheduleID == scheduleID; 
        };
        auto it_schedule_check = std::find_if(schedules.begin(), schedules.end(), schedule_check);
        if(it_schedule_check != schedules.end()){
          it_schedule_check->isStop = false;

          websocketClient.send(String("Start attendance ") + String(scheduleID) + String(" successfully"));
          delay(50);
          websocketClient.send(String("Start attendance ") + String(scheduleID) + String(" successfully"));
        }
        else{
          websocketClient.send(String("Start attendance ") + String(scheduleID) + String(" failed"));
          delay(50);
          websocketClient.send(String("Start attendance ") + String(scheduleID) + String(" failed"));
        }
      }
      else if(event == "CheckCurrentSession"){
        if(session){
          websocketClient.send("Check current session " + String(session->sessionID));
          delay(50);
          websocketClient.send("Check current session " + String(session->sessionID));
        }
      }
      else if(event == "PrepareSchedules"){
        if(session){
          uint16_t sessionId = receiveData["SessionID"];
          if(session->sessionID == sessionId){
            session->prepareDate = (const char*)receiveData["PrepareDate"];

            appMode = PREPARE_SCHEDULES_MODE;

            websocketClient.send(String("Prepare schedules ") + String(sessionId));
            delay(50);
            websocketClient.send(String("Prepare schedules ") + String(sessionId));
          }
        }
      }
      else if(event == "CheckUploadedSchedule"){
        uint16_t scheduleId = receiveData["ScheduleId"];
        if(schedules.size() > 0){
          auto have_schedule = [scheduleId](const auto& obj){
            return (obj.scheduleID == scheduleId);
          };
          auto it_have_schedule = std::find_if(schedules.begin(), schedules.end(), have_schedule);
          if(it_have_schedule != schedules.end()){
            // Uploaded schedule exist
            websocketClient.send(String("Check uploaded schedule ") + String(scheduleId) + " available");
            delay(50);
            websocketClient.send(String("Check uploaded schedule ") + String(scheduleId) + " available");
          }
          else{
            websocketClient.send(String("Check uploaded schedule ") + String(scheduleId) + " unavailable");
            delay(50);
            websocketClient.send(String("Check uploaded schedule ") + String(scheduleId) + " unavailable");
          }
        }
        else{
          websocketClient.send(String("Check uploaded schedule ") + String(scheduleId) + " unavailable");
          delay(50);
          websocketClient.send(String("Check uploaded schedule ") + String(scheduleId) + " unavailable");
        }
      }
      else if(event == "SyncingAttendanceData"){
        uint16_t scheduleId = receiveData["ScheduleId"];

        // Let's search schedule
        auto schedule_existed = [scheduleId](const auto& obj){
          return (obj.scheduleID == scheduleId);
        };
        auto it_schedule_existed = std::find_if(schedules.begin(), schedules.end(), schedule_existed);
        if(it_schedule_existed != schedules.end()){
          if(syncingAttendanceData(scheduleId)){
            websocketClient.send(String("Syncing attendance data successfully ") + String(scheduleId));
            delay(50);
            websocketClient.send(String("Syncing attendance data successfully ") + String(scheduleId));
          }
          else{
            websocketClient.send(String("Syncing attendance data failed ") + String(scheduleId));
            delay(50);
            websocketClient.send(String("Syncing attendance data failed ") + String(scheduleId));
          }
        }
        else{
          websocketClient.send(String("Syncing attendance data failed ") + String(scheduleId));
          delay(50);
          websocketClient.send(String("Syncing attendance data failed ") + String(scheduleId));
        }
      }
      else if(event == "ApplyConfigurations"){
        connectionSound = receiveData["ConnectionSound"];
        connectionSoundDurationMs = receiveData["ConnectionSoundDurationMs"];
        attendanceSound = receiveData["AttendanceSound"];
        attendanceSoundDurationMs = receiveData["AttendanceSoundDurationMs"];
        connectionLifeTimeSeconds = receiveData["ConnectionLifeTimeSeconds"];
        attendanceDurationMinutes = receiveData["AttendanceDurationMinutes"];

        websocketClient.send("Apply configurations successfully");
        delay(50);
        websocketClient.send("Apply configurations successfully");
      }
      else if(event == "SetupDateTime"){
        String updatedDateTime = receiveData["UpdatedDateTime"];
        // Setup new DateTime
        int year = updatedDateTime.substring(0, 4).toInt();
        int month = updatedDateTime.substring(5, 7).toInt();
        int day = updatedDateTime.substring(8, 10).toInt();
        int hour = updatedDateTime.substring(11, 13).toInt();
        int min = updatedDateTime.substring(14, 16).toInt();
        int sec = updatedDateTime.substring(17, 19).toInt();
        //ECHOLN("Updated datetime " + String(year) + ", " + String(month) + ", " + String(day) + ", " + String(hour) + ", " + String(min) + ", " + String(sec));
        DateTime newDateTime(year, month, day, hour, min, sec);
        RTCService.setupDS1307DateTime(newDateTime);
      }
    }
    else if(message.isBinary()){
      const char* data = message.c_str();
      if(strcmp(data, "ping") == 0){
        //ECHOLN("Receive custom ping, lets send pong");
        String sendMessage = "pong";

        websocketClient.sendBinary(sendMessage.c_str(), sendMessage.length());
        delay(50);
        websocketClient.sendBinary(sendMessage.c_str(), sendMessage.length());
      }
    }
    else{
      //ECHOLN("Does not found anything");
    }
  });

  char buffer[26];
  strcpy_P(buffer, key);

  char path[45];
  strcpy(path, "/ws/module?key=");
  strcat(path, buffer);

  memset(buffer, '\0', sizeof(buffer));  // Clears the buffer

  return websocketClient.connect(DOMAIN, 8080, path);
}

void resetData(){
  // Empty fingerprint sensor
  FINGERPSensor.emptyDatabase();

  // Emty data in RAM
  schedules.clear();
  storedFingerprints.clear();
  uploadedAttendedList.clear();
  unUploadedAttendedList.clear();
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

    DateTime newDateTime(year_, month_, day_, hour_, minute_, second_); 
    RTCService.setupDS1307DateTime(newDateTime);
  }
}
//=======================================================================================



//=====================================Main code=========================================
void setup() {
  Serial.begin(9600);
  delay(1);

  ECHO("000: ");
  ECHOLN(ESP.getFreeHeap());

  unsigned long lcdTimeout = 0;
  bool check = false;

  connectBuzzer();
  delay(1);
  connectButton();
  delay(1);
  connectLCD();
  delay(1);

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
  while((lcdTimeout + 1000) > millis()){
    delay(200);
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
  while((lcdTimeout + 1000) > millis()){
    delay(200);
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
    delay(10);
    setupDateTime();
    while((lcdTimeout + 1000) > millis()){
      delay(200);
    }
    printTextLCD("NTP server connected", 1);
    lcdTimeout = millis();

    // Connect to websockets
    check = connectWebSocket();
    while((lcdTimeout + 1000) > millis()){
      delay(200);
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

  // Setup certificate's fingerprint
  //secureWifiClient->setFingerprint(fingerprint_sams_com);

  delay(50);
  printTextLCD("Setup done!!!", 1);
  delay(1000);
  
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
  else if(appMode == CONNECT_MODULE){
    handleConnectModuleMode();
  }
  else if(appMode == PREPARE_ATTENDANCE_MODE){
    handleAttendancePreparationMode();
  }
  else if(appMode == PREPARE_SCHEDULES_MODE){
    handlePrepareSchedulesMode();
  }
  checkModeReset();
  delay(1);
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
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
      delay(50);
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
      appMode = NORMAL_MODE;
      session = nullptr;
    }
  }
  else{
    unsigned long limitRange = 4294907290;
    if(millis() > session->endTimeStamp && millis() < limitRange){
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
      delay(50);
      websocketClient.send(String("Session cancelled ") + String(session->sessionID));
      appMode = NORMAL_MODE;
      session = nullptr;
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
  if(WifiService.checkWifi() && checkWebsocket()){
    if(!websocketClient.available(true)){
      printTextLCD("Websocket falied", 0);
      printTextLCD("Reconnecting...", 1);
      delay(1000);
      uint8_t c = 1;
      while(c <= 5){
        printTextLCD("Reconnect: " + String(c), 1);
        unsigned long lcdTimeout = millis();
        if(connectWebSocket()){
          while((lcdTimeout + 1500) > millis()){
            delay(800);
          }
          printTextLCD("Connect websocket successfully", 1);
          delay(1500);
          clearLCD();
          break;
        }
        c++;
      }
      return;
    }
  }

  if(checkUpdateDateTime()){
    setupDateTime();
  }

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  if(unUploadedAttendedList.size() > 0 && checkUpdateAttendanceStatus()){
    updateAttendanceStatusAgain();
  }

  //print current datetime
  if(getCurrentDateTime()){
    printTextNoResetLCD(Date, 0);
    printTextNoResetLCD(Time, 1);
  }
  else{
    printTextLCD("Failed to get", 0);
    printTextLCD("current datetime", 1);
    delay(1000);
  }

  if(getOnGoingSchedule()){
    printScheduleInformation = true;
    return;
  }

  // let the websockets client check for incoming messages
  if(websocketClient.available()) {
    websocketClient.poll();
  }
}

void handleAttendanceMode(){
  if(onGoingSchedule == nullptr || onGoingSchedule->isStop){
    endAttendanceSession();
    clearLCD();
    delay(1);
    return;
  }

  unsigned long lcdTimeout = 0;
  uint16_t startTime = getScheduleStartTimeInMinute(onGoingSchedule->startTimeStruct);
  while(1){
    if(attendanceModeToSetupMode()){
      while(1){
        handleSetUpMode();
        if(attendanceModeToSetupMode()){
          printScheduleInformation = true;
          break;
        }
      }
    }

    if(onGoingSchedule == nullptr || onGoingSchedule->isStop){
      endAttendanceSession();
      clearLCD();
      delay(1);
      break;
    }

    if(appMode != ATTENDANCE_MODE){
      onGoingSchedule = nullptr;
      clearLCD();
      break;
    }

    uint16_t endTime = getScheduleEndTimeInMinute(onGoingSchedule->startTimeStruct, onGoingSchedule->endTimeStruct);
    uint16_t currentTime = getCurrentTimeInMinute();
    
    if(currentTime > endTime || currentTime < startTime){
      //ECHOLN("Slot end");
      endAttendanceSession();
      clearLCD();
      delay(1);
      break;
    }

    if(printScheduleInformation){
      while((lcdTimeout + 1000) > millis()){
        delay(200);
      }
      printTextLCD((onGoingSchedule->classCode).c_str(), 0);
      printTextLCD((onGoingSchedule->startTime.substr(0, 5) + "-" + onGoingSchedule->endTime.substr(0, 5)).c_str(), 1);
      printScheduleInformation = false;
    }

    if(unUploadedAttendedList.size() > 0 && checkUpdateAttendanceStatus()){
      updateAttendanceStatusAgain();
    }

    if(FINGERPSensor.scanFinger() == FINGERPRINT_OK){
      if(FINGERPSensor.image2Tz() == FINGERPRINT_OK){
        printTextLCD("Scanning...", 0);
        if(FINGERPSensor.seachFinger() == FINGERPRINT_OK){
          uint16_t fingerId = FINGERPSensor.getFingerID();
          if(fingerId > 0){
            if(websocketClient.available()) {
              websocketClient.poll();
            }
            auto finger_matched = [fingerId](const auto& obj){ return obj == fingerId; };
            auto finger_check = [fingerId, &finger_matched](const auto& obj){
              auto it_finger_check = std::find_if(obj.storedFingerID.begin(), obj.storedFingerID.end(), finger_matched);
              return it_finger_check != obj.storedFingerID.end(); 
            };
            auto attendance_it = std::find_if(onGoingSchedule->attendances.begin(), onGoingSchedule->attendances.end(), finger_check);
            if (attendance_it != onGoingSchedule->attendances.end()) {
              // Lets make sound
              if(attendanceSound){
                digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
                delay(attendanceSoundDurationMs);
                digitalWrite(ACTIVE_BUZZER_PIN, LOW);
              }
              //====================Marking attendance===========================
              //==========Checking whether if attended or not====================
              uint16_t scheduleID = attendance_it->scheduleID;
              std::string userID = attendance_it->userID;
              auto is_already_attended = [scheduleID, userID](const auto& obj){ 
                return (obj.scheduleID == scheduleID && obj.userID == userID); 
              };
              auto it_already_attended_uploaded = std::find_if(uploadedAttendedList.begin(), uploadedAttendedList.end(), is_already_attended);
              auto it_already_attended_unUploaded = std::find_if(unUploadedAttendedList.begin(), unUploadedAttendedList.end(), is_already_attended);
              if(it_already_attended_uploaded != uploadedAttendedList.end() || it_already_attended_unUploaded != unUploadedAttendedList.end()){
                printTextLCD((attendance_it->studentCode + " already attended").c_str(), 1);
              }
              //================If not, lets marking attendance==================
              else{
                DateTime attendedTime = getCurrentDateTimeNow();
                Attended attended(attendance_it->scheduleID, attendance_it->userID, attendedTime);
                if(updateAttendanceStatus(attendance_it->scheduleID, attendance_it->userID, attendedTime)){
                  uploadedAttendedList.push_back(attended);
                }
                else{
                  unUploadedAttendedList.push_back(attended);
                }
                printTextLCD((attendance_it->studentCode + " attended").c_str(), 1);
              }
            }
            else{
              printTextLCD("You're not in class", 1);
            }
          }
          else{
            printTextLCD("Finger not matched", 1);
          }
        }
        else{
          printTextLCD("Finger not matched", 1);
        }
        lcdTimeout = millis();
        printScheduleInformation = true;
      }
    }

    if(WifiService.checkWifi() && checkWebsocket()){
      if(!websocketClient.available(true)){
        connectWebSocket();
      }
    }

    if(websocketClient.available()) {
      websocketClient.poll();
    }

    delay(1);
  }
}

void handleAttendancePreparationMode(){
  if(websocketClient.available()) {
    websocketClient.poll();
  }
  if(session){
    // Clear all data
    resetData();

    printTextLCD("Loading schedule data", 0);
    printTextLCD("Loading...", 1);

    uint16_t totalFingers = 0;
    uint16_t uploadedFingers = 0;

    bool result = getScheduleById(session->scheduleID, totalFingers, uploadedFingers);

    // then notify to server that the job is finished
    if(result){
      printTextLCD("Loaded successfully", 1);
      websocketClient.send(String("Schedule preparation completed successfully;;;") + String(session->sessionID) + ";;;" + String(uploadedFingers));
      delay(50);
      //websocketClient.send(String("Schedule preparation completed successfully ") + String(session->sessionID));
    }
    else{
      printTextLCD("Loaded failed", 1);
      websocketClient.send(String("Schedule preparation completed failed;;;") + String(session->sessionID));
      delay(50);
      //websocketClient.send(String("Schedule preparation completed failed ") + String(session->sessionID));
    }
  }
  else{
    printTextLCD("Loading failed", 0);
    printTextLCD("Session not started", 1);
  }
  appMode = NORMAL_MODE;
  session = nullptr;
  if(websocketClient.available()) {
    websocketClient.poll();
  }
  delay(2000);
  if(websocketClient.available()) {
    websocketClient.poll();
  }
  clearLCD();
}

void handlePrepareSchedulesMode(){
  if(websocketClient.available()) {
    websocketClient.poll();
  }

  if(session){
    // Clear all data
    resetData();

    clearLCD();
    printTextLCD("Loading schedules data", 0);
    printTextLCD("Loading...", 1);

    uint8_t total = 0;
    uint8_t loaded = 0;
    uint16_t totalFingers = 0;
    uint16_t uploadedFingers = 0;
    int loadingStatus = loadingData(total, loaded, totalFingers, uploadedFingers, session->sessionID);

    switch(loadingStatus){
      case CONNECTION_LOST:
        printTextLCD("Connection lost", 1);
        websocketClient.send(String("Schedule preparation completed failed ") + String(session->sessionID));
        delay(50);
        break;
      case GET_FAIL:
        printTextLCD("Loaded failed", 1);
        websocketClient.send(String("Schedule preparation completed failed ") + String(session->sessionID));
        delay(50);
        break;
      case GET_SUCCESS:
        printTextLCD("Loaded " + String(loaded) +  "/" + String(total) + " schedule", 1);
        websocketClient.send(String("Schedule preparation completed successfully;;;") + String(session->sessionID) + ";;;" + String(uploadedFingers));
        delay(50);
        break;
      case UNDEFINED_ERROR:
        printTextLCD("Undefined error", 1);
        websocketClient.send(String("Schedule preparation completed failed ") + String(session->sessionID));
        delay(50);
        break;
      default:
        printTextLCD("Undefined error", 1);
        websocketClient.send(String("Schedule preparation completed failed ") + String(session->sessionID));
        delay(50);
        break;
    }
  }
  else{
    printTextLCD("Loading failed", 0);
    printTextLCD("Session not started", 1);
  }

  appMode = NORMAL_MODE;
  session = nullptr;
  if(websocketClient.available()) {
    websocketClient.poll();
  }
  delay(1000);
  if(websocketClient.available()) {
    websocketClient.poll();
  }
  clearLCD();
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

DateTime getCurrentDateTimeNow(){
  DateTime now;
  if(RTCService.getDS1307DateTime(now)){
    return now;
  }
  else{
    if(WifiService.checkWifi() != CONNECT_OK){
      return DateTime(0, 0, 0);
    }
    bool updateTime = timeClient.update();
    if(!updateTime){
      return DateTime(0, 0, 0);
    }
    unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
    return DateTime(year(unix_epoch), month(unix_epoch), day(unix_epoch), hour(unix_epoch), minute(unix_epoch), second(unix_epoch));
  }
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

uint16_t getCurrentTimeInMinute(){
  DateTime now;
  if(RTCService.getDS1307DateTime(now)){
    return now.hour() * 60 + now.minute();
  }
  else{
    if(WifiService.checkWifi() != CONNECT_OK){
      return 0;
    }
    bool updateTime = timeClient.update();
    if(!updateTime){
      return 0;
    }
    unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
    return hour(unix_epoch) * 60 + minute(unix_epoch);
  }
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

bool checkWebsocket(){
  unsigned long now = millis();
  if(now < lastUpdateWebsocket){
    lastUpdateWebsocket = 0;
    return false;
  }
  if(now > (lastUpdateWebsocket + intervalTimeCheckWebsocket)){
    lastUpdateWebsocket = now;
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
          setupDateTime();
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

    bool checkWebSocket = connectWebSocket();
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
}

void resetNormalMode(){
  if(websocketClient.available()){
    websocketClient.close();
  }
  timeClient.end();
}

bool attendanceModeToSetupMode(){
  bool updated = false;
  button_state = digitalRead(BUTTON_PIN);
  if(button_state == HIGH){
    settingTimeout = millis();
    while(button_state == HIGH)
    {
      if((settingTimeout + SETTING_HOLD_TIME) <= millis()){
        // Change mode
        if(appMode == SERVER_MODE){
          updated = true;
          appMode = ATTENDANCE_MODE;
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
        else if(appMode == ATTENDANCE_MODE){
          updated = true;
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
  return updated;
}

int loadingData(uint8_t& total, uint8_t& loaded, uint16_t& totalFingers, uint16_t& uploadedFingers, uint16_t sessionID){
  if(!WifiService.checkWifi()) {
    return CONNECTION_LOST;
  }

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  int getSchedulesStatus = getSchedules(total);
  switch(getSchedulesStatus){
    case GET_FAIL:
      //ECHOLN("[loadingData] Get failed at getSchedule()");
      return GET_FAIL;
      break;
    case INVALID_DATA:
      //ECHOLN("[loadingData] Get failed at getSchedule()");
      return GET_FAIL;
      break;
    case GET_SUCCESS:
      break;
    default:
      //("[loadingData] Get failed at getSchedule()");
      return UNDEFINED_ERROR;
      break;
  }

  //Order and get first 4 schedules here
  //...

  //uint16_t oldStoredModelId = 1;
  uint16_t oldStoredModelId = 1;
  uint16_t storeModelID = 1;

  WiFiClient wifiClient;
  HTTPClient http;

  for(Schedule& schedule : schedules){
    if(getScheduleInformation(schedule, storeModelID, totalFingers, false) == GET_SUCCESS){
      loaded++;
      printTextLCD("Loaded " + String(loaded) + " schedules", 1);
      //ECHOLN("Old: " + String(oldStoredModelId) + ", new: " + String(storeModelID));
      // Should update the state of preparation - uploaded finger of the current schedule
      String updateStateUrlPath = "http://" + String(DOMAIN) + "/api/Session/schedule-preparation/state-update";
      // http://34.81.223.233/api/Session/schedule-preparation/state-update
      JSONVar updateState;
      updateState["SessionId"] = sessionID;
      updateState["UploadedFingerprints"] = storeModelID - oldStoredModelId;
      updateState["ScheduleId"] = schedule.scheduleID;

      String payload = JSON.stringify(updateState);

      http.begin(wifiClient, updateStateUrlPath);
      http.addHeader("Content-Type", "application/json");
      int httpCode = http.POST(payload);

      http.end();
      payload.clear();
      
      oldStoredModelId = storeModelID;
    }
  }
  //ECHOLN("Stored models: " + String(storeModelID - 1));
  uploadedFingers = storeModelID - 1;

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  ECHOLN("");
  ECHOLN("");
  ECHOLN("Finger Id: " + String(storeModelID));
    ECHOLN("Stored fingers: " + String(storedFingerprints.size()));
    ECHOLN("Schedules: " + String(schedules.size()));
    for(Schedule& schedule : schedules){
      ECHOLN(String("Schedule of ") + schedule.classCode.c_str() + " has " + String(schedule.attendances.size()) + " attendances");
      for(Attendance& attendance : schedule.attendances){
        for(uint16_t fingerId : attendance.storedFingerID){
          ECHO(String(fingerId));
        }
        ECHOLN();
      }
    }

  // Make a notification about the status of receiving schedules
  // Call notification API here
  return GET_SUCCESS;
}

int getSchedules(uint8_t& totalSchedules) {
  String currentDate = getCurrentDate();
  if(session){
    currentDate = session->prepareDate.c_str();
  }
  String urlPath = "http://" + String(DOMAIN) + "/api/Schedule?lecturerId=" + lecturerId + "&semesterId=" + semesterId + "&startDate=" + currentDate + "&endDate=" + currentDate + "&quantity=10";
  //http://34.81.223.233/api/Schedule?lecturerId=a829c0b5-78dc-4194-a424-08dc8640e68a&quantity=10&semesterId=2&startDate=2024-07-07&endDate=2024-07-07

  WiFiClient wifiClient;
  HTTPClient https;
  https.begin(wifiClient, urlPath);

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK){
    ECHOLN("[getSchedules] Url: " + urlPath);
    ECHOLN("[getSchedules] HTTP is not OK: " + https.errorToString(httpCode));
    return GET_FAIL;
  }

  const String& payload = https.getString();

  JSONVar scheduleDataArray = JSON.parse(payload);
  // Check whether if data is in correct format
  if(JSON.typeof(scheduleDataArray)!="array"){
    //ECHOLN("[getSchedules] Invalid data format: " + payload);
    return INVALID_DATA;
  }

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  https.end();

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
      schedule.isStop = false;
    
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

bool getScheduleById(uint16_t scheduleId, uint16_t& totalFingers, uint16_t& uploadedFingers){
  //std::string domainStr = "sams-project.com";
  std::string urlPath = "http://sams-project.com/api/Schedule/module/" + std::to_string(scheduleId);

  WiFiClient wifiClient;
  HTTPClient https;
  https.begin(wifiClient, urlPath.c_str());


  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK){
    return false;
  }
  const String& payload = https.getString();
  JSONVar resultData = JSON.parse(payload);

  https.end();

  // Check whether if data is in correct format
  if(JSON.typeof(resultData)!="object"){
    //ECHOLN("[getScheduleById] Invalid data format: " + payload);
    return false;
  }

  JSONVar scheduleData = resultData["result"];

  if(JSON.typeof(scheduleData)!="object"){
    //ECHOLN("[getScheduleById] Invalid data format: " + payload);
    return false;
  }

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  Schedule schedule;
  schedule.scheduleID = scheduleData["scheduleID"];
  schedule.classID = scheduleData["class"]["classID"];
  schedule.date = (const char*)scheduleData["date"];
  schedule.slotNumber = scheduleData["slot"]["slotNumber"];
  schedule.classCode = (const char*)scheduleData["class"]["classCode"];
  schedule.startTime = (const char*)scheduleData["slot"]["startTime"];
  schedule.endTime = (const char*)scheduleData["slot"]["endtime"];
  schedule.isStop = false;

  //schedule.subjectCode = (const char*)scheduleData[i]["subjectCode"];
  //schedule.roomName = (const char*)scheduleData[i]["roomName"];
    
  // Create struct for date, start time and end time
  strptime(schedule.date.c_str(), dateFormat, &schedule.dateStruct);
  strptime(schedule.startTime.c_str(), timeFormat, &schedule.startTimeStruct);
  strptime(schedule.endTime.c_str(), timeFormat, &schedule.endTimeStruct);

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  urlPath.clear();
  scheduleData = JSONVar();
  resultData = JSONVar();

  uint16_t storeModelID = 1;
  int getInformationResult = getScheduleInformation(schedule, storeModelID, totalFingers, true);
  uploadedFingers = storeModelID - 1;

  if(getInformationResult == GET_SUCCESS){
    schedules.push_back(schedule);

    ECHOLN("");
    ECHOLN("");
    ECHOLN("Finger Id: " + String(storeModelID));
    ECHOLN("Stored fingers: " + String(storedFingerprints.size()));
    ECHOLN("Schedules: " + String(schedules.size()));
    for(Schedule& schedule : schedules){
      ECHOLN(String("Schedule of ") + schedule.classCode.c_str() + " has " + String(schedule.attendances.size()) + " attendances");
      for(Attendance& attendance : schedule.attendances){
        for(uint16_t fingerId : attendance.storedFingerID){
          ECHO(String(fingerId));
        }
        ECHOLN();
      }
    }


    return true;
  }

  if(websocketClient.available()) {
    websocketClient.poll();
  }

  return false;
}

// not yet
void orderSchedules(){
  if(schedules.size() == 0){
    return;
  }

  std::unordered_map<int, int> scheduleOrders;
  for(const Schedule& schedule : schedules){

  }
}

int getScheduleInformation(Schedule& schedule, uint16_t& storeModelID, uint16_t& totalFingers, bool isHttps){
  int page = 1;
  std::string baseUrl;
  //http://34.81.223.233/api/Student/get-students-by-classId?classID=2&startPage=1&endPage=1&quantity=2&isModule=true

  if(isHttps){
    baseUrl = "https://sams-project.com:444/api/Student/get-students-by-classId-v2?isModule=true&quantity=1&classID=" + std::to_string(schedule.classID);
  }
  else{
    baseUrl = "http://sams-project.com/api/Student/get-students-by-classId-v2?isModule=true&quantity=1&classID=" + std::to_string(schedule.classID);
  }

  if(session && (appMode == PREPARE_ATTENDANCE_MODE || appMode == PREPARE_SCHEDULES_MODE)){
    baseUrl = baseUrl + "&sessionId=" + std::to_string(session->sessionID);
  }

  std::unique_ptr<BearSSL::WiFiClientSecure> wifiClient(new BearSSL::WiFiClientSecure);
  WiFiClient wifiClientNoSecure;
  HTTPClient https;
  int httpCode;

  while(true){
    ECHO("111: ");
    ECHOLN(ESP.getFreeHeap());
  
    std::string calledUrl = baseUrl + "&startPage=" + std::to_string(page) + "&endPage=" + std::to_string(page);
    DynamicJsonDocument students(2300);

    if(isHttps){
      wifiClient->setFingerprint(fingerprint_sams_com);
      wifiClient->setBufferSizes(2500, 256);
      https.useHTTP10(true);
      https.begin(*wifiClient, calledUrl.c_str());

      httpCode = https.GET();
      deserializeJson(students, https.getStream());

      https.end();
      wifiClient->stop();
    }
    else{
      https.begin(wifiClientNoSecure, calledUrl.c_str());
      httpCode = https.GET();
      deserializeJson(students, https.getStream());
      https.end();
    }

    if(websocketClient.available()) {
      websocketClient.poll();
    }

    if (httpCode != HTTP_CODE_OK){
      ECHOLN("[getScheduleInformation] HTTP is not OK: " + https.errorToString(httpCode));
      page++;
      continue;
    }

    if(students.size() == 0){
      ECHOLN(F("[getScheduleInformation] Size of students array is 0"));
      break;
    }

    if(websocketClient.available()) {
      websocketClient.poll();
    }

    for(int i = 0; i < students.size(); i++) {
      // If student have finger informations, lets store it
      if(students[i]["fingerprintTemplateData"].is<JsonArray>() && students[i]["fingerprintTemplateData"].size() > 0){
        Attendance attendance;
        attendance.scheduleID = schedule.scheduleID;
        attendance.userID = students[i]["userID"].as<const char*>();
        attendance.studentName = students[i]["studentName"].as<const char*>();
        attendance.studentCode = students[i]["studentCode"].as<const char*>();

        // if there is a student information already added, so the fingerprint template also, but we still need to record that uploaded
        std::string userID = attendance.userID;
        auto finger_already = [userID](const auto& obj){
          return (obj.userID == userID && obj.storedFingerID.size() > 0);
        };
        auto it_finger_already = std::find_if(storedFingerprints.begin(), storedFingerprints.end(), finger_already);
        if(it_finger_already != storedFingerprints.end()){
          attendance.storedFingerID = it_finger_already->storedFingerID;
          storeModelID = storeModelID + it_finger_already->storedFingerID.size();
        }
        else{
          StoredFingerprint storedFingerprint;
          storedFingerprint.userID = userID;
          for(uint8_t fingerIndex = 0; fingerIndex < students[i]["fingerprintTemplateData"].size(); fingerIndex++){
            ++totalFingers;
            bool uploadFingerStatus = FINGERPSensor.uploadFingerprintTemplate(students[i]["fingerprintTemplateData"][fingerIndex].as<const char*>(), storeModelID);
            if(!uploadFingerStatus){
              const std::string classId = std::to_string(schedule.classID);
              UploadFingerprintTemplateAgain(storeModelID, fingerIndex, storedFingerprint, attendance, classId);
            }
            else{
              attendance.storedFingerID.push_back(storeModelID);
              storedFingerprint.storedFingerID.push_back(storeModelID);
              ++storeModelID;
            }
          }

          if(storedFingerprint.storedFingerID.size() > 0){
            storedFingerprints.push_back(storedFingerprint);
          }
        }
        schedule.attendances.push_back(attendance);
      }

      if(websocketClient.available()) {
        websocketClient.poll();
      }
    }

    page++;

    if(websocketClient.available()) {
      websocketClient.poll();
    }
    delay(1);
  }

  // Thêm cơ chế chạy lại những thằng lỗi

  return GET_SUCCESS;
}

bool getOnGoingSchedule(){
  if(schedules.size() == 0){
    return false;
  }

  for(Schedule& schedule : schedules){
    if(!schedule.isStop){
      if(checkOnDate(schedule.dateStruct)){
        if(checkOnTime(schedule.startTimeStruct, schedule.endTimeStruct)){
          onGoingSchedule = &schedule;
          appMode = ATTENDANCE_MODE;
          return true;
        }
      }
    }
  }

  return false;
}

bool checkOnDate(const struct tm& date){
  if((date.tm_year+1900) != year_){
    return false;
  }
  if((date.tm_mon+1) != month_){
    return false;
  }
  if(date.tm_mday != day_){
    return false;
  }
  return true;
}

bool checkOnTime(const struct tm& startTime, const struct tm& endTime){
  uint32_t start = (startTime.tm_hour * 60 + startTime.tm_min) * 60 + startTime.tm_sec;
  uint32_t now = (hour_ * 60 + minute_) * 60 + second_;
  uint32_t end = 0;
  if(attendanceDurationMinutes > 0){
    end = (startTime.tm_hour * 60 + startTime.tm_min) * 60 + startTime.tm_sec + attendanceDurationMinutes * 60;
    uint32_t checkEnd = (endTime.tm_hour * 60 + endTime.tm_min) * 60 + endTime.tm_sec;
    if(end > checkEnd){
      end = checkEnd;
    }
  }
  else{
    end = (endTime.tm_hour * 60 + endTime.tm_min) * 60 + endTime.tm_sec;
  }
  
  if(end < start){
    if(now > start || now < end){
      return true;
    }
  }
  else{
    if(start < now && now < end){
      return true;
    }
  }
  return false;
}

uint16_t getScheduleEndTimeInMinute(const struct tm& startTime, const struct tm& endTime){
  uint16_t checkEnd = endTime.tm_hour * 60 + endTime.tm_min;
  if(attendanceDurationMinutes > 0){
    uint16_t start = startTime.tm_hour * 60 + startTime.tm_min;
    uint16_t end = start + attendanceDurationMinutes;
    if(end > checkEnd){
      return checkEnd;
    }
    return end;
  }
  return checkEnd;
}

uint16_t getScheduleStartTimeInMinute(const struct tm& startTime){
  uint16_t start = startTime.tm_hour * 60 + startTime.tm_min;
  return start;
}

bool updateAttendanceStatus(const uint16_t& scheduleID, const std::string& userID, const DateTime& attendedTime){
  char buf[] = "YYYY-MM-DD hh:mm:ss";
  String dateTime = attendedTime.toString(buf);
  dateTime[10] = 'T';

  //=====================================
  char buf1[] = "YYYY-MM-DD hh:mm:ss";
  String dateTime1 = attendedTime.toString(buf1);
  ECHOLN(dateTime1);
  //=====================================

  if(!WifiService.checkWifi()) {
    return false;
  }
  //2024-06-11T16:29:24
  //http://35.221.168.89/api/Attendance/update-attendance-status?scheduleID=5&attendanceStatus=3&attendanceTime=2024-06-11T16%3A29%3A24&studentID=fa00c1a6-0a14-435c-a421-08dc8640e68a
  String url = "https://" + String(DOMAIN) + "/api/Attendance/update-attendance-status?attendanceStatus=1&scheduleID=" + String(scheduleID) + "&studentID=" + userID.c_str() + "&attendanceTime=" + dateTime;

  std::unique_ptr<BearSSL::WiFiClientSecure> wifiClient(new BearSSL::WiFiClientSecure);
  wifiClient->setFingerprint(fingerprint_sams_com);
  wifiClient->setBufferSizes(250, 512);
  HTTPClient https;
  https.begin(*wifiClient, url);
  int httpCode = https.PUT("");

  https.end();
  wifiClient->stop();

  if (httpCode != HTTP_CODE_OK){
    ECHO("[updateAttendanceStatus] Update failed: "); ECHOLN(https.errorToString(httpCode));
    return false;
  }
  //String payload = https.getString();
  //ECHOLN("[updateAttendanceStatus] Put request payload: " + payload);

  return true;
}

void updateAttendanceStatusAgain(){
  if(!WifiService.checkWifi()) {
    return;
  }

  char buf[] = "YYYY-MM-DDThh:mm:ss";
  JSONVar attendanceArray;
  uint8_t index = 0;
  for(Attended& item : unUploadedAttendedList){
    JSONVar attendance;
    attendance["ScheduleID"] = item.scheduleID;
    attendance["StudentID"] = item.userID.c_str();
    attendance["AttendanceStatus"] = "1";
    attendance["AttendanceTime"] = item.attendanceTime.toString(buf);
    attendanceArray[index++] = attendance;
  }
  String payload = JSON.stringify(attendanceArray);

  //http://35.221.168.89/api/Attendance/update-list-student-status
  String url = "https://" + String(DOMAIN) + "/api/Attendance/update-list-student-status";

  std::unique_ptr<BearSSL::WiFiClientSecure> wifiClient(new BearSSL::WiFiClientSecure);
  wifiClient->setFingerprint(fingerprint_sams_com);
  wifiClient->setBufferSizes(200, payload.length() + 100);
  HTTPClient https;
  https.addHeader("Content-Type", "application/json");
  https.begin(*wifiClient, url);
  https.addHeader("Content-Type", "application/json");
  int httpCode = https.PUT(payload);
  if(httpCode == HTTP_CODE_OK){
    ECHO("Ok 1");
    uploadedAttendedList.insert(uploadedAttendedList.end(), unUploadedAttendedList.begin(), unUploadedAttendedList.end());
    unUploadedAttendedList.clear(); // Makes it empty
  }

  https.end();
  wifiClient->stop();
  payload.clear();
}

bool checkUpdateAttendanceStatus() {
  unsigned long now = millis();
  if(now < lastUpdateAttendanceStatus){
    lastUpdateAttendanceStatus = 0;
    return false;
  }
  if(now > (lastUpdateAttendanceStatus + updateAttendanceStatusIntervalTime)){
    lastUpdateAttendanceStatus = now;
    return true;
  }
  else{
    return false;
  }
}

void endAttendanceSession(){
  appMode = NORMAL_MODE;
  onGoingSchedule = nullptr;
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        //ECHOLN("[Main][WebsocketEvent] Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        //ECHOLN("[Main][WebsocketEvent] Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        //ECHOLN("[Main][WebsocketEvent] Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        //ECHOLN("[Main][WebsocketEvent] Got a Pong!");
    }
}

// Cơ chế dự phòng, upload lại 3 lần
void UploadFingerprintTemplateAgain(uint16_t& storeModelID, uint8_t& fingerIndex, StoredFingerprint& storedFingerprint, Attendance& attendance, const std::string& classID){
  ECHOLN(F("Upload fingerprint again state"));
  std::string baseUrl = "http://sams-project.com/api/Student/get-students-by-classId-v2?isModule=true&classID=" + classID + "&userId=" + attendance.userID;

  int num = 0;
  WiFiClient client;
  HTTPClient https;

  //http://34.81.223.233/api/Student/get-students-by-classId?classID=29&userId=b60b2e83-b6d3-4240-a422-08dc8640e68a&isModule=true
  for(uint8_t i = 0; i < 3; i++){
    https.begin(client, baseUrl.c_str());
    int httpCode = https.GET();
    JSONVar students = JSON.parse(https.getString());
    https.end();
    
    if (httpCode == HTTP_CODE_OK){
      if(JSON.typeof(students)=="array"){
        if(JSON.typeof(students[0]["fingerprintTemplateData"])=="array" && students[0]["fingerprintTemplateData"].length() > 0){
          ECHO(F("Get Oke: ")); ECHOLN(String(i));
          bool uploadFingerStatus = FINGERPSensor.uploadFingerprintTemplate((const char*)students[0]["fingerprintTemplateData"][fingerIndex], storeModelID);
          ECHO(F("Get Oke: ")); ECHO(String(uploadFingerStatus)); ECHOLN(String(i));
          if(uploadFingerStatus){
            ECHO("Stored fingerprint id: "); ECHOLN(String(storeModelID));
            attendance.storedFingerID.push_back(storeModelID);
            storedFingerprint.storedFingerID.push_back(storeModelID);
            ++storeModelID;
            break;
          }
        }
      }
    }

    if(websocketClient.available()) {
      websocketClient.poll();
    }
    delay(1);
  }
  delay(1);
}

bool syncingAttendanceData(uint16_t scheduleID){
  if(!WifiService.checkWifi()) {
    return false;
  }

  //http://35.221.168.89/api/Attendance/update-list-student-status
  String url = "https://" + String(DOMAIN) + "/api/Attendance/update-list-student-status";

  char buf[] = "YYYY-MM-DDThh:mm:ss";

  // Create a payload string
  JSONVar attendanceArray;
  uint8_t index = 0;
  for(Attended& item : unUploadedAttendedList){
    JSONVar attendance;
    attendance["ScheduleID"] = item.scheduleID;
    attendance["StudentID"] = item.userID.c_str();
    attendance["AttendanceStatus"] = "1";
    attendance["AttendanceTime"] = item.attendanceTime.toString(buf);
    attendanceArray[index++] = attendance;
  }
  for(Attended& item : uploadedAttendedList){
    if(item.scheduleID == scheduleID){
      JSONVar attendance;
      attendance["ScheduleID"] = item.scheduleID;
      attendance["StudentID"] = item.userID.c_str();
      attendance["AttendanceStatus"] = "1";
      attendance["AttendanceTime"] = item.attendanceTime.toString(buf);
      attendanceArray[index++] = attendance;
    }
  }

  if(attendanceArray.length() <= 0){
    ECHOLN("Sync 1");
    return true;
  }

  String payload = JSON.stringify(attendanceArray);

  std::unique_ptr<BearSSL::WiFiClientSecure> wifiClient(new BearSSL::WiFiClientSecure);
  HTTPClient https;
  wifiClient->setFingerprint(fingerprint_sams_com);
  wifiClient->setBufferSizes(300, payload.length() + 100);
  https.addHeader("Content-Type", "application/json");
  https.begin(*wifiClient, url);
  https.addHeader("Content-Type", "application/json");

  int httpCode = https.PUT(payload);

  if(httpCode != HTTP_CODE_OK){
    ECHO("[syncingAttendanceData] Update failed: "); ECHOLN(https.errorToString(httpCode));
    ECHOLN("Payload: " + payload);
    ECHOLN("Url: " + url);
    ECHOLN("Response: " + https.getString());
    ECHOLN("Status code: " + String(httpCode));
    return false;
  }

  ECHOLN("Sync 2");

  uploadedAttendedList.insert(uploadedAttendedList.end(), unUploadedAttendedList.begin(), unUploadedAttendedList.end());
  unUploadedAttendedList.clear(); // Makes it empty
  payload.clear();
  https.end();
  wifiClient->stop();

  ECHOLN("Sync 3");
  return true;
}