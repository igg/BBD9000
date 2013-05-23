#!/usr/bin/perl -w
use strict;
use warnings;

our $DOMAIN_ROOT = '/users/home/wifixed/domains/baltimorebiodiesel.org';
our $LOG_DIR = "$DOMAIN_ROOT/tmp";
our $LOGFILE = "$LOG_DIR/BBD_test.log";

# use FindBin;
# use lib $FindBin::Bin;
# 
# die "died!";
# 
# use BBD;
# my $BBD = new BBD('BBD-test.pl');
# 
# $BBD->init(\&do_request);


sub autoflush (*$) { 
# Set the state of autoflush for 
# the handle supplied as $_[0] 
# to the boolean state supplied as $_[1]
    select( 
        do{ 
            my $old = select( $_[0] ); 
            $| = $_[1]; 
            $old; 
        } 
    ) 
}





open (LOG,">> $LOGFILE");

autoflush(LOG,1);

print LOG scalar(gmtime)." Started BBD-test.pl...\n";

print "Status: 200 OK\n";
print "Content-type: text/plain\n\n";
print "Hi!\n";
print LOG scalar(gmtime)." Printed BBD-test.pl...\n";

close (LOG);
exit (1);

sub do_request {
#		$BBD->printLog (scalar(gmtime)." Request started\n");
}
