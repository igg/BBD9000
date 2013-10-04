#### This directory contains source for the SmartIO AVR flash firmware
* The text-based API is described in `SmartIO-API-notes.txt`
* The pre-made flash file is `SmartIO.hex`
* The make output is main.hex, which should be renamed to SmartIO.hex
* `SmartIO.c` contains helper functions for `main.c`, including the main ISR
* `SamrtIO.h` defines most of the constants and variables for the program (pin assignments, structures, etc)
* The ISR interrupt is at 2 kHz, which reads the ADCs, debounces DIO, and sets flags for tasks in the main loop (`main.c`)

#### `fastboot` directory
* Contains an avr-gcc implementation of Peter Dannegers's (danni) excellent fast tiny bootloader for various AVR microcontrollers
originally contributed to [AVR Freaks](http://www.avrfreaks.net/index.php?module=Freaks%20Academy&func=viewItem&item_id=1008&item_type=project)
