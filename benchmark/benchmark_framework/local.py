# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

import shutil
import subprocess
from pathlib import Path
from typing import List

from benchmark_framework import ITCOIN_FBFT_PATH, TMUX_SESSION_NAME
from benchmark_framework.base import Bench
from benchmark_framework.commands import CommandMaker
from benchmark_framework.logs import LogParser, RunResult
from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import NodeType, get_node_dirname
from benchmark_framework.utils import Print, BenchError


class LocalBench(Bench):

    def _kill_nodes(self):
        try:
            cmd = CommandMaker.kill_tmux_session()
            subprocess.run(cmd, stderr=subprocess.DEVNULL, shell=True)
            for i in range(self.bench_parameters.nodes):
                cmd = CommandMaker.kill_bitcoind(i, NodeType.VALIDATOR, self.bench_parameters.nodes).split()
                subprocess.run(cmd, stderr=subprocess.DEVNULL, check=False)
            for i in range(self.bench_parameters.clients):
                cmd = CommandMaker.kill_bitcoind(i, NodeType.CLIENT, self.bench_parameters.nodes).split()
                subprocess.run(cmd, stderr=subprocess.DEVNULL, check=False)
        except subprocess.SubprocessError as e:
            raise BenchError('Failed to kill testbed', e)

    def _background_run(self, name, command, wait=False):
        if wait:
            tmp_file = subprocess.run(['mktemp', '-p', self.tmpdirname.name], capture_output=True, text=True).stdout.strip()
            command = command + '; tmux wait-for -S ' + tmp_file
            subprocess.run(['tmux', 'new-window', '-t', TMUX_SESSION_NAME, '-n', name, command])
            subprocess.run(['tmux', 'wait-for', tmp_file])
            Path.unlink(Path(tmp_file))
        else:
            subprocess.run(['tmux', 'new-window', '-t', TMUX_SESSION_NAME, '-n', name, command])

    def _clean_up(self) -> None:
        """Clean up outputs from previous runs."""
        cmd = f'{CommandMaker.clean_logs()} ; {CommandMaker.clean_output()} ; {CommandMaker.mk_node_log_dirs(self.bench_parameters.nodes, self.bench_parameters.clients)}'
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)

    def _setup(self) -> None:
        # Setup signet
        genesis_block_timestamp = self.bench_parameters.compute_genesis_block_timestamp()
        target_block_time = self.bench_parameters.target_block_time
        nb_clients = self.bench_parameters.clients
        output_directory = PathMaker.output_path()
        cmd = CommandMaker.setup_signet(
            f"--output-directory {output_directory} --itcoin-core-dir {self.itcoin_core_directory} --nb-validators {self.bench_parameters.nodes} "
            f"--nb-clients {nb_clients} --genesis-block-timestamp "
            f"{genesis_block_timestamp} --target-block-time {target_block_time}")
        cmd = f'{cmd} &> {PathMaker.logs_path()}/setup_signet_logs.txt'
        Print.cmd(cmd)
        self._background_run('setup_signet', cmd, wait=True)

    def _preamble(self) -> None:
        cmd = f'{CommandMaker.kill_tmux_session()}'
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)
        cmd = f'{CommandMaker.create_tmux_session()}'
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)

    def _run_validator_bitcoinds(self) -> None:
        """Run itcoin-core for validators."""
        for i in range(self.bench_parameters.nodes):
            cmd = CommandMaker.run_signet(
                f"--bitcoin-path {self.bitcoind_path} --itcoin-core-test-framework-path {self.test_framework_path} "
                f"--id {i} --node-type VALIDATOR --nb-of-type {self.bench_parameters.nodes}")
            Print.cmd(cmd)
            self._background_run(f'run-signet-VALIDATOR-{i}', cmd, wait=True)

    def _get_network_stats(self, run_directory: Path) -> None:
        return

    def _run_validators(self) -> None:
        """Run the replicas."""
        for i in range(self.bench_parameters.nodes):
            fault_info = ''
            if i < self.bench_parameters.faults:
                fault_info = f'--fault-info {self.bench_parameters.faults} {self.bench_parameters.fault_time}'
            cmd = CommandMaker.run_replicas(
                f"--itcoin-fbft-path {ITCOIN_FBFT_PATH} --id {i} --nb-validators {self.bench_parameters.nodes} {fault_info}")
            Print.cmd(cmd)
            self._background_run(f'run-replica-{i}', cmd)

    def _run_client_bitcoinds(self) -> None:
        """Run the clients's Bitcoin nodes."""
        for i in range(self.bench_parameters.clients):
            cmd = CommandMaker.run_signet(
                f"--bitcoin-path {self.bitcoind_path} --itcoin-core-test-framework-path {self.test_framework_path} "
                f"--id {i} --node-type CLIENT --nb-of-type {self.bench_parameters.clients}")
            Print.cmd(cmd)
            self._background_run(f'run-signet-CLIENT-{i}', cmd, wait=True)

    def _get_client_addresses(self) -> List[str]:
        """Get the client addresses."""
        client_addrs: List[str] = []
        for i in range(self.bench_parameters.clients):
            client_name = get_node_dirname(i, NodeType.CLIENT, self.bench_parameters.clients)
            output_dir = Path(PathMaker.output_path()).resolve()
            client_dir = output_dir / client_name
            bitcoin_cli = self.itcoin_core_directory / "src" / "bitcoin-cli"
            from test_framework.test_node import TestNodeCLI
            cli = TestNodeCLI(str(bitcoin_cli), str(client_dir))
            address = cli.getnewaddress()
            client_addrs.append(address)
        return client_addrs

    def _run_donors(self, client_addrs: List[str]) -> None:
        """Run the donors."""
        client_addrs = " --client-address ".join(client_addrs)
        client_addrs = "--client-address " + client_addrs
        for i in range(self.bench_parameters.nodes):
            cmd = CommandMaker.run_donor(
                f'--bitcoin-path {self.bitcoind_path} --itcoin-core-test-framework-path {self.test_framework_path} '
                f'--id {str(i)} --nb-validators {str(self.bench_parameters.nodes)} {client_addrs}')
            Print.cmd(cmd)
            self._background_run(f'run-donor-{i}', cmd)

    def _run_clients(self) -> None:
        """Run the clients."""
        # Run clients
        warmup_duration = self.bench_parameters.warmup_duration
        block_time = self.bench_parameters.target_block_time
        client_rate = self.bench_parameters.rate
        tx_size = self.bench_parameters.tx_size
        for i in range(self.bench_parameters.clients):
            cmd = CommandMaker.run_client(
                f'--bitcoin-path {self.bitcoind_path} --itcoin-core-test-framework-path {self.test_framework_path} '
                f'--id {i} --nb-clients {self.bench_parameters.clients} '
                f'--warmup-duration {warmup_duration} --block-time {block_time} '
                f'--target-tx-rate {client_rate} --tx-size '
                f'{tx_size}')
            Print.cmd(cmd)
            self._background_run(f'run-client-{i}', cmd)

    def _process_logs(self, run_directory: Path) -> RunResult:
        """Process logs and return a RunResult object."""
        Print.info('Parsing logs...')
        self.bench_parameters.print(PathMaker.logs_path() + '/bench_parameters.json')
        shutil.copytree(PathMaker.logs_path(), run_directory, dirs_exist_ok=True)
        return LogParser.process(run_directory, self.output_directory).result()
