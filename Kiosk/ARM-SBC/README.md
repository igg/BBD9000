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

#### Dependencies
* [libcurl] (http://curl.haxx.se/download.html): HTTP client. curl-7.17.1 has been successfully used previously
* [timer_q] (http://www.and.org/timer_q/latest/): Timer library without alarm(). timer_q-1.0.7  has been successfully used previously
* [libconfuse] (http://www.nongnu.org/confuse/): Configuration file processing. confuse-2.5 has been successfully used previously (configuration reading)

#### Generating keys
1024-bit RSA is used to communicate to the remote hosted BBD9000-CMS server.  
To conserve bandwidth over expensive cellular M2M networks, the kiosk transmits binary RSA-encrypted 1024-bit message blocks
as HTTP POST requests followed by a 1024-bit kiosk signature block.  
Likewise, the server verifies the kiosk signature and responds with one or more binary 1024-bit RSA-encrypted message blocks followed by
a 1024-bit server signature block.
* Make a kiosk key - the number (e.g. `-0001`) must match the Kiosk ID in the server database.

        openssl genrsa -F4 -out BBD9000-0001.pem 1024
        openssl rsa -in BBD9000-0001.pem -out BBD9000-0001-pub.pem -pubout
        chmod 400 BBD9000-0001.pem
        chmod a-w BBD9000-0001-pub.pem
* Copy the public `BBD9000-0001-pub.pem` to the server's `priv/` directory.
* Keep the private `BBD9000-0001.pem` on the kiosk in the `/BBD9000/` directory, and not anywhere else.
* Make a server key - only one key per server, regardless of number of kiosks:
 
        openssl genrsa -F4 -out BBD.pem 1024
        openssl rsa -in BBD.pem -out BBD-pub.pem -pubout
        chmod 400 BBD.pem
        chmod a-w BBD-pub.pem
* Copy the public `BBD-pub.pem` to each kiosk's `/BBD9000/` directory.
* Keep the private `BBD.pem` on the server's `priv/` directory, and not anywhere else.
* Register the kiosk's key with the database on the BBD9000-CMS server (e.g. for Kiosk ID = 1)

        bin/setKioskRSA.pl 1 BBD9000-0001-pub.pem
