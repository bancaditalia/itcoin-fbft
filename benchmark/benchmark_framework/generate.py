"""This module contains functions to generate configuration files for the experiments."""
from pathlib import Path
from typing import List, Optional

from invoke import Context

from benchmark_framework.config import BenchParameters, get_dirname_str
from benchmark_framework.local import LocalBench
from benchmark_framework.remote import RemoteBench
from benchmark_framework.utils import byzantine_replies_quorum, remove_dir_or_fail


NORMAL_OPERATION = "normal-operation"
VIEW_CHANGE = "view-change"


def generate_all_experiments(
    nodes: List[int],
    output_directory: Path,
    clients: int,
    fault_time: float,
    signature_algorithm: str = "UNSET",
    warmup_duration: int = 30,
    rate: int = 10000,
    tx_size: int = 512,
    genesis_hours_in_the_past: float = 0,
    target_block_time: int = 1,
    duration: int = 60,
    runs: int = 1,
    force: bool = False
):
    """Generate all configuration files for reproducing the experiments."""

    assert warmup_duration < fault_time < duration

    # remove and recreate directories
    remove_dir_or_fail(output_directory, force)
    normal_operation = output_directory / NORMAL_OPERATION
    view_change = output_directory / VIEW_CHANGE
    output_directory.mkdir()
    normal_operation.mkdir()
    view_change.mkdir()

    # normal operation configurations
    for n in nodes:
        params = BenchParameters(
            nodes=n,
            clients=clients,
            signature_algorithm=signature_algorithm,
            warmup_duration=warmup_duration,
            rate=rate,
            tx_size=tx_size,
            genesis_hours_in_the_past=genesis_hours_in_the_past,
            target_block_time=target_block_time,
            duration=duration,
            runs=runs,
            faults=0,
            fault_time=None,
        )
        output_filepath = normal_operation / (get_dirname_str(params, max(nodes), 0) + ".json")
        params.print(output_filepath)

    # view-change configurations
    for n in nodes:
        max_faults = byzantine_replies_quorum(n)
        for f in range(1, max_faults):
            params = BenchParameters(
                nodes=n,
                clients=clients,
                signature_algorithm=signature_algorithm,
                warmup_duration=warmup_duration,
                rate=rate,
                tx_size=tx_size,
                genesis_hours_in_the_past=genesis_hours_in_the_past,
                target_block_time=target_block_time,
                duration=duration,
                runs=runs,
                faults=f,
                fault_time=fault_time,
            )
            output_filepath = view_change / (get_dirname_str(params, max(nodes), max_faults) + ".json")
            params.print(output_filepath)


def run_all_experiments(
    config_directory: Path,
    output_directory: Path,
    is_local: bool,
    ctx: Optional[Context] = None
):
    for config_file in sorted(config_directory.glob("*.json"), reverse=True):
        experiment_output_dir = output_directory / config_file.stem
        parameters = BenchParameters.from_file(config_file)
        if is_local:
            bench = LocalBench(parameters, output_directory=experiment_output_dir)
        else:
            assert ctx is not None, "ctx must be set when is_local=False"
            bench = RemoteBench(ctx, parameters, output_directory=experiment_output_dir)
        _results = bench.run()
