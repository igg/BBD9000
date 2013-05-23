#!/usr/bin/perl -w
# Coop sales
# -----------------------------------------------------------------------------
# BBD-purchases.pl:  Coop sales
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
our $BBD = new BBD('BBD-purchases.pl');


use constant GET_N_LAST_MEMBER_PURCHASES => <<SQL;
	SELECT m.name,UNIX_TIMESTAMP(s.timestamp) as timestamp, k.name, s.item, s.quantity, s.per_item, s.amount, s.credit,
		c.amount, c.auth_code, c.gwy_trans_id
	FROM sales s, cc_transactions c, members m, kiosks k
	WHERE s.cc_trans_id = c.cc_trans_id
	AND s.member_id = m.member_id
	AND s.kiosk_id = k.kiosk_id
	AND s.member_id = ?
	AND UNIX_TIMESTAMP(s.timestamp) > ?
	UNION
	SELECT m.name,UNIX_TIMESTAMP(s.timestamp) as timestamp, k.name, s.item, s.quantity, s.per_item, s.amount, s.credit,
		'N/A', 'N/A', 'N/A'
	FROM sales s, members m, kiosks k
	WHERE s.cc_trans_id is null
	AND s.member_id = m.member_id
	AND s.kiosk_id = k.kiosk_id
	AND s.member_id = ?
	AND UNIX_TIMESTAMP(s.timestamp) > ?


	UNION
	SELECT m.name,UNIX_TIMESTAMP(c.timestamp) as timestamp, k.name, 'N/A', 'N/A', 'N/A', 'N/A', 'N/A',
		c.amount, c.auth_code, c.gwy_trans_id
	FROM cc_transactions c, members m, kiosks k
	WHERE NOT EXISTS (SELECT cc_trans_id FROM sales WHERE sales.cc_trans_id = c.cc_trans_id)
	AND c.member_id = m.member_id
	AND c.kiosk_id = k.kiosk_id
	AND c.member_id = ?
	AND UNIX_TIMESTAMP(c.timestamp) > ?
	ORDER BY timestamp DESC
SQL

use constant GET_N_LAST_MEMBERSHIP_PURCHASES => <<SQL;
	SELECT m.name, UNIX_TIMESTAMP(s.timestamp) as timestamp, k.name, s.item, s.quantity, s.per_item, s.amount, s.credit,
		c.amount, c.auth_code, c.gwy_trans_id
	FROM sales s, cc_transactions c, members m, kiosks k
	WHERE s.cc_trans_id = c.cc_trans_id
	AND s.member_id = m.member_id
	AND s.kiosk_id = k.kiosk_id
	AND m.membership_id = ?
	AND UNIX_TIMESTAMP(s.timestamp) > ?
	UNION
	SELECT m.name, UNIX_TIMESTAMP(s.timestamp) as timestamp, k.name, s.item, s.quantity, s.per_item, s.amount, s.credit,
		'N/A', 'N/A', 'N/A'
	FROM sales s, members m, kiosks k
	WHERE s.cc_trans_id is null
	AND s.member_id = m.member_id
	AND s.kiosk_id = k.kiosk_id
	AND m.membership_id = ?
	AND UNIX_TIMESTAMP(s.timestamp) > ?
	UNION
	SELECT m.name, UNIX_TIMESTAMP(c.timestamp) as timestamp, k.name, 'N/A', 'N/A', 'N/A', 'N/A', 'N/A',
		c.amount, c.auth_code, c.gwy_trans_id
	FROM cc_transactions c, members m, kiosks k
	WHERE NOT EXISTS (SELECT cc_trans_id FROM sales WHERE sales.cc_trans_id = c.cc_trans_id)
	AND c.kiosk_id = k.kiosk_id
	AND c.member_id = m.member_id
	AND m.membership_id = ?
	AND UNIX_TIMESTAMP(c.timestamp) > ?



	ORDER BY timestamp DESC
SQL

use constant GET_N_LAST_PURCHASES => <<SQL;
	SELECT m.name, UNIX_TIMESTAMP(s.timestamp) as timestamp, k.name, s.item, s.quantity, s.per_item, s.amount, s.credit,
		c.amount, c.auth_code, c.gwy_trans_id
	FROM sales s, cc_transactions c, members m, kiosks k
	WHERE s.cc_trans_id = c.cc_trans_id
	AND s.kiosk_id = k.kiosk_id
	AND c.member_id = m.member_id
	AND UNIX_TIMESTAMP(s.timestamp) > ?


	UNION


	SELECT m.name, UNIX_TIMESTAMP(s.timestamp) as timestamp, k.name, s.item, s.quantity, s.per_item, s.amount, s.credit,
		'N/A', 'N/A', 'N/A'
	FROM sales s, members m, kiosks k
	WHERE s.cc_trans_id is null
	AND s.kiosk_id = k.kiosk_id
	AND s.member_id = m.member_id
	AND UNIX_TIMESTAMP(s.timestamp) > ?


	UNION
	SELECT m.name, UNIX_TIMESTAMP(c.timestamp) as timestamp, k.name, 'N/A', 'N/A', 'N/A', 'N/A', 'N/A',
		c.amount, c.auth_code, c.gwy_trans_id
	FROM cc_transactions c, members m, kiosks k
	WHERE NOT EXISTS (SELECT cc_trans_id FROM sales WHERE sales.cc_trans_id = c.cc_trans_id)
	AND c.kiosk_id = k.kiosk_id
	AND c.member_id = m.member_id
	AND UNIX_TIMESTAMP(c.timestamp) > ?



	ORDER BY timestamp DESC
SQL


use constant GET_KIOSK_INFO => <<SQL;
	SELECT kiosk_id, name, timezone
	FROM kiosks
SQL


use Scalar::Util qw(looks_like_number);


$BBD->myRole ('Purchases');
$BBD->myTemplate ('BBD-purchases.tmpl');


###
# Our globals
our ($CGI, $DBH, $member_id, $member_info);
our %sale_types;
our @sale_types_list;
our %kiosks;
our ($p_from,$p_to,$sale_type,$can_all_sales,$can_ms_sales);

$BBD->init(\&do_request);


sub do_request {

####
# Initialize our globals
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	%sale_types = ();
	@sale_types_list = ();
	%kiosks = ();
	($p_from,$p_to,$sale_type,$can_all_sales,$can_ms_sales) = (undef,undef,undef,undef,undef);

	$member_id = $BBD->session_param ('member_id');
	$member_info = $BBD->get_member_info ($member_id);
	
	$BBD->relogin () unless $member_info->{ms_id};

####
# Populate our purchase types
	%sale_types = (memb_sales => $member_info->{memb_name}."'s");
	push (@sale_types_list,'memb_sales');
	if ( $BBD->has_role('Family Purchases') ) {
		$can_ms_sales = 1;
		$sale_types{family_sales} = $member_info->{memb_name}."'s Family";
		push (@sale_types_list,'family_sales');
	}
	if ( $BBD->has_role('All Purchases') ) {
		$can_all_sales = 1;
		$sale_types{all_sales} = 'All';
		push (@sale_types_list,'all_sales');
	}

####
# Gather kiosk info - needed for timezones and proper time display
	my ($kiosk_id,$k_name,$k_tz);
	my $sth = $DBH->prepare(GET_KIOSK_INFO) or die "Could not prepare handle";
	$sth->execute( );
	$sth->bind_columns (\$kiosk_id,\$k_name,\$k_tz);
	while($sth->fetch()) {
		$kiosks{$k_name} = {
			kiosk_id => $kiosk_id,
			name => $k_name,
			tz => $k_tz,
		};
	}

####
# Process CGI params

	$p_from = $BBD->safeCGIparam('p_from');
	if ( !$p_from or ! $BBD->isNumber ($p_from) ) {
		$p_from = undef;
	}

	$p_to = $BBD->safeCGIparam('p_to');
	if ( !$p_to or ! $BBD->isNumber ($p_to) ) {
		$p_to = undef;
	}

	$sale_type = $BBD->safeCGIparam('popup_purchase_types');
	if ( !$sale_type or ! exists $sale_types{$sale_type} ) {
		$sale_type = undef;
	}

	
	if (! $sale_type) {
		$sale_type = 'memb_sales';
	}
	
	show_form();
}



sub show_form {

	my @transactions = ();
	my ($m_name,$timestamp,$k_name,$item,$s_quantity,$s_per_item,$s_amount,$s_credit,$c_amount,$c_auth_code,$c_gwy_trans_id);
	my $show_name;
	my $sth;
	
	# Should collect data for past 30 days regardless of $p_from and $p_to.
	# populate table based on p_from and p_to, but calculate totals for
	# the indicated timespan, $past_7 and $past_30.
	# Pre-calculate timestamps for past_7 and past_30 (based on local midnight)
	my $dt = DateTime->now;
	$dt->set_time_zone ($BBD->getTimezone());
	# a second to midnight today
	$dt->set (hour=>23, minute=>59, second=>59);

	$p_to = $dt->epoch() if (not defined $p_to or $p_to <= $dt->epoch());
	undef ($p_from) if (defined $p_from and $p_from > $p_to);
	
	$p_to = DateTime->from_epoch (epoch =>$p_to, time_zone=>$BBD->getTimezone())->
		set (hour=>23, minute=>59, second=>59)->epoch();

	if (! $p_from ) {
		# Default p_from is 30 days earlier than p_to at midnight.
		$p_from = DateTime->from_epoch (epoch =>$p_to, time_zone=>$BBD->getTimezone())->
			subtract (days => 30)->set (hour=>0, minute=>0, second=>0)->epoch();
	} else {
		$p_from = DateTime->from_epoch (epoch =>$p_from, time_zone=>$BBD->getTimezone())->
			set (hour=>0, minute=>0, second=>0)->epoch();
	}
	
	# Make new ones 7 and 30 days ago.
	my ($past7,$past30) = (
		$dt->clone->subtract (days => 7)->set(hour=>0, minute=>0, second=>0)->epoch(),
		$dt->clone->subtract (days => 30)->set (hour=>0, minute=>0, second=>0)->epoch() );
	# The DB_since is the smaller of $past_30 or $p_from
	my $DB_since = $past30 < $p_from ? $past30 : $p_from;
	# The query will go up to the latest transaction for past7 and past30

	# Totals
	my ($table_s_quantity_t, $table_s_amount_t, $table_s_credit_t, $table_c_amount_t) = (0.0, 0.0, 0.0, 0.0);
	my ($past7_s_quantity_t, $past7_s_amount_t, $past7_s_credit_t, $past7_c_amount_t) = (0.0, 0.0, 0.0, 0.0);
	my ($past30_s_quantity_t, $past30_s_amount_t, $past30_s_credit_t, $past30_c_amount_t) = (0.0, 0.0, 0.0, 0.0);

	if ($sale_type eq 'memb_sales') {
		$sth = $DBH->prepare(GET_N_LAST_MEMBER_PURCHASES) or die "Could not prepare handle";
		$sth->execute( $member_info->{memb_id}, $DB_since, $member_info->{memb_id}, $DB_since, $member_info->{memb_id}, $DB_since  );
	} elsif ($sale_type eq 'family_sales') {
		$show_name = 1;
		$sth = $DBH->prepare(GET_N_LAST_MEMBERSHIP_PURCHASES) or die "Could not prepare handle";
		$sth->execute( $member_info->{ms_id}, $DB_since, $member_info->{ms_id}, $DB_since, $member_info->{ms_id}, $DB_since  );
	} elsif ($sale_type eq 'all_sales') {
		$show_name = 1;
		$sth = $DBH->prepare(GET_N_LAST_PURCHASES) or die "Could not prepare handle";
		$sth->execute( $DB_since, $DB_since, $DB_since  );
	}
	#
	# CC transactions can appear multiple times because they can cover multiple sales.
	# For totals, we can only count them once per time-span, so we keep them in a hash.
	my %CC_Trans;
	
	if ($sth) {
		$sth->bind_columns (\$m_name,\$timestamp,\$k_name,\$item,
			\$s_quantity,\$s_per_item,\$s_amount,\$s_credit,
			\$c_amount,\$c_auth_code,\$c_gwy_trans_id
		);
		while($sth->fetch()) {
			if ($timestamp >= $p_from and $timestamp < $p_to) {
				push (@transactions, {
					show_name => $show_name,
					m_name => $m_name,
					timestamp => $BBD->epoch_to_ISOdatetime ($timestamp,$kiosks{$k_name}->{tz}),
					k_name => $k_name,
					item => $item,
					s_quantity => looks_like_number ($s_quantity) ? sprintf ('%.3f',$s_quantity) : $s_quantity,
					s_per_item => looks_like_number ($s_per_item) ? sprintf ('$%.2f',$s_per_item) : $s_per_item,
					s_amount => looks_like_number ($s_amount) ? sprintf ('$%.2f',$s_amount) : $s_amount,
					s_credit => looks_like_number ($s_credit) ? sprintf ('$%.2f',$s_credit) : $s_credit,
					c_amount => looks_like_number ($c_amount) ? sprintf ('$%.2f',$c_amount) : $c_amount,
					c_auth_code => $c_auth_code,
					c_gwy_trans_id => $c_gwy_trans_id,
				});
				$table_s_quantity_t += $s_quantity if looks_like_number ($s_quantity);
				$table_s_amount_t   += $s_amount if looks_like_number ($s_amount);
				$table_s_credit_t   += $s_credit if looks_like_number ($s_credit);
				$table_c_amount_t   += $c_amount if looks_like_number ($c_amount)
					and not $c_auth_code =~ /^[0]+$/ and not exists $CC_Trans{$c_gwy_trans_id};
			}
			
			if ($timestamp >= $past7) {
				$past7_s_quantity_t += $s_quantity if looks_like_number ($s_quantity);
				$past7_s_amount_t   += $s_amount if looks_like_number ($s_amount);
				$past7_s_credit_t   += $s_credit if looks_like_number ($s_credit);
				$past7_c_amount_t   += $c_amount if looks_like_number ($c_amount)
					and not $c_auth_code =~ /^[0]+$/ and not exists $CC_Trans{$c_gwy_trans_id};
			}
			
			if ($timestamp >= $past30) {
				$past30_s_quantity_t += $s_quantity if looks_like_number ($s_quantity);
				$past30_s_amount_t   += $s_amount if looks_like_number ($s_amount);
				$past30_s_credit_t   += $s_credit if looks_like_number ($s_credit);
				$past30_c_amount_t   += $c_amount if looks_like_number ($c_amount)
					and not $c_auth_code =~ /^[0]+$/ and not exists $CC_Trans{$c_gwy_trans_id};
			}
			
			$CC_Trans{$c_gwy_trans_id} = 1;
			
		}
	}



	$BBD->{TEMPLATE}->param(
		show_name => $show_name,
		transactions => \@transactions,
		p_from => $BBD->epoch_to_ISOdate ($p_from),
		p_to => $BBD->epoch_to_ISOdate ($p_to),

		table_s_quantity_t => sprintf ('%.3f',$table_s_quantity_t),
		table_s_amount_t   => sprintf ('%.2f',$table_s_amount_t),
		table_s_credit_t   => sprintf ('%.2f',$table_s_credit_t),
		table_c_amount_t   => sprintf ('%.2f',$table_c_amount_t),

		past7_s_quantity_t => sprintf ('%.3f',$past7_s_quantity_t),
		past7_s_amount_t   => sprintf ('%.2f',$past7_s_amount_t),
		past7_s_credit_t   => sprintf ('%.2f',$past7_s_credit_t),
		past7_c_amount_t   => sprintf ('%.2f',$past7_c_amount_t),

		past30_s_quantity_t => sprintf ('%.3f',$past30_s_quantity_t),
		past30_s_amount_t   => sprintf ('%.2f',$past30_s_amount_t),
		past30_s_credit_t   => sprintf ('%.2f',$past30_s_credit_t),
		past30_c_amount_t   => sprintf ('%.2f',$past30_c_amount_t),

		popup_purchase_types => $CGI->popup_menu(
			-name =>'popup_purchase_types',
			-values =>\@sale_types_list,
			-labels =>\%sale_types,
			-class  => 'popup_menu',
			-default => $sale_type,
			-override => 1,
		),
	);
	$BBD->HTML_out();
}
