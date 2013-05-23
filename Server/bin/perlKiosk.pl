#!/usr/bin/perl
use lib '/fromBiggen/BBD9000/Server/OSX-lib/lib/perl5';
use strict;
use warnings;
use Crypt::OpenSSL::Random;
use Crypt::OpenSSL::Bignum;
use Crypt::OpenSSL::RSA;
use MIME::Base64;
use Scalar::Util qw(looks_like_number);
use Digest::SHA1 qw(sha1_base64);
use LWP::UserAgent;

sub encryptRSAsign {
	my $plaintext = shift;
	my $textLngth = length ($plaintext);
	my $pubKey = shift;
	my $privKey = shift;

	$pubKey->use_pkcs1_oaep_padding();
	my $keySize = $pubKey->size();
	my $chunkSize = $keySize-42;
	my $crypt_resp;
	
	# The response is encrypted in chunkSize size chunks
	my $chunk;
	my $offset=0;
	while ($offset < $textLngth) {
		$chunk = substr ($plaintext,$offset,$chunkSize);
		
		if ($chunk) {
			$pubKey->use_pkcs1_oaep_padding();
			$crypt_resp .= $pubKey->encrypt ($chunk);
		}
		$offset += $chunkSize;
	}

	$privKey->use_pkcs1_oaep_padding();
	my $signature = $privKey->sign($plaintext);
	return (encode_base64($crypt_resp),encode_base64($signature));
}

sub decryptRSA {
	my $cryptext = shift;
	my $key = shift;
	my $cryptLngth = length ($cryptext);
	my $keySize = $key->size();
	my $plaintext;
	
	# The cryptext may come in keySize size chunks
	# so we process it a chunk at a time
	my $chunk;
	my $offset=0;
	while ($offset < $cryptLngth) {
		$chunk = substr ($cryptext,$offset,$keySize);
		if ($chunk) {
			$key->use_pkcs1_oaep_padding();
			eval {$plaintext .= $key->decrypt ($chunk);};
			if ($@) {
				$key->use_pkcs1_padding();
				eval {$plaintext .= $key->decrypt ($chunk);};
			}
		}
		$offset += $keySize;
	}

	return $plaintext;
}

sub decryptRSAverify {
	my ($cryprespb64,$pubKey,$privKey) = @_;
	my $crypt_resp = decode_base64 ($cryprespb64);
	my $lngth = length ($crypt_resp);
	my $crypt_msg = substr ($crypt_resp,0,$lngth-$privKey->size());
	my $sig = substr ($crypt_resp,$lngth-$privKey->size(),$privKey->size());

	my $plaintext = decryptRSA ($crypt_msg,$privKey);
	return undef unless $plaintext and $sig and $pubKey;
	if ( $pubKey->verify( $plaintext, $sig) ) {
		print "Message contains non-graphing/non-printing characters.\n"
			if $plaintext =~ /[^[:graph:][:space:]]/;
		$plaintext =~ s/[^[:graph:][:space:]]//g;
		return $plaintext;
	} else {
		print "BOGUS message.\n";
	}
	return undef;
}

my ($kiosk_ID,$kiosk_key_file,$server_key_file,$URL) = ($ARGV[0],$ARGV[1],$ARGV[2],$ARGV[3]);
	die "try:\n".
		"$0 kiosk_id kiosk_key.pem server_key.pem URL\n".
		"e.g.:\n".
		"$0 2 BBD9000-002.pem BBD-pub.pem http://baltimorebiodiesel.org/fcgi-bin/test/BBD-kiosk.pl\n"
	unless $kiosk_ID and $kiosk_key_file and $server_key_file and $URL;

	open (PEM_FILE,"< $kiosk_key_file")
	or die "Could not open $kiosk_key_file\n";
	
	my $pem_key = '';
	while (<PEM_FILE>) {
		$pem_key .= $_;
	}
	close PEM_FILE;
	my $rsa_priv = Crypt::OpenSSL::RSA->new_private_key($pem_key);
	if ($rsa_priv) {
		$rsa_priv->check_key()
			or die "Server private key does not check out";
		$rsa_priv->use_pkcs1_oaep_padding();
		$rsa_priv->use_sha1_hash();
	} else {
		die "Could not make a private RSA key from the contents of $kiosk_key_file\n"
	}

	open (PEM_FILE,"< $server_key_file")
	or die "Could not open $server_key_file\n";
	$pem_key = '';
	while (<PEM_FILE>) {
		$pem_key .= $_;
	}
	close PEM_FILE;
	my $rsa_pub = Crypt::OpenSSL::RSA->new_public_key($pem_key);
	if ($rsa_pub) {
		$rsa_pub->use_pkcs1_oaep_padding();
	} else {
		die "Could not make a public RSA key from the contents of $server_key_file\n"
	}

	my $ua = LWP::UserAgent->new;
	$ua->timeout(30);
	$ua->agent( "perlKiosk $kiosk_ID" );
	my %serv_params;
	$serv_params{kioskID} = $kiosk_ID;
	my $res;


	($serv_params{message},$serv_params{signature}) = encryptRSAsign(time()."\n",$rsa_pub,$rsa_priv);
	$res = $ua->post($URL, \%serv_params);
	if ($res->code() == 200) {
		
		print "Server comms OK.\n";
		my $resp = decryptRSAverify ($res->content,$rsa_pub,$rsa_priv);
		if ($resp) {
			print "Server response verified.  resync response:\n$resp";
		} else {
			die "Verification failed";
		}
	}

	my ($line,$message);
	my ($crypt_message,$sig);
	while (<STDIN>) {
		$line = $_;
		chomp ($line);
		$message = $line."\n".time()."\n";
		($serv_params{message},$serv_params{signature}) =
			encryptRSAsign($message,$rsa_pub,$rsa_priv);
		$res = $ua->post($URL, \%serv_params);
		print "-----------------\n";
		if ($res->code() == 200) {
			my $resp = decryptRSAverify ($res->content,$rsa_pub,$rsa_priv);
			if ($resp) {
				print "$resp";
			} else {
				die "Verification failed";
			}
		} else {
			die "Server comms failed\n";
		}
		
	}
	
#	my $crypt_message = $CGI->param('message');
#	my $message = $BBD->decryptRSAverify($crypt_message,$CGI->param('signature'),$kioskID);
