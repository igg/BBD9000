# Name: Makefile
# Author: <insert your name here>
# Copyright: <insert your copyright message here>
# License: <insert your license reference here>

# This is a prototype Makefile. Modify it according to your needs.
# You should at least check the settings for
# DEVICE ....... The AVR device you compile for
# CLOCK ........ Target AVR clock rate in Hertz
# OBJECTS ...... The object files created from your source files. This list is
#                usually the same as the list of source files with suffix ".o".
# PROGRAMMER ... Options to avrdude which define the hardware you use for
#                uploading to the AVR and the interface where this hardware
#                is connected.
# FUSES ........ Parameters for avrdude to flash the fuses appropriately.

# KEEP_FLASH = 
KEEP_FLASH = -D
# DEVICE     = atmega164pa
DEVICE     = atmega324pa
PROGRAMMER = -c avrispmkII -P usb
OBJECTS    = main.o SmartIO.o parser.o uart2.o buffer.o lcd.o

# From http://www.engbedded.com/fusecalc/
CLOCK      = 18432000
# Fuses for atmega164pa:
# FUSES      = -U lfuse:w:0xCF:m -U hfuse:w:0xD4:m -U efuse:w:0xFC:m
# Fuses for atmega324pa:
FUSES      = -U lfuse:w:0xCF:m -U hfuse:w:0xD6:m -U efuse:w:0xFC:m


# Int. osc 8 MHz, no clock div
#CLOCK      = 8000000
#FUSES      = -U hfuse:w:0x99:m -U lfuse:w:0xC2:m -U efuse:w:0xFF:m
# Full-swing osc:
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0x99:m -U lfuse:w:0xF7:m -U efuse:w:0xFF:m
# Ext cryst osc. ("low power")
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0x99:m -U lfuse:w:0xFF:m -U efuse:w:0xFF:m
# Full-swing osc + EESAVE + BOD@4.3V - UNSTABLE
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0x91:m -U lfuse:w:0xF7:m -U efuse:w:0xFC:m
# Full-swing osc + EESAVE
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0x91:m -U lfuse:w:0xF7:m -U efuse:w:0xFF:m
# low-power osc + EESAVE + BOD - STABLE (no delay for low-power rise due to BOD) - JTAG
# Fails when powering up with ISP plugged in but unplugged from USB
# low-power osc + EESAVE + BOD - STABLE (no delay for low-power rise due to BOD) - JTAG
# With bootloader
# Fails when powering up with ISP plugged in but unplugged from USB
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0xD0:m -U lfuse:w:0xDF:m -U efuse:w:0xFC:m
# low-power osc (div 8) + EESAVE + BOD - UNSTABLE (no delay for low-power rise due to BOD) - JTAG
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0xD1:m -U lfuse:w:0x5F:m -U efuse:w:0xFC:m
# low-power osc + EESAVE - BOD - STABLE (64ms delay) - JTAG
#CLOCK      = 18432000
#FUSES      = -U hfuse:w:0xD1:m -U lfuse:w:0x5F:m -U efuse:w:0xFC:m

# Tune the lines below only if you know what you are doing:

AVRDUDE = avrdude $(KEEP_FLASH) $(PROGRAMMER) -p $(DEVICE)
COMPILE = avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

# symbolic targets:
all:	main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

eeprom:	main.eep
	$(AVRDUDE) -U eeprom:w:main.eep

fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse

# if you use a bootloader, change the command below appropriately:
load: all
	bootloadHID main.hex

clean:
	rm -f main.hex main.eep main.elf $(OBJECTS)

# file targets:
main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)
	avr-size -C --mcu=$(DEVICE) main.elf

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
# If you have an EEPROM section, you must also create a hex file for the
# EEPROM and add it to the "flash" target.
main.eep: main.elf
	avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex main.elf main.eep

# Targets for code debugging and analysis:
disasm:	main.elf
	avr-objdump -zhD main.elf > main.od

cpp:
	$(COMPILE) -E main.c
