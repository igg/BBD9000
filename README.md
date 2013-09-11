# BBD9000
#### A Free Software and Hardware Project for Automated Biodiesel Dispensing Kiosks and Coop Management Software
* Kiosks interface with pre-existing fuel fill/transfer pumps and fuel meters,
providing self-service credit-card purchases to coop/fleet members.
* The Coop Management Software runs on a remote hosted server and manages coop/fleet accounts and one or more kiosks.
* This software and hardware has been in operation at the [Baltimore Biodiesel Coop] (http://baltimorebiodiesel.org/) since 2009

### Kiosk Hardware:
* SmartIO: An interface board with an 8-bit AVR microcontroller that interfaces to
the hardware components of a fuel station:  
    Pump, fuel meter, display, keypad, credit-card reader, electric strike, motion detector, lights, etc.
    * Serial interface to an ARM SBC in the kiosk running Linux that manages secure internet communication with the hosted
Coop Management Software. An off-the-shelf Linux ARM SBC is used; the ARM SBC itself is not part of this project.
    * Schematics and board layouts using [KiCad](http://www.kicad-pcb.org) in `BBD9000/Kiosk/SmartIO/hardware/v3.3/`
* Design files for secure outdoor mounting arrangements, 12 V standalone operations, etc.

### Kiosk Software:
* SmartIO: Firmware running on the AVR 8-bit microcontroller. Cross-compiled C code (avr-gcc)
    * source in `BBD9000/Kiosk/SmartIO/src/`
* ARM-SBC: Software running on a single-board Linux computer in the kiosk for interfacing between SmartIO board and BBD9000-CMS.
    * Source in `BBD9000/Kiosk/ARM-SBC/src/`
    * Several C programs and scripts that interact using shared memory and fifos (memory layout in `BBD9000mem.h`).
    * Main event-driven finite state machine for kiosk automation (`BBD9000fsm.c`).
    * Serial communication to SmartIO (`BBD9000SmartIO.c`)
    * Encrypted HTTP/HTML communication to hosted BBD9000-CMS (wired/wireless/GPRS) (`BBD9000server.c`)

### Coop Management Software: BBD9000-CMS:
* Hosted software that communicates with kiosks, coop/fleet members and administrators using HTML/HTTP.
    * Source in `BBD9000/Server/web/fcgi-bin/`, supporting files/directories in `BBD9000/Server/`
    * Maintains per-member account, balance, sales and contact information
    * Administrative pages to keep track of inventory levels at kiosks, consolidated sales, etc.
    * Member pages to update contact info, track purchases, add family members, credit-cards, etc.
    * Credit Card Gateway communications for financial transactions (currently Authorize.net Card Present protocol)
    * Implemented using Perl/MySQL, deployed using [lighttpd] (http://www.lighttpd.net)
