#include <iostream>

#include "three_fbft_helpers.hpp"

// The function CKDpriv((kpar, cpar), i) → (ki, ci) computes a child extended private key from the parent extended private key
bool DerivePrivateFromPrivate(PrivateNonceDerivation &nonceDerivation, const uint32_t index)
{
  unsigned char output[64];
  bool hardened_child = (index >> 31 != 0);

  if (hardened_child) {
    unsigned char privkeydata[32];
    itcoin_secp256k1_scalar_get_b32(privkeydata, &nonceDerivation.master_nonce);
    //  If so (hardened child): let I = HMAC-SHA512(Key = cpar, Data = 0x00 || ser256(kpar) || ser32(i)).
    BIP32Hash(nonceDerivation.chaincode, index, 0, privkeydata, output);
  } else {
    itcoin_secp256k1_gej pubkey;
    compute_point(&pubkey, &nonceDerivation.master_nonce);
    unsigned char serialized_pubkey[33];
    size_t pubkey_size=33;
    serialize_point(&pubkey, serialized_pubkey, &pubkey_size);
    //  If not (normal child): let I = HMAC-SHA512(Key = cpar, Data = serP(point(kpar)) || ser32(i)).
    BIP32Hash(nonceDerivation.chaincode, index, *serialized_pubkey, serialized_pubkey + 1, output);
  }

  //  Split I into two 32-byte sequences, IL and IR:
  //  The returned child key ki is parse256(IL) + kpar (mod n).
  convert_b32_to_scalar(output, &nonceDerivation.child_nonce);
  itcoin_secp256k1_scalar_add(&nonceDerivation.child_nonce, &nonceDerivation.child_nonce, &nonceDerivation.master_nonce);

  //  The returned chain code ci is IR.
  memcpy(nonceDerivation.chaincode.begin(), output + 32, 32);

  // In case parse256(IL) ≥ n or ki = 0, the resulting key is invalid, and one should proceed with the next value for i. (Note: this has probability lower than 1 in 2127.)
  itcoin_secp256k1_context *ctx = itcoin_secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
  unsigned char childkeydata[32];
  itcoin_secp256k1_scalar_get_b32(childkeydata, &nonceDerivation.child_nonce);
  bool ret = itcoin_secp256k1_ec_seckey_tweak_add(ctx, childkeydata, output);

  return ret;
}

// The function CKDpub((Kpar, cpar), i) → (Ki, ci) computes a child extended public key from the parent extended public key. It is only defined for non-hardened child keys.
bool DerivePublicFromPublic(PublicCommitmentDerivation &commitDerivation, const uint32_t index)
{
  // Check whether i ≥ 2^31 (whether the child is a hardened key).
  bool hardened_child = (index >> 31 != 0);
  // If so (hardened child): return failure
  if (hardened_child){
    return false;
  }

  unsigned char output[64];
  unsigned char serialized_pubkey[33];
  size_t pubkey_size=33;
  serialize_point(&commitDerivation.master_commitment, serialized_pubkey, &pubkey_size);
  // If not (normal child): let I = HMAC-SHA512(Key = cpar, Data = serP(Kpar) || ser32(i)).
  BIP32Hash(commitDerivation.chaincode, index, *serialized_pubkey, serialized_pubkey + 1, output);

  //  The returned chain code ci is IR.
  memcpy(commitDerivation.chaincode.begin(), output + 32, 32);

  // In case parse256(IL) ≥ n or Ki is the point at infinity, the resulting key is invalid, and one should proceed with the next value for i.
  itcoin_secp256k1_context *ctx = itcoin_secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
  itcoin_secp256k1_pubkey pubkey1;
  if (!itcoin_secp256k1_ec_pubkey_parse(ctx, &pubkey1, serialized_pubkey, 33)) {
    return false;
  }

  // Split I into two 32-byte sequences, IL and IR.
  // The returned child key Ki is point(parse256(IL)) + Kpar.
  if (!itcoin_secp256k1_ec_pubkey_tweak_add(ctx, &pubkey1, output)) {
    return false;
  }
  unsigned char pub[33];
  size_t publen = 33;
  itcoin_secp256k1_ec_pubkey_serialize(ctx, pub, &publen, &pubkey1, SECP256K1_EC_COMPRESSED);
  memcpy(serialized_pubkey, pub, publen);
  deserialize_point(&commitDerivation.child_commitment, pub, publen);
  return true;
}

void ComputeCombinationsInternal(std::vector<uint32_t> &participants,
                                 std::vector<std::vector<uint32_t>> &combinations,
                                 int k,
                                 int offset,
                                 std::vector<uint32_t> combination) {
  if (k == 0) {
    combinations.push_back(combination);
    return;
  }
  for (int i = offset; i + k <= participants.size(); ++i) {
    combination.push_back(participants[i]);
    ComputeCombinationsInternal(participants, combinations, k -1, i+1, combination);
    combination.pop_back();
  }
}

void ComputeCombinations(std::vector<uint32_t> &participants,
                         std::vector<std::vector<uint32_t>> &combinations,
                         int k) {
  std::vector<uint32_t> combination;
  std::sort(participants.begin(), participants.end(),
            [](const int &lhs, const int &rhs)
            { return lhs < rhs; });
  return ComputeCombinationsInternal(participants, combinations, k, 0, combination);
}
