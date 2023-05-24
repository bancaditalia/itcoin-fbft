#!/usr/bin/env bash
#
# This script will not work for paths containing spaces

FILE_COUNT=0

for file in $1;
do
  test -f "${file}" || { echo "library ${file} is missing."; exit 1; }
  FILE_COUNT=$((FILE_COUNT+1))
done

echo "no library is missing. Checked ${FILE_COUNT} libraries."
exit 0
