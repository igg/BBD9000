#!/bin/bash
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


# Generate a root password for mysql (this needs to be done before installing MySql)
ROOT_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
export DEBIAN_FRONTEND=noninteractive
sudo apt-get -q -y install mysql-client-5.5 mysql-common
sudo apt-get -q -y install mysql-server
mysqladmin -u root password $ROOT_PASS

# Generate a password for the bbd-server user, which is used by the website
BBD_PASS=$(apg -n 1 -a 1 -m 16 -M NCL)
mysql --user=root  --password=$ROOT_PASS mysql <<EOF
CREATE USER 'bbd-server'@'localhost' IDENTIFIED BY '$BBD_PASS';
GRANT ALL PRIVILEGES ON *.* TO 'bbd-server'@'localhost' WITH GRANT OPTION;
create database bbd_members;
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
database = bbd_members
user     = bbd-server
password = $BBD_PASS
EOF
chmod 400 $BBD_CNF

# Generate an equivalent credential file used by the backup/maintenance script
BBD_CNF="${ROOT_DIR}/priv/DB_BACKUP.cnf"
cat > "$BBD_CNF" <<EOF
# DB_BACKUP.cnf 

[client]
host     = localhost
user     = bbd-server
password = $BBD_PASS
EOF
chmod 400 $BBD_CNF

BBD_CNF="${ROOT_DIR}/priv/DB_BACKUP.cnf"
EMPTY_DB="${ROOT_DIR}/setup/Schema-bbd_members.sql"
BACKUP_DB="${ROOT_DIR}/backups/DB-bbd_members-$(date '+%F-%T').sql.bz2"
EXIST_DB=$(mysql --defaults-extra-file="$BBD_CNF" bbd_members -e 'show tables')

if [ -n "$EXIST_DB" ]; then
	echo "Database bbd_members already exists and has defined tables!"
	read -p "Destroy existing contents and create an empty database? (y/N)?" choice
	case "$choice" in 
		y|Y )
			echo "Existing bbd_members database will be backed up to ${BACKUP_DB}"
			/usr/bin/mysqldump --defaults-extra-file=priv/DB_BACKUP.cnf --single-transaction bbd_members | bzip2 > "$BACKUP_DB"
			EXIST_DB=""
		;;
		* ) echo "Skipping making an empty database";;
	esac
fi

if [ -z "$EXIST_DB" ]; then
	echo "Making empty bbd_members database"
	mysql --defaults-extra-file="$BBD_CNF" bbd_members < "$EMPTY_DB"	
fi
#
