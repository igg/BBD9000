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



$BBD->init(\&do_request);
exit;



sub do_request {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	my $DBH = $BBD->DBH();
	
	while (<STDIN>) {
		chomp;
		my $memb_id = $_;
		my $member_info = $BBD->get_member_info ($memb_id);
		
		die "Member_id $memb_id was not found in the DB" unless $member_info->{memb_id};
		$BBD->send_welcome_email ($member_info->{email},$memb_id,$member_info->{memb_name});
		print "Email sent to ".$member_info->{memb_name}." <".$member_info->{email}.">\n";
	}
	# We don't need the session that was made for us
	$BBD->session_delete();
}



