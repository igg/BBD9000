#!/usr/bin/perl -w
# -----------------------------------------------------------------------------
# setKioskRSA.pl:  Store a kiosk's public key on the server
# 
#  Copyright (C) 2008 Ilya G. Goldberg
# 
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
# -----------------------------------------------------------------------------
use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;

use BBD;
my $BBD = new BBD();
$BBD->require_login (0);

use constant SET_KIOSK_RSA => <<"SQL";
	UPDATE kiosks
	SET rsa_key = ?
	WHERE kiosk_id = ?
SQL


$BBD->init(\&do_request);
exit;



sub do_request {
	$BBD->session_delete();
	print "Urp?\n" and die unless $BBD->{CGI}->remote_host() eq 'localhost';
	my $DBH = $BBD->DBH();
	if (! ($ARGV[0] && $ARGV[1]) ) {
		print <<USAGE;
Usage:
  $0 kiosk_id pub_key.pem
  Where kiosk_id is the dabase ID of the kiosk,
  and pub_key.pem is the path to the kiosk's public key
USAGE
		exit();
	}


	open (PEM_FILE,"< $ARGV[1]")
		or die "Could not open public key pem file $ARGV[1]\n";

	my $pem_key = '';
	while (<PEM_FILE>) {
		$pem_key .= $_;
	}
	close PEM_FILE;

	print "Public key:\n$pem_key\n";


	my $sth = $DBH->prepare(SET_KIOSK_RSA)
		or die "Could not prepare handle";
	$sth->execute( $pem_key, $ARGV[0] )
		or die "Could not execute statement handle";

	print "Key updated for Kiosk $ARGV[0]\n";
}
