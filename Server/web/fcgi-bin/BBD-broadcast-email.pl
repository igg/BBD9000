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

use Email::Simple;

use BBD;
our $BBD = new BBD('BBD-broadcast-email.pl');

$BBD->myRole ('Broadcast Email');
$BBD->myTemplate ('BBD-broadcast-email.tmpl');

###
# Our globals
our ($CGI, $DBH, $member_id, $member_info, $p_from, $p_to, $exp_to, $broadcast_type);

use constant GET_ALL_EMAILS => <<"SQL";
	SELECT DISTINCT m.name,m.email
	FROM members m
SQL

use constant GET_EMAILS_BY_PURCHASE_DATE => <<"SQL";
	SELECT DISTINCT m.name,m.email
	FROM members m, sales s
	WHERE s.member_id = m.member_id
	     AND UNIX_TIMESTAMP(s.timestamp) >= ?
	     AND UNIX_TIMESTAMP(s.timestamp) <= ?
SQL

use constant GET_EMAILS_BY_EXP_DATE => <<"SQL";
	SELECT DISTINCT m.name,m.email
	FROM members m, memberships ms
	WHERE m.membership_id = ms.membership_id
	     AND UNIX_TIMESTAMP(ms.expires) <= ?
SQL



$BBD->init(\&do_request);

sub do_request {

##
# Initialize our globals
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	$member_id = $BBD->session_param ('member_id');
	$member_info = $BBD->get_member_info ($member_id);
	$p_from = undef;
	$p_to = undef;
	$exp_to = undef;
	$broadcast_type = 'broadcast_all';
	
	$BBD->relogin () unless $member_info->{ms_id};
	
	
	###
	# test_email button: Send a test email
	if ( $BBD->safeCGIparam ('test_email') ) {
		send_email('test');
	} elsif ( $BBD->safeCGIparam ('broadcast_email') ) {
		send_email( $BBD->safeCGIparam ('broadcast_type') );
	}
	show_form();
}


sub send_email {
my $type = shift;

	my $param = parse_header ( $CGI->param('email_body') );
	if (not exists $param->{To}) {
		$BBD->error ('No "To" field found. You probably did not paste the "Raw Source" of your email.');
		$CGI->param('email_body','');
		return();
	}
	if (not exists $param->{From}) {
		$BBD->error ('No "From" field found. You probably did not paste the "Raw Source" of your email.');
		$CGI->param('email_body','');
		return();
	}
	if (not exists $param->{Subject}) {
		$BBD->error ('No "Subject" field found. You probably did not paste the "Raw Source" of your email.');
		$CGI->param('email_body','');
		return();
	}

	# This confuses the emailer
#	delete $param->{'Mime-Version'};
	delete $param->{'Reply-To'};
	$param->{From} = $BBD->getCoopName().' <donotreply@'.$BBD->getCoopDomain().'>';
	my $rx = $Mail::Sendmail::address_rx;

	
	if ($type eq 'test') {
		if ($member_info->{email} =~ /$rx/) {
			$param->{To} = "$member_info->{memb_name} <$member_info->{email}>";
			$BBD->send_email (%$param);
			$BBD->error ('Test email sent to: '.$param->{To}.' from: '.$param->{From});
		} else {
			$BBD->error ("Your email address <$member_info->{email}> is invalid!");
			return;
		}
	} else {
		$broadcast_type = $type;
		my $sth;
		my $nEmails=0;
		my ($name,$email);
		my $dt = DateTime->now;
		$dt->set_time_zone ($BBD->getTimezone());
		# a second to midnight today
		$dt->set (hour=>23, minute=>59, second=>59);

		if ($type eq 'broadcast_all') {
			$sth = $DBH->prepare(GET_ALL_EMAILS) or die "Could not prepare handle";
			$sth->execute( );
		} elsif ($type eq 'broadcast_purchase_date') {
			$p_from = $BBD->safeCGIparam('p_from');
			if ( !$p_from or ! $BBD->isNumber ($p_from) or $p_from > $dt->epoch() ) {
				$p_from = undef;
				$BBD->error ("Invalid From date for purchases");
				return;
			}
			$p_to = $BBD->safeCGIparam('p_to');
			if ( !$p_to or ! $BBD->isNumber ($p_to) or $p_to > $dt->epoch() ) {
				$p_to = undef;
				$BBD->error ("Invalid To date for purchases");
				return;
			}
			if ($p_from > $p_to) {
				$p_to = undef;
				$p_from = undef;
				$BBD->error ("From purchase date cannot be after To date");
				return;
			}
			$p_to = DateTime->from_epoch (epoch =>$p_to, time_zone=>$BBD->getTimezone())->
				set (hour=>23, minute=>59, second=>59)->epoch();
			$p_from = DateTime->from_epoch (epoch =>$p_from, time_zone=>$BBD->getTimezone())->
				set (hour=>0, minute=>0, second=>0)->epoch();

			$sth = $DBH->prepare(GET_EMAILS_BY_PURCHASE_DATE) or die "Could not prepare handle";
			$sth->execute( $p_from, $p_to );
		} elsif ($type eq 'broadcast_exp_date') {
			$exp_to = $BBD->safeCGIparam('exp_to');
			if ( !$exp_to or ! $BBD->isNumber ($exp_to) or $exp_to > $dt->epoch() ) {
				$exp_to = undef;
				$BBD->error ("Invalid Expiration date");
				return;
			}

			$exp_to = DateTime->from_epoch (epoch =>$exp_to, time_zone=>$BBD->getTimezone())->
				set (hour=>23, minute=>59, second=>59)->epoch();

			$sth = $DBH->prepare(GET_EMAILS_BY_EXP_DATE) or die "Could not prepare handle";
			$sth->execute($exp_to);
		}

		$sth->bind_columns (\$name,\$email);
		while($sth->fetch()) {
			if ($email =~ /$rx/) {
				$param->{To} = "$name <$email>";
				$BBD->send_email (%$param);
				$nEmails++;
			}
		}
		$BBD->error ("$nEmails emails sent");
	}
}



sub show_form {
	
	my ($broadcast_all_checked,$broadcast_purchase_date_checked,$broadcast_exp_date_checked) = 
		('checked',undef,undef);

	if ($broadcast_type eq 'broadcast_all') {
		$broadcast_all_checked = 'checked';
		$broadcast_purchase_date_checked = undef;
		$broadcast_exp_date_checked = undef;
	} elsif ($broadcast_type eq 'broadcast_purchase_date') {
		$broadcast_all_checked = undef;
		$broadcast_purchase_date_checked = 'checked';
		$broadcast_exp_date_checked = undef;
	} elsif ($broadcast_type eq 'broadcast_exp_date') {
		$broadcast_all_checked = undef;
		$broadcast_purchase_date_checked = undef;
		$broadcast_exp_date_checked = 'checked';
	}

	my $dt = DateTime->now;
	$dt->set_time_zone ($BBD->getTimezone());
	# a second to midnight today
	$dt->set (hour=>23, minute=>59, second=>59);
	if (! $p_to ) {
		$p_to = $dt->epoch();
	}
	if (! $p_from ) {
		# Default p_from is 7 days earlier than p_to at midnight.
		$p_from = DateTime->from_epoch (epoch =>$p_to, time_zone=>$BBD->getTimezone())->
			subtract (days => 7)->set (hour=>0, minute=>0, second=>0)->epoch();
	} 
	if (! $exp_to ) {
		$exp_to = $dt->epoch();
	}


	$BBD->{TEMPLATE}->param(
		broadcast_all_checked => $broadcast_all_checked,
		broadcast_purchase_date_checked => $broadcast_purchase_date_checked,
		broadcast_exp_date_checked => $broadcast_exp_date_checked,
		p_from     => $BBD->epoch_to_ISOdate ($p_from),
		p_to       => $BBD->epoch_to_ISOdate ($p_to),
		exp_to     => $BBD->epoch_to_ISOdate ($exp_to),
		email_body => scalar($CGI->param('email_body')),
	);

	$BBD->HTML_out();
}


sub parse_header {
	my $email = Email::Simple->new(shift);
	my %clean_headers;
	my @headers = $email->header_names;
	
	foreach my $header (@headers) {
		if ($header eq 'To'
			or $header eq 'From'
			or $header eq 'Subject'
			or $header eq 'Mime-Version'
			or $header =~ /^Content-/
			) {
				$clean_headers{$header} = $email->header($header);
			}
	}
	$BBD->printLog("Clean Headers:\n");
	$BBD->printLog("[$_]: [".$clean_headers{$_}."]\n") foreach (keys %clean_headers);
	$BBD->printLog("^^^^^^^^^^^\n");

	$clean_headers{Message} = $email->body;
	return \%clean_headers;
}
