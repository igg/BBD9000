#!/usr/bin/perl -w
# -----------------------------------------------------------------------------
# BBD-kiosk.pl:  The server-end of BBD9000 communications
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

use diagnostics;
use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;

use BBD;
our $BBD = new BBD('BBD-kiosk.pl');
$BBD->require_login (0);

# For gateway comms
use LWP::UserAgent;
use Sys::SigAction qw( timeout_call );


$BBD->init(\&do_request);

###
# Our globals
our ($CGI,$DBH,$kioskID,$member_id,$member_info);

use constant SALE_EPSILON => 0.0001;


# Queries - these may need to be modified to match the DB schema

use constant GET_USER_INFO_BY_ID => <<"SQL";
	SELECT m.name,m.first_name,m.last_name,m.member_id,m.login,ms.type,ms.status,m.credit,m.fuel_preauth,ms.membership_id,
		ms.membership_number,UNIX_TIMESTAMP(ms.start_date),UNIX_TIMESTAMP(ms.last_renewal),
		m.email,m.address1,m.address2,m.city,m.state,m.zip,
		m.home_phone,m.work_phone,m.mobile_phone
		FROM members m, memberships ms
		WHERE m.membership_id = ms.membership_id
			AND m.member_id = ?
SQL


use constant GET_MEMBER_BY_CC_NAME => <<"SQL";
	SELECT members.member_id, members.pin FROM members, member_ccs
	WHERE member_ccs.member_id = members.member_id
	AND member_ccs.cc_name = ?
SQL

use constant GET_MEMBER_BY_CC_NAME_LAST4 => <<"SQL";
	SELECT members.member_id, members.pin FROM members, member_ccs
	WHERE member_ccs.member_id = members.member_id
	AND member_ccs.mag_name = ?
	AND member_ccs.last4 = ?
SQL

use constant GET_CC_BY_MAG_NAME_LAST4 => <<"SQL";
	SELECT cc_id, member_id FROM member_ccs
	WHERE mag_name = ?
	AND last4 = ?
SQL

use constant GET_CC_BY_MEMB_ID_LAST4 => <<"SQL";
	SELECT cc_id FROM member_ccs
	WHERE member_id = ?
	AND last4 = ?
SQL

use constant GET_SPNS_BY_F_L_NAMES => <<"SQL";
	SELECT name,member_id,pin FROM members WHERE
SQL

use constant INSERT_MEMBER_CC => <<"SQL";
	INSERT INTO member_ccs
	SET
		cc_name = ?,
		mag_name = ?,
		last4 = ?,
		member_id = ?
SQL

use constant UPDATE_MEMBER_CC => <<"SQL";
	UPDATE member_ccs
	SET
		member_id = ?
	WHERE
		cc_id = ?
SQL

use constant GET_KIOSK_INFO_BY_IDS => <<"SQL";
	SELECT kiosk_id, name, address1, address2, city, state, zip, timezone
		FROM kiosks
		WHERE kiosk_id IN
SQL

use constant GET_CREDIT_MEMBERSHIP_ID_BY_MEMBER_ID => <<"SQL";
	SELECT credit, membership_id
	FROM members
	WHERE member_id = ?
SQL

use constant UPDATE_USER_CREDIT_BY_ID => <<"SQL";
	UPDATE members
	SET credit = ?
	WHERE member_id = ?
SQL

use constant GET_MEMBERSHIP_STATUS => <<"SQL";
	SELECT type, status, UNIX_TIMESTAMP(last_renewal), UNIX_TIMESTAMP(expires)
	FROM memberships
	WHERE membership_id = ?
SQL

use constant UPDATE_MEMBERSHIP_STATUS => <<"SQL";
	UPDATE memberships
	SET type = ?,
		status = ?,
		last_renewal = FROM_UNIXTIME(?),
		expires = FROM_UNIXTIME(?)
	WHERE membership_id = ?
SQL



use constant UPDATE_MEMBERSHIP_PENDING => <<"SQL";
	UPDATE memberships
	SET status = 'ACTIVE'
	WHERE status = 'PENDING'
		AND membership_id = ?
SQL

use constant UPDATE_ONE_DAY_MEMBERSHIP_PENDING => <<"SQL";
	UPDATE memberships
		SET status = 'ACTIVE',
			last_renewal = NOW()
	WHERE status = 'PENDING'
		AND type = 'ONE-DAY'
		AND membership_id = ?
SQL

use constant UPDATE_STATUS => <<"SQL";
	UPDATE kiosks
	SET is_online = 1,
		status = ?,
		voltage =?,
		last_ip = ?,
		last_checkin = NOW(),
		last_status = NOW()
	WHERE kiosk_id = ?
SQL

use constant CHECK_KIOSK_IS_ONLINE => <<"SQL";
	SELECT is_online
	FROM kiosks
	WHERE kiosk_id = ?
SQL

use constant UPDATE_CHECKIN => <<"SQL";
	UPDATE kiosks
	SET  is_online = 1,
		last_checkin = NOW()
	WHERE kiosk_id = ?
SQL

use constant CHECK_MESSAGES => <<"SQL";
	SELECT message_id, parameter, value
	FROM messages
	WHERE kiosk_id = ?
	ORDER BY message_id ASC
SQL

use constant ACK_MESSAGE => <<"SQL";
	DELETE FROM messages
	WHERE kiosk_id = ?
	AND   message_id = ?
SQL

use constant INSERT_ALERT => <<"SQL";
	INSERT INTO kiosk_alerts
	SET
		kiosk_id = ?,
		timestamp = FROM_UNIXTIME(?),
		alert_id = ?,
		message = ?,
		status = 'ACTIVE',
		received = NOW()
SQL

use constant GET_ALERT_MEMBER_EMAILS => <<"SQL";
	SELECT m.name, m.email
	FROM members m,
		alert_roles ar,
		member_roles mr
	WHERE m.member_id = mr.member_id
	AND mr.role_id = ar.role_id
	AND ar.alert_id = ?
SQL

use constant INSERT_NOTICES => <<"SQL";
	INSERT INTO notices
	SET
		kiosk_id = ?,
		timestamp = FROM_UNIXTIME(?),
		message = ?,
		received = NOW()
SQL

use constant INSERT_CC_TRANS => <<"SQL";
	INSERT INTO cc_transactions
	SET
		kiosk_id = ?,
		timestamp = FROM_UNIXTIME(?),
		member_id = ?,
		amount = ?,
		pre_auth = ?,
		response_code = ?,
		reason_code = ?,
		auth_code = ?,
		gwy_trans_id = ?,
		cc_id = ?,
		message = ?,
		status = ?
SQL

use constant CHK_IDENT_CC_TRANS => <<"SQL";
	SELECT UNIX_TIMESTAMP(timestamp) FROM cc_transactions
	WHERE kiosk_id = ?
		AND member_id = ?
		AND ABS(amount - ?) < 0.00001
		AND ABS(pre_auth - ?) < 0.00001
		AND response_code = ?
		AND reason_code = ?
		AND auth_code = ?
		AND gwy_trans_id = ?
		AND cc_id = ?
		AND message = ?
		AND status = ?
SQL

use constant CHK_PREAUTH_CC_TRANS => <<"SQL";
	SELECT cc_trans_id,message,cc_id FROM cc_transactions
	WHERE kiosk_id = ?
		AND gwy_trans_id = ?
	ORDER BY timestamp DESC
	LIMIT 1
SQL


use constant UPDATE_PREAUTH_CC_TRANS_VOIDED => <<"SQL";
	UPDATE cc_transactions
	SET
		status = 'VOIDED',
		timestamp = FROM_UNIXTIME(?),
		cc_id = ?,
		member_id = ?
	WHERE cc_trans_id = ?
SQL

use constant UPDATE_PREAUTH_CC_TRANS_CAPTURED => <<"SQL";
	UPDATE cc_transactions
	SET
		status = 'CAPTURED',
		timestamp = FROM_UNIXTIME(?),
		amount = ?,
		cc_id = ?,
		member_id = ?
	WHERE cc_trans_id = ?
SQL

use constant UPDATE_PREAUTH_CC_TRANS_ERROR => <<"SQL";
	UPDATE cc_transactions
	SET
		status = 'ERROR',
		timestamp = FROM_UNIXTIME(?),
		cc_id = ?,
		member_id = ?,
		message = ?
	WHERE cc_trans_id = ?
SQL

use constant UPDATE_MEMB_ID_PREAUTH_CC_TRANS => <<"SQL";
	UPDATE cc_transactions
	SET
		member_id = ?,
		cc_id = ?
	WHERE kiosk_id = ?
		AND gwy_trans_id = ?
	ORDER BY timestamp DESC
	LIMIT 1
SQL

use constant GET_SALE_ID => <<"SQL";
	SELECT LAST_INSERT_ID()
SQL
use constant VALID_CODE => 1;

use constant INSERT_SALES => <<"SQL";
	INSERT INTO sales
	SET
		kiosk_id = ?,
		timestamp = FROM_UNIXTIME(?),
		member_id = ?,
		item = ?,
		quantity = ?,
		per_item = ?,
		amount = ?,
		max_flow = ?,
		credit = ?
SQL

use constant GET_ITEM_PRICE => <<"SQL";
	SELECT price
	FROM item_prices
	WHERE
		item = ?
SQL

use constant UPDATE_SALE_CREDIT => <<"SQL";
	UPDATE sales
	SET
		credit = ?
	WHERE
		sale_id = ? 
SQL

use constant CONSOLIDATE_SALES => <<"SQL";
	UPDATE sales
	SET
		cc_trans_id = ?
	WHERE
		sale_id IN 
SQL

use constant SELECT_UNCONSOLIDATED_SALES => <<"SQL";
	SELECT sale_id, UNIX_TIMESTAMP(timestamp), amount, credit, item, quantity, per_item, kiosk_id
	FROM sales
	WHERE
		member_id = ?
		AND cc_trans_id IS NULL
		AND amount > credit
	ORDER BY timestamp DESC
SQL

# Queries to deal with duplicate requests
use constant GET_KIOSK_REQ => <<"SQL";
	SELECT UNIX_TIMESTAMP(timestamp), count
	FROM kiosk_request_log
	WHERE kiosk_id = ?
	AND   message_sha1 = ?
SQL

use constant SAVE_KIOSK_REQ => <<"SQL";
	INSERT INTO kiosk_request_log SET
		kiosk_id = ?,
		message_sha1 = ?,
		timestamp = NOW()
SQL

use constant PURGE_KIOSK_REQS => <<"SQL";
	DELETE FROM kiosk_request_log
	WHERE TIMESTAMPDIFF(MINUTE,timestamp,NOW()) > 10
SQL

use constant UPDATE_KIOSK_REQS => <<"SQL";
	UPDATE kiosk_request_log
	SET timestamp = NOW(),
		count = count + 1
	WHERE kiosk_id = ?
	AND   message_sha1 = ?
SQL


sub do_request {
	$BBD->session_delete();
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();

##################
# command-line test code
# 	$kioskID = 2;
# 	my $message = join ("\t",(
# 		'notice',
# 		'cc','1251856793','1','3.12','59.00','1','1','093064','2589167458','4133',
# 		'This transaction has been approved.','CAPTURED'
# 		))."\n";
#
# 	my $resp = do_notice ($test_message);
# 	print "Resp: [$resp]\n";
# 	return();
##################

	my $kiosk_message;
	my $message;
	if ($CGI->request_method() eq 'PUT') {
		$BBD->useB64 (0);
		$kioskID = $CGI->url_param('kioskID');
		$kiosk_message = $CGI->param('PUTDATA');
		$message = $BBD->decryptRSAverify($kioskID,$kiosk_message);
	} elsif ($CGI->request_method() eq 'POST') {
		$BBD->useB64 (1);
		$kioskID = $CGI->url_param('kioskID') || $CGI->param('kioskID');
		$kiosk_message = $CGI->param('message');
		$message = $BBD->decryptRSAverify($kioskID,$kiosk_message,$CGI->param('signature'));
	} else {
		print "Content-type: text/plain\n\nHuh?\n";
#		$BBD->printLog ("Bad request.\n");
		return 1;
	}
	if (! $kioskID || !defined($kiosk_message)) {
		print "Content-type: text/plain\n\nHuh?\n";
		$kioskID = 'UNDEFINED' unless $kioskID;
		$kiosk_message = 'UNDEFINED' unless defined ($kiosk_message);
		$BBD->printLog ("Error: kioskID: $kioskID, message: $kiosk_message (".length($kiosk_message)." bytes).\n");
		return 1;
	}
	$BBD->printLog ("Request from kiosk: $kioskID, ".length($kiosk_message)." bytes\n");

	if (! $message ) {
		print "Content-type: text/plain\n\nHuh?\n";
		$BBD->printLog ("Kiosk $kioskID: BOGUS message.\n");
		return 1;
	}

	my ($last_msg,$msg_count,$sha1);
	if ($message) {
		# Check for duplicate messages
		$sha1 = $BBD->SHA1_b64($message);
		$DBH->do (PURGE_KIOSK_REQS);
		($last_msg,$msg_count) =
			$DBH->selectrow_array (GET_KIOSK_REQ, undef, $kioskID, $sha1);

	}


	my $resp;
	my @lines = split ("\n",$message);
	$message =~ s/./x/g;
	my $timeSig = pop (@lines);
	# Ignore the request if its more than 60 seconds out of sync
	# Also, if there are no additional lines in the request (a kiosk time only)
	if ( (abs (time() - $timeSig) - $BBD->reqTime()) > 30 || scalar(@lines) == 0) {
		$resp = "resync\t".time()."\n";
		my $enc_resp = $BBD->encryptRSAsign ($resp,$kioskID);
		$BBD->printLog ("RESYNC Response length: ".length ($enc_resp)."\n");
		foreach (0..$#lines) {$lines[$_] =~ s/./x/g if $lines[$_];}
		print "Content-type: text/plain\n\n$enc_resp";
		return;
	}

	foreach (0..$#lines) {
		my $line_num = $_;
		my $resp_nlines=0;
		my ($type,$message) = ($1,$2) if $lines[$line_num] =~ /^(\S+)\s+(.+)$/;
		$lines[$line_num] =~ s/./x/g if $lines[$line_num];
		next unless $type and $message;

		do_checkin () if $line_num == 0;
		my $line_resp;
		if ($type eq 'auth') {
			$BBD->printLog ("auth message\n");
			$line_resp = do_auth ($message);
		} elsif ($type eq 'status') {
			$BBD->printLog ("status message: $message\n");
			$line_resp = do_status ($message);
		} elsif ($type eq 'notice') {
			$BBD->printLog ("notice message: $message\n");
			$line_resp = do_notice ($message);
		} elsif ($type eq 'new_memb') {
			$BBD->printLog ("new_memb message\n");
			$line_resp = do_new_memb ($message);
		} elsif ($type eq 'GW') {
			$BBD->printLog ("GW message\n");
			$line_resp = do_GW ($message);
		} else {
			$BBD->printLog ("UNRECOGNIZED message: $type\n");
		}
		
		if (defined $line_resp) {
			$resp_nlines++;
			$resp .= "";
			$resp .= "$line_resp\n" if $line_resp;
		}

		$message =~ s/./x/g;
	}

	if ($last_msg) {
		$DBH->do (UPDATE_KIOSK_REQS, undef, $kioskID, $sha1);
		$BBD->printLog ("Duplicate request\n");
		if ($msg_count == 1) {
			$DBH->do (INSERT_ALERT,undef, $kioskID, time(), 'Software', "Server detected duplicate request from kiosk");
			email_alert ($kioskID, time(), 'Software',
				sprintf ("Server detected duplicate request from kiosk.\nRequest:  [%.16s...] %d bytes %d lines\nResponse: [%.16s...] %d bytes %d lines\n",
				$message,length ($message),scalar(@lines),$resp,length ($resp),$#{ [split ("\n",$resp)]}+1)
			);
		} elsif ($msg_count == 10) {
			$DBH->do (INSERT_ALERT,undef, $kioskID, time(), 'Software',
				"Server detected $msg_count duplicate requests from kiosk. No further notifications");
			email_alert ($kioskID, time(), 'Software',
				"Server detected $msg_count duplicate requests from kiosk\nNo further email notifications will be sent");
		}
	} elsif ($sha1) {
		$DBH->do (SAVE_KIOSK_REQ,undef,$kioskID,$sha1);
	}

	
	# We retransmit the messages on a duplicate request
	if (defined $resp) {
		my $messages = "";
		my ($msg_id,$key,$val);
		my $sth = $DBH->prepare(CHECK_MESSAGES) or die "Could not prepare handle";
		$sth->execute( $kioskID );
		$sth->bind_columns (\$msg_id,\$key,\$val);
		while($sth->fetch()) {		
			$messages .= "msg\t$msg_id\t$key\t$val\n";
		}
		

		# If we're printing the response inot the log, do not print the gateway string
#		my @printRespPieces = split (/\t/,$resp);
#		$printRespPieces[$#printRespPieces] = '-----' if ($printRespPieces[0] eq 'auth');
#		$BBD->printLog ("Response: $messages".join ("\t",@printRespPieces)."time\t".time()."\n");

		$resp = $messages.$resp."time\t".time()."\n";

		my $enc_resp = $BBD->encryptRSAsign ($resp,$kioskID);
		$resp =~ s/./x/g;

		my $resp_lngth = length ($enc_resp);
		$BBD->printLog ("Response length: $resp_lngth\n");
		print "Content-type: application/octet-stream\nContent-Length: $resp_lngth\n\n$enc_resp";
	} else {
		$BBD->printLog ("Response: [NULL]\n");
		$resp = "Huh?\n";
		my $resp_lngth = length ($resp);
		print $CGI->header (-connection => 'Keep-Alive',-content_length => $resp_lngth, -content_type => 'text/plain'),$resp;
	}
	
	

}

# Fuel prices
# As advertised, only for FULL, non-EXPIRED members.
#  Everyone else pays a surcharge.
# To avoid paying the surcharge, must upgrade and/or renew membership.
#
# Upgrades:
# one-day memberships never expire!
# non-fueling memberships pay $30 dues and expire
#
# Non-fueling expired -> full $100
#	$renewal_fee, $trial_fee, $upgrade_fee (=$70)
#   renew->upgrade->preauth
#   in upgrade: if (renewal_fee > 0 && no_renewal) upgrade_fee += renewal
# Non-fueling expired -> Non-fueling active $30
#	$renewal_fee, $trial_fee, $upgrade_fee (=$70)
#   in upgrade: if (renewal_fee > 0 && no_renewal) upgrade_fee += renewal
#   renew->upgrade->preauth
# Non-fueling active -> full $70
#	0.0, $trial_fee, $upgrade_fee (=$70)
#   upgrade->preauth
# Full expired -> full $30
#   $renewal_fee
#   renew->preauth
# one-day -> full $100
#	0.0, $trial_fee, $upgrade_fee (=$100)
#   upgrade->preauth
# 
# When buying a NEW membership (full or trial), the membership gets registered with the server
# before fueling.  The membership sale is reflected in the credit (in do_new_memb).
# An upgrade sends the sale to the server (being reflected in the credit), but the kiosk's credit isn't updated
# when a sale is made, so the kiosk has to keep track of the fees outside of the credit (just like renewal).
sub do_auth {
my $message = shift;

	my $resp = "auth\t";
	

	my $GW_string=$BBD->getGWconf();

# Get the params from the message and do a sanity check
	my ($mag_name,$last4,$pin_in) = split (/\t/,$message);
	return $resp."Failed: Unauthorized" unless $mag_name and $last4 and $pin_in;

# Existing member, but failed logon
	my ($auth_memb_id,$DB_memb_id);
	($auth_memb_id,$DB_memb_id) = auth_by_CC_mag_name_last4 ($mag_name,$last4,$pin_in);
	if ($DB_memb_id and not $auth_memb_id) {
		save_CC_name ($mag_name,$last4,undef);
		return $resp."Failed: SPN" ;
	}
	($auth_memb_id,$DB_memb_id) = auth_by_CC_name ($mag_name,$pin_in) unless $auth_memb_id;
	if ($DB_memb_id and not $auth_memb_id) {
		save_CC_name ($mag_name,$last4,undef);
		return $resp."Failed: SPN" ;
	}
	($auth_memb_id,$DB_memb_id) = auth_by_FL_name ($mag_name,$pin_in) unless $auth_memb_id;
	save_CC_name ($mag_name,$last4,$auth_memb_id);
	return $resp."Failed: SPN" if ($DB_memb_id and not $auth_memb_id);

# Member not found
 	if (not $DB_memb_id) {
 		my $membership_fee = $BBD->getMembershipPrice ($kioskID);
 		my $trial_fee = $BBD->getTrialSurcharge ();

 		$resp .= sprintf ("Failed: Membership\t%.2f\t%.2f\t%s",$membership_fee,$trial_fee,$GW_string);
 		return $resp;
 	}
# N.B.: Uncomment the "if" block above to turn off selling memberships @ the kiosk
# 	if (not $DB_memb_id) {
# 		$resp .= sprintf ("Failed: Unauthorized");
# 		return $resp;
# 	}

# Member found.  Send membership modification options if applicable.
	my ($memb_name,$memb_fname,$memb_lname,$id,$login,$type,$status,$credit,$fuel_preauth,$ms_id,$ms_num,$start,$renew,$email,$ad1,$ad2,$city,$state,$zip,$h_ph,$w_ph,$m_ph) =
		$DBH->selectrow_array (GET_USER_INFO_BY_ID,undef,$auth_memb_id);
	my $ppg = $BBD->getFuelPrice('FULL', $kioskID);
	$fuel_preauth = $BBD->getDefaultFuelPreauth() unless $fuel_preauth;
	$resp .= sprintf ("%d\t%s\t%s\t%.2f\t%.2f\t%.2f\t%s",$id,$type,$status,$ppg,$fuel_preauth,$credit,$ms_num);

# Membership modification options
	my $renewal_fee = 0.0;
	my $trial_fee = 0.0;
	my $upgrade_fee = 0.0;
# This is set for full or non-fueling memberships by a cron job.
# ASK_RENEWAL is set during a grace period.
	if ($status eq 'ASK_RENEWAL') {
		$renewal_fee = $BBD->getRenewalPrice ();
# EXPIRED is set when the grace period is over.
	} elsif ($status eq 'EXPIRED') {
		$renewal_fee = $BBD->getRenewalPrice ();
		$trial_fee = $BBD->getTrialSurcharge ();
	}
# Upgrades
	if ($type ne 'FULL') {
		$trial_fee = $BBD->getTrialSurcharge ();
		if ($type eq 'ONE-DAY') {
			$upgrade_fee = $BBD->getMembershipPrice ($kioskID);
		} else {
			$upgrade_fee = $BBD->getUpgradePrice ();
		}
	}


	$resp .= sprintf ("\t%.2f\t%.2f\t%.2f\t%s",$renewal_fee,$trial_fee,$upgrade_fee,$GW_string);
	return $resp;	
}

sub do_new_memb {
my $message = shift;

# Message format:
# new_memb type CC_mag_name last4 SPN pre_auth gw_trans_id gw_auth
	my $resp = "auth\t";

	my ($type,$mag_name,$last4,$pin_in,$fuel_preauth,$gw_trans_id,$gw_auth,$price) = split (/\t/,$message);
	return $resp."Failed: BadCC" unless $type and $mag_name and $last4 and $pin_in and $gw_trans_id;

	# Check for valid auth and trans ID
	# Both have to be valid and can't consist entirely of "0"s
	# FIXME: Unfortunately, need this for test trasnactions, so commented out for now
#	if (not ($gw_auth and $gw_trans_id and not $gw_auth =~ /^[0]+$/ and not $gw_trans_id =~ /^[0]+$/ ) ) {
#		return $resp."Failed: BadCCAuth";
#	}

	# Check for duplicate CC
	my ($auth_memb_id,$DB_memb_id);
	($auth_memb_id,$DB_memb_id) = auth_by_CC_mag_name_last4 ($mag_name,$last4,$pin_in);
	# If DB_memb_id is defined, its a duplicate
	return $resp."Failed: DuplicateCC" if defined $DB_memb_id;

	# This is for backwards-compatibility.  Not needed if last4 is collected for all CCs
	($auth_memb_id,$DB_memb_id) = auth_by_CC_name ($mag_name,$pin_in) unless $auth_memb_id;
	# If DB_memb_id is defined, its a duplicate
	return $resp."Failed: DuplicateCC" if defined $DB_memb_id;

	# Register the new membership, member and CC
	# The preauth ammount comes from the kiosk in dollars (chosen by the user).
	# We store the gallon equivalent (rounded up to the nearest gallon).
	my $fuel_price = $BBD->getFuelPrice ($type, $kioskID);
	if (defined $fuel_price and $fuel_price > 0.0) {
		$fuel_preauth /= $fuel_price;
		$fuel_preauth = ( $fuel_preauth == int($fuel_preauth) ? $fuel_preauth : int($fuel_preauth + 1) );
	} else {
		$fuel_preauth = undef; # will be set to default value by new_membership()
	}

	# Convert CCname to proper name (lower-case, first letter capitalized)
	my $properName = lc(mag_name_to_CC_name ($mag_name));
	$properName=~s/\b(\w)/\u$1/g;
	# This call will update promos if any
	my $new_member_id = 
		$BBD->new_membership (time(),$type,'ACTIVE',$properName,"",$pin_in,$fuel_preauth,$price,$kioskID);
	
	
	# The membership has a pre-auth CC transaction in the DB with a bogus member_ID
	# The member-CCs member-ID is also bogus.
	save_CC_name ($mag_name,$last4,$new_member_id);
	# Get the CC_id saved by save_CC_name.
	my ($CC_id) = $DBH->selectrow_array (GET_CC_BY_MEMB_ID_LAST4,undef,$new_member_id,$last4);
	$DBH->do (UPDATE_MEMB_ID_PREAUTH_CC_TRANS, undef, $new_member_id, $CC_id, $kioskID, $gw_trans_id);

	# Register the sale if full membership
	if ($type eq 'FULL') {
		do_notice (join ("\t",'sale',time(),$new_member_id,'membership',1,$price,$price));
	}
	
	# Prepare the response
	my ($memb_name,$memb_fname,$memb_lname,$id,$login,$DB_type,$status,$credit,$DB_fuel_preauth,$ms_id,$ms_num,$start,$renew,$email,$ad1,$ad2,$city,$state,$zip,$h_ph,$w_ph,$m_ph) =
		$DBH->selectrow_array (GET_USER_INFO_BY_ID,undef,$new_member_id);
	my $ppg = $BBD->getFuelPrice($DB_type, $kioskID);

	$DB_fuel_preauth = 0.0 if not defined $DB_fuel_preauth;
	$resp .= sprintf ("%d\t%s\t%s\t%.2f\t%.2f\t%.2f\t%s",$id,$DB_type,$status,$ppg,$DB_fuel_preauth,$credit,$ms_num);

	return $resp;	
}

sub do_status {
my $message = shift;
	# 2: fuel_avail, 3: fuel_type 4: price
	my ($status,$fuel_avail,$fuel_type,$last_ppg,$voltage,$next_checkin) = split (/\t/,$message);
	$DBH->do (INSERT_NOTICES,undef,
		$kioskID, time(), "Voltage: $voltage");

	# Status reports the kiosk's current idea of fuel type, fuel available and its price.
	# These quantities can be changed at the DB or at the kiosk.
	# Changes from the DB are done using messages to the kiosk.
	#   These changes are reflected in the kiosks table immediately.
	#   The kiosk is informed with a message, but the kiosk has to checkin to read the message.
	# Changes from the kiosk are done using messages to the DB.  These should arrive before a checkin message.
	# Therefore, the DB values take precedence over the kiosk values if the kiosk's values don't match.
	# In case of mismatch the messages table is cleared of fuel messages, and a new fuel message is sent to the kiosk to correct it.
	# In this case, we have the kiosk on the line, so it will get the message in the response.
	# Note that we do not add messages to the response here - they are added from the DB after the message response.
	$BBD->updateKioskFuel ($kioskID, updateKiosk => 'fromDB', avail => $fuel_avail, type => $fuel_type, price => $last_ppg);

	my $sth = $DBH->prepare(UPDATE_STATUS) or die "Could not prepare handle";
	$sth->execute($status, $voltage, $CGI->remote_host(), $kioskID );

	return "";
}

# This gets called every time there's any kind of message form the kiosk
# So we don't check the message body, or update any kiosk parameters other than last_checkin
# If the kiosk's on_line status was previously false or NULL/undef, send an email about status change.
sub do_checkin {

	my $is_online = $DBH->selectrow_array (CHECK_KIOSK_IS_ONLINE,undef,$kioskID);
	# Its online now, so if is_online is false or undef in the DB, send an email
	if (!$is_online) {
		email_alert ($kioskID, time(), 'Network', '');
		$BBD->setWebsiteKioskStatus ($kioskID,1);
	}

	# This sets is_online to 1.
	my $sth = $DBH->prepare(UPDATE_CHECKIN) or die "Could not prepare handle";
	$sth->execute($kioskID );
}


#
# Gateway messages
# "PreAuth", "Capture","AuthCapture", "Void"
# First iteration, the kiosk FSM sends these to the server FIFO instead of the GW FIFO
# The server simply transmits them here, so we re-implement the GW FIFO reader in perl
# With kiosk-GW comms, the local GW responds with messages sent over the event FIFO.
# With a remote-GW, the responses will be the same as the local GW, and end up on the FSM
# event queue through the kiosk's server process instead of through the local GW.
sub do_GW {
my $message = shift;
my $ua = LWP::UserAgent->new;
# Static params
my %GW_params = (
	x_response_format  => '1',
	x_delim_char       => '|',
	x_duplicate_window => '0', # ask gateway to ignore duplicate transactions
);
my $GW_URL;
my $GW_resp = "GW";

	my $GW_string=$BBD->getGWconf(1);
	if ($GW_string eq 'Server GW') {
		$BBD->printLog ("Could not read gateway configuration file!\n");
	}

	my ($type,$params) = ($1,$2) if $message =~ /^(\S+)\s+(.+)$/;
	$message =~ s/./x/g;

	if (! ($type and $params and $GW_string) ) {
		$params =~ s/./x/g;
		return "GW Error";
	}

	# CC_URL x_cpversion x_login x_market_type x_device_type x_tran_key
	# Fillup the account-dependent parts of the GW params hash
	($GW_URL,
		$GW_params{x_cpversion},
		$GW_params{x_login},
		$GW_params{x_market_type},
		$GW_params{x_device_type},
		$GW_params{x_tran_key}
	) = split (/\s/,$GW_string);
	$GW_string =~ s/./x/g;

	# PreAuth: amount,track1,track2
	# AuthCapture: amount,track1,track2
	my ($amount,$track1,$track2,$trans_id,$member_id);
	my ($last4,$magname);

	if ($type eq 'PreAuth' or $type eq 'AuthCapture') {
		($member_id,$amount,$track1,$track2) = split (/\t/,$params);
		$BBD->printLog ("Kiosk trans: $type memb:$member_id amount:$amount\n");
		if ($type eq 'PreAuth') {
			$GW_params{x_type} = 'AUTH_ONLY';
		} else {
			$GW_params{x_type} = 'AUTH_CAPTURE';
		}
		$GW_params{x_amount} = $amount;

		if ($track1) {
			$GW_params{x_track1} = $track1;
			($last4,$magname) = ($1,$2) if $track1 =~ /^.+(\d\d\d\d)\^(.+)\^.+$/;
		} elsif ($track2) {
			$GW_params{x_track2} = $track2;
		} else {
			delete $GW_params{x_type}; # flag as error
		}
	# Capture: amount,trans_id
	} elsif ($type eq 'Capture') {
		($member_id,$amount,$trans_id) = split (/\t/,$params);
		$BBD->printLog ("Kiosk trans: $type memb:$member_id amount:$amount transID:$trans_id\n");
		$GW_params{x_type} = 'PRIOR_AUTH_CAPTURE';
		$GW_params{x_ref_trans_id} = $trans_id;
		$GW_params{x_amount} = $amount;

	# Void: trans_id
	} elsif ($type eq 'Void') {
		($member_id,$trans_id) = split (/\t/,$params);
		$BBD->printLog ("Kiosk trans: $type memb:$member_id transID:$trans_id\n");
		$GW_params{x_type} = 'VOID';
		$GW_params{x_ref_trans_id} = $trans_id;

	} else {
		$BBD->printLog ("Kiosk trans: [$type] MALFORMED!\n");
	}
	# We're done with these now
	$params =~ s/./x/g;
	
	# All these things have to be set to continue on to GW comms
	if (not ($GW_URL and $GW_params{x_cpversion} and $GW_params{x_login}
		and $GW_params{x_market_type} and $GW_params{x_device_type} and $GW_params{x_tran_key}
		and $GW_params{x_type}
	) ) {
		$track1 =~ s/./x/g;
		$track2 =~ s/./x/g;
		foreach ( keys (%GW_params)) {$GW_params{$_} =~ s/./x/g if $GW_params{$_};}
		$BBD->printLog ("Kiosk trans: [$type] ERROR: Gateway string cannot be parsed!\n");
		return "GW Error";
	};



	# Talk to the gateway
	$ua->timeout(15);
	$ua->agent( "BBD9000 Server $BBD::VERSION" );
	my $res;

	# 3 times to connect to the GW
	# server seponses to recognixe:
	# 503: GW busy - retry
	# 200: Valid GW response
	my $GW_srv_code;
	my ($code,$reas_code,$gw_message,$auth_code,$gw_trans_id,$MD5_hash);

	my ($retry, $retries) = (0,4);
#####
# Pretend transactions
	if (BBD::TEST_DB) {
		(undef,$code,$reas_code,$gw_message,$auth_code,undef,undef,$gw_trans_id,$MD5_hash) = (
			'foo',
			1,1,
			'Server GW Test: OK',
			'12345',
			'foo','foo',
			'67890',
			'123abc456def123abc'
		);
		$gw_trans_id = join ('', (0..9, 'A'..'Z')[rand 36, rand 36, rand 36, rand 36, rand 36, rand 36, rand 36, rand 36]);

		$retry = $retries;
		$GW_srv_code = 200;
	}
######
	while ($retry < $retries) {
		$retry++;
		if( timeout_call( $ua->timeout(), sub {$res = $ua->post($GW_URL, \%GW_params);}) ) {
			$GW_srv_code = 999;
			$BBD->printLog ("Timeout communicating with gateway.  Retry: $retry\n");
			sleep ( $retry * $retry );
		} else {
			$GW_srv_code = $res->code();
			if ($GW_srv_code == 503) { # GW is busy
				$BBD->printLog ("Gateway reports busy (503).  Retry: $retry\n");
				sleep ( $retry * $retry );
			} elsif ($GW_srv_code == 200) {
				(undef,$code,$reas_code,$gw_message,$auth_code,undef,undef,$gw_trans_id,$MD5_hash) =
					split (/\|/,$res->content);
				next unless $code; # loop again if $code isn't set
				# Track1 invalid, but have track2
				if ($code == 3 && $reas_code == 88 && $track2) {
					delete $GW_params{x_track1};
					$BBD->printLog ("Gateway refused track1, trying track2.  Retry: $retry\n");
					$GW_params{x_track2} = $track2;
				# Have a valid response code, can't fix anything else here.
				} else {
					last;
				}
			} # success code from GW server
		} # Not a GW timeout
	} # Retry for various GW server errors

# This is not exactly industrial strength, but slightly better than nothing.
$track1 =~ s/./x/g if $track1;
$track2 =~ s/./x/g if $track2;
foreach ( keys (%GW_params)) {$GW_params{$_} =~ s/./x/g if $GW_params{$_};}


# Parse the GW response
my %GW_reas_codes = (
	19 => 'Busy', 20 => 'Busy', 21 => 'Busy', 22 => 'Busy', 23 => 'Busy', 25 => 'Busy', 26 => 'Busy',
	57 => 'Busy', 58 => 'Busy', 59 => 'Busy', 60 => 'Busy', 61 => 'Busy', 62 => 'Busy', 63 => 'Busy',
	120 => 'Busy', 121 => 'Busy', 122 => 'Busy', 181 => 'Busy',
	36 => 'Declined', 49 => 'Declined',
	17 => 'CCTypeRejected', 28 => 'CCTypeRejected',
	11 => 'TransLost', 16 => 'TransLost', 33 => 'TransLost',
	 8 => 'CardExpired',
);

	my ($r_code,$r_reas_code,$r_gw_trans_id,$r_auth_code) = ($code,$reas_code,$gw_trans_id,$auth_code);
	$r_code = 0 unless defined ($r_code);
	$r_reas_code = 0 unless defined ($r_reas_code);
	$r_gw_trans_id = '0' unless defined ($r_gw_trans_id) and $r_gw_trans_id;
	$r_auth_code = '0' unless defined ($r_auth_code) and $r_auth_code;
	if ($code == 1 && $reas_code == 1) {
		$GW_resp .= " Accepted\t";
		$GW_resp .= join ("\t",$r_code,$r_reas_code,$r_gw_trans_id,$r_auth_code,'accepted','xxxx');
		# Tell ourselves what happened
		# Process a CC: 2:member_id, 3:amount, 4:pre_auth, 4:response_code, 5:reas_code,
		#               6:auth_code, 7:gwy_trans_id, 8:last4, 9:message, 10:status
		# Message pieces: [cc],[1284304014],[],[],[175.00],[1],[1],[000000],[],[4133],[(TESTMODE) This transaction has been approved.],[PREAUTH]
		# New members do not have a valid member ID, which will be fixed when the kiosk send a new_memb message
		# Otherwise, we get the memberID from the request.
		my ($pre_auth,$status);
		if ($member_id == 0) {$member_id = undef;}
		if ($type eq 'PreAuth') {
			$pre_auth = $amount ; $amount = undef; $status = 'PREAUTH';
			$trans_id = $gw_trans_id;
		} elsif ($type eq 'AuthCapture') {
			$status = 'CAPTURED';
			$trans_id = $gw_trans_id;
		} elsif ($type eq 'Capture') {
			$status = 'CAPTURED';
			$gw_message = undef;
		} elsif ($type eq 'Void') {
			$status = 'VOIDED';
			$gw_message = undef;
		}
		# gw_trans_id was generated by the GW, while trans_id came from the kiosk
		# The gw_trans_id is send for cc processing on PreAuth and AuthCapture requests
		# The kiosk's trans_id is send otherwise

		do_notice (join ("\t","cc",time(),$member_id,$amount,($pre_auth ? $pre_auth : ''),
			$code,$code,$auth_code,$trans_id,($last4 ? $last4 : ''),($gw_message ? $gw_message : ''),$status) );
	} elsif ($code == 2) {
		$GW_resp .= " Declined\t";
		$GW_resp .= join ("\t",$r_code,$r_reas_code,$r_gw_trans_id,$r_auth_code,'declined','xxxx');
	} elsif ($code == 3) {
		if ( exists $GW_reas_codes{$r_reas_code} ) {
			$GW_resp .= " ".$GW_reas_codes{$r_reas_code}."\t";
		} else {
			$GW_resp .= " Error\t";
		}
		$GW_resp .= join ("\t",$r_code,$r_reas_code,$r_gw_trans_id,$r_auth_code,'error','xxxx');
	} elsif ($GW_srv_code == 503) { # Gateway is busy
		$GW_resp .= " Busy\t";
		$GW_resp .= join ("\t",
			0,
			0,
			'0',
			'0',
			'busy','xxxx'
		);
	} elsif ($GW_srv_code == 999) { # our own timeout
		$GW_resp .= " Timeout\t";
		$GW_resp .= join ("\t",
			0,
			0,
			'0',
			'0',
			'timeout','xxxx'
		);
	} else {
		$GW_resp .= " Error\t";
		$GW_resp .= join ("\t",
			0,
			0,
			'0',
			'0',
			'error','xxxx'
		);
	}


	my @pieces = split (/\t/,$GW_resp);
	$BBD->printLog ('GW Response pieces: ['.join ('],[',@pieces)."]\n");
	return $GW_resp;
}



# notices are tab-delimited.
# first field is the notice type
# second field is the time_t unix timestamp at the kiosk
# third field (and beyond) are the contents
# Notice types:
#  cc - seven additional tab-delimited fields described above in the sql query
#  sale - four additional fields: member_id, item, quantity, amount

#  notice - one additional field
sub do_notice {
my ($message) = @_;

	my @pieces = split (/\t/,$message);
	$BBD->printLog ('Message pieces: ['.join ('],[',@pieces)."]\n");
	# The first piece tells us what kind of notice
#####
# Process a CC: 2:member_id, 3:amount, 4:pre_auth, 4:response_code, 5:reas_code,
#               6:auth_code, 7:gwy_trans_id, 8:last4, 9:message, 10:status
#####
	if ($pieces[0] eq 'cc') {
		my ($type, $timestamp, $member_id, $amount, $pre_auth, $response_code, $reas_code,
			$auth_code, $gwy_trans_id, $last4, $k_message, $status) = @pieces;

		# Get the credit card ID
		my ($CCid) = $DBH->selectrow_array (GET_CC_BY_MEMB_ID_LAST4,undef,$member_id,$last4);
		# check for identical transaction
		my ($last_trans) = $DBH->selectrow_array (CHK_IDENT_CC_TRANS,undef,
			$kioskID, $member_id,
			$amount, $pre_auth,
			$response_code, $reas_code,
			$auth_code, $gwy_trans_id, $CCid, $k_message, $status
		);
		if ($last_trans and abs($timestamp-$last_trans) < 600) { 
			return "";
		}
		
		# Get a previous preauth transaction
		my ($last_trans_id,$last_message,$DB_CC_id) = ('','','');
		($last_trans_id,$last_message,$DB_CC_id) = $DBH->selectrow_array (CHK_PREAUTH_CC_TRANS,undef,
			$kioskID, $gwy_trans_id
		);
		$CCid = $DB_CC_id if $DB_CC_id;

$BBD->printLog ("GW last_trans_id: [$last_trans_id], last_message: [$last_message], status: [$status]\n");
		if ($last_trans_id && $status eq "VOIDED") {
			$DBH->do (UPDATE_PREAUTH_CC_TRANS_VOIDED, undef, $timestamp,
				$CCid, $member_id, $last_trans_id
			);
		} elsif ($last_trans_id && $status eq "CAPTURED") {
			$DBH->do (UPDATE_PREAUTH_CC_TRANS_CAPTURED, undef, $timestamp,
				$amount, $CCid, $member_id, $last_trans_id
			);
		} elsif ($last_trans_id && $status eq "ERROR") {
			$last_message .= " Upd: amount:$amount, GW code:$response_code, GW Reas:$reas_code, msg: $k_message.";
			$DBH->do (UPDATE_PREAUTH_CC_TRANS_ERROR, undef, $timestamp,
				$CCid, $member_id, $last_message, $last_trans_id
			);
		} else {
			$DBH->do (INSERT_CC_TRANS, undef, $kioskID, $timestamp,
				$member_id, $amount, $pre_auth, $response_code, $reas_code,
				$auth_code, $gwy_trans_id, $CCid, $k_message, $status
			);
			($last_trans_id,$last_message,$DB_CC_id) = $DBH->selectrow_array (CHK_PREAUTH_CC_TRANS,undef,
				$kioskID, $gwy_trans_id
			);
			$CCid = $DB_CC_id if $DB_CC_id;
		}
		# Since CC notices come after sales, lets try to consolidate
		# but only if the status is 'CAPTURED'
		if ($status eq 'CAPTURED') {
			consolidateCC($last_trans_id, $timestamp, $member_id, $amount, $gwy_trans_id, $kioskID);
		}
#####
# Process a sale: member_id, item, quantity, per_unit, amount, max_flow
#####
	} elsif ($pieces[0] eq 'sale') {
		# Get the user credit
		my ($credit,$membership_id) =
			$DBH->selectrow_array (GET_CREDIT_MEMBERSHIP_ID_BY_MEMBER_ID,undef,$pieces[2]);
		$credit = 0.0 unless defined $credit;
		$credit = 0.0 if abs ($credit) < SALE_EPSILON;
		$credit = sprintf ("%.2f",$credit);
		$pieces[6] = sprintf ("%.2f",$pieces[6]);
		my $credit_used = 0.0;
		# If the user has enough credit, we auto-consolidate the sale with the credit,
		# by entering the amount of credit used to consolidate in the credit column.
		# If there is credit, but less than the sale amount, we enter the amount of credit used.
		if ($credit > 0.0) {
			if ($pieces[6] > $credit) {
				$credit_used = $credit;
			} elsif ($pieces[6] <= $credit) {
				$credit_used = $pieces[6];
			}
		} else {
		# If the credit is negative, we can't use it to cover anything
			$credit_used = 0.0;
		}

		# The user credit is updated by the amount used for this sale
		$credit = sprintf ("%.2f",$credit - $pieces[6]);
		

		$DBH->do (INSERT_SALES, undef, $kioskID, $pieces[1],
			$pieces[2], $pieces[3], $pieces[4], $pieces[5], $pieces[6], $pieces[7], $credit_used
		);
		my ($sale_id) = $DBH->selectrow_array (GET_SALE_ID);


		# Update the user credit
		$DBH->do (UPDATE_USER_CREDIT_BY_ID, undef, $credit, $pieces[2]);
		# Update PENDING membership to ACTIVE
		$DBH->do (UPDATE_MEMBERSHIP_PENDING, undef, $membership_id );
		# One-day memberships are PENDING when first made,
		# become ACTIVE on the first sale, with the date set in last_renewal
		$DBH->do (UPDATE_ONE_DAY_MEMBERSHIP_PENDING, undef, $membership_id );
		

		# update membership expiration/status/renewal based on sale type
		set_expiration ($membership_id,uc ($pieces[3]), $pieces[6]);

		# If the sale was completely covered, issue a receipt.
		if ($credit_used + SALE_EPSILON >= $pieces[6]) {
			my @covered_sales = ($sale_id);
			my %sale_info;
			$sale_info{$sale_id} = {
				timestamp => $pieces[1],
				amount    => $pieces[6],
				credit    => 0.0,
				item      => $pieces[3],
				per_item  => $pieces[5],
				quantity  => $pieces[4],
				kiosk_id  => $kioskID
			};
			emailCCreceipt (\@covered_sales,\%sale_info, undef, $pieces[2], undef, $pieces[6], $kioskID, $pieces[1], undef);
		}
		
		# If this was a fuel sale, update the kiosk's fuel quantity in the DB
		# Fuel sale items are "B" followed by digits
		# Some potentially useful queries:
		# SELECT quantity, item, quantity*SUBSTRING(item,2)/100 as biodiesel FROM sales WHERE item LIKE "B%";
		# SELECT SUM(quantity*SUBSTRING(item,2)/100) FROM sales WHERE item LIKE "B%";
		# Update DB from previous 'fuel' entries:
		# UPDATE sales SET item = 'B99.9' WHERE item = 'fuel' and MONTH(timestamp) >= 4 and MONTH(timestamp) <= 9;
		# UPDATE sales SET item = 'B50' WHERE item = 'fuel' and MONTH(timestamp) < 4 or MONTH(timestamp) > 9;
		# 78.5% reduction in carbon dioxide emissions (biodiesel.org).  22.2 pounds/gallon diesel (epa).  2,205 lb/meteric ton (US ton, wikipedia)
		# biodiesel * (22.2 * .785) = biodiesel * 17.427 = lb CO2 displaced
		# (biodiesel * (22.2 * .785))/2205 = biodiesel * 0.0079034 = tons CO2 displaced
		


		if ($pieces[3] =~ /^B[0-9.]+$/) {
			$BBD->updateKioskFuel ($kioskID, updateDB => 'fromParam', sale => $pieces[4]);
		}
		
		
#####
# Process an ack notice
#####
	} elsif ($pieces[0] eq 'ack') {
	# Its just a plain old notice
		$DBH->do (ACK_MESSAGE,undef,$kioskID,  $pieces[1]);

#####
# Process an alert notice
#   type message
#####
	} elsif ($pieces[0] eq 'alert') {
		$DBH->do (INSERT_ALERT,undef, $kioskID, $pieces[1], $pieces[2], $pieces[3]);
		email_alert ($kioskID, $pieces[1], $pieces[2], $pieces[3]);

#####
# Process a fuel notice (gals available, type and price)
#####
	} elsif ($pieces[0] eq 'fuel') {
	# 2: fuel_avail, 3: fuel_type 4: price
		$BBD->updateKioskFuel ($kioskID, updateDB => 'fromParam', avail => $pieces[2], type => $pieces[3], price => $pieces[4]);

#####
# Process a plain old notice
#####
	} elsif ($pieces[0] eq 'notice') {
	# Its just a plain old notice
		$DBH->do (INSERT_NOTICES,undef,
			$kioskID, $pieces[1], $pieces[2]);
	} else {
		die "unrecognized notice type";
	}
	
	return "";
}

sub consolidateCC {
my ($cc_id, $cc_timestamp, $memb_id, $cc_amount, $cc_trans_id, $cc_kiosk_id) = @_;
my ($sale_id, $sale_timestamp, $sale_amount, $sale_credit, $item, $quantity, $per_item, $sale_kiosk_id );
my @covered_sales;
my @partial_sales;
my %sale_info;
my $partial_sale;

	# Get the user credit - don't care about the membership id
	my ($memb_credit,undef) = $DBH->selectrow_array (GET_CREDIT_MEMBERSHIP_ID_BY_MEMBER_ID,undef,$memb_id);
	$memb_credit = 0.0 unless defined $memb_credit;
	$memb_credit = 0.0 if abs ($memb_credit) < SALE_EPSILON;
	my $sales_credit = 0.0;

	# Only use positive credit for consolidation
	my $total_amount = $cc_amount + ($memb_credit < 0.0 ? 0.0 : $memb_credit);
	$BBD->printLog ("Consolidating CC transaction. Member credit: $memb_credit, CC amount: $cc_amount\n");

	# get the unconsolidated sales for this member
	# in descending order by date (latest first)
	# FIXME: At this kiosk?  Does it matter? For now, no
	# Where the sale amount is greater than the credit amount.
	my $sth = $DBH->prepare(SELECT_UNCONSOLIDATED_SALES) or die "Could not prepare handle";
	$sth->execute( $memb_id );
	$sth->bind_columns (\$sale_id,\$sale_timestamp, \$sale_amount, \$sale_credit, \$item, \$quantity, \$per_item, \$sale_kiosk_id );

	# Cover sales until we run out of money
	while($sth->fetch()) {		
		$sale_credit = 0.0 if abs ($sale_credit) < SALE_EPSILON;
		$sale_info{$sale_id} = {
			timestamp => $sale_timestamp,
			amount    => $sale_amount,
			credit    => $sale_credit,
			item      => $item,
			per_item  => $per_item,
			quantity  => $quantity,
			kiosk_id  => $sale_kiosk_id
		};
		# Three conditions:  Sale is completely covered, sale is partially covered, sale is entirely uncovered.
		# Completely covered
		if ( ($sale_amount - $sale_credit) < ($total_amount + SALE_EPSILON)) {
			$BBD->printLog ("Marking sale $sale_id ($sale_amount, credit: $sale_credit) for consolidation. ");
			push (@covered_sales,$sale_id);
			$sales_credit += ($sale_amount - $sale_credit);
		# Partially covered (still some money left)
		# We add it to the current sale's credit since the member credit reflects any outstanding sale ammounts
		} elsif ($total_amount > 0.0) {
			$BBD->printLog ("Partially covered sale $sale_id ($sale_amount, credit: $sale_credit) for consolidation. ");
			$sale_credit = sprintf ("%.2f",$sale_credit + $total_amount);
#			$sale_info{$sale_id}->{credit} = $sale_credit;
			$DBH->do (UPDATE_SALE_CREDIT, undef, $sale_credit, $sale_id);
			$partial_sale = $sale_id;
			$sales_credit += $total_amount;
		# Not covered (no money left)
		} else {
			$BBD->printLog ("Uncovered sale $sale_id ($sale_amount, credit: $sale_credit) for consolidation. ");
		}
		# Subtract the sale amount wether or not it was covered.
		$total_amount = sprintf ("%.2f",$total_amount - ($sale_amount - $sale_credit));
		$BBD->printLog ("$total_amount remaining\n");
	}
	$BBD->printLog ("Finished Marking sales. $total_amount remaining\n");

	# Calculate any outstanding debt.
	# Debt is any debt existing before covering sales
	my $prev_debt = $memb_credit + $sales_credit;
	$prev_debt = 0.0 if ($prev_debt > 0);
	$prev_debt = sprintf ("%.2f",$prev_debt);
	$BBD->printLog ("Previous debt: $prev_debt\n");

	# new_memb_credit = total_amount
	# Doesn't work with uncovered sales
	# new_memb_credit = $memb_credit + $cc_amount
	# Doesn't work with covered sales
	my $new_memb_credit = sprintf ("%.2f",$memb_credit + $cc_amount);
	$BBD->printLog ("Previous member credit: $memb_credit, new member credit: $new_memb_credit, sales credit: $sales_credit\n");

	# the member's credit is any money left over after covering sales and any outstanding debt
	$DBH->do (UPDATE_USER_CREDIT_BY_ID, undef, $new_memb_credit, $memb_id);
	
	# Mark any sales we have consolidated
	if (scalar @covered_sales) {
		# we need to add enough question marks to the CONSOLIDATE_SALES sql
		my @questions;
		push (@questions,'?') foreach (@covered_sales);
		my $SQL = CONSOLIDATE_SALES.' ('.join (',',@questions).')';
#		$BBD->printLog ("Update Sales SQL: $SQL\n");
		$DBH->do ($SQL, undef, $cc_id, @covered_sales);
	}
	push (@covered_sales,$partial_sale) if $partial_sale;
	emailCCreceipt (\@covered_sales,\%sale_info,$cc_id, $memb_id, $prev_debt, $cc_amount, $cc_kiosk_id, $cc_timestamp, $cc_trans_id);

}


sub emailCCreceipt {
my ($sales,$sale_info,$cc_id, $memb_id, $debt, $cc_amount, $cc_kiosk_id, $cc_timestamp, $cc_trans_id) = @_;
my ($kiosk_id, $kiosk_name, $kiosk_address1, $kiosk_address2, $kiosk_city, $kiosk_state, $kiosk_zip, $kiosk_timezone);



	# Get the member info
	my ($memb_name,$memb_fname,$memb_lname,$id,$login,$type,$status,$credit,$fuel_preauth,$ms_id,$ms_num,$start,$renew,$email,$ad1,$ad2,$city,$state,$zip,$h_ph,$w_ph,$m_ph) =
		$DBH->selectrow_array (GET_USER_INFO_BY_ID,undef,$memb_id);
	return unless $email;

	my $datetime_format = DateTime::Format::Strptime->new(pattern => '%D %r');
	my $date_format = DateTime::Format::Strptime->new(pattern => '%D');
	
	# Gather up the info for all the kiosks involved
	my %kiosk;
	$kiosk{$cc_kiosk_id} = {};
	my $sale_id;
	foreach (values %$sale_info) {
		$kiosk{$_->{kiosk_id}} = {};
	}
	# Construct the SQL to get all of the kiosks involved in the sales receipt
	my @questions;
	push (@questions,'?') foreach (keys %kiosk);
	
	# Do the DB query and populate the kiosk hash
	my $SQL = GET_KIOSK_INFO_BY_IDS.' ('.join (',',@questions).')';
	my $sth = $DBH->prepare($SQL) or die "Could not prepare handle";
	$sth->execute (keys %kiosk) or die "Could not execute GET_KIOSK_INFO_BY_IDS";
	$sth->bind_columns(\$kiosk_id, \$kiosk_name, \$kiosk_address1, \$kiosk_address2, \$kiosk_city, \$kiosk_state, \$kiosk_zip, \$kiosk_timezone);
	while($sth->fetch()) {
		$kiosk{$kiosk_id} = {
			name       => $kiosk_name,
			address1   => $kiosk_address1,
			address2   => $kiosk_address2,
			city       => $kiosk_city,
			state      => $kiosk_state,
			zip        => $kiosk_zip,
			timezone   => $kiosk_timezone
		};
	}
	
	# Format the transaction time for the kiosk's timezone
	my $cc_dt = DateTime->from_epoch( epoch => $cc_timestamp, formatter => $datetime_format);
	$cc_dt->set_time_zone ($kiosk{$cc_kiosk_id}->{timezone});
	
	my $message = $BBD->getCoopName();
	if ($cc_trans_id) {
		$message .= " Credit Card Transaction Receipt\n";
	} else {
		$message .= " Sale Receipt\n";
	}
	$message .= 'Location: '.$kiosk{$cc_kiosk_id}->{name}."\n";
	$message .= '          '.$kiosk{$cc_kiosk_id}->{address1}."\n" if $kiosk{$cc_kiosk_id}->{address1};
	$message .= '          '.$kiosk{$cc_kiosk_id}->{address2}."\n" if $kiosk{$cc_kiosk_id}->{address2};
	$message .= '          '.$kiosk{$cc_kiosk_id}->{city}.", ".$kiosk{$cc_kiosk_id}->{state}.' '.$kiosk{$cc_kiosk_id}->{zip}."\n";
	$message .= "\nDate and time: $cc_dt\n";
	if ($cc_trans_id) {
		$message .= "Credit-card holder: $memb_name\n";
		$message .= "Transaction ID: $cc_trans_id\n";
		$message .= 'Total charged: $'.sprintf ("%.2f",$cc_amount)."\n";
	} else {
		$message .= 'Total: $'.sprintf ("%.2f",$cc_amount)."\n";
	}


	$message .= "\nItemized sales:\n";
	# Format the sale times to their corresponding kiosk timezones
	my $sale;
	my $total_sales = 0.0;
	my $cash_left = $cc_amount;
	my $report_date_cutoff = $cc_dt->clone->subtract ( hours => 1);
	foreach $sale_id (@$sales) {
		$sale = $sale_info->{$sale_id};
		my $dt = DateTime->from_epoch( epoch => $sale->{timestamp}, formatter => $datetime_format );
		$dt->set_time_zone ($kiosk{$sale->{kiosk_id}}->{timezone});		
		$sale->{timestamp} = "$dt";

		$sale->{per_item}  = sprintf ("%.2f", $sale->{per_item});
		$sale->{amount}    = sprintf ("%.2f",$sale->{amount});
		$sale->{quantity}  = sprintf ("%.3f",$sale->{quantity});
		$sale->{credit}  = sprintf ("%.2f",$sale->{credit});
		$message .= '     '.$sale->{item}.', '.$sale->{quantity}.' @ $'.$sale->{per_item};

		my $item_amount = $sale->{amount} - $sale->{credit} >= $cash_left ? $cash_left : $sale->{amount} - $sale->{credit};
		if ($sale->{credit} > 0.0 or $item_amount < $sale->{amount} - $sale->{credit}) {
			$message .= ' ($'.$sale->{amount}.' - $'.$sale->{credit}.' cr.)';
		}
		
		$cash_left -= $item_amount;
		$total_sales += $item_amount;

		$message .= ' : $'.sprintf ("%.2f",$item_amount);
		
		if ($sale->{kiosk_id} ne $cc_kiosk_id or DateTime->compare( $dt, $report_date_cutoff ) < 0) {
			$message .= " ($dt @ ".$kiosk{$sale->{kiosk_id}}->{name}.')';
		}
		$message .= "\n";
	}
	
	$BBD->printLog ("Receipt: After sales, left over CC amount = $cash_left\n");
	# See if the CC was used to cover outstanding debt
	if ($debt and $debt < 0.0 and $cash_left > 0.0) {
		my $covered_debt = sprintf ("%.2f", (-$debt >= $cash_left ? $cash_left : -$debt));
		$message .= '     debt, 1.0 @ '.sprintf ("%.2f",-$debt)." : $covered_debt\n";
		$total_sales += $covered_debt;
	}

	$total_sales  = sprintf ("%.2f", $total_sales);
	
	# See if the CC was used to generate credit
	if ($total_sales and $total_sales < $cc_amount) {
		my $new_credit = sprintf ("%.2f", $cc_amount - $total_sales);
		$message .= '     credit, 1.0 @ '."$new_credit : $new_credit\n";
	}
	
	$message .= "Thank You!\n";
	$message .= "\n";
	$message .= "Account Information for $memb_name\n";
	my $dt_start = DateTime->from_epoch( epoch => $start, formatter => $date_format );
	my $dt_renew = DateTime->from_epoch( epoch => $renew, formatter => $date_format );
	$message .= "     Membership Number: $ms_num\n";
	$message .= "     Member since: $dt_start\n";
	$message .= "     Last renewal: $dt_renew\n";
	$message .= "     Status: $status\n";
	$message .= 'You have $'.sprintf ("%.2f",$credit)." remaining credit in your account.\n"
		if ($credit > 0);
	$message .= 'You have an outstanding balance of $'.sprintf ("%.2f",-$credit)." in your account.\n"
		if ($credit < 0);
	$message .= "\nAddress and contact information:\n";
	$message .= "     $ad1\n" if ($ad1);
	$message .= "     $ad2\n" if ($ad2);
	$message .= "     $city, $state $zip\n" if ($city or $state or $zip);
	$message .= "NOTE: We do not have an address for you!\n" unless ($ad1 and $city and $state and $zip);
	$message .= "     Home phone: $h_ph\n" if ($h_ph);
	$message .= "     Work phone: $w_ph\n" if ($w_ph);
	$message .= "     Cell phone: $m_ph\n" if ($m_ph);
	$message .= "NOTE: We do not have a phone number for you!\n" unless ($h_ph or $w_ph or $m_ph);
	
	#
	# Generate a login-reset session if the member has never logged in.
	if (not $login or length ($login) < 1) {
		my ($sid,$reset_url,$memb_url,$lifetime) = $BBD->getLoginResetSession ($memb_id,$email);
		$message .= "You have never logged-in to your member page.\n";
		$message .= "Use this link to set up your login-in information:\n";
		$message .= "<$reset_url>\n";
		$message .= "This link can only be used once, and will expire in $lifetime days.\n";
		$message .= "After setting up your account, use the link below to access your member page:\n";
		$message .= "<$memb_url>\n";
		$message .= "Your member-page lets you track previous purchases, update contact information, etc.\n";
	} else {
		$message .= "Use your member page to track fuel purchases, update your contact information, etc.:\n";
		$message .= '<'.$BBD->getMemberURL().">\n";
	}

	$message .= "\nThank you for using the BBD9000 Automated Fuel Dispensing Kiosk\n";
	$message .= "http://".$BBD->getCoopDomain()."\n\n";
	
	$BBD->send_email (
		To      => "$memb_name <$email>",
		From    => $BBD->getCoopName().' <donotreply@'.$BBD->getCoopDomain().'>',
		Subject => 'CC Transaction Receipt',
		Message => $message
	);

}

sub email_alert {
	my ($kioskID,$timestamp,$type,$message) = @_;
	my $now = time();
	
	# Construct the SQL for a single kiosk ID, and get the info for it
	my $SQL = GET_KIOSK_INFO_BY_IDS.' (?)';
	my ($kiosk_id, $kiosk_name, $kiosk_address1, $kiosk_address2, $kiosk_city, $kiosk_state, $kiosk_zip, $kiosk_timezone) = 
		$DBH->selectrow_array ($SQL,undef,$kioskID);
	
	# Format the transaction time for the kiosk's timezone
	my $datetime_format = DateTime::Format::Strptime->new(pattern => '%F %T');
	my $dtSent = DateTime->from_epoch( epoch => $timestamp, formatter => $datetime_format);
	$dtSent->set_time_zone( $kiosk_timezone );
	my $dtRcvd = DateTime->from_epoch( epoch => $now, formatter => $datetime_format);
	$dtRcvd->set_time_zone( $kiosk_timezone );

	# Get the names and emails of the members with the alert_role matching the $type
	my ($memb_name,$memb_email);
	my @TOs;
	my $sth = $DBH->prepare(GET_ALERT_MEMBER_EMAILS) or die "Could not prepare handle";
	$sth->execute ($type);
	$sth->bind_columns(\$memb_name,\$memb_email);
	while($sth->fetch()) {
		push (@TOs,"$memb_name <$memb_email>");
	}

	my $kiosk_address = $kiosk_address1;
	$kiosk_address .= "\n$kiosk_address2" if ($kiosk_address2);

	my ($email_subject, $email_message);
	# The kiosk doesn't send 'Network' alerts - these are generated internally.
	# The only way to get a Network alert in this script is if the kiosk came back online.
	if ($type eq 'Network') {
		$email_subject = "BBC Kiosk \"$kiosk_name\" came back online";
		$email_message = << "TEXT";
BBC Kiosk "$kiosk_name" came back online on $dtRcvd.

Kiosk Address:
$kiosk_name
$kiosk_address
$kiosk_city, $kiosk_state $kiosk_zip
TEXT
	} else {
		$email_subject = "$type Alert from BBC Kiosk \"$kiosk_name\"";
		$email_message = << "TEXT";
BBC Kiosk "$kiosk_name" sent a "$type" alert on $dtSent (received $dtRcvd).
Message:
$message

Kiosk Address:
$kiosk_name
$kiosk_address
$kiosk_city, $kiosk_state $kiosk_zip
TEXT
	}
	$BBD->send_email (
		To      => join (' , ',@TOs),
		From    => $BBD->getCoopName().' <donotreply@'.$BBD->getCoopDomain().'>',
		Subject => $email_subject,
		Message => $email_message);
	
}


# renewal sets expiration date to a year from the expiration date
# if we're within the grace period of the expiration date
# if not, then set the expiration date to a year from now
# Upgrades are:
#   ONE-DAY -> FULL : expiration date is a year from today (dues were paid)
#   NON-FUELING -> FULL : expiration date is left alone b/c only first-time fuel charge paid
sub set_expiration {
	my $membership_id = shift;
	my $sale_type = shift;
	my $price = shift;
	my $timezone = $BBD->getTimezone();
	my $expires_dt;
	my $now = DateTime->now();
	

	my ($type,$status,$last_renewal,$expires) = $DBH->selectrow_array (GET_MEMBERSHIP_STATUS,undef,$membership_id);
	my $type_new = $type;
	$type_new = 'FULL' if $sale_type eq 'UPGRADE';
	my $status_new = $status;
	
	# Make sure the number of promotions is updated if we bought a full membership
	if ($sale_type eq 'UPGRADE' && $type eq 'ONE-DAY') {
		$BBD->updateMembershipPromos($price,$kioskID);
	}
	
	# NON-FUELING -> FULL upgrade does not affect dues or expiration date
	if ( not ($sale_type eq 'UPGRADE' and $type eq 'NON-FUELING') ) {
		$expires_dt = DateTime->from_epoch( epoch => $expires );
		$expires_dt->set_time_zone ($timezone);
		
		my ($before_days,$after_days) = $BBD->getRenewalGracePeriod();
		if ($sale_type eq 'RENEWAL' || $sale_type eq 'MEMBERSHIP' || $sale_type eq 'UPGRADE') {
			if (
				$expires_dt->clone()->subtract (days => $before_days)->epoch() < $now->epoch() && 
				$expires_dt->clone()->add (days => $after_days)->epoch() > $now->epoch() ) {
				
					$expires = $expires_dt->add(years => 1)->epoch();
			} else {
					$expires = $now->clone()->add(years => 1)->epoch();
			}
			$last_renewal = $now->epoch();
		}
		$status_new = 'ACTIVE';
	}
	
	$DBH->do (UPDATE_MEMBERSHIP_STATUS,undef,$type_new,$status_new,$last_renewal,$expires,$membership_id);	

}

# return member_id or undef
sub auth_by_CC_mag_name_last4 {
	my ($mag_name,$last4,$spn_in) = @_;
	my $nRows = 0;
	
	$BBD->printLog ("By CC_mag_name_last4; MSR: [$mag_name] last4: [$last4]\n");

	my ($DB_memb_id,$DB_spn);
	my $sth = $DBH->prepare(GET_MEMBER_BY_CC_NAME_LAST4) or die "Could not prepare handle";
	$sth->execute ($mag_name,$last4);
	$sth->bind_columns(\$DB_memb_id,\$DB_spn);

	my $auth_memb_id;
	$BBD->printLog ('DB IDs: ');
	while($sth->fetch()) {
		$nRows++;
		$BBD->printLog ("$DB_memb_id ");
		if (not $auth_memb_id and crypt($spn_in,$DB_spn) eq $DB_spn) {
			$auth_memb_id = $DB_memb_id;
			$BBD->printLog ("<-AUTH ");
		}
	}

	$BBD->printLog ("*** Member not unique ! *****") if $nRows > 1;
	$BBD->printLog ("*** Member not found in DB ! *****") if not $DB_memb_id;
	$BBD->printLog ("*** Member not authorized *****") if not $auth_memb_id;
	$BBD->printLog ("\n");

	return ($auth_memb_id,$DB_memb_id);
}

sub auth_by_CC_name {
	my ($mag_name,$spn_in) = @_;
	my $CCname = mag_name_to_CC_name ($mag_name);
	
	$BBD->printLog ("By CC_name; MSR: [$mag_name] CC Name: [$CCname]\n");

	my ($DB_memb_id,$DB_spn);
	my $sth = $DBH->prepare(GET_MEMBER_BY_CC_NAME) or die "Could not prepare handle";
	$sth->execute ($CCname);
	$sth->bind_columns(\$DB_memb_id,\$DB_spn);

	my $auth_memb_id;
	$BBD->printLog ('DB IDs: ');
	while($sth->fetch()) {
		$BBD->printLog ("$DB_memb_id ");
		if (not $auth_memb_id and crypt($spn_in,$DB_spn) eq $DB_spn) {
			$auth_memb_id = $DB_memb_id;
			$BBD->printLog ("<-AUTH ");
		}
	}

	$BBD->printLog ("*** Member not found in DB ! *****") if not $DB_memb_id;
	$BBD->printLog ("*** Member not authorized *****") if not $auth_memb_id;
	$BBD->printLog ("\n");

	return ($auth_memb_id,$DB_memb_id);
}

sub auth_by_FL_name {
	my ($mag_name,$spn_in) = @_;
	my ($last,$first,$middle) = split (/\//,$mag_name);
	$first = '' unless $first;
	$last = '' unless $last;
	$middle = '' unless $middle;
	my $CCname = "$first $middle $last";
	$CCname =~ s/^\s+//; # strip white space from the beginning
	$CCname =~ s/\s+$//; # strip white space from the end
	$CCname =~ s/\s+/ /g; # replace multiple whitespace with a single one
	$CCname =~ s/[.,]//g; # remove punctuation
	my @pieces = split (/\s/,$CCname);
	my @names;
	my @SQL_L;
	foreach (@pieces[1..$#pieces]) {
		if (length ($_) > 1 and $_ ne 'JR' and $_ ne 'SR' and $_ ne 'INC' and $_ ne 'LLC') {
			push (@SQL_L,"last_name = '$_'");
			push (@names,$_);
		}
	}
	# Exception: If INC or LLC is all we have for a last name, keep it.
	if (! scalar (@SQL_L) ) {
		foreach (@pieces[1..$#pieces]) {
			if (length ($_) > 1 and $_ ne 'JR' and $_ ne 'SR') {
				push (@SQL_L,"last_name = '$_'");
				push (@names,$_);
			}
		}
	}
	# If there's still nothing, make it blank.
	if (! scalar (@SQL_L) ) {
		push (@SQL_L,"last_name = ''");
	}
	
	$BBD->printLog ("By F/L name; MSR: [$mag_name] CC Name: [$CCname] Names: [$pieces[0]] ["
		.join ('] [',@names)."]\n");
	
	my $SQL = GET_SPNS_BY_F_L_NAMES." first_name = '$pieces[0]' AND (".
		join (' OR ',@SQL_L).')';

	my $sth = $DBH->prepare($SQL) or die "Could not prepare handle";
	$sth->execute();
	my ($DB_name,$DB_memb_id,$DB_spn);
	my $auth_memb_id;
	$sth->bind_columns (\$DB_name, \$DB_memb_id, \$DB_spn);
	$BBD->printLog ("DB: ");
	
	while($sth->fetch()) {
		$BBD->printLog ("[$DB_name] ");
		if (not $auth_memb_id and crypt($spn_in,$DB_spn) eq $DB_spn) {
			$auth_memb_id = $DB_memb_id;
			$BBD->printLog ("<-AUTH ");
		}
	}

		$BBD->printLog ("*** Member not found in DB ! *****") if not $DB_memb_id;
		$BBD->printLog ("*** Member not authorized *****") if not $auth_memb_id;
		$BBD->printLog ("\n");
	
	return ($auth_memb_id,$DB_memb_id);

}


sub save_CC_name {
	my ($mag_name,$last4,$memb_id) = @_;
	my $CCname = mag_name_to_CC_name ($mag_name);
	# CCs are registered by their mag_name and last4 digits.
	my ($CC_name_id,$CC_memb_id) = $DBH->selectrow_array (GET_CC_BY_MAG_NAME_LAST4,undef,$mag_name,$last4);

	if (not $CC_name_id) { # CC never before seen
		$DBH->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,$last4,$memb_id);
	} elsif (not $CC_memb_id and $memb_id) { # CC seen before, but memb_id was NULL and now is authorized
		$DBH->do (UPDATE_MEMBER_CC,undef,$memb_id,$CC_name_id);
	} elsif ($CC_memb_id != $memb_id) {
		# Don't re-register this card under a different member id!
	}
}

sub mag_name_to_CC_name {
	my $mag_name = shift;
	my ($last,$first,$middle) = split (/\//,$mag_name);
	$first = '' unless $first;
	$last = '' unless $last;
	$middle = '' unless $middle;
	my $CCname = "$first $middle $last";
	$CCname =~ s/^\s+//; # strip white space from the beginning
	$CCname =~ s/\s+$//; # strip white space from the end
	$CCname =~ s/\s+/ /g; # replace multiple whitespace with a single one
	return $CCname;
}

sub parse_CC_name {
	my $name = shift;
	# Note that we're assuming the name field delimiters ('^') have been stripped off

	my ($last,$first) = split (/\//,$name);
	# So much for the standard encoding.
	# 1.  Leading and/or trailing white-space occurs
	$first =~ s/^\s+// if $first; # strip white space from the beginning
	$first =~ s/\s+$// if $first; # strip white space from the end
	$last =~ s/^\s+// if $last; # strip white space from the beginning
	$last =~ s/\s+$// if $last; # strip white space from the end
	# 2.  First and last names may have trailing initials, suffixes, etc.
	my @first_pieces = split (/\s/,$first);
	my @last_pieces = split (/\s/,$last);
	$first = $first_pieces[0];
	$last = $last_pieces[0];
	# 3.  Occasionally, the whole name is encoded as the last name
	#     as in "First I Last".
	#     Sometimes this is followed by a first/last delimiter '/',
	#     but only sometimes.
	# That's what ISO standards are for, folks - so that you can make up your own shit.
	if (! scalar (@first_pieces)) {
		$first = $last_pieces[0];
		$last = '';
		# We only count it is a last name if its more than one letter.
		# Do double-initials ever occur?  We'll find out soon enough...
		$last = $last_pieces[1] if $last_pieces[1] and length ($last_pieces[1]) > 1;
		$last = $last_pieces[2] if not $last and $last_pieces[2] and length ($last_pieces[2]) > 1;
	}
	$BBD->printLog ("CC Name [$name] First: [$first] Last: [$last]\n");

	return ($first,$last)

}


