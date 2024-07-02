const int BUTTON_PIN = D5; // The ESP8266 pin D5 connected to button

int prev_button_state = HIGH; // the previous state from the input pin
int button_state;    // the current reading from the input pin

void setup() {
  // Initialize the Serial to communicate with the Serial Monitor.
  Serial.begin(9600);
  // initialize the pushbutton pin as an pull-up input
  // the pull-up input pin will be HIGH when the switch is open and LOW when the switch is closed.
  pinMode(BUTTON_PIN, INPUT_PULLDOWN_16);
}

void loop() {
  // read the state of the switch/button:
  button_state = digitalRead(BUTTON_PIN);

  if(button_state == HIGH){
    Serial.println("HIGH");
  }
  else{
    Serial.println("LOW");
  }
}