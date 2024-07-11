#include "Crypto.h"
#include "HttpServerH.h"

bool startConfigServer(){
  if(server) {
    return false;
  }

  ECHOLN("[HttpServerH][startConfigServer] Begin create new server...");
  server = new ESP8266WebServer(HTTP_PORT);

  server->on(F("/"), HTTP_GET, handleRoot);
  server->on(F("/status"), HTTP_GET, handleStatus);
  server->on(F("/wifis"), HTTP_GET, handleWifis);
  server->on(F("/connect-to"), HTTP_POST, handleConnectTo);
  server->on(F("/"), HTTP_OPTIONS, handleOk);
  server->on(F("/status"), HTTP_OPTIONS, handleOk);
  server->on(F("/wifis"), HTTP_OPTIONS, handleOk);
  server->on(F("/connect-to"), HTTP_OPTIONS, handleOk);
  server->begin();
  ECHOLN("[HttpServerH][startConfigServer] HTTP server started");

  return true;
}

void stopServer(){
  if(server){
    server->stop();
    server = nullptr;
  }
}

void handleOk(){
  ECHOLN("[HttpServerH][handleOk] Send OK");
  if(!server){
    ECHOLN("[HttpServer][handleOk] Server not found");
    return;
  }
  server->sendHeader(F("Access-Control-Allow-Headers"), "*");
  server->sendHeader(F("Access-Control-Allow-Methods"), "*");
  server->sendHeader(F("Access-Control-Allow-Origin"), "*");
  server->send(200, F("application/json; charset=utf-8"), F("{\"status\":\"ok\"}"));
}

void handleRoot(){
  if(!server){
    ECHOLN("[HttpServerH][handleRoot] Server not found");
    return;
  }
  ECHOLN("[HttpServerH][handleRoot] Send ROOT");
  server->send(200, F("text/html"), F("<h1>You are connected</h1>"));
}

void handleConnectTo(){
  if(!server){
    ECHOLN("[HttpServerH][handleConnectTo] Server not found");
    printTextLCD("[Error] Server not found", 1);
    return;
  }
  printTextLCD("Changing wifi...", 1);
  unsigned long lcdTimeout = millis();

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;

  ECHO("[HttpServerH][handleConnectTo] Receive data: ");
  ECHOLN(server->arg("plain"));
  JsonObject& rootData = jsonBuffer.parseObject(server->arg("plain"));

  ECHOLN("[HttpServerH][handleConnectTo] Send response {\"status\":\"ok\"} ");
  server->sendHeader(F("Access-Control-Allow-Headers"), "*");
  server->sendHeader(F("Access-Control-Allow-Origin"), "*");
  server->send(200, F("application/json; charset=utf-8"), F("{\"status\":\"ok\"}"));

  while((lcdTimeout + 1500) > millis()){
    delay(500);
  }
  if(rootData.success()){
    String nssid = rootData[F("ssid")];
    String npass = rootData[F("pass")];
    ECHO("Wifi new name: ");
    ECHOLN(nssid);
    ECHO("Wifi new password: ");
    ECHOLN(npass);
    if(WifiService.connectNewWifi(nssid, npass) > 0) {
      printTextLCD("Connect wifi successfully", 1);
      delay(2500);
      return;
    }
    printTextLCD("Wrong wifi!!!", 1);
    delay(2500);
    return;
  }

  ECHOLN("Wrong data!!!");
  printTextLCD("Wrong data!!!", 1);
  delay(2500);
}

void handleStatus() {
  if(!server){
    ECHOLN("[HttpServerH][handleStatus] Server not found");
    return;
  }
  String response = F("{\"status\":\"ok\"}");
  server->sendHeader(F("Access-Control-Allow-Headers"), "*");
  server->sendHeader(F("Access-Control-Allow-Origin"), "*");
  server->send(200, F("application/json; charset=utf-8"), response);
}

void handleWifis(){
  if(!server){
    ECHOLN("[HttpServerH][handleWifis] Server not found");
    printTextLCD("[Error] Server not found", 1);
    return;
  }

  printTextLCD("Getting wifis...", 1);
  // Scan start
  String wifis = "[";

  // Wifi.scanNetworks will return the number of the networks found
  int n = WiFi.scanNetworks();
  yield();
  ECHOLN("Scan done");
  if(n == 0){
    ECHOLN("No networks found");
  } else {
    ECHO(n);
    ECHOLN(" networks found");
    for(int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      ECHO(i + 1);
      ECHO(": ");
      ECHO(WiFi.SSID(i));
      ECHO(" (");
      ECHO(WiFi.RSSI(i));
      ECHO(")");
      ECHOLN((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
      wifis += F("{\"name\":\"");
      wifis += WiFi.SSID(i);
      wifis += F("\",\"quality\":\"");
      wifis += WiFi.RSSI(i);
      wifis += F("\"}");
      if(i != n - 1) {
        wifis += F(",");
      }
    }
  }
  wifis += "]";
  ECHOLN(wifis);
  server->sendHeader(F("Access-Control-Allow-Headers"), F("*"));
  server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server->send(200, F("application/json; charset=utf-8"), wifis);
}











