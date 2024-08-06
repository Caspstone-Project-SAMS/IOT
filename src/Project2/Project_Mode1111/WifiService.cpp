#include "WiFiClient.h"
#include "IPAddress.h"
#include "ESP8266WiFiType.h"
#include "WifiService.h"

ESP8266WiFiClass* WiFii = nullptr;
ESP8266WiFiMulti* WiFiMulti = nullptr;

// Constructor
WifiServiceClass::WifiServiceClass(){
}


// Destructor
WifiServiceClass::~WifiServiceClass(){
}


void WifiServiceClass::setupWiFi(ESP8266WiFiClass &Wifi, ESP8266WiFiMulti &wifiMulti){
  WiFii = &Wifi;
  WiFiMulti = &wifiMulti;
  // Serial.print("Address of WiFi object (2): "); Serial.println(reinterpret_cast<uintptr_t>(&Wifi), HEX);
  // Serial.print("Address of WiFi object (3): "); Serial.println(reinterpret_cast<uintptr_t>(WiFii), HEX);
  // Serial.print("Address of WiFi object (4): "); Serial.println(reinterpret_cast<uintptr_t>(&WiFi), HEX);
}


int WifiServiceClass::connect(){
  if(WiFii){
    ECHOLN("[WifiServiceClass][connect] Read wifi SSID and PASS from EEPROM");
    String ssid = EEPROMH.read(EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
    String pass = EEPROMH.read(EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);
    return connect(ssid, pass);
  }
  ECHOLN("[WifiServiceClass][connect] WiFi object not found");
  return CONNECT_TIMEOUT;
}


int WifiServiceClass::connect(const String &ssid, const String &pass){
  ECHOLN("[WifiService][connect] Open STA....");
  ECHO("[WifiService][connect] Wifi connect to: ");
  ECHOLN(ssid);
  ECHO("[WifiService][connect] With pass: ");
  ECHOLN(pass);
  WiFii->softAPdisconnect();
  WiFii->disconnect();
  WiFii->mode(WIFI_STA);
  delay(100);
  WiFii->begin(ssid.c_str(), pass.c_str());
  WiFiMulti->addAP(ssid.c_str(), pass.c_str());

  ECHOLN("Waiting for Wifi to connect");
  int c = 0;
  while(c < 30){
    if(WiFii->status() == WL_CONNECTED && WiFiMulti->run() == WL_CONNECTED){
      ECHOLN("Wifi connected!");
      ECHO("Local IP: ");
      ECHOLN(WiFii->localIP());
      return CONNECT_OK;
    }
    delay(500);
    ECHO(".");
    c++;
  }

  ECHOLN("");
  ECHOLN("Connection timed out");
  return CONNECT_TIMEOUT;
}


int WifiServiceClass::connectNewWifi(const String &ssid, const String &pass){
  ECHOLN("[WifiService][connect] Open STA....");
  ECHO("[WifiService][connect] Wifi connect to: ");
  ECHOLN(ssid);
  ECHO("[WifiService][connect] With pass: ");
  ECHOLN(pass);

  WiFii->softAPdisconnect();
  WiFii->disconnect();
  WiFii->mode(WIFI_STA);
  delay(100);
  WiFii->begin(ssid.c_str(), pass.c_str());

  ECHOLN("Waiting for Wifi to connect");
  int c = 0;
  while(c < 40){
    if(WiFii->status() == WL_CONNECTED){
      ECHOLN("Wifi connected!");
      ECHO("Local IP: ");
      ECHOLN(WiFii->localIP());
      storeWifi(ssid, pass);
      setupAPWifi();
      return CONNECT_OK;
    }
    delay(500);
    ECHO(".");
    c++;
  }

  ECHOLN("Connection timed out");
  setupAPWifi();
  return CONNECT_TIMEOUT;
}


void WifiServiceClass::storeWifi( const String &ssid, const String &pass){
  EEPROMH.write(ssid, EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
  EEPROMH.write(pass, EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);
  EEPROMH.commit();
}


void WifiServiceClass::setupAP(){
  ECHOLN("[WifiService][setupAP] Open AP....");
  WiFii->softAPdisconnect();
  WiFii->disconnect();
  delay(3000);
  WiFii->mode(WIFI_AP);

  WiFii->softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  ECHO("[WifiService][setupAP] Connect to wifi:");
  ECHOLN(WIFI_AP_SSID);
  ECHO("[WifiService][setupAP] Password:");
  ECHOLN(WIFI_AP_PASSWORD);

  ECHOLN("[WifiService][setupAP] Softap is running!");
  IPAddress myIP = WiFii->softAPIP();
  ECHO("[WifiService][setupAP] IP address: ");
  ECHOLN(myIP);
}


void WifiServiceClass::beginWifi(){
  String ssid = EEPROMH.read(EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
  String pass = EEPROMH.read(EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);
  WiFii->begin(ssid.c_str(), pass.c_str());
}


bool WifiServiceClass::checkWifi(){
  return WiFii->isConnected();
}


void WifiServiceClass::setupAPWifi(){
  WiFii->softAPdisconnect();
  WiFii->disconnect();
  WiFii->mode(WIFI_AP);
  delay(100);
  WiFii->softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
}

WifiServiceClass WifiService;