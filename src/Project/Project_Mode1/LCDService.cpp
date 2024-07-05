#include "LCDService.h"

LiquidCrystal_I2C lcd(0x27, LCD_COLUMNS, LCD_ROWS);

void connectLCD(){
  lcd.init();
  lcd.backlight();
  //lcd.setBacklightBrightness(50);
}

void printTextLCD(String message, int row){
  
  if(message.length() > MAX_LENGTH) {
    for(int i=0; i < 5; i++) {
      message = " " + message; 
    }
    message = message + " ";

    for(int pos = 0; pos < message.length() - 16; pos++){
      lcd.setCursor(0, row);
      lcd.print("");
      lcd.print(message.substring(pos, pos + 16));
      delay(300);
    }
  }
  else{
    lcd.setCursor(0, row);
    lcd.print(""); 
    delay(50);
    lcd.print(message); 
  }
}

void clearLCD(){
  lcd.clear();
}