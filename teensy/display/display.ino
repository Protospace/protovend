// Example LCD sketch
// Connect to SCL and SDA
// (pins 19 & 18 on Teensy 4.1)

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
	lcd.init();
	lcd.backlight();
	lcd.setCursor(0,0);
	lcd.print("Hello, world!");
}

void loop()
{
}
