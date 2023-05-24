#!/usr/bin/env python3
import subprocess
from pathlib import Path

import click
import os

from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import NodeType, get_node_dirname, get_logger
from time import sleep
from re import search

@click.command("run-replicas")
@click.option(
    "--itcoin-pbft-path",
    type=click.Path(exists=False, file_okay=False, dir_okay=True),
    required=True,
    help="Path to itcoin-pbft.",
)
@click.option(
    "--id",
    type=int,
    required=True,
    help="Id of the replica.",
)
@click.option(
    "--nb-validators",
    type=click.IntRange(min=1),
    required=True,
    help="Number of validators in the network.",
)
@click.option(
    "--fault-info",
    default=[None] * 2,
    type=click.Tuple([int, float]),
    help="Total number of faults and number of seconds after which miner needs to be killed",
)
def main(itcoin_pbft_path, id: int, nb_validators: int, fault_info) -> None:
    """Run a network of miners nodes."""
    node_name = get_node_dirname(id, NodeType.VALIDATOR, nb_validators)
    filename = PathMaker.node_logs_path(node_name) + '/miner_logs.txt'
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    fault_info_provided = None not in fault_info
    if fault_info_provided:
        nb_faults, fault_time = fault_info
        print(f"{fault_info_provided=}, {fault_info=}, {nb_faults=}, {fault_time=}")

    with open(filename, 'w') as fd:
        pbftd = (Path(itcoin_pbft_path) / "build" / "apps" / "main").resolve().absolute()
        lib_dir = (Path(itcoin_pbft_path) / "build" / "usrlocal" / "lib").resolve().absolute()

        output_dir = Path(itcoin_pbft_path + '/' + PathMaker.benchmark_path() + '/' + PathMaker.output_path())
        datadir = output_dir / node_name
        args = [pbftd, f"-datadir={datadir}"]
        process = subprocess.Popen(
            args,
            cwd=datadir,
            env=dict(LD_LIBRARY_PATH=str(lib_dir)),
            stdout=fd, stderr=fd
        )
        if fault_info_provided:
            print(f"Waiting {fault_time} sec before killing miner R{id}")
            sleep(fault_time)
            print(f"Killed miner R{id}")
            process.kill()

    if fault_info_provided:
        with open(filename, 'r+') as fd:
            view_nb = 0
            for line in fd:
                res = search(r'.*V=(\d+).*', line)
                if res:
                    view_nb = max(view_nb, int(res.group(1)))

            logger = get_logger(fd)
            msg = f"Killed replica R{id} at view V={view_nb} when {nb_faults} fault(s) needed."
            if view_nb % nb_validators <= id: 
                logger.info(msg) 
            else:
                logger.error(msg)

    return


if __name__ == "__main__":
    main()
