#!/usr/bin/env python3
# This file has been originally imported from https://github.com/bancaditalia/itcoin-core @ 0e5c54044f103cd807748613ff7d5fa0dbdcd762
# and subsequently modified to fit a test framework for itcoin-pbft
"""An example functional test

The module-level docstring should include a high-level description of
what the test is doing. It's the first thing people see when they open
the file and should give the reader information about *what* the test
is testing and *how* it's being tested
"""
# Imports should be in PEP8 ordering (std library first, then third party
# libraries then local imports).
from collections import defaultdict
import time

# Configure path for successive imports
from test_framework_pbft.configure_sys_paths import configure_sys_paths
configure_sys_paths()

# Now we can import the miner
import miner

# Avoid wildcard * imports
from test_framework.blocktools import (create_block, create_coinbase)
from test_framework.messages import CInv, MSG_BLOCK
from test_framework.p2p import (
    P2PInterface,
    msg_block,
    msg_getdata,
    p2p_lock,
)
from test_framework_pbft.test_framework_pbft import MinerTestFramework
from test_framework.util import (
    assert_equal,
)

# P2PInterface is a class containing callbacks to be executed when a P2P
# message is received from the node-under-test. Subclass P2PInterface and
# override the on_*() methods if you need custom behaviour.
class BaseNode(P2PInterface):
    def __init__(self):
        """Initialize the P2PInterface

        Used to initialize custom properties for the Node that aren't
        included by default in the base class. Be aware that the P2PInterface
        base class already stores a counter for each P2P message type and the
        last received message of each type, which should be sufficient for the
        needs of most tests.

        Call super().__init__() first for standard initialization and then
        initialize custom properties."""
        super().__init__()
        # Stores a dictionary of all blocks received
        self.block_receive_map = defaultdict(int)

    def on_block(self, message):
        """Override the standard on_block callback

        Store the hash of a received block in the dictionary."""
        message.block.calc_sha256()
        self.block_receive_map[message.block.sha256] += 1

    def on_inv(self, message):
        """Override the standard on_inv callback"""
        pass

def custom_function():
    """Do some custom behaviour

    If this function is more generally useful for other tests, consider
    moving it to a module in test_framework."""
    # self.log.info("running custom_function")  # Oops! Can't run self.log outside the BitcoinTestFramework
    pass


class ExampleTest(MinerTestFramework):
    # Each functional test is a subclass of the BitcoinTestFramework class.

    # Override the set_test_params(), skip_test_if_missing_module(), add_options(), setup_chain(), setup_network()
    # and setup_nodes() methods to customize the test setup as required.


    def set_test_params(self):
        """Override test parameters for your individual test.

        This method must be overridden and num_nodes must be explicitly set."""
        self.signet_num_signers = 4
        super().set_test_params()

        # self.log.info("I've finished set_test_params")  # Oops! Can't run self.log before run_test()

    # Use skip_test_if_missing_module() to skip the test if your test requires certain modules to be present.
    # This test uses generate which requires wallet to be compiled
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    # Use add_options() to add specific command-line options for your test.
    # In practice this is not used very much, since the tests are mostly written
    # to be run in automated environments without command-line options.
    # def add_options()
    #     pass

    # Use setup_chain() to customize the node data directories. In practice
    # this is not used very much since the default behaviour is almost always
    # fine
    # def setup_chain():
    #     pass

    def setup_network(self):
        """Setup the test network topology

        Often you won't need to override this, since the standard network topology
        (linear: node0 <-> node1 <-> node2 <-> ...) is fine for most tests.

        If you do override this method, remember to start the nodes, assign
        them to self.nodes, connect them and then sync."""

        self.setup_nodes()

        # In this test, we're not connecting node2 to node0 or node1. Calls to
        # sync_all() should not include node2, since we're not expecting it to
        # sync.
        self.connect_nodes(0, 1)
        self.sync_all(self.nodes[0:2])

    # Use setup_nodes() to customize the node start behaviour (for example if
    # you don't want to start all nodes at the start of the test).
    # def setup_nodes():
    #     pass

    def custom_method(self):
        """Do some custom behaviour for this test

        Define it in a method here because you're going to use it repeatedly.
        If you think it's useful in general, consider moving it to the base
        BitcoinTestFramework class so other tests can use it."""

        self.log.info("Running custom_method")

    def run_test(self):
        """Main test logic"""

        # Create P2P connections will wait for a verack to make sure the connection is fully up
        # TODO: Skip. It does not work because of the magic number, the following is in node0 debug.log
        # HEADER ERROR - MESSAGESTART (version, 111 bytes), received 0a03cf40, peer=1
        # peer_messaging = self.nodes[0].add_p2p_connection(BaseNode())

        # Generating a block on one of the nodes will get us out of IBD
        blocks = []
        args0 = self.node(0).args
        block, _, _ = miner.do_generate_next_block(args0)
        signed_block = self.do_multisign_block({0, 1, 2}, block, self.signet_challenge)
        miner.do_propagate_block(args0, signed_block)
        self.sync_all(self.nodes[0:2])
        blocks.append(block)

        # Notice above how we called an RPC by calling a method with the same
        # name on the node object. Notice also how we used a keyword argument
        # to specify a named RPC argument. Neither of those are defined on the
        # node object. Instead there's some __getattr__() magic going on under
        # the covers to dispatch unrecognised attribute calls to the RPC
        # interface.

        # Logs are nice. Do plenty of them. They can be used in place of comments for
        # breaking the test into sub-sections.
        self.log.info("Starting test!")

        self.log.info("Calling a custom function")
        custom_function()

        self.log.info("Calling a custom method")
        self.custom_method()

        self.log.info("Create some blocks")
        for _ in range(10):
            # Use the custom itcoin functionality to  build a block.
            # Calling the generate() rpc is easier, but this allows us to exactly
            # control the blocks and transactions.
            args0 = self.node(0).args
            block, _, _ = miner.do_generate_next_block(args0)
            signed_block = self.do_multisign_block({0, 1, 2}, block, self.signet_challenge)
            miner.do_propagate_block(args0, signed_block)
            blocks.append(block)

        node0_height = self.nodes[0].getblockcount()
        assert_equal(node0_height, 11)
        self.log.info("Wait for node1 to reach current tip (height 11) using RPC")
        self.nodes[1].waitforblockheight(node0_height)

        self.log.info("Connect node2 and node1")
        self.connect_nodes(1, 2)
        self.connect_nodes(2, 3)

        self.log.info("Wait for node2 to receive all the blocks from node1")
        self.sync_all()

        self.log.info("Add P2P connection to node2")
        self.nodes[0].disconnect_p2ps()

        # TODO: Skip. It does not work because of the magic number
        # peer_receiving = self.nodes[2].add_p2p_connection(BaseNode())
        # self.log.info("Test that node2 propagates all the blocks to us")
        # getdata_request = msg_getdata()
        # for block in blocks:
        #     getdata_request.inv.append(CInv(MSG_BLOCK, block))
        # peer_receiving.send_message(getdata_request)

        # wait_until() will loop until a predicate condition is met. Use it to test properties of the
        # P2PInterface objects.
        # peer_receiving.wait_until(lambda: sorted(blocks) == sorted(list(peer_receiving.block_receive_map.keys())), timeout=5)

        # self.log.info("Check that each block was received only once")
        # The network thread uses a global lock on data access to the P2PConnection objects when sending and receiving
        # messages. The test thread should acquire the global lock before accessing any P2PConnection data to avoid locking
        # and synchronization issues. Note p2p.wait_until() acquires this global lock internally when testing the predicate.
        # with p2p_lock:
        #     for block in peer_receiving.block_receive_map.values():
        #         assert_equal(block, 1)


if __name__ == '__main__':
    ExampleTest().main()
