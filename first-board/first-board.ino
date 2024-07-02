#include <RTClib.h>

RTC_DS1307 rtc;

const int BUTTON_PIN = D5; // The ESP8266 pin D7 connected to button
int button_state;

void setup() {
  Serial.begin(9600);

  Serial.println("Hello everyonr fsfdsfsdfdsfsdfdsfds");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // put your setup code here, to run once:
  if(rtc.begin()){
      Serial.println("Connected");
    }
    else{
      Serial.println("Not connect");
    }
}

void loop() {
  // put your main code here, to run repeatedly:
  button_state = digitalRead(BUTTON_PIN);

  if(button_state == HIGH){
    Serial.println("HIGH");
  }
  else{
    //Serial.println("LOW");
  }
}
