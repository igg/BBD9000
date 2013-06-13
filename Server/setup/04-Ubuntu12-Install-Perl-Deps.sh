#!/bin/sh
sudo apt-get -y install libcgi-fast-perl libdbi-perl libdbd-mysql-perl libcgi-session-perl libhtml-template-perl libdatetime-perl libdatetime-format-strptime-perl
sudo apt-get -y install postfix libmail-sendmail-perl  libcrypt-openssl-random-perl libcrypt-openssl-bignum-perl libcrypt-openssl-rsa-perl libmime-base64-perl
sudo apt-get -y install libscalar-util-numeric-perl libdigest-sha-perl libtime-hires-perl libsys-sigaction-perl
sudo apt-get -y install libemail-simple-perl

# Uncomment below for specific perl dependencies
# # use CGI::Fast
# sudo apt-get install libcgi-fast-perl
# # use DBI qw(:sql_types);
# sudo apt-get install libdbi-perl libdbd-mysql-perl
# # use CGI::Session;
# sudo apt-get install libcgi-session-perl
# # use HTML::Template;
# sudo apt-get install libhtml-template-perl
# # use DateTime;
# sudo apt-get install libdatetime-perl
# # use DateTime::Format::Strptime;
# sudo apt-get install libdatetime-format-strptime-perl
# 
# # use Mail::Sendmail;
# sudo apt-get install postfix libmail-sendmail-perl
# # use Crypt::OpenSSL::Random;
# sudo apt-get install libcrypt-openssl-random-perl
# # use Crypt::OpenSSL::Bignum;
# sudo apt-get install libcrypt-openssl-bignum-perl
# # use Crypt::OpenSSL::RSA;
# sudo apt-get install libcrypt-openssl-rsa-perl
# # use MIME::Base64;
# sudo apt-get install libmime-base64-perl
# # use Scalar::Util qw(looks_like_number);
# sudo apt-get install libscalar-util-numeric-perl
# # use Digest::SHA qw(sha1_base64);
# sudo apt-get install libdigest-sha-perl
# # use Time::HiRes;
# sudo apt-get install libtime-hires-perl
# # use Sys::SigAction;
# sudo apt-get install libsys-sigaction-perl
# # use Email::Simple;
