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

use constant UPDATE_MEMBERSHIP_RENEWAL => <<"SQL";
	UPDATE memberships ms
	SET ms.status = 'ASK_RENEWAL'
		WHERE ms.type != 'ONE-DAY'
		AND ms.type != 'SUPPLIER'
		AND ms.status != 'ASK_RENEWAL'
		AND ms.status != 'EXPIRED'
		AND TO_DAYS(ms.expires)-TO_DAYS(NOW())<?;
SQL

use constant UPDATE_MEMBERSHIP_EXPIRED => <<"SQL";
	UPDATE memberships ms
	SET ms.status = 'EXPIRED'
		WHERE ms.type != 'ONE-DAY'
		AND ms.type != 'SUPPLIER'
		AND ms.status != 'EXPIRED'
		AND TO_DAYS(NOW())-TO_DAYS(ms.expires)>?;
SQL

my $BBD = new BBD('BBD-membership-update.pl');
$BBD->require_login (0);

$BBD->init(\&do_request);
exit;


sub do_request {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	my $DBH = $BBD->DBH();
	
	my ($grace_before,$grace_after) = $BBD->getRenewalGracePeriod();

	$DBH->do (UPDATE_MEMBERSHIP_RENEWAL,undef,$BBD->getAskRenewDays());	
#	$DBH->do (UPDATE_MEMBERSHIP_EXPIRED,undef,$grace_after);	
	

}



