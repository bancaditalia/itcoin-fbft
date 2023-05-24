This shows an example of how to sign a message and verify its signature using the bitcoin internal wallet.
The example uses the wallet of node01

```bash
BITCOIN_CLI="./bitcoin-cli.sh 0 "

$BITCOIN_CLI loadwallet itcoin_signer true

PUBKEY="03baff8453b64e0db96d7c904174cc775d35823bd7e064e957db461ff8d97a39c8"
echo $PUBKEY

PUBKEY_DESCRIPTOR=$( $BITCOIN_CLI getdescriptorinfo "pkh($PUBKEY)" | jq --raw-output .descriptor )
echo $PUBKEY_DESCRIPTOR

PUBKEY_P2PKH_ADDRESS=$( $BITCOIN_CLI deriveaddresses $PUBKEY_DESCRIPTOR | jq --raw-output .[0] )
echo $PUBKEY_P2PKH_ADDRESS

# Doesnt work with spaces!!
MESSAGE="HelloWorld"
echo $MESSAGE

MESSAGE_SIG=$( $BITCOIN_CLI signmessage $PUBKEY_P2PKH_ADDRESS $MESSAGE )
echo $MESSAGE_SIG

SIG_VERIFY=$( $BITCOIN_CLI verifymessage $PUBKEY_P2PKH_ADDRESS $MESSAGE_SIG $MESSAGE )
echo $SIG_VERIFY
```
