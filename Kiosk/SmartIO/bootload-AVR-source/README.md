Board on stack, with RS232 connected to ARM CPU.
AVR ISP MkII connected to Mac and plugged into ISP headxer
On Mac:
cd /fromBiggen/BBDC/Smart_IO/Software/

avrdude -p atmega164p -P usb     -c avrispmkII    -U hfuse:w:0xD4:m -U lfuse:w:0xDF:m -U efuse:w:0xFC:m
avrdude -p atmega164p -P usb     -c avrispmkII    -U flash:w:bootload-wdt-read.hex

Unpplug ISP MkII from ISP header.


On ARM CPU:
/etc/init.d/BBD9000 stop
cd /mnt/cf/root/src/
bootloader/bootloader -p SmartIO.hex -d /dev/ttyAM1 -b 115200

minicom com2
