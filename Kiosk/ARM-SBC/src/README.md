* `BBD9000.conf`: Main configuration file with comments
  * `BBD9000-cal.conf`: Calibrations modifiable at runtime via keypad/LCD
  * `BBD9000-run.conf`: File saving run-time state (created on shutdown)
  * `BBD9000cfg.c`: utilities for reading/writing configuration files.
* `BBD9000mem.h`: shared memory layout.
* Daemons:
  * `BBD9000fsm.c`: Main event-driven finite state machine for kiosk automation.
  * `BBD9000SmartIO.c`: Serial communication to SmartIO
  * `BBD9000server.c`: Encrypted HTTP/HTML communication to hosted BBD9000-CMS
  * `BBD9000timer.c`: Delayed event timer daemon (timeouts, etc.)
  * `BBD9000temp.c`: ARM SBC on-board temperature monitor (optional, ARM-SBC board-specific)
  * `BBD9000authorizeDotNet.c`: CC Gateway comms. No longer used since server handles Gateway comms
* Initialization:
	* `BBD9000init.c`: Initialize shared memory with configuration file (`BBD9000.conf`)  
            This verifies the SmartIO on-board firmware against firmware in SmartIO.hex, and reflashes if necessary.
	* `etc/init.d/BBD9000`: Main start/stop script to launch all BBD9000 daemons on bootup 
* Cron:
	* `BBD9000twilight.c`: Calculate time of sunrise/sunset every 12 hours (for lighting, LCD backlight).
* Bootloader-uploader source in `bootloader/` directory
  * executable runs on POSIX OS and reflashes AVR over serial using the pre-flashed bootloader on AVR.
