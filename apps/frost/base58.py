# Copyright (c) 2012-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Bitcoin base58 encoding and decoding.

Based on https://bitcointalk.org/index.php?topic=1026.0 (public domain)
'''
import hashlib

# for compatibility with following code...
class SHA256:
    new = hashlib.sha256

if str != bytes:
    # Python 3.x
    def ord(c):
        return c
    def chr(n):
        return bytes( (n,) )

__b58chars = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
__b58base = len(__b58chars)
b58chars = __b58chars

def b58encode(v):
    """ encode v, which is a string of bytes, to base58.
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        if isinstance(c, str):
            c = ord(c)
        long_value += (256**i) * c

    result = ''
    while long_value >= __b58base:
        div, mod = divmod(long_value, __b58base)
        result = __b58chars[mod] + result
        long_value = div
    result = __b58chars[long_value] + result

    # Bitcoin does a little leading-zero-compression:
    # leading 0-bytes in the input become leading-1s
    nPad = 0
    for c in v:
        if c == 0:
            nPad += 1
        else:
            break

    return (__b58chars[0]*nPad) + result

def b58decode(v, length = None):
    """ decode v into a string of len bytes
    """
    long_value = 0
    for i, c in enumerate(v[::-1]):
        pos = __b58chars.find(c)
        assert pos != -1
        long_value += pos * (__b58base**i)

    result = bytes()
    while long_value >= 256:
        div, mod = divmod(long_value, 256)
        result = chr(mod) + result
        long_value = div
    result = chr(long_value) + result

    nPad = 0
    for c in v:
        if c == __b58chars[0]:
            nPad += 1
            continue
        break

    result = bytes(nPad) + result
    if length is not None and len(result) != length:
        return None

    return result

def checksum(v):
    """Return 32-bit checksum based on SHA256"""
    return SHA256.new(SHA256.new(v).digest()).digest()[0:4]

def b58encode_chk(v):
    """b58encode a string, with 32-bit checksum"""
    return b58encode(v + checksum(v))

def b58decode_chk(v):
    """decode a base58 string, check and remove checksum"""
    result = b58decode(v)
    if result is None:
        return None
    if result[-4:] == checksum(result[:-4]):
        return result[:-4]
    else:
        return None

def get_bcaddress_version(strAddress):
    """ Returns None if strAddress is invalid.  Otherwise returns integer version of address. """
    addr = b58decode_chk(strAddress)
    if addr is None or len(addr)!=21:
        return None
    version = addr[0]
    return ord(version)

MAINNET_PUBLIC = "0488B21E"
MAINNET_PRIVATE = "0488ADE4"
TESTNET_PUBLIC = "043587CF"
TESTNET_PRIVATE = "04358394"
PRIVKEY_PREFIX = "00"

def generate_extended_key(version, key, chaincode, depth = "00", parent_fingerprint = "00000000", child_number = "00000000"):
    return version + depth + parent_fingerprint + child_number + chaincode + key

if __name__ == '__main__':
    # ser_key_b58enc = "tprv8ZgxMBicQKsPeiU5xNau1YkrNQm7vFcKNscxwvVPsjY89yNPgeo4iebHEaghMS2oR1UcCVvntDWsLJDjyCnbYj55jtuVjHVRMfBWxqpSCqG"

    # # this decodes in bytes
    # decoded_bytes = b58decode_chk(ser_key_b58enc)
    # serialized_key = decoded_bytes.hex()
    # print(serialized_key)

    # serialized_key = "043587cf0307c56932800000001bf5a5cdc5a8b59d124f8f04c026a28b818013b6e753922dab6c36c36d8d0252034cc98461de71d100cc12f30fd449d7267612802edba5e83a8abfa5e18bf645bc"
    # serialized_key = "043583940000000000000000001d7d2a4c940be028b945302ad79dd2ce2afe5ed55e1a2937a5af57f8401e73dd003e2d5383c9e0165d8a033d0223529d8bc8332bc42bd75c4ee8549c082793e6a8"

    rnd_chaincode = "774f0e70c73bcb266170aa720c1615a9f1839d1d9f7717a2f32a396efcc2c4be"
    key = "e9e7e31250a4cb06d63f8b29979935f48eb848bb12463470ad5c2a9a0726c863"
    serialized_key = generate_extended_key(TESTNET_PRIVATE, PRIVKEY_PREFIX + key, rnd_chaincode)


    rnd_chaincode = "774f0e70c73bcb266170aa720c1615a9f1839d1d9f7717a2f32a396efcc2c4be"
    key = "035d8b4242b9658f9db0769f3bd255714aeb73d48af6a5a3cc3d7073540a47d0fc"
    serialized_key = generate_extended_key(TESTNET_PUBLIC, key, rnd_chaincode)

    print(serialized_key)

    serialized_key_bytes = bytes.fromhex(serialized_key)
    if (len(serialized_key) == 78 * 2):
        ser_key_b58enc = b58encode_chk(serialized_key_bytes)
        print("encoded chk: ", ser_key_b58enc)
    else:
        ser_key_b58enc = b58encode(serialized_key_bytes)
        print("encoded no chk: ", ser_key_b58enc)

    _r = b58decode("cR7gSzCypZfs5SxBHHzgeXsAjsX9KqifykQoZPQRs1HixSaX1rQN").hex()
    print("decode: ", _r)

    print(" ====== ===== ===== ====== ===== ===== ====== ===== ===== ====== ===== ===== ")
    print("  Itcoin Node Keys ")

    gpk = "03677d7198c8ce3b5012dba0196998b454c40236fa6c72ebadbb17b5db701b5515"
    n0pk = "032e8c9d8151f63083f2238cf48c842f8c67a4eb303ea4cea7d2020eea5af4ca12"
    n0sk = "6df42429d1647541c1fab62c59ea20c479adbca10ed05df21f2398bca18143b0"
    n1pk = "020bf11b1f5e437e1eee4ff765e452aad8e20958fcc6997ba483a3d84bb6a39c3d"
    n1sk = "3b2955461bf59b588e90dbbe2f42e1db7b8846ba1f1a1a7dee85cd1b07ac1bf9"
    n2pk = "0371baa10f6365cfc8b8206119c0ddadb602b0ca56630a44e72b3498cef006c1ff"
    n2sk = "1542364d0ceb643d2f992498155a9ad04ba03f050f5640183ee9b7c77052128e"
    n3pk = "02a903ea03a85f4b7db7f004c2ff49a53e08052aafd23cd4bf63290667c962d60d"
    n3sk = "fc3ec73ea445cfefa51390ba0c314ba1a4a482688ecd6efcd021b74eaba968b0"

    # read: https://en.bitcoin.it/wiki/Wallet_import_format
    testnet_prefix = "ef"
    #mainnet_prefix = "80"
    n0skb58c = b58encode_chk(bytes.fromhex(testnet_prefix + n0sk))
    print("n0sk b58c: ", n0skb58c)
    n1skb58c = b58encode_chk(bytes.fromhex(testnet_prefix +n1sk))
    print("n1sk b58c: ", n1skb58c)
    n2skb58c = b58encode_chk(bytes.fromhex(testnet_prefix +n2sk))
    print("n2sk b58c: ", n2skb58c)
    n3skb58c = b58encode_chk(bytes.fromhex(testnet_prefix + n3sk))
    print("n3sk b58c: ", n3skb58c)


# n0sk b58c:  92RLnKvbcNF6sR32PCsZx5fhzKrvk2FoKoBMpPjwzG8vwjAmtTN
# n1sk b58c:  922yN2AGxAnFFXDFsiigzhQce1KFSe51etkLt7hvcQYiGZM2SYH
# n2sk b58c:  91kHBnAnAXtoRUh45xyA4wu4Af3MuV7cuVHYoGie2te4iA4oggu
# n3sk b58c:  93W1RqQ96tFzX747vXEy89jixXJ5FEyiQWc6Z9kfZ5Pzzwbf9Cx

    print (">",     b58decode_chk("92RLnKvbcNF6sR32PCsZx5fhzKrvk2FoKoBMpPjwzG8vwjAmtTN").hex())