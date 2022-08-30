#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>

#include "mdb_defs.h"
#include "mdb_parse.h"
#include "mdb_cashless.h"

uint8_t state = 0x0;

static const char* state_labels[] =
{
    "INACTIVE",
    "DISABLED",
    "ENABLED",
    "IDLE",
    "VEND",
    "REVALUE", //Level 2/3 Device Only
    "NEGVEND", //Level 3 Device Only
};

const int led_pin = 13;

HardwareSerial *peripheral = &Serial3;
HardwareSerial *sniff = &Serial1;
usb_serial_class *host = &Serial;

size_t tx(uint16_t data)
{
    return peripheral->write9bit(data);
}

void log(const char* msg)
{
    host->print(msg);
}

void setup()
{
    pinMode(led_pin, OUTPUT);

    host->begin(115200);
    sniff->begin(9600, SERIAL_9N1);
    peripheral->begin(9600, SERIAL_9N1_RXINV_TXINV);

    mdb_parser_init(&tx, host);
    mdb_cashless_init(host);

	log("Host boot up");
}

uint8_t current_addr = 0;
void dump_incoming(uint16_t incoming)
{
    host->print(">>> ");

    if (incoming & MDB_MODE_BIT)
    {
        current_addr = MDB_ADDRESS_MASK & incoming;

        switch (current_addr)
        {
        case MDB_ADDR_VMC:
            host->print("VMC            ");
            break;
        case MDB_ADDR_CHANGER:
            host->print("COINMECH       ");
            break;
        case MDB_ADDR_CASHLESS1:
            host->print("CASHLESS       ");
            break;
        case MDB_ADDR_BILLVALIDATOR:
            host->print("BILL VALIDATOR ");
            break;
        case MDB_ADDR_COINDISPENSER2:
            host->print("COIN DISPENSER ");
            break;
        default:
            host->print("UNKNOWN        ");
            break;
        }
    }
    else
    {
        host->print("               ");
    }
    
    host->print(" 0x");
    host->print(incoming, HEX);
    host->print(" 0b");
    host->println(incoming, BIN);
}

uint16_t count = 0;
uint16_t toSend = 0;

void loop()
{
    int pin_state = HIGH;
    int incoming;

    if (peripheral->available() > 0)
    {
        incoming = peripheral->read();
		if (incoming != -1) {
			digitalWrite(led_pin, pin_state);
			pin_state = (pin_state == HIGH) ? LOW : HIGH;
			//dump_incoming(incoming);
			mdb_parse(incoming);
		}

    }

	if (count == 5000) {
		mdb_cashless_funds_available(150);
	}

	if (last_item) {
    delay(100);
		last_item = 0;
		vend_approved = true;
    mdb_vend_approval(last_item, last_price);
	}

	//if (count % 1000 == 0) {
	//	int state = mdb_cashless_get_current_state();
	//	host->print("Current state: ");
	//	host->println(state_labels[state]);
	//}

	count++;
	delay(1);

//    if (sniff->available() > 0)
//    {
//        incoming = sniff->read();
//        host->print("TO VMC:   ");
//        host->println(incoming, HEX);
//    }
}
