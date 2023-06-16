# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

from pathlib import Path
from benchmark_framework import TMUX_SESSION_NAME

from invoke import Context
from joblib import Parallel, delayed, cpu_count

from benchmark_framework.base import Bench, DEFAULT_ITCOIN_CORE_PATH, DEFAULT_OUTPUT_PATH
from benchmark_framework.commands import CommandMaker
from benchmark_framework.config import BenchParameters
from benchmark_framework.instance import InstanceManager
from benchmark_framework.paths import PathMaker
from benchmark_framework.logs import LogParser, RunResult
from benchmark_framework.utils import BenchError, NodeType, Print, get_node_dirname, progress_bar
from fabric import Connection, ThreadingGroup as Group
from fabric.exceptions import GroupException
import json
from paramiko import RSAKey
from paramiko.ssh_exception import PasswordRequiredException, SSHException
import patchwork.transfers
import shutil
import subprocess
from time import time, sleep
from typing import Dict, Callable, List, Any, Sequence, Tuple


class FabricError(Exception):
    ''' Wrapper for Fabric exception with a meaningfull error message. '''

    def __init__(self, error):
        assert isinstance(error, GroupException)
        message = list(error.result.values())[-1]
        super().__init__(message)


class ExecutionError(Exception):
    pass


def install(ctx: Context, path_to_settings: Path = Path("settings.json")):
    Print.info('Installing dependencies, cloning and building the repo...')
    manager = InstanceManager.make(str(path_to_settings))
    settings = manager.settings
    try:
        ctx.connect_kwargs.pkey = RSAKey.from_private_key_file(
            str(Path(settings.key_path).expanduser())
        )
    except (IOError, PasswordRequiredException, SSHException) as e:
        raise BenchError('Failed to load SSH key', e)

    cmd = [
        'sudo apt update',
        'sudo apt -y upgrade',
        'sudo apt -y autoremove',

        # Clean git config (uncomment to perform a deep reset of the repo)
        # f'rm -rf ~/{settings.repo_name}',
        # Clone or fetch the repo
        f'git clone {settings.repo_url} || (cd {settings.repo_name} ; git fetch --all ; cd ..)',

        # Install itcoin-fbft build dependencies
        "sudo apt install --no-install-recommends -y \
            autoconf \
            automake \
            bsdextrautils \
            ca-certificates \
            cmake \
            g++ \
            gcc \
            git \
            jq \
            libtool \
            make \
            openssh-client \
            pkg-config \
            python3 \
            python3-pip \
            tmux \
            xxd \
            zlib1g-dev",

        "sudo apt install --no-install-recommends -y \
            libargtable2-dev \
            libboost-filesystem1.74-dev \
            libboost-log1.74-dev \
            libboost-program-options1.74-dev \
            libboost-test1.74-dev \
            libboost-thread1.74-dev \
            libcurl4-openssl-dev \
            libdb5.3++-dev \
            libevent-dev \
            libsqlite3-dev \
            libssl-dev \
            libzmq3-dev \
            swi-prolog",

        # zmq needed in client
        "pip install zmq",

        # Build the project
        f"cd {settings.repo_name}",
        f'git checkout -f {settings.branch}',
        f'git reset --hard origin/{settings.branch}',
        'mkdir -p build',
        'cd build',
        f"cmake {settings.cmake_options} ..",
        'make'

    ]
    hosts = manager.hosts(flat=True)
    try:
        g = Group(*hosts, user='ubuntu', connect_kwargs=ctx.connect_kwargs)
        g.run(' && '.join(cmd), hide=False)
        Print.heading(f'Initialized testbed of {len(hosts)} nodes')
    except (GroupException, ExecutionError) as e:
        e = FabricError(e) if isinstance(e, GroupException) else e
        raise BenchError('Failed to install repo on testbed', e)


class RemoteBench(Bench):

    def __init__(self,
                 ctx: Context,
                 benchmark_parameters: BenchParameters,
                 itcoin_core_directory: Path = DEFAULT_ITCOIN_CORE_PATH,
                 output_directory: Path = DEFAULT_OUTPUT_PATH,
                 skip_local_build: bool = True,
                 nb_jobs_for_upload: int = cpu_count()):
        super().__init__(benchmark_parameters, itcoin_core_directory, output_directory, skip_local_build)
        self.manager = InstanceManager.make()
        self.settings = self.manager.settings
        self.skip_local_build = skip_local_build
        self.nb_jobs_for_upload = nb_jobs_for_upload
        try:
            ctx.connect_kwargs.pkey = RSAKey.from_private_key_file(
                str(Path(self.manager.settings.key_path).expanduser())
            )
            ctx.connect_kwargs.timeout = 1000
            ctx.connect_kwargs.banner_timeout = 1000
            ctx.connect_kwargs.auth_timeout = 1000
            self.connect = ctx.connect_kwargs
        except (IOError, PasswordRequiredException, SSHException) as e:
            raise BenchError('Failed to load SSH key', e)

        # Select which hosts to use.
        self.selected_validators, self.selected_clients = self._select_hosts()
        if not self.selected_validators or not self.selected_clients:
            Print.warn('There are not enough instances available')
            return

        self.validators_ip_to_node_id: Dict[str, int] = {host: node_id for node_id, host in
                                                         enumerate(self.selected_validators)}
        self.clients_ip_to_node_id: Dict[str, int] = {host: node_id for node_id, host in
                                                      enumerate(self.selected_clients)}

    @property
    def hosts(self) -> List:
        """Get all the hosts."""
        return self.selected_validators + self.selected_clients

    def _run_in_parallel(self, func: Callable, args_list: Sequence[Sequence[Any]]) -> List[Any]:
        """
        Run tasks in parallel (multi-threading).

        :param func: the function to execute
        :param args_list: the list of arguments
        :return: The list of results
        """
        return Parallel(n_jobs=self.nb_jobs_for_upload, backend="threading")(
            delayed(func)(*args) for args in args_list
        )

    def _select_hosts(self) -> Tuple[List[str], List[str]]:
        nodes = self.bench_parameters.nodes
        clients = self.bench_parameters.clients

        # Ensure there are enough hosts.
        validators_hosts = self.manager.hosts(nodeType=NodeType.VALIDATOR)
        clients_hosts = self.manager.hosts(nodeType=NodeType.CLIENT)
        if sum(len(x) for x in validators_hosts.values()) < nodes or sum(
                len(x) for x in clients_hosts.values()) < clients:
            return [], []

        # Select the hosts in different data centers.
        ordered_validators = zip(*validators_hosts.values())
        ordered_validators = [x for y in ordered_validators for x in y]
        ordered_clients = zip(*clients_hosts.values())
        ordered_clients = [x for y in ordered_clients for x in y]
        return ordered_validators[:nodes], ordered_clients[:clients]

    def upload_datadirs(self, hosts, nodeType: NodeType):
        # Upload configuration files to EC2 instances.
        Print.info(f'Uploading datadir folder to EC2 {nodeType.value}...')
        self._run_in_parallel(self._upload_datadir, [(hosts[i], i, nodeType, len(hosts)) for i in range(len(hosts))])
        return

    def _upload_datadir(self, host_ip_addr: str, node_id: int, node_type: NodeType, nb_hosts: int):
        ssh_opts = '-i ' + self.manager.settings.key_path + ' -oStrictHostKeyChecking=no'
        c = Connection(host_ip_addr, user='ubuntu', connect_kwargs=self.connect)
        src = PathMaker.output_path() + '/' + get_node_dirname(node_id, node_type, nb_hosts)
        target = PathMaker.remote_benchmark_path() + '/' + PathMaker.output_path()
        patchwork.transfers.rsync(c, src, target, ssh_opts=ssh_opts, rsync_opts="--quiet")
        print(f"Upload for {node_type.value} {node_id} (IP address: {host_ip_addr}) done!")

    def _clean_up(self) -> None:
        """Clean up outputs from previous runs."""
        hosts = self.selected_validators + self.selected_clients
        # Cleanup LOCAL log and output folder
        Print.info('Cleaning LOCAL log and output folder...')
        cmd = f'{CommandMaker.clean_logs()} ; {CommandMaker.clean_output()} ; {CommandMaker.mk_log_dir()}'
        Print.cmd(cmd)
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)

        # Cleanup all REMOTE nodes.
        Print.info('Cleaning REMOTE log and output folder...')
        cmd = f'cd {PathMaker.remote_benchmark_path()}; {CommandMaker.clean_logs()} ; {CommandMaker.clean_output()} ; ' \
              f'{CommandMaker.mk_log_dir()}'
        Print.cmd(cmd)
        g = Group(*hosts, user='ubuntu', connect_kwargs=self.connect)
        g.run(cmd, hide=True)

    def _run_validator_bitcoinds(self) -> None:
        """Run itcoin-core for validators."""
        self._run_bitcoinds(self.validators_ip_to_node_id, NodeType.VALIDATOR)
        sleep(6)

    def _run_validators(self) -> None:
        """Run the replicas."""
        # Run miner in every EC2 instance.
        Print.info('Running miners...')
        self._run_in_parallel(self._run_miner,
                              [(node_id,) for ip_addr, node_id in self.validators_ip_to_node_id.items()])

    def _run_client_bitcoinds(self) -> None:
        """Run the clients's Bitcoin nodes."""
        self._run_bitcoinds(self.clients_ip_to_node_id, NodeType.CLIENT)
        sleep(6)

    def _get_client_addresses(self) -> List[str]:
        """Get the client addresses."""
        # Gather a new addresses for each client
        Print.info('Getting new addresses from clients...')
        client_addrs = []
        for host in self.clients_ip_to_node_id.keys():
            datadir = PathMaker.get_remote_datadir(self.clients_ip_to_node_id[host], NodeType.CLIENT,
                                                   self.bench_parameters.clients)
            cmd = f"{PathMaker.bitcoin_path() + '/bitcoin-cli'} -datadir={datadir} getnewaddress"
            Print.cmd(cmd)
            cnx = Connection(host, user='ubuntu', connect_kwargs=self.connect)
            result = cnx.run(cmd)
            client_addrs.append(result.stdout.rstrip())

        return client_addrs

    def kill(self, hosts=[], delete_logs=False):
        assert isinstance(hosts, list)
        assert isinstance(delete_logs, bool)
        hosts = hosts if hosts else self.manager.hosts(flat=True)
        delete_logs = CommandMaker.clean_logs() if delete_logs else 'true'

        # Kill all the remote TMUX sessions
        cmd = [delete_logs, f'({CommandMaker.kill_tmux_session()} || true)']
        Print.info('Killing all itcoin-fbft processes...')
        try:
            Print.cmd(cmd)
            g = Group(*hosts, user='ubuntu', connect_kwargs=self.connect)
            g.run(' && '.join(cmd), hide=True)
        except GroupException as e:
            raise BenchError('Failed to kill nodes', FabricError(e))

        # Gracefully stop all the remote bitcoind processes
        # Both in VALIDATORS
        nb_validator_nodes = len(self.validators_ip_to_node_id)
        self._run_in_parallel(self._stop_bitcoind,
                              [(ip_addr, node_id, nb_validator_nodes, NodeType.VALIDATOR) for ip_addr, node_id in self.validators_ip_to_node_id.items()])
        # and in CLIENTS
        nb_client_nodes = len(self.clients_ip_to_node_id)
        self._run_in_parallel(self._stop_bitcoind,
                              [(ip_addr, node_id, nb_client_nodes, NodeType.CLIENT) for ip_addr, node_id in self.clients_ip_to_node_id.items()])

        # Kill local TMUX session
        subprocess.run(CommandMaker.kill_tmux_session(), stderr=subprocess.DEVNULL, shell=True)

    def _stop_bitcoind(self, ip_addr, node_id, nb_nodes, node_type: NodeType):
        cmd = f'{CommandMaker.kill_bitcoind(node_id, node_type, nb_nodes, run_remotely=True)}'
        Print.cmd(cmd)
        cnx = Connection(ip_addr, user='ubuntu', connect_kwargs=self.connect)
        cnx.run(cmd, asynchronous=True)
        return

    def _run_bitcoind(self, ip_addr, node_id, nb_nodes, node_type: NodeType):
        node_type_str = node_type.value.upper()
        cmd = f'cd {PathMaker.remote_benchmark_path()} ; '
        run_signet_cmd = f'{CommandMaker.run_signet(f"--bitcoin-path {PathMaker.bitcoin_path()} --itcoin-core-test-framework-path {PathMaker.itcoin_core_test_framework_path()} --id {node_id} --node-type {node_type_str} --nb-of-type {str(nb_nodes)}", run_remotely=True)}'
        run_signet_cmd = self._remote_background_run_cmd(f'run-signet-{node_type.value}-{node_id}', run_signet_cmd)
        cmd = cmd + run_signet_cmd
        Print.cmd(cmd)
        cnx = Connection(ip_addr, user='ubuntu', connect_kwargs=self.connect)
        cnx.run(cmd, asynchronous=True)

    def _run_miner(self, node_id):
        fault_info = ''
        nb_nodes = self.bench_parameters.nodes
        nb_faults, fault_time = self.bench_parameters.faults, self.bench_parameters.fault_time
        ip_addr = self.selected_validators[node_id]
        if node_id < nb_faults:
            fault_info = f'--fault-info {nb_faults} {fault_time}'
        cmd = f'cd {PathMaker.remote_benchmark_path()} ; '
        run_miner_cmd = f'{CommandMaker.run_replicas(f"--itcoin-fbft-path {PathMaker.remote_itcoin_fbft_path()} --id {node_id} --nb-validators {str(nb_nodes)} {fault_info}", run_remotely=True)}'
        run_miner_cmd = self._remote_background_run_cmd(f'run-replica-{node_id}', run_miner_cmd)
        cmd = cmd + run_miner_cmd
        Print.cmd(cmd)
        cnx = Connection(ip_addr, user='ubuntu', connect_kwargs=self.connect)
        cnx.run(cmd, asynchronous=True)

    def _get_clients_addresses(self):
        sleep(3)

        # Gather a new addresses for each client
        Print.info('Getting new addresses from clients...')
        client_addrs = []
        for host in self.clients_ip_to_node_id.keys():
            datadir = PathMaker.get_remote_datadir(self.clients_ip_to_node_id[host], NodeType.CLIENT,
                                                   self.bench_parameters.clients)
            cmd = f"{PathMaker.bitcoin_path() + '/bitcoin-cli'} -datadir={datadir} getnewaddress"
            Print.cmd(cmd)
            cnx = Connection(host, user='ubuntu', connect_kwargs=self.connect)
            result = cnx.run(cmd)
            client_addrs.append(result.stdout.rstrip())

        return client_addrs

    def _run_donor(self, node_id, client_addrs):
        nb_validators = self.bench_parameters.nodes
        ip_addr = self.selected_validators[node_id]
        client_addrs = " --client-address ".join(client_addrs)
        client_addrs = "--client-address " + client_addrs
        cnx = Connection(ip_addr, user='ubuntu', connect_kwargs=self.connect)
        cmd = f'cd {PathMaker.remote_benchmark_path()} ; '
        run_donor_cmd = f'{CommandMaker.run_donor(f"--bitcoin-path {PathMaker.bitcoin_path()} --itcoin-core-test-framework-path {PathMaker.itcoin_core_test_framework_path()} --id {node_id} --nb-validators {str(nb_validators)} {client_addrs}", run_remotely=True)}'
        run_donor_cmd = self._remote_background_run_cmd(f'run-donor-{node_id}', run_donor_cmd)
        cmd = cmd + run_donor_cmd
        Print.cmd(cmd)
        cnx.run(cmd, asynchronous=True)

    def _run_client(self, node_id):
        nb_clients = self.bench_parameters.clients
        warmup_duration = self.bench_parameters.warmup_duration
        block_time = self.bench_parameters.target_block_time
        client_rate, tx_size = self.bench_parameters.rate, self.bench_parameters.tx_size
        ip_addr = self.selected_clients[node_id]
        cnx = Connection(ip_addr, user='ubuntu', connect_kwargs=self.connect)
        cmd = f'cd {PathMaker.remote_benchmark_path()} ; '
        run_client_cmd = f'{CommandMaker.run_client(f"--bitcoin-path {PathMaker.bitcoin_path()} --itcoin-core-test-framework-path {PathMaker.itcoin_core_test_framework_path()} --id {node_id} --nb-clients {nb_clients} --warmup-duration {warmup_duration} --block-time {block_time} --target-tx-rate {client_rate} --tx-size {tx_size}", run_remotely=True)}'
        run_client_cmd = self._remote_background_run_cmd(f'run-client-{node_id}', run_client_cmd)
        cmd = cmd + run_client_cmd
        Print.cmd(cmd)
        cnx.run(cmd, asynchronous=True)

    def _run_bitcoinds(self, ip_to_node_id, node_type: NodeType):
        # Run bitcoind in every EC2 instance.
        Print.info('Running bitcoind...')
        nb_nodes = len(ip_to_node_id)
        self._run_in_parallel(self._run_bitcoind,
                              [(ip_addr, node_id, nb_nodes, node_type) for ip_addr, node_id in ip_to_node_id.items()])

    def _run_miners(self, ip_to_node_id, nb_faults, fault_time):
        # Run miner in every EC2 instance.
        Print.info('Running miners...')
        nb_nodes = len(ip_to_node_id)
        self._run_in_parallel(self._run_miner,
                              [(ip_addr, node_id, nb_nodes, nb_faults, fault_time) for ip_addr, node_id in
                               ip_to_node_id.items()])

    def _run_donors(self, client_addrs):
        self._run_in_parallel(self._run_donor,
                              [(host, client_addrs) for ip_addr, host in self.validators_ip_to_node_id.items()])

    def _run_clients(self):
        self._run_in_parallel(self._run_client, [(node_id,) for ip_addr, node_id in self.clients_ip_to_node_id.items()])

    def _process_logs(self, run_directory: Path) -> RunResult:
        # Delete LOCAL logs (if any).
        Print.heading('Starting log processing...')
        Print.info('Cleaning LOCAL log folder')
        cmd = f'{CommandMaker.clean_logs()} ; {CommandMaker.mk_log_dir()} ; {CommandMaker.mk_node_log_dirs(self.bench_parameters.nodes, self.bench_parameters.clients)}'
        Print.cmd(cmd)
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)

        self.bench_parameters.print(PathMaker.logs_path() + '/bench_parameters.json')

        # Download REMOTE log files.
        Print.info('Downloading log files from validators.')
        progress = progress_bar(self.selected_validators, prefix='Downloading logs:')
        for i, host in enumerate(progress):
            c = Connection(host, user='ubuntu', connect_kwargs=self.connect)
            c.get(PathMaker.remote_benchmark_path() + '/' + PathMaker.miner_log_file(i, self.bench_parameters.nodes),
                  local=PathMaker.miner_log_file(i, self.bench_parameters.nodes))

        Print.info('Downloading log files from clients.')
        progress = progress_bar(self.selected_clients, prefix='Downloading logs:')
        for i, host in enumerate(progress):
            c = Connection(host, user='ubuntu', connect_kwargs=self.connect)
            c.get(PathMaker.remote_benchmark_path() + '/' + PathMaker.client_log_file(i, self.bench_parameters.clients),
                  local=PathMaker.client_log_file(i, self.bench_parameters.clients))

        # Parse logs and return the parser.
        shutil.copytree(PathMaker.logs_path(), run_directory, dirs_exist_ok=True)
        Print.info('Parsing logs and computing performance...')
        return LogParser.process(run_directory, self.output_directory).result()

    def _kill_nodes(self) -> None:
        Print.heading('Killing itcoin-fbft processes on selected hosts')
        self.kill(hosts=self.hosts)

    def _preamble(self) -> None:
        """Do preliminary steps before starting the runs."""
        # Kill and re-create TMUX session, both locally (needed for setup_signet) and remotely
        g = Group(*self.hosts, user='ubuntu', connect_kwargs=self.connect)
        cmd = f'{CommandMaker.kill_tmux_session()}'
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)
        g.run(cmd, hide=True, warn=True)
        cmd = f'{CommandMaker.create_tmux_session()}'
        subprocess.run([cmd], shell=True, stderr=subprocess.DEVNULL)
        g.run(cmd, hide=True)
        try:
            Print.info(
                'Recompile locally the latest code if needed... ' + ' Skip local build: ' + str(self.skip_local_build))
            if not self.skip_local_build:
                options = '-DITCOIN_CORE_SRC_DIR=../../itcoin-core ..'
                cmd = CommandMaker.cmake(options).split()
                Print.cmd(cmd)
                subprocess.run(cmd, check=True, cwd=PathMaker.build_path())
                cmd = CommandMaker.compile('-j4').split()
                Print.cmd(cmd)
                subprocess.run(cmd, check=True, cwd=PathMaker.build_path())

            Print.info(
                f'Updating {len(self.hosts)} nodes (branch "{self.settings.branch}")...'
            )
            cmd = [
                f'cd {self.settings.repo_name}',
                f'git fetch -f',
                f'git checkout -f {self.settings.branch}',
                f'git reset --hard origin/{self.settings.branch}',
                f'git pull -f',
                # f'rm -rf build', # Enable when a full rebuild is needed
                # f'mkdir -p build', # Enable when a full rebuild is needed
                f'cd build',
                f'{CommandMaker.cmake(self.settings.cmake_options)}',
                # f'make -j4', # Enable when a full rebuild is needed
                f'make -j4 generated_specs/fast',
                f'make -j4 generated_engine/fast',
                f'make -j4 itcoin-fbft/fast',
                f'make -j4 main/fast'
            ]
            Print.cmd(cmd)
            g = Group(*self.hosts, user='ubuntu', connect_kwargs=self.connect)
            g.run(' && '.join(cmd), hide=True)
        except (GroupException, ExecutionError) as e:
            e = FabricError(e) if isinstance(e, GroupException) else e
            raise BenchError('Failed to update nodes', e)

    def _get_network_stats(self, run_directory: Path):
        Print.info('Getting network ping statistics...')
        cmd = f'for IP in {{{",".join(self.hosts)}}}; do ping -w 10 -c 5 -q -n $IP ; done'
        Print.cmd(cmd)
        g = Group(*self.hosts, user='ubuntu', connect_kwargs=self.connect)
        result = g.run(cmd, hide=True)
        filename = run_directory / 'network_stats.txt'
        filename.parent.mkdir(exist_ok=True, parents=True)
        with open(filename, 'w') as fd:
            for cnx in result.succeeded:
                fd.write(f'Ping results for host: {cnx.host}')
                fd.write(result.succeeded[cnx].stdout)
        return

    def _local_background_run(self, name, command, wait=False):
        if wait:
            tmp_file = subprocess.run(['mktemp', '-p', self.tmpdirname.name], capture_output=True, text=True).stdout.strip()
            command = command + '; tmux wait-for -S ' + tmp_file
            subprocess.run(['tmux', 'new-window', '-t', TMUX_SESSION_NAME, '-n', name, command])
            subprocess.run(['tmux', 'wait-for', tmp_file])
            Path.unlink(Path(tmp_file))
        else:
            subprocess.run(['tmux', 'new-window', '-t', TMUX_SESSION_NAME, '-n', name, command])

    def _remote_background_run_cmd(self, name, command):
        cmd = f'tmux new-window -t {TMUX_SESSION_NAME} -n {name} {command}'
        return cmd

    def _setup(self) -> None:
        """Set up the node directories."""
        try:
            # Setup signet locally (s.t. I generate the config files I need and then I upload them in the EC2
            # instances).
            genesis_block_timestamp = self.bench_parameters.compute_genesis_block_timestamp()
            target_block_time = self.bench_parameters.target_block_time
            Print.info('Setting up signet locally...')
            node_id_to_ip: Dict[int, str] = {node_id: self.hosts[node_id] for node_id in
                                             range(self.bench_parameters.nodes + self.bench_parameters.clients)}
            json_node_id_to_ip = json.dumps(json.dumps(list(node_id_to_ip.items()), separators=(',', ':')))
            output_directory = PathMaker.output_path()
            cmd = CommandMaker.setup_signet(
                f"--output-directory {output_directory} --itcoin-core-dir {self.itcoin_core_directory} --nb-validators "
                f"{self.bench_parameters.nodes} --nb-clients {self.bench_parameters.clients} --genesis-block-timestamp {genesis_block_timestamp} "
                f"--target-block-time {target_block_time} --remote-config {json_node_id_to_ip}")
            setup_signet_log_file = PathMaker.logs_path() + '/setup_signet_logs.txt'
            cmd = f'{cmd} &> {setup_signet_log_file}'
            Print.cmd(cmd)
            Print.info('Writing logs to ' + setup_signet_log_file)
            self._local_background_run('setup_signet', cmd, wait=True)

            # Upload configuration files to EC2 instances.
            self.upload_datadirs(self.selected_validators, NodeType.VALIDATOR)
            self.upload_datadirs(self.selected_clients, NodeType.CLIENT)

        except (subprocess.SubprocessError, GroupException) as e:
            e = FabricError(e) if isinstance(e, GroupException) else e
            Print.error(BenchError('Failed to configure nodes', e))
            raise e