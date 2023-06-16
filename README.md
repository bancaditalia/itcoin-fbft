# Frosted Byzantine Fault Tolerance (FBFT)

This repository contains the implementation of an itcoin "consensus node",
which implements the Frosted Byzantine Fault Tolerance (FBFT) Proof-of-Authority consensus algorithm for the **itcoin** blockchain.
You can find more information about itcoin at [the project web page](https://bancaditalia.github.io/itcoin).

Please note that this software is solely intended for testing and experimentation purposes, and is not ready for use in a production environment: In its current form, it misses features — such as support for dynamic federations — that are crucial for any real-world deployment.

👇👇👇 Hereafter you find the instructions to get started with FBFT. 👇👇👇

## Quick start on Ubuntu 22.04 LTS

1. Install build and dev dependencies with apt (system-wide):

```
sudo apt install --no-install-recommends -y \
    autoconf \
    automake \
    bsdextrautils \
    ca-certificates \
    cmake \
    g++ \
    gcc \
    git \
    jq \
    libtool \
    make \
    openssh-client \
    pkg-config \
    python3 \
    tmux \
    xxd \
    zlib1g-dev

sudo apt install --no-install-recommends -y \
    libargtable2-dev \
    libboost-filesystem1.74-dev \
    libboost-log1.74-dev \
    libboost-program-options1.74-dev \
    libboost-test1.74-dev \
    libboost-thread1.74-dev \
    libcurl4-openssl-dev \
    libdb5.3++-dev \
    libevent-dev \
    libsqlite3-dev \
    libssl-dev \
    libzmq3-dev \
    swi-prolog
```

2. Clone the repository `itcoin-core`

```bash
cd ~
git clone https://github.com/bancaditalia/itcoin-core.git
```

3. Clone the repository `bancaditalia/itcoin-fbft`, at the same level of `itcoin-core`:

```bash
cd ~
git clone https://github.com/bancaditalia/itcoin-fbft.git
```

4. Build the project

```
cd ~/itcoin-fbft
mkdir build
cd build
cmake -DITCOIN_CORE_SRC_DIR=../../itcoin-core ..
make -j$(nproc --ignore=1)
```

5. Reset and start 4 mining network bridge nodes.

This will setup a 4-nodes infrastructure with 10 seconds target block time.

```
cd ~/itcoin-fbft
./infra/reset-infra.sh 10
./infra/start-infra.sh
```

The above command will spawn 4 itcoin-core daemon nodes that are used for
testing.
You can query the state of the mining nodes using the command line.
After a reset, all nodes should have 0 blocks.

```
cd ~/itcoin-fbft
./infra/bitcoin-cli.sh 0 getblockchaininfo
./infra/bitcoin-cli.sh 1 getblockchaininfo
./infra/bitcoin-cli.sh 2 getblockchaininfo
./infra/bitcoin-cli.sh 3 getblockchaininfo
```

6. Start the miner consensus nodes

We suggest that you run the four nodes in different terminals:

```
cd ~/itcoin-fbft
build/src/main -datadir=infra/node00
build/src/main -datadir=infra/node01
build/src/main -datadir=infra/node02
build/src/main -datadir=infra/node03
```

## Run the tests

Ensure that the 4 mining network bridge nodes are up and running. Then launch:

```
cd ~/itcoin-fbft/build
make test
```
