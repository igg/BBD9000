#!/usr/bin/perl

package BBD;
our ($HTML_DIR,$TMPL_DIR,$LOGFILE,$TRACEFILE,$REQUEST_TIME_LOG,$DB_CONF,$GW_CONF_FILE,$LOGOUT_REDIRECT);
our ($LOG_DIR,$TMP_DIR,$PRIV_DIR,$TIMEZONE);
our $URL_BASE;
############################################
# Host-specific info
use constant DOMAIN_ROOT_CNST => '/home/ubuntu/bbd9000.com/piedmontbiofuels.bbd9000.com/';
use constant COOP_DOMAIN_CNST => 'piedmontbiofuels.bbd9000.com';
use constant COOP_NAME_CNST => 'Piedmont Biofuels';

# Use INSTALL_BASE=DOMAIN_ROOT_CNST for perl Makefile.PL
#use lib DOMAIN_ROOT_CNST."/lib/perl5/";
#use lib DOMAIN_ROOT_CNST."/perl5lib/lib/perl5";

use constant TEST_DB => 0;
# The coop's default timezone - each kiosk still has their own timezone.
$TIMEZONE = 'US/Eastern';

# END Host-specific info
############################################
# We're telling kiosks that we'll act as the CC gateway
use constant SERVER_GW => 1;


our $DOMAIN_ROOT = DOMAIN_ROOT_CNST;
our $COOP_DOMAIN = COOP_DOMAIN_CNST;
our $COOP_NAME   = COOP_NAME_CNST;
$LOG_DIR = "$DOMAIN_ROOT/tmp";
$TMP_DIR = "$DOMAIN_ROOT/tmp";
$PRIV_DIR = "$DOMAIN_ROOT/priv";

###
# Things that are different b/w test and live servers
if (TEST_DB) {
	$URL_BASE = "http://$COOP_DOMAIN/fcgi-bin/test";
	$HTML_DIR = "$DOMAIN_ROOT/web/public/test_site";
	$TMPL_DIR = "$DOMAIN_ROOT/tmpl/test";
	$LOGFILE = "$LOG_DIR/BBD_test.log";
	$TRACEFILE = "$LOG_DIR/DB_trace_test.log";
	$REQUEST_TIME_LOG = "$LOG_DIR/req_time_test.log";
	$DB_CONF = "$PRIV_DIR/DB_CONF_test.cnf";
	$GW_CONF_FILE = "$PRIV_DIR/GW_test.conf";
	$LOGOUT_REDIRECT = '/test_site';
} else {
	$URL_BASE = "http://$COOP_DOMAIN/fcgi-bin";
	$HTML_DIR = "$DOMAIN_ROOT/web/public";
	$TMPL_DIR = "$DOMAIN_ROOT/tmpl";
	$LOGFILE = "$LOG_DIR/BBD.log";
	$TRACEFILE = "$LOG_DIR/DB_trace.log";
	$REQUEST_TIME_LOG = "$LOG_DIR/req_time.log";
	$DB_CONF = "$PRIV_DIR/DB_CONF.cnf";
	$GW_CONF_FILE = "$PRIV_DIR/GW.conf";
	$LOGOUT_REDIRECT = '/';
}

our $VERSION = '2.2';


use strict;
use warnings;
#use CGI::Carp qw(fatalsToBrowser);
use CGI::Carp;
use CGI::Fast qw(:standard);

use IO::Handle;

# website information



our $KEYFILE = "$PRIV_DIR/BBD.pem";
our $KIOSK_STATUS_FILE = "$HTML_DIR/kiosks/[kiosk-name]-status.php";
our $FUEL_TYPE_FILE = "$HTML_DIR/kiosks/[kiosk-name]-type.php";
our $FUEL_PRICE_FILE = "$HTML_DIR/kiosks/[kiosk-name]-price.php";
our $FUEL_QUANTITY_FILE = "$HTML_DIR/kiosks/[kiosk-name]-quantity.php";
our $CARBON_TONS_FILE = "$HTML_DIR/carbon-tons.php";

=pod

# DB_CONF.cnf

[client]
host     = localhost
database = my_db
user     = my_usr
password = my_pwd

=cut


our $LOGIN_PAGE = 'BBD-login.pl';
our $FATAL_TEMPLATE = 'BBD-fatal.tmpl';
our $CONF_EMAIL_TEMPLATE = 'BBD-conf_email.tmpl';
our $WELCOME_EMAIL_TEMPLATE = 'BBD-welcome_email.tmpl';
our $LOGIN_RESET_EMAIL_TEMPLATE = 'BBD-login_reset_email.tmpl';
# This is in days:
our $LOGIN_RESET_LIFETIME = 14;
# This is in seconds:
our $CRYPT_LIFETIME = 60;
# before and after grace periods in days
our $GRACE_PERIOD_BEFORE_DAYS = 90;
our $GRACE_PERIOD_AFTER_DAYS = 90;
our $ASK_RENEW_START_DAYS = 30;


# Debugging and DB tracing
our $debugging = 1;
# Set to 5 for really verbose tracing
# Set to 0 to turn off
our $db_trace = 0;

use constant REQUEST_TIME_LOGGING => 1;


if ($debugging) {
	open (LOG,">> $LOGFILE") or die "Couldn't open logfile: $LOGFILE: $@\n";
	LOG->autoflush(1);
# 
# 	my $ofh = select LOG;
# 	$| = 1;
# 	select $ofh;
# 	open (STDERR,">> $LOGFILE");
}




use DBI qw(:sql_types);
use CGI::Session;
use CGI::Cookie;
use HTML::Template;
use DateTime::Format::Strptime;
use DateTime;
use Mail::Sendmail;
use Crypt::OpenSSL::Random;
use Crypt::OpenSSL::Bignum;
use Crypt::OpenSSL::RSA;
use MIME::Base64;
use Scalar::Util qw(looks_like_number);
use Digest::SHA qw(sha1_base64);
use Time::HiRes;


use constant GET_USER_INFO_BY_ID => <<"SQL";
	SELECT m.name,m.first_name,m.last_name,m.pin,m.member_id,ms.type,ms.status,m.credit,ms.membership_id,
		ms.membership_number,UNIX_TIMESTAMP(ms.start_date),UNIX_TIMESTAMP(ms.expires),UNIX_TIMESTAMP(ms.last_renewal),
		m.fuel_preauth,m.email,m.address1,m.address2,m.city,m.state,m.zip,
		m.home_phone,m.work_phone,m.mobile_phone,
		m.login,m.password,m.is_primary
		FROM members m, memberships ms
		WHERE m.membership_id = ms.membership_id
			AND m.member_id = ?
SQL


##
# Queries for member login
##
use constant GET_MEMBER_PASS_BY_LOGIN => <<"SQL";
	SELECT members.member_id, members.password, members.is_primary, memberships.type
	FROM members, memberships
	WHERE memberships.membership_id = members.membership_id
	AND members.login = ?
SQL
use constant GET_MEMBER_ROLES => <<"SQL";
	SELECT roles.role_id, roles.label, roles.script FROM roles,member_roles
	WHERE roles.role_id = member_roles.role_id
	AND member_roles.member_id= ?
	ORDER BY roles.order ASC
SQL

##
# Queries for modifying roles
##
use constant REMOVE_MEMBER_ROLE => <<"SQL";
	DELETE FROM member_roles WHERE member_id = ?
	AND role_id = ?
SQL
use constant ADD_MEMBER_ROLE => <<"SQL";
	INSERT INTO member_roles SET
	member_id = ?,
	role_id = ?
SQL
use constant GET_ALL_ROLES => <<"SQL";
	SELECT role_id FROM roles
	ORDER BY roles.order ASC
SQL
use constant GET_ALL_ROLES_DESC => <<"SQL";
	SELECT r.role_id, r.label, r.script, r.description,
		(SELECT GROUP_CONCAT(DISTINCT m.name SEPARATOR ',')
			FROM members m, member_roles mr WHERE m.member_id = mr.member_id AND mr.role_id = r.role_id
		) AS members
	FROM roles r
	GROUP BY r.role_id
	ORDER BY r.order ASC
SQL


##
# Queries for member deletion
##
use constant CHECK_MEMBER_SALES => <<"SQL";
	SELECT member_id FROM sales WHERE member_id = ?
	LIMIT 1
SQL
use constant CHECK_MEMBER_TRANS => <<"SQL";
	SELECT member_id FROM cc_transactions WHERE member_id = ?
	LIMIT 1
SQL
use constant CHECK_MEMBER_PRIMARY_CREDIT => <<"SQL";
	SELECT credit,is_primary FROM members WHERE member_id = ?
	LIMIT 1
SQL

use constant DELETE_MEMBER_ROLES => <<"SQL";
	DELETE FROM member_roles WHERE member_id = ?
SQL

use constant DELETE_MEMBER => <<"SQL";
	DELETE FROM members WHERE member_id = ?
SQL

##
# Queries to make new memberships
##
use constant GET_MEMBERSHIP_TYPES => <<"SQL";
	SHOW COLUMNS FROM memberships LIKE "type"
SQL

use constant GET_MEMBERSHIP_STATUSES => <<"SQL";
	SHOW COLUMNS FROM memberships LIKE "status"
SQL

use constant LOCK_MEMBERSHIPS_TABLE => <<"SQL";
	LOCK TABLES memberships WRITE
SQL

use constant UNLOCK_MEMBERSHIPS_TABLE => <<"SQL";
	UNLOCK TABLES;
SQL

use constant GET_NEW_MEMBERSHIP_NUMBER => <<"SQL";
	SELECT MAX(membership_number)+1 from memberships;
SQL

use constant INSERT_NEW_MEMBERSHIP => <<"SQL";
	INSERT INTO memberships SET
		membership_number = ?,
		start_date = FROM_UNIXTIME(?),
		last_renewal = FROM_UNIXTIME(?),
		expires = FROM_UNIXTIME(?),
		type = ?,
		status = ?
SQL

use constant GET_MEMBERSHIP_ID => <<"SQL";
	SELECT LAST_INSERT_ID()
SQL

use constant INSERT_NEW_MEMBER => <<"SQL";
	INSERT INTO members SET
		name = ?,
		first_name = ?,
		last_name = ?,
		email = ?,
		membership_id = ?,
		is_primary = 1,
		fuel_preauth = ?,
		pin = ENCRYPT(?)
SQL

use constant GET_NEW_MEMBER_ID => <<"SQL";
	SELECT LAST_INSERT_ID()
SQL


use constant DEFAULT_SPN => "8675309";
use constant DEFAULT_FUEL_PREAUTH => "20";

##
# Query to check for unique (first_name,last_name,SPN)
##
use constant GET_SPNS_BY_F_L_NAMES => <<"SQL";
	SELECT member_id,pin FROM members WHERE first_name = ? AND last_name = ?
SQL

##
# Query to memb_id from name
##
use constant GET_MEMB_ID_BY_NAME => <<"SQL";
	SELECT member_id FROM members WHERE name = ?
SQL

##
# Queries to get membership fees
##
use constant GET_RENEWAL_PRICE => <<"SQL";
	SELECT price FROM item_prices WHERE item = "renewal"
SQL

use constant GET_MEMBERSHIP_PRICE => <<"SQL";
	SELECT price FROM item_prices WHERE item = "membership"
SQL

use constant GET_TRIAL_SURCHARGE_PRICE => <<"SQL";
	SELECT price FROM item_prices WHERE item = "trial_surcharge"
SQL

use constant GET_MEMBERSHIP_PROMO_PRICE => <<"SQL";
	SELECT price FROM promotions
	WHERE
		item = "membership"
		AND (number_left > 0 OR number_left IS NULL)
		AND (kiosk_id = ? OR kiosk_id IS NULL)
		AND ( (now() > date_start AND now() < date_end) OR (date_start IS NULL AND date_end IS NULL) )
SQL

use constant UPDATE_MEMBERSHIP_PROMOS => <<"SQL";
	UPDATE promotions
		SET number_left = number_left - 1
		WHERE
			item = "membership"
			AND number_left > 0
			AND kiosk_id <=> ?
SQL

# Upgrade from a non-ONE-DAY membership to a FULL membership.
use constant GET_UPGRADE_PRICE => <<"SQL";
	SELECT price FROM item_prices WHERE item = "upgrade"
SQL

use constant GET_FUEL_PRICE => <<"SQL";
	SELECT fuel_price FROM kiosks WHERE kiosk_id = ?
SQL

use constant GET_KIOSK_NAME => <<"SQL";
	SELECT name FROM kiosks WHERE kiosk_id = ?
SQL

use constant GET_KIOSKS => <<"SQL";
	SELECT kiosk_id FROM kiosks
SQL


use constant DELETE_FUEL_MESSAGES => <<"SQL";
	DELETE FROM messages
	WHERE kiosk_id = ?
	AND (parameter = "fuel" OR parameter = "add_gallons")
SQL

use constant INSERT_FUEL_MESSAGE => <<"SQL";
	INSERT INTO messages SET
		kiosk_id = ?,
		parameter = "fuel",
		value = ?
SQL

use constant GET_KIOSK_FUEL => <<"SQL";
	SELECT fuel_avail, fuel_type, fuel_price
	FROM kiosks
	WHERE kiosk_id = ?
SQL

use constant UPDATE_KIOSK_FUEL => <<"SQL";
	UPDATE kiosks SET
		fuel_avail = ?,
		fuel_type = ?,
		fuel_price = ?
	WHERE kiosk_id = ?
SQL

use constant GET_TOTAL_BIODIESEL => <<"SQL";
	SELECT SUM(quantity*SUBSTRING(item,2)/100)
	FROM sales WHERE item LIKE "B%"
SQL

##
# Query to get the kiosk RSA key
##
use constant GET_KIOSK_KEY => <<"SQL";
	SELECT rsa_key
	FROM kiosks
	WHERE kiosk_id = ?
SQL


our $DT_ISO_DATETIME_FORMAT = new DateTime::Format::Strptime(pattern => '%F %R');
our $DT_ISO_DATETIME_SEC_FORMAT = new DateTime::Format::Strptime(pattern => '%F %H:%M:%S');
our $DT_DATETIME_FORMAT = new DateTime::Format::Strptime(pattern => '%D %R');
our $DT_ISO_DATE_FORMAT = new DateTime::Format::Strptime(pattern => '%F');
our $DT_DATE_FORMAT = new DateTime::Format::Strptime(pattern => '%D');

# Cached objects
our $DBH_GLOB;
our $CGI_GLOB;
our $CGI_ONESHOT_GLOB;
our %CACHED_TEMPLATES;
our $SERVER_KEY;
our %KIOSK_KEYS;
our $REAL_GW_CONF;

sub new {
	my $proto = shift;
	my $invoker = shift;
	if (not $invoker) {
		my $filename = ( caller(1) )[1];
		$filename = '' unless $filename;
		my $subroutine = ( caller(1) )[3];
		$subroutine = '' unless $subroutine;
		$invoker = $filename.':'.$subroutine;
	}
    my $class = ref($proto) || $proto;
    my $self = {
		REQUIRE_LOGIN   => 1,
		FUEL_PRICE_FILE => $FUEL_PRICE_FILE,
		REQUEST_START   => Time::HiRes::time,
		INVOKED_BY      => $invoker,
	};

	$self = bless($self, $class);
	$self->DBH();

    return $self;
}

sub reqTime {
	my $self = shift;
	return Time::HiRes::time - $self->{REQUEST_START};
}

sub printLog {
	my $self = shift;
	print LOG $self->epoch_to_ISOdatetimeSecs(time()).":$$ @_" if $debugging;
}

sub require_login {
	my $self = shift;
	if (scalar @_) {
		my $param = shift;
		if ($param) {
			$self->{REQUIRE_LOGIN} = 1;
		} else {
			$self->{REQUIRE_LOGIN} = 0;
		}
	}
	return $self->{REQUIRE_LOGIN};
}

sub init {
	my $self = shift;
	my $callback = shift or die "No callback";

	# The CGI::Fast call could have been made elsewhere, in which case, we already have
	# the object stored.  Otherwise, we run the request loop from in here
#####
# The whole thing hangs up at this point until a request comes in
	while ( my $CGI = $self->CGI() ) {
		$self->printLog (" Request started\n");
		my $DBH = $self->{DBH};
	####
	# Clear out instance stuff we may have set.
		$self->{ERROR} = undef;
		$self->{SESSION} = undef;
		$self->{CGISESSID} = undef;
		$self->{SESSION_IS_NEW} = undef;
		$self->{SESSION_WAS_EXPIRED} = undef;
	
		# Keep only the program name.
		my ($full_url,$script) = ($CGI->url(),$CGI->url(-relative => 1));
		$self->{SCRIPT} = $script;
		$self->{URL} = $self->{URL_BASE} = $full_url;
#		$self->{URL_BASE} =~ s/\/?$script//;
		$self->{URL_BASE} = $URL_BASE;
		
		my $sid = $CGI->url_param('CGISESSID') || $CGI->param ('CGISESSID') || $CGI->cookie('CGISESSID');
		$sid = $self->safeCGIstring ($sid);
		my $session = $self->getSession ($sid);

	####
	# Templates
		if ($self->{TEMPLATE_FILE}) {
	# WARNING: no way to back-out of an -associate parameter if the object is later undef'ed
	# So, every parameter needs to be sent to the template explicitly.
			$self->{TEMPLATE} = $self->cacheTemplate ($self->{TEMPLATE_FILE});
		}

		if ($self->{REQUIRE_LOGIN}) {
			if (not $session->param ('logged_in') ) {
				$self->relogin();
				next;
			}
		}
	
		if ( $session->param ('logged_in') and not $session->param ('member_id') ) {
			$self->relogin();
			next;
		}
		
		my $roles = $session->param ('roles');
		if ($self->{MY_ROLE}) {
			if (! $self->has_role ($self->{MY_ROLE})) {
				$session->param ('redirect',$session->param('default_uri'));
				$self->doRedirect();
				next;
			}
		}
		
		$session->clear ('edit_member_id') if !$self->{MY_ROLE} or $self->{MY_ROLE} ne 'Member Info';
	
	
	
		if ($self->{TEMPLATE} and $self->{MY_ROLE}) {
			my @role_loop;
			foreach my $role (@$roles) {
				if ($role->{role} eq $self->{MY_ROLE}) {
					push (@role_loop, { label => $role->{label} });
				} elsif ($role->{script}) {
					push (@role_loop, { label => $role->{label}, script => $role->{script}});
				}
			}
			$self->{TEMPLATE}->param (role_loop => \@role_loop);
		}
		$self->{TEMPLATE}->param (logged_in => 1) if $session->param ('logged_in') and $self->{TEMPLATE};

	
		$self->printLog (" Calling callback: $callback\n");
	####
	# Call the call-back to process the request
		eval {
			$callback->();
		};
		
		if ($@) {
			$DBH->rollback();
			$self->printLog (" Transaction aborted: $@\n");
		} else {
			$DBH->commit();
			$self->printLog (" Request finished\n");
		}
		$self->CGI(undef);
		$self->{TEMPLATE}->clear_params() if $self->{TEMPLATE};
		$self->session_close();
		if ( REQUEST_TIME_LOGGING and open (REQUEST_TIME_LOG_FH,">> $REQUEST_TIME_LOG") ) {
			print REQUEST_TIME_LOG_FH join ("\t",$self->{INVOKED_BY},Time::HiRes::time,$self->reqTime)."\n";
			close (REQUEST_TIME_LOG_FH);
		}

	}

	return 1;
}

sub getSession {
my $self = shift;
my $sid = shift;
		
	my $session = CGI::Session->load("driver:MySQL", $sid, {Handle=>$self->{DBH}});
	if ($session->is_expired()) {
		$session->delete();
		$self->{SESSION_WAS_EXPIRED} = 1;
		$session = CGI::Session->new("driver:MySQL", 'crap', {Handle=>$self->{DBH}});
	} elsif ($session->is_empty()) {
		$self->{SESSION_IS_NEW} = 1;
		$session = CGI::Session->new("driver:MySQL", 'crap', {Handle=>$self->{DBH}});
	}

	$self->{SESSION} = $session;
	$self->{CGISESSID} = $session->id();
	$session->expire('+30m');
	return ($session);
}

sub cleanExpiredSessions {
my $self = shift;

	CGI::Session->find("driver:MySQL", sub {}, {Handle=>$self->{DBH}});
	return 1;
}

sub login {
	my $self = shift;
	my ($login,$password) = @_;
	return undef unless $login and $password;

	my ($member_id,$DB_password,$is_primary,$type) = $self->{DBH}->selectrow_array (GET_MEMBER_PASS_BY_LOGIN, undef, $login);
	return undef unless $member_id and $DB_password;

	# Check password
	return undef unless crypt($password, $DB_password) eq $DB_password;
	
	# Make a new session if we don't have one.
	$self->{SESSION} = CGI::Session->new("driver:MySQL", 'crap', {Handle=>$self->{DBH} })
		unless $self->{SESSION};


	# Set session stuff if successful
	$self->{CGISESSID} = $self->{SESSION}->id();
	$self->{SESSION}->param ('logged_in',1);
	$self->{SESSION}->param ('member_id',$member_id);
	$self->{SESSION}->expire('+30m');

	# Populate member roles
	# Default roles
	my @roles;
	my $default_uri;
	if ($type ne 'SUPPLIER') {
		push (@roles,{role => 'Purchases', script => 'BBD-purchases.pl'});
		push (@roles,{role => 'Family Purchases'}) if ($is_primary);
		push (@roles,{role => 'Member Info', script => 'BBD-member.pl'});
		push (@roles,{role => 'Family Memberships', script => 'BBD-family_memberships.pl'}) if ($is_primary);
		$self->{SESSION}->param ('default_uri','BBD-purchases.pl');
	} else {
		push (@roles,{role => 'Fuel Deliveries', script => 'BBD-fuel_deliveries.pl'});
		$self->{SESSION}->param ('default_uri','BBD-fuel_deliveries.pl');
	}
	# Additional roles in the DB.
	my $sth = $self->{DBH}->prepare(GET_MEMBER_ROLES) or die "Could not prepare handle";
	$sth->execute( $member_id );
	my ($role,$label,$script);
	$sth->bind_columns (\$role,\$label,\$script);
	while($sth->fetch()) {
		push (@roles,{role => $role, label => $label, script => $script});
	}
	# Make sure the roles have labels.
	foreach (@roles) {
		$_->{label} = $_->{role} unless $_->{label};
	}

	$self->{SESSION}->param ('roles',\@roles);


	$self->{SESSION}->flush();

	return 1;
}

sub myURL {
	return (shift->{SCRIPT});
}

sub logout {
	my $self = shift;

	my $cookie = new CGI::Cookie(-name    =>  'CGISESSID',
					 -value   =>  $self->{CGISESSID},
					 -expires =>  '-10m',
					 -path    =>  '/',
					);

	$self->{SESSION}->delete();
	undef ($self->{SESSION});
	undef ($self->{CGISESSID});
	print $self->{CGI}->redirect(-cookie => $cookie, -uri => $LOGOUT_REDIRECT);
}

sub get_member_info {
	my $self = shift;

	my ($memb_name,$memb_fname,$memb_lname,
		$pin,$memb_id,$type,$status,$credit,
		$ms_id,$ms_num,$start,$expires,$renew,
		$preAuth,$email,$ad1,$ad2,$city,$state,$zip,
		$h_ph,$w_ph,$m_ph,
		$login,$password,$is_primary) =
			$self->{DBH}->selectrow_array (GET_USER_INFO_BY_ID,undef,shift);

	return {
		memb_name => $memb_name,
		memb_fname => $memb_fname,
		memb_lname => $memb_lname,
		pin => $pin,
		memb_id => $memb_id,
		type => $type,
		status => $status,
		credit => $credit,
		ms_id => $ms_id,
		ms_num => $ms_num,
		start => $start,
		expires => $expires,
		renew => $renew,
		fuel_preauth => $preAuth,
		email => $email,
		ad1 => $ad1,
		ad2 => $ad2,
		city => $city,
		state => $state,
		zip => $zip,
		h_ph => $h_ph,
		w_ph => $w_ph,
		m_ph => $m_ph,
		login => $login,
		password => $password,
		is_primary => $is_primary,
	}
}

sub myRole {
	my $self = shift;
	$self->{MY_ROLE} = shift;
}

sub has_role {
	my $self = shift;
	my $role = shift;
	return undef unless $self->{SESSION};
	foreach (@{$self->{SESSION}->param('roles')}) {
		return 1 if $_->{role} eq $role;
	}
	return undef;
}

sub get_all_roles {
	my $self = shift;
	my @roles;
	my $sth = $self->{DBH}->prepare(GET_ALL_ROLES) or die "Could not prepare handle";
	$sth->execute();
	my ($role);
	$sth->bind_columns (\$role);
	while($sth->fetch()) {
		push (@roles,$role);
	}
	return @roles;
}

sub get_member_roles {
	my $self = shift;
	my $member_id = shift;
	my @roles;

	my $sth = $self->{DBH}->prepare(GET_MEMBER_ROLES) or die "Could not prepare handle";
	$sth->execute( $member_id );
	my ($role,$label,$script);
	$sth->bind_columns (\$role,\$label,\$script);

	while($sth->fetch()) {
		push (@roles,$role);
	}
	return @roles;
}

sub get_all_roles_hashref {
	my $self = shift;
	my %roles;

	my $sth = $self->{DBH}->prepare(GET_ALL_ROLES_DESC) or die "Could not prepare handle";
	$sth->execute();
	my ($role,$label,$script,$description,$members);
	$sth->bind_columns (\$role,\$label,\$script,\$description,\$members);

	while($sth->fetch()) {
		$roles{$role} = {
			role => $role, label => $label, script => $script, description => $description, members => [split(',',$members)]
		};
	}
	return \%roles;
}

sub add_role {
	my $self = shift;
	my ($member, $role) = @_;
	$self->{DBH}->do (ADD_MEMBER_ROLE,undef,$member,$role);
}

sub remove_role {
	my $self = shift;
	my ($member, $role) = @_;
	$self->{DBH}->do (REMOVE_MEMBER_ROLE,undef,$member,$role);
}

sub myTemplate {
	my $self = shift;
	$self->{TEMPLATE_FILE} = shift;
}

sub cacheTemplate {
	my $self = shift;
	my $templateFile = shift;
	my $template;
	# WARNING: no way to back-out of an -associate parameter if the object is later undef'ed
	# So, every parameter needs to be sent to the template explicitly.
	if (exists $CACHED_TEMPLATES{$templateFile}) {
		$template = $CACHED_TEMPLATES{$templateFile};
		$template->clear_params() if $template;
	} else {
		$template = HTML::Template->new(filename => "$TMPL_DIR/".$templateFile)
			or die "Could not load template";
		$CACHED_TEMPLATES{$templateFile} = $template;
	}
	
	return ($template);
}

sub CGI {
	my $self = shift;

	if (scalar (@_)) {
		$self->{CGI} = $CGI_GLOB = shift;
#$self->printLog ("Setting CGI object\n") if ($CGI_GLOB);
#$self->printLog ("Unsetting CGI object\n") unless ($CGI_GLOB);
		$CGI_ONESHOT_GLOB = 1 if ($CGI_GLOB);
	} else {
		if ($CGI_ONESHOT_GLOB and not $CGI_GLOB) {
#$self->printLog ("Returning undef CGI object\n");
			$self->{CGI} = $CGI_ONESHOT_GLOB = undef;
		} elsif (not $CGI_ONESHOT_GLOB  and not $CGI_GLOB) {
#$self->printLog ("Blocking for a CGI::Fast object\n");
			$self->{CGI} = new CGI::Fast;
		} elsif ($CGI_GLOB) {
#$self->printLog ("Returning cached CGI object\n");
			$self->{CGI} = $CGI_GLOB;
		}
	}

	return $self->{CGI};
}

sub DBH {
	my $self = shift;
#####
# Initialize things that will be persistent across many requests
	if ( $DBH_GLOB ) {
		my $try;
		eval {$try = $DBH_GLOB->ping();};
		if ($@ or not $try) {
$self->printLog ("Stale DBH handle: $@\n");
			undef ($DBH_GLOB);
		}
	}
	
	# set up database connection
	if (! $DBH_GLOB ) {
#$self->printLog ("Making new DBH handle\n");
		my $DSN =
		  "DBI:mysql:;" . 
		  "mysql_read_default_file=$DB_CONF";
		my $DBH = DBI->connect($DSN,undef,undef,{RaiseError => 1, AutoCommit => 0})
			or  die "DBI::errstr: $DBI::errstr";
		$DBH->trace($db_trace, $LOGFILE) if ($db_trace);
		$self->{DBH} = $DBH_GLOB = $DBH;
	} else {
#$self->printLog ("Reusing DBH handle\n");
		$self->{DBH} = $DBH_GLOB;
	}

	return $self->{DBH};
}


sub getMemberURL {
	my $self = shift;
	return ($self->{URL_BASE}."/BBD-login.pl");
}


sub HTML_out {
	my $self = shift;

	$self->{TEMPLATE}->param(
		ERROR => $self->{ERROR},
	);
	
	if ($self->{SESSION}) {
		$self->{TEMPLATE}->param(CGISESSID => $self->{CGISESSID});
		my $cookie = new CGI::Cookie(-name    =>  'CGISESSID',
						 -value   =>  $self->{CGISESSID},
						 -expires =>  '+30m',
						 -path    =>  '/',
						);
	
		print $self->{CGI}->header(-cookie => $cookie);
		$self->session_close();
	} else {
		print $self->{CGI}->header() if $self->{CGI};
	}

    print $self->{TEMPLATE}->output() if $self->{TEMPLATE};

}

sub relogin {
	my $self = shift;
	$self->{SESSION}->param('redirect',$self->{SCRIPT}) if $self->{SESSION};
	$self->doRedirect ($LOGIN_PAGE);
}

sub doRedirect {
	my $self = shift;
	my $uri = shift;
	
	if ($self->{SESSION}) {
		$uri = $self->{SESSION}->param('redirect') || $self->{SESSION}->param('default_uri')
			unless $uri;
		
		my $cookie = new CGI::Cookie(-name    =>  'CGISESSID',
						 -value   =>  $self->{CGISESSID},
						 -expires =>  '+30m',
						 -path    =>  '/',
						);
		$self->session_close();
		print $self->{CGI}->redirect(-cookie => $cookie, -uri => "$uri?CGISESSID=".$self->{CGISESSID});
	} else {
		print $self->{CGI}->redirect(-uri => $uri);
	}
	$self->CGI(undef);
	$self->{TEMPLATE}->clear_params() if $self->{TEMPLATE};

}


sub delete_member {
	my $self = shift;

	my $DBH = $self->{DBH};
	
	my $member = shift;
	my ($test_id) = $DBH->selectrow_array (CHECK_MEMBER_SALES,undef,$member);
	if ($test_id) {
		$self->{ERROR} = "Member could not be deleted because of sales in the DB";
		return undef;
	}
	($test_id) = $DBH->selectrow_array (CHECK_MEMBER_TRANS,undef,$member);
	if ($test_id) {
		$self->{ERROR}  = "Member could not be deleted because of CC transactions in the DB";
		return undef;
	}

	my ($credit,$is_primary) = $DBH->selectrow_array (CHECK_MEMBER_PRIMARY_CREDIT,undef,$member);
	if ($credit) {
		$self->{ERROR}  = "Member could not be deleted because of account credit";
		return undef;
	} elsif ($is_primary) {
		$self->{ERROR}  = "Member could not be deleted because of primary membership";
		return undef;
	}

	$DBH->do (DELETE_MEMBER_ROLES,undef,$member);
	$DBH->do (DELETE_MEMBER,undef,$member);
	
	return 1;
}

sub get_membership_types {
my $self = shift;
	return $self->{membership_types} if $self->{membership_types};

	my $DBH = $self->{DBH};
	my $FetchHashKeyName = $DBH->{FetchHashKeyName};
	$DBH->{FetchHashKeyName} = "NAME_lc";
	my $x = $DBH->selectrow_hashref(GET_MEMBERSHIP_TYPES);
	$x->{type} =~ s/^enum\('//;
	$x->{type} =~ s/'\)$//;
	my @fields = split /','/, $x->{type};
	$self->{membership_types}{$_} = $_ foreach @fields;
	$DBH->{FetchHashKeyName} = $FetchHashKeyName;
	return %{$self->{membership_types}};
}

sub get_membership_statuses {
my $self = shift;
	return $self->{membership_statuses} if $self->{membership_statuses};

	my $DBH = $self->{DBH};
	my $FetchHashKeyName = $DBH->{FetchHashKeyName};
	$DBH->{FetchHashKeyName} = "NAME_lc";
	my $x = $DBH->selectrow_hashref(GET_MEMBERSHIP_STATUSES);
	$x->{type} =~ s/^enum\('//;
	$x->{type} =~ s/'\)$//;
	my @fields = split /','/, $x->{type};
	$self->{membership_statuses}{$_} = $_ foreach @fields;
	$DBH->{FetchHashKeyName} = $FetchHashKeyName;
	return %{$self->{membership_statuses}};
}

sub new_membership {
my $self = shift;
my ($new_start,$new_type,$new_status,$new_name,$new_email,$new_spn,$fuel_preauth,$price,$kiosk_id) = @_;

	my $DBH = $self->{DBH};

	$new_spn = DEFAULT_SPN unless $new_spn;
	$fuel_preauth = DEFAULT_FUEL_PREAUTH unless $fuel_preauth;
	my $first_name = $1 if $new_name =~ /^(\S+)/;
	my $last_name = $1 if $new_name =~ /(\S+)$/;
	
	# Get an available membership_number - need to get a write lock on the memberships table.
	$DBH->do (LOCK_MEMBERSHIPS_TABLE);
	my $new_membership_number = $DBH->selectrow_array (GET_NEW_MEMBERSHIP_NUMBER);
	$new_membership_number = 1 unless $new_membership_number;
	$DBH->do (INSERT_NEW_MEMBERSHIP,undef,
		$new_membership_number,
		$new_start,
		$new_start,
		DateTime->from_epoch( epoch => $new_start, time_zone => $TIMEZONE)->add(years=>1)->epoch(),
		$new_type,
		$new_status
	);
	my $new_membership_id = $DBH->selectrow_array (GET_MEMBERSHIP_ID);
	$DBH->do (UNLOCK_MEMBERSHIPS_TABLE);
	$DBH->do (INSERT_NEW_MEMBER,undef,
		$new_name,
		$first_name,
		$last_name,
		$new_email,
		$new_membership_id,
		$fuel_preauth,
		$new_spn,
	);
	my $new_member_id = $DBH->selectrow_array (GET_NEW_MEMBER_ID);

	# update the number of promos left if we paid less than full price
	$self->updateMembershipPromos($price,$kiosk_id);

	return ($new_member_id);
}

sub updateMembershipPromos {
my $self = shift;
my $price = shift;
my $kiosk_id = shift;

	my ($full_price) = $self->{DBH}->selectrow_array (GET_MEMBERSHIP_PRICE);
	$self->{DBH}->do(UPDATE_MEMBERSHIP_PROMOS, undef, $kiosk_id) if ($price and $price < $full_price);
}

sub getDefaultFuelPreauth {
	return DEFAULT_FUEL_PREAUTH;
}

sub getFuelPrice {
my $self = shift;
	my $type = shift;
	my $kiosk_id = shift;

	my ($fuel_price) = $self->{DBH}->selectrow_array (GET_FUEL_PRICE, undef, $kiosk_id);
	if (uc($type) ne 'FULL') {
		$fuel_price += $self->getTrialSurcharge();
	}
	return ($fuel_price);
}

# This is for updating the kiosk based on parameters and/or DB entries
# The kiosk is sent a message with the new fuel settings.
# If all of the parameters are undef, then the call will have no effect other than return settings in the DB
# This is not to be used for processing fuel messages FROM the kiosk - it is for updating the kiosk FROM the DB!
sub updateKioskFuel {
my $self = shift;
my $kiosk_id = shift;
my %params = @_;

	return unless defined $kiosk_id;

	my ($DB_avail,$DB_type,$DB_price) =
		$self->{DBH}->selectrow_array (GET_KIOSK_FUEL, undef, $kiosk_id);

	my ($avail,$type,$price) = ($params{avail},$params{type},$params{price});
	if ($params{sale}) {
		$avail = $DB_avail - $params{sale};
		$type = $DB_type;
		$price = $DB_price;
	} else {
		$avail = $DB_avail unless defined $avail;
		$type = $DB_type unless defined $type;
		$price = $DB_price unless defined $price;
	}

	if ( (abs($DB_avail - $avail) > 0.001) or ($DB_type ne $type) or (abs($DB_price - $price) > 0.001) ) {
$self->printLog ("Updating kiosk fuel: DB_avail-avail ([$DB_avail]-[$avail]=".abs($DB_avail - $avail).") ".
"DB_price-price ([$DB_price]-[$price]=".abs($DB_price - $price).") ".
"DB_type=[$DB_type] type=[$type]\n");
		if ($params{updateDB}) {
			$self->{DBH}->do(UPDATE_KIOSK_FUEL, undef, $avail, $type, $price, $kiosk_id);
			$self->setWebsiteFuel ($kiosk_id,$avail,$type,$price);
		}
		# We're updating the kiosk with slightly looser parameters than the DB
		# to avoid unnecessary traffic.
		if ($params{updateKiosk} and ( (abs($DB_avail - $avail) >= 0.01) or ($DB_type ne $type) or (abs($DB_price - $price) >= 0.01) ) ) {
			$self->{DBH}->do(DELETE_FUEL_MESSAGES, undef, $kiosk_id);
			if ($params{updateKiosk} eq "fromParam") {
				$self->{DBH}->do (INSERT_FUEL_MESSAGE,undef,
					$kiosk_id, "$avail\t$type\t$price");
$self->printLog ("Set kiosk message: avail: $avail, type: $type, price: $price\n");
			} else {
				$self->{DBH}->do (INSERT_FUEL_MESSAGE,undef,
					$kiosk_id, "$DB_avail\t$DB_type\t$DB_price");
$self->printLog ("Set kiosk message: avail: $DB_avail, type: $DB_type, price: $DB_price\n");
			}
		}
	}
}

sub getMembershipPrice {
my $self = shift;
my $kiosk_id = shift;

	my ($price) = $self->{DBH}->selectrow_array (GET_MEMBERSHIP_PROMO_PRICE, undef, $kiosk_id);
	$price = $self->{DBH}->selectrow_array (GET_MEMBERSHIP_PRICE) unless $price;

	return ($price);

}

sub getRenewalPrice {
my $self = shift;

	my ($price) = $self->{DBH}->selectrow_array (GET_RENEWAL_PRICE);
	return ($price);

}

sub getTrialSurcharge {
my $self = shift;

	my ($price) = $self->{DBH}->selectrow_array (GET_TRIAL_SURCHARGE_PRICE);
	return ($price);

}

sub getUpgradePrice {
my $self = shift;

	my ($price) = $self->{DBH}->selectrow_array (GET_UPGRADE_PRICE);
	return ($price);

}

sub safeCGIparam {
	my ($self,$param) = @_;
	return $self->safeCGIstring($self->{CGI}->param($param));
}

sub safeCGIstring {
	my ($self,$param) = @_;
	return undef unless defined $param;
	return $param unless
		$param =~ /((\%3C)|<)[^\n]+((\%3E)|>)/ix         # CSS Attack
		or $param =~ /(\%27)|(\')|(\-\-)|(\%23)|(\#)/ix  # SQL Injection
	;
	return undef;
}

sub isCGInumber {
	my ($self,$param) = @_;
	return looks_like_number ( $self->safeCGIstring($self->{CGI}->param($param)) );
}

sub isNumber {
	my ($self,$param) = @_;
	return looks_like_number ( $param );
}

sub error {
	my $self = shift;
	$self->{ERROR} = shift;
}

sub setWebsiteKioskStatus {
	my $self = shift;
	my ($kiosk_id,$is_online) = @_;
	
	my $fname;
	my ($name) = $self->{DBH}->selectrow_array (GET_KIOSK_NAME,undef,$kiosk_id);
	$fname = $KIOSK_STATUS_FILE;
	$fname =~ s/\[.+\]/$name/;

	if ( open (KIOSK_STATUS_FH,"> ".$fname) ) {
		if ($is_online) {
			print KIOSK_STATUS_FH "Online\n";
		} else {
			print KIOSK_STATUS_FH "Offline\n";
		}
		close (KIOSK_STATUS_FH);		
	} else {
#$self->printLog ("Error: $@\n");
	}
	return 1;
}

sub setWebsiteFuel {
	my $self = shift;
	my ($kiosk_id,$fuel_avail,$fuel_type,$fuel_price) = @_;

	my $fname;
	my ($name) = $self->{DBH}->selectrow_array (GET_KIOSK_NAME,undef,$kiosk_id);
	$fname = $FUEL_QUANTITY_FILE;
	$fname =~ s/\[.+\]/$name/;
#$self->printLog ("Updating website: $fname\n");
	if ( open (FUEL_QUANTITY_FH,"> ".$fname) ) {
		print FUEL_QUANTITY_FH sprintf ("%.0f\n",$fuel_avail);
		close (FUEL_QUANTITY_FH);		
	} else {
#$self->printLog ("Error: $@\n");
	}

	$fname = $FUEL_TYPE_FILE;
	$fname =~ s/\[.+\]/$name/;
#$self->printLog ("Updating website: $fname\n");
	if ( open (FUEL_TYPE_FH,"> ".$fname) ) {
		print FUEL_TYPE_FH $fuel_type;
		close (FUEL_TYPE_FH);		
	} else {
#$self->printLog ("Error: $@\n");
	}

	$fname = $FUEL_PRICE_FILE;
	$fname =~ s/\[.+\]/$name/;
#$self->printLog ("Updating website: $fname\n");
	if ( open (FUEL_PRICE_FH,"> ".$fname) ) {
		print FUEL_PRICE_FH sprintf ("%.2f\n",$fuel_price);
		close (FUEL_PRICE_FH);		
	} else {
#$self->printLog ("Error: $@\n");
	}
	
	my ($biodiesel_gals) = $self->{DBH}->selectrow_array (GET_TOTAL_BIODIESEL);
#$self->printLog ("Updating website: $CARBON_TONS_FILE\n");
	if ( open (CARBON_TONS_FH,"> ".$CARBON_TONS_FILE) ) {
	# 78.5% reduction in carbon dioxide emissions (biodiesel.org).
	# 22.2 pounds/gallon diesel (epa).
	# 2,205 lb/meteric ton (US ton, wikipedia)
	# (biodiesel * (22.2 * .785))/2205 = biodiesel * 0.0079034
		print CARBON_TONS_FH sprintf ("%.2f\n",$biodiesel_gals * 0.0079034);
		close (CARBON_TONS_FH);		
	} else {
#$self->printLog ("Error: $@\n");
	}

}

sub getGWconf {
	my $self = shift;
	my $force = shift; # Get the real GW string.  Otherwise controlled by SERVER_GW constant
	my $GW_conf;

	if (!$REAL_GW_CONF && open (GW_CONF_FH,"< ".$GW_CONF_FILE)) {
		while (<GW_CONF_FH>) {
			chomp;
			$REAL_GW_CONF = $_;
			next if $REAL_GW_CONF =~ /^#.+/;
			next if $REAL_GW_CONF =~ /^\s*$/;
			last;
		}
		close (GW_CONF_FH);		
	}
	
	die "Could not fetch the GW conf string!" unless $REAL_GW_CONF;
	
	if ( $force || !SERVER_GW ) {
		$GW_conf=$REAL_GW_CONF;
	} else {
		$GW_conf='Server GW';
	}
	return ($GW_conf);
}

sub getKioskKey {
	my $self = shift;
	my $kioskID = shift;
	
	die "Requesting key for undefined kiosk"
		unless defined $kioskID;

	return ($KIOSK_KEYS{$kioskID}) if exists $KIOSK_KEYS{$kioskID};

	my $key;
	my $client_key_pem;
	my $sth = $self->{DBH}->prepare(GET_KIOSK_KEY) or die "Could not prepare handle";
	$sth->execute( $kioskID );
	$sth->bind_columns (\$client_key_pem);
	if ($sth->fetch()) {
#		$self->printLog ("Client key:\n$client_key_pem\n");
		$key = Crypt::OpenSSL::RSA->new_public_key($client_key_pem);
		$key->use_pkcs1_oaep_padding() if $key;
	}

	die "Could not load RSA key for kiosk $kioskID" unless $key;
	
	$KIOSK_KEYS{$kioskID} = $key;
	return ($KIOSK_KEYS{$kioskID});
}

sub getKioskKeys {
	my $self = shift;
	my $kioskID;

	foreach (keys %KIOSK_KEYS) {
		delete $KIOSK_KEYS{$_};
	}

	my $sth = $self->{DBH}->prepare(GET_KIOSKS) or die "Could not prepare handle";
	$sth->execute( );
	$sth->bind_columns (\$kioskID);
	while ($sth->fetch()) {
		$self->getKioskKey ($kioskID);
	}

}



sub getServerKey {
	my $self = shift;
	return $SERVER_KEY if $SERVER_KEY;
	
	open (PEM_FILE,"< $KEYFILE")
	or die "Could not open $KEYFILE\n";
	
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
	}
	$SERVER_KEY = $rsa_priv;
	return ($SERVER_KEY);
}


sub getRSAmodulus {
	my $self = shift;
	my $key = $self->getServerKey();
	my ($n,$e,$d,$p,$q,$dmp1,$dmq1,$iqmp) = $key->get_key_parameters();
	return ($n->to_hex());
}

sub decryptRSApasswd {
	my $self = shift;
	my $plaintext = $self->decryptRSA (decode_base64(shift));
	my ($time,$secret) = ($1,$2) if ($plaintext =~ /^\[(\d+)\](.*)$/);
	return undef unless $time and $secret;
	return undef if time() - $time > $CRYPT_LIFETIME;

	return $secret;
	
}

sub useB64 {
	my $self = shift;
	if (scalar (@_)) {
		if (shift) {
			$self->{USE_B64} = 1;
		} else {
			$self->{USE_B64} = 0;
		}
	}
	return ($self->{USE_B64});
}

sub decryptRSA {
	my $self = shift;
	my $cryptext = shift;

	my $cryptLngth = length ($cryptext);
	my $key = $self->getServerKey();
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
	my $self = shift;
	my ($kioskID,$cryptext,$sig_b64) = @_;
	my $kioskKey = $self->getKioskKey ($kioskID);

	my ($crypt_msg,$sig);
	if ($self->useB64()) {
		$sig = decode_base64 ($sig_b64);
		$crypt_msg = decode_base64 ($cryptext);
	} else {
		my $lngth = length ($cryptext);
		$crypt_msg = substr ($cryptext,0,$lngth-$kioskKey->size());
		$sig = substr ($cryptext,$lngth-$kioskKey->size(),$kioskKey->size());
	}

	my $plaintext = $self->decryptRSA ($crypt_msg);
	
	return undef unless $plaintext and $sig and $kioskKey;
	if ( $kioskKey->verify( $plaintext, $sig) ) {
		$self->printLog ("Kiosk $kioskID: Verified message.\n");
		$self->printLog ("Message contains non-graphing/non-printing characters.\n")
			if $plaintext =~ /[^[:graph:][:space:]]/;
		$plaintext =~ s/[^[:graph:][:space:]]//g;
		return $plaintext;
	} else {
		return undef;
	}
}

sub SHA1_b64 {
	my $self = shift;
	return sha1_base64 (shift);
}

sub encryptRSAsign {
	my $self = shift;
	my $plaintext = shift;
	my $textLngth = length ($plaintext);
	my $kioskKey = $self->getKioskKey (shift);
	my $key = $self->getServerKey();

	$kioskKey->use_pkcs1_oaep_padding();
	my $keySize = $kioskKey->size();
	my $chunkSize = $keySize-42;
	my $crypt_resp;
	
	# The response is encrypted in chunkSize size chunks
	my $chunk;
	my $offset=0;
	while ($offset < $textLngth) {
		$chunk = substr ($plaintext,$offset,$chunkSize);
		
		if ($chunk) {
			$kioskKey->use_pkcs1_oaep_padding();
			$crypt_resp .= $kioskKey->encrypt ($chunk);
		}
		$offset += $chunkSize;
	}

#	$key->use_pkcs1_padding();
	$key->use_pkcs1_oaep_padding();
	my $serv_signature = $key->sign($plaintext);
	$plaintext =~ s/./x/g;
	if ($self->useB64()) {
		return (encode_base64($crypt_resp.$serv_signature));
	} else {
		return ($crypt_resp.$serv_signature);
	}
}

sub getLoginResetSession {
	my $self = shift;
	my ($member_id,$email,$isEmailReset) = @_;

	my $session = new CGI::Session("driver:MySQL", 'crap', { Handle=>$self->{DBH} })
		or die "Could not create a new session!";
	my $sid = $session->id();

	my $url_base = $self->{URL_BASE};
	my $reset_url;

	$session->expire('+'.$LOGIN_RESET_LIFETIME.'d');
	if ($isEmailReset) {
		$session->param ('email_reset',1);
		$reset_url = "$url_base/BBD-email-reset.pl?CGISESSID=$sid",
	} else {
		$session->param ('login_reset',1);
		$reset_url = "$url_base/BBD-login-reset.pl?CGISESSID=$sid";
	}
	$session->param ('email',$email);
	$session->param ('member_id',$member_id);
	$session->flush();	
	undef($session);
	
	return ($sid,$reset_url,$self->getMemberURL(),$LOGIN_RESET_LIFETIME);

}

sub send_email {
	my $self = shift;
	my %param = @_;
	$self->printLog ('Sending email to '.$param{To}.', Subject: '.$param{Subject}."\n");
	sendmail(%param) or $self->printLog ('sendmail error: '. $Mail::Sendmail::error."\n");
}

sub send_conf_email {
	my $self = shift;
	my ($new_email,$member_id,$member_name) = @_;

	my ($sid,$reset_url,$memb_url,$lifetime) =
		$self->getLoginResetSession($member_id,$new_email,1);

	my $template = HTML::Template->new( filename => "$TMPL_DIR/$CONF_EMAIL_TEMPLATE" )
		or die "Could not load template";		
	$template->param(
		MEMBER_NAME => $member_name,
		RESET_URL => $reset_url,
		MEMBER_URL => $memb_url,
		LIFETIME => $lifetime,
	);

	my $message = $template->output();

	$self->send_email (
		To      => "$member_name <$new_email>",
		From    => $COOP_NAME.' <donotreply@'.$COOP_DOMAIN.'>',
		Subject => 'Email Address Confirmation',
		Message => $message
	);
}


sub send_welcome_email {
	my $self = shift;
	my ($email,$member_id,$member_name) = @_;
	
	my ($renewal_fee) = $self->{DBH}->selectrow_array (GET_RENEWAL_PRICE);
	my ($sid,$reset_url,$memb_url,$lifetime) = 
		$self->getLoginResetSession($member_id,$email);

	my $template = HTML::Template->new( filename => "$TMPL_DIR/$WELCOME_EMAIL_TEMPLATE" )
		or die "Could not load template";		
	$template->param(
		MEMBER_NAME => $member_name,
		RESET_URL => $reset_url,
		MEMBER_URL => $memb_url,
		LIFETIME => $lifetime,
		RENEWAL_FEE => $renewal_fee,
	);

	my $message = $template->output();

	$self->send_email (
		To      => "$member_name <$email>",
		From    => $COOP_NAME.' <donotreply@'.$COOP_DOMAIN.'>',
		Subject => $COOP_NAME.' Automated Fuel Kiosk Information',
		Message => $message
	);
	
}


sub send_reset_email {
	my $self = shift;
	my ($email,$member_id,$member_name) = @_;
	
	my $email_session = new CGI::Session("driver:MySQL", 'crap', { Handle=>$self->{DBH} })
		or die "Could not create a new session!";
	my $email_sid = $email_session->id();

	$email_session->expire('+'.$LOGIN_RESET_LIFETIME.'d');
	$email_session->param ('login_reset',1);
	$email_session->param ('email',$email) if $email;
	$email_session->param ('member_id',$member_id);
	$email_session->flush();
	undef($email_session);

	my $url_base = $self->{URL_BASE};
	my $template = HTML::Template->new( filename => "$TMPL_DIR/$LOGIN_RESET_EMAIL_TEMPLATE" )
		or die "Could not load template";		
	$template->param(
		MEMBER_NAME => $member_name,
		RESET_URL => "$url_base/BBD-login-reset.pl?CGISESSID=$email_sid",
		MEMBER_URL => "$url_base/BBD-login.pl",
		LIFETIME => $LOGIN_RESET_LIFETIME,
	);

	my $message = $template->output();

	$self->send_email (
		To      => "$member_name <$email>",
		From    => $COOP_NAME.' <donotreply@'.$COOP_DOMAIN.'>',
		Subject => $COOP_NAME.' Login Reset',
		Message => $message
	);
}


# Returns undef or at most 1 member_id
# if member_id being checked does not match the returned member_id
# then the SPN, first name and last name combination is not unique.
sub check_unique_SPN_names {
	my $self = shift;
	my ($fname,$lname,$SPN) = @_;
	my ($DB_id, $DB_SPN);
	my $authenticated;
	
	my $sth = $self->{DBH}->prepare(GET_SPNS_BY_F_L_NAMES) or die "Could not prepare handle";
	$sth->execute( $fname, $lname );
	$sth->bind_columns (\$DB_id, \$DB_SPN);
	while($sth->fetch()) {
		$authenticated = 1 if crypt($SPN, $DB_SPN) eq $DB_SPN;
	}
	return $DB_id if $authenticated;
	return undef;
}

sub get_memb_ids_by_name {
	my $self = shift;
	my $name = shift;
	my $memb_id;
	my @memb_ids;
	
	my $sth = $self->{DBH}->prepare(GET_MEMB_ID_BY_NAME) or die "Could not prepare handle";
	$sth->execute( $name );
	$sth->bind_columns (\$memb_id);
	while($sth->fetch()) {
		push (@memb_ids, $memb_id);
	}
	return @memb_ids;
}

sub session_clear {
	my $self = shift;
	return $self->{SESSION}->clear (@_);
}

sub session_close {
	my $self = shift;
	$self->{SESSION}->flush () if $self->{SESSION};
	undef ($self->{SESSION});
}

sub session_delete {
	my $self = shift;
	return unless $self->{SESSION};
	$self->{SESSION}->delete ();
	undef ($self->{SESSION});
}

sub session_param {
	my $self = shift;
	return undef unless $self->{SESSION};
	return $self->{SESSION}->param (@_);
}

sub session_is_new {
	return shift->{SESSION_IS_NEW};
}

sub session_was_expired {
	return shift->{SESSION_WAS_EXPIRED};
}

sub getRenewalGracePeriod {
	return ($GRACE_PERIOD_BEFORE_DAYS,$GRACE_PERIOD_AFTER_DAYS);
}

sub getAskRenewDays {
	return ($ASK_RENEW_START_DAYS);
}

sub getTimezone {
	return $TIMEZONE;
}

sub epoch_to_ISOdate {
	my ($self,$epoch,$timezone) = @_;
	$timezone = $TIMEZONE unless $timezone;
	if ($epoch) {
		my $dt = DateTime->from_epoch( epoch => $epoch, formatter => $DT_ISO_DATE_FORMAT );
		$dt->set_time_zone ($timezone);
		return "$dt";
	} else {
		return '';
	}
}

sub epoch_to_date {
	my ($self,$epoch,$timezone) = @_;
	$timezone = $TIMEZONE unless $timezone;
	if ($epoch) {
		my $dt = DateTime->from_epoch( epoch => $epoch, formatter => $DT_DATE_FORMAT );
		$dt->set_time_zone ($timezone);
		return "$dt";
	} else {
		return '';
	}
}

sub epoch_to_ISOdatetime {
	my ($self,$epoch,$timezone) = @_;
	$timezone = $TIMEZONE unless $timezone;
	if ($epoch) {
		my $dt = DateTime->from_epoch( epoch => $epoch, formatter => $DT_ISO_DATETIME_FORMAT );
		$dt->set_time_zone ($timezone);
		return "$dt";
	} else {
		return '';
	}
}

sub epoch_to_ISOdatetimeSecs {
	my ($self,$epoch,$timezone) = @_;
	$timezone = $TIMEZONE unless $timezone;
	if ($epoch) {
		my $dt = DateTime->from_epoch( epoch => $epoch, formatter => $DT_ISO_DATETIME_SEC_FORMAT );
		$dt->set_time_zone ($timezone);
		return "$dt";
	} else {
		return '';
	}
}

sub epoch_to_datetime {
	my ($self,$epoch,$timezone) = @_;
	$timezone = $TIMEZONE unless $timezone;
	if ($epoch) {
		my $dt = DateTime->from_epoch( epoch => $epoch, formatter => $DT_DATETIME_FORMAT );
		$dt->set_time_zone ($timezone);
		return "$dt";
	} else {
		return '';
	}
}

# This should be called if the error occurred to early to even process the form
# or if the error makes form processing or displaying inappropriate.
# This uses a separate template for the page.
sub fatal {
	my $self = shift;
	my $error = shift;

	my $template = HTML::Template->new( filename => "$TMPL_DIR/$FATAL_TEMPLATE" )
		or die "Could not load template";		

	$template->param(
		ERROR => $error,
	);

	print "Status: 500 Error\n";
	print "Content-type: text/html\n\n";
    print $template->output();
}


1;
__END__


=head1 NAME

BBD - Perl utilities for running the Baltimore Biodiesel Coop

=head1 SYNOPSIS

  use BBD;
  Don't feel inclined to write documentation right now - use the code, Luke.

=head1 DESCRIPTION

This is a collection of utilities for running the Baltimore Biodiesel Coop.
If you are running or planning to start up a biodiesel/biofuels/fuel coop,
this may be of use to you.

Otherwise, probably not.

=head1 SEE ALSO

http://www.baltimorebiodiesel.org/

=head1 AUTHOR

Ilya Goldberg, igg at cathilya dot org

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2008 by Ilya Goldberg

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

=cut
