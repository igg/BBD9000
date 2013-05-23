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
our $BBD = new BBD('BBD-login-reset.pl');

use constant UPDATE_LOGON_BY_ID => <<"SQL";
	UPDATE members
	SET
		login = ?,
		password = ?
	WHERE
		member_id = ?
SQL

use constant GET_MEMBER_ID_BY_LOGIN => <<"SQL";
	SELECT member_id FROM members WHERE login = ?
SQL


use constant UPDATE_EMAIL_BY_ID => <<"SQL";
	UPDATE members
	SET email = ?
	WHERE member_id = ?
SQL


# fuel credit for updating email
use constant UPDATE_SET_EMAIL_FUEL_CREDIT => <<"SQL";
	UPDATE members
	SET credit = credit + IFNULL((
		SELECT price FROM promotions
		WHERE
			item = "set_email_fuel_credit"
			AND (number_left > 0 OR number_left IS NULL)
			AND ( (now() > date_start  AND now() < date_end) OR (date_start IS NULL AND date_end IS NULL) )
			ORDER BY price DESC LIMIT 1
	),0)
	WHERE member_id = ?
SQL



$BBD->require_login (0);
$BBD->myTemplate ('BBD-login-reset.tmpl');

###
# Our globals
our ($CGI,$DBH,$member_id,$member_info,$email);

$BBD->init(\&do_request);

sub do_request {
	
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	
	if ($BBD->session_was_expired() ) {
		$BBD->session_delete();
		$BBD->fatal (<<END);
	Looks like you've waited too long to click on the link in the email.<br>
	Please contact us to try again.
END
		return undef;
	}
	
	if ($BBD->session_is_new()
		or ! $BBD->session_param('login_reset')
		or !$BBD->session_param ('member_id')) {
			$BBD->session_delete() unless $BBD->session_param('logged_in');
			$BBD->fatal ("Not a valid login-reset URL");
			return undef;
	}
	
	$member_id =  $BBD->session_param ('member_id');
	$member_info = $BBD->get_member_info ($member_id);
	if (not $member_info->{memb_id}) {
		$BBD->fatal ("Not a valid login-reset URL");
		return undef;
	}
	
	# Update the email address if it was in the session.
	$email = $member_info->{email};
	if ($BBD->session_param('email') and $BBD->session_param('email') ne $member_info->{email}) {
		$email = $BBD->session_param('email');
		$DBH->do (UPDATE_EMAIL_BY_ID, undef, $email, $member_id);
		# If the email in the DB was blank, give the user a fuel credit
		if ($member_info->{type} eq 'FULL'    # Only full memberships get a credit
			and ($member_info->{is_primary})  # Only the primary membership holder gets a credit
			and (not defined ($member_info->{email}) or not $member_info->{email} =~ /@/) ) {
				$DBH->do (UPDATE_SET_EMAIL_FUEL_CREDIT, undef, $member_id);
		}
	}
	
	
	# OK, so now we have a valid session for a valid member_id.
	
	# Either we're here to present a form to let the user reset their login/password, or
	# the login and password were sent, so we set them and send the user to their member page.
	if (!$CGI->param('submit')) {
		show_form();
		return undef;
	} else {
		# Decrypt the passwords if necessary.
		my $password1 = $BBD->decryptRSApasswd ($CGI->param('crypt_password1')) if $CGI->param('crypt_password1');
		my $password2 = $BBD->decryptRSApasswd ($CGI->param('crypt_password2')) if $CGI->param('crypt_password2');
		$password1 = $CGI->param('password1') if not defined $password1 and $CGI->param('password1');
		$password2 = $CGI->param('password2') if not defined $password2 and $CGI->param('password2');
		$CGI->param('password1','');
		$CGI->param('password2','');
		$CGI->param('crypt_password1','');
		$CGI->param('crypt_password2','');
		my $login = $BBD->safeCGIparam ('login');
		$CGI->param('login','');

		if ($password1 and not $password2 or $password2 and not $password1) {
			$BBD->error("The two passwords do not match.");
			show_form();
			return undef;
		} elsif ($password1 and $password2) {
			if ($password1 ne $password2) {
				$BBD->error("The two passwords do not match.");
				show_form();
				return undef;
			}
			if (! $BBD->safeCGIstring ($password1) ) {
				$BBD->error("Passwords contain invalid characters");
				show_form();
				return undef;
			}
			if (! $login ) {
				$BBD->error("Login contains invalid characters");
				show_form();
				return undef;
			}
			if (length ($password1) < 1 or length ($login) < 1) {
				$BBD->error("The login and password cannot be blank");
				show_form();
				return undef;
			}
		} else {
				$BBD->error("Passwords cannot be blank");
				show_form();
				return undef;
		}
		# Have valid pasword and login
		my ($test_id) = $DBH->selectrow_array (GET_MEMBER_ID_BY_LOGIN, undef, $login);
		if ($test_id and $test_id != $member_id) {
			$BBD->error("The login name is not unique");
			show_form();
			return undef;
		}
		
	
		my $crypt_pass = crypt ($password1,join ('', ('.', '/', 0..9, 'A'..'Z', 'a'..'z')[rand 64, rand 64]));
		$DBH->do (UPDATE_LOGON_BY_ID, undef, $login, $crypt_pass, $member_id);
		
		# We don't want to use the login-reset session as a regular session (one-time use)
		$BBD->session_delete();
	
		# login will make a new session if we don't have one.
		$BBD->login ($login,$password1);
		
		# We don't want to remember any passwords
		undef ($login);
		undef ($password1);
		undef ($password2);

		# Most likely we want to adjust more membership info at this point,
		# so we go to the member page.
		$BBD->doRedirect('BBD-member.pl');
	}
}



sub show_form {
	my $login = $member_info->{login};
	$login = $email unless $login;
	
	$BBD->{TEMPLATE}->param(
		memb_name => $member_info->{memb_name},
		LOGIN => $login,
		modulus => $BBD->getRSAmodulus(),
		server_time => time(),
	);

	$BBD->HTML_out();

}
