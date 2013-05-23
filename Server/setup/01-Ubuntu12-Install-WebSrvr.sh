#!/bin/sh
sudo apt-get -y update
sudo apt-get -y upgrade
sudo apt-get -y install s3cmd
#sudo apt-get -y install apache2 apache2-suexec-custom
#sudo apt-get -y install libapache2-mod-php5 libapache2-mod-fcgid libapache-dbi-perl
sudo apt-get -y install lighttpd php5-cgi

# Apache/Suxec setup (make sure that apache2-suexec-custom package is installed)
# cat > /etc/apache2/suexec/www-data 
# /home/ubuntu/web/
# ^d
# 
# sudo a2enmod suexec
# sudo cp setup/BBDC-Apache-mod_fcgid.conf /etc/apache2/sites-available/
# sudo a2dissite default
# sudo service apache2 restart

# lighttpd setup instead of Apache
# sudo service apache2 stop
# sudo apt-get --purge remove libapache2-mod-php5 libapache2-mod-fcgid libapache-dbi-perl
# sudo apt-get --purge remove apache2-suexec-custom apache2
