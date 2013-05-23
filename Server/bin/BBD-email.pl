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
use Mail::Sendmail;

my ($name,$email,$subject) = ($ARGV[0],$ARGV[1],$ARGV[2]);
die <<END unless ($name and $email and $subject);
You must specify a name, email address and subject.  For example:
$0 'Mr Moo' moo\@cow.org 'Something to moo about!'
Then paste your email body into the terminal, and type control-D.
END

my $message;
while (<STDIN>) {
	chomp;
	$message .= "$_\n";
}


my %mail = ( To      => "$name <$email>",
	From    => 'Baltimore Biodiesel Coop <donotreply@baltimorebiodiesel.org>',
	Subject => $subject,
	Message => $message
);
sendmail(%mail) or die $Mail::Sendmail::error;

print "Message sent to $name <$email>\n";

