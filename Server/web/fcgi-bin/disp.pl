#!/usr/bin/perl -w
use FindBin;
use lib $FindBin::Bin;

use strict;
use warnings;
use CGI::Fast;
use EmbedPersistent;
use Sys::SigAction qw( timeout_call );

use BBD;

{
	my $timeout;
    my $p = EmbedPersistent->new();
    my $BBD = BBD->new('disp.pl');
    $BBD->printLog ("Dispatcher Started\n");

	opendir(DIR, $FindBin::Bin);
	my @files = grep(/^BBD-.*\.pl$/,readdir(DIR));
	closedir(DIR);
	foreach (@files) {
		my $filename = "$FindBin::Bin/$_";
		my $package = $p->valid_package_name($filename);
		my $mtime = -M $filename;
		eval {
			my $code = $p->prepare($filename, $package); 
			$code->compile;
			$p->cache($package, $mtime);
		};
		if ($@) {
 		   $BBD->printLog ("Errors caching $filename:\n$@");
 		   exit (-1);
		}
	}
    $BBD->printLog ("Compiled and cached perl code in $FindBin::Bin\n");
	
	opendir(DIR, $BBD::TMPL_DIR);
	@files = grep(/^[^.].*\.tmpl$/,readdir(DIR));
	closedir(DIR);
	foreach (@files) {
		$BBD->cacheTemplate ($_);
	}
    $BBD->printLog ("Compiled and cached templates in $BBD::TMPL_DIR\n");

	# pre-fetch all the keys;
	$BBD->getGWconf();
	$BBD->getServerKey();
	$BBD->getKioskKeys();
    $BBD->printLog ("Pre-fetched keys\n");

	maintenance($BBD->DBH());
	
    $BBD->printLog ("Dispatcher Initialized\n");
	my $CGI;
	while ( 1 ) {
		# random timeout of 2-5 minutes
		$timeout = 120 + int(rand(180));
		if (timeout_call( $timeout, sub {$CGI = new CGI::Fast;})) {
			maintenance($BBD->DBH());
		} else {
			last unless $CGI;
			$BBD->CGI ($CGI);
#			$BBD->printLog ("Request\n");
			my $filename = $ENV{SCRIPT_FILENAME};
			next unless $filename;
#			$BBD->printLog ("Executing $filename\n");
			my $package = $p->valid_package_name($filename);
			my $mtime;
			if ($p->cached($filename, $package, \$mtime)) {
#				$BBD->printLog ("$filename package: CACHED\n");
				eval {$package->handler;};
				if ($@) {
					$BBD->printLog ("Error Executing $filename: $@\n");
				}
			} else {
				$BBD->printLog ("$filename package: NOT CACHED\n");
				$p->eval_file($ENV{SCRIPT_FILENAME});
			}
			
			$CGI = undef;
			$BBD->CGI (undef);
		}
	}
	$BBD->printLog ("Dispatcher Exiting\n");
}

# This should do some DB access to exercise the handle
sub maintenance {
my $handle = shift;
	CGI::Session->find("driver:MySQL", sub {}, {Handle=>$handle});
}
