# BBD9000
#### A Free Software and Hardware Project for an Automated Biodiesel Dispensing Kiosk and Coop Management Software
* The kiosk is designed to interface with a pre-existing fuel fill/transfer pump and fuel meter, and provide self-service credit-card purchases to coop/fleet members.
* The Coop Management software runs on a hosted server and manages coop/fleet accounts, communicates with kiosks, etc.

### Kiosk Hardware:
* SmartIO: An interface board with an 8-bit AVR microcontroller that interfaces to the hardware components of a fuel station:  
    Pump, fuel meter, display, keypad, credit-card reader, electric strike, motion detector, lights, etc.
    * Interfaces to an ARM SBC running Linux (not part of this project) that manages secure internet communication with the hosted
Coop Management Software.
    * Schematics and board layouts using [KiCad](http://www.kicad-pcb.org)
* Design files for secure outdoor mounting arrangements, 12 V standalone operations, etc.

### Kiosk Software:
* SmartIO: Firmware running on the AVR 8-bit microcontroller. Cross-compiled C code (avr-gcc)
* BBD9000: Software running on the ARM SBC for interfacing between SmartIO board and BBD9000-CMS.
    * Several C programs and scripts that interact using shared memory and fifos.
    * Main event-driven finite state machine for kiosk automation (BBD9000-FSM).
    * Serial communication to SmartIO
    * Encrypted HTTP/HTML communication to hosted BBD9000-CMS (wired/wireless/GPRS)

### Coop Management Software: BBD9000-CMS:
* Hosted software that communicates with kiosks, coop/fleet members and administrators using HTML/HTTP.
    * Maintains per-member account, balance, sales and contact information
    * Administrative pages to keep track of inventory levels at kiosks, consolidated sales, etc.
    * Credit Card gateway communications for financial transactions
    * Perl/MySQL
