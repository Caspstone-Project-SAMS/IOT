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

//=========================Macro define=============================
#define SERVER_IP "35.221.168.89"

#ifndef STASSID
#define STASSID "Nhim"
#define STAPSK "1357924680"
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
}

class Student {
  public:
    std::string studentName;
    std::string userID;
    std::string studentCode;
    std::string fingerprintTemplateData;
}

class Class {
  public:
    uint8_t classID;
    std::string classCode;
    std::vector<Student> students;

    Class(std::string code){
      classCode = code;
    }
}

class Attendance {
  public:
    uint8_t storedFingerID;
    uint8_t scheduleID;
    std::string userID;
    DateTime attendanceTime;
    bool attended;

    Attendance(uint8_t storedFingerId, uint8_t scheduleId, std::string userId) {
      storedFingerID = storedFingerId;
      scheduleID = scheduleId;
      userID = userId;
      attended = false;
    }
}
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
char Time[] = "TIME:00:00:00";
char Date[] = "DATE:00/00/2000";
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
    if(!rtc.begin()) {
      Serial.println("Couldn't find RTC");
      Serial.println("Do you want to reconnect RTC");
      Serial.println("'Y' to yes, others to no");
      while(1){
        if(Serial.available()){
          if(Serial.read() == 'Y'){
            break;
          }
          else{
            connectToRTC = true;
            break;
          }
        }
      }
    }
    else{
      connectToRTC = true;
      haveRTC = true;
    }
  }
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
  
  int checkUpdateDateTime = checkUpdateDateTime();
  if(checkUpdateDateTime == 1){
    setupDS1307DateTime();
  }

  // Get/Update on going schedule in each loop
  getOnGoingSchedule();

  // Store fingerprint to sensor if not write and (on-going schedule is avialable - check inside the called method)
  if(!fingerprintIsStored){
    writeFingerprintTemplateToSensor();
  }

  // if((WiFi.status() == WL_CONNECTED)) {
  //   lcd.clear();
  //   printTextLCD("Loading info...", 0);

  //   // Get fingerprint from server
  //   http.begin(client, "http://" SERVER_IP "/api/Hello/fingerprint"); 

  //   Serial.print("Download fingerprint templates from server using [HTTP] GET...\n");
  //   int httpCode = http.GET();

  //   if(httpCode > 0) {
  //     Serial.printf("[HTTP] GET... code: %d\n", httpCode);

  //     // Server return OK
  //     if (httpCode == HTTP_CODE_OK) {
  //       lcd.clear();
  //       printTextLCD("Get OK", 0);

  //       String payload = http.getString();
  //       Serial.println("received payload:\n<<");

  //       // Data get from API
  //       Serial.println("Working with Json String.......");
  //       JSONVar fingerDataArray = JSON.parse(payload);

  //       // Store fingerprint data in microcontroller
  //       Serial.println("Create an array of FingerData to store list of fingerprint datas");
  //       FingerData fingerDatas[fingerDataArray.length()];

  //       Serial.println("Iterate through result get form API and store it to the array of FingerData");
  //       for(int i = 0; i < fingerDataArray.length(); i++) {
  //         if(JSON.typeof(fingerDataArray[i])=="object"){
  //           FingerData fingerData;
  //           fingerData.id = fingerDataArray[i]["id"];
  //           fingerData.fingerprintTemplate = (const char*)fingerDataArray[i]["finger"];
  //           fingerDatas[i] = fingerData;
  //           Serial.print("Get "); Serial.printf("%d", i); Serial.println();
  //           delay(500);
  //         }
  //       }

  //       delay(1000);

  //       // Display again
  //       Serial.println("Display template again");
  //       const int fingerDataArrayLength = sizeof(fingerDatas) / sizeof(fingerDatas[0]);
  //       Serial.printf("Size of array: %d\n", fingerDataArrayLength);
  //       for(int i = 0; i < fingerDataArrayLength; i++){
  //         Serial.printf("Fingerprint Template %d: ", fingerDatas[i].id); Serial.println(fingerDatas[i].fingerprintTemplate.c_str());
  //         delay(1000);
  //       }

  //       delay(2000);

  //       //Again and write to sensor
  //       Serial.println("Display template in decimal format (which represent 8-bits/1-byte data)");
  //       Serial.println("Then write to sensor\n");
  //       delay(2000);
  //       for(int i = 0; i < fingerDataArrayLength; i++){

  //         Serial.println("\nConverting template in 2-digits hexa string to 8-bits interger.....");
  //         std::string fingertemplate = fingerDatas[i].fingerprintTemplate;


  //         uint8_t fingerTemplate[512];
  //         memset(fingerTemplate, 0xff, 512);
  //         for (int i = 0; i < 512; i++) {
  //           // Extract the current pair of characters
  //           char hexPair[2]; // Two characters + null terminator
  //           hexPair[0] = fingertemplate[2 * i];
  //           hexPair[1] = fingertemplate[2 * i + 1];
  //           // Convert the pair to a uint8_t value
  //           std::string hexPairString(hexPair, hexPair + 2);
  //           fingerTemplate[i] = convert_hex_to_binary(hexPairString);
  //         }

  //         Serial.print("Template in decimal format: ");
  //         for (int i=0; i<512; i++)
  //         {
  //           Serial.print(fingerTemplate[i]); Serial.print(" ");
  //         }

  //         //Write to sensor's buffer
  //         if (finger.write_template_to_sensor(template_buf_size,fingerTemplate)) { //telling the sensor to download the template data to it's char buffer from upper computer (this microcontroller's "fingerTemplate" buffer)
  //           Serial.println("now writing to sensor buffer 1...");
  //         } else {
  //           Serial.println("writing to sensor failed");
  //           return;
  //         }

  //         delay(500);
  //         Serial.println("Storing......");
  //         delay(500);

  //         if (finger.storeModel(i+1) == FINGERPRINT_OK) { //saving the template against the ID you entered or manually set
  //           Serial.print("Successfully stored against ID#");Serial.println(i);
  //           lcd.clear();
  //           printTextLCD("Store " + String(i), 0);
  //           fingerDatas[i].fingerId = i+1;
  //           FingerMap[i+1] = fingerDatas[i].id;
  //         } else {
  //           Serial.println("Storing error");
  //           return ;
  //         }
  //       }
  //     }

  //   } 
  //   else {
  //     Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  //     printTextLCD("Got error", 0);
  //   }

  //   delay(2000);

  //   http.end();
  // }

  uint8_t scheduleID;
    uint8_t classID;
    std::string date;
    uint8_t slotNumber;
    std::string classCode;
    std::string subjectCode;
    std::string roomName;
    std::string startTime;
    std::string endTime;

  // Fingerprint scanning if in slot
  if(onGoingSchedule!=nullptr){
    
  }

  // Scan fingerprint
  String classCode = "NET1604";
  String subjectCode = "SWP391";
  String date = "09/06/2024";
  String startTime = "14:00";
  String endTime = "16:00";
  String mssv = "SE161404";
  String fullName = "Le Dang Khoa";

  Serial.println("Ready to scan fingerprint");
  delay(50);
  while(1){
    lcd.clear();
    printTextLCD(classCode + " - " + subjectCode, 0);
    printTextLCD(date + "  " + startTime + " - " + endTime, 1);
    int fingerId = getFingerprintIDez();
    if(fingerId > 0)
    {
      Serial.print("Attending fingerprint #"); Serial.println(fingerId);
      auto it = FingerMap.find(fingerId);
      int id = it->second;
      Serial.println(id);
      Serial.println(WiFi.status());

      getDS1307DateTime();

      if(WiFi.status() == WL_CONNECTED){
        WiFiClient client;
        HTTPClient http;

        const std::string url = std::string("http://") + SERVER_IP + "/api/Hello/attendance/" + std::to_string(id) + "?dateTime=" + CDateTime;
        Serial.println(url.c_str());
        delay(5000);
        http.begin(client, url.c_str()); 
        int httpCode = http.PUT("");
        Serial.println(httpCode);
        if(httpCode > 0){
          if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println("Attendance successfully: " + payload);
            lcd.clear();
            printTextLCD(mssv + " - " + fullName + " attended", 1);
          }
        }
        else{
          Serial.printf("[HTTP] PUT... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
      }
    }
    else if(fingerId == 0) {
      lcd.clear();
      printTextLCD("Finger does not match", 1);
    }
    delay(2000);
    Serial.print(".");
  }

  delay(3000);
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
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;
  lcd.clear();
  printTextLCD("Scanning", 1);

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return 0;
  lcd.clear();
  printTextLCD("Finger matched", 1);
  delay(1000);

  // found a match!
  Serial.print("\nFound ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
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
      delay(300);
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
  String startDate = "2024-06-11";
  String endDate = "2024-06-13";
  String url = "http://" + SERVER_IP + "/api/Schedule?lecturerId=" + lecturerId + "&semesterId=" + semesterId + "&startDate=" + startDate + "&endDate=" + endDate;
  //http://35.221.168.89/api/Schedule?lecturerId=a829c0b5-78dc-4194-a424-08dc8640e68a&semesterId=2&startDate=2024-11-06&endDate=2024-12-06

  int checkWifi = checkWifi();
  if(checkWifi == 0){
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
    classes.push_back(newClass);

    totalGet++;
    lcd.clear();
    printTextLCD("Get schedule " + (i+1), 0);
    delay(600);
  }

  lcd.clear();
  printTextLCD("Total get: " + totalGet, 0);
  delay(2000);

  http.end();
}



void getStudent(){
  lcd.clear();
  printTextLCD("Get students information", 0);
  delay(600);

  int totalGet = 0;

  String url = "http://" + SERVER_IP + "/api/Student/get-students-by-classId?isModule=true&classID=";
  //http://35.221.168.89/api/Student/get-students-by-classId?classID=1&isModule=true
  int checkWifi = checkWifi();
  if(checkWifi == 0){
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
    printTextLCD("Class " + item.classCode.c_str(), 0);
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
    JSONVar studentDataArray = JSON.parse(payload);

    // Check whether if data is in correct format
    if(JSON.typeof(studentDataArray)!="array"){
      lcd.clear();
      printTextLCD("Invalid data format!!!", 0);
      delay(1000);
      return;
    }

    std::vector<Student> studentInClass;
    for(int i = 0; i < studentDataArray.length(); i++) {
      Student student;
      student.studentName = (const char*)studentDataArray[i]["studentName"];
      student.userID = (const char*)studentDataArray[i]["UserID"];
      student.studentCode = (const char*)studentDataArray[i]["StudentCode"];
      student.fingerprintTemplateData = (const char*)studentDataArray[i]["FingerprintTemplateData"];
      studentInClass.push_back(student);
    }
    item.students = studentInClass;
    totalGet++;
  }

  lcd.clear();
  printTextLCD("Get students in " + String(totalGet) + " classes", 0);
  delay(2000);

  http.end();
}



int checkWifi(){
  for(int i = 1; i<= 5; i++){
    if(WiFi.status() == WL_CONNECTED){
      return 1;
    }
    lcd.clear();
    printTextLCD("Attempting to connect wifi: " + i, 0);
    WiFi.begin(STASSID, STAPSK);
    delay(500);
  }
  printTextLCD("Connect failed");
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
  delay(1000);

  // Empty data
  scheduleDatas.clear();
  classes.clear();
  attendances.clear();
  lcd.clear();
  printTextLCD("Empty in-memory datas", 0);
  delay(1000);
}



void getOnGoingSchedule() {

  if(scheduleDatas.size() == 0){
    return;
  }

  getDS1307DateTime();

  for(const ScheduleData& item : scheduleDatas){
    // const char* date = item.date.c_str();
    // const char* startTime = item.startTime.c_str();
    // const char* endTime = item.endTime.c_str();

    // // Convert Date string to Struct tm
    // struct tm dateStruct;
    // strptime(date, dateFormat, &tmStruct);
    // if(checkOnDate(dateStruct)){
    //     // Convert time string to Struct tm (startTime and endTime)
    //     struct tm startTimeStruct, endTimeStruct;
    //     strptime(startTime, timeFormat, &startTimeStruct);
    //     strptime(endTime, timeFormat, &endTimeStruct);
    //     if(checkWithinInterval(startTimeStruct, endTimeStruct)){
    //       onGoingSchedule = &item;
    //       lcd.clear();
    //       printTextLCD("Get a schedule: " + item.scheduleID, 0)
    //       return;
    //     }
    // }
    if(checkOnDate(item.dateStruct)){
      if(checkWithinInterval(item.startTimeStruct, item.endTimeStruct)){
        onGoingSchedule = &item;
        lcd.clear();
        printTextLCD("Get a schedule: " + item.scheduleID, 0);
        delay(1000);
        return;
      }
    }
  }

  onGoingSchedule = nullptr;
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
  if(startTime.tm_hour < hour_ && endTime.tm_hour > hour_){
    return true;
  }
  if(startTime.tm_hour == hour_ && startTime.tm_min <= minute_){
    return true;
  }
  if(endTime.tm_hour == hour_ && endTime.tm_min >= minute_){
    return true;
  }
  return false;
}



void writeFingerprintTemplateToSensor(){
  if(onGoingSchedule == nullptr){
    return;
  }

  lcd.clear();
  printTextLCD("Uploading fingerprint ...", 0);
  delay(500);

  uint8_t classID = onGoingSchedule.classID;

  for(const Class& item : classes){
    if(item.classID == classID){
      printTextLCD("From class " + classID, 1);
      delay(500);

      int totalUploadedFingerprint = 0;
      for(const Student& student : item.students){
        int i = 0;
        uint8_t fingerTemplate[512];
        memset(fingerTemplate, 0xff, 512);
        for (int i = 0; i < 512; i++) {
          // Extract the current pair of 2 characters
          char hexPair[2]; // Two characters + null terminator (array of characters always have null terminator is '\0')
          hexPair[0] = student.fingerprintTemplateData[2 * i];
          hexPair[1] = student.fingerprintTemplateData[2 * i + 1];
          // Convert the pair to a uint8_t value
          std::string hexPairString(hexPair, hexPair + 2);
          fingerTemplate[i] = convert_hex_to_binary(hexPairString);
        }
        if(finger.write_template_to_sensor(template_buf_size,fingerTemplate)){
          if(finger.storeModel(++i) == FINGERPRINT_OK){
            totalUploadedFingerprint++;
            Attendance attendance(i, onGoingSchedule.scheduleID, student.userID);
            attendances.push_back(attendance);
          }
        }
      }

      printTextLCD(" ", 1);
      printTextLCD("uploaded " + totalUploadedFingerprint + "/" + item.students.size(), 1);
      delay(2000);

      return;
    }
  }

  printTextLCD("No class found", 1);
  delay(500);
}




