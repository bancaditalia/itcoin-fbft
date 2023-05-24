# VSCode debug popups on glibc

- https://github.com/microsoft/vscode-cpptools/issues/811

```bash
sudo apt install glibc-source
cd /usr/src/glibc
sudo tar xvf glibc-2.31.tar.xz
sudo mkdir /build
sudo ln -s /usr/src/glibc /build/glibc-eX1tMB
```

# Valgrind

sudo apt-get install -y valgrind

# Debugger useful commands ...

-exec p signet_txs->m_to_spend.GetHash()

block.hashMerkleRoot.GetHex()

signet_txs->m_to_sign.vin[0].prevout.hash.GetHex()

signet_merkle.GetHex()

HexStr(witness_commitment)

witness_commitment.size()
