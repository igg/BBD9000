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
sudo cp "${ROOT_DIR}/setup/lighttpd.conf /etc/lighttpd/
mkdir "${ROOT_DIR}/var
sudo service lighttpd start

# cron
sudo crontab "${ROOT_DIR}/setup/crontab
