#!/usr/bin/perl -w
use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;
use BBD_test;
my $BBD = new BBD();
$BBD->require_login (0);

use constant GET_SPNS_BY_F_L_NAMES => <<"SQL";
	SELECT name,member_id,pin FROM members WHERE
SQL

use constant GET_MEMBER_CC_BY_NAME => <<"SQL";
	SELECT id FROM member_ccs WHERE
	cc_name = ?
SQL

use constant INSERT_MEMBER_CC => <<"SQL";
	INSERT INTO member_ccs
	SET
		cc_name = ?,
		mag_name = ?,
		member_id = ?
SQL


$BBD->init(\&plan_c);
exit;



sub plan_a {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';

	open (IN,'< CCNames.log');
	while ( <IN> ) {
		my $line = $_;
		chomp ($line);
		print "$line\n";
		if ($line =~ /\^([^^]+)\^/) {
			my $name = $1;
			my ($last,$first) = split (/\//,$name);
			# So much for the standard encoding.
			# 1.  Leading and/or trailing white-space occurs
			$first =~ s/^\s+// if $first; # strip white space from the beginning
			$first =~ s/\s+$// if $first; # strip white space from the end
			$last =~ s/^\s+// if $last; # strip white space from the beginning
			$last =~ s/\s+$// if $last; # strip white space from the end
			# 2.  First and last names may have trailing initials, suffixes, etc.
			my @first_pieces = split (/\s/,$first) if $first;
			my @last_pieces = split (/\s/,$last) if $last;
			$first = $first_pieces[0];
			$last = $last_pieces[0];
			# 3.  Occasionally, the whole name is encoded as the last name
			#     in "First MI Last" form.
			#     Sometimes this is followed by a first/last delimiter '/',
			#     but only sometimes.
			#     For that extra special something, the middle initial sometimes has an
			#     actual period after it!
			if (! scalar (@first_pieces)) {
				$first = $last_pieces[0];
				$last = '';
				# We only count it is a last name if its more than one letter.
				# Do double-initials ever occur?  We'll find out soon enough...
				$last = $last_pieces[1] if $last_pieces[1] and length ($last_pieces[1]) > 1;
				$last = $last_pieces[2] if not $last and $last_pieces[2] and length ($last_pieces[2]) > 1;
			}
			print "[$name] [$first] [$last]\n";
		}
		print "-----------------\n"
	
	}
	close IN;
}

sub plan_b {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	

	open (IN,'< CCNames.log');
	while ( <IN> ) {
		my $line = $_;
		chomp ($line);
		print "$line\n";
		if ($line =~ /\^([^^]+)\^/) {
			my $mag_name = $1;
			my ($last,$first,$middle) = split (/\//,$mag_name);
			$first = '' unless $first;
			$last = '' unless $last;
			$middle = '' unless $middle;
			my $CCname = "$first $middle $last";
			$CCname =~ s/^\s+//; # strip white space from the beginning
			$CCname =~ s/\s+$//; # strip white space from the end
			$CCname =~ s/\s+/ /g; # replace multiple whitespace with a single one
			my @pieces = split (/\s/,$CCname);
			my @names;
			my @SQL_F;
			my @SQL_L;
			foreach (@pieces) {
			#	if (length ($_) > 1) {
					push (@SQL_F,"first_name = '$_'");
					push (@SQL_L,"last_name = '$_'");
					push (@names,$_);
			#	}
			}
			print "[$mag_name] [$CCname] [";
			print join ('] [',@names);
			print "]\n";
			
			my $SQL = GET_SPNS_BY_F_L_NAMES.'('.
				join (' OR ',@SQL_F).') AND ('.join (' OR ',@SQL_L)
			.')';
			my $sth = $BBD->{DBH}->prepare($SQL) or die "Could not prepare handle";
			$sth->execute();
			my ($DB_name,$DB_id,$DB_SPN);
			$sth->bind_columns (\$DB_name, \$DB_id, \$DB_SPN);
			print "DB: ";
			while($sth->fetch()) {
				print "[$DB_name] ";
				my ($CC_id) = $BBD->{DBH}->selectrow_array (GET_MEMBER_CC_BY_NAME,undef,$CCname);
				if (not $CC_id) {
					$BBD->{DBH}->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,$DB_id);
				}
			}
			if (not $DB_name) {
				print "*** Member not found in DB ! *****" ;
				my ($CC_id) = $BBD->{DBH}->selectrow_array (GET_MEMBER_CC_BY_NAME,undef,$CCname);
				if (not $CC_id) {
					$BBD->{DBH}->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,undef);
				}
			}
			print "\n";
			
			
		}
		print "-----------------\n"
	
	}
	close IN;
}

# Like plan B, except we always use the first piece as the first name.
sub plan_c {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	

	open (IN,'< CCNames.log');
	while ( <IN> ) {
		my $line = $_;
		chomp ($line);
		print "$line\n";
		if ($line =~ /\^([^^]+)\^/) {
			my $mag_name = $1;
			my ($last,$first,$middle) = split (/\//,$mag_name);
			$first = '' unless $first;
			$last = '' unless $last;
			$middle = '' unless $middle;
			my $CCname = "$first $middle $last";
			$CCname =~ s/^\s+//; # strip white space from the beginning
			$CCname =~ s/\s+$//; # strip white space from the end
			$CCname =~ s/\s+/ /g; # replace multiple whitespace with a single one
			my @pieces = split (/\s/,$CCname);
			my @names;
			my @SQL_L;
			foreach (@pieces[1..$#pieces]) {
			#	if (length ($_) > 1) {
					push (@SQL_L,"last_name = '$_'");
					push (@names,$_);
			#	}
			}
			print "[$mag_name] [$CCname] ";
			print "[$pieces[0]] [".join ('] [',@names);
			print "]\n";
			
			my $SQL = GET_SPNS_BY_F_L_NAMES." first_name = '$pieces[0]' AND (".
				join (' OR ',@SQL_L).')';
			print "SQL: $SQL\n";
			my $sth = $BBD->{DBH}->prepare($SQL) or die "Could not prepare handle";
			$sth->execute();
			my ($DB_name,$DB_id,$DB_SPN);
			my $CC_id;
			$sth->bind_columns (\$DB_name, \$DB_id, \$DB_SPN);
			print "DB: ";
			
			while($sth->fetch()) {
				print "[$DB_name] ";
				($CC_id) = $BBD->{DBH}->selectrow_array (GET_MEMBER_CC_BY_NAME,undef,$CCname);
				if (not $CC_id) {
					$BBD->{DBH}->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,$DB_id);
				}
			}
			if (not $DB_name) {
				print "*** Member not found in DB ! *****" ;
				($CC_id) = $BBD->{DBH}->selectrow_array (GET_MEMBER_CC_BY_NAME,undef,$CCname);
				if (not $CC_id) {
					$BBD->{DBH}->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,undef);
				}
			}
			print "\n";
			
			
		}
		print "-----------------\n"
	
	}
	close IN;
}

# Basically plan C, 
sub plan_d {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	

	open (IN,'< CCNames.log');
	while ( <IN> ) {
		my $line = $_;
		chomp ($line);
		print "$line\n";
		if ($line =~ /\^([^^]+)\^/) {
			my $mag_name = $1;
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
				push (@names,'');
			}

			print "[$mag_name] [$CCname] ";
			print "[$pieces[0]] [".join ('] [',@names);
			print "]\n";
			
			my $SQL = GET_SPNS_BY_F_L_NAMES." first_name = '$pieces[0]' AND (".
				join (' OR ',@SQL_L).')';
			print "SQL: $SQL\n";
			my $sth = $BBD->{DBH}->prepare($SQL) or die "Could not prepare handle";
			$sth->execute();
			my ($DB_name,$DB_id,$DB_SPN);
			my $CC_id;
			$sth->bind_columns (\$DB_name, \$DB_id, \$DB_SPN);
			print "DB: ";
			
			while($sth->fetch()) {
				print "[$DB_name] ";
				($CC_id) = $BBD->{DBH}->selectrow_array (GET_MEMBER_CC_BY_NAME,undef,$CCname);
				if (not $CC_id) {
					$BBD->{DBH}->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,$DB_id);
				}
			}
			if (not $DB_name) {
				print "*** Member not found in DB ! *****" ;
				($CC_id) = $BBD->{DBH}->selectrow_array (GET_MEMBER_CC_BY_NAME,undef,$CCname);
				if (not $CC_id) {
					$BBD->{DBH}->do (INSERT_MEMBER_CC,undef,$CCname,$mag_name,undef);
				}
			}
			print "\n";
			
			
		}
		print "-----------------\n"
	
	}
	close IN;
}
