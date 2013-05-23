#### ARM-SBC: Software running on a single-board solid-state Linux computer for interfacing between SmartIO board and BBD9000-CMS.
* First deployed kiosks used a [TS7200] (http://www.embeddedarm.com/products/board-detail.php?product=TS-7200).
Other, more modern possibilities include [iMX233-OLinuXino-MICRO] (https://www.olimex.com/Products/OLinuXino/iMX233/iMX233-OLinuXino-MICRO/),
or [BeagleBone Black] (http://beagleboard.org/Products/BeagleBone%20Black).
Pay close attention to the specified operating temperature range, esp. for deployment in cold environments.
* SBC Linux software consists of several C programs, daemons and scripts that interact using shared memory and fifos
* `BBD9000.conf`: Main configuration file with comments
	* `BBD9000-cal.conf`: Calibrations modifiable at runtime via keypad/LCD
	* `BBD9000-run.conf`: File saving run-time state (created on shutdown)
	* `BBD9000cfg.c`: utilities for reading/writing configuration files.
* `BBD9000mem.h`: shared memory layout.
* Daemons:
	* `BBD9000fsm.c`: Main event-driven finite state machine for kiosk automation ().
	* `BBD9000SmartIO.c`: Serial communication to SmartIO ()
	* `BBD9000server.c`: Encrypted HTTP/HTML communication to hosted BBD9000-CMS (wired/wireless/GPRS) ()
	* `BBD9000timer.c`: Delayed event timer daemon (timeouts, etc.)
	* `BBD9000temp.c`: ARM SBC on-board temperature monitor (optional, ARM-SBC board-specific)
	* `BBD9000authorizeDotNet.c`: CC Gateway comms. No longer used since server handles Gateway comms
* Initialization:
	* `BBD9000init.c`: Initialize shared memory with configuration file (`BBD9000.conf`)
* Cron:
	* `BBD9000twilight.c`: Calculate time of sunrise/sunset every 12 hours (for lighting, LCD backlight).
