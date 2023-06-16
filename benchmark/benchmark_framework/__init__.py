# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

import inspect
from pathlib import Path

BENCHMARK_ROOT_PATH = Path(inspect.getfile(inspect.currentframe())).resolve().parent
ITCOIN_FBFT_PATH = (BENCHMARK_ROOT_PATH / ".." / "..").resolve()
ITCOIN_CORE_PATH = (BENCHMARK_ROOT_PATH / ".." / ".." / ".." / "itcoin-core").resolve()
TMUX_SESSION_NAME = 'itcoin-testing-session'
REMOTE_PYTHON = 'python3'