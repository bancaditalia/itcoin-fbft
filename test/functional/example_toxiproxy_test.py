#!/usr/bin/env python3
"""An example functional test that uses toxiproxy

This test shows how to use the toxiprosy functionality of the test framework
"""
# Configure path for successive imports
from test_framework_pbft.configure_sys_paths import configure_sys_paths
configure_sys_paths()

import miner

from test_framework_pbft.test_framework_pbft import MinerInterfaceType, MinerTestFramework, MinerNodeType
from test_framework.util import (
    assert_equal,
)

class ExampleToxiproxyTest(MinerTestFramework):

    def set_test_params(self):
        self.signet_num_signers = 4
        super().set_test_params()

    def run_test(self):
        """Main test logic"""

        # Generating a block on one of the nodes will get us out of IBD
        blocks = []
        args0 = self.node(0).args
        block, _, _ = miner.do_generate_next_block(args0)
        signed_block = self.do_multisign_block({0, 1, 2}, block, self.signet_challenge)
        miner.do_propagate_block(args0, signed_block)
        self.sync_all(self.nodes[0:2])
        blocks.append(block)

        self.toxiproxy_poison_channel(MinerNodeType.REPLICA, 0, MinerInterfaceType.P2P, 1, "latency", {"latency": 1000})
        # TODO: remove raise when test is developed. Keeping it now so that we can inspect the working directories
        # raise


if __name__ == '__main__':
    ExampleToxiproxyTest().main()
