#!/usr/bin/perl -w
# -----------------------------------------------------------------------------
# BBD-family_memberships.pl:  Present and process a form with family members
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
our $BBD = new BBD('BBD-family_memberships.pl');

use constant GET_FAMILY_MEMBERS => <<"SQL";
	SELECT member_id, name, email, login FROM members
	WHERE membership_id = ?
	AND is_primary = 0
SQL

use constant INSERT_FAMILY_MEMBER => <<"SQL";
	INSERT INTO members SET
		name = 'New Family Member',
		is_primary = 0,
		first_name = '',
		last_name = '',
		email = '',
		pin = ENCRYPT(8675309),
		membership_id = ?,
		address1 = ?,
		address2 = ?,
		city = ?,
		state = ?,
		zip = ?,
		home_phone = ?,
		work_phone = '',
		mobile_phone = ''
SQL

use constant GET_NEW_MEMBER_ID => <<"SQL";
	SELECT LAST_INSERT_ID()
SQL

$BBD->myRole ('Family Memberships');
$BBD->myTemplate ('BBD-family_memberships.tmpl');

###
# Our globals
our ($CGI, $DBH, $member_id, $member_info);
our @family_memberships;
our %family_members;
our $can_add_member;

$BBD->init(\&do_request);

sub do_request {
	
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	
	
	
	$member_id = $BBD->session_param ('member_id');
	$member_info = $BBD->get_member_info ($member_id);
	
	$BBD->relogin () unless $member_info->{ms_id};
	
	get_family_members();
	
	my @params = $CGI->param();
	foreach my $param (@params) {
		if ( $param =~ /^edit_(\d+)$/ and $1 and exists $family_members{$1} ) {
			edit_memb ($1);
			return undef;
		}
		if ( $param =~ /^delete_(\d+)$/ and $1 and exists $family_members{$1} ) {
			delete_memb ($1);
			return undef;
		}
		if ( $param =~ /^send_email_(\d+)$/ and $1 and exists $family_members{$1} ) {
			email_memb ($1);
			return undef;
		}
	}
	
	if ($CGI->param('add_member')) {
		$DBH->do (INSERT_FAMILY_MEMBER, undef,
			$member_info->{ms_id},
			defined $member_info->{ad1} ? $member_info->{ad1} : '',
			defined $member_info->{ad2} ? $member_info->{ad2} : '',
			defined $member_info->{city} ? $member_info->{city} : '',
			defined $member_info->{state} ? $member_info->{state} : '',
			defined $member_info->{zip} ? $member_info->{zip} : '',
			defined $member_info->{h_ph} ? $member_info->{h_ph} : '',
		);
		my $new_member_id = $DBH->selectrow_array (GET_NEW_MEMBER_ID);
		edit_memb ($new_member_id);
	}
	show_form();
}



sub get_family_members {
	@family_memberships = ();
	%family_members = ();
	undef ($can_add_member);

	my %edit_send_email_popup = (
		invite => 'Invite',
		email_confirm => 'Confirm Address',
		login_reset => 'Login Reset',
	);

	my $sth = $DBH->prepare(GET_FAMILY_MEMBERS) or die "Could not prepare handle";
	$sth->execute( $member_info->{ms_id} );
	my ($member_id,$name,$email,$login);

	$sth->bind_columns (\$member_id,\$name,\$email,\$login);

	while($sth->fetch()) {
		my $member = {
			name => $name,
			email => $email,
			edit_id => "edit_$member_id",
			delete_id => "delete_$member_id",
			send_email_id => "send_email_$member_id",
			send_email_popup_id => $CGI->popup_menu(
				-name=>"send_email_popup_$member_id",
				# Sort by the values rather than the keys
				-values=> [keys %edit_send_email_popup],
				-labels=>\%edit_send_email_popup,
				-class => 'popup_menu',
			),
		};

		push (@family_memberships,$member);
		$family_members{$member_id} = {
			name => $name,
			email => $email,
		};
	}

	$can_add_member = 1 if scalar(@family_memberships) < 5;

}

sub delete_memb {
	if ($BBD->delete_member(shift)) {
		$BBD->error ("Member deleted");
	}
	get_family_members();

	show_form();
}

sub edit_memb {
	my $family_member = shift;
	$BBD->session_param ('edit_member_id',$family_member);
	$BBD->session_param ('redirect',$BBD->myURL());
	$BBD->doRedirect ('BBD-member.pl');
}

sub email_memb {
	my $family_member = shift;
	my $email_type = $CGI->param("send_email_popup_$family_member");
	return undef unless $email_type;
	if ($email_type eq 'invite') {
		$BBD->send_welcome_email (
			$family_members{$family_member}->{email},
			$family_member,
			$family_members{$family_member}->{name},
		);
		$BBD->error ('Invitation email send to '.$family_members{$family_member}->{name}.' <'.$family_members{$family_member}->{email}.'>');
	} elsif ($email_type eq 'login_reset') {
		$BBD->send_reset_email (
			$family_members{$family_member}->{email},
			$family_member,
			$family_members{$family_member}->{name},
		);
		$BBD->error ('Login-reset email send to '.$family_members{$family_member}->{name}.' <'.$family_members{$family_member}->{email}.'>');
	} elsif ($email_type eq 'email_confirm') {
		$BBD->send_conf_email (
			$family_members{$family_member}->{email},
			$family_member,
			$family_members{$family_member}->{name},
		);
		$BBD->error ('Confirmation email send to '.$family_members{$family_member}->{name}.' <'.$family_members{$family_member}->{email}.'>');
	}
	show_form();
}


sub show_form {
	# Populate family members

	$BBD->{TEMPLATE}->param(
		memb_name => $member_info->{memb_name},
		can_add_member => $can_add_member,
	);
	$BBD->{TEMPLATE}->param ('family_memberships',\@family_memberships);

	$BBD->HTML_out();
}


