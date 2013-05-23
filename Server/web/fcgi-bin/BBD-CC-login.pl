#!/usr/bin/perl -w
# Present a login form based on CC transaction info
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
our $BBD = new BBD('BBD-CC-login.pl');
$BBD->require_login (0);
$BBD->myTemplate ('BBD-CC-login.tmpl');
$BBD->init(\&do_request);


use constant GET_MEMBER_BY_CC_NAME_LAST4 => <<"SQL";
	SELECT members.member_id, members.pin, members.name FROM members, member_ccs
	WHERE member_ccs.member_id = members.member_id
	AND member_ccs.cc_name = ?
	AND member_ccs.last4 = ?
SQL

our $success;
our ($CC_name,$last4,$email);

sub do_request {
	my $CGI = $BBD->CGI();
	my $DBH = $BBD->DBH();
	undef ($success);
	undef ($CC_name);
	undef ($last4);
	undef ($email);
	
	if (!$CGI->param('submit')) {
		show_form();
	} else {

		if (! ($BBD->safeCGIstring ('crypt_password') or $BBD->safeCGIstring ('password')) ) {
			$BBD->error ("Login failed.  Please try again.");
			show_form();
			return();
		}

		# Decrypt the SPN if necessary.
		my $SPN = $BBD->decryptRSApasswd ($CGI->param('crypt_password')) if $CGI->param('crypt_password');
		$SPN = $CGI->param('password') if not defined $SPN and $CGI->param('password');
		$CGI->param('password','');
		$CGI->param('crypt_password','');
		$CC_name = $BBD->safeCGIparam ('CC_name');
		$CGI->param('CC_name','');
		$last4 = $BBD->safeCGIparam ('last4');
		$CGI->param('last4','');
		$email = $BBD->safeCGIparam ('email');
		$CGI->param('email','');

		if (! $email  or not $email =~ /@/) {
			undef ($email);
			$BBD->error ("Email address is required");
			show_form();
			return();
		}

		if (! $CC_name ) {
			undef ($CC_name);
			$BBD->error ("Name on credit-card is required");
			show_form();
			return();
		}

		if (! $last4 or !$BBD->isNumber ($last4) or length ($last4) != 4 ) {
			undef ($last4);
			$BBD->error ("Last 4 digits on credit-card are required");
			show_form();
			return();
		}

		if ($SPN and $SPN=~ /^(\d+)#$/ ) {
			$SPN = $1;
		}

		if (! $SPN or !$SPN=~ /^\d+$/ ) {
			undef ($SPN);
			$BBD->error ("The SPN is required.  This is the code you entered at the keypad on the BBD9000");
			show_form();
			return();
		}

		my ($DB_memb_id,$DB_spn,$DB_name);
		my $sth = $DBH->prepare(GET_MEMBER_BY_CC_NAME_LAST4) or die "Could not prepare handle";
		$sth->execute ($CC_name,$last4);
		$sth->bind_columns(\$DB_memb_id,\$DB_spn,\$DB_name);
	
		my $auth_memb_id;
		while($sth->fetch()) {
			if (crypt($SPN,$DB_spn) eq $DB_spn) {
				$auth_memb_id = $DB_memb_id;
				last;
			}
		}
	
		undef ($SPN);
		undef ($DB_spn);

		
		if ($auth_memb_id) {
			$BBD->send_reset_email ($email,$auth_memb_id,$DB_name);
			$success = 1;
			show_form();
			return();
		} else {
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
		CC_name => $CC_name,
		last4   => $last4,
		email   => $email,
		SUCCESS => $success
	);

	$BBD->HTML_out();

}

