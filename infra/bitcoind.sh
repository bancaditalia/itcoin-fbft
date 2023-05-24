#!/usr/bin/env bash

set -eu
shopt -s inherit_errexit

errecho() {
    # prints to stderr
    >&2 echo "${@}"
}

# https://stackoverflow.com/questions/59895/how-to-get-the-source-directory-of-a-bash-script-from-within-the-script-itself#246128
MYDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PATH_TO_BINARIES=$(realpath --quiet --canonicalize-existing "${MYDIR}/../../itcoin-core/src")

# the total number of bitcoin mining nodes
TOT_NODES=4


NODE="${1-}"

if [ -z "${NODE##*[!0-9]*}" ]; then
  errecho "ERROR: the first argument to this script must be a non negative integer"
  exit 1
fi

if ! ((${NODE} >= 0 && ${NODE} <= "$((TOT_NODES-1))")); then
  errecho "ERROR: the first argument to this script must be an integer index between 0 and $((TOT_NODES-1))"
  exit 1
fi

shift

DATADIR=$(realpath --canonicalize-existing "${MYDIR}/node0${NODE}")

LD_LIBRARY_PATH=${MYDIR}/../build/usrlocal/lib exec "${PATH_TO_BINARIES}/bitcoind" -datadir="${DATADIR}" "${@}"
