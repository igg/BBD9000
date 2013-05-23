#!/bin/sh

insmod usbcore
insmod pcipool
insmod usb-ohci
insmod usb-ohci-ep93xx
insmod scsi_mod
insmod sd_mod
insmod usb-storage;
sleep 2
mount  -t ext2 -o sync,noatime /dev/scsi/host0/bus0/target0/lun0/part1 /mnt/cf
#sleep 2
#mount  -t ext2 -o sync,noatime /dev/scsi/host1/bus0/target0/lun0/part1 /mnt/cf2

