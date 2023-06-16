# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

import dataclasses
import time

from typing import Optional, Any

from benchmark_framework import utils
from benchmark_framework.utils import check_int_in_range, BaseParameters, \
    ConfigError, Int, PositiveInt, NonnegativeInt, Float, add_zeros_prefix


DEFAULT_BENCH_PARAMETERS_FILENAME = "bench_parameters.json"
DEFAULT_SUMMARY_FILENAME = "summary.txt"


def _check_faults(instance: Any, value: int) -> None:
    """Check the number of faults."""
    check_int_in_range(value, range(0, instance.nodes + 1))


def _check_fault_info(instance: Any, value: float) -> None:
    """Check fault info are correct."""
    fault_time = value
    if instance.faults > 0 and fault_time is None:
        raise ConfigError(f'missing fault time when faults > 0. got: {instance.faults=}, {fault_time=}')
    if instance.faults == 0 and fault_time is not None:
        raise ConfigError(f'If there are no faults, you cannot give a fault time. got: {fault_time=}')


@dataclasses.dataclass(frozen=True, eq=True)
class BenchParameters(BaseParameters):
    nodes: int = PositiveInt()
    clients: int = PositiveInt()
    warmup_duration: int = NonnegativeInt()
    rate: float = Float()
    tx_size: int = PositiveInt()
    genesis_hours_in_the_past: float = Float()
    target_block_time: int = NonnegativeInt()
    duration: int = NonnegativeInt()
    runs: int = Int(default=1, min_value=1)
    faults: int = Int(default=0, min_value=0, validators=[_check_faults])
    fault_time: Optional[float] = Float(default=None, validators=[_check_fault_info], is_nullable=True)

    def compute_genesis_block_timestamp(self) -> int:
        """
        Given the value of genesis_hours_in_the_past and considering the current
        time, computes the value to assign to genesis_block_timestamp.

        BEWARE: the value returned by this method depends on WHEN it is invoked.
        """
        current_time = int(time.time())
        current_time_delta = 3600 * self.genesis_hours_in_the_past
        genesis_block_timestamp = int(current_time - current_time_delta)
        utils.Print.info(f'{current_time=}')
        utils.Print.info(f'{current_time_delta=}')
        utils.Print.info(f'{genesis_block_timestamp=}')
        return genesis_block_timestamp


def get_dirname_str(bench_param: BenchParameters, max_nodes: int, max_faults: int) -> str:
    nodes_str = add_zeros_prefix(bench_param.nodes, max_nodes)
    faults_str = add_zeros_prefix(bench_param.faults, max_faults)
    return f"{nodes_str}-{faults_str}-{bench_param.rate}-{bench_param.tx_size}"
