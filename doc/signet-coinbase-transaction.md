# Signet Coinbase Transaction

Output0: newly minted coins
  Amount: 100 coin
  ScriptPublicKey: whatever spending condition, e.g. central bank public key

Output1
  Amount: 0 coin
  ScriptPublicKey: OP_RETURN pushdata(WITNESS_HEADER, witness_merkle_root) pushdata(SIGNET_HEADER, signet_solution)

Output2
  Amount: 0 coin
  ScriptPublicKey: OP_RETURN pushdata(whatever)

NB: There is 80 byte limitation for OP_RETURN pushdata, nevertheless this is a relay standard, but not a consensus rule. This can be modified on a per-node basis with `bicoind -datacarriersize` flag. See https://bitcoin.stackexchange.com/questions/78572/op-return-max-bytes-clarification

## Example

- WITNESS_HEADER = aa21a9ed
- SIGNET_HEADER  = ecc7daa2
- OP_PUSHDATA1 = 0x4c
- OP_PUSHDATA2 = 0x4d
- OP_PUSHDATA4 = 0x4e

6a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf94c4fecc7daa249004730440220458acc6a9fde979f3691bebf65c32c143d960f86029f16e6057ca67f1071793402207a91f435c733f19f57aeeec95bf13c24f10eb6c08882f025e37a3db260b8b3f70100

6a  24  aa21a9ed  e2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf9  4c4f  ecc7daa2  49004730440220458acc6a9fde979f3691bebf65c32c143d960f86029f16e6057ca67f1071793402207a91f435c733f19f57aeeec95bf13c24f10eb6c08882f025e37a3db260b8b3f70100

- OP_RETURN = 6a
- 24-hex means 36 bytes, i.e. 72 chars, i.e. 4 WITNESS_HEADER + 32 bytes commitment hash
- WITNESS_HEADER 
- commitment hash from e2...f9
- 4c4f-hex explaination SIGNET_HEADER + signet_solution size is 158 chars, i.e. 79 bytes, i.e. 4f hex. This is >= OP_PUSHDATA1 but less than 0xff, so it goes into the second if in script.h, first put OP_PUSHDATA1 then actual size, that is 47, then the actual data
- SIGNET_HEADER
- signet_solution = 49004730440220458acc6a9fde979f3691bebf65c32c143d960f86029f16e6057ca67f1071793402207a91f435c733f19f57aeeec95bf13c24f10eb6c08882f025e37a3db260b8b3f70100


## Example2

6a  24  aa21a9ed  ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  0c  ecc7daa2  abababababababab
