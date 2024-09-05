#include "WiFiClient.h"
#include "IPAddress.h"
#include "ESP8266WiFiType.h"
#include "WifiService.h"

// Constructor
WifiServiceClass::WifiServiceClass(){
}


// Destructor
WifiServiceClass::~WifiServiceClass(){
}


int WifiServiceClass::connect(){
  ECHOLN("[WifiServiceClass][connect] Read wifi SSID and PASS from EEPROM");
  String ssid = EEPROMH.read(EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
  String pass = EEPROMH.read(EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);
  return connect(ssid, pass);
}


int WifiServiceClass::connect(const String &ssid, const String &pass){
  ECHOLN("[WifiService][connect] Open STA....");
  ECHO("[WifiService][connect] Wifi connect to: ");
  ECHOLN(ssid);
  ECHO("[WifiService][connect] With pass: ");
  ECHOLN(pass);
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());

  ECHOLN("Waiting for Wifi to connect");
  int c = 0;
  while(c < 35){
    if(WiFi.status() == WL_CONNECTED){
      ECHOLN("Wifi connected!");
      ECHO("Local IP: ");
      ECHOLN(WiFi.localIP());
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

  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());

  ECHOLN("Waiting for Wifi to connect");
  int c = 0;
  while(c < 30){
    if(WiFi.status() == WL_CONNECTED){
      ECHOLN("Wifi connected!");
      ECHO("Local IP: ");
      ECHOLN(WiFi.localIP());
      storeWifi(ssid, pass);
      setupAPWifi();
      return CONNECT_OK;
    }
    delay(300);
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
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(3000);
  WiFi.mode(WIFI_AP);

  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  ECHO("[WifiService][setupAP] Connect to wifi:");
  ECHOLN(WIFI_AP_SSID);
  ECHO("[WifiService][setupAP] Password:");
  ECHOLN(WIFI_AP_PASSWORD);

  ECHOLN("[WifiService][setupAP] Softap is running!");
  IPAddress myIP = WiFi.softAPIP();
  ECHO("[WifiService][setupAP] IP address: ");
  ECHOLN(myIP);
}


void WifiServiceClass::beginWifi(){
  String ssid = EEPROMH.read(EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
  String pass = EEPROMH.read(EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);
  WiFi.begin(ssid.c_str(), pass.c_str());
}


bool WifiServiceClass::checkWifi(){
  return WiFi.isConnected();
}


void WifiServiceClass::setupAPWifi(){
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
}

WifiServiceClass WifiService;