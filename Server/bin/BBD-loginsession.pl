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

use constant GET_MEMBER_ROLES => <<"SQL";
	SELECT roles.role_id, roles.script FROM roles,member_roles
	WHERE roles.role_id = member_roles.role_id
	AND member_roles.member_id= ?
SQL



$BBD->init(\&do_request);
exit;



sub do_request {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	my $DBH = $BBD->DBH();
	
	my $memb_id = $ARGV[0];
	
	if (not $memb_id) {
		print <<"END";
$0 Generate a log-in session as any member.
You must provide a member_id as a parameter, i.e.:
$0 1
  Generate a login session as member_id 1.

END
	exit;
	}
	my $member_info = $BBD->get_member_info ($memb_id);
	
	die "Member_id $memb_id was not found in the DB" unless $member_info->{memb_id};

	
	# Make a new session.
	my $session = CGI::Session->new("driver:MySQL", 'crap', {Handle=>$DBH });

	# Set session stuff if successful
	$session->param ('logged_in',1);
	$session->param ('member_id',$memb_id);
	$session->expire('+30m');

	# Populate member roles
	# Default roles
	my @roles;
	my $default_uri;
	if ($member_info->{type} ne 'SUPPLIER') {
		push (@roles,{role => 'Purchases', script => 'BBD-purchases.pl'});
		push (@roles,{role => 'Family Purchases'}) if ($member_info->{is_primary});
		push (@roles,{role => 'Member Info', script => 'BBD-member.pl'});
		push (@roles,{role => 'Family Memberships', script => 'BBD-family_memberships.pl'})
			if ($member_info->{is_primary});
		$session->param ('default_uri','BBD-purchases.pl');
	} else {
		push (@roles,{role => 'Fuel Deliveries', script => 'BBD-fuel_deliveries.pl'});
		$session->param ('default_uri','BBD-fuel_deliveries.pl');
	}
	# Additional roles in the DB.
	my $sth = $DBH->prepare(GET_MEMBER_ROLES) or die "Could not prepare handle";
	$sth->execute( $memb_id );
	my ($role,$script);
	$sth->bind_columns (\$role,\$script);
	while($sth->fetch()) {
		push (@roles,{role => $role, script => $script});
	}
	$session->param ('roles',\@roles);
	my $sid = $session->id();

	print $BBD->{URL_BASE}.'/'.$session->param('default_uri')."?CGISESSID=$sid\n";
	$session->close();
	
}



