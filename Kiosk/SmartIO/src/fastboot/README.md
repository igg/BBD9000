#### The "fast tiny & mega UART bootloader" (fastboot)
Originally [contributed](http://www.avrfreaks.net/index.php?module=Freaks%20Academy&func=viewItem&item_id=1008&item_type=project) to AVR Freaks by danni (Peter Dannegers). This is an excellent AVR bootloader written in assembly with auto baud detection, one-wire or two-wire serial on any pin, all under 256 words (512 bytes).
This was later made compatible with avr-gcc by mizch (H. C. Zimmerer) and contributed to [mikrocontroller.net](http://www.mikrocontroller.net/topic/73196) (German).
This was then slightly modified to tolerate the absence of gawk, and other minor things.
#### This bootloader uses its own protocol, and requiring a POSIX-compatible host executable
The Unix/Linux/POSIX bootloader-uploader was [contributed](http://www.avrfreaks.net/index.php?module=Freaks%20Academy&func=viewItem&item_type=project&item_id=1927) to AVR Freaks by iggie01.  The source code is in this repository under `Kiosk/ARM-SBC/src/bootloader`, and is used by the ARM-SBC to update and verify the SmartIO firmware.

#### Flash bootloader onto AVR using avrdude, AVR ISP MkII, and ISP header on SmartIO
* Fuse settings calculated using [Engbedded Fuse Calculator](http://www.engbedded.com/fusecalc) for ATMEGA164PA and ATMEGA324PA
* Fuse settings for ATMEGA164PA: `-U lfuse:w:0xCF:m -U hfuse:w:0xD4:m -U efuse:w:0xFC:m`, ATMEGA324PA: `-U lfuse:w:0xCF:m -U hfuse:w:0xD6:m -U efuse:w:0xFC:m`
  * Ext. Crystal Osc >= 8MHz; 1k CK + 65 ms startup time
  * Boot Reset vector Enabled (default address=$0000: i.e. run bootloader on reset)
  * Boot Flash section size = 256 words, start address = $1F00 for ATMEGA164PA, $3F00 for ATMEGA324PA
  * Preserve EEPROM through chip erase cycle
  * SPI enabled
  * Brown-out at 4.3V

* AVR ISP MkII connected to PC USB and plugged into ISP header on SmartIO board.

On PC (ATMEGA164PA):

    avrdude -p atmega164p -P usb -c avrispmkII -U lfuse:w:0xCF:m -U hfuse:w:0xD4:m -U efuse:w:0xFC:m
    avrdude -p atmega164p -P usb -c avrispmkII -U flash:w:bootload-ATMEGA164PA.hex

On PC (ATMEGA324PA):

    avrdude -p atmega324p -P usb -c avrispmkII -U lfuse:w:0xCF:m -U hfuse:w:0xD6:m -U efuse:w:0xFC:m
    avrdude -p atmega324p -P usb -c avrispmkII -U flash:w:bootload-ATMEGA324PA.hex

* Unpplug ISP MkII from ISP header.

#### Using bootloader to flash the SmartIO firmware
* SmartIO connected to PC/ARM-SBC via serial cable (or as deployed)
* Source code for bootloader host executable in `Kiosk/ARM-SBC/src/bootloader` (MacOS, Linux), deployed in `/BBD9000/SmartIO/bootloader` on ARM-SBC.
* SmartIO firmware source in `Kiosk/SmartIO/src/`, deployed in `/BBD9000/SmartIO/SmartIO.hex` (copy of `main.hex`).


On PC/ARM-SBC:

* Ensure that the BBD9000 is stopped  
    `/etc/init.d/BBD9000 stop`
* Flash the SmartIO firmware (assuming SmartIO tty device is `/dev/ttyAPP0`)  for the first time:  
    `/BBD9000/SmartIO/bootloader -p /BBD9000/SmartIO/SmartIO.hex -d /dev/ttyAPP0 -b 115200`
* After the SmartIO firmware was flashed for the first time, the AVR needs to be rebooted to enter the bootloader by using the `RESET` API command:  
    `/BBD9000/SmartIO/bootloader -p /BBD9000/SmartIO/SmartIO.hex -d /dev/ttyAPP0 -b 115200 -r '\n\nRESET\n'`
