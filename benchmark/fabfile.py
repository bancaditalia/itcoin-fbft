from distutils.util import strtobool
from pathlib import Path

from fabric import task

from benchmark_framework.config import BenchParameters
from benchmark_framework.instance import InstanceManager
from benchmark_framework.local import LocalBench
from benchmark_framework.logs import LogParser, ParseError
from benchmark_framework.remote import RemoteBench, install as _install
from benchmark_framework.utils import NodeType, Print, BenchError, remove_dir_or_fail


# POLYFILL FOR PYTHON 3.11 COMPATIBILITY - START
#
# This snippet needs to stay at the beginning of fabfile.py.
#
# This polyfill selectively monkey patches pynvoke making it compatible with
# python 3.11. It is applied only if python version >= 3.11, and is due to
# pyinvoke 1.7.3, one of fabric's 2.7.1 dependencies, relying on a deprecated
# python functionality, that was removed in python 3.11.
#
# Once fabric and its pyinvoke dependency are updated, this snippet can be
# removed.
#
# Modified from: https://github.com/pyinvoke/invoke/issues/833#issuecomment-1312420546
#
def fix_annotations():
    """
    Pyinvoke doesn't accept annotations by default, this fix that
    Based on: @zelo's fix in https://github.com/pyinvoke/invoke/pull/606
    Context in: https://github.com/pyinvoke/invoke/issues/357
        Python 3.11 https://github.com/pyinvoke/invoke/issues/833
    """
    # TODO: use feature detection instead of checking version info
    import sys
    if sys.version_info < (3, 11, 0):
        return
    import unittest.mock
    from collections import namedtuple
    from inspect import getfullargspec
    import invoke
    ArgSpec = namedtuple("ArgSpec", ["args", "defaults"])
    def patched_inspect_getargspec(func):
        spec = getfullargspec(func)
        return ArgSpec(spec.args, spec.defaults)
    org_task_argspec = invoke.tasks.Task.argspec
    def patched_task_argspec(*args, **kwargs):
        with unittest.mock.patch(
            target="inspect.getargspec", new=patched_inspect_getargspec, create=True
        ):
            return org_task_argspec(*args, **kwargs)
    invoke.tasks.Task.argspec = patched_task_argspec

fix_annotations()
# POLYFILL FOR PYTHON 3.11 COMPATIBILITY - END


@task
def local(ctx, config_file, output_directory):
    ''' Run benchmarks on localhost '''
    try:
        benchmark_parameters = BenchParameters.from_file(config_file)
        ret = LocalBench(benchmark_parameters, output_directory=output_directory, skip_build=True).run()
        for run_id, log_result in enumerate(ret):
            print(f"Summary for run {run_id}")
            print(log_result.summary)
    except BenchError as e:
        Print.error(e)


@task
def create(ctx, nodes=1, clients=1):
    ''' Create a testbed'''
    try:
        InstanceManager.make().create_instances(nodes, NodeType.VALIDATOR)
        InstanceManager.make().create_instances(clients, NodeType.CLIENT)
    except BenchError as e:
        Print.error(e)


@task
def destroy(ctx):
    ''' Destroy the testbed '''
    try:
        InstanceManager.make().terminate_instances()
    except BenchError as e:
        Print.error(e)


@task
def start(ctx, max=50):
    ''' Start at most `max` machines per data center '''
    try:
        InstanceManager.make().start_instances(max)
    except BenchError as e:
        Print.error(e)


@task
def stop(ctx):
    ''' Stop all machines '''
    try:
        InstanceManager.make().stop_instances()
    except BenchError as e:
        Print.error(e)


@task
def info(ctx):
    ''' Display connect information about all the available machines '''
    try:
        for nodeType in NodeType:
            InstanceManager.make().print_info(nodeType=nodeType)
    except BenchError as e:
        Print.error(e)


@task
def install(ctx, settings_filepath):
    ''' Install the codebase on all machines '''
    try:
        _install(ctx, settings_filepath)
    except BenchError as e:
        Print.error(e)


@task
def logs(ctx, log_dir, output_dir):
    ''' Print a summary of the logs '''
    try:
        log_dir = Path(log_dir)
        output_dir = Path(output_dir)
        Print.info("Preparing output directories...")
        remove_dir_or_fail(output_dir, force=False)
        output_dir.mkdir()
        result = LogParser.process(log_dir, output_dir).result()
        print(result.summary)
    except ParseError as e:
        Print.error(BenchError('Failed to parse logs', e))


@task
def remote(ctx, config_file, output_directory):
    ''' Run benchmarks on AWS '''
    try:
        benchmark_parameters = BenchParameters.from_file(config_file)
        ret = RemoteBench(ctx, benchmark_parameters, output_directory=output_directory, skip_local_build=True).run()
        for run_id, log_result in enumerate(ret):
            print(f"Summary for run {run_id}")
            print(log_result.summary)
    except BenchError as e:
        Print.error(e)


@task
def run_all(ctx, config_directory, is_local, output_directory):
    """
    Run all the experiments.

    Steps:
    - set up configurations directory, output directory
    - run all the experiments in config_directory
    - print csv results to output_directory

    :param ctx: the Fabric context.
    :param config_directory: the configuration directory created with generate.generate_all_experiments
    :param is_local: if True, the experiment is run in local, if False it is run remotely
    :param output_directory: the directory where to store the results
    """
    is_local = bool(strtobool(is_local))

    # set up output directories
    output_directory = Path(output_directory)
    remove_dir_or_fail(output_directory, False)
    output_directory.mkdir()

    config_directory = Path(config_directory)

    for config_file in sorted(config_directory.glob("*.json"), reverse=True):
        experiment_output_dir = output_directory / config_file.stem
        parameters = BenchParameters.from_file(config_file)
        if is_local:
            bench = LocalBench(parameters, output_directory=experiment_output_dir)
        else:
            assert ctx is not None, "ctx must be set when is_local=False"
            bench = RemoteBench(ctx, parameters, output_directory=experiment_output_dir)
        _results = bench.run()


