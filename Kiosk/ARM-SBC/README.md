#### ARM-SBC: Software running on a single-board solid-state Linux computer for interfacing between SmartIO board and BBD9000-CMS.
* The first kiosks deployed used a [TS7200] (http://www.embeddedarm.com/products/board-detail.php?product=TS-7200)
industrial Linux ARM-based computer.
Other, more modern possibilities include [iMX233-OLinuXino-MICRO] (https://www.olimex.com/Products/OLinuXino/iMX233/iMX233-OLinuXino-MICRO/),
or [BeagleBone Black] (http://beagleboard.org/Products/BeagleBone%20Black).  
Pay close attention to the specified operating temperature range, esp. for deployment in cold environments.
Although the [RaspberryPi] (http://www.raspberrypi.org/) is a popular ARM-SBC, it is not rated to operate below freezing.
* SBC Linux software consists of several C programs, daemons and scripts that interact using shared memory and fifos
* C source and scripts in `src/` directory.
* Initialization scripts in `etc/init.d/` directory.
    * Main BBD9000 start/stop script in `etc/init.d/BBD9000`
* Compilation/installation usually done natively on ARM-SBC (i.e. without cross-compilation).
* Deployment on ARM-SBC usually in `/BBD9000/`

#### Dependencies
* [libcurl] (http://curl.haxx.se/download.html): HTTP client.
curl-7.17.1 has been successfully used previously
* [timer_q] (http://www.and.org/timer_q/latest/): Timer library without alarm().
timer_q-1.0.7  has been successfully used previously
* [libconfuse] (http://www.nongnu.org/confuse/): Configuration file processing.
confuse-2.5 has been successfully used previously.

#### Development/Building/Installing - NFS method
* Configure ARM CPU as an NFS client
* Download and unpack a full linux-arm distribution on host computer (in e.g. /BBD9000/linux-arm-deb)
* Setup host computer as NFS server. Keep the linux distro inside the exported directory (e.g. /BBD9000/linux-arm-deb). Add the following to /etc/exports (assuming Kiosk is on 10.0.1.8, /BBD9000 and distro is owned by username 'igg'):

        /BBD9000 -maproot=igg 10.0.1.8
* Restart NFS on host/server:

        sudo nfsd restart
* Make sure that the export is working on the host/NFS server:

        showmount -e
* Make a shell script on the Kiosk (in /root for e.g.) to mount and chroot to the linux distro on NFS (assuming NFS server is on 10.0.1.254):

        #!/bin/sh
        mount -t nfs 10.0.1.254:/BBD9000 /mnt/nfs
        mount -t nfs 10.0.1.254:/BBD9000 /mnt/nfs/linux-arm-deb/mnt/nfs
        mount -o bind /dev /mnt/nfs/linux-arm-deb/dev
        mount -o bind / /mnt/nfs/linux-arm-deb/mnt/main
        chroot /mnt/nfs/linux-arm-deb /bin/bash
* Make a shell script on the Kiosk (in /root for e.g.) to unmount the chrooted NFS linux distro:

        #!/bin/sh
        umount /mnt/nfs/linux-arm-deb/mnt/main
        umount /mnt/nfs/linux-arm-deb/dev
        umount /mnt/nfs/linux-arm-deb/mnt/nfs
        umount /mnt/nfs
* If the above structure is used, the kiosk's root file system ('/') is available in /mnt/main while chrooted.


#### Generating keys
1024-bit RSA is used to communicate to the remote hosted BBD9000-CMS server.  
To conserve bandwidth over expensive cellular M2M networks, the kiosk sends encrypted and signed message blocks over http using POST requests. Each request contains one or more binary message blocks followed by a signature block.
The server verifies the message using the kiosk signature and responds with one or more binary 1024-bit RSA-encrypted message blocks followed by the server's signature.
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
