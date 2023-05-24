#!/usr/bin/env python3
# This file has been originally imported from https://github.com/bancaditalia/itcoin-core @ 0e5c54044f103cd807748613ff7d5fa0dbdcd762
# and subsequently modified to fit a test framework for itcoin-pbft
"""Base class for RPC testing."""

import configparser
import math
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from enum import Enum
from typing import List

from itertools import combinations

from test_framework.address import base58_to_byte, key_to_p2pkh
from test_framework.conftest import ITCOIN_CORE_ROOT_DIR, ITCOIN_PBFT_ROOT_DIR
from test_framework.key import ECKey
from test_framework.p2p import NetworkThread
from test_framework.test_node import TestNode
from test_framework.test_framework_signet import BaseSignetTest

from test_framework.util import (
    MAX_NODES,
    PortSeed,
    assert_equal,
    check_json_precision,
    get_datadir_path,
)

from test_framework.test_framework import (
    TestStatus,
    SkipTest,
    TMPDIR_PREFIX
)

# Pbft specific imports

from toxiproxy import Toxiproxy, proxy
from .test_node_pbft import TestNodePbft
from test_framework_pbft.util import (
    initialize_pbft_datadir,
    pbft_p2p_ports,
    pbft_rpc_port
)

# Pbft node types

class MinerNodeType(Enum):
    CLIENT = 1
    CORE = 2
    REPLICA = 3

class MinerInterfaceType(Enum):
    P2P = 1
    RPC = 2

#
# Test framework class
#

class MinerTestFramework(BaseSignetTest):
    """Base class for a bitcoin test script.

    Individual bitcoin test scripts should subclass this class and override the set_test_params() and run_test() methods.

    Individual tests can also override the following methods to customize the test setup:

    - add_options()
    - setup_chain()
    - setup_network()
    - setup_nodes()

    The __init__() and main() methods should not be overridden.

    This class also contains various public and private helper methods."""

    chain = None  # type: str
    setup_clean_chain = None  # type: bool

    def setup(self):
        """Call this method to start up the test framework object with options set."""

        PortSeed.n = self.options.port_seed

        check_json_precision()

        self.options.cachedir = os.path.abspath(self.options.cachedir)

        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile))
        # Custom config fields
        config["environment"]["SRCDIR"] = str(ITCOIN_PBFT_ROOT_DIR)
        config["environment"]["PBFT_BUILDDIR"] = str(ITCOIN_PBFT_ROOT_DIR)
        config["environment"]["CORE_BUILDDIR"] = str(ITCOIN_CORE_ROOT_DIR)

        self.config = config
        fname_bitcoind = os.path.join(
            config["environment"]["CORE_BUILDDIR"],
            "src",
            "bitcoind" + config["environment"]["EXEEXT"],
        )
        fname_bitcoincli = os.path.join(
            config["environment"]["CORE_BUILDDIR"],
            "src",
            "bitcoin-cli" + config["environment"]["EXEEXT"],
        )
        fname_pbftd = os.path.join(
            config["environment"]["PBFT_BUILDDIR"],
            "target",
            "main" + config["environment"]["EXEEXT"],
        )
        self.options.bitcoind = os.getenv("BITCOIND", default=fname_bitcoind)
        self.options.bitcoincli = os.getenv("BITCOINCLI", default=fname_bitcoincli)
        self.options.pbftd = os.getenv("PBFTD", default=fname_pbftd)

        os.environ['PATH'] = os.pathsep.join([
            os.path.join(config['environment']['PBFT_BUILDDIR'], 'target'),
            os.path.join(config['environment']['CORE_BUILDDIR'], 'src'),
            os.path.join(config['environment']['CORE_BUILDDIR'], 'src', 'qt'), os.environ['PATH']
        ])

        # Set up temp directory and start logging
        if self.options.tmpdir:
            self.options.tmpdir = os.path.abspath(self.options.tmpdir)
            os.makedirs(self.options.tmpdir, exist_ok=False)
        else:
            self.options.tmpdir = tempfile.mkdtemp(prefix=TMPDIR_PREFIX)
        self._start_logging()

        # Seed the PRNG. Note that test runs are reproducible if and only if
        # a single thread accesses the PRNG. For more information, see
        # https://docs.python.org/3/library/random.html#notes-on-reproducibility.
        # The network thread shouldn't access random. If we need to change the
        # network thread to access randomness, it should instantiate its own
        # random.Random object.
        seed = self.options.randomseed

        if seed is None:
            seed = random.randrange(sys.maxsize)
        else:
            self.log.debug("User supplied random seed {}".format(seed))

        random.seed(seed)
        self.log.debug("PRNG seed is: {}".format(seed))

        self.log.debug('Setting up network thread')
        self.network_thread = NetworkThread()
        self.network_thread.start()

        if self.options.usecli:
            if not self.supports_cli:
                raise SkipTest("--usecli specified but test does not support using CLI")
            self.skip_if_no_cli()
        self.skip_test_if_missing_module()

        #initialize pbft configuration
        self.log.info(f'Signet with {self.signet_num_signers} signers can tolerate up to {self.pbft_max_failures} byzantine failures, requires {self.signet_num_signatures} signatures')

        # Start toxiproxy if it's not already running
        if (self.toxiproxy.running()):
            self.log.info("Toxiproxy is already running, destroying all existing proxies")
            self.toxiproxy.destroy_all()
        else:
            self.log.info("Toxiproxy is not running, starting...")
            tp_stdout_stderr = tempfile.NamedTemporaryFile(dir=self.options.tmpdir, prefix="toxiproxy_", delete=False)
            tp_process = subprocess.Popen("toxiproxy-server", stdout=tp_stdout_stderr, stderr=tp_stdout_stderr, cwd=self.options.tmpdir)
            continue_poll_timeout_msec = 500
            while not self.toxiproxy.running():
                tp_state = tp_process.poll()
                if (tp_state):
                    raise RuntimeError("Unable to start toxiproxy")
                time.sleep(100 / 1000)
                continue_poll_timeout_msec -= 100
                if not continue_poll_timeout_msec:
                    raise RuntimeError("Unable to start toxiproxy in reasonable time")

        # Configure toxi proxies, such as
        # toxi_proxies_configs = [
        #     {
        #         "name": "test_toxiproxy_populate1",
        #         "upstream": "localhost:3306",
        #         "listen": "localhost:22222"
        #     }, ...
        # ]
        for n1,n2 in combinations(range(self.num_nodes), 2):
            # Adding toxiproxy from n1 to n2
            upstream_port = pbft_p2p_ports(n2)[0]
            listen_port = pbft_p2p_ports(n2, from_node=n1)[0]
            toxi_config1 = {
                "name": self._toxiproxy_name(MinerNodeType.REPLICA, n1, MinerInterfaceType.P2P, n2),
                "upstream": f"localhost:{upstream_port}",
                "listen": f"localhost:{listen_port}"
            }

            # Adding toxiproxy from n2 to n1
            upstream_port = pbft_p2p_ports(n1)[0]
            listen_port = pbft_p2p_ports(n1, from_node=n2)[0]
            toxi_config2 = {
                "name": self._toxiproxy_name(MinerNodeType.REPLICA, n2, MinerInterfaceType.P2P, n1),
                "upstream": f"localhost:{upstream_port}",
                "listen": f"localhost:{listen_port}"
            }

            # Populate proxies
            self.toxiproxy.populate([toxi_config1, toxi_config2])

        # Configure the node keys
        node_keys = [ECKey() for i in range(self.num_nodes)]
        for i, key in enumerate(node_keys):
            key_b = base58_to_byte(self.signet_key_pairs[i].key)[0][:-1]
            key.set(key_b, True)
        node_pubkeys = [key.get_pubkey().get_bytes() for key in node_keys]
        node_p2pkh = [key_to_p2pkh(pubkey) for pubkey in node_pubkeys]

        # Create the configuration dictionary
        for i in range(self.num_nodes):
            pbft_conf_dict = {
                "id": i,
                "test_rpc_port": pbft_rpc_port(i),
                "pbft_replica_set": [],
                "pbft_client_set": []
            }
            for j in range(self.num_nodes):
                replica_port = pbft_p2p_ports(j, from_node=i)[0]
                client_port = pbft_p2p_ports(j)[1]
                replica_set = {
                    "host" : '127.0.0.1',
                    "port" : replica_port,
                    "p2pkh" : node_p2pkh[j]
                }
                client_set = {
                    "host" : '127.0.0.1',
                    "port" : client_port,
                    "p2pkh" : node_p2pkh[j]
                }
                pbft_conf_dict["pbft_replica_set"].append(replica_set)
                pbft_conf_dict["pbft_client_set"].append(client_set)

            self.pbft_conf_dicts.append(pbft_conf_dict)

        self.setup_chain()
        self.setup_network()

        self.success = TestStatus.PASSED

    # Methods to override in subclass test scripts.

    def run_test(self) -> None:
        """
        This method is empty, nevertheless it must be present because of TestFramework metaclass rules.
        """
        raise NotImplementedError

    def set_test_params(self):
        if not (3 < self.signet_num_signers):
            raise ValueError(f"expected 3 < N, got N={self.signet_num_signers}, please configure signet_num_signers")

        self.pbft_max_failures = math.floor( (self.signet_num_signers-1) / 3 )
        self.signet_num_signatures = self.signet_num_signers - self.pbft_max_failures

        self.pbft_nodes: List[TestNodePbft] = []
        self.pbft_conf_dicts = []

        self.toxiproxy : Toxiproxy = Toxiproxy()

        super().set_test_params(set_signet_challenge_as_extra_arg=False)

    def setup_nodes(self):
        """Override this method to customize test node setup"""
        super().setup_nodes()
        self.start_nodes_pbft()

    # Public helper methods. These can be accessed by the subclass test scripts.

    def add_nodes(self, num_nodes: int, extra_args=None, *, rpchost=None, binary=None, binary_cli=None, binary_pbft=None, versions=None):
        """Instantiate TestNode objects.

        Should only be called once after the nodes have been specified in
        set_test_params()."""
        def get_bin_from_version(version, bin_name, bin_default):
            if not version:
                return bin_default
            return os.path.join(
                self.options.previous_releases_path,
                re.sub(
                    r'\.0$',
                    '',  # remove trailing .0 for point releases
                    'v{}.{}.{}.{}'.format(
                        (version % 100000000) // 1000000,
                        (version % 1000000) // 10000,
                        (version % 10000) // 100,
                        (version % 100) // 1,
                    ),
                ),
                'bin',
                bin_name,
            )

        if self.bind_to_localhost_only:
            extra_confs = [["bind=127.0.0.1"]] * num_nodes
        else:
            extra_confs = [[]] * num_nodes
        if extra_args is None:
            extra_args = [[]] * num_nodes
        if versions is None:
            versions = [None] * num_nodes
        if binary is None:
            binary = [get_bin_from_version(v, 'bitcoind', self.options.bitcoind) for v in versions]
        if binary_cli is None:
            binary_cli = [get_bin_from_version(v, 'bitcoin-cli', self.options.bitcoincli) for v in versions]
        if binary_pbft is None:
            binary_pbft = [self.options.pbftd for v in versions]
        assert_equal(len(extra_confs), num_nodes)
        assert_equal(len(extra_args), num_nodes)
        assert_equal(len(versions), num_nodes)
        assert_equal(len(binary), num_nodes)
        assert_equal(len(binary_cli), num_nodes)
        for i in range(num_nodes):
            test_node_i = TestNode(
                i,
                get_datadir_path(self.options.tmpdir, i),
                chain=self.chain,
                rpchost=rpchost,
                timewait=self.rpc_timeout,
                timeout_factor=self.options.timeout_factor,
                bitcoind=binary[i],
                bitcoin_cli=binary_cli[i],
                version=versions[i],
                coverage_dir=self.options.coveragedir,
                cwd=self.options.tmpdir,
                extra_conf=extra_confs[i],
                extra_args=extra_args[i],
                use_cli=self.options.usecli,
                start_perf=self.options.perf,
                use_valgrind=self.options.valgrind,
                descriptors=self.options.descriptors,
            )
            self.nodes.append(test_node_i)
            if not test_node_i.version_is_at_least(170000):
                # adjust conf for pre 17
                conf_file = test_node_i.bitcoinconf
                with open(conf_file, 'r', encoding='utf8') as conf:
                    conf_data = conf.read()
                with open(conf_file, 'w', encoding='utf8') as conf:
                    conf.write(conf_data.replace('[regtest]', ''))
        for i in range(num_nodes):
            test_node_pbft_i = TestNodePbft(
                i,
                get_datadir_path(self.options.tmpdir, i),
                config=self.pbft_conf_dicts[i],
                pbftd=binary_pbft[i],
                cwd=self.options.tmpdir
            )
            self.pbft_nodes.append(test_node_pbft_i)

    def toxiproxy_poison_channel(self, node_from_type: MinerNodeType, node_from_id: int, node_interface: MinerInterfaceType, node_to_id: int,
            toxic_type, toxic_attributes):
        proxy_name : str = self._toxiproxy_name(node_from_type, node_from_id, node_interface, node_to_id)
        proxy_to_poison : proxy.Proxy = self.toxiproxy.get_proxy(proxy_name)
        proxy_to_poison.add_toxic(type=toxic_type, attributes=toxic_attributes)

    def start_nodes_pbft(self, extra_args=None, *args, **kwargs):
        """Start multiple pbftds"""

        if extra_args is None:
            extra_args = [None] * self.num_nodes
        assert_equal(len(extra_args), self.num_nodes)
        try:
            for i, node_pbft in enumerate(self.pbft_nodes):
                node_pbft.start(extra_args[i], *args, **kwargs)
            for node_pbft in self.pbft_nodes:
                node_pbft.wait_for_rpc_connection()
        except:
            # If one node failed to start, stop the others
            self.stop_nodes()
            raise

    def stop_nodes(self, wait=0):
        """Stop multiple bitcoind test nodes"""
        for node_pbft in self.pbft_nodes:
            # Issue RPC to stop nodes
            node_pbft.stop_node(wait=wait)

        for node_pbft in self.pbft_nodes:
            # Wait for nodes to stop
            node_pbft.wait_until_stopped()

        for node in self.nodes:
            # Issue RPC to stop nodes
            node.stop_node(wait=wait)

        for node in self.nodes:
            # Wait for nodes to stop
            node.wait_until_stopped()

    # Private helper methods. These should not be accessed by the subclass test scripts.

    def _initialize_chain(self):
        """Initialize a pre-mined blockchain for use by the test.

        Create a cache of a 199-block-long chain
        Afterward, create num_nodes copies from the cache."""

        CACHE_NODE_ID = 0  # Use node 0 to create the cache for all other nodes
        cache_node_dir = get_datadir_path(self.options.cachedir, CACHE_NODE_ID)
        assert self.num_nodes <= MAX_NODES

        if not os.path.isdir(cache_node_dir):
            self.log.debug("Creating cache directory {}".format(cache_node_dir))

            initialize_pbft_datadir(self.options.cachedir, CACHE_NODE_ID, self.chain, self.tf_signet_challenge, self.pbft_conf_dicts)
            self.nodes.append(
                TestNode(
                    CACHE_NODE_ID,
                    cache_node_dir,
                    chain=self.chain,
                    extra_conf=["bind=127.0.0.1"],
                    extra_args=['-disablewallet'],
                    rpchost=None,
                    timewait=self.rpc_timeout,
                    timeout_factor=self.options.timeout_factor,
                    bitcoind=self.options.bitcoind,
                    bitcoin_cli=self.options.bitcoincli,
                    coverage_dir=None,
                    cwd=self.options.tmpdir,
                    descriptors=self.options.descriptors,
                ))
            self.start_node(CACHE_NODE_ID)
            cache_node = self.nodes[CACHE_NODE_ID]

            # Wait for RPC connections to be ready
            cache_node.wait_for_rpc_connection()

            # Set a time in the past, so that blocks don't end up in the future
            cache_node.setmocktime(cache_node.getblockheader(cache_node.getbestblockhash())['time'])

            # Create a 199-block-long chain; each of the 4 first nodes
            # gets 25 mature blocks and 25 immature.
            # The 4th node gets only 24 immature blocks so that the very last
            # block in the cache does not age too much (have an old tip age).
            # This is needed so that we are out of IBD when the test starts,
            # see the tip age check in IsInitialBlockDownload().
            for i in range(8):
                cache_node.generatetoaddress(
                    nblocks=25 if i != 7 else 24,
                    address=TestNode.PRIV_KEYS[i % 4].address,
                )

            assert_equal(cache_node.getblockchaininfo()["blocks"], 199)

            # Shut it down, and clean up cache directories:
            self.stop_nodes()
            self.nodes : List[TestNode] = []

            def cache_path(*paths):
                return os.path.join(cache_node_dir, self.chain, *paths)

            os.rmdir(cache_path('wallets'))  # Remove empty wallets dir
            for entry in os.listdir(cache_path()):
                if entry not in ['chainstate', 'blocks']:  # Only keep chainstate and blocks folder
                    os.remove(cache_path(entry))

        for i in range(self.num_nodes):
            self.log.debug("Copy cache directory {} to node {}".format(cache_node_dir, i))
            to_dir = get_datadir_path(self.options.tmpdir, i)
            shutil.copytree(cache_node_dir, to_dir)
            initialize_pbft_datadir(self.options.tmpdir, i, self.chain, self.signet_challenge, self.pbft_conf_dicts)  # Overwrite port/rpcport in bitcoin.conf

    def _initialize_chain_clean(self):
        """Initialize empty blockchain for use by the test.

        Create an empty blockchain and num_nodes wallets.
        Useful if a test case wants complete control over initialization."""
        for i in range(self.num_nodes):
            initialize_pbft_datadir(self.options.tmpdir, i, self.chain, self.signet_challenge, self.pbft_conf_dicts)

    def _toxiproxy_name(self, node_from_type: MinerNodeType, node_from_id: int, channel_to_type: MinerInterfaceType, channel_to_id: int) -> str:
        return f"tf_miner_from_{node_from_type.name}_{node_from_id}_{channel_to_type.name}_to_{channel_to_id}"
