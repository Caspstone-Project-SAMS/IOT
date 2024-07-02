#ifndef LCD_SERVICE_H_
#define LCD_SERVICE_H_

#include <LiquidCrystal_I2C.h>

#define MAX_LENGTH 16
#define LCD_COLUMNS 16
#define LCD_ROWS 2

extern void connectLCD();
extern void printTextLCD(String message, int row);
extern void clearLCD();

#endif