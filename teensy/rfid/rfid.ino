#include <Arduino.h>
#include <HardwareSerial.h>

usb_serial_class *host = &Serial;
HardwareSerial *rfid = &Serial7;

void setup()
{
    host->begin(115200);

    rfid->begin(9600);
    rfid->setTimeout(50);

    host->println("Host boot up");
}


void loop()
{
	if (rfid->available() > 0)
	{
		String data = rfid->readString();

		host->print("RFID: ");
		host->println(data);
	}
}
