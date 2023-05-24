import logging
import os
import subprocess
import tempfile
import sys
import time
import json
import grpc

from test_framework.conftest import ITCOIN_PBFT_ROOT_DIR
from test_rpc_pb2_grpc import NodeTestRpcStub
from test_rpc_pb2 import NodeInfo

class FailedToStartError(Exception):
    """Raised when a node fails to start correctly."""

class TestNodePbft():
  def __init__(self, i, datadir, *, config, pbftd, cwd, extra_args=None):
    self.index = i
    self.datadir = datadir
    self.stdout_dir = os.path.join(self.datadir, "stdout")
    self.stderr_dir = os.path.join(self.datadir, "stderr")
    self.binary = pbftd
    self.config = config
    self.cwd = cwd

    self.extra_args = extra_args
    self.args = [
      self.binary,
      "-datadir=" + self.datadir
    ]

    self.running = False
    self.process = None

    self.log = logging.getLogger('TestNodePbft%d' % i)
    self.log.setLevel(logging.DEBUG)
    self.log.propagate = False
    logger_formatter = logging.Formatter(fmt='%(asctime)s.%(msecs)03d000Z %(name)s (%(levelname)s): %(message)s', datefmt='%Y-%m-%dT%H:%M:%S')
    stdout_handler = logging.StreamHandler(sys.stdout)
    stdout_handler.setLevel(logging.DEBUG)
    stdout_handler.setFormatter(logger_formatter)
    self.log.addHandler(stdout_handler)

  def start(self, extra_args=None, *, cwd=None, stdout=None, stderr=None, **kwargs):
    if extra_args is None:
      extra_args = self.extra_args

    # Add a new stdout and stderr file each time bitcoind is started
    if stderr is None:
        stderr = tempfile.NamedTemporaryFile(dir=self.stderr_dir, prefix="pbft_", delete=False)
    if stdout is None:
        stdout = tempfile.NamedTemporaryFile(dir=self.stdout_dir, prefix="pbft_", delete=False)
    self.stderr = stderr
    self.stdout = stdout

    if cwd is None:
      cwd = self.cwd

    subp_env = dict(os.environ,
      LD_LIBRARY_PATH=f'$LD_LIBRARY_PATH:{ITCOIN_PBFT_ROOT_DIR}/usrlocal/lib'
    )
    self.process = subprocess.Popen(self.args, env=subp_env, stdout=stdout, stderr=stderr, cwd=cwd, **kwargs)
    self.running = True
    self.log.debug("pbftd started, waiting for RPC to come up")

  def _node_msg(self, msg: str) -> str:
      """Return a modified msg that identifies this node by its index as a debugging aid."""
      return "[pbft %d] %s" % (self.index, msg)

  def stop_node(self, expected_stderr='', wait=0):
    self.process.terminate()

  def get_info(self):
    id = self.config['id']
    host = self.config['pbft_client_set'][id]['host']
    port = str(self.config['test_rpc_port'])
    channel = grpc.insecure_channel(host + ':' + port)
    stub = NodeTestRpcStub(channel)
    request = NodeInfo()
    response = stub.GetInfo(request)
    return response

  def wait_for_rpc_connection(self):
    # it should be a succesfull getinfo on the GRPC interface
    timeout = 10
    poll_per_sec = 4
    for _ in range(poll_per_sec * timeout):
      if self.process.poll() is not None:
        raise FailedToStartError(self._node_msg(
        'pbftd exited with status {} during initialization'.format(self.process.returncode)))
      try:
        self.get_info()
        return
      except grpc.RpcError as error:
        if (error.code()==grpc.StatusCode.UNAVAILABLE):
          self.log.debug(f'Pbft node {self.index} not yet started, returned {str(error.code())}...')
        else:
          self.log.error(f'Pbft node returned error: {str(error.code())}')
      time.sleep(1.0/poll_per_sec)
    raise AssertionError("Unable to connect after {}s".format(timeout))

  def wait_until_stopped(self):
    # TODO this is only a placeholder
    time.sleep(1.0)
