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

# Make a readonly account for this DB
CNF_FILENAME="DB_READONLY.cnf"
CNF_FILE="${ROOT_DIR}/priv/${CNF_FILENAME}"
if [ ! -f ${CNF_FILE} ]; then
	# Generate a MySQL user for the server to connect as
	DB_USER="${DB_BASE}-ro"
	echo "Generating new credentials in ${CNF_FILE} for readonly user ${DB_USER}"
	DB_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
	# We're using the home directory's DB connection to do this
	echo "...creating new user ${DB_USER}"
	mysql --defaults-extra-file=${DB_CNF} <<EOF
CREATE USER '${DB_USER}'@'localhost' IDENTIFIED BY '$DB_PASS';
GRANT SELECT ON ${DB_NAME}.* TO '${DB_USER}'@'localhost';
\q
EOF

	# Generate the credential file used by the server scripts
	cat > "${CNF_FILE}" <<EOF
# ${CNF_FILENAME}

[client]
host     = localhost
database = ${DB_NAME}
user     = ${DB_USER}
password = ${DB_PASS}
EOF
	chmod 400 $CNF_FILE
else
	echo "Readonly credentials in ${CNF_FILE} already exist"
fi
