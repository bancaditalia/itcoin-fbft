#!/usr/bin/env python3
import sys
import signal
import time
from pathlib import Path
from typing import List
import subprocess
import click
import os

from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import import_module_by_abspath, NodeType, get_node_dirname, get_logger


@click.command("run-donor")
@click.option(
    "--bitcoin-path",
    type=click.Path(exists=False, file_okay=False, dir_okay=True),
    required=True,
    help="Path to bitcoind binary.",
)
@click.option(
    "--itcoin-core-test-framework-path",
    type=click.Path(exists=False, file_okay=True, dir_okay=False),
    required=True,
    help="Path to itcoin-core test fraemwork __init__.py.",
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
    '--client-address',
    required=True,
    multiple=True
)
def main(bitcoin_path, itcoin_core_test_framework_path, id: str, nb_validators: int, client_address: List[str]) -> None:
    test_framework_module = import_module_by_abspath(itcoin_core_test_framework_path, "test_framework")
    from test_framework.test_node import TestNodeCLI
    from test_framework.authproxy import JSONRPCException

    node_name = get_node_dirname(id, NodeType.VALIDATOR, nb_validators)
    filename = PathMaker.node_logs_path(node_name) + '/donor_logs.txt'
    os.makedirs(os.path.dirname(filename), exist_ok=True)

    with open(filename, 'w') as fd:
        stop_sig_received = False
        logger = get_logger(fd)

        def signal_handler(signum, frame):
            logger.debug("Stop signal received")
            global stop_sig_received
            stop_sig_received=True

        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)

        bitcoin_cli = bitcoin_path + "/bitcoin-cli"
        output_dir = Path(PathMaker.output_path()).absolute()
        datadir = output_dir / node_name
        
        cli = TestNodeCLI(str(bitcoin_cli), str(datadir))

        ready = False
        while not ready and not stop_sig_received:
            try:
                res = cli.getwalletinfo()
                if res['balance'] > 50:
                    ready = True
            except JSONRPCException as e: # Initialization phase
                logger.debug(f"Error while validator {id} was waiting for funds.")
            except subprocess.CalledProcessError as e:
                logger.debug("failed to interact with bitcoind")
            time.sleep(6)

        while not stop_sig_received:
            try:
                for i,address in enumerate(client_address):
                    txid = cli.sendtoaddress(address=address, amount=1, fee_rate=25)
                    logger.debug(f"Validator {id} sending funds to client {i} in {txid}.")
            except JSONRPCException as e: # Initialization phase
                logger.debug(f"Error in {id} sending funds to client {i} in {txid}.")
            except subprocess.CalledProcessError as e:
                logger.debug("failed to interact with bitcoind")
            time.sleep(6) # TODO, this should depend on block time

if __name__ == "__main__":
    main()
