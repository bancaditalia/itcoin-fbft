# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

"""This module contains base classes."""
import subprocess
import types
from abc import ABC, abstractmethod
from pathlib import Path
from time import sleep
from typing import List

from benchmark_framework import BENCHMARK_ROOT_PATH
from benchmark_framework.commands import CommandMaker
from benchmark_framework.config import BenchParameters
from benchmark_framework.logs import RunResult, ParseError
from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import Print, import_module_by_abspath, BenchError, \
    remove_dir_or_fail, add_zeros_prefix, PathLike
import tempfile

DEFAULT_ITCOIN_CORE_PATH = (BENCHMARK_ROOT_PATH / ".." / ".." / ".." / "itcoin-core").resolve()
DEFAULT_OUTPUT_PATH = BENCHMARK_ROOT_PATH / "experiment-output"
DEFAULT_SLEEP_TIME = 10


class Bench(ABC):
    """A base class for an experiment."""

    def __init__(self,
                 bench_parameters: BenchParameters,
                 itcoin_core_directory: PathLike = DEFAULT_ITCOIN_CORE_PATH,
                 output_directory: PathLike = DEFAULT_OUTPUT_PATH,
                 skip_build: bool = True,
                 force: bool = False) -> None:
        """
        Initialize a benchmark object.

        :param bench_parameters: the benchmarking parameters
        :param itcoin_core_directory: the ItCoin core directory
        :param output_directory: the output directory
        :param skip_build: whether the local build of itcoin-fbft should be skipped or not.
        :param force: whether the output directory should be removed without asking for confirmation

        """
        self.bench_parameters = bench_parameters
        self.itcoin_core_directory = Path(itcoin_core_directory).resolve()
        self.output_directory = Path(output_directory).resolve()
        self.skip_build = skip_build
        self.force = force

        self._test_framework_module = import_module_by_abspath(self.test_framework_path, "test_framework")
        self.tmpdirname = tempfile.TemporaryDirectory(dir=".")

    @property
    def test_framework_path(self) -> Path:
        """Get the test framework path."""
        return self.itcoin_core_directory / "test" / "functional" / "test_framework" / "__init__.py"

    @property
    def test_framework_module(self) -> types.ModuleType:
        """Get the test framework Python module."""
        return self._test_framework_module

    @property
    def bitcoind_path(self) -> Path:
        """Get the bitcoind path."""
        return self.itcoin_core_directory / "src"

    def run(self) -> List[RunResult]:
        Print.heading(
            f'\nRunning {self.bench_parameters.nodes} VALIDATOR nodes (input rate: '
            f'{self.bench_parameters.rate:,} tx/s)')

        Print.info("Preparing output directories...")
        remove_dir_or_fail(self.output_directory, force=self.force)
        self.output_directory.mkdir()

        Print.heading('Starting benchmark')
        self._kill_nodes()
        self._clean_up()

        self._preamble()

        results: List[RunResult] = []
        for i in range(self.bench_parameters.runs):

            try:
                Print.heading(f'Run {i + 1}/{self.bench_parameters.runs}')
                run_directory = self.output_directory / add_zeros_prefix(i, self.bench_parameters.runs)

                self._clean_up()
                self._setup()
                # self._get_network_stats(run_directory)
                self._run_validator_bitcoinds()
                self._run_validators()
                self._run_client_bitcoinds()
                client_addrs = self._get_client_addresses()
                self._run_donors(client_addrs)
                self._run_clients()

                self._wait_for_duration()

                self._kill_nodes()
                self.tmpdirname.cleanup()

                result = self._process_logs(run_directory)
                result.print(str(run_directory / 'results.txt'))
                results.append(result)

            except (subprocess.SubprocessError, ParseError) as e:
                self._kill_nodes()
                self.tmpdirname.cleanup()
                raise BenchError('Failed to run benchmark', e)

        return results

    def _preamble(self) -> None:
        """Do preliminary steps before starting the runs (default: no-op)."""

    @abstractmethod
    def _kill_nodes(self) -> None:
        """Kill all nodes."""

    @abstractmethod
    def _clean_up(self) -> None:
        """Clean up outputs from previous runs."""

    @abstractmethod
    def _setup(self) -> None:
        """Set up the node directories."""

    @abstractmethod
    def _get_network_stats(self, directory: Path) -> None:
        """Get ping statistics among instances."""

    @abstractmethod
    def _run_validator_bitcoinds(self) -> None:
        """Run the validators."""

    @abstractmethod
    def _run_validators(self) -> None:
        """Run the replicas."""

    @abstractmethod
    def _run_client_bitcoinds(self) -> None:
        """Run the clients."""

    @abstractmethod
    def _get_client_addresses(self) -> List[str]:
        """Get the client addresses."""

    @abstractmethod
    def _run_donors(self, client_addrs: List[str]) -> None:
        """Run the donors."""

    @abstractmethod
    def _run_clients(self) -> None:
        """Run the clients."""

    @abstractmethod
    def _process_logs(self, directory: Path) -> RunResult:
        """Process logs and return a RunResult object."""

    def _build_local(self) -> None:
        """Recompile the latest code if needed."""
        if not self.skip_build:
            options = '-DITCOIN_CORE_SRC_DIR=../../itcoin-core'
            cmd = CommandMaker.cmake(options).split()
            subprocess.run(cmd, check=True, cwd=PathMaker.build_path())
            options = '-j4'
            cmd = CommandMaker.compile(options).split()
            subprocess.run(cmd, check=True, cwd=PathMaker.build_path())

    def _wait_for_duration(self) -> None:
        """Wait for a number of seconds specified by the 'duration' configuration."""
        Print.info(f'READY! Start measuring for {self.bench_parameters.duration} sec ...')

        total_sleep_time = 0
        while total_sleep_time < self.bench_parameters.duration:
            nb_seconds_to_sleep = min(DEFAULT_SLEEP_TIME, self.bench_parameters.duration - total_sleep_time)
            sleep(nb_seconds_to_sleep)
            total_sleep_time += nb_seconds_to_sleep
            Print.info(
                f'Slept for {total_sleep_time} sec, test completed at '
                f'{100 * total_sleep_time / self.bench_parameters.duration:.0f}% ...')
