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


# Generate an empty database.  Backup any existing database.
# The credentials in ~/priv/DB_BACKUP.cnf will be used to make the new user
#   in other words, the root or top-level of any multi-database setup.
eval DB_CNF="~/priv/DB_BACKUP.cnf"
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


# Create new priv/DB_CONF.cnf and priv/DB_BACKUP.cnf MySQL connection files
# If the files do not exist, a new DB user & password will be created
CNF_FILENAME="DB_CONF.cnf"
CNF_FILE="${ROOT_DIR}/priv/${CNF_FILENAME}"
if [ ! -f ${CNF_FILE} ]; then
	# Generate a MySQL user for the server to connect as
	DB_USER="${DB_USER_BASE}-srv"
	echo "Generating new credentials in ${CNF_FILE} for read/write user ${DB_USER}"
	DB_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
	echo "...creating new DB user ${DB_USER}"
	mysql --defaults-extra-file=${DB_CNF} <<EOF
CREATE USER '${DB_USER}'@'localhost' IDENTIFIED BY '$DB_PASS';
GRANT ALL PRIVILEGES ON ${DB_NAME}.* TO '${DB_USER}'@'localhost' WITH GRANT OPTION;
\q
EOF
	mkdir priv
	chmod 700 priv
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

	# Generate an equivalent credential file used by the backup/maintenance script
	CNF_FILENAME="DB_BACKUP.cnf"
	CNF_FILE="${ROOT_DIR}/priv/${CNF_FILENAME}"
	if [ ! -f ${CNF_FILE} ]; then
		cat > "${CNF_FILE}" <<EOF
# ${CNF_FILENAME}

[client]
host     = localhost
user     = ${DB_USER}
password = ${DB_PASS}
EOF
		chmod 400 $CNF_FILE
	fi
else
	echo "Credentials in ${CNF_FILE} already exist"
fi



# Make a readonly account for this DB
bash ${ROOT_DIR}/setup/add_readonly.sh
