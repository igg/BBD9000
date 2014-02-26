#!/usr/bin/perl -w
# Edit members personal info
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
our $BBD = new BBD('BBD-memberships.pl');

use constant GET_N_LAST_MEMBERSHIPS => <<"SQL";
	SELECT m.member_id,m.name,m.email,ms.membership_id,ms.membership_number,
		ms.type,ms.status,UNIX_TIMESTAMP(ms.start_date),UNIX_TIMESTAMP(ms.last_renewal)
	FROM members m, memberships ms
	WHERE m.membership_id = ms.membership_id
	ORDER BY ms.start_date DESC
	LIMIT ?
SQL

use constant GET_MEMBERSHIP_BY_ID => <<"SQL";
	SELECT m.member_id,m.name,m.email,ms.membership_id,ms.membership_number,
		ms.type,ms.status,UNIX_TIMESTAMP(ms.start_date),UNIX_TIMESTAMP(ms.last_renewal),UNIX_TIMESTAMP(ms.expires)
	FROM members m, memberships ms
	WHERE m.membership_id = ms.membership_id
	AND m.is_primary = 1
	AND ms.membership_id = ?
SQL

use constant GET_FAMILY_MEMBERS => <<"SQL";
	SELECT member_id, name, email, login, is_primary FROM members
	WHERE membership_id = ?
SQL

use constant IS_UNIQUE_MEMBERSHIP_NUMBER => <<"SQL";
	SELECT membership_number from memberships where membership_number = ?;
SQL

use constant UPDATE_MEMBERSHIP => <<"SQL";
	UPDATE memberships SET
		membership_number = ?,
		start_date = FROM_UNIXTIME(?),
		expires = FROM_UNIXTIME(?),
		last_renewal = FROM_UNIXTIME(?),
		type = ?,
		status = ?
	WHERE
		membership_id = ?
SQL

use constant UPDATE_MEMBER => <<"SQL";
	UPDATE members SET
		name = ?,
		email = ?
	WHERE
		member_id = ?
SQL

use constant UPDATE_PRIMARY => <<"SQL";
	UPDATE members SET
		is_primary = ?
	WHERE
		member_id = ?
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



use Scalar::Util qw(looks_like_number);

$BBD->myRole ('Memberships');
$BBD->myTemplate ('BBD-memberships.tmpl');

###
# Our globals
our ($CGI, $DBH, $member_id, $member_info);
our %membership_types;
our %membership_statuses;
our $edit_membership_tmpl_params;
our $new_membership_tmpl_params;
our $edit_membership_ms_id;
our %family_members;

$BBD->init(\&do_request);

sub do_request {

##
# Initialize our globals
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	%membership_types = ();
	%membership_statuses = ();
	$edit_membership_tmpl_params = undef;
	$new_membership_tmpl_params = undef;
	$edit_membership_ms_id = undef;
	%family_members = ();

	
	
	$member_id = $BBD->session_param ('member_id');
	$member_info = $BBD->get_member_info ($member_id);
	
	$BBD->relogin () unless $member_info->{ms_id};
	
	%membership_types = $BBD->get_membership_types();
	my $def_membership_type;
	$def_membership_type = 'FULL' if exists $membership_types{'FULL'};
	%membership_statuses = $BBD->get_membership_statuses();
	
	
	
	
	$edit_membership_ms_id = $BBD->safeCGIparam ('edit_membership_ms_id') || $BBD->session_param ('edit_membership_ms_id');
	$BBD->session_clear ('edit_membership_ms_id');
	$edit_membership_tmpl_params = select_edit_memb ($edit_membership_ms_id);
	
	
	
	
	$new_membership_tmpl_params = {
		new_name => $BBD->safeCGIparam ('new_name'),
		new_email => $BBD->safeCGIparam ('new_email'),
		new_start => $BBD->safeCGIparam ('new_start'),
		new_expires => $BBD->safeCGIparam ('new_expires'),
		new_type_popup => $CGI->popup_menu(
			-name=>'new_type_popup',
			# Sort by the values rather than the keys
			-values=> [sort { $membership_types{$a} cmp $membership_types{$b} } keys %membership_types],
			-labels=>\%membership_types,
			-class => 'popup_menu',
			-default => $BBD->safeCGIparam ('new_type_popup') ? $BBD->safeCGIparam ('new_type_popup') : $def_membership_type,
			-override => 1,
		),
		new_status_popup => $CGI->popup_menu(
			-name=>'new_status_popup',
			# Sort by the values rather than the keys
			-values=> [sort { $membership_statuses{$a} cmp $membership_statuses{$b} } keys %membership_statuses],
			-labels=>\%membership_statuses,
			-class => 'popup_menu',
			-default=> 'PENDING',
			-override => 1,
		),
	};
	$new_membership_tmpl_params->{new_email} = $BBD->safeCGIparam ('new_email');

	
	my @params = $CGI->param();
	foreach my $param (@params) {
		if ($param =~ /^select_(\d+)$/ and $1) {
			$edit_membership_tmpl_params = select_edit_memb ($1);
			show_form();
			return undef;
		}
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
	
	
	###
	# add_memb button
	if ( $BBD->safeCGIparam ('add_ms') ) {
		my ($new_name,$new_email) =
			verify_member_info (
				$new_membership_tmpl_params->{new_name},
				$new_membership_tmpl_params->{new_email},
			) or return undef;
		my ($new_start,$new_expires,undef,$new_type,$new_status) =
			verify_ms_info (
				$new_membership_tmpl_params->{new_start},
				$new_membership_tmpl_params->{new_expires},
				$new_membership_tmpl_params->{new_start},
				$BBD->safeCGIparam ('new_type_popup'),
				$BBD->safeCGIparam ('new_status_popup')
			) or return undef;
	
		if ( ($new_type eq 'FULL' or $new_type eq 'ONE-DAY' or $new_type eq 'EMPLOYEE') and $new_status ne 'PENDING') {
			$BBD->error ('Fueling memberships should start with a PENDING status.');
			show_form();
			return undef;
		}
		
		my $new_member_id = 
			$BBD->new_membership ($new_start,$new_expires,$new_type,$new_status,$new_name,$new_email,undef,undef,undef);

		$CGI->param ('new_name',undef);
		$CGI->param ('new_email',undef);
		$CGI->param ('new_start',undef);
		$CGI->param ('new_expires',undef);
		$new_membership_tmpl_params->{new_name} = undef;
		$new_membership_tmpl_params->{new_email} = undef;
		$new_membership_tmpl_params->{new_start} = undef;
		$new_membership_tmpl_params->{new_expires} = undef;
	
		show_form();
		return undef;
	
	###
	# update_memb button
	} elsif ( $BBD->safeCGIparam ('update_ms') and $edit_membership_ms_id ) {
		my ($upd_start,$upd_expires,$upd_renew,$upd_type,$upd_status) =
			verify_ms_info (
				$BBD->safeCGIparam ('edit_start'),
				$BBD->safeCGIparam ('edit_expires'),
				$BBD->safeCGIparam ('edit_renew'),
				$BBD->safeCGIparam ('edit_type_popup'),
				$BBD->safeCGIparam ('edit_status_popup')
			) or return undef;
		###
		# If we're changing the membership number, we have to make sure its unique
		my $old_ms_num = $edit_membership_tmpl_params->{edit_membership_ms_num};
		my $upd_ms_num = $old_ms_num;
		if ($BBD->isCGInumber ('edit_membership_ms_num')
			and $BBD->safeCGIparam ('edit_membership_ms_num') != $old_ms_num) {
				my $new_ms_num = $BBD->safeCGIparam ('edit_membership_ms_num');
				my ($test_num) = $DBH->selectrow_array (IS_UNIQUE_MEMBERSHIP_NUMBER,undef,$new_ms_num);
				$BBD->printLog ("Testing $new_ms_num returned $test_num");
				if ($test_num) {
					$BBD->error ("This membership number ($new_ms_num) is already taken.");
					show_form();
					return undef;
				} else {
					$upd_ms_num = $new_ms_num;
				}
		}

		$DBH->do (UPDATE_MEMBERSHIP,undef,
			$upd_ms_num,
			$upd_start,
			$upd_expires,
			$upd_renew,
			$upd_type,
			$upd_status,
			$edit_membership_ms_id,
		);
		
		####
		# Check if the primary membership changed
		my $primary = $BBD->safeCGIparam ('edit_primary_popup');
		if ($primary and $family_members{$primary} and not $family_members{$primary}->{is_primary}) {
			my $last_primary;
			foreach (keys %family_members) {
				$last_primary = $_ if $family_members{$_}->{is_primary};
			}
			$DBH->do (UPDATE_PRIMARY,undef,0,$last_primary) if $last_primary;
			$DBH->do (UPDATE_PRIMARY,undef,1,$primary);
		}

		$edit_membership_tmpl_params = select_edit_memb ($edit_membership_ms_id);
	
	} elsif ($BBD->safeCGIparam('add_memb') and $edit_membership_ms_id and $edit_membership_tmpl_params->{can_add_member}) {
		my $primary;
		foreach (keys %family_members) {
			$primary = $_ if $family_members{$_}->{is_primary};
		}
		$member_info = $BBD->get_member_info ($primary);
	
	
		$DBH->do (INSERT_FAMILY_MEMBER, undef,
			$edit_membership_ms_id,
			defined $member_info->{ad1} ? $member_info->{ad1} : '',
			defined $member_info->{ad2} ? $member_info->{ad2} : '',
			defined $member_info->{city} ? $member_info->{city} : '',
			defined $member_info->{state} ? $member_info->{state} : '',
			defined $member_info->{zip} ? $member_info->{zip} : '',
			defined $member_info->{h_ph} ? $member_info->{h_ph} : '',
		);
		my $new_member_id = $DBH->selectrow_array (GET_NEW_MEMBER_ID);
		edit_memb ($new_member_id);
		return undef;
	}
	
	
	
	
	
	show_form();
}








sub select_edit_memb {
	# This is the primary member's membership_id (not member_id)
	my $membership = shift;
	return undef unless $membership;

	# Make sure the membership exists
	my ($memb_id,$memb_name,$memb_email,$ms_membership_id,$ms_membership_number,
		$ms_type,$ms_status,$ms_start,$ms_renew,$ms_expires) = $DBH->selectrow_array (GET_MEMBERSHIP_BY_ID,undef,$membership);

	return undef unless $ms_membership_id;

	my %edit_send_email_popup = (
		invite => 'Invite',
		email_confirm => 'Address Confirm',
		login_reset => 'Login Reset',
	);

	$edit_membership_ms_id = $ms_membership_id;
	$CGI->param ('edit_membership_ms_id',$edit_membership_ms_id);

	my %tmpl_param = (
		edit_membership_ms_id   => $edit_membership_ms_id,
		edit_membership_ms_num  => $ms_membership_number,		
		edit_start => $BBD->epoch_to_ISOdatetime ($ms_start),
		edit_renew => $BBD->epoch_to_ISOdate ($ms_renew,'GMT'),
		edit_expires => $BBD->epoch_to_ISOdate ($ms_expires,'GMT'),
		edit_type_popup => $CGI->popup_menu(
			-name=>'edit_type_popup',
			# Sort by the values rather than the keys
			-values=> [sort { $membership_types{$a} cmp $membership_types{$b} } keys %membership_types],
			-labels=>\%membership_types,
			-class => 'popup_menu',
			-default=>$ms_type,
			-override => 1,
		),
		edit_status_popup => $CGI->popup_menu(
			-name=>'edit_status_popup',
			# Sort by the values rather than the keys
			-values=> [sort { $membership_statuses{$a} cmp $membership_statuses{$b} } keys %membership_statuses],
			-labels=>\%membership_statuses,
			-class => 'popup_menu',
			-default=>$ms_status,
			-override => 1,
		),
	);

	####
	# The members in the selected membership

	my @family_memberships = ();
	my @popup_values;
	my %popup_hash;

	my $sth = $DBH->prepare(GET_FAMILY_MEMBERS) or die "Could not prepare handle";
	$sth->execute( $edit_membership_ms_id );
	my ($f_member_id,$f_name,$f_email,$f_login,$f_is_primary);

	$sth->bind_columns (\$f_member_id,\$f_name,\$f_email,\$f_login,\$f_is_primary);
	while($sth->fetch()) {
		my $member = {
			name => $f_name,
			email => $f_email,
			logged_in => length ($f_login) > 0 ? 'Yes' : 'No',
			edit_id => "edit_$f_member_id",
			delete_id => "delete_$f_member_id",
			send_email_id => "send_email_$f_member_id",
			send_email_popup_id => $CGI->popup_menu(
				-name=>"send_email_popup_$f_member_id",
				# Sort by the values rather than the keys
				-values=> [keys %edit_send_email_popup],
				-labels=>\%edit_send_email_popup,
				-class => 'popup_menu',
			),
		};
		push (@family_memberships,$member);

		if ($f_is_primary) {
			unshift (@popup_values,$f_member_id);
		} else {
			push (@popup_values,$f_member_id);
		}
		$popup_hash{$f_member_id} = $f_name;

		$family_members{$f_member_id} = {
			name => $f_name,
			email => $f_email,
			login => $f_login,
			is_primary => $f_is_primary,
		};
	}
	$tmpl_param{edit_family_memberships} = \@family_memberships;
	$tmpl_param{edit_primary_popup} = $CGI->popup_menu(
		-name=>"edit_primary_popup",
		# Sort by the values rather than the keys
		-values=> \@popup_values,
		-labels=> \%popup_hash,
		-class => 'popup_menu',
		-default => $popup_values[0],
		-override =>1,
	);
	$tmpl_param{can_add_member} = 1 if scalar(@family_memberships) < 5;

	return \%tmpl_param;
	
}

sub verify_member_info {
	my ($new_name,$new_email) = @_;

	if (!$new_name) {
		$BBD->error ('Membership name cannot be blank');
		show_form();
		return undef;
	}
	
	if (!$new_email or not $new_email =~ /@/) {
		$BBD->error ('Email does not appear to be valid');
		show_form();
		return undef;
	}
	
	
	return ($new_name,$new_email);
}

sub verify_ms_info {
	my ($new_start,$new_expires,$new_renew,$new_type,$new_status) = @_;
	
	# Javascript should have converted to a local unix timestamp
	if (!$new_start or not looks_like_number ($new_start) ) {
		$BBD->error ('Invalid start date format');
		show_form();
		return undef;
	}
	
	if (!$new_expires or not looks_like_number ($new_expires) ) {
		$BBD->error ('Invalid expiration date format');
		show_form();
		return undef;
	}
	
	if (!$new_renew or not looks_like_number ($new_renew) ) {
		$BBD->error ('Invalid renewal date format');
		show_form();
		return undef;
	}
	
	if (!$new_type or not exists $membership_types {$new_type}) {
		$BBD->error ('Invalid membership type');
		show_form();
		return undef;
	}
	
	if (!$new_status or not exists $membership_statuses {$new_status}) {
		$BBD->error ('Invalid status');
		show_form();
		return undef;
	}
	
	return ($new_start,$new_expires,$new_renew,$new_type,$new_status);
}





sub delete_memb {
	if ($BBD->delete_member(shift)) {
		$BBD->error ("Member deleted");
	}
	$edit_membership_tmpl_params = select_edit_memb ($BBD->safeCGIparam ('edit_membership_ms_id'));

	show_form();
}

sub edit_memb {
	my $family_member = shift;
	$BBD->session_param ('edit_membership_ms_id',$edit_membership_ms_id);
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
	my $last_n_memberships = $BBD->safeCGIparam ('last_n_memberships')
		if ( $BBD->safeCGIparam ('last_n_memberships') and $BBD->isCGInumber ('last_n_memberships') );
	$last_n_memberships = 250 unless $last_n_memberships;

	my @last_members;
	my ($memb_id,$memb_name,$memb_email,$ms_membership_id,$ms_membership_number,
		$ms_type,$ms_status,$ms_start,$ms_renew);
	my $sth = $DBH->prepare(GET_N_LAST_MEMBERSHIPS) or die "Could not prepare handle";
	$sth->execute( $last_n_memberships );
	$sth->bind_columns (\$memb_id,\$memb_name,\$memb_email,\$ms_membership_id,\$ms_membership_number,
		\$ms_type,\$ms_status,\$ms_start,\$ms_renew);
	while($sth->fetch()) {
		push (@last_members, {
			last_m_name => $memb_name,
			last_m_email => $memb_email,
			last_m_number => $ms_membership_number,
			last_m_start => $BBD->epoch_to_ISOdate ($ms_start),
			last_m_renew => $BBD->epoch_to_ISOdate ($ms_renew),
			last_m_type => $ms_type,
			last_m_status => $ms_status,
			last_m_select_id => 'select_'.$ms_membership_id,
		});
	}





	$BBD->{TEMPLATE}->param(
		last_n_memberships => $last_n_memberships,
		last_members => \@last_members,
	);
	
	$BBD->{TEMPLATE}->param(%$edit_membership_tmpl_params) if $edit_membership_tmpl_params;
	$BBD->{TEMPLATE}->param(%$new_membership_tmpl_params) if $new_membership_tmpl_params;
	

	$BBD->HTML_out();
}
