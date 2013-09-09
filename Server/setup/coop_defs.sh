# coop_defs.sh
# This file is the common place to define coop-specific variables
# This file is sourced from other scripts in the bin directory
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$BIN_DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
BIN_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
ROOT_DIR=$( dirname "$BIN_DIR" )

# coop-specific variables
COOP_NAME="Test Coop"
DB_BASE="test"
DOMAIN="${DB_BASE}.bbd9000.com"
DB_NAME="${DB_BASE}_members"
DB_CNF="${ROOT_DIR}/priv/DB_CONF.cnf"
S3_BUCKET="s3://Test-Coop-Backup/"
eval S3CFG="~/priv/.s3cfg"
COOP_TIMEZONE="US/Eastern"

# other variables
ISO_DATE=`date '+%F'`
DATE=`date`
