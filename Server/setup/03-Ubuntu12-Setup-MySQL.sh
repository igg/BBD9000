#!/bin/bash
# Coop-specific variables:
DB_BASE="piedmont"

# Fancy way of determining the directory of this script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$BIN_DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
ROOT_DIR=$( dirname "$BIN_DIR" )
date=`date '+%F'`

read -s -p "Enter MySQL root password: " ROOT_PASS

# Generate a password for the bbd-server user, which is used by the website
BBD_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
mysql --user=root  --password=$ROOT_PASS mysql <<EOF
CREATE USER '${DB_BASE}-server'@'localhost' IDENTIFIED BY '$BBD_PASS';
GRANT ALL PRIVILEGES ON *.* TO '${DB_BASE}-server'@'localhost' WITH GRANT OPTION;
create database ${DB_BASE}_members;
\q
EOF

mkdir priv
chmod 700 priv
# Generate the credential file used by the server scripts
BBD_CNF="${ROOT_DIR}/priv/DB_CONF.cnf"
cat > "$BBD_CNF" <<EOF
# DB_CONF.cnf

[client]
host     = localhost
database = ${DB_BASE}_members
user     = ${DB_BASE}-server
password = $BBD_PASS
EOF
chmod 400 $BBD_CNF

# Generate an equivalent credential file used by the backup/maintenance script
BBD_CNF="${ROOT_DIR}/priv/DB_BACKUP.cnf"
cat > "$BBD_CNF" <<EOF
# DB_BACKUP.cnf 

[client]
host     = localhost
user     = ${DB_BASE}-server
password = $BBD_PASS
EOF
chmod 400 $BBD_CNF

BBD_CNF="${ROOT_DIR}/priv/DB_BACKUP.cnf"
EMPTY_DB="${ROOT_DIR}/setup/BBD-Schema.sql"
BACKUP_DB="${ROOT_DIR}/backups/DB-${DB_BASE}_members-$(date '+%F-%T').sql.bz2"
EXIST_DB=$(mysql --defaults-extra-file="$BBD_CNF" ${DB_BASE}_members -e 'show tables')

if [ -n "$EXIST_DB" ]; then
	echo "Database ${DB_BASE}_members already exists and has defined tables!"
	read -p "Destroy existing contents and create an empty database? (y/N)?" choice
	case "$choice" in 
		y|Y )
			echo "Existing ${DB_BASE}_members database will be backed up to ${BACKUP_DB}"
			/usr/bin/mysqldump --defaults-extra-file=priv/DB_BACKUP.cnf --single-transaction ${DB_BASE}_members | bzip2 > "$BACKUP_DB"
			EXIST_DB=""
		;;
		* ) echo "Skipping making an empty database";;
	esac
fi

if [ -z "$EXIST_DB" ]; then
	echo "Making empty ${DB_BASE}_members database"
	mysql --defaults-extra-file="$BBD_CNF" ${DB_BASE}_members < "$EMPTY_DB"
fi

#
