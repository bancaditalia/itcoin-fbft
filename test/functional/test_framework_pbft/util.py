import json
import os

from test_framework.util import (
    get_datadir_path,
    p2p_port,
    rpc_port,
    MAX_NODES
)

def pbft_p2p_ports(n, from_node=None):
    """
    Returns a port for replica and a port for the client, to be used in combination with toxiproxy.

    Given a couple of nodes P,Q, we have:
    PBFT_REPLICA_P --(toxiproxy listen port to Q from node P)--> TOXIPROXY_P_TO_Q --(toxiproxy upstream port to Q)--> PBFT_REPLICA_Q
    and simmetrically
    PBFT_REPLICA_Q --(toxiproxy listen port to P from node Q)--> TOXIPROXY_Q_TO_P --(toxiproxy upstream port to P)--> PBFT_REPLICA_P

    If from_node is not set, or n==from_node, then the result is the listening port of replica n, to be used in the toxiproxy upstream.
    Otherwise, toxiproxy listen port to m from node from_node is returned.

    :param n: the target replica, that implements the service. In toxiproxy, this node is the one which provides the upstream
    :param from_node: the source node, whose requests are proxied
    :returns a list [replica_port, client_port]
    """

    # from_node is used in the toxiproxy setup
    if from_node is None:
        from_node = n
    replica_port = p2p_port(from_node) + MAX_NODES + 1 + n*MAX_NODES
    client_port =  p2p_port(from_node) + MAX_NODES + 1 + MAX_NODES**2 + n*MAX_NODES
    return [replica_port, client_port]

def pbft_rpc_port(n):
    return rpc_port(n) + MAX_NODES

# Node functions
################

def initialize_pbft_datadir(dirname, n, chain, signet_challenge: str, pbft_conf_dicts = []):
    # NOTE: for now, pbft_conf_dict has a default empty parameter to avoid disrupting existing code
    datadir = get_datadir_path(dirname, n)
    if not os.path.isdir(datadir):
        os.makedirs(datadir)
    # Translate chain name to config name
    if chain == 'testnet3':
        chain_name_conf_arg = 'testnet'
        chain_name_conf_section = 'test'
    else:
        chain_name_conf_arg = chain
        chain_name_conf_section = chain
    with open(os.path.join(datadir, "bitcoin.conf"), 'w', encoding='utf8') as f, \
        open(os.path.join(datadir, "miner.conf.json"), 'w', encoding='utf8') as p:
        #bitcoin conf
        f.write("{}=1\n".format(chain_name_conf_arg))
        f.write("[{}]\n".format(chain_name_conf_section))
        f.write("signetchallenge=" + signet_challenge + "\n")
        f.write("port=" + str(p2p_port(n)) + "\n")
        f.write("rpcport=" + str(rpc_port(n)) + "\n")
        f.write("fallbackfee=0.0002\n")
        f.write("server=1\n")
        f.write("keypool=1\n")
        f.write("discover=0\n")
        f.write("dnsseed=0\n")
        f.write("listenonion=0\n")
        f.write("printtoconsole=0\n")
        f.write("upnp=0\n")
        f.write("shrinkdebugfile=0\n")
        # pbft conf
        pbft_conf_dict = list(filter(lambda x : x["id"] == n, pbft_conf_dicts))[0]
        json.dump(pbft_conf_dict, p, indent=2)
    os.makedirs(os.path.join(datadir, 'stderr'), exist_ok=True)
    os.makedirs(os.path.join(datadir, 'stdout'), exist_ok=True)
    return datadir
