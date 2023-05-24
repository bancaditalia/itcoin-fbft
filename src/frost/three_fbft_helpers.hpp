#ifndef THREE_FBFT_HELPERS_HPP
#define THREE_FBFT_HELPERS_HPP

#include <hash.h>
#include <itcoin_secp256k1.h>
#include <algorithm>
#include "frost_helpers.hpp"
#include "secp256k1_extension.h"

struct PrivateNonceDerivation
{
    uint32_t index;
    ChainCode chaincode;
    itcoin_secp256k1_scalar master_nonce;
    itcoin_secp256k1_scalar child_nonce;
    bool valid;
};

struct PublicCommitmentDerivation
{
    uint32_t index;
    ChainCode chaincode;
    itcoin_secp256k1_gej child_commitment;
    itcoin_secp256k1_gej master_commitment;
    bool valid;
};

// The function CKDpriv((kpar, cpar), i) → (ki, ci) computes a child extended private key from the parent extended private key
bool DerivePrivateFromPrivate(PrivateNonceDerivation &nonceDerivation, const uint32_t index);
// The function CKDpub((Kpar, cpar), i) → (Ki, ci) computes a child extended public key from the parent extended public key. It is only defined for non-hardened child keys.
bool DerivePublicFromPublic(PublicCommitmentDerivation &commitDerivation, const uint32_t index);

void pretty_print(const std::vector<int>& v);
void ComputeCombinations(std::vector<uint32_t> &participants,
                         std::vector<std::vector<uint32_t>> &combinations,
                         int k);

#endif /* 3THREE_FBFT_HELPERS_HPP */
