#include "FingerprintSensorService.h"

SoftwareSerial mySerial1(FINGER_RX, FINGER_TX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial1);
  
// Constructor
FingerprintSensorClass::FingerprintSensorClass(){
}


// Destructor
FingerprintSensorClass::~FingerprintSensorClass(){
}


bool FingerprintSensorClass::connectFingerprintSensor(){
  ECHOLN(F("\n\nAdafruit Fingerprint sensor enrollment"));
  finger.begin(57600);
  if (finger.verifyPassword()) {
    ECHOLN(F("Found fingerprint sensor!"));
    return true;
  } else {
    ECHOLN(F("Did not find fingerprint sensor :("));
    return false;
  }
}


uint8_t FingerprintSensorClass::getFingerprintEnroll(){
  // Scan
  int p = -1;
  ECHOLN("Waiting for valid finger to enroll");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      ECHOLN("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      ECHO(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ECHOLN("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      ECHOLN("Imaging error");
      break;
    default:
      ECHOLN("Unknown error");
      break;
    }
  }

  // Get image 1
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      ECHOLN("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      ECHOLN("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      ECHOLN("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      ECHOLN("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      ECHOLN("Could not find fingerprint features");
      return p;
    default:
      ECHOLN("Unknown error");
      return p;
  }

  // Remove finger
  ECHOLN("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }


  // Place finger again
  p = -1;
  ECHOLN("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      ECHOLN("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      ECHO(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ECHOLN("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      ECHOLN("Imaging error");
      break;
    default:
      ECHOLN("Unknown error");
      break;
    }
  }

  // Get image 2
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      ECHOLN("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      ECHOLN("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      ECHOLN("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      ECHOLN("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      ECHOLN("Could not find fingerprint features");
      return p;
    default:
      ECHOLN("Unknown error");
      return p;
  }

  // Create model
  ECHOLN("Creating model for fingerprint");
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    ECHOLN("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    ECHOLN("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    ECHOLN("Fingerprints did not match");
    return p;
  } else {
    ECHOLN("Unknown error");
    return p;
  }

  ECHOLN("Created successfully");
  delay(2000);

  return true;
}



uint8_t FingerprintSensorClass::scanFinger(){
  return finger.getImage();
}

uint8_t FingerprintSensorClass::getImage1(){
  return finger.image2Tz(1);
}

uint8_t FingerprintSensorClass::getImage2(){
  return finger.image2Tz(2);
}

uint8_t FingerprintSensorClass::createModel(){
  return finger.createModel();
}



String FingerprintSensorClass::getFingerprintTemplate(){
  ECHO("Attempting to get fingerprint template...\n");
  uint8_t p = finger.getModel();
  switch (p) {
    case FINGERPRINT_OK:
      ECHO("Template "); ECHOLN(" transferring:");
      break;
    default:
      ECHO("Unknown error "); ECHOLN(p);
      return "";
  }

  // one data packet is 139 bytes. in one data packet, 11 bytes are 'usesless' :D
  uint8_t bytesReceived[556]; // 4 data packets
  memset(bytesReceived, 0xff, 556);
  uint32_t starttime = millis();
  int byteIndex = 0;
  while (byteIndex < 556 && (millis() - starttime) < 5000) {
    if (mySerial1.available()) {
      bytesReceived[byteIndex++] = mySerial1.read();
    }
  }
  ECHO(byteIndex); ECHOLN(" bytes read.");
  ECHOLN("Decoding packet...");

  uint8_t fingerTemplate[512]; // the real template
  memset(fingerTemplate, 0xff, 512);

  for(int m=0;m<4;m++){ //filtering data packets
    uint8_t stat=bytesReceived[(m*(128+11))+6];
    if( stat!= FINGERPRINT_DATAPACKET && stat!= FINGERPRINT_ENDDATAPACKET){
      ECHOLN("Bad fingerprint_packet");
      while(1){
        delay(1);
      }
    }
    memcpy(fingerTemplate + (m*128), bytesReceived + (m*(128+11))+9, 128); 
  }

  for(int i = 0; i < 512; i++)
  {
    ECHO(fingerTemplate[i]); ECHO(" ");
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
  ECHOLN();
  ECHO("Fingerprint template: " ); ECHOLN(fingerprintTemplate);
  return fingerprintTemplate;
}


uint8_t FingerprintSensorClass::deleteFingerprint(uint8_t id){
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    ECHOLN("Delete successfully!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    ECHOLN("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    ECHOLN("Could not delete in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    ECHOLN("Error writing to flash");
  } else {
    ECHO("Unknown error: 0x"); Serial.println(p, HEX);
  }

  return p;
}


void FingerprintSensorClass::emptyDatabase(){
  finger.emptyDatabase();
}


// Convert fingerprint template in a string of hexa to binary (8 bit unsigned integer)
uint8_t FingerprintSensorClass::convert_hex_to_binary(std::string hexString){
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


bool FingerprintSensorClass::uploadFingerprintTemplate(const std::string& fingerData, uint16_t storeModelID){
  uint8_t fingerTemplate[512];
  memset(fingerTemplate, 0xff, 512);
  for (int i = 0; i < 512; i++) {
    char hexPair[2];
    hexPair[0] = fingerData[2 * i];
    hexPair[1] = fingerData[2 * i + 1];
    std::string hexPairString(hexPair, hexPair + 2);
    fingerTemplate[i] = convert_hex_to_binary(hexPairString);
  }

  if(finger.write_template_to_sensor(TEMPLATE_BUF_SIZE,fingerTemplate)){
    if(finger.storeModel(storeModelID) == FINGERPRINT_OK){
      return true;
    }
  }

  return false;
}


FingerprintSensorClass FINGERPSensor;