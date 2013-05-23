#### ARM-SBC: Software running on a single-board solid-state Linux computer for interfacing between SmartIO board and BBD9000-CMS.
* First deployed kiosks used a [TS7200] (http://www.embeddedarm.com/products/board-detail.php?product=TS-7200).
Other, more modern possibilities include [iMX233-OLinuXino-MICRO] (https://www.olimex.com/Products/OLinuXino/iMX233/iMX233-OLinuXino-MICRO/),
or [BeagleBone Black] (http://beagleboard.org/Products/BeagleBone%20Black).  
Pay close attention to the specified operating temperature range, esp. for deployment in cold environments.
* SBC Linux software consists of several C programs, daemons and scripts that interact using shared memory and fifos
* C source and scripts in `src/` directory.
* Initialization scripts in `etc/init.d/` directory.
    * Main BBD9000 start/stop script in `etc/init.d/BBD9000`
* Compilation/installation usually done natively on ARM-SBC (i.e. without cross-compilation).
* Deployment on ARM-SBC usually in `/BBD9000/`
