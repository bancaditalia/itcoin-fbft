#!/usr/bin/env bash
#
# ItCoin
#
# REQUIREMENTS:
# - python3
# - jq
# - the "date" command

set -eu
shopt -s inherit_errexit

# https://stackoverflow.com/questions/59895/how-to-get-the-source-directory-of-a-bash-script-from-within-the-script-itself#246128
MYDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# the total number of bitcoin mining nodes
TOT_NODES=4

errecho() {
    # prints to stderr
    >&2 echo "${@}"
}

checkPrerequisites() {
    if ! command -v python3 &> /dev/null; then
        errecho "Please install python3"
        exit 1
    fi
    if ! command -v jq &> /dev/null; then
        errecho "Please install jq (https://stedolan.github.io/jq/)"
        exit 1
    fi
    if ! command -v date &> /dev/null; then
        errecho "The date command is not available"
        exit 1
    fi
}

# Do not run if the required packages are not installed
checkPrerequisites

read -r -d '' __usage <<-EOF || true
For each of the ${TOT_NODES} predefined nodes:
    - in miner.conf.json, sets genesis_block_timestamp to the current timestamp
    - in miner.conf.json, sets target_block_time to the given value (or 60 if not given)
    - deletes the blockchain data
    - deletes the prolog database
    - deletes the bitcoin debug.log

In case of error, nothing is changed.

USAGE:
    $(basename "$0") [--help|-H]
    $(basename "$0") [TARGET_BLOCK_TIME]

    TARGET_BLOCK_TIME: the desired interval in seconds for the production of a
                       new block
                       Default: 60 seconds.

    TARGET_BLOCK_TIME must be an integer >= 1.

EXAMPLES:
    ./$(basename "$0")
    ./$(basename "$0") 2
EOF

if [ $# -ge 2 ] || [ "${1-}" = '--help' ] || [ "${1-}" = '-H' ] ; then
  errecho "${__usage}"
  exit 1
fi

TARGET_BLOCK_TIME="${1:-60}"
if [ -z "${TARGET_BLOCK_TIME##*[!0-9]*}" ]; then
  errecho "ERROR: if present, the first argument to this script must be an integer"
  exit 1
fi

if [ "${TARGET_BLOCK_TIME}" -lt 1 ]; then
  errecho "ERROR: the first argument to this script must be >= 1"
  exit 1
fi

errecho "Using TARGET_BLOCK_TIME=${TARGET_BLOCK_TIME}"

# current timestamp in seconds since the epoch
CURRENT_TIMESTAMP=$(date -d "0 minutes ago" "+%s")

for ((i = 0; i < "${TOT_NODES}"; i++)); do
    # ID will be a two-digits, zero-padded, zero-based sequence.
    # For example, if TOT_NODES = 4, then ID will be "00", "01", "02", "03"
    ID=$(printf "%02d" "${i}")

    rm -f  "${MYDIR}/node${ID}/miner.pbft.db"
    rm -f  "${MYDIR}/node${ID}/signet/.lock"
    rm -f  "${MYDIR}/node${ID}/signet/debug.log"
    rm -rf "${MYDIR}/node${ID}/signet/blocks"
    rm -rf "${MYDIR}/node${ID}/signet/chainstate"
    rm -f  "${MYDIR}/node${ID}/signet/"*.dat

    # Set the genesis_block_timestamp parameter to CURRENT_TIMESTAMP.
    #
    #   - jq is not able to rewrite a file in place, so we slurp it in a
    #     variable beforehand (we do not want to rely on sponge from moreutils).
    #   - the configuration file is minified before being fed to jq, so that
    #     comments do not cause problems
    #   - the json_minify library is taken from
    #     https://github.com/getify/JSON.minify/tree/python
    #   - we run each step separately and save the result in memory, instead of
    #     using a more idiomatic pipe. This guarantees that in case of errors
    #     miner.conf.json do not get overwritten with an empty string
    MINER_CONF_FILEPATH="${MYDIR}/node${ID}/miner.conf.json"
    MINER_CONF_CONTENTS=$(<"${MINER_CONF_FILEPATH}")

    # cd-ing into "${MYDIR}" is a poor man's version of modifying the python
    # import path.
    MINIFIED_CONTENTS=$(cd "${MYDIR}" && python3 -c "import sys; import json_minify ; print(json_minify.json_minify(sys.stdin.read()))" <<<"${MINER_CONF_CONTENTS}")

    RECONFIGURED_CONTENTS=$(jq ".genesis_block_timestamp=${CURRENT_TIMESTAMP} | .target_block_time=${TARGET_BLOCK_TIME}" <<<"${MINIFIED_CONTENTS}")
    printf "%s" "${RECONFIGURED_CONTENTS}" > "${MINER_CONF_FILEPATH}"
done

rm -rf  "${MYDIR}/__pycache__"
