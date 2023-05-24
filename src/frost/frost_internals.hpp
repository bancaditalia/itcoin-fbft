#ifndef FROST_INTERNALS_HPP
#define FROST_INTERNALS_HPP

#include <map>
#include <vector>

#include <itcoin_secp256k1.h>

void compute_point(itcoin_secp256k1_gej *point, const itcoin_secp256k1_scalar *scalar);

/**
 * Create secret shares for a given secret. This function accepts a secret to
 * generate shares from. While in FROST this secret should always be generated
 * randomly, we allow this secret to be specified for this internal function
 * for testability
 */
void generate_shares(
    const itcoin_secp256k1_scalar secret,
    uint32_t num_shares,
    uint32_t threshold,
    uint32_t generator_index,
    std::vector<share> &shares,
    shares_commitment &shares_commitment);

/**
 * Verify that a share is consistent with a commitment.
 */
bool verify_share(
    const share share,
    const shares_commitment com);

/**
 * keygen_with_dealer() generates shares and distributes them via a trusted
 * dealer. Note this approach is not the FROST specified key generation protocol
 * but we include this to demonstrate its compatibility with the FROST signing
 * protocol.
 */
void keygen_with_dealer(
    uint32_t num_shares,
    uint32_t threshold,
    shares_commitment &shares_com,
    std::vector<keypair> &key_pairs);

/**
 * generates the Lagrange coefficients for the ith participant. This allows for
 * performing Lagrange interpolation, which underpins threshold secret sharing
 * schemes based on Shamir secret sharing.
 */
itcoin_secp256k1_scalar derive_lagrange_coefficient(uint32_t x_coord, uint32_t signer_index,
  const std::vector<uint32_t> all_signer_indices);

/**
 * generates the challenge value H(m, R) used for both signing and verification.
 * ed25519_ph hashes the message first, and derives the challenge as H(H(m), R),
 * this would be a better optimization but incompatibility with other
 * implementations may be undesirable.
 */
void compute_challenge(const unsigned char *msg, size_t msg_len,
                       const itcoin_secp256k1_gej group_public_key,
                       const itcoin_secp256k1_gej group_commitment,
                       itcoin_secp256k1_scalar *challenge);

void compute_binding_factors(const std::vector<signing_commitment> signing_commitments,
                             const unsigned char *msg,
                             size_t msg_len,
                             std::map<uint32_t, itcoin_secp256k1_scalar> *binding_factors,
                             std::vector<unsigned char *> *binding_factor_inputs);

void participants_from_commitment_list(const std::vector<signing_commitment> signing_commitments,
                std::vector<uint32_t> *indices);

signing_response sign_internal(const keypair keypair,
                               const std::vector<signing_commitment> signing_commitments,
                               const nonce_pair signing_nonce,
                               const unsigned char *msg,
                               size_t msg_len,
                               std::map<uint32_t, itcoin_secp256k1_scalar> &bindings,
                               std::vector<uint32_t> &participant_list);

void serialize_point_xonly(const itcoin_secp256k1_gej *point, unsigned char *output);
void serialize_point(const itcoin_secp256k1_gej *point, unsigned char *output, size_t *size);
void deserialize_point(itcoin_secp256k1_gej *output, const unsigned char *point, size_t size);
void convert_b32_to_scalar(unsigned char *hash_value, itcoin_secp256k1_scalar *output);

#endif /* FROST_INTERNALS_HPP */
