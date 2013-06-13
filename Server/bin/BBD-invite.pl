#!/usr/bin/perl -w
# Send welcome email with SID
# -----------------------------------------------------------------------------
# BBD-first-logon.pl:  Generate a session and URL for a first-time user
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
my $BBD = new BBD();
$BBD->require_login (0);

use constant COUNT_MEMBERS => <<"SQL";
	SELECT COUNT(*) FROM members
SQL


$BBD->init(\&do_request);
exit;



sub do_request {
	$BBD->session_delete();
	print "Urp?\n" and die unless $BBD->{CGI}->remote_host() eq 'localhost';
	my $DBH = $BBD->DBH();
	my ($type,$name,$email,$spn);
	
	my $n_members = $DBH->selectrow_array (COUNT_MEMBERS);
	my $first_member = 1 unless $n_members > 0;
	my $memb_id;
	my $member_info;



	if (scalar (@ARGV)) {
		my @memb_ids = $BBD->get_memb_ids_by_name ($ARGV[0]);
		if (scalar (@memb_ids) == 1) {
			$memb_id = $memb_ids[0];
			$member_info = $BBD->get_member_info ($memb_id);
		} else {
			print "Error: Number of members with name '$ARGV[0]' is ".scalar (@memb_ids)."\n" and die;
		}
	} else {
		print "Adding new member. To send invite to existing member, supply the member name:\n";
		print "$0 'John K. Doe'\n";
		($type,$name,$email,$spn) = get_membership_info ();
		# ($new_start,$new_type,$new_status,$new_name,$new_email,$new_spn,$fuel_preauth,$price,$kiosk_id)
		$memb_id = $BBD->new_membership (time(),$type,'PENDING',$name,$email,$spn,undef,undef,undef);
		$member_info = $BBD->get_member_info ($memb_id);
	}




	print "Member_id $memb_id was not found in the DB\n" and die unless $member_info->{memb_id};
	$BBD->send_welcome_email ($member_info->{email},$memb_id,$member_info->{memb_name});
	print "Email sent to ".$member_info->{memb_name}." <".$member_info->{email}.">\n";
	
	if ($first_member) {
		print "First member\n" if $first_member;
		my @all_roles = $BBD->get_all_roles();
		print "       All roles: ".join (',',@all_roles)."\n";
		foreach my $role (@all_roles) {
			$BBD->add_role ($memb_id,$role) unless $role eq 'Supplier';
		}
		my @member_roles = $BBD->get_member_roles($memb_id);
		print "New Member roles: ".join (', ',@member_roles)."\n";
	}

	# We don't need the session that was made for us
	$BBD->session_delete();
}

sub promptUser {
   my($promptString,$defaultValue) = @_;
   if ($defaultValue) {
      print $promptString, "[", $defaultValue, "]: ";
   } else {
      print $promptString, ": ";
   }

   $| = 1;               # force a flush after our print
   $_ = <STDIN>;         # get the input from STDIN (presumably the keyboard)
   chomp;

   if ($defaultValue) {
      return $_ ? $_ : $defaultValue;    # return $_ if it has a value
   } else {
      return $_;
   }
}


sub get_membership_info {
	my ($type,$name,$email,$spn);
	my %membership_types = $BBD->get_membership_types();

	my $confirmed;
	do {
		$name = promptUser ("Name (first last)",$name);
	
		do {
			$type = promptUser ("Type (".join (',',keys(%membership_types)).")", $type);
			print "unrecognized membership type\n" unless exists $membership_types{$type};
		} while (! exists $membership_types{$type});

		$email = promptUser ("email",$email);
	
	
		do { {
			$spn = promptUser ("SPN");
			print "SPN too short\n" and $spn = undef if $spn and length($spn) < 4;
			print "SPN too long\n" and $spn = undef if $spn and length($spn) > 20;
			print "contains non-numeric characters\n" and $spn = undef if $spn and not $spn =~ /^\d+$/;
			$spn = crypt ($spn,join ('', ('.', '/', 0..9, 'A'..'Z', 'a'..'z')[rand 64, rand 64])) if $spn;
		} } while (! $spn);
		
		print "Name: [$name]\n";
		print "Type: [$type]\n";
		print "Email: [$email]\n";
		print "SPN (hashed): [$spn]\n";
		print "\n";
		$confirmed = promptUser ("Accept?","y");
	} while not $confirmed =~ /^[yY]$/;
	return ($type,$name,$email,$spn);
	
	
}


