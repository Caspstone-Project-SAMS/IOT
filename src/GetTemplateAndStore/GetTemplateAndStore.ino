//C:\Users\....\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\libraries\ESP8266HTTPClient\src
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#include <Adafruit_Fingerprint.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <string>

#include <WiFiUdp.h>
#include <NTPClient.h>               
#include <TimeLib.h>
#include <map>

#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>

//=========================Macro define=============================
#define SERVER_IP "34.81.224.196"

#ifndef STASSID
#define STASSID "FPTU_Student-616" //"oplus_co_apcnzr" //"FPTU_Library" //"oplus_co_apcnzr" //"Nhim" //"oplus_co_apcnzr" //"FPTU_Library" // //"Garage Coffee" //
#define STAPSK "12345678" //"vhhd3382" //"12345678" //"vhhd3382" //"1357924680" //"vhhd3382" //"12345678" // //"garageopen24h" //
#endif

#define Finger_Rx 0 //D3 in ESP8266 is GPIO0
#define Finger_Tx 2 //D4 is GPIO2
//=====================================================================



//===========================Class definition===========================
class FingerData {
  public:
    uint8_t id;
    std::string fingerprintTemplate;
    uint8_t fingerId;
};

class ScheduleData {
  public:
    uint8_t scheduleID;
    uint8_t classID;
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
};

class Student {
  public:
    std::string studentName;
    std::string userID;
    std::string studentCode;
    std::vector<std::string> fingerprintTemplateData;
};

class Class {
  public:
    uint8_t classID;
    std::string classCode;
    std::vector<Student> students;

    Class(std::string code){
      classCode = code;
    }
};

class Attendance {
  public:
    uint16_t storedFingerID;
    uint8_t scheduleID;
    std::string userID;
    std::string studentCode;
    DateTime attendanceTime;
    bool attended;

    Attendance(uint8_t storedFingerId, uint8_t scheduleId, std::string userId, std::string StudentCode) {
      storedFingerID = storedFingerId;
      scheduleID = scheduleId;
      userID = userId;
      studentCode = StudentCode;
      attended = false;
    }
};

class AttendanceObject {
  public:
    uint8_t ScheduleID;
    uint8_t AttendanceStatus;
    std::string AttendanceTime;
    std::string StudentID;

    AttendanceObject(uint8_t scheduleID, uint8_t attendanceStatus, std::string attendanceTime, std::string studentID){
      ScheduleID = scheduleID;
      AttendanceStatus = attendanceStatus;
      AttendanceTime = attendanceTime;
      StudentID = studentID;
    }
};
//=======================================================================



//===========================Memory variables declaration here===========================
// DateTime format
const char* dateFormat = "%Y-%m-%d";
const char* timeFormat = "%H:%M:%S";
const char* dateTimeFormat = "%Y-%m-%d %H:%M:%S";

// List of schedules, classes and attendances
std::vector<ScheduleData> scheduleDatas;
std::vector<Class> classes;
std::vector<Attendance> attendances;

// Whether if update to server or not
std::vector<AttendanceObject> unUpadatedAttendances;
bool attedancesUpdated = true;

bool displayAttendanceSession = true;

// On-going schedule
ScheduleData* onGoingSchedule = nullptr;

// Check store fingerprint template or not
bool fingerprintIsStored = false;

// Fingerprint sensor
SoftwareSerial mySerial(Finger_Rx, Finger_Tx);
int template_buf_size = 512;
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
//================================


// DateTime information, NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
char Time[] = "TIME:00:00:00   ";
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


// LCD I2C
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
//================================


// Http Client
WiFiClient client;
HTTPClient http;
//================================



//========================Set up code==================================
void conenctLCD() {
  lcd.init();
  lcd.backlight();
}

void connectFingerprintSensor() {
  //Serial.println("\n\nAdafruit Fingerprint sensor enrollment");
  finger.begin(57600);
  if (finger.verifyPassword()) {
    //Serial.println("Found fingerprint sensor!");
  } else {
    //Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  // Serial.println(F("Reading sensor parameters"));
  // finger.getParameters();
  // Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  // Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  // Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  // Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  // Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  // Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  // Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);
}

void conenctWifi() {
  delay(100);
  // Serial.println("\n\n\n================================");
  // Serial.println("Connecting to Wifi");

  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  //Serial.print("Connected! IP address: "); Serial.println(WiFi.localIP());
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
  // while(!connectToRTC){
  //   if(!rtc.begin()) {
  //     Serial.println("Couldn't find RTC");
  //     Serial.println("Do you want to reconnect RTC");
  //     Serial.println("'Y' to yes, others to no");
  //     while(1){
  //       if(Serial.available()){
  //         if(Serial.read() == 'Y'){
  //           break;
  //         }
  //         else{
  //           connectToRTC = true;
  //           break;
  //         }
  //       }
  //     }
  //   }
  //   else{
  //     connectToRTC = true;
  //     haveRTC = true;
  //   }
  // }
}
//===================================================================


void setup() {
  Serial.begin(9600);
  delay(100);

  // Connect I2C LCD
  conenctLCD();
  delay(2000);

  // Connect to DS1307 (RTC object)
  lcd.clear();
  printTextLCD("Connect RTC", 0);
  connectDS1307();
  delay(2000);
  
  // Connect to R308 fingerprint sensor
  lcd.clear();
  printTextLCD("Connect Fingerprint Sensor", 0);
  connectFingerprintSensor();
  delay(2000);

  // Connet to wifi
  lcd.clear();
  printTextLCD("Connect Wifi", 0);
  conenctWifi();
  delay(2000);

  // Connect to ntp server;
  lcd.clear();
  printTextLCD("Connect NTP server", 0);
  timeClient.begin();
  delay(2000);

  // Setup datetime for module
  lcd.clear();
  printTextLCD("Setup DateTime", 0);
  delay(1000);
  if(haveRTC){
    lcd.clear();
    printTextLCD("RTC found", 0);
    printTextLCD("Setup RTC", 0);
    setupDS1307DateTime();
  }
  else{
    lcd.clear();
    printTextLCD("RTC not found", 0);
    while(1);
  }

  lcd.clear();
  printTextLCD("Prepare data", 0);
  delay(1000);

  //Reset data
  resetData();

  // Load data
  getSchedule();
  getStudent();

  lcd.clear();
  printTextLCD("Setup done!!!", 0);
  delay(2000);
}

void loop() {
  int checkUpdateDateTimeStatus = checkUpdateDateTime();
  if(checkUpdateDateTimeStatus == 1){
    setupDS1307DateTime();
  }


  // UpdateTime continuously
  getDS1307DateTime();


  // Get/Update on going schedule in each loop
  getOnGoingSchedule();


  // Store fingerprint to sensor if not write and (on-going schedule is avialable - check inside the called method)
  if(!fingerprintIsStored && onGoingSchedule != nullptr){
    writeFingerprintTemplateToSensor();
  }


  // Session start if in slot, otherwise display date-time
  if(onGoingSchedule!=nullptr){
    uint32_t duration = 0;
    getDurationInSeconds(onGoingSchedule->startTimeStruct, onGoingSchedule->endTimeStruct, duration);
    lcd.clear();
    printTextLCD("Duration: " + String(duration), 0);
    delay(3000);

    if(duration > 0){
      attendanceSession(duration);
    }
  }
  else{
    //print datetime get from RTC
    parseDateTimeToString();
    printTextLCD(Date, 0);
    printTextLCD(Time, 1);
    delay(1000);
  }
}


//===========================Common functions================================

// Convert fingerprint template in a string of hexa to binary (8 bit unsigned integer)
uint8_t convert_hex_to_binary(std::string hexString){
  uint8_t resultArray[512];
  // Process the input string
  if(hexString.length() != 2) {
    // Handle invalid input (string length must be exactly 2)
    Serial.print("Invalid number of digits of hexString");
    return 0;
  }
  // Parse the hexadecimal string and convert to uint8_t
  uint8_t result;
  sscanf(hexString.c_str(), "%hhx", &result);
  return result;
}



// Fingerprint scanning
int16_t getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -2;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  // Serial.print("\nFound ID #"); Serial.print(finger.fingerID);
  // Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}



// Set DateTime of DS1307
void setupDS1307DateTime() {
  int checkwifi = checkWifi();
  if(checkWifi == 0){
    lcd.clear();
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



void printTextLCD(String message, int row){
  if(message.length() > 16) {
    for(int i=0; i < 5; i++) {
      message = " " + message; 
    }
    message = message + " ";

    for(int pos = 0; pos < message.length() - 16; pos++){
      lcd.setCursor(0, row);
      lcd.print("");
      lcd.print(message.substring(pos, pos + 16));
      delay(350);
    }
  }
  else{
    lcd.setCursor(0, row);
    lcd.print(message); 
  }
}



void getSchedule() {
  int totalGet = 0;

  String lecturerId = "a829c0b5-78dc-4194-a424-08dc8640e68a";
  String semesterId = "2";
  String startDate = "2024-06-23";
  String endDate = "2024-06-25";
  String url = "http://" + String(SERVER_IP) + "/api/Schedule?lecturerId=" + lecturerId + "&semesterId=" + semesterId + "&startDate=" + startDate + "&endDate=" + endDate;
  //http://35.221.168.89/api/Schedule?lecturerId=a829c0b5-78dc-4194-a424-08dc8640e68a&semesterId=2&startDate=2024-11-06&endDate=2024-12-06

  int checkWifiStatus = checkWifi();
  if(checkWifiStatus == 0){
    lcd.clear();
    printTextLCD("Cannot get schedules", 0);
    printTextLCD("Wifi not connected", 1);
    delay(1000);
    return;
  }

  http.begin(client, url);
  int httpCode = http.GET();
  if(httpCode <= 0){
    lcd.clear();
    printTextLCD("GET failed: " + http.errorToString(httpCode), 0);
    delay(1000);
    return;
  }

  if (httpCode != HTTP_CODE_OK){
    lcd.clear();
    printTextLCD("GET failed!!!", 0);
    printTextLCD("Status code: " + httpCode, 1);
    delay(1000);
    return;
  }

  String payload = http.getString();
  http.end();
  JSONVar scheduleDataArray = JSON.parse(payload);

  // Check whether if data is in correct format
  if(JSON.typeof(scheduleDataArray)!="array"){
    lcd.clear();
    printTextLCD("Invalid data format!!!", 0);
    delay(1000);
    return;
  }
  for(int i = 0; i < scheduleDataArray.length(); i++) {
    if(JSON.typeof(scheduleDataArray[i])!="object"){
      lcd.clear();
      printTextLCD("Invalid data format!!!", 0);
      delay(1000);
      return;
    }
  }

  std::vector<int> loadedClassIds;

  // Load schedules information
  for(int i = 0; i < scheduleDataArray.length(); i++) {
    ScheduleData scheduleData;
    scheduleData.scheduleID = scheduleDataArray[i]["scheduleID"];
    scheduleData.classID = scheduleDataArray[i]["classID"];
    scheduleData.date = (const char*)scheduleDataArray[i]["date"];
    scheduleData.slotNumber = scheduleDataArray[i]["slotNumber"];
    scheduleData.classCode = (const char*)scheduleDataArray[i]["classCode"];
    scheduleData.subjectCode = (const char*)scheduleDataArray[i]["subjectCode"];
    scheduleData.roomName = (const char*)scheduleDataArray[i]["roomName"];
    scheduleData.startTime = (const char*)scheduleDataArray[i]["startTime"];
    scheduleData.endTime = (const char*)scheduleDataArray[i]["endTime"];
    
    // Create struct for date, start time and end time
    strptime(scheduleData.date.c_str(), dateFormat, &scheduleData.dateStruct);
    strptime(scheduleData.startTime.c_str(), timeFormat, &scheduleData.startTimeStruct);
    strptime(scheduleData.endTime.c_str(), timeFormat, &scheduleData.endTimeStruct);

    scheduleDatas.push_back(scheduleData);

    // load class information
    Class newClass(scheduleData.classCode);
    newClass.classID = scheduleDataArray[i]["classID"];
    addClass(loadedClassIds, newClass);

    ++totalGet;
  }

  lcd.clear();
  printTextLCD("Total get: " + String(totalGet), 0);
  delay(2000);
}



void getStudent(){
  lcd.clear();
  printTextLCD("Get students information", 0);
  delay(1000);

  int totalGet = 0;

  String url = "http://" + String(SERVER_IP) + "/api/Student/get-students-by-classId?isModule=true&classID=";
  //http://35.221.168.89/api/Student/get-students-by-classId?classID=1&isModule=true
  int checkWifiStatus = checkWifi();
  if(checkWifiStatus == 0){
    lcd.clear();
    printTextLCD("Cannot get students", 0);
    printTextLCD("Wifi not connected", 1);
    delay(1000);
    return;
  }

  if(classes.size() == 0){
    lcd.clear();
    printTextLCD("No classes information", 0);
    delay(1000);
    return;
  }

  for(Class& item : classes){
    lcd.clear();
    printTextLCD(String("From class ") + item.classCode.c_str(), 0);
    delay(1000);

    String apiEndpoint = url + String(item.classID);

    http.begin(client, apiEndpoint);
    int httpCode = http.GET();
    if(httpCode <= 0){
      lcd.clear();
      printTextLCD("GET failed: " + http.errorToString(httpCode), 0);
      delay(1000);
      return;
    }

    if (httpCode != HTTP_CODE_OK){
      lcd.clear();
      printTextLCD("GET failed!!!", 0);
      printTextLCD("Status code: " + httpCode, 1);
      delay(1000);
      return;
    }

    String payload = http.getString();
    http.end();
    JSONVar studentDataArray = JSON.parse(payload);

    // Check whether if data is in correct format
    if(JSON.typeof(studentDataArray)!="array"){
      lcd.clear();
      printTextLCD("Invalid data format!!!", 0);
      delay(1000);
      return;
    }

    std::vector<Student> studentInClass;
    Student student;
    std::vector<std::string> fingerprints;
    for(int i = 0; i < studentDataArray.length(); i++) {
      student.studentName = (const char*)studentDataArray[i]["studentName"];
      student.userID = (const char*)studentDataArray[i]["userID"];
      student.studentCode = (const char*)studentDataArray[i]["studentCode"];
      if(JSON.typeof(studentDataArray[i]["fingerprintTemplateData"])!="array"){
        lcd.clear();
        printTextLCD("Invalid data format!!!", 0);
        delay(1000);
        return;
      }
      for(int index = 0; index < studentDataArray[i]["fingerprintTemplateData"].length(); index++){
        fingerprints.push_back((const char*)studentDataArray[i]["fingerprintTemplateData"][index]);
      }
      student.fingerprintTemplateData = fingerprints;
      fingerprints.clear();
      studentInClass.push_back(student);
      printTextLCD("Get " + String(i+1), 1);
      delay(1000);
    }
    item.students = studentInClass;
    totalGet++;
  }

  lcd.clear();
  printTextLCD("Number of classes: " + String(classes.size()), 0);
  printTextLCD("Get students of " + String(totalGet) + " classes", 1);
  delay(2000);
}



int checkWifi(){
  // for(int i = 1; i<= 5; i++){
    
  //   lcd.clear();
  //   printTextLCD("Attempting to connect wifi: " + String(1), 0);
  //   WiFi.begin(STASSID, STAPSK);
  //   delay(500);
  // }
  // printTextLCD("Connect failed", 1);
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



void resetData(){

  // Empty sensor
  finger.emptyDatabase();
  lcd.clear();
  printTextLCD("Empty database", 0);

  // Empty data
  scheduleDatas.clear();
  classes.clear();
  attendances.clear();
  unUpadatedAttendances.clear();
  printTextLCD("Empty in-memory datas", 1);
  delay(2000);
}



void getOnGoingSchedule() {

  if(scheduleDatas.size() == 0){
    return;
  }

  for(ScheduleData& item : scheduleDatas){
    if(checkOnDate(item.dateStruct)){
      if(checkWithinInterval(item.startTimeStruct, item.endTimeStruct)){
        onGoingSchedule = &item;
        lcd.clear();
        printTextLCD("Get a schedule: " + String(item.scheduleID), 0);
        delay(2000);
        return;
      }
    }
  }
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



bool checkWithinInterval(const struct tm& startTime, const struct tm& endTime){
  uint32_t start = (startTime.tm_hour * 60 + startTime.tm_min) * 60 + startTime.tm_sec;
  uint32_t end = (endTime.tm_hour * 60 + endTime.tm_min) * 60 + endTime.tm_sec;
  uint32_t now = (hour_ * 60 + minute_) * 60 + second_;
  
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

  // if(startTime.tm_hour < hour_ && endTime.tm_hour > hour_){
  //   return true;
  // }
  // if(startTime.tm_hour == endTime.tm_hour){
  //   if(startTime.tm_min <= minute_ && endTime.tm_min >= minute_){
  //     return true;
  //   }
  // }
  // else{
  //   if(startTime.tm_hour == hour_ && startTime.tm_min <= minute_){
  //   return true;
  //   }
  //     if(endTime.tm_hour == hour_ && endTime.tm_min >= minute_){
  //     return true;
  //   }
  // }
  // return false;
}



void writeFingerprintTemplateToSensor(){
  lcd.clear();
  printTextLCD("Uploading fingerprint ...", 0);
  delay(500);

  uint8_t classID = onGoingSchedule->classID;

  for(const Class& item : classes){
    if(item.classID == classID){
      printTextLCD(" ", 1);
      printTextLCD("From class " + String(classID) + " have " + String(item.students.size()) + " students", 1);
      delay(2000);

      int totalUploadedFingerprint = 0;
      int count = 1;
      int totalFingerprint = 0;
      for(const Student& student : item.students){
        for(const std::string& fingerprintData : student.fingerprintTemplateData){
          totalFingerprint++;
          uint8_t fingerTemplate[512];
          memset(fingerTemplate, 0xff, 512);
          for (int i = 0; i < 512; i++) {
            // Extract the current pair of 2 characters
            char hexPair[2]; // Two characters + null terminator (array of characters always have null terminator is '\0')
            hexPair[0] = fingerprintData[2 * i];
            hexPair[1] = fingerprintData[2 * i + 1];
            // Convert the pair to a uint8_t value
            std::string hexPairString(hexPair, hexPair + 2);
            fingerTemplate[i] = convert_hex_to_binary(hexPairString);
          }

          if(finger.write_template_to_sensor(template_buf_size,fingerTemplate)){
            if(finger.storeModel(count) == FINGERPRINT_OK){
              Attendance attendance(count, onGoingSchedule->scheduleID, student.userID, student.studentCode);
              attendances.push_back(attendance);
              totalUploadedFingerprint++;
              count++;
            }
          }
        }
      }

      fingerprintIsStored = true;

      printTextLCD(" ", 1);
      printTextLCD("uploaded " + String(totalUploadedFingerprint) + "/" + totalFingerprint, 1);
      delay(2000);

      return;
    }
  }

  printTextLCD("No class found", 1);
  delay(500);
}



void addClass(std::vector<int>& classIds, const Class& newClass){
  for(const int classID : classIds){
    if(classID == newClass.classID){
      return;
    }
  }
  classes.push_back(newClass);
  classIds.push_back(newClass.classID);
}



void attendanceSession(uint32_t& duration){
  unsigned long timeStamp = millis() + duration * 1000;
  while(1){
    unsigned long now = millis();
    if(now > timeStamp){
      stopAttendanceSession();
      return;
    }

    if(!attedancesUpdated){
      updateAttendanceAgain();
    }

    if(displayAttendanceSession){
      lcd.clear();
      printTextLCD((onGoingSchedule->classCode + " - " + onGoingSchedule->subjectCode).c_str(), 0);
      printTextLCD((onGoingSchedule->startTime.substr(0, 5) + "-" + onGoingSchedule->endTime.substr(0, 5)).c_str(), 1);
      displayAttendanceSession = false;
    }

    // Scanning to mark attendance
    int16_t fingerId = getFingerprintIDez();
    if(fingerId > 0){
      auto is_matched = [fingerId](const auto& obj){ return obj.storedFingerID == fingerId; };
      auto it = std::find_if(attendances.begin(), attendances.end(), is_matched);
      if (it != attendances.end()) {

        // Check whether if user is already attended with others finger or not
        uint8_t scheduleId = it->scheduleID;
        std::string userId = it->userID;
        auto is_already_attended = [scheduleId, userId](const auto& obj){ return (obj.scheduleID == scheduleId && obj.userID == userId && obj.attended == true); };
        auto it_already_attended = std::find_if(attendances.begin(), attendances.end(), is_already_attended);
        if(it_already_attended != attendances.end()){
          printTextLCD(" ", 1);
          printTextLCD((it_already_attended->studentCode + " already attended").c_str(), 1);
          delay(2000);
        }
        //==================================================
        // If not attended with others finger, lets check attendance status of the current fingerprint
        else{
          if(it->attended == false){
            it->attended = true;
            it->attendanceTime = rtc.now();
            printTextLCD(" ", 1);
            printTextLCD((it->studentCode + " attended").c_str(), 1);
            updateAttendance(*it);
            delay(2000);
          }
          else{
            printTextLCD(" ", 1);
            printTextLCD((it->studentCode + " already attended").c_str(), 1);
            delay(2000);
          }
        }
      }
      displayAttendanceSession = true;
    }
    else if(fingerId == -1){
      printTextLCD(" ", 1);
      printTextLCD("Finger does not matched", 1);
      delay(2000);
      displayAttendanceSession = true;
    }
  }
}



void getDurationInSeconds(const struct tm& startTime, const struct tm& endTime, uint32_t& duration){
  uint32_t start = (startTime.tm_hour * 60 + startTime.tm_min) * 60 + startTime.tm_sec;
  uint32_t end = (endTime.tm_hour * 60 + endTime.tm_min) * 60 + endTime.tm_sec;
  int32_t timeSpan = end - start;

  if(timeSpan > 0){
    duration = timeSpan;
  }
  else{
    duration = 24 * 60 * 60 - start + end;
  }
}



void stopAttendanceSession() {
  onGoingSchedule = nullptr;
  finger.emptyDatabase();
  fingerprintIsStored = false;
}



void updateAttendance(Attendance& attendance) {

  // Get datetime string
  char buf[] = "YYYY-MM-DD hh:mm:ss";
  String dateTime = attendance.attendanceTime.toString(buf);
  dateTime[10] = 'T';

  int checkWifiStatus =  checkWifi();
  if(checkWifiStatus == 0) {
    AttendanceObject attedanceObject(attendance.scheduleID, 1, dateTime.c_str(), attendance.userID);
    unUpadatedAttendances.push_back(attedanceObject);
    attedancesUpdated = false; 
    return;
  }

  //2024-06-11T16:29:24
  //http://35.221.168.89/api/Attendance/update-attendance-status?scheduleID=5&attendanceStatus=3&attendanceTime=2024-06-11T16%3A29%3A24&studentID=fa00c1a6-0a14-435c-a421-08dc8640e68a
  String url = "http://" + String(SERVER_IP) + "/api/Attendance/update-attendance-status?attendanceStatus=1&scheduleID=" + String(attendance.scheduleID) + "&studentID=" + attendance.userID.c_str() + "&attendanceTime=" + dateTime;
  http.begin(client, url);
  int httpCode = http.PUT("");
  http.end();
}



void updateAttendanceAgain() {
  int checkWifiStatus =  checkWifi();
  if(checkWifiStatus == 0) {
    return;
  }
  if(unUpadatedAttendances.empty()){
    attedancesUpdated = true;
    return;
  }

  //http://35.221.168.89/api/Attendance/update-list-student-status
  String url = "http://" + String(SERVER_IP) + "/api/Attendance/update-list-student-status";

  // Create a payload string
  JSONVar attendanceArray;
  uint8_t index = 0;
  for(AttendanceObject& item : unUpadatedAttendances){
    JSONVar attendanceObject;
    attendanceObject["ScheduleID"] = item.ScheduleID;
    attendanceObject["StudentID"] = item.StudentID.c_str();
    attendanceObject["AttendanceStatus"] = item.AttendanceStatus;
    attendanceObject["AttendanceTime"] = item.AttendanceTime.c_str();
    attendanceArray[index++] = attendanceObject;
  }
  String payload = JSON.stringify(attendanceArray);

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PUT(payload);
  if(httpCode == HTTP_CODE_OK){
    unUpadatedAttendances.clear(); // Makes it empty
    attedancesUpdated = true;
  }

  http.end();
}


