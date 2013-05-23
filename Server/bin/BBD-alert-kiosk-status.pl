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

use constant GET_KIOSK_INFO => <<"SQL";
	SELECT k.kiosk_id,k.name,k.is_online,UNIX_TIMESTAMP(k.last_checkin),UNIX_TIMESTAMP(k.last_status),k.status,
		k.address1,k.address2,k.city,k.state,k.zip,k.timezone
		FROM kiosks k
                WHERE (k.last_checkin + INTERVAL k.checkin_delta_h HOUR) < NOW()
SQL

use constant UPDATE_KIOSK_IS_ONLINE_FALSE => <<"SQL";
	UPDATE kiosks k
	SET k.is_online = 0
		WHERE k.kiosk_id = ?
SQL


use constant GET_ALERT_MEMBER_EMAILS => <<"SQL";
	SELECT m.name, m.email
	FROM members m,
		alert_roles ar,
		member_roles mr
	WHERE m.member_id = mr.member_id
	AND mr.role_id = ar.role_id
	AND ar.alert_id = 'Network'
SQL

our $DT_DATE_FORMAT = DateTime::Format::Strptime->new(pattern => '%D %r');

my $BBD = new BBD('BBD-alert-kiosk-status.pl');
$BBD->require_login (0);

$BBD->init(\&do_request);
exit;


sub do_request {
	$BBD->session_delete();
	die "Urp?\n" unless $BBD->{CGI}->remote_host() eq 'localhost';
	my $DBH = $BBD->DBH();

	# Collect the kiosk czars' emails
	my ($memb_name,$memb_email);
	my @TOs;
	my $sth = $DBH->prepare(GET_ALERT_MEMBER_EMAILS) or die "Could not prepare handle";
	$sth->execute ();
	$sth->bind_columns(\$memb_name,\$memb_email);
	while($sth->fetch()) {
		push (@TOs,"$memb_name <$memb_email>");
	}

	
	my ($kiosk_id,$name,$is_online,$last_chk,$last_stat,$stat,$ad1,$ad2,$city,$state,$zip,$timezone);

	$sth = $DBH->prepare(GET_KIOSK_INFO) or die "Could not prepare handle";
	$sth->execute () or die "Could not execute GET_KIOSK_INFO";
	
	$sth->bind_columns(\$kiosk_id,\$name,\$is_online,\$last_chk,\$last_stat,\$stat,\$ad1,\$ad2,\$city,\$state,\$zip,\$timezone);
	while($sth->fetch()) {
		# if it was considered online before, then make is_online false, and send email.
		# otherwise, ignore.
		next if defined $is_online and not $is_online;
		
		$DBH->do (UPDATE_KIOSK_IS_ONLINE_FALSE,undef,$kiosk_id);
		$BBD->setWebsiteKioskStatus ($kiosk_id,0);
		my $address = $ad1;
		$address .= "\n$ad2" if $ad2;
		my $dt_last_chk = DateTime->from_epoch(
			epoch => $last_chk, formatter => $DT_DATE_FORMAT, time_zone => $timezone);
		my $dt_last_stat = DateTime->from_epoch(
			epoch => $last_stat, formatter => $DT_DATE_FORMAT, time_zone => $timezone);
		my $last_heard = sprintf ("%.0f",(time() - $last_chk)/3600.0);
		my $hours = $last_heard == 1 ? 'hour' : 'hours';
		$BBD->send_email (
			To      => join (' , ',@TOs),
			From    => 'Baltimore Biodiesel Coop <donotreply@baltimorebiodiesel.org>',
			Subject => "BBD Kiosk \"$name\" off-line for $last_heard $hours",
			Message => <<"TEXT");
Last request from "$name" on $dt_last_chk
Last reported status: "$stat" on $dt_last_stat

Kiosk Address:
$name
$address
$city, $state $zip
TEXT
	}

}



