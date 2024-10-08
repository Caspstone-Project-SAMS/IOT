#include <ESP8266WebServer.h>
#include "EEPRomService.h"
#include "AppDebug.h"
#include "AppConfig.h"
#include "WifiService.h"
#include "HttpServerH.h"

ESP8266WebServer* server = nullptr;
int appMode = NORMAL_MODE;
unsigned long settingTimeout = 0;
unsigned long toggleTimeout = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("\nStart up...");
  pinMode(PIN_RESET, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  WifiService.connect();
}

void handleNormalMode(){
  if(WiFi.status() == WL_CONNECTED){
    if(toggleTimeout <= millis()) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      toggleTimeout = millis() + LED_TIME_INTERVAL;
    }
    return;
  }

  WifiService.connect();
}

void handleServerMode(){
  if(server) {
    server->handleClient();
  }
  else{
    appMode = NORMAL_MODE;
  }
}

void checkButtonResetClick(){
  if(digitalRead(PIN_RESET) == LOW && (settingTimeout + SETTING_HOLD_TIME) <= millis() && appMode != SERVER_MODE){
    ECHOLN("Go to server Mode");
    settingTimeout == millis();
    if(appMode != SERVER_MODE){
      appMode = SERVER_MODE;

      // Start server
      startConfigServer();
      WifiService.setupAP();
    }
  }
  else if(digitalRead(PIN_RESET) == HIGH){
    ECHOLN("Button is pushed");
    settingTimeout = millis();
  }
}

void loop() {
  if(appMode == NORMAL_MODE) {
    handleNormalMode();
  }
  else {
    handleServerMode();
  }
  checkButtonResetClick();
}
