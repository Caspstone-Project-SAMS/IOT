#include <Adafruit_Fingerprint.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>

#define SERVER_IP "35.221.168.89"

#ifndef STASSID
#define STASSID "Nhim"
#define STAPSK "1357924680"
#endif

#define Finger_Rx 0 //D3 in ESP8266 is GPIO0
#define Finger_Tx 2 //D4 is GPIO2
SoftwareSerial mySerial(Finger_Rx, Finger_Tx);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

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
        JSONVar myArray = JSON.parse(payload);
        
        String fingerprintTemplates[myArray.length()];
        for(int i = 0; i < myArray.length(); i++) {
          Serial.print("Template: "); Serial.println(myArray[i]);
          fingerprintTemplates[i] = (const char*)myArray[i];
        }
        Serial.println(">>");

        //Again
        Serial.println("Again");
        for(int i = 0; i < sizeof(fingerprintTemplates); i++){
          Serial.println(fingerprintTemplates[i]);
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }

  delay(3000);
}
