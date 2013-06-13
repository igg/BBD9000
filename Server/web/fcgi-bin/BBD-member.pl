#!/usr/bin/perl -w
# -----------------------------------------------------------------------------
# BBD-member.pl:  Edit members personal info
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
our $BBD = new BBD('BBD-member.pl');

use constant UPDATE_USER_INFO => <<"SQL";
	UPDATE members SET
		credit = ?,
		name = ?,
		first_name = ?,
		last_name = ?,
		email = ?,
		fuel_preauth = ?,
		pin = ?,
		address1 = ?,
		address2 = ?,
		city = ?,
		state = ?,
		zip = ?,
		home_phone = ?,
		work_phone = ?,
		mobile_phone = ?
	WHERE member_id = ?
SQL

$BBD->myRole ('Member Info');
$BBD->myTemplate ('BBD-member.tmpl');

###
# Our globals
our ($CGI,$DBH,$member_id,$member_info,$sending_email);

$BBD->init(\&do_request);


sub do_request {
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	$sending_email = undef;
	
	$member_id = $BBD->session_param ('edit_member_id') || $BBD->session_param ('member_id');
	
	$sending_email = 1 unless $BBD->session_param ('edit_member_id');
	
	$member_info = $BBD->get_member_info ($member_id);
	$BBD->relogin () unless $member_info->{memb_id};
	
	# OK, so now we have a valid session for a valid member_id.
	my @updates;
	# Either we're here to present a form to let the user reset their login/password, or
	# the login and password were sent, so we set them and send the user to their member page.
	if (!$CGI->param('submit')) {
		show_form();
		return();
	} else {
		# Decrypt the SPN if it was set.
		my $SPN1 = $BBD->decryptRSApasswd ($CGI->param('crypt_SPN1')) if $CGI->param('crypt_SPN1');
		my $SPN2 = $BBD->decryptRSApasswd ($CGI->param('crypt_SPN2')) if $CGI->param('crypt_SPN2');
		$SPN1 = $CGI->param('SPN1') if not defined $SPN1 and $CGI->param('SPN1');
		$SPN2 = $CGI->param('SPN2') if not defined $SPN2 and $CGI->param('SPN2');
		if ( ($SPN1 and not $SPN2) or ($SPN2 and not $SPN1) ) {
			$BBD->error ("The two SPNs do not match.");
			show_form();
			return();
		} elsif ($SPN1 and $SPN2) {
			if ($SPN1 ne $SPN2) {
				$BBD->error ("The two SPNs do not match.");
				show_form();
				return();
			} elsif (length ($SPN1) < 4 or length ($SPN1) > 20) {
				$BBD->error ("SPNs must be between 4 and 20 digits long.");
				show_form();
				return();
			} elsif (not $SPN1 =~ /^\d+$/) {
				$BBD->error ("SPNs must consist of digits only.");
				show_form();
				return();
			} else {
				$member_info->{pin} = crypt ($SPN1,join ('', ('.', '/', 0..9, 'A'..'Z', 'a'..'z')[rand 64, rand 64]));
				push (@updates,'SPN');
			}
		}
		
		if (defined $CGI->param('credit') and $BBD->has_role ('Memberships') ) {			
			if ( $BBD->isNumber($CGI->param('credit')) ) {
				if ($member_info->{credit} != $CGI->param('credit')) {
					$member_info->{credit} = $CGI->param('credit');
					push (@updates,'credit');
				}
			} else {
				$BBD->error ("The Credit field does not look like a number.");
				show_form();
				return();
			}
		}
		
		if (defined $CGI->param('memb_roles') and $BBD->has_role ('Roles') ) {
			my $roles_changed;
			my @new_roles = $CGI->param('memb_roles');
			my @old_roles = $BBD->get_member_roles($member_id);
			# add new roles
			foreach my $role (@new_roles) {
				if (! grep {$_ eq $role} @old_roles) {
					$BBD->add_role ($member_id, $role);
					push (@updates,"Role '$role' (added)");
					$roles_changed = 1;
				}
			}
			# remove old roles
			foreach my $role (@old_roles) {
				if (! grep {$_ eq $role} @new_roles) {
					$BBD->remove_role ($member_id, $role);
					push (@updates,"Role '$role' (removed)");
					$roles_changed = 1;
				}
			}
			push (@updates,"Role changes take effect after logout/login")
				if $roles_changed and not $BBD->session_param ('edit_member_id');
		}



		if (!$CGI->param('memb_fname') or $CGI->param('memb_fname') ne $member_info->{memb_fname}) {
			if (! $CGI->param('memb_fname') ) {
				$BBD->error ("First and last names cannot be blank.");
				show_form();
				return();
			} elsif (! $CGI->param('memb_fname') =~ /^[a-zA-Z-]$/) {
				$BBD->error ("First and last names cannot contain spaces or punctuation.");
				show_form();
				return();
			} else {
				$member_info->{memb_fname} = $CGI->param('memb_fname');
				push (@updates,'first name');
			}
		}
		if (! $CGI->param('memb_lname') or $CGI->param('memb_lname') ne $member_info->{memb_lname}) {
			if (! $CGI->param('memb_lname') ) {
				$BBD->error ("First and last names cannot be blank.");
				show_form();
				return();
			} elsif (! $CGI->param('memb_lname') =~ /^[a-zA-Z-]$/) {
				$BBD->error ("First and last names cannot contain spaces or punctuation.");
				show_form();
				return();
			} else {
				$member_info->{memb_lname} = $CGI->param('memb_lname');
				push (@updates,'last name');
			}
		}
		# The first name, last name, SPN combination must be unique
		my $test_unique = $BBD->check_unique_SPN_names ($member_info->{memb_fname},$member_info->{memb_lname},$SPN1);
		if ($test_unique and $test_unique != $member_id) {
			$BBD->error ("Sorry, someone has taken this SPN! Please try another.");
			show_form();
			return();
		}
		
		# If the email changed, send confirmation email
		my $email = $BBD->safeCGIparam ('email');
		if (!$email or $email ne $member_info->{email}) {
			if (!$email) {
				$BBD->error ("Email cannot be blank.");
				show_form();
				return();
			} elsif ($sending_email and $email and $email =~ /@/) {
				$BBD->send_conf_email ($email,$member_id,$member_info->{memb_name});
				push (@updates,'email (pending confirmation)');
			} elsif ($email and $email =~ /@/) {
				$member_info->{email} = $CGI->param('email');
				push (@updates,'email');
			} else {
				$BBD->error ("Not a valid email address.");
				show_form();
				return();
			}
		}

		my $fuel_preauth = $BBD->safeCGIparam ('fuel_preauth');
		if (! $fuel_preauth or $fuel_preauth != $member_info->{fuel_preauth}) {
			if (! $fuel_preauth ) {
				$BBD->error ("Pre-authorization amount cannot be blank.");
				show_form();
				return();
			} elsif ( not $BBD->isCGInumber ('fuel_preauth') ) {
				$BBD->error ("The pre-authorization amount must be a number.");
				show_form();
				return();
			} elsif ( $fuel_preauth < 1 ) {
				$BBD->error ("The pre-authorization amount must be 1 gallon or more.");
				show_form();
				return();
			} else  {
				$member_info->{fuel_preauth} = sprintf ('%.1f',$fuel_preauth);
				push (@updates,'CC Pre-authorization');
			}
		}

		my $name = $BBD->safeCGIparam ('memb_name');
		if (!$name) {
			$BBD->error ("The member's name cannot be blank.");
			show_form();
			return();
		}
		if ($name ne $member_info->{memb_name}) {
			$member_info->{memb_name} = $name;
			push (@updates,'member name');
		}
	
		if ( $BBD->safeCGIparam ('ad1') and $CGI->param('ad1') ne $member_info->{ad1} ) {
			$member_info->{ad1} = $CGI->param('ad1');
			push (@updates,'address 1');
		}
		if ( $BBD->safeCGIparam ('ad2') and $CGI->param('ad2') ne $member_info->{ad2}) {
			$member_info->{ad2} = $CGI->param('ad2');
			push (@updates,'address 2');
		}
		if ( $BBD->safeCGIparam ('city') and $CGI->param('city') ne $member_info->{city}) {
			$member_info->{city} = $CGI->param('city');
			push (@updates,'city');
		}
		if ( $BBD->safeCGIparam ('state') and $CGI->param('state') ne $member_info->{state}) {
			$member_info->{state} = $CGI->param('state');
			push (@updates,'state');
		}
		if ( $BBD->safeCGIparam ('zip') and $CGI->param('zip') ne $member_info->{zip}) {
			$member_info->{zip} = $CGI->param('zip');
			push (@updates,'zip');
		}
		if ( $BBD->safeCGIparam ('h_ph') and $CGI->param('h_ph') ne $member_info->{h_ph}) {
			$member_info->{h_ph} = $CGI->param('h_ph');
			push (@updates,'home phone');
		}
		if ( $BBD->safeCGIparam ('w_ph') and $CGI->param('w_ph') ne $member_info->{w_ph}) {
			$member_info->{w_ph} = $CGI->param('w_ph');
			push (@updates,'work phone');
		}
		if ( $BBD->safeCGIparam ('m_ph') and $CGI->param('m_ph') ne $member_info->{m_ph}) {
			$member_info->{m_ph} = $CGI->param('m_ph');
			push (@updates,'mobile phone');
		}
		$DBH->do (UPDATE_USER_INFO, undef,
			$member_info->{credit},
			$member_info->{memb_name}, $member_info->{memb_fname}, $member_info->{memb_lname},
			$member_info->{email}, $member_info->{fuel_preauth}, $member_info->{pin},
			$member_info->{ad1}, $member_info->{ad2},
			$member_info->{city}, $member_info->{state}, $member_info->{zip},
			$member_info->{h_ph},$member_info->{w_ph},$member_info->{m_ph},
			$member_id
		);
		if ( $BBD->session_param ('edit_member_id') ) {		
			$BBD->doRedirect ();
		} else {
			$BBD->error ("Successfully updated ".join (', ',@updates).'.')
				if (scalar @updates);
			show_form();
			return();
		}
	}
}

sub show_form {

	my $memb_id = $member_info->{memb_id};
	# delete some of the member stuff from the standard query we don't want in the form.
	delete $member_info->{pin};
	delete $member_info->{memb_id};
	delete $member_info->{ms_id};
	delete $member_info->{login};
	delete $member_info->{password};
	delete $member_info->{is_primary};
	
	# change the format of some of the elements
	$member_info->{start} = $BBD->epoch_to_date ($member_info->{start});
	$member_info->{renew} = $BBD->epoch_to_date ($member_info->{renew});
	$member_info->{expires} = $BBD->epoch_to_date ($member_info->{expires});
	$member_info->{credit} = sprintf ('%.2f',$member_info->{credit});
	$member_info->{fuel_preauth} = sprintf ('%.1f',$member_info->{fuel_preauth});

	# Add additional form elements
	$member_info->{sending_email} = $sending_email;
	
	# Add RSA elements
	$member_info->{modulus} = $BBD->getRSAmodulus();
	$member_info->{server_time} = time();
	
	my @member_roles = $BBD->get_member_roles($memb_id);
	if ($BBD->has_role ('Roles')) {
		$member_info->{roles_display} = 1;
		my $all_roles = $BBD->get_all_roles_hashref();
		my @all_roles_tmpl;
		foreach my $role (keys %$all_roles) {
			push (@all_roles_tmpl, {
				role => $role,
				roles_editable => 1,
				role_check => (grep {$_ eq $role} @member_roles) ? 'checked' : '',
				role_desc => $all_roles->{$role}->{description} ? $all_roles->{$role}->{description} : '',
				role_membs => join (', ', @{$all_roles->{$role}->{members}})
			});
		}
		$member_info->{all_roles} = \@all_roles_tmpl;
	} elsif (scalar (@member_roles)) {
		$member_info->{roles_display} = 1;
		my $all_roles = $BBD->get_all_roles_hashref();
		my @all_roles_tmpl;
		foreach my $role (@member_roles) {
			push (@all_roles_tmpl, {
				role => $role,
				roles_editable => 0,
				role_check => 'checked',
				role_desc => $all_roles->{$role}->{description} ? $all_roles->{$role}->{description} : '',
				role_membs => join (', ', @{$all_roles->{$role}->{members}})
			});
		}
		$member_info->{all_roles} = \@all_roles_tmpl;
	} else {
		$member_info->{roles_display} = 0;
		$member_info->{all_roles} = [];
	}
	
	if ($BBD->has_role ('Memberships')) {
		$member_info->{ms_credit_editable} = 1;
	} else {
		$member_info->{ms_credit_editable} = 0;
	}

	$BBD->{TEMPLATE}->param (%$member_info);

	$BBD->HTML_out();

}
