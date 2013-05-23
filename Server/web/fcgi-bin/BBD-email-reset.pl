#!/usr/bin/perl -w
# Reset a member's login credentials
# Must pass in a valid SID as a CGI parameter
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
our $BBD = new BBD('BBD-email-reset.pl');


use constant UPDATE_EMAIL_BY_ID => <<"SQL";
	UPDATE members
	SET email = ?
	WHERE member_id = ?
SQL


$BBD->require_login (0);
$BBD->myTemplate ('BBD-email-reset.tmpl');


###
# Our globals

$BBD->init(\&do_request);

sub do_request {
	
	my $CGI = $BBD->CGI();
	my $DBH = $BBD->DBH();
	
	if ($BBD->session_was_expired() ) {
		$BBD->session_delete();
		$BBD->fatal (<<END);
	Looks like you've waited too long to click on the link in the email.<br>
	Please contact us to try again.
END
		return undef;
	}
	
	if ($BBD->session_is_new()
		or ! $BBD->session_param('email_reset')
		or !$BBD->session_param ('member_id')) {
			$BBD->session_delete() unless $BBD->session_param('logged_in');
			$BBD->fatal ("Not a valid email validation URL");
			return undef;
	}
	
	my $member_id =  $BBD->session_param ('member_id');
	my $member_info = $BBD->get_member_info ($member_id);
	if (not $member_info->{memb_id}) {
		$BBD->fatal ("Not a valid email validation URL");
		return undef;
	}
	
	
	# Update the email address if it was in the session.
	my $email = $member_info->{email};
	if ($BBD->session_param('email') ne $email) {
		$email = $BBD->session_param('email');
		$DBH->do (UPDATE_EMAIL_BY_ID, undef, $email, $member_id);
	}
	
	# We don't want to use the login-reset session as a regular session (one-time use)
	$BBD->session_delete();
	
	$BBD->{TEMPLATE}->param(
		email => $email,
		memb_name => $member_info->{memb_name},
	);
	
	# Note that we deleted the session, so this will not print a cookie
	$BBD->HTML_out();

	return undef;

}
