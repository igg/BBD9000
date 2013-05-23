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
our $BBD = new BBD('BBD-fuel_deliveries.pl');

use constant GET_N_LAST_MEMBER_FUEL_DELIVERIES => <<"SQL";
	SELECT UNIX_TIMESTAMP(d.timestamp), member_id, from_kiosk_id, kiosk_id, d.quantity, d.price_per_item, d.total_price
	FROM deliveries d
	WHERE d.item = "fuel"
		AND d.member_id = ?
	ORDER BY d.timestamp DESC
	LIMIT ?
SQL

use constant GET_N_LAST_FUEL_DELIVERIES => <<"SQL";
	SELECT UNIX_TIMESTAMP(d.timestamp), member_id, from_kiosk_id, kiosk_id, d.quantity, d.price_per_item, d.total_price
	FROM deliveries d
	WHERE d.item = "fuel"
	ORDER BY d.timestamp DESC
	LIMIT ?
SQL

use constant GET_KIOSK_INFO => <<"SQL";
	SELECT kiosk_id, name, fuel_capacity, fuel_value, fuel_avail, fuel_price, fuel_type, UNIX_TIMESTAMP(last_checkin), timezone
	FROM kiosks
	ORDER BY name
SQL

use constant GET_SUPPLIERS => <<"SQL";
	SELECT m.member_id, m.name
	FROM members m, memberships ms
	WHERE 
		m.membership_id = ms.membership_id
		AND ms.type = "SUPPLIER"
		UNION (
			SELECT m.member_id, m.name
			FROM  members m, member_roles mr, roles r
			WHERE
				mr.member_id = m.member_id
				AND mr.role_id = r.role_id
				AND r.role_id = "Supplier"
		)
SQL

use constant INSERT_FUEL_AVAIL_MESSAGE => <<"SQL";
	INSERT INTO messages SET
		kiosk_id = ?,
		parameter = "avail_gallons",
		value = ?
SQL

use constant INSERT_ADD_FUEL_MESSAGE => <<"SQL";
	INSERT INTO messages SET
		kiosk_id = ?,
		parameter = "add_gallons",
		value = ?
SQL

use constant UPDATE_FUEL_PRICE => <<"SQL";
	UPDATE kiosks SET
		fuel_price = ?
	WHERE
		kiosk_id = "fuel"
SQL

use constant INSERT_DELIVERY => <<"SQL";
	INSERT INTO deliveries SET
		member_id = ?,
		from_kiosk_id = ?,
		timestamp = FROM_UNIXTIME(?),
		kiosk_id = ?,
		item = 'fuel',
		quantity = ?,
		price_per_item = ?,
		total_price = ?
SQL



use constant ADD_KIOSK_FUEL => <<"SQL";
	UPDATE kiosks SET
		fuel_avail = fuel_avail + ?,
		fuel_value = ?
	WHERE
		kiosk_id = ?
SQL

use constant SET_KIOSK_FUEL => <<"SQL";
	UPDATE kiosks SET
		fuel_avail = ?
	WHERE
		kiosk_id = ?
SQL

use constant N_FUEL_DELIVERIES => 20;

use constant PER_GALLON_MARKUP => 0.45;

$BBD->myRole ('Fuel Deliveries');
$BBD->myTemplate ('BBD-fuel_deliveries.tmpl');


###
# Our globals
our ($CGI, $DBH, $member_id, $member_info);
our @kiosk_tmpl_loop;
our %kiosk_names;
our %suppliers;
our @from_list;
our %from_labels;
our $fuel_czar;
our $current_fuel_price;
our $sugg_price;
our $k_cap_total;
our $k_avail_total;
our $k_value_total;
our $k_price_total;

$BBD->init(\&do_request);


sub do_request {
	
	$CGI = $BBD->CGI();
	$DBH = $BBD->DBH();
	@kiosk_tmpl_loop = ();
	%kiosk_names = ();
	%suppliers = ();
	@from_list = ();
	%from_labels = ();
	$fuel_czar = undef;
	$sugg_price = undef;
	$k_cap_total = 0.0;
	$k_avail_total = 0.0;
	$k_value_total = 0.0;
	$k_price_total = 0.0;

	
	
	
	$member_id = $BBD->session_param ('member_id');
	$member_info = $BBD->get_member_info ($member_id);
	
	$BBD->relogin () unless $member_info->{ms_id};
	# My fuel deliveries or all of them?
	# A member type SUPPLIER only does 'My Deliveries'
	# Other members may have a 'Fuel Deliveries' role.
	# Unless they also have a 'Fuel Czar' role, they only get to see what they delivered
	$fuel_czar = 1 if $BBD->has_role('Fuel Czar');

	%suppliers = @{
		$DBH->selectcol_arrayref(GET_SUPPLIERS,{ Columns=>[1,2]})
	};
	
	my %kiosks;
	my ($kiosk_id,$k_name,$k_capacity,$k_fuel_value,$k_fuel_avail,$k_fuel_price,$k_fuel_type,$k_last_checkin,$k_tz);
	my $sth = $DBH->prepare(GET_KIOSK_INFO) or die "Could not prepare handle";
	$sth->execute( );
	$sth->bind_columns (\$kiosk_id,\$k_name,\$k_capacity,\$k_fuel_value,\$k_fuel_avail,\$k_fuel_price,\$k_fuel_type,\$k_last_checkin,\$k_tz);
	while($sth->fetch()) {
		$k_cap_total += $k_capacity;
		$k_avail_total += $k_fuel_avail;
		$k_value_total += ($k_fuel_value * $k_fuel_avail);
		$k_price_total += ($k_fuel_price * $k_fuel_avail);
		$kiosks{$kiosk_id} = {
			name => $k_name,
			capacity => $k_capacity,
			fuel_value => $k_fuel_value,
			fuel_avail => $k_fuel_avail,
			fuel_price => $k_fuel_price,
			fuel_type => $k_fuel_type,
			tz => $k_tz,
			last_checkin => $k_last_checkin,
		};
		$kiosk_names{$kiosk_id} = $k_name;
	}
	
	push (@from_list,"S_$member_id") if exists $suppliers{$member_id};
	$from_labels{"S_$member_id"} = 'S: '.$suppliers{$member_id} if exists $suppliers{$member_id};

	my @update_kiosks;
	if ($fuel_czar) {
		foreach (keys %suppliers) {
			if ($_ != $member_id) {
				push (@from_list,"S_$_");
				$from_labels{"S_$_"} = 'S: '.$suppliers{$_};
			}
		}
		foreach (keys %kiosks) {
			push (@from_list,"K_$_");
			$from_labels{"K_$_"} = 'K: '.$kiosks{$_}->{name};
			if ($CGI->param('update_kiosks')) {
				push (@update_kiosks,$_) if $BBD->safeCGIparam("do_edit_$_");
			}
		}
	}
	
	# The from_popup is a list of suppliers and kiosks

	my @update_names;
	foreach my $kiosk_id (@update_kiosks) {		
		my ($form_avail, $form_type, $form_price) = (
			$BBD->safeCGIparam ("edit_available_$kiosk_id"),
			$BBD->safeCGIparam ("edit_type_$kiosk_id"),
			$BBD->safeCGIparam ("edit_price_$kiosk_id"),
		);
		
		$form_price =~ s/\$//;
#$BBD->printLog ("Updating kiosk $kiosk_id: avail [$form_avail], type [$form_type] price [$form_price]\n");

		push (@update_names,$kiosks{$kiosk_id}->{name});
		$BBD->updateKioskFuel ($kiosk_id,
			updateKiosk => 'fromParam', updateDB => 'fromParam',
			avail => $form_avail, type => $form_type, price => $form_price
		);
	}

	if (scalar (@update_names)) {
		$BBD->error ("Updated Fuel in: ".join (', ',@update_names).".");
		show_form();
		return undef;
	}

	if ($CGI->param('add_delivery')) {
		# Javascript should have converted to a local unix timestamp
		my $timestamp = $BBD->safeCGIparam('timestamp');
		if (! $timestamp or ! $BBD->isCGInumber ('timestamp') ) {
			$BBD->error ('Invalid date/time format');
			show_form();
			return undef;
		}
	
		
		my $supplier = $BBD->safeCGIparam ('supplier_popup');
		my ($supplier_id,$from_kiosk);
		if ($supplier =~ /^S_(\d+)$/ and exists $suppliers{$1}) {
			$supplier_id = $1;
		} elsif ($supplier =~ /^K_(\d+)$/ and exists $kiosks{$1}) {
			$from_kiosk = $1;
		} else {
			$BBD->error ('Invalid Supplier');
			show_form();
			return undef;
		}
		
		my $kiosk_id = $BBD->safeCGIparam ('kiosk_popup_delivery');
		if (! $kiosk_id or not exists $kiosks{$kiosk_id}) {
			$BBD->error ('Invalid Kiosk');
			show_form();
			return undef;
		}
	
		
		my $gallons = $BBD->safeCGIparam ('gallons');
		if (! $gallons or !$BBD->isCGInumber ('gallons')
			or not ($gallons > 0.0 and $gallons < $kiosks{$kiosk_id}->{capacity}) ) {
				$BBD->error ('Gallons delivered must be between 0 and '.
					$kiosks{$kiosk_id}->{capacity}.' ('.$kiosks{$kiosk_id}->{name}.' capacity).');
				show_form();
				return undef;
		}
		
		my $ppg = $BBD->safeCGIparam ('ppg');		
		my $deliv_total = $BBD->safeCGIparam ('total');
		
		# Total blank, ppg set - get total from ppg
		if (!$deliv_total and ($ppg and $BBD->isCGInumber ('ppg')) ) {
			$deliv_total = $ppg * $gallons;
		# PPG blank, total set - get ppg from total
		} elsif (!$ppg and ($deliv_total and $BBD->isCGInumber ('total')) ) {
			$ppg = $deliv_total / $gallons;
		# Both blank for a transfer
		} elsif (!$ppg and !$deliv_total and $from_kiosk) {
			$ppg = $kiosks{$from_kiosk}->{fuel_value};
			$deliv_total = $ppg * $gallons;
		}
		if (! $ppg ) {
			$BBD->error ('Invalid price per gallon');
			show_form();
			return undef;
		}

		if (! $deliv_total 
			or not (abs ($deliv_total - ($ppg * $gallons)) < ($deliv_total / 100.0)) ) {
				$BBD->error ("Total price isn't consistent with gallons and price-per-gallon");
				show_form();
			return undef;
		}
		
		$DBH->do (INSERT_DELIVERY, undef,
			$supplier_id,
			$from_kiosk,
			$timestamp,
			$kiosk_id,
			$gallons,
			$ppg,
			$deliv_total
		);
	
		$DBH->do (INSERT_ADD_FUEL_MESSAGE, undef,
			$kiosk_id,
			$gallons,
		);

		my $new_fuel_val = (
			($kiosks{$kiosk_id}->{fuel_avail} * $kiosks{$kiosk_id}->{fuel_value})
			+ $deliv_total) / ($kiosks{$kiosk_id}->{fuel_avail} + $gallons);

		# Add fuel to the 'TO' kiosk
		$DBH->do (ADD_KIOSK_FUEL, undef,
			$gallons,
			$new_fuel_val,
			$kiosk_id,
		);

		# Subtract fuel from the 'FROM' kiosk
		if ($from_kiosk) {
			$DBH->do (ADD_KIOSK_FUEL, undef,
				- $gallons,
				$kiosks{$from_kiosk}->{fuel_value},
				$from_kiosk,
			);
		} else {
			$k_value_total += $deliv_total;
			$k_avail_total += $gallons;
		}		
		
		$sugg_price = ($k_value_total / $k_avail_total) + PER_GALLON_MARKUP;
		if ( $sugg_price > sprintf ('%.2f',$sugg_price) ) {
			$sugg_price = sprintf ('%.2f',$sugg_price) + 0.01;
		} else {
			$sugg_price = sprintf ('%.2f',$sugg_price);
		}

		$BBD->error ("Added delivery to database. Added fuel to kiosk.");

		show_form();
		return undef;
	}

	show_form();
}



sub show_form {

	my @deliveries;
	my ($d_timestamp,$m_id,$kf_id,$kt_id,$d_quantity,$d_price_per_item,$d_total_price);
	my $sth;
	my ($kt_name,$from_name,$d_type);
	if ($fuel_czar) {
		$sth = $DBH->prepare(GET_N_LAST_FUEL_DELIVERIES) or die "Could not prepare handle";
		$sth->execute( N_FUEL_DELIVERIES );
	} else {
		$sth = $DBH->prepare(GET_N_LAST_MEMBER_FUEL_DELIVERIES) or die "Could not prepare handle";
		$sth->execute( $member_info->{memb_id}, N_FUEL_DELIVERIES );
	}
	$sth->bind_columns (\$d_timestamp,\$m_id,\$kf_id,\$kt_id,\$d_quantity,\$d_price_per_item, \$d_total_price);
	while($sth->fetch()) {
		if (!$m_id and $kf_id) {
			$from_name = $kiosk_names{$kf_id};
			$d_type = 'TRANSFER'
		} elsif ($m_id and !$kf_id) {
			$from_name = $suppliers{$m_id};
			$d_type = 'PURCHASE'
		}

		$kt_name = $kiosk_names{$kt_id};
		push (@deliveries, {
			d_timestamp => $BBD->epoch_to_ISOdatetime ($d_timestamp),
			d_supplier => $from_name,
			d_location => $kt_name,
			d_type => $d_type,
			d_gallons => sprintf ('%.2f',$d_quantity),
			d_ppg => sprintf ('$%.3f',$d_price_per_item),
			d_total => sprintf ('$%.2f',$d_total_price),
		});
	}
	

	$k_cap_total = 0.0;
	$k_avail_total = 0.0;
	$k_value_total = 0.0;
	$k_price_total = 0.0;
	my ($kiosk_id,$k_name,$k_capacity,$k_fuel_value,$k_fuel_avail,$k_fuel_price,$k_fuel_type,$k_last_checkin,$k_tz);
	$sth = $DBH->prepare(GET_KIOSK_INFO) or die "Could not prepare handle";
	$sth->execute( );
	$sth->bind_columns (\$kiosk_id,\$k_name,\$k_capacity,\$k_fuel_value,\$k_fuel_avail,\$k_fuel_price,\$k_fuel_type,\$k_last_checkin,\$k_tz);
	while($sth->fetch()) {
		$k_cap_total += $k_capacity;
		$k_avail_total += $k_fuel_avail;
		$k_value_total += ($k_fuel_value * $k_fuel_avail);
		$k_price_total += ($k_fuel_price * $k_fuel_avail);
		push (@kiosk_tmpl_loop,{
			k_name => $k_name,
			k_capacity => sprintf ('%.0f',$k_capacity),
			k_available => sprintf ('%.2f',$k_fuel_avail),
			k_value => sprintf ('$%.2f',$k_fuel_value),
			k_price => sprintf ('$%.2f',$k_fuel_price),
			k_type => $k_fuel_type,
			k_last_checkin => $BBD->epoch_to_ISOdatetime ($k_last_checkin,$k_tz),
			k_fuel_czar => $fuel_czar,
			edit_available_id => "edit_available_$kiosk_id",
			edit_type_id => "edit_type_$kiosk_id",
			edit_price_id => "edit_price_$kiosk_id",
			do_edit_id => "do_edit_$kiosk_id", # The checkbox
		});
	}


	$BBD->{TEMPLATE}->param(
		N_FUEL_DELIVERIES => N_FUEL_DELIVERIES,
		fuel_czar => $fuel_czar,
		memb_name => $member_info->{memb_name},
		fuel_deliveries => \@deliveries,
		kiosks => \@kiosk_tmpl_loop,
		k_cap_total => sprintf ('%.0f',$k_cap_total),
		k_avail_total => sprintf ('%.2f',$k_avail_total),
		k_value_total => sprintf ('$%.2f',$k_value_total),
		k_price_total => sprintf ('$%.2f',$k_price_total),
		k_value_average => sprintf ('$%.2f',$k_value_total/$k_avail_total),
		k_price_average => sprintf ('$%.2f',$k_price_total/$k_avail_total),
		kiosk_popup_delivery => $CGI->popup_menu(
			-name=>'kiosk_popup_delivery',
			# Sort by the values rather than the keys
			-values=>[sort { uc($kiosk_names{$a}) cmp uc($kiosk_names{$b}) } keys %kiosk_names],
			-labels=>\%kiosk_names,
			-class => 'popup_menu',
		),
		supplier_popup => $CGI->popup_menu(
			-name=>'supplier_popup',
			# Sort by the values rather than the keys
			-values=>\@from_list,
			-default=>$from_list[0],
			-labels=>\%from_labels,
			-class => 'popup_menu',
		),

	);
	
	$BBD->HTML_out();
}
