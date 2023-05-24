import sys
from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import get_node_dirname, NodeType


class CommandMaker:

    @staticmethod
    def cmake(options):
        return f'cmake {options} ..'

    @staticmethod
    def compile(options):
        return f'make {options}'

    @staticmethod
    def clean_logs():
        return f'rm -fr {PathMaker.logs_path()}'

    @staticmethod
    def clean_results():
        return f'rm -fr {PathMaker.results_path()} ; mkdir -p {PathMaker.results_path()}'

    @staticmethod
    def clean_output():
        return f'rm -fr {PathMaker.output_path()}'

    @staticmethod
    def clean_plots():
        return f'rm -fr {PathMaker.plots_path()}'

    @staticmethod
    def mk_node_log_dirs(nodes, clients):
        mk_validators_log_dirs = '; '.join([CommandMaker.mk_node_log_dir(get_node_dirname(index, NodeType.VALIDATOR, nodes)) for index in range(nodes)])
        mk_clients_log_dirs = '; '.join([CommandMaker.mk_node_log_dir(get_node_dirname(index, NodeType.CLIENT, clients)) for index in range(clients)])
        return f'{mk_validators_log_dirs}; {mk_clients_log_dirs}'

    @staticmethod
    def mk_log_dir():
        return f'mkdir -p {PathMaker.logs_path()}'

    @staticmethod
    def setup_signet(options):
        return sys.executable + ' ' + f'./setup-signet.py {options}'

    @staticmethod
    def run_signet(options, run_remotely=False):
        path = PathMaker.remote_benchmark_path() if run_remotely else sys.executable + ' .'
        return path + f'/run-signet.py {options}'

    @staticmethod
    def run_donor(options, run_remotely=False):
        path = PathMaker.remote_benchmark_path() if run_remotely else sys.executable + ' .'
        return path + f'/run-donor.py {options}'

    @staticmethod
    def run_client(options, run_remotely=False):
        path = PathMaker.remote_benchmark_path() if run_remotely else sys.executable + ' .'
        return path + f'/run-client.py {options}'

    @staticmethod
    def run_replicas(options, run_remotely=False):
        path = PathMaker.remote_benchmark_path() if run_remotely else sys.executable + ' .'
        return path + f'/run-replicas.py {options}'

    @staticmethod
    def mk_node_log_dir(node_name):
        return f"mkdir -p {PathMaker.logs_path() + '/' + node_name}"

    @staticmethod
    def kill():
        return "kill -9 `ps aux | grep -E '[i]tcoin-pbft|[r]un-client|[r]un-donor'| awk '{print $2}'`"