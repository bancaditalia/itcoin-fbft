#!/usr/bin/env bash

set -eu
shopt -s inherit_errexit

# https://stackoverflow.com/questions/59895/how-to-get-the-source-directory-of-a-bash-script-from-within-the-script-itself#246128
MYDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# the total number of bitcoin mining nodes
TOT_NODES=4

# https://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux#5947802
RED='\033[0;31m'
GREEN='\033[0;32m'
BROWN='\033[0;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'

NC='\033[0m' # no color

# https://stackoverflow.com/questions/26761627/check-if-there-is-an-stdout-redirection-in-bash-script#26761733
if [ -t 1 ] ; then
    RUNNING_ON_TERMINAL=true
else
    RUNNING_ON_TERMINAL=false
fi

# build a different color map, depending on whether we are running on a terminal
# or the output is being redirected on a file (in that case there will be no
# colors)
COLORMAP=()
if [ "${RUNNING_ON_TERMINAL}" = true ]; then
    COLORMAP+=("${RED}")
    COLORMAP+=("${GREEN}")
    COLORMAP+=("${BROWN}")
    COLORMAP+=("${BLUE}")
    COLORMAP+=("${PURPLE}")
    COLORMAP+=("${CYAN}")

    NO_COLOR="${NC}"
else
    # we'll access COLORMAP via a modulo operation: no need to use more than one
    # element when everything will be no color.
    COLORMAP+=("")

    NO_COLOR=""
fi

# We need to make the while-loop to ignore SIGINT. To do so, we use 'trap', as explained here:
#   https://unix.stackexchange.com/a/407254/262552
function _run_node()
{
  COUNT_AVAILABLE_COLORS=${#COLORMAP[@]}

  # if there are more nodes than available colors, go around
  COLOR=${COLORMAP[(($1 % $COUNT_AVAILABLE_COLORS))]}

  "${MYDIR}/bitcoind.sh" "${1}" | (
    trap '' INT
    while read line; do
      printf "${COLOR}[node-%02d]${NO_COLOR} %s\n" "${1}" "${line}"
    done;
  )
}


for ((i = 0; i < "${TOT_NODES}"; i++)); do
    _run_node "${i}" &
done

wait
