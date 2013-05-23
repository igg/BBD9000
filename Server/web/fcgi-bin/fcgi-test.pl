#!/usr/bin/perl

use FCGI; # Imports the library; required line

# Initialization code

our $cnt = 0;

# Response loop

while (FCGI::accept >= 0) {
  print "Content-type: text/html\r\n\r\n";
  print "<head>\n<title>FastCGI Demo Page (perl)</title>\n</head>\n";
  print  "<h1>FastCGI Demo Page (perl)</h1>\n";
  print "This is coming from a FastCGI server.\n<BR>\n";
  print "Running on <EM>$ENV{SERVER_NAME}</EM> to <EM>$ENV{REMOTE_HOST}</EM>\n<BR>\n";
  print "PATH: <EM>$ENV{PATH}</EM>\n<BR>\n";
   $cnt++;
  print "This is connection number $cnt\n";
}
