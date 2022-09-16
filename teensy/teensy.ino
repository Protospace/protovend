#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>

#include "mdb_defs.h"
#include "mdb_parse.h"
#include "mdb_cashless.h"


const int led_pin = 13;

HardwareSerial *peripheral = &Serial3;
HardwareSerial *sniff = &Serial1;
usb_serial_class *host = &Serial;

enum cashlessStates {
	INACTIVE,
	DISABLED,
	ENABLED,
	IDLE,
	SEND_CANCELSESSION,
	SEND_ENDSESSION,
	VEND,
	//REVALUE,      // level 2
	SEND_ERROR,
	SEND_OUTOFSEQ,
};

static const char* cashlessStateLabels[] =
{
	"INACTIVE",
	"DISABLED",
	"ENABLED",
	"IDLE",
	"SEND_CANCELSESSION",
	"SEND_ENDSESSION",
	"VEND",
	//"REVALUE",    // level 2
	"SEND_ERROR",
	"SEND_OUTOFSEQ",
};

enum controllerStates {
	BEGIN,
	CONNECT,
	WAITING,
	GET_BALANCE,
};

static struct mdb_cashless_config_response my_config = {
	0x01,   // Feature level 1
	0x1111, // currency code
	1,      // Scale factor
	0,      // Decimal places
	10,     // Max response time (s)
	0b00000000  // not sure if this does anything. wrong endianess?
	//    ||||
	//    |||supports cash sale?
	//    ||has display
	//    |multivend capable
	//    supports refunds?
};

enum cashlessStates cashlessState = INACTIVE;
enum controllerStates controllerState = BEGIN;

uint16_t available_funds = 0;
uint16_t last_item = 0;
uint16_t last_price = 0;

#define VEND_NULL 0
#define VEND_OK 1
#define VEND_BAD 2
int allowed_vend = VEND_NULL;

uint8_t mdb_cashless_handler(uint8_t* rx, uint8_t* tx, uint8_t cmd, uint8_t subcmd) {
	static bool has_prices = false;
	static bool has_config = false;

	uint8_t len;

    host->printf("Cashless: Command %02X:%02X in state %s\n", cmd, subcmd, cashlessStateLabels[cashlessState]);

	switch (cashlessState) {
		case INACTIVE:
			if (cmd == MDB_CMD_SETUP && subcmd == MDB_CMD_SETUP_CONFIG) {
				host->println("Cashless: Setup config");
				tx[0] = MDB_RESPONSE_READERCFG;
				memcpy(tx + 1, &my_config, sizeof(my_config));
				len = 8;
				has_config = true;

				if (has_prices) cashlessState = DISABLED;
				break;
			}
			else if (cmd == MDB_CMD_SETUP && subcmd == MDB_CMD_SETUP_PRICES) {
				host->println("Cashless: Setup prices");
				tx[0] = MDB_ACK;
				len = 0;
				has_prices = true;

				if (has_config) cashlessState = DISABLED;
				break;
			}
			else if (cmd == MDB_CMD_RESET) {
				host->println("Cashless: Resetting reader");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			else if (cmd == MDB_CMD_POLL) {
				tx[0] = MDB_RESPONSE_JUSTRESET;
				len = 1;
				break;
			}

			host->println("Cashless: Unexpected command in state INACTIVE");
			break;

		case DISABLED:
			if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_ENABLE) {
				host->println("Cashless: Enabling reader");
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = ENABLED;
				break;
			}
			else if (cmd == MDB_CMD_RESET) {
				host->println("Cashless: Resetting reader");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			else if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_CANCEL) {
				host->println("Cashless: Reader cancel");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			//else if (cmd == MDB_CMD_EXT && subcmd == MDB_CMD_EXT_REQUEST_ID) {
			//	host->println("Cashless: Request ID");

			//	char manufacturer[] = "WTF";
			//	char serial[] = "10 digits?";
			//	char model[] =  "SparkVend!!1";
			//	uint16_t version = 0x0001;

			//	tx[0] = MDB_RESPONSE_PERIPHERALID;
			//	memcpy(tx +  1, manufacturer,  3);
			//	memcpy(tx +  4, serial      , 10);
			//	memcpy(tx + 14, model       , 12);
			//	memcpy(tx + 28, &version    ,  2);

			//	memcpy(manufacturer, rx +  2,  3);
			//	memcpy(serial      , rx +  5, 10);
			//	memcpy(model       , rx + 15, 12);
			//	memcpy(&version    , rx + 29,  2);
			//	len = 30;

			//	break;
			//}
			else if (cmd == MDB_CMD_POLL) {
				tx[0] = MDB_ACK;
				len = 0;
				break;
			}

			host->println("Cashless: Unexpected command in state DISABLED");
			break;

		case ENABLED:
			if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_DISABLE) {
				host->println("Cashless: Disabling reader");
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = DISABLED;
				break;
			}
			else if (cmd == MDB_CMD_RESET) {
				host->println("Cashless: Resetting reader");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			else if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_CANCEL) {
				host->println("Cashless: Reader cancel");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			else if (cmd == MDB_CMD_POLL) {
				tx[0] = MDB_ACK;
				len = 0;
				break;
			}

			host->println("Cashless: Unexpected command in state ENABLED");
			break;

		case IDLE:
			if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_ENABLE) {
				host->println("Cashless: Enabling reader");
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = ENABLED;
				break;
			}
			else if (cmd == MDB_CMD_RESET) {
				host->println("Cashless: Resetting reader");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			else if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_DISABLE) {
				host->println("Cashless: Disabling reader");
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = DISABLED;
				break;
			}
			//else if (cmd == MDB_CMD_REVALUE) {
			//	host->println("Cashless: Revalue request");
			//	tx[0] = MDB_RESPONSE_REVALUEDENY;
			//	len = 1;
			//	break;
			//}
			else if (cmd == MDB_CMD_READER && subcmd == MDB_CMD_READER_CANCEL) {
				host->println("Cashless: Reader cancel");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}
			else if (cmd == MDB_CMD_VEND && subcmd == MDB_CMD_VEND_SESSION_COMPLETE) {
				host->println("Cashless: Session complete");
				tx[0] = MDB_ACK;
				len = 0;

				available_funds = 0;

				cashlessState = SEND_ENDSESSION;
				break;
			}
			else if (cmd == MDB_CMD_VEND && subcmd == MDB_CMD_VEND_REQUEST) {
				last_price = (rx[2] << 8) + rx[3];
				last_item = (rx[4] << 8) + rx[5];

				host->printf("Vend request: item price: %d, number: %04X\n", last_price, last_item);

				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = VEND;
				break;
			}
			else if (cmd == MDB_CMD_POLL) {
				if (available_funds > 0) {
					tx[0] = MDB_RESPONSE_NEWSESSION;
					tx[1] = available_funds >> 8;
					tx[2] = available_funds & 0xFF;
					len = 3;
					available_funds = 0;
					break;
				}
				else {
					tx[0] = MDB_ACK;
					len = 0;
					break;
				}
			}

			host->println("Cashless: Unexpected command in state IDLE");
			break;

		case SEND_CANCELSESSION:
			if (cmd == MDB_CMD_POLL) {
				host->println("Cashless: Send cancel session");
				tx[0] = MDB_RESPONSE_CANCELSESSION;
				len = 1;

				cashlessState = IDLE;
				break;
			}

			host->println("Cashless: Unexpected command in state SEND_CANCELSESSION");
			break;

		case SEND_ENDSESSION:
			if (cmd == MDB_CMD_POLL) {
				host->println("Cashless: Send end session");
				tx[0] = MDB_RESPONSE_ENDSESSION;
				len = 1;

				cashlessState = ENABLED;
				break;
			}

			host->println("Cashless: Unexpected command in state SEND_ENDSESSION");
			break;

		case VEND:
			if (cmd == MDB_CMD_VEND && subcmd == MDB_CMD_VEND_SUCCESS) {
				last_item = (rx[2] << 8) + rx[3];
				host->printf("Cashless: Vend success. number: %04X\n", last_item);
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = IDLE;
				break;
			}
			else if (cmd == MDB_CMD_VEND && subcmd == MDB_CMD_VEND_FAILURE) {
				host->println("Cashless: Vend failure");
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = IDLE;
				break;
			}
			else if (cmd == MDB_CMD_VEND && subcmd == MDB_CMD_VEND_SESSION_COMPLETE) {
				host->println("Cashless: Vend session complete");
				tx[0] = MDB_ACK;
				len = 0;

				cashlessState = IDLE;
				break;
			}
			else if (cmd == MDB_CMD_POLL) {
				if (allowed_vend == VEND_OK) {
					allowed_vend = VEND_NULL;

					host->println("Cashless: Vend OK");
					tx[0] = MDB_RESPONSE_VENDOK;
					tx[1] = last_price >> 8;
					tx[2] = last_price & 0xFF;
					len = 3;
					break;
				}
				else if (allowed_vend == VEND_BAD) {
					allowed_vend = VEND_NULL;

					host->println("Cashless: Vend denied");
					tx[0] = MDB_RESPONSE_VENDDENIED;
					len = 1;

					cashlessState = IDLE;
					break;
				}
				else {
					tx[0] = MDB_ACK;
					len = 0;
					break;
				}
			}

			host->println("Cashless: Unexpected command in state VEND");
			break;

		case SEND_ERROR:
			if (cmd == MDB_CMD_POLL) {
				host->println("Cashless: error");
				tx[0] = MDB_RESPONSE_ERROR;
				len = 1;

				break;
			}
			else if (cmd == MDB_CMD_RESET) {
				host->println("Cashless: Resetting reader");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}

			host->println("Cashless: Unexpected command in state SEND_ERROR");
			break;

		case SEND_OUTOFSEQ:
			if (cmd == MDB_CMD_POLL) {
				host->println("Cashless: out of sequence");
				tx[0] = MDB_RESPONSE_OUTOFSEQ;
				len = 1;

				break;
			}
			else if (cmd == MDB_CMD_RESET) {
				host->println("Cashless: Resetting reader");
				tx[0] = MDB_ACK;
				len = 0;

				has_config = false;
				has_prices = false;
				available_funds = 0;

				cashlessState = INACTIVE;
				break;
			}

			host->println("Cashless: Unexpected command in state SEND_OUTOFSEQ");
			break;
	}

	return len;
}


size_t tx(uint16_t data)
{
    return peripheral->write9bit(data);
}

void setup()
{
    pinMode(led_pin, OUTPUT);

    host->begin(115200);
    host->setTimeout(250);
    sniff->begin(9600, SERIAL_9N1);
    peripheral->begin(9600, SERIAL_9N1_RXINV_TXINV);

    mdb_parser_init(&tx, host);
    mdb_cashless_init(host, &mdb_cashless_handler);

    host->println("Host boot up");
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

void loop()
{
    int pin_state = HIGH;
    int incoming;

    if (peripheral->available() > 0)
    {
		while (peripheral->available()) {
			incoming = peripheral->read();
		}

		if (incoming != -1) {
			digitalWrite(led_pin, pin_state);
			pin_state = (pin_state == HIGH) ? LOW : HIGH;
			//dump_incoming(incoming);
			mdb_parse(incoming);
		}

    }

    if (host->available() > 0)
    {
		String data = host->readString();
		data = data.trim();
		host->print("Serial: ");
		host->println(data);

		uint16_t cents = data.toInt();

		if (data == "ok") {
			host->println("Setting VEND_OK");
			allowed_vend = VEND_OK;
		}
		else if (data == "no") {
			host->println("Setting VEND_BAD");
			allowed_vend = VEND_BAD;
		}
		else if (data == "reset") {
			host->println("resetting vend");
			cashlessState = SEND_ENDSESSION;
		}
		else if (data == "error") {
			host->println("sending error");
			cashlessState = SEND_ERROR;
		}
		else if (data == "oos") {
			host->println("sending out of sequence");
			cashlessState = SEND_OUTOFSEQ;
		}
		else if (data == "cancel") {
			host->println("sending cancel session request");
			cashlessState = SEND_CANCELSESSION;
		}
		else if (cents) {
			host->print("Cents: ");
			host->println(cents);
			available_funds = cents;
			cashlessState = IDLE;
		}
    }

}
