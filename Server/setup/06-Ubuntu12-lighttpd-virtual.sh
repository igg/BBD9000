#!/bin/bash
# Coop-specific variables:
# Fancy way of determining the directory of this script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$BIN_DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
ROOT_DIR=$( dirname "$BIN_DIR" )
# File with site-specific variables
source ${ROOT_DIR}/setup/coop_defs.sh

# The tricky variable substitution below is to construct a host regular expression from the DOMAIN:
# $HTTP["host"] =~ "(^|\.)test\.bbd9000\.com$" when DOMAIN="test.bbd9000.com"
# Also all literal $ below must be escaped to avoid variable substitution
cat > "${ROOT_DIR}/setup/lighttpd-virt.conf" <<EOF
\$HTTP["host"] =~ "(^|\.)${DOMAIN//./\\.}\$" {
	var.root    = "${ROOT_DIR}/"

	# No need to change anything below here for a standard installation
	var.web    = var.root + "web/"
	var.logs    = var.root + "tmp/"
	var.var    = var.root + "var/"
	var.fcgi    = var.web + "fcgi-bin/"
	server.document-root = var.web + "public" 
	\$HTTP["url"] =~ "^/fcgi-bin/" {
		server.document-root = var.fcgi
	}
	alias.url = ( "/cgi-bin/" => var.fcgi,
              "/fcgi-bin/" => var.fcgi )
	server.errorlog             = var.logs + "lighttpd.error.log"
	accesslog.filename          = var.logs + "lighttpd.access.log"
	fastcgi.server = ( ".pl" => ((
		"bin-path"        => var.fcgi + "disp.pl",
		"socket"          => var.var + "BBD-disp.socket",
		"check-local"     => "enable",
		"max-procs"       => 3,
		"idle-timeout"    => 60
	)))
	fastcgi.server += ( ".php" => ((
		"bin-path" => "/usr/bin/php-cgi",
		"socket" => var.var + "php.socket",
		"max-procs" => 3,
		"bin-environment" => ( 
			"PHP_FCGI_CHILDREN" => "2",
			"PHP_FCGI_MAX_REQUESTS" => "10000"
		),
		"bin-copy-environment" => (
			"PATH", "SHELL", "USER"
		),
		"broken-scriptfilename" => "enable"
	)))
}
EOF

echo "remember to restart lighttpd:"
echo "sudo service lighttpd restart"

