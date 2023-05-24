#!/usr/bin/env bash
# This script is meant to be used only by CMake build commands.

set -u

# https://stackoverflow.com/questions/59895/how-can-i-get-the-directory-where-a-bash-script-is-located-from-within-the-scrip#246128
MY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

ITCOIN_CORE_LIBRARIES=$1
CMAKE_CURRENT_BINARY_DIR=$2
ITCOIN_CORE_SRC_DIRECTORY=$3
USR_DIR=$4
CMAKE_CXX_COMPILER=$5
CMAKE_C_COMPILER=$6

"${MY_DIR}/_check-files-existence.sh" "${ITCOIN_CORE_LIBRARIES}"
LIBRARIES_EXIST=$?

"${MY_DIR}/check-repo-src-changed.py" --status-dirpath="${CMAKE_CURRENT_BINARY_DIR}/itcoin-core-repo-status" --repo-dirpath="${ITCOIN_CORE_SRC_DIRECTORY}"
FILES_CHANGED=$?
if [[ ${FILES_CHANGED} != 0 && ${FILES_CHANGED} != 47 ]]; then
    echo "check-repo-src-changed.py exited with error code ${FILES_CHANGED}. Valid values are 0 or 47. Please verify what's going on."
    exit "${FILES_CHANGED}"
fi

if [[ ${LIBRARIES_EXIST} == 0 && ${FILES_CHANGED} == 0 ]]; then
    echo "No need to build itcoin-core"
    exit 0
fi

echo "itcoin-core needs to be rebuilt: check_existence=${LIBRARIES_EXIST}, check_repo_changed=${FILES_CHANGED}"

export CXX=${CMAKE_CXX_COMPILER}
export CC=${CMAKE_C_COMPILER}

# find number of cores on this machine
NPROC=$(nproc --ignore=1)

infra/configure-itcoin-core-dev.sh ${USR_DIR} ${USR_DIR} && make --jobs=${NPROC} && make install-strip
BUILD_RC=$?
exit "${BUILD_RC}"
