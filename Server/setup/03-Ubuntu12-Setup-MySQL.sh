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
# File with site-specific variables
source ${BIN_DIR}/coop_defs.sh



# Create new priv/DB_CONF.cnf and priv/DB_BACKUP.cnf MySQL connection files
# If the files do not exist, a new DB user & password will be created
# The credentials in ~/priv/DB_BACKUP.cnf will be used to make the new user
DB_CNF="${ROOT_DIR}/priv/DB_CONF.cnf"
if [ ! -f ${DB_CNF} ]; then
	# Generate a MySQL user for the server to connect as
	echo "Generating new credentials in ${DB_CNF}"
	DB_USER="${DB_BASE}-server"
	DB_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
	# We're using the home directory's DB connection to do this
	eval DB_CNF="~/priv/DB_BACKUP.cnf"
	echo "...creating new user ${DB_USER}"
	mysql --defaults-extra-file=${DB_CNF} <<EOF
CREATE USER '${DB_USER}'@'localhost' IDENTIFIED BY '$DB_PASS';
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
	if [ ! -f ${DB_CNF} ]; then
		cat > "${DB_CNF}" <<EOF
# DB_BACKUP.cnf 

[client]
host     = localhost
user     = ${DB_USER}
password = ${DB_PASS}
EOF
		chmod 400 $DB_CNF
	fi
else
	echo "Credentials in ${DB_CNF} already exist"
fi


# Generate an empty database.  Backup any existing database.
DB_CNF="${ROOT_DIR}/priv/DB_BACKUP.cnf"
EMPTY_DB="${ROOT_DIR}/setup/BBD-Schema.sql"
BACKUP_DB="${ROOT_DIR}/backups/DB-${DB_NAME}-${ISO_DATE}.sql.bz2"
echo "Checking existence of database '${DB_NAME}'"

EXIST_DB=$(mysql --defaults-extra-file="$DB_CNF" ${DB_NAME} -e 'show tables')

if [ -n "$EXIST_DB" ]; then
	echo "Database ${DB_NAME} already exists and has defined tables!"
	read -p "Destroy existing contents and create an empty database? (y/N)?" choice
	case "$choice" in 
		y|Y )
			echo "Existing ${DB_NAME} database will be backed up to ${BACKUP_DB}"
			/usr/bin/mysqldump --defaults-extra-file=${DB_CNF} --single-transaction ${DB_NAME} | bzip2 > "$BACKUP_DB"
			mysql --defaults-extra-file=${DB_CNF} -e "drop database ${DB_NAME};"
			mysql --defaults-extra-file=${DB_CNF} -e "create database ${DB_NAME};"
			EXIST_DB=""
		;;
		* ) echo "Skipping making an empty database";;
	esac
fi

if [ -z "$EXIST_DB" ]; then
	echo "Making empty ${DB_NAME} database"
	mysql --defaults-extra-file=${DB_CNF} -e "create database ${DB_NAME};"
	echo "...Reading schema from ${EMPTY_DB}"
	mysql --defaults-extra-file=${DB_CNF} ${DB_NAME} < "$EMPTY_DB"
fi

#
