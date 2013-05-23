#!/bin/bash
# Fancy way of determining the directory of this script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$BIN_DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
DB_CNF=$( dirname "$BIN_DIR" )"/priv/DB_BACKUP.cnf"

date=`date '+%F'`
# First dump structure and data for system tables (i.e. tables with system data)
mysqldump --defaults-extra-file=$DB_CNF --single-transaction bbd_members roles promotions item_prices fuel_prices alerts alert_roles > Schema-bbd_members-$date.sql
# Append the rest of the tables, excluding the system tables.
mysqldump --defaults-extra-file=$DB_CNF --single-transaction --no-data bbd_members \
	--ignore-table=bbd_members.roles \
	--ignore-table=bbd_members.promotions \
	--ignore-table=bbd_members.item_prices \
	--ignore-table=bbd_members.fuel_prices \
	--ignore-table=bbd_members.alerts \
	--ignore-table=bbd_members.alert_roles \
	>> Schema-bbd_members-$date.sql
