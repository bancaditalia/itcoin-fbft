# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#!/usr/bin/env python3
import collections
import contextlib
import hashlib
import importlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import types
from random import randrange
from pathlib import Path
from typing import Any, List, Dict, Sequence, Tuple, Type, Union

import click

from benchmark_framework.utils import import_module_by_abspath, byzantine_quorum, byzantine_replies_quorum, \
    get_node_dirname, NodeType

# constants
LOCALHOST = "127.0.0.1"
DEFAULT = "DEFAULT"
SIGNET = "signet"
BITCOIND_CONFIG_FILENAME = "bitcoin.conf"
MINER_CONFIG_FILENAME = "miner.conf.json"
PORT_START = 38000
RPC_PORT_START = 37000
FBFT_REPLICA_SET_PORT_START = 13000
FBFT_CLIENT_SET_PORT_START = 14000
RPC_BIND = "127.0.0.1"
RPC_ALLOW_IP = "127.0.0.1/0"
ZMQ_PORT_START_PUBHASHTX = 29000
ZMQ_PORT_START_PUBITCOINBLOCK = 30000
MAX_BITCOIND_OUTBOUND_CONNECTIONS = 10

@contextlib.contextmanager
def save_argv():
    old_argv = sys.argv
    sys.argv = sys.argv[:1]
    yield
    sys.argv = old_argv


# just for readability/documentation
BitcoinTestFramework = Any
AddressKeyPair = collections.namedtuple("AddressKeyPair", ["address", "key"])


@contextlib.contextmanager
def bitcoin_testcase_network(bitcoin_testcase_cls: Type["BitcoinTestFramework"]):
    test_case_object = bitcoin_testcase_cls()
    test_case_object.setup()
    yield test_case_object
    test_case_object.shutdown()

def sha256(s):
    return hashlib.sha256(s).digest()


def hash256(s):
    return sha256(sha256(s))


chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"


def byte_to_base58(b, version):
    result = ""
    b = bytes([version]) + b  # prepend version
    b += hash256(b)[:4]  # append checksum
    value = int.from_bytes(b, "big")
    while value > 0:
        result = chars[value % 58] + result
        value //= 58
    while b[0] == 0:
        result = chars[0] + result
        b = b[1:]
    return result


class SignetDirectoryBuilder:
    """Builder class for a signet configuration directory."""

    def __init__(
        self,
        output_directory: Union[str, Path],
        itcoin_core_dir: Union[str, Path],
        nb_validators: int,
        nb_clients: int,
        genesis_block_timestamp: int,
        target_block_time: int,
        remote_config: str
    ) -> None:

        self._output_directory = Path(output_directory).resolve()
        self._itcoin_core_dir = Path(itcoin_core_dir).resolve()
        self._nb_validators = nb_validators
        self._nb_clients = nb_clients
        self._genesis_block_timestamp = genesis_block_timestamp
        self._target_block_time = target_block_time
        self._remote_config : Dict[int, str] = dict(json.loads(remote_config)) if remote_config else None
        self._test_framework_itcoin_module, self._test_framework_itcoin_frost_module = self._import_bitcoin_test_framework()

        self._build_called: bool = False

    def _get_validator_dirname(self, validator_id: int) -> str:
        """Get the name of the directory of the validator configuration."""
        return get_node_dirname(validator_id, NodeType.VALIDATOR, self._nb_validators)

    def _get_client_dirname(self, client_id: int) -> str:
        """Get the name of the directory of the client configuration."""
        return get_node_dirname(client_id, NodeType.CLIENT, self._nb_clients)

    def _import_bitcoin_test_framework(self) -> types.ModuleType:
        """
        Import the bitcoin test framework.

        It returns the module 'test_framework.test_framework_itcoin'.

        The method patches the constant test_framework.util.MAX_NODES to make the
        signet configuration generalizable to any number of nodes.

        Then, it patches the class attribute TestNode.PRIV_KEYS that are generated on-the-fly.

        :return: the test_framework_itcoin module object.
        """
        # import test_framework root module
        test_framework_path = (
            self._itcoin_core_dir
            / "test"
            / "functional"
            / "test_framework"
            / "__init__.py"
        )
        import_module_by_abspath(test_framework_path, "test_framework")

        # patch MAX_NODES variable to match the number of validators requested
        test_framework_util_module = importlib.import_module("test_framework.util")
        test_framework_util_module.MAX_NODES = self._nb_validators + self._nb_clients

        # import test_framework_itcoin module
        test_framework_itcoin_module = importlib.import_module(
            "test_framework.test_framework_itcoin"
        )
        # import test_framework_itcoin_frost module
        test_framework_itcoin_frost_module = importlib.import_module(
            "test_framework.test_framework_itcoin_frost"
        )

        # patch the class attribute TestNode.PRIV_KEYS
        test_framework_itcoin_module.TestNode.PRIV_KEYS = self._generate_key_pairs(
            test_framework_itcoin_module, self._itcoin_core_dir, self._nb_validators + self._nb_clients
        )
        test_framework_itcoin_module.TestNode.MAX_NODES = len(
            test_framework_itcoin_module.TestNode.PRIV_KEYS
        )

        return test_framework_itcoin_module, test_framework_itcoin_frost_module

    def _get_new_rpc_credentials(self) -> Tuple[str, str]:
        """Get new RPC credentials."""
        rpcauth_script_path = self._itcoin_core_dir / "share" / "rpcauth" / "rpcauth.py"
        result = subprocess.run([rpcauth_script_path, "user"], capture_output=True)
        stdout = result.stdout.decode()
        rpcauth_line = stdout.splitlines()[1]
        user, password = rpcauth_line.split("=")[1].split(":")
        return user, password

    @classmethod
    def _new_key_pair(cls, rpc) -> AddressKeyPair:
        """
        Get a new key pair.

        The implementation follows the instruction reported here:

            https://en.bitcoin.it/wiki/Signet#Generating_keys_used_for_signing_a_block

        :param: the RPC client
        :return: the key pair for the signet.
        """
        addr = rpc.getnewaddress("just_a_random_label_as_1st_arg", "legacy")
        privkey = rpc.dumpprivkey(addr)
        return AddressKeyPair(addr, privkey)

    @classmethod
    def _generate_key_pairs(
        cls, itcoin_module: types.ModuleType, itcoin_core_dir: Path, n: int
    ) -> Sequence[AddressKeyPair]:
        """
        Generate key pairs.

        This function spawns a temporary Bitcoind process and
        apply n times the procedure explained at this link:

            https://en.bitcoin.it/wiki/Signet#Generating_keys_used_for_signing_a_block

        """
        result: List[AddressKeyPair] = []
        # start a temporary bitcoind process
        with tempfile.TemporaryDirectory() as tmpdir:
            config_path = itcoin_core_dir / "test" / "config.ini"
            bitcoind_node = itcoin_module.BitcoindNode(tmpdir, configfile=config_path)
            bitcoind_node.chain = SIGNET
            with bitcoind_node as node:
                node.rpc.createwallet("default_wallet")
                for _ in range(n):
                    key_pair = cls._new_key_pair(node.rpc)
                    result.append(key_pair)
        return result

    def _check_build_called(self) -> None:
        if self._build_called:
            raise ValueError("build() already called!")

    def _declare_itcoin_test_case_class(self) -> Type[BitcoinTestFramework]:

        test_framework_itcoin_module = self._test_framework_itcoin_module
        test_framework_itcoin_frost_module = self._test_framework_itcoin_frost_module
        nb_validators = self._nb_validators
        nb_clients = self._nb_clients

        class MyFrostItcoinTestCase(test_framework_itcoin_frost_module.BaseFrostTest):
            def set_test_params(self):
                self.num_nodes = nb_validators + nb_clients
                self.signet_num_signers = nb_validators
                self.signet_num_signatures = byzantine_replies_quorum(nb_validators)
                super().set_test_params(tweak_public_key=False)

            def run_test(self):
                pass

        return MyFrostItcoinTestCase

    def _get_bitcoind_config_file_path(self, node_id: int) -> Path:
        """Get the bitcoind configuration path."""
        if node_id < self._nb_validators:
            node_name = self._get_validator_dirname(node_id)
        else:
            node_name = self._get_client_dirname(node_id - self._nb_validators)
        return self._output_directory / node_name / BITCOIND_CONFIG_FILENAME

    def _get_miner_config_file_path(self, node_id: int) -> Path:
        """Get the miner configuration path."""
        assert 0 <= node_id < self._nb_validators
        return self._output_directory / self._get_validator_dirname(node_id) / MINER_CONFIG_FILENAME

    def _create_bitcoind_config(self, node_id: int, signet_challenge: str, use_remote_config : bool = False):
        bitcoind_config_file_path = self._get_bitcoind_config_file_path(node_id)
        rpc_user, rpc_password = self._get_new_rpc_credentials()

        addnode_lines = []
        # We select 10 nodes to connect to
        # The number of outbound connections is limited to 10
        # https://bitcoin.stackexchange.com/questions/109833/how-do-i-increase-the-maximum-number-of-outbound-peers

        # First connect to miner nodes
        nb_validator_connections = min(self._nb_validators, MAX_BITCOIND_OUTBOUND_CONNECTIONS)
        for i in range(node_id, node_id + nb_validator_connections):
            if use_remote_config:
                addnode_lines.append(f"addnode={self._remote_config[i % self._nb_validators]}:{PORT_START}")
            else:
                addnode_lines.append(f"addnode={LOCALHOST}:{PORT_START + (i % self._nb_validators)}")

        # If there is still room for connections, then connecto to a few random client nodes
        client_selected_for_connection = []
        for i in range(min(self._nb_clients, MAX_BITCOIND_OUTBOUND_CONNECTIONS-nb_validator_connections)):
            node_i = self._nb_validators + randrange(self._nb_clients)
            while node_i in client_selected_for_connection:
                node_i = self._nb_validators + randrange(self._nb_clients)
            client_selected_for_connection.append(node_i)
            if use_remote_config:
                addnode_lines.append(f"addnode={self._remote_config[node_i]}:{PORT_START}")
            else:
               addnode_lines.append(f"addnode={LOCALHOST}:{PORT_START + node_i}")

        config_file_contents = textwrap.dedent(f"""\
        signet=1

        [signet]
        signetchallenge={signet_challenge}
        port={PORT_START if use_remote_config else PORT_START + node_id}
        rpcport={RPC_PORT_START if use_remote_config else RPC_PORT_START + node_id}
        rpcuser={rpc_user}
        rpcpassword={rpc_password}
        rpcbind={RPC_BIND}
        rpcallowip={RPC_ALLOW_IP}
        zmqpubhashtx=tcp://127.0.0.1:{ZMQ_PORT_START_PUBHASHTX if use_remote_config else ZMQ_PORT_START_PUBHASHTX + node_id}
        zmqpubitcoinblock=tcp://127.0.0.1:{ZMQ_PORT_START_PUBITCOINBLOCK if use_remote_config else ZMQ_PORT_START_PUBITCOINBLOCK + node_id}
        debug=leveldb
        debug=reindex
        debug=coindb
        debug=prune
        debug=net
        debug=http
        debug=zmq

        # connect to peers
        """)
        config_file_contents += "\n".join(addnode_lines)

        # Write configuration to file
        bitcoind_config_file_path.write_text(config_file_contents)

    def _create_miner_config(self, node_id, signet_key_pairs, frost_info, use_remote_config : bool = False):
        miner_config_file_path = self._get_miner_config_file_path(node_id)
        config = {
            "id": node_id,

            # Genesis block hash is fixed for all signets
            "genesis_block_hash": "49dc109285f9a914c1d16a6b03bb139ef298e97c1d561bc805dad5e8463ec26f",

            # We set genesis block timestamp to current time, so that we start mining asap
            "genesis_block_timestamp": self._genesis_block_timestamp,

            "target_block_time": self._target_block_time,
            "fbft_replica_set": [
                {
                    "host": self._remote_config[replica_id] if use_remote_config else LOCALHOST,
                    "port": FBFT_REPLICA_SET_PORT_START if use_remote_config else FBFT_REPLICA_SET_PORT_START + replica_id,
                    "p2pkh": address,
                    "pubkey": getattr(frost_info[replica_id],'pubkey')
                }
                for replica_id, (address, privkey) in enumerate(signet_key_pairs)
            ],
        }
        miner_config_file_path.write_text(json.dumps(config, indent=2))

    def build(self) -> None:
        """Build the"""
        self._check_build_called()
        self._build_called = True

        itcoin_test_case_cls = self._declare_itcoin_test_case_class()
        with save_argv(), bitcoin_testcase_network(
            itcoin_test_case_cls
        ) as test_case_object:
            # copy test temporary directory to output directory
            test_dir = test_case_object.nodes[0].cwd
            shutil.copytree(test_dir, self._output_directory)

            # save signet info
            signet_key_pairs = test_case_object.signet_key_pairs
            frost_info = test_case_object.frost_info
            signet_challenge = test_case_object.signet_challenge

        # rename node/client folders by prepending digits '0' depending on max number of nodes
        for directory in self._output_directory.glob("node*"):
            node_number = int(re.search("node([0-9]+)", directory.name).group(1))

            # check if node is for a validator or for a client
            if node_number < self._nb_validators:
                dirname = self._get_validator_dirname(node_number)
            else:
                dirname = self._get_client_dirname(node_number - self._nb_validators)
            directory.rename(self._output_directory / dirname)

        # remove stdout and stderr subdirectories
        (self._output_directory / "test_framework.log").unlink(missing_ok=True)
        for path in self._output_directory.rglob("stdout"):
            shutil.rmtree(path)
        for path in self._output_directory.rglob("stderr"):
            shutil.rmtree(path)

        # create miner configuration for each node
        for node_id in range(self._nb_validators + self._nb_clients):
            self._create_bitcoind_config(node_id, signet_challenge, True if self._remote_config is not None else False)
            if node_id < self._nb_validators:
                self._create_miner_config(node_id, signet_key_pairs, frost_info, True if self._remote_config is not None else False)


@click.command("setup-signet")
@click.option(
    "--output-directory",
    type=click.Path(exists=False, file_okay=False, dir_okay=True),
    required=True,
    help="Path to output directory.",
)
@click.option(
    "--itcoin-core-dir",
    type=click.Path(exists=False, file_okay=False, dir_okay=True),
    required=True,
    help="Path to Itcoin Core project.",
)
@click.option(
    "--nb-validators",
    type=click.IntRange(min=1),
    required=True,
    help="Number of validators in the network.",
)
@click.option(
    "--nb-clients",
    type=click.IntRange(min=1),
    required=True,
    help="Number of clients in the network.",
)
@click.option(
    "--genesis-block-timestamp",
    type=click.IntRange(min=1598918400),
    required=True,
    help="The UNIX timestamp of the genesis block. Together with --target-block-time, it will be used to calculate timestamps of the block requests.",
)
@click.option(
    "--target-block-time",
    type=click.IntRange(min=1),
    required=True,
    help="The number of seconds between blocks. Together with --genesis-block-timestamp, it will be used to calculate timestamps of the block requests.",
)
@click.option(
    "--remote-config",
    type=str,
    help="JSON string mapping node_id to ip address for remote configuration.",
)
def main(output_directory: str, itcoin_core_dir: str, nb_validators: int, nb_clients: int,
genesis_block_timestamp: int, target_block_time: int, remote_config: str) -> None:
    """Set up a Signet network configuration directory."""

    signet_builder = SignetDirectoryBuilder(
        output_directory=output_directory,
        itcoin_core_dir=itcoin_core_dir,
        nb_validators=nb_validators,
        nb_clients=nb_clients,
        genesis_block_timestamp=genesis_block_timestamp,
        target_block_time=target_block_time,
        remote_config = remote_config
    )
    signet_builder.build()


if __name__ == "__main__":
    main()
