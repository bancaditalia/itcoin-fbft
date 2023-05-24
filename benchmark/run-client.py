#!/usr/bin/env python3
import ipaddress
import json
import signal
import socket
import subprocess
import time
import warnings
from random import randrange
from threading import Thread
from typing import Any, Optional

from decimal import *

import zmq
from click import Context, Parameter, ParamType

from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import import_module_by_abspath, NodeType, get_node_dirname, get_logger

from pathlib import Path
import click


ZMQ_PUB_HASH_TX_TOPIC_NAME = "hashtx"
ZMQ_PUB_ITCOIN_BLOCK_TOPIC_NAME = "itcoinblock"

class IpAddrParamType(ParamType):

    name = "ip_addr"

    def convert(
        self, value: Any, param: Optional["Parameter"], ctx: Optional["Context"]
    ) -> Any:
        """
        Validate IP address.

        :param value: IP address passed from command line
        :param param: not used
        :param ctx: not used
        :return: the same value
        """

        try:
            return ipaddress.ip_address(value)
        except socket.error:
            self.fail(f"value {value} is not a valid IP address")

class Client():

    def __init__(self, bitcoin_path, itcoin_core_test_framework_path, datadir: Path, nb_warmup_rounds: int, target_tx_rate: int, tx_size: int, host: str, port: int, log_fd: int) -> None:
        test_framework_module = import_module_by_abspath(itcoin_core_test_framework_path, "test_framework")
        from test_framework.test_node import TestNodeCLI

        """Initialize the client."""
        self.nb_warmup_rounds = nb_warmup_rounds
        self.target_tx_rate = target_tx_rate
        self.tx_size = tx_size
        self.host = host
        self.port = port
        self.bitcoin_cli = bitcoin_path + "/bitcoin-cli"

        self.datadir = datadir
        self._cli = TestNodeCLI(str(self.bitcoin_cli), str(self.datadir))

        self._stopped = True

        self.logger = get_logger(log_fd)


    def send_transactions(self, target_tx_rate: int, tx_size: int):
        from test_framework.authproxy import JSONRPCException
        from test_framework.script_util import key_to_p2wpkh_script
        from test_framework.messages import (
            COIN,
            COutPoint,
            CTransaction,
            CTxIn,
            CTxOut,
            tx_from_hex,
        )

        logger = self.logger
        counter = 0
        logger.info("Transactions size: %s byte", tx_size)
        nb_tx_outputs = int(tx_size/180) + 1
        logger.info("Transactions will have %s outputs", nb_tx_outputs)
        logger.info("Transactions rate: %s tx/sec", target_tx_rate)
        target_byte_rate = target_tx_rate * tx_size
        logger.info("Block space rate: %s byte/sec", target_byte_rate)
        logger.info("Start sending transactions")
        num_addresses = 10
        target_addresses = [ self._cli.getnewaddress() for i in range(num_addresses) ]
        validate_addresses = { address: self._cli.validateaddress(address) for address in target_addresses }
        sleep_time=0.1
        start_time = time.time()
        total_bytes_sent = 0

        # timings
        t1 = 0
        t2 = 0
        t3 = 0
        t4 = 0
        t5 = 0
        t6 = 0
        t7 = 0

        cum_ms_listunspent = 0
        cum_ms_coinselect = 0
        cum_ms_create = 0
        cum_ms_sign = 0
        cum_ms_send = 0
        cum_ms = 0

        query_unspent = True
        unspent_coins = []
        coin_selected_idxs = []

        # Warmup rouonds are used
        # During warmup round of {}s, the number of outputs is always big (e.g. 200), independently from the tx_size parameters
        # This is useful to increase the number of UTXO in the client wallet
        warmup_rounds = 0

        while not self._stopped:
            # Create transactions as long as there are funds
            try:
                t1 = time.perf_counter_ns()
                if query_unspent:
                    unspent_coins = self._cli.listunspent(1)
                else:
                    new_unspent_coins = []
                    for i,coin in enumerate(unspent_coins):
                        if i not in coin_selected_idxs:
                            new_unspent_coins.append(coin)
                    unspent_coins = new_unspent_coins
                coin_selected_idxs = []

                # During warmup (first rounds) use a very high number of outputs
                if warmup_rounds < self.nb_warmup_rounds:
                    nb_tx_outputs = 200
                    logger.info(f"This is the {warmup_rounds+1}-th warmup round of {self.nb_warmup_rounds}, transactions will have {nb_tx_outputs} outputs")
                else:
                    nb_tx_outputs = int(tx_size/180) + 1
                    logger.info("This is an actual round, transactions will have %s outputs", nb_tx_outputs)

                current_balance = sum([unspent["amount"] for unspent in unspent_coins])
                t2 = time.perf_counter_ns()
                current_time = time.time()
                current_byte_rate = total_bytes_sent / (current_time - start_time)
                amount_sent_per_output = 0.001
                required_balance = (nb_tx_outputs + 1) * amount_sent_per_output
                logger.info(f"Client has {len(unspent_coins)} UTXO, {current_balance=}, {required_balance=}")
                if current_balance < required_balance:
                    logger.info(f"Insufficient balance {current_balance}. Required balance: {required_balance}")
                    query_unspent = True
                    sleep_time=0.5
                elif current_byte_rate < target_byte_rate:
                    # We have sufficient balance, we will send a transaction1

                    # Increase the number of warmup round of {}s, until num_warmup_rounds
                    if warmup_rounds<self.nb_warmup_rounds:
                        warmup_rounds += 1

                    # Next time do not query unspent, use
                    query_unspent = False
                    # tx_outputs must be a dict {"target_address": amount, "data": "HEX"}
                    tx_outputs = []
                    for i in range(nb_tx_outputs):
                        target_address = target_addresses[i % len(target_addresses) ]
                        tx_out = {}
                        tx_out[target_address] = amount_sent_per_output
                        tx_outputs.append(tx_out)
                    logger.info(f"Next transaction has {len(tx_outputs)} outputs")

                    # Select the inputs
                    total_in_output = nb_tx_outputs*amount_sent_per_output
                    total_to_cover_with_inputs = (nb_tx_outputs + 1) * amount_sent_per_output
                    # tx_inputs must be a list '[{"txid" : "12b8..e7ad", "vout" : 0}]
                    tx_inputs = []

                    total_covered_with_inputs = 0
                    while total_covered_with_inputs<total_to_cover_with_inputs:
                        i = randrange(len(unspent_coins))
                        while i in coin_selected_idxs:
                            i = randrange(len(unspent_coins))
                        coin_selected_idxs.append(i)
                        coin_selected = unspent_coins[i]
                        tx_input = {
                            "txid": coin_selected["txid"],
                            "vout": coin_selected["vout"]
                        }
                        tx_inputs.append(tx_input)
                        total_covered_with_inputs += coin_selected["amount"]

                    logger.info(f"Next transaction has {len(tx_inputs)} inputs")
                    # Calculate approx transaction size
                    # Heuristic from https://bitcoinops.org/en/tools/calc-size/
                    tx_overhead_size = 10
                    tx_n_inputs = len(tx_inputs)
                    tx_n_outputs = len(tx_outputs)
                    logger.info(f"Current transaction size: {tx_n_inputs=}, {tx_n_outputs=}")

                    tx_approx_vbyte = tx_overhead_size + 148*tx_n_inputs + 34*tx_n_outputs
                    tx_approx_fee_sat = 5*tx_approx_vbyte
                    tx_change = total_covered_with_inputs\
                        - Decimal(total_in_output).quantize(Decimal("1.00000000"))\
                        - Decimal(tx_approx_fee_sat/1e8).quantize(Decimal("1.00000000"))
                    # This modifies the tx_outputs list in place
                    first_output = tx_outputs[0]
                    first_output[list(first_output.keys())[0]] = float( Decimal( first_output[list(first_output.keys())[0]] ).quantize(Decimal("1.00000000")) + tx_change )
                    # tx_outputs = [tx_outputs[i] if i>0 else first_output for i in range(len(tx_outputs))]

                    t3 = time.perf_counter_ns()
                    tx_obj = CTransaction()
                    tx_obj.nVersion = 1
                    tx_obj.nLockTime = 0xFFFFFFFE
                    tx_obj.vin = [CTxIn( COutPoint(int(txin["txid"], 16), txin["vout"]), nSequence=0xFFFFFFFF) for txin in tx_inputs]
                    for txout in tx_outputs:
                        amount = int( list(txout.values())[0] * COIN )
                        address = list(txout.keys())[0]
                        witnessprogram = validate_addresses[address]["scriptPubKey"]
                        ctxout = CTxOut(amount, bytes.fromhex(witnessprogram))
                        tx_obj.vout.append(ctxout)

                    tx_draft = tx_obj.serialize_without_witness().hex()
                    t4 = time.perf_counter_ns()
                    # tx_funded = self._cli.fundrawtransaction(tx_draft, '{"fee_rate":1}')
                    t5 = time.perf_counter_ns()
                    tx_final = self._cli.signrawtransactionwithwallet(tx_draft)
                    t6 = time.perf_counter_ns()
                    txhash = self._cli.sendrawtransaction(tx_final["hex"])
                    t7 = time.perf_counter_ns()
                    # sendtoaddress_args = dict(address=target_addresses[0], amount=amount_sent, fee_rate=1)
                    # txhash = self._cli.sendtoaddress(**sendtoaddress_args)
                    # tx_final = self._cli.gettransaction(txhash)
                    tx_byte_len = len(tx_final['hex'])/2
                    total_bytes_sent += tx_byte_len
                    logger.info(f"Sent transaction number {counter} with hash {txhash} of size {tx_byte_len:.0f}")
                    counter += 1
                    current_time = time.time()
                    current_byte_rate = total_bytes_sent / (current_time - start_time)
                    logger.info(f"Current_byte_rate {current_byte_rate:.2f} bytes/sec, target_rate {target_byte_rate:.2f} bytes/sec")
                    sleep_time -= 0.1
                else:
                    sleep_time += 0.1

                # compute timings and print them
                cur_ms_listunspent = int((t2 - t1) / 1_000_000)
                cur_ms_coinselect = int((t4 - t3) / 1_000_000)
                cur_ms_create = int((t5 - t4) / 1_000_000)
                cur_ms_sign = int((t6 - t5) / 1_000_000)
                cur_ms_send = int((t7 - t6) / 1_000_000)
                cur_ms_tot = cur_ms_listunspent + cur_ms_coinselect + cur_ms_create + cur_ms_sign + cur_ms_send

                percent_cur_listunspent = 100 * cur_ms_listunspent / cur_ms_tot
                percent_cur_coinselect = 100 * cur_ms_coinselect / cur_ms_tot
                percent_cur_create = 100 * cur_ms_create / cur_ms_tot
                percent_cur_sign = 100 * cur_ms_sign / cur_ms_tot
                percent_cur_send = 100 * cur_ms_send / cur_ms_tot

                cum_ms_listunspent += cur_ms_listunspent
                cum_ms_coinselect += cur_ms_coinselect
                cum_ms_create += cur_ms_create
                cum_ms_sign += cur_ms_sign
                cum_ms_send += cur_ms_send
                cum_ms_tot = cum_ms_listunspent + cum_ms_coinselect + cum_ms_create + cum_ms_sign + cum_ms_send

                percent_cum_walletinfo = 100 * cum_ms_listunspent / cum_ms_tot
                percent_cum_create = 100 * cum_ms_coinselect / cum_ms_tot
                percent_cum_fund = 100 * cum_ms_create / cum_ms_tot
                percent_cum_sign = 100 * cum_ms_sign / cum_ms_tot
                percent_cum_send = 100 * cum_ms_send / cum_ms_tot

                logger.info(
                    f"Current iteration timings: "
                    f"{cur_ms_tot=}, "
                    f"{cur_ms_listunspent=}, ({percent_cur_listunspent:.0f}%), "
                    f"{cur_ms_coinselect=}, ({percent_cur_coinselect:.0f}%), "
                    f"{cur_ms_create=}, ({percent_cur_create:.0f}%), "
                    f"{cur_ms_sign=}, ({percent_cur_sign:.0f}%), "
                    f"{cur_ms_send=}, ({percent_cur_send:.0f}%)"
                )
                logger.info(
                    f"Cumulative timings: "
                    f"{cum_ms_tot=}, "
                    f"{cum_ms_listunspent=:.0f} ({percent_cum_walletinfo:.0f}%), "
                    f"{cum_ms_coinselect=:.0f} ({percent_cum_create:.0f}%), "
                    f"{cum_ms_create=:.0f} ({percent_cum_fund:.0f}%), "
                    f"{cum_ms_sign=:.0f} ({percent_cum_sign:.0f}%), "
                    f"{cum_ms_send=:.0f} ({percent_cum_send:.0f}%)"
                )
            except JSONRPCException as e: # Initialization phase
                logger.error(f"Error sending transactions {str(e)}.")
            except subprocess.CalledProcessError:
                logger.error("failed to interact with bitcoind")
            except Exception as e:
                logger.error(f"Some other error: {str(e)}.")
            # Sleep
            if sleep_time > 0:
                logger.info(f"Sleep for {sleep_time:.3f}s")
                time.sleep(sleep_time)
        logger.info("Send transaction job is done")

    def wait_for_tx(self, host: str, port: int, topic: str):
        logger = self.logger
        logger.info(f"Subscribing to {host}:{port} on topic {topic}...")

        # Prepare our context and publisher
        context = zmq.Context()
        subscriber = context.socket(zmq.SUB)
        subscriber.connect(f"tcp://{host}:{port}")
        subscriber.setsockopt(zmq.SUBSCRIBE, topic.encode(encoding="ascii"))

        while not self._stopped:
            # Read envelope with address
            logger.info(f"Waiting for notifications from {topic}")
            result = subscriber.recv_multipart()
            logger.info("got notification, parsing...")
            [topic, tx_hash_bytes, seqnumber_bytes] = result
            seqnumber = int.from_bytes(seqnumber_bytes, byteorder="little")
            tx_hash = tx_hash_bytes.hex()
            logger.info(f"seqnumber: {seqnumber}, tx hash={tx_hash}")

        # We never get here but clean up anyhow
        subscriber.close()
        context.term()
        logger.info("ZMQ monitor is done")

    def stop(self):
        self._stopped = True

    def sighandler(self, sig, frame):
        logger = self.logger
        if sig == signal.SIGINT or sig == signal.SIGTERM:
            logger.info("Received signal %s, stopping...", sig)
            self.stop()

    def run(self) -> None:
        logger = self.logger
        self._stopped = False
        signal.signal(signal.SIGINT, self.sighandler)
        signal.signal(signal.SIGTERM, self.sighandler)

        thread_2 = Thread(target=self.wait_for_tx, args=[self.host, self.port, ZMQ_PUB_HASH_TX_TOPIC_NAME])
        thread_2.start()

        self.send_transactions(self.target_tx_rate, self.tx_size)
        thread_2.join()
        logger.info("Client stopped gracefully")


@click.command("run-client")
@click.option(
    "--bitcoin-path",
    type=click.Path(exists=False, file_okay=False, dir_okay=True),
    required=True,
    help="Path to bitcoin binary.",
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
    help="Id of the client.",
)
@click.option(
    "--nb-clients",
    type=click.IntRange(min=1),
    required=True,
    help="Number of clients in the network.",
)
@click.option(
    "--warmup-duration",
    type=int,
    required=True,
    default=30,
    help="Number of seconds at the beginning of the experiment that is not used for measurements.",
)
@click.option(
    "--block-time",
    type=int,
    required=True,
    default=60,
    help="The blockchain time between blocks.",
)
@click.option(
    "--target-tx-rate",
    type=float,
    required=True,
    default=500,
    help="Target tx rate.",
)
@click.option(
    "--tx-size",
    type=int,
    required=True,
    default=512,
    help="Tx size.",
)
@click.option(
    "--host", type=IpAddrParamType(), default=ipaddress.ip_address("127.0.0.1"),
    help="host for the ZMQ publisher socket"
)
@click.option("--port", type=click.IntRange(min=1024, max=65535), default=29110, help="port for the ZMQ publisher socket")
def main(bitcoin_path, itcoin_core_test_framework_path, id: int, nb_clients: int, warmup_duration: int, block_time: int, target_tx_rate: float, tx_size: int, host: str, port: int) -> None:
    client_name = get_node_dirname(id, NodeType.CLIENT, nb_clients)
    with open(PathMaker.node_logs_path(client_name) + '/client_logs.txt', 'w') as fd:
        output_dir = Path(PathMaker.output_path()).resolve()
        client_dir = output_dir / client_name
        nb_warmup_rounds = int(warmup_duration/block_time)
        client = Client(bitcoin_path, itcoin_core_test_framework_path, client_dir, nb_warmup_rounds, target_tx_rate/nb_clients, tx_size, host, port, fd)
        client.run()

if __name__ == "__main__":
    main()
