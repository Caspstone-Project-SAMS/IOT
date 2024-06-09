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
    int id;
    std::string fingerprintTemplate;
    int fingerId;
};
//=======================================================================



//===========================Memory variables declaration here===========================
std::map<int, int> FingerMap;

// Fingerprint sensor
SoftwareSerial mySerial(Finger_Rx, Finger_Tx);
int template_buf_size = 512;
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// DateTime information, NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
char Time[] = "TIME:00:00:00";
char Date[] = "DATE:00/00/2000";
char CDateTime[] = "0000-00-00T00:00:00";
byte last_second, second_, minute_, hour_, day_, month_;
int year_;

// DS1307 real-time
RTC_DS1307 rtc;
bool haveRTC = false;

// LCD I2C
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

//=======================================================================

//========================Connecting==================================
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

  // Get Time from NTP server
  Serial.println("\nGet Time from server!");
  Serial.println("Update time");
  updateDateTime();
  Serial.println(Time);
  Serial.println(Date);

  // Update Time of RTC object (DS1307)
  if(haveRTC){
    rtc.adjust(DateTime(year_, month_, day_, hour_, minute_, second_));
  }

  lcd.clear();
  printTextLCD("Setup done!!!", 0);
  delay(2000);
}

void loop() {
  Serial.println("\n\nUpdate Time again!");
  Serial.println("Press 'Y' key to Update, other keys to Cancel");
  while(1) {
    if(Serial.available())
    {
      if(Serial.read() == 'Y')
      {
        updateDateTime();
        Serial.println(Time);
        Serial.println(Date);
        if(haveRTC){
          rtc.adjust(DateTime(year_, month_, day_, hour_, minute_, second_));
          lcd.clear();
          printTextLCD("Update time!!", 0);
        }
        break;
      }
      else
      {
        break;
      }
    }
  }

  Serial.println("\n\nReady to download fingerprint!");
  Serial.println("Press 'Y' key to continue");
  while(1) {
    if (Serial.available() && (Serial.read() == 'Y'))
    {
      break;
    }
  }

  // Empty the database
  Serial.println("\n\n===========================");
  Serial.println("Attempting to empty the database");
  finger.emptyDatabase();
  Serial.println("Database is empty");
  lcd.clear();
  printTextLCD("Empty database", 0);
  delay(1000);

  if((WiFi.status() == WL_CONNECTED)) {
    lcd.clear();
    printTextLCD("Loading info...", 0);

    // Get fingerprint from server
    WiFiClient client;
    HTTPClient http;

    http.begin(client, "http://" SERVER_IP "/api/Hello/fingerprint"); 

    Serial.print("Download fingerprint templates from server using [HTTP] GET...\n");
    int httpCode = http.GET();

    if(httpCode > 0) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // Server return OK
      if (httpCode == HTTP_CODE_OK) {
        lcd.clear();
        printTextLCD("Get OK", 0);

        String payload = http.getString();
        Serial.println("received payload:\n<<");

        // Data get from API
        Serial.println("Working with Json String.......");
        JSONVar fingerDataArray = JSON.parse(payload);

        // Store fingerprint data in microcontroller
        Serial.println("Create an array of FingerData to store list of fingerprint datas");
        FingerData fingerDatas[fingerDataArray.length()];

        Serial.println("Iterate through result get form API and store it to the array of FingerData");
        for(int i = 0; i < fingerDataArray.length(); i++) {
          if(JSON.typeof(fingerDataArray[i])=="object"){
            FingerData fingerData;
            fingerData.id = fingerDataArray[i]["id"];
            fingerData.fingerprintTemplate = (const char*)fingerDataArray[i]["finger"];
            fingerDatas[i] = fingerData;
            Serial.print("Get "); Serial.printf("%d", i); Serial.println();
            delay(500);
          }
        }

        delay(1000);

        // Display again
        Serial.println("Display template again");
        const int fingerDataArrayLength = sizeof(fingerDatas) / sizeof(fingerDatas[0]);
        Serial.printf("Size of array: %d\n", fingerDataArrayLength);
        for(int i = 0; i < fingerDataArrayLength; i++){
          Serial.printf("Fingerprint Template %d: ", fingerDatas[i].id); Serial.println(fingerDatas[i].fingerprintTemplate.c_str());
          delay(1000);
        }

        delay(2000);

        //Again and write to sensor
        Serial.println("Display template in decimal format (which represent 8-bits/1-byte data)");
        Serial.println("Then write to sensor\n");
        delay(2000);
        for(int i = 0; i < fingerDataArrayLength; i++){

          Serial.println("\nConverting template in 2-digits hexa string to 8-bits interger.....");
          std::string fingertemplate = fingerDatas[i].fingerprintTemplate;
          uint8_t fingerTemplate[512];
          memset(fingerTemplate, 0xff, 512);
          for (int i = 0; i < 512; i++) {
            // Extract the current pair of characters
            char hexPair[2]; // Two characters + null terminator
            hexPair[0] = fingertemplate[2 * i];
            hexPair[1] = fingertemplate[2 * i + 1];
            // Convert the pair to a uint8_t value
            std::string hexPairString(hexPair, hexPair + 2);
            fingerTemplate[i] = convert_hex_to_binary(hexPairString);
          }

          Serial.print("Template in decimal format: ");
          for (int i=0; i<512; i++)
          {
            Serial.print(fingerTemplate[i]); Serial.print(" ");
          }

          //Write to sensor's buffer
          if (finger.write_template_to_sensor(template_buf_size,fingerTemplate)) { //telling the sensor to download the template data to it's char buffer from upper computer (this microcontroller's "fingerTemplate" buffer)
            Serial.println("now writing to sensor buffer 1...");
          } else {
            Serial.println("writing to sensor failed");
            return;
          }

          delay(500);
          Serial.println("Storing......");
          delay(500);

          if (finger.storeModel(i+1) == FINGERPRINT_OK) { //saving the template against the ID you entered or manually set
            Serial.print("Successfully stored against ID#");Serial.println(i);
            lcd.clear();
            printTextLCD("Store " + String(i), 0);
            fingerDatas[i].fingerId = i+1;
            FingerMap[i+1] = fingerDatas[i].id;
          } else {
            Serial.println("Storing error");
            return ;
          }
        }
      }

    } 
    else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      printTextLCD("Got error", 0);
    }

    delay(2000);

    http.end();
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

      updateDS1307DateTime();

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

  // found a match!
  Serial.print("\nFound ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

// Update DateTime information on memory
void updateDateTime() {
  timeClient.update();
  unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
  second_ = second(unix_epoch);
  if(last_second != second_){
    minute_ = minute(unix_epoch);
    hour_   = hour(unix_epoch);
    day_    = day(unix_epoch);
    month_  = month(unix_epoch);
    year_   = year(unix_epoch);

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

    last_second = second_;
  }
}

// Update DateTime of DS1307
void updateDS1307DateTime(){
  DateTime now = rtc.now();

  second_ = now.second();
  minute_ = now.minute();
  hour_   = now.hour();
  day_    = now.day();
  month_  = now.month();
  year_   = now.year();

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