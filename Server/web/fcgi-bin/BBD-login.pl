#!/usr/bin/perl -w
# Present a login form
# -----------------------------------------------------------------------------
# BBD-login-reset.pl:  Reset a member's login credentials
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

#------------------------------------------------------------------------------
# Written by:	Ilya G. Goldberg <igg at cathilya dot org>
#------------------------------------------------------------------------------
use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;

use BBD;

our $BBD = new BBD('BBD-login.pl');
$BBD->require_login (0);
$BBD->myTemplate ('BBD-login.tmpl');
$BBD->init(\&do_request);

sub do_request {
	$BBD->printLog (" in BBD-login.pl do_request(), BBD: $BBD\n");
	my $CGI = $BBD->CGI();
	$BBD->printLog ("   CGI: ".($CGI ? "$CGI" : "UNDEFINED")."\n");
	my $DBH = $BBD->DBH();
	$BBD->printLog ("   DBH: ".($DBH ? "$DBH" : "UNDEFINED")."\n");

	# Either we're here to present a form to let the user reset their login/password, or
	# the login and password were sent, so we set them and send the user to their member page.
	if (!$CGI->param('submit')) {
		show_form();
	} else {
		# Decrypt the password if necessary.
		my $password = $BBD->decryptRSApasswd ($CGI->param('crypt_password')) if $CGI->param('crypt_password');
		$password = $CGI->param('password') if not defined $password and $CGI->param('password');
		$CGI->param('password','');
		$CGI->param('crypt_password');
		my $login = $BBD->safeCGIparam ('login');
		$CGI->param('login','');
		if (! $BBD->safeCGIstring ($password) ) {
			$BBD->error ("Login failed.  Please try again.");
			show_form();
			return();
		}
		if (! $login ) {
			$BBD->error ("Login failed.  Please try again.");
			show_form();
			return();
		}
	
		if (! $password or ! $login) {
			$BBD->error ("Login failed.  Please try again.");
			show_form();
			return();
		}
		
		if ($BBD->login ($login,$password)) {
			undef ($login);
			undef ($password);
			$BBD->doRedirect();
		} else {
			undef ($login);
			undef ($password);
			$BBD->error ("Login failed.  Please try again.");
			show_form();
			return();
		}
	
	}
}


sub show_form {

	$BBD->{TEMPLATE}->param(
		modulus => $BBD->getRSAmodulus(),
		server_time => time(),
	);

	$BBD->HTML_out();

}

