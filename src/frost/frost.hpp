/**
 * This code is based on the IETF Draft "Two-Round Threshold Schnorr Signatures
 * with FROST", revision 10.
 *
 * https://datatracker.ietf.org/doc/draft-irtf-cfrg-frost/10/
 */
#ifndef FROST_HPP
#define FROST_HPP

#include <map>
#include <vector>
#include <string>

#include <itcoin_secp256k1.h>
#include <itcoin_eckey.h>

#define SCALAR_SIZE 32
#define SHA256_SIZE 32
#define SERIALIZED_PUBKEY_SIZE 33
inline std::string context_string = "FROST-secp256k1-SHA256-v10";

struct nonce
{
  itcoin_secp256k1_scalar secret;
  itcoin_secp256k1_gej commitment;
};

struct nonce_pair
{
  /* The 'd' nonce is also known as 'hiding' nonce. */
  nonce hiding_nonce;
  /* The 'e' nonce is also known as 'binding' nonce. */
  nonce binding_nonce;
};

struct signature
{
  itcoin_secp256k1_gej r;
  itcoin_secp256k1_scalar z;
};

struct signing_commitment
{
  unsigned int index;
  /* The 'd' commitment is also known as 'hiding' commitment. */
  itcoin_secp256k1_gej hiding_commitment;
  /* The 'e' commitment is also known as 'binding' commitment. */
  itcoin_secp256k1_gej binding_commitment;
};

struct share
{
  uint32_t generator_index;
  uint32_t receiver_index;
  itcoin_secp256k1_scalar value;
};

struct shares_commitment
{
  std::vector<itcoin_secp256k1_gej> commitment;
};

struct keygen_dkg_proposed_commitment
{
  uint32_t index = 0;
  shares_commitment shares_commit;
  signature zkp = {};
};

struct keygen_dkg_commitment
{
  uint32_t index = 0;
  shares_commitment shares_commit;
};

struct keypair
{
  uint32_t index;
  itcoin_secp256k1_scalar secret;
  itcoin_secp256k1_gej public_key;
  itcoin_secp256k1_gej group_public_key;
};

struct signing_response
{
  itcoin_secp256k1_scalar response;
  uint32_t index;
};

/**
 * keygen_begin() is performed by each participant to initialize a Pedersen
 *
 * This function assumes there is an additional layer which performs the
 * distribution of shares to their intended participants.
 *
 * Note that while keygen_begin() returns Shares, these shares should be sent
 * *after* participants have exchanged commitments via
 * keygen_receive_commitments_and_validate_peers(). So, the caller of
 * keygen_begin() should store shares until after
 * keygen_receive_commitments_and_validate_peers() is complete, and then
 * exchange shares via keygen_finalize().
 */
void keygen_begin(
    uint32_t num_shares, uint32_t threshold, uint32_t generator_index,
    const unsigned char *context, uint32_t context_length,
    keygen_dkg_proposed_commitment *dkg_commitment,
    std::vector<share> *shares);

/**
 * keygen_receive_commitments_and_validate_peers() gathers commitments from
 * peers and validates the zero knowledge proof of knowledge for the peer's
 * secret term. It returns a list of all participants who failed the check, a
 * list of commitments for the peers that remain in a valid state, and an error
 * term.
 *
 * Here, we return a DKG commitment that is explicitly marked as valid, to
 * ensure that this step of the protocol is performed before going on to
 * keygen_finalize().
 */
void keygen_receive_commitments_and_validate_peers(
    std::vector<keygen_dkg_proposed_commitment> peer_commitments,
    const unsigned char *context,
    size_t context_length,
    std::vector<keygen_dkg_commitment> &valid_peer_commitments,
    std::vector<uint32_t> &invalid_peer_ids);

/**
 * keygen_finalize() finalizes the distributed key generation protocol.
 * It is performed once per participant.
 */
keypair keygen_finalize(
    uint32_t index,
    const std::vector<share> shares,
    const std::vector<keygen_dkg_commitment> commitments);

/**
 * @brief Create nonces and signature commitments for participant_index
 *
 * This function runs the pre-process step of FROST. It works as follows:
 *  1. Each Pi creates a list Li and adds \pi:
 *    - samples nonces (dij ; eij) from Zq \times Zq
 *    - derives (Dij ;Eij) = (g^dij ; g^eij)
 *    - stores ((dij ;Dij); (eij ;Eij))
 *  2. Publishes (i; Li)
 *
 * @param number_commitments Number of commitments to generate
 * @param participant_index Identifier of participant
 * @param nonces Returned vector of nonces
 * @param commitments Returned vector of signature commitments
 */
void preprocess(
    unsigned int number_commitments,
    unsigned int participant_index,
    std::vector<nonce_pair> *nonces,
    std::vector<signing_commitment> *commitments);

/**
 * @brief Create nonces and signature commitments.
 *
 * This function runs the pre-process step of FROST. 
 *
 * @param participant_index Identifier of participant
 * @param nonces Returned nonces
 * @param commitments Returned signature commitments
 */
nonce_pair create_nonce();

/**
 * sign() is performed by each participant selected for the signing operation;
 * these responses are then aggregated into the final FROST signature by the
 * signature aggregator performing the aggregate function with each response.
 *
 * A nonce is consumed from signing_nonces and is deleted from it.
 *
 * BEWARE: indices positions in signing_nonces which were computed before a call
 *         to sign() will no longer be valid and will need to be recomputed.
 */
signing_response sign(const keypair keypair,
                      const std::vector<signing_commitment> signing_commitments,
                      std::vector<nonce_pair> signing_nonces,
                      const unsigned char *msg,
                      size_t msg_len);

/**
 * aggregate() collects all responses from participants. It first performs a
 * validity check for each participant's response, and will return an error in
 * the case the response is invalid. If all responses are valid, it aggregates
 * these into a single signature that is published. This function is executed
 * by the entity performing the signature aggregator role.
 */
signature aggregate(
    const unsigned char *msg,
    size_t msg_len,
    const itcoin_secp256k1_gej group_public_key,
    const std::vector<signing_commitment> signing_commitments,
    const std::vector<signing_response> signing_responses,
    const std::map<uint32_t, itcoin_secp256k1_gej> signer_public_keys);

/**
 * validate() performs a plain Schnorr validation operation; this is identical
 * to performing validation of a Schnorr signature that has been signed by a
 * single party.
 */
void validate(const unsigned char *msg,
              size_t msg_len,
              const signature sig,
              const itcoin_secp256k1_gej pubkey);

#endif /* FROST_HPP */
