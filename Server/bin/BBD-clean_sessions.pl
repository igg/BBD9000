#!/usr/bin/perl -w
# Clean out old sessions
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
use CGI::Session;

my $BBD = new BBD('BBD-clean_sessions.pl');
$BBD->require_login (0);

$BBD->init(\&do_request);
exit;


sub do_request {
	$BBD->session_delete();
	die "Urp?" unless $BBD->{CGI}->remote_host() eq 'localhost';
	
	$BBD->cleanExpiredSessions();
}
