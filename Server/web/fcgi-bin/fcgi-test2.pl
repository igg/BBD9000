#!/usr/bin/perl
use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;

use CGI::Fast qw(:standard);


my $COUNTER = 0;
while (new CGI::Fast) {
	print header;
	print start_html("Fast CGI Rocks");
	print
		h1("Fast CGI Rocks"),
		"Invocation number ",b($COUNTER++),
		" PID ",b($$),".",
		hr;
	
	print 'Script: '.$FindBin::Bin."<br>\n";
	print hr;

	eval "use BBD;";
	if ($@) {
		print "use BBD ERROR:<br><pre>$@</pre>\n";
	} else {
		print "use BBD OK<br>\n";
	}
	print hr;
	print '<h2>@INC</h2>';
	foreach (@INC) {
		print "$_<br>\n";
	}

	print hr;
	print "<h2>%ENV</h2>";
	foreach (keys %ENV) {
		print $_.': '.$ENV{$_}."<br>\n";
	}
	print end_html;
}

