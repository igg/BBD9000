#!/bin/bash
# Coop-specific variables:
DB_BASE="piedmont"

# Generate a root password for mysql (this needs to be done before installing MySql)
ROOT_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
export DEBIAN_FRONTEND=noninteractive
sudo apt-get -q -y install mysql-client-5.5 mysql-common
sudo apt-get -q -y install mysql-server
mysqladmin -u root password $ROOT_PASS
echo "The MySQL root password is [${ROOT_PASS}] (without '[]')"
