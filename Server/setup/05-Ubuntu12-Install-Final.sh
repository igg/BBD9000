#!/bin/bash
# Fancy way of determining the directory of this script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$BIN_DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
ROOT_DIR=$( dirname "$BIN_DIR" )
date=`date '+%F'`

# set up lighttpd and start it
sudo mv /etc/lighttpd/lighttpd.conf /etc/lighttpd/lighttpd.conf-OLD
sudo cp "${ROOT_DIR}/setup/lighttpd.conf" /etc/lighttpd/
mkdir "${ROOT_DIR}/var"
sudo service lighttpd start

# cron - append to root's pre-existing crontab
sudo bash -c "crontab -l > ${ROOT_DIR}/setup/crontab-OLD"
sudo bash -c "cat ${ROOT_DIR}/setup/crontab >> ${ROOT_DIR}/setup/crontab-OLD"
sudo bash -c "mv ${ROOT_DIR}/setup/crontab-OLD ${ROOT_DIR}/setup/crontab"
sudo crontab "${ROOT_DIR}/setup/crontab"

# generate server key
openssl genrsa -F4 -out "${ROOT_DIR}/priv/BBD.pem" 1024
openssl rsa -in "${ROOT_DIR}/priv/BBD.pem" -out "${ROOT_DIR}/priv/BBD-pub.pem" -pubout
chmod 400 "${ROOT_DIR}/priv/BBD.pem"
chmod a-w "${ROOT_DIR}/priv/BBD-pub.pem"
# get gateway conf
# This is a test gateway (server won't start without one):
cp "${ROOT_DIR}/setup/GW_test.conf" "${ROOT_DIR}/priv/GW.conf"
chmod 400 "${ROOT_DIR}/priv/GW.conf"
# use bin/BBD-invite.pl to create the first member
