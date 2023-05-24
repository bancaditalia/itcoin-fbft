-----------  to_spend or spendme or m_to_spend  -----------

nVersion = 0
nLockTime = 0
vin = [
  {
    outpoint: Outpoint(hash=0, n=0xFFFFFFFF), 
    scriptSig: b"\x00" + CScriptOp.encode_op_pushdata(serialized_block_header), 
    nsequence: 0
  }
]
vout = [
  {
    nValue = 0,
    scriptPubKey = signet_challenge
  }
]


-----------  spend or signme or m_to_sign  -----------

nVersion = 0
nLockTime = 0
vin = [
  {
    outpoint: Outpoint(hash=to_spend.sha256, n=0),
    scriptSig: b"",
    nsequence: 0
  }
]
vout = [
  {
    nValue = 0,
    scriptPubKey = b"\x6a"
  }
]

-----------  spend  during verification -----------