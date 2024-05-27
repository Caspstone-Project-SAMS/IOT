#include <Adafruit_Fingerprint.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <string>

#define SERVER_IP "35.221.168.89"

#ifndef STASSID
#define STASSID "Nhim"
#define STAPSK "1357924680"
#endif

#define Finger_Rx 0 //D3 in ESP8266 is GPIO0
#define Finger_Tx 2 //D4 is GPIO2
SoftwareSerial mySerial(Finger_Rx, Finger_Tx);
int template_buf_size = 512;
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

class FingerData {
  public:
    int id;
    std::string fingerprintTemplate;
    int fingerId;
};

void setup() {
  Serial.begin(9600);
  delay(100);
  
  // Connect to R308 fingerprint sensor
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

  // Connet to wifi
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println("================================");
  Serial.println("Connecting to Wifi");

  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected! IP address: "); Serial.println(WiFi.localIP());
}

void loop() {
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

  delay(1000);

  if((WiFi.status() == WL_CONNECTED)) {
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
            fingerDatas[i].fingerId = i+1;
          } else {
            Serial.println("Storing error");
            return ;
          }
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }

  // Scan fingerprint
  Serial.println("Ready to scan fingerprint");
  while(1){
    getFingerprintIDez();
    delay(50);
    Serial.print(".");
  }

  delay(3000);
}

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

int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  Serial.print("\nFound ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}
