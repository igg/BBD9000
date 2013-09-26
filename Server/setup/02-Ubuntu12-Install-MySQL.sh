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

# Generate a root password for mysql (this needs to be done before installing MySql)
ROOT_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
export DEBIAN_FRONTEND=noninteractive
sudo apt-get -q -y install mysql-client-5.5 mysql-common
sudo apt-get -q -y install mysql-server
mysqladmin -u root password $ROOT_PASS



# Create new priv/DB_CONF.cnf and priv/DB_BACKUP.cnf MySQL connection files
# A new DB user & password will be created and stored in these files
# The root user credentials defined above will be used to make the new user
DB_CNF="${ROOT_DIR}/priv/DB_CONF.cnf"
# Generate a MySQL user for the server to connect as
DB_USER="${DB_BASE}-server"
DB_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)

mysql --user=root --password=$ROOT_PASS mysql <<EOF
CREATE USER '${DB_USER}'@'localhost' IDENTIFIED BY '${DB_PASS}';
GRANT ALL PRIVILEGES ON *.* TO '${DB_USER}'@'localhost' WITH GRANT OPTION;
\q
EOF

mkdir priv
chmod 700 priv
# Generate the credential file used by the server scripts
DB_CNF="${ROOT_DIR}/priv/DB_CONF.cnf"
cat > "${DB_CNF}" <<EOF
# DB_CONF.cnf

[client]
host     = localhost
database = ${DB_NAME}
user     = ${DB_USER}
password = ${DB_PASS}
EOF
chmod 400 $DB_CNF

# Generate an equivalent credential file used by the backup/maintenance script
DB_CNF="${ROOT_DIR}/priv/DB_BACKUP.cnf"
cat > "${DB_CNF}" <<EOF
# DB_BACKUP.cnf 

[client]
host     = localhost
user     = ${DB_USER}
password = ${DB_PASS}
EOF
chmod 400 $DB_CNF
