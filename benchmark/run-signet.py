# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#!/usr/bin/env python3
import time
import subprocess
from pathlib import Path

import click
import os
from benchmark_framework import TMUX_SESSION_NAME
from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import import_module_by_abspath, NodeType, get_node_dirname


@click.command("run-signet")
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
    "--node-type",
    type=click.Choice(NodeType.__members__),
    callback=lambda c, p, v: getattr(NodeType, v) if v else None
)
@click.option(
    "--nb-of-type",
    type=click.IntRange(min=1),
    required=True,
    help="Number of validators/clients in the network.",
)
def main(bitcoin_path, itcoin_core_test_framework_path, id: str, node_type: NodeType, nb_of_type: int) -> None:
    """Run a network of signet nodes."""

    test_framework_module = import_module_by_abspath(itcoin_core_test_framework_path, "test_framework")
    from test_framework.test_node import TestNodeCLI
    from test_framework.authproxy import JSONRPCException

    bitcoin_path = Path(bitcoin_path)
    node_name = get_node_dirname(id, node_type, nb_of_type)
    filename = PathMaker.node_logs_path(node_name) + '/bitcoind_logs.txt'
    os.makedirs(os.path.dirname(filename), exist_ok=True)

    bitcoind = bitcoin_path / "bitcoind"
    output_dir = Path(PathMaker.output_path()).resolve()
    datadir = output_dir / node_name
    command = f"{bitcoind} -datadir={datadir} &> {filename}"
    subprocess.run(['tmux', 'new-window','-t', TMUX_SESSION_NAME, '-n', f'bitcoind-{node_type.value}-{id}', command])

    # Wait until the node has started
    bitcoin_cli = bitcoin_path / "bitcoin-cli"
    cli = TestNodeCLI(str(bitcoin_cli), str(datadir))
    ready = False
    while not ready:
        try:
            cli.getblockchaininfo()
            ready = True
        except JSONRPCException as e: # Initialization phase
            # -28 RPC in warmup
            if e.error['code'] != -28:
                raise # unknwon JSON RPC exception
        except subprocess.CalledProcessError as e:
            pass
        time.sleep(0.2)

    return

if __name__ == "__main__":
    main()
