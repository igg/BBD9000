#### Using bootloader to re-flash the AVR
* SmartIO connected to PC/ARM-SBC via serial cable (e.g. as deployed)
* bootloader executable deployed in `/BBD9000/SmartIO/bootloader` on ARM-SBC
* SmartIO firware source in `Kiosk/SmartIO/src/`. Makefile makes firmware in `Kiosk/SmartIO/src/main.hex`.
* SmartIO firmware deployed in `/BBD9000/SmartIO/SmartIO.hex` (copy of main.hex)


On PC/ARM-SBC:
* Ensure that the BBD9000 is stopped  
    `/etc/init.d/BBD9000 stop`
* Flash the SmartIO firmware (assuming SmartIO tty device is `/dev/ttyAM1`)  
N.B.: This is normally done automatically by BBD9000init
    `/BBD9000/SmartIO/bootloader -p /BBD9000/SmartIO/SmartIO.hex -d /dev/ttyAM1 -b 115200`
