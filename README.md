# itcoin-pbft

A Practical Byzantine Fault Tolerant consensus algorithm for the itcoin blockchain

## Clone itcoin-pbft and install dependencies

Clone the repository `bancaditalia/itcoin-pbft`:

```bash
cd ~
git clone git@github.com:bancaditalia/itcoin-pbft.git
cd itcoin-pbft
```

Install itcoin-pbft build dependencies with apt (system-wide):

```
sudo apt install --no-install-recommends -y \
    autoconf \
    automake \
    bsdmainutils \
    ca-certificates \
    cmake \
    g++-10 \
    gcc-10 \
    git \
    jq \
    libargtable2-dev \
    libboost-filesystem1.71-dev \
    libboost-log1.71-dev \
    libboost-program-options1.71-dev \
    libboost-test1.71-dev \
    libboost-thread1.71-dev \
    libcurl4-openssl-dev \
    libdb5.3++-dev \
    libevent-dev \
    libsqlite3-dev \
    libssl-dev \
    libtool \
    libzmqpp-dev \
    make \
    openssh-client \
    pkg-config \
    python3 \
    xxd \
    zlib1g-dev
```

* Python dependencies for the functional test framework

Grpc and grpc tools:

```bash
pip install --user future grpcio grpcio-tools
pip install --user --upgrade protobuf
```

The toxiproxy server and python client:

```bash
# Toxyproxi server
wget https://github.com/Shopify/toxiproxy/releases/download/v2.2.0/toxiproxy_2.2.0_linux_amd64.deb
sudo dpkg -i toxiproxy_2.2.0_linux_amd64.deb

# Python client
cd ~
git clone https://github.com/douglas/toxiproxy-python.git
```

## Build the project

Run the following command:

```
mkdir build
cd build
cmake [-DCMAKE_BUILD_TYPE=Debug] [-DITCOIN_CORE_SRC_DIR=path/to/itcoin-core] [-DITCOIN_CORE_GIT_REV=itcoin] [-DITCOIN_CORE_GIT_URL=git@github.com:bancaditalia/itcoin-core] [-DITCOIN_SECP256K1_SRC_DIR=path/to/itcoin-secp256k1] [-DITCOIN_SECP256K1_GIT_URL=git@github.com:bancaditalia/itcoin-secp256k1] [-DITCOIN_SECP256K1_GIT_REV=library-renaming] [-DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10] ..
make -j$(nproc --ignore=1)
```

Where:
- `DCMAKE_BUILD_TYPE` specifies the build type to use. Typical values include Debug and Release;
- `ITCOIN_CORE_SRC_DIR` is the path to a local copy of the itcoin-core repository (default is not to use it and clone the default rev from GitHub);
- `ITCOIN_CORE_GIT_URL` is the remote to checkout from (default to GitHub remote via SSH: `git@github.com:bancaditalia/itcoin-core`);
- `ITCOIN_CORE_GIT_REV` is the revision to checkout from GitHub (default: `itcoin`);
- `ITCOIN_SECP256K1_SRC_DIR` as in `ITCOIN_CORE_SRC_DIR`, but for the `itcoin-secp256k1` dependency;
- `ITCOIN_SECP256K1_GIT_URL`: as in `ITCOIN_CORE_GIT_URL`, but for the `itcoin-secp256k1` dependency;
- `ITCOIN_SECP256K1_GIT_REV`: as in `ITCOIN_CORE_GIT_REV`, but for the `itcoin-secp256k1` dependency;
- `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` are respectively the C and C++ compiler executables. Only gcc is supported. Minimum version is 10 (default: use system compilers).

Setting the variable `ITCOIN_CORE_SRC_DIR` nullifies the effect of `ITCOIN_CORE_GIT_REV`.

## Run itcoin-core nodes

Run itcoind node with

```bash
cd ~/itcoin-pbft/infra
# Replace 0 with the id of the node to start
./bitcoind.sh 0
```

To spawn the Bitcoin Daemon nodes all at once:
```
./infra/start-infra.sh
```

## Build and run itcoin-pbft nodes

```bash
cd ~/itcoin-pbft
make
make run
```

## Run tests

* Spawn the Bitcoin Daemon nodes:
```
infra/start-infra.sh
```

* Build the tests:
```
cd build
make main-test
```

* Run the tests:
```
make test
```

* To run a specific subset of tests, use:
```
ctest --tests-regex <test_name> --verbose
```
e.g.:
```
ctest --tests-regex test_transport_metadata --verbose
ctest --tests-regex test_pbft_ --verbose
```

* The list of available tests can be obtained via:

```
ctest --show-only [--verbose]
```

## Tips and tricks

### Using /fast to build unit tests without dependencies

```bash
cd build
# Build itcoin-core
make -j$(nproc --ignore=1) -C <itcoin-core path>
# Generate source files
make -j$(nproc --ignore=1) generated_src/fast
# Build itcoin-pbft library
make -j$(nproc --ignore=1) itcoin-pbft/fast
# Build the test executable
make -j$(nproc --ignore=1) main-test/fast
```

Example, in one line.

```bash
make -j8 -C ../../itcoin-core && make -j8 generated_src/fast && make -j8 itcoin-pbft/fast && make -j8 main-test/fast
```

### Wallet files version control management

Assume wallet files as unchanged during development:

```bash
git update-index --skip-worktree infra/node00/signet/wallets/itcoin_signer/wallet.dat
git update-index --skip-worktree infra/node01/signet/wallets/itcoin_signer/wallet.dat
git update-index --skip-worktree infra/node02/signet/wallets/itcoin_signer/wallet.dat
git update-index --skip-worktree infra/node03/signet/wallets/itcoin_signer/wallet.dat
```

When you need to commit wallet files or checkout a different branch:

```
git update-index --no-skip-worktree infra/node00/signet/wallets/itcoin_signer/wallet.dat
git update-index --no-skip-worktree infra/node01/signet/wallets/itcoin_signer/wallet.dat
git update-index --no-skip-worktree infra/node02/signet/wallets/itcoin_signer/wallet.dat
git update-index --no-skip-worktree infra/node03/signet/wallets/itcoin_signer/wallet.dat
```

### Build with docker (experimental)

In order to check if the project can be built successfully on different
operating systems, some Dockerfiles are provided.

Please note that **for now you can only verify that the build succeeds**: there
is no support for running the project or its tests yet.

#### Build with Docker

On a machine with a modern Docker (>= 20.10) and ssh agent correctly set up (see
next session if you do not know what ssh-agent forwarding is) use any of the
following:

```
DOCKER_BUILDKIT=1 docker build --ssh default --file Dockerfile.ubuntu20.04 .
DOCKER_BUILDKIT=1 docker build --ssh default --file Dockerfile.ubuntu22.04 .
DOCKER_BUILDKIT=1 docker build --ssh default --file Dockerfile.fedora36 .
```

If your system is running selinux in enforcing mode (for example, Fedora 36),
you will have to temporarily disable selinux (`sudo setenforce 0`) before doing
the build.

This is caused by a known issue in buildkit
(https://github.com/moby/buildkit/issues/2320).

#### Setting up ssh agent forwarding

The Docker build will need to clone the private GitHub repository
`git@github.com:bancaditalia/itcoin-core.git` via ssh. It will use credentials
forwarded via `ssh-agent` from your local user/host.

1. verify that the current user in your host has the needed access rights:
   ```bash
   TMP_DIR=$(mktemp --directory --tmpdir clonetest.XXXX) && ((git clone --filter=blob:none --sparse --depth=1 git@github.com:bancaditalia/itcoin-core.git "${TMP_DIR}" >/dev/null 2>&1 && echo "YOU HAVE ACCESS" ) || echo "CLONE FAILED") && rm -rf "${TMP_DIR}"
   ```
   If the above command prints `YOU HAVE ACCESS`, then proceed to point 2;

2. Verify that the following command prints at least a line of the given form:
   ```
   $ ssh-add -L # this must print at least a line of the form:
   ssh-rsa xxxxxxx user@yourpc
   ```

   If something is printed, you can proceed to build, otherwise continue to
   point 3.

3. enable ssh-agent via:
   ```
   $ eval $(ssh-agent)
   Agent pid 239525
   ```

   Add you ssh key to the agent:
   ```
   ssh-add ~/.ssh/id_rsa # or your preferred algorithm...
   ```

   Then go back to point 2 above.

The agent forwarding works even if you are performing the build in a remote
untrusted server (for example, if your host is windows and you are running the
build in a VM, or if you use linux but are doing the build in a VM in the
cloud).

Just verify that your daemon is forwarding the ssh-agent (most daemons do this),
execute the `ssh-add -L` command in the remove VM and verify it prints
something.
