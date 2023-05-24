INPUT_CHARSET = "0123456789()[],'/*abcdefgh@:$%{}IJKLMNOPQRSTUVWXYZ&+-.;<=>?!^_|~ijklmnopqrstuvwxyzABCDEFGH`#\"\\ "
CHECKSUM_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"
GENERATOR = [0xf5dee51989, 0xa9fdca3312, 0x1bab10e32d, 0x3706b1677a, 0x644d626ffd]

def descsum_polymod(symbols):
    """Internal function that computes the descriptor checksum."""
    chk = 1
    for value in symbols:
        top = chk >> 35
        chk = (chk & 0x7ffffffff) << 5 ^ value
        for i in range(5):
            chk ^= GENERATOR[i] if ((top >> i) & 1) else 0
    return chk

def descsum_expand(s):
    """Internal function that does the character to symbol expansion"""
    groups = []
    symbols = []
    for c in s:
        if not c in INPUT_CHARSET:
            return None
        v = INPUT_CHARSET.find(c)
        symbols.append(v & 31)
        groups.append(v >> 5)
        if len(groups) == 3:
            symbols.append(groups[0] * 9 + groups[1] * 3 + groups[2])
            groups = []
    if len(groups) == 1:
        symbols.append(groups[0])
    elif len(groups) == 2:
        symbols.append(groups[0] * 3 + groups[1])
    return symbols

def descsum_check(s):
    """Verify that the checksum is correct in a descriptor"""
    if s[-9] != '#':
        return False
    if not all(x in CHECKSUM_CHARSET for x in s[-8:]):
        return False
    symbols = descsum_expand(s[:-9]) + [CHECKSUM_CHARSET.find(x) for x in s[-8:]]
    return descsum_polymod(symbols) == 1

def descsum_create(s):
    """Add a checksum to a descriptor without"""
    symbols = descsum_expand(s) + [0, 0, 0, 0, 0, 0, 0, 0]
    checksum = descsum_polymod(symbols) ^ 1
    return s + '#' + ''.join(CHECKSUM_CHARSET[(checksum >> (5 * (7 - i))) & 31] for i in range(8))

# To obtain #f6y9r8s5", pass the whole definition:
#   tr(tprv8ZgxMBicQKsPeiU5xNau1YkrNQm7vFcKNscxwvVPsjY89yNPgeo4iebHEaghMS2oR1UcCVvntDWsLJDjyCnbYj55jtuVjHVRMfBWxqpSCqG/86'/1'/0'/0/*)
s = descsum_create("tr(tpubD6NzVbkrYhZ4XXWtfafgxpUwMALteLYArx9Wid4Tfu2kVUYHgurxTW3TR6fLcqAoUgFh3YQkWdnYqXcDgSPpk7Gz2CMruskrt7oVhh1bUeH/0/*)")
s = descsum_create("tr([96fc658e/0']tpubD9NVdb42N9YpA98XGEaqokRYqcVddtNqVZ2WAr24Vb8WbqWu3Fw2eFJcZWpvniocpgQ3L3Fmdqe7qpzrteAx5YCcY8YcFz3kVxfXQVzBkLB/0/0/0/10/1/0/*)")
print(s)

# print()

# import hashlib
# import hmac 
# import secrets

# bits = secrets.randbits(256)
# seed = hex(bits)[2:]
# # 66d891b5ed7f51e5044be6a7ebe4e2eae32b960f5aa0883f7cc0ce4fd6921e31
# print("Seed:", seed)

# key = "Bitcoin seed"
# msg = seed
# msg = "2ae27c8d6f58947d43ea45658eb996a36555b1e7858c3f76ca8fbf97b3c921b791cb5d834d5e087c4689bc8f4439e54cbebb3eb65266a9e2c9b42da19066e7ce"
# master_key = hmac.new(key.encode(), msg.encode(), hashlib.sha512).hexdigest()
# print("master key:", master_key)

# il = master_key[0:64]
# ir = master_key[64:]

# print("il:", il)
# print("ir:", ir)

# # chaincode: 774f0e70c73bcb266170aa720c1615a9f1839d1d9f7717a2f32a396efcc2c4be

# # c1af48ee51edb3963426a3495e8baa8ca6536357d9c19cf6547eebc7e207f20425dae45c7fa483538890cc8fb801147f6f29198c93757618051786ec732f67e0
# # 296967d8a1b602f840edd92e88a80ca482e7e7dfe0fc4d8ce48124db50ae2d3ab74f03e1a330dd8a2f8daa961bb7cc5fdf3cf9354d98712c10cede5a416a7208