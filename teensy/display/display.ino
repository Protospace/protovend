// Example LCD sketch
// Connect to SCL and SDA
// (pins 19 & 18 on Teensy 4.1)

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte protocoin[] = {
	B01110,
	B10101,
	B11111,
	B10100,
	B11111,
	B00101,
	B11111,
	B00100
};

void setup()
{
	lcd.init();
	lcd.createChar(0, protocoin);
	lcd.backlight();

	lcd.setCursor(0,0);
	lcd.print("Hello, world!");

	lcd.setCursor(0,1);
	lcd.write(0);
	lcd.setCursor(1,1);
	lcd.print("12.34");
}

void loop()
{
}
