##### The "fast tiny & mega UART bootloader" in this directory was contributed to AVR Freaks by danni 
[fast tiny & mega UART bootloader on AVR Freaks] (http://www.avrfreaks.net/index.php?module=Freaks%20Academy&func=viewItem&item_id=1008&item_type=project)
##### The POSIX-compatible host executable for danni's bootloader was contributed to AVR Freaks by iggie01
[linux bootloader uploader on AVR Freaks] (http://www.avrfreaks.net/index.php?module=Freaks%20Academy&func=viewItem&item_type=project&item_id=1927)  
The source code is also part of this repository in `Kiosk/BBD9000/src/bootloader/`

#### Flash bootloader onto AVR using avrdude, AVR ISP MkII, and ISP header on SmartIO
AVR ISP MkII connected to PC USB and plugged into ISP header on SmartIO board

On PC:

    avrdude -p atmega164p -P usb -c avrispmkII -U hfuse:w:0xD4:m -U lfuse:w:0xDF:m -U efuse:w:0xFC:m
    avrdude -p atmega164p -P usb -c avrispmkII -U flash:w:bootload-wdt-read.hex

Unpplug ISP MkII from ISP header.

#### Using bootloader to re-flash the AVR
* SmartIO connected to PC/ARM-SBC via serial cable (e.g. as deployed)
* Source code for bootloader host executable in `Kiosk/BBD9000/src/bootloader/` (MacOS, Linux)
* bootloader executable deployed in `/BBD9000/SmartIO/bootloader` on ARM-SBC
* SmartIO firware source in `Kiosk/SmartIO/src/`. Makefile makes firmware in `Kiosk/SmartIO/src/main.hex`.
* SmartIO firmware deployed in `/BBD9000/SmartIO/SmartIO.hex` (copy of main.hex)


On PC/ARM-SBC:
* Ensure that the BBD9000 is stopped  
    `/etc/init.d/BBD9000 stop`
* Flash the SmartIO firmware (assuming SmartIO tty device is `/dev/ttyAM1`)  
    `/BBD9000/SmartIO/bootloader -p /BBD9000/SmartIO/SmartIO.hex -d /dev/ttyAM1 -b 115200`
