# Protovend Teensy Firmware

## Setup

Locate and edit the Teensy's `HardwareSerial.h` which may be stored in this location:

```
arduino-1.8.19/hardware/teensy/avr/cores/teensy3/HardwareSerial.h
```

Uncomment the `#define SERIAL_9BIT_SUPPORT` line, save and exit.


