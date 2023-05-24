import inspect
from pathlib import Path

BENCHMARK_ROOT_PATH = Path(inspect.getfile(inspect.currentframe())).resolve().parent
ITCOIN_PBFT_PATH = (BENCHMARK_ROOT_PATH / ".." / "..").resolve()
