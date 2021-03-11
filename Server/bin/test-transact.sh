#!/bin/bash
# first param on command line is the gateway configuration file (e.g. BBDC_GW_test.conf)
# second param is track data file (1 track per line, track1 followed by track2)
# both files may contain comment lines beginning with #
while IFS=$' ' read -r CC_URL x_cpversion x_login x_market_type x_device_type x_tran_key; do
  [[ "$CC_URL" =~ ^#.*$ ]] && continue
  printf "%b\n" "CC_URL <${CC_URL}>"
  printf "%b\n" "x_cpversion <${x_cpversion}>"
  printf "%b\n" "x_login <${x_login}>"
  printf "%b\n" "x_market_type <${x_market_type}>"
  printf "%b\n" "x_device_type <${x_device_type}>"
  printf "%b\n" "x_tran_key <${x_tran_key}>"
  break
done < "$1"

tracks=()
while read -r line; do
  [[ "$line" =~ ^#.*$ ]] && continue
  tracks+=("$line")
done < "$2"

printf "%b\n" "track1 <${tracks[0]}>"
printf "%b\n" "track2 <${tracks[1]}>"

x_type=AUTH_ONLY
x_track2="$tracks[1]"
x_amount=1.0

curl -v --trace --trace-ascii -X POST -F "x_cpversion=${x_cpversion}" -F x_login="${x_login}" \
  -F x_market_type="${x_market_type}" -F "x_device_type=${x_device_type}" -F "x_tran_key=${x_tran_key}" \
  -F "x_type=${x_type}" -F "x_track2=${x_track2}" -F "x_amount=${x_amount}" \
  "${CC_URL}"

echo ""