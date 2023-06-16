# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

from os.path import join

from benchmark_framework.utils import NodeType, get_node_dirname
from datetime import date

REMOTE_HOME = "/home/ubuntu"
REMOTE_ITCOIN_FBFT_PATH = REMOTE_HOME + "/itcoin-fbft"
REMOTE_ITCOIN_FBFT_BENCHMARK_PATH = REMOTE_ITCOIN_FBFT_PATH + "/benchmark"

class PathMaker:
    @staticmethod
    def build_path():
        return join('..', 'build')

    @staticmethod
    def logs_path():
        return 'logs'

    @staticmethod
    def output_path():
        return 'datadirs'

    @staticmethod
    def benchmark_path():
        return 'benchmark'

    @staticmethod
    def node_logs_path(node_name):
        return 'logs/' + node_name

    @staticmethod
    def get_local_datadir(node_name: int, node_type: NodeType, nb_nodes: int):
        return PathMaker.output_path() + '/' + get_node_dirname(node_name, node_type, nb_nodes)

    @staticmethod
    def get_remote_datadir(node_name: int, node_type: NodeType, nb_nodes: int):
        return PathMaker.remote_benchmark_path() + '/' + PathMaker.output_path() + '/' + get_node_dirname(node_name, node_type, nb_nodes)

    @staticmethod
    def miner_log_file(i, nb_validators):
        assert isinstance(i, int) and i >= 0
        return PathMaker.node_logs_path(f'{get_node_dirname(i, NodeType.VALIDATOR, nb_validators)}/miner_logs.txt')

    @staticmethod
    def client_log_file(i, nb_clients):
        assert isinstance(i, int) and i >= 0
        return PathMaker.node_logs_path(f'{get_node_dirname(i, NodeType.CLIENT, nb_clients)}/client_logs.txt')

    @staticmethod
    def plots_path():
        return 'plots'

    @staticmethod
    def csv_result_file():
        today = date.today()
        return f'bench-{today.strftime("%d-%m-%Y")}.csv'

    @staticmethod
    def csv_by_height_result_file():
        today = date.today()
        return f'bench-{today.strftime("%d-%m-%Y")}-by-height.csv'

    @staticmethod
    def remote_itcoin_fbft_path():
        return REMOTE_ITCOIN_FBFT_PATH

    @staticmethod
    def remote_benchmark_path():
        return REMOTE_ITCOIN_FBFT_BENCHMARK_PATH

    @staticmethod
    def bitcoin_path():
        return f'{REMOTE_ITCOIN_FBFT_PATH}/build/usrlocal/bin'

    @staticmethod
    def itcoin_core_test_framework_path():
        return f'{REMOTE_ITCOIN_FBFT_PATH}/build/thirdparty/_itcoin-core-external-project-prefix/src/_itcoin-core-external-project/test/functional/test_framework/__init__.py'
