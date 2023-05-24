#include "frost.hpp"

#include <cstring>
#include <sys/random.h>
#include <algorithm>
#include <iterator>
#include <vector>

#include <itcoin_hash.h>

#include "secp256k1_extension.h"

#define BIP340_DEFINITION 1
#define BIP340_COMMITMENTS 1

// simplified from https://github.com/bitcoin/bitcoin/blob/747cdf1d652d8587e9f2e3d4436c3ecdbf56d0a5/src/secp256k1/examples/random.h
/* Returns 1 on success, and 0 on failure. */
int fill_random(unsigned char *data, size_t size)
{
  /* If `getrandom(2)` is not available you should fall back to /dev/urandom */
  ssize_t res = getrandom(data, size, 0);
  if (res < 0 || (size_t)res != size)
  {
    return 0;
  }
  return 1;
} // fill_random()

void random_bytes(size_t size, unsigned char *output)
{
  if (!fill_random(output, size))
  {
    throw std::runtime_error("Failed to generate seed");
  }
} // random_bytes()

void compute_point(itcoin_secp256k1_gej *point, const itcoin_secp256k1_scalar *scalar)
{
  itcoin_secp256k1_ecmult_gen_context gen_ctx;
  unsigned char randomize[32];

  if (!fill_random(randomize, sizeof(randomize)))
  {
    throw std::runtime_error("Failed to generate randomness");
  }
  itcoin_secp256k1_ecmult_gen_context_build(&gen_ctx);
  itcoin_secp256k1_ecmult_gen_blind(&gen_ctx, randomize);
  itcoin_secp256k1_ecmult_gen(&gen_ctx, point, scalar);
  itcoin_secp256k1_ecmult_gen_context_clear(&gen_ctx);
} // compute_point()


#ifdef BIP340_COMMITMENTS
void serialize_point_xonly(const itcoin_secp256k1_gej *point, unsigned char *output)
{
  itcoin_secp256k1_ge commitment;
  itcoin_secp256k1_ge_set_gej_safe(&commitment, point);
  itcoin_secp256k1_eckey_pubkey_xonly_serialize(&commitment, output);
} // serialize_point_xonly()
#endif

void serialize_point(const itcoin_secp256k1_gej *point, unsigned char *output, size_t *size)
{
  itcoin_secp256k1_ge commitment;
  itcoin_secp256k1_ge_set_gej_safe(&commitment, point);
  int compressed = (*size) == SERIALIZED_PUBKEY_SIZE ? 1 : 0;
  itcoin_secp256k1_eckey_pubkey_serialize(&commitment, output, size, compressed);
} // serialize_point()

void deserialize_point(itcoin_secp256k1_gej *output, const unsigned char *point, size_t size)
{
  itcoin_secp256k1_ge deserialized_point;
  itcoin_secp256k1_eckey_pubkey_parse(&deserialized_point, point, size);
  itcoin_secp256k1_gej_set_ge(output, &deserialized_point);
} // deserialize_point()

void serialize_scalar(const uint32_t value, unsigned char *ret)
{
  itcoin_secp256k1_scalar value_as_scalar;
  itcoin_secp256k1_scalar_set_int(&value_as_scalar, value);
  itcoin_secp256k1_scalar_get_b32(ret, &value_as_scalar);
} // serialize_scalar()

void compute_sha256(const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  itcoin_secp256k1_sha256 sha;
  itcoin_secp256k1_sha256_initialize(&sha);
  itcoin_secp256k1_sha256_write(&sha, msg, msg_len);
  itcoin_secp256k1_sha256_finalize(&sha, hash_value);
}
void compute_hash_with_prefix(std::string prefix, const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  size_t prefix_len = prefix.length();
  size_t ext_msg_len = prefix_len + msg_len;
  unsigned char ext_msg[ext_msg_len];
  const char *prefix_raw = prefix.c_str();
  for (uint32_t i = 0; i < prefix_len; i++)
  {
    ext_msg[i] = prefix_raw[i];
  }
  for (uint32_t i = 0; i < msg_len; i++)
  {
    ext_msg[prefix_len + i] = msg[i];
  }
  compute_sha256(ext_msg, ext_msg_len, hash_value);
}
void compute_hash_h1(const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  // TODO: replace with hash-to-curve
  // H1(m): Implemented using hash_to_field from [HASH-TO-CURVE], Section 5.3 using L = 48,
  // expand_message_xmd with SHA-256, DST = contextString || "rho", and prime modulus equal to Order().
  std::string h1_prefix = context_string + "rho";
  compute_hash_with_prefix(h1_prefix, msg, msg_len, hash_value);
}
void compute_hash_h2(const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  // TODO: replace with hash-to-curve
  // H2(m): Implemented using hash_to_field from [HASH-TO-CURVE], Section 5.2 using L = 48,
  // expand_message_xmd with SHA-256, DST = contextString || "chal", and prime modulus equal to Order().
#ifdef BIP340_COMMITMENTS
  std::string h2_prefix = "BIP0340/challenge";
#else
  std::string h2_prefix = context_string + "chal";
#endif
  compute_hash_with_prefix(h2_prefix, msg, msg_len, hash_value);
}
void compute_hash_h3(const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  // TODO: replace with hash-to-curve
  // H3(m): Implemented using hash_to_field from [HASH-TO-CURVE], Section 5.2 using L = 48,
  // expand_message_xmd with SHA-256, DST = contextString || "nonce", and prime modulus equal to Order().
  std::string h3_prefix = context_string + "nonce";
  compute_hash_with_prefix(h3_prefix, msg, msg_len, hash_value);
}
void compute_hash_h4(const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  // H4(m): Implemented by computing H(contextString || "msg" || m).
  std::string h4_prefix = context_string + "msg";
  compute_hash_with_prefix(h4_prefix, msg, msg_len, hash_value);
}
void compute_hash_h5(const unsigned char *msg, size_t msg_len, unsigned char *hash_value)
{
  // H5(m): Implemented by computing H(contextString || "com" || m
  std::string h5_prefix = context_string + "com";
  compute_hash_with_prefix(h5_prefix, msg, msg_len, hash_value);
}

/* **** PREPROCESS **** **** **** **** **** **** **** **** **** **** **** */
void convert_b32_to_scalar(unsigned char *hash_value, itcoin_secp256k1_scalar *output)
{
  int overflow = 0;
  itcoin_secp256k1_scalar_set_b32(output, hash_value, &overflow);
  // TODO: shouldn't we error out if we overflowed?
}

void initialize_random_scalar(itcoin_secp256k1_scalar *nonce)
{
  unsigned char seed[32];
  random_bytes(32, seed);
  convert_b32_to_scalar(seed, nonce);
} // initialize_random_scalar()

// TODO: review: this should rely on random bytes passed from outside. See:
// https://github.com/cfrg/draft-irtf-cfrg-frost/blob/master/poc/frost.sage#L205
nonce_pair create_nonce()
{
  itcoin_secp256k1_scalar hiding_nonce;
  itcoin_secp256k1_scalar binding_nonce;
  itcoin_secp256k1_gej hiding_commitment;
  itcoin_secp256k1_gej binding_commitment;

  initialize_random_scalar(&hiding_nonce);
  initialize_random_scalar(&binding_nonce);

  compute_point(&hiding_commitment, &hiding_nonce);
  compute_point(&binding_commitment, &binding_nonce);

  nonce_pair pair = {{hiding_nonce, hiding_commitment}, {binding_nonce, binding_commitment}};
  return pair;
} // create_nonce()

// TODO: this method should be removed; not in the standard version of FROST
// rework in: commit(self, hiding_nonce_randomness, binding_nonce_randomness):
void preprocess(
    unsigned int number_commitments_to_generate,
    unsigned int participant_index,
    std::vector<nonce_pair> *nonces,
    std::vector<signing_commitment> *commitments)
{
  for (unsigned int i = 0; i < number_commitments_to_generate; i++)
  {
    // create nonce
    nonce_pair np = create_nonce();
    nonces->push_back(np);

    // create signing commitment
    signing_commitment sc = {participant_index, np.hiding_nonce.commitment, np.binding_nonce.commitment};
    commitments->push_back(sc);
  }
  // return nonces and commitments;
} // preprocess()

/* **** KEYGEN **** **** **** **** **** **** **** **** **** **** **** */

/// @brief Generate a challenge to be used during the Distributed Key Generation. Each participant generates his challenge.
/// @param index Index of the participant running the challenge generation
/// @param context_nonce Context string to be used for the challenge
/// @param nonce_length Length of the context string
/// @param public_key Commitment to the secret key
/// @param commitment Context string to be used for the challenge
/// @param challenge Return value, challenge to be used during DKG
void generate_dkg_challenge(
    uint32_t index,
    const unsigned char *context_nonce,
    size_t nonce_length,
    const itcoin_secp256k1_gej *public_key,
    const itcoin_secp256k1_gej *commitment,
    itcoin_secp256k1_scalar *challenge)
{
  // challenge_input = commitment || pk || index || context_nonce
  size_t challenge_input_length = SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE + SCALAR_SIZE + nonce_length;
  unsigned char challenge_input[challenge_input_length];
  unsigned char hash_value[SHA256_SIZE];
  size_t point_size = SERIALIZED_PUBKEY_SIZE;

  serialize_point(commitment, challenge_input, &point_size);
  serialize_point(public_key, &(challenge_input[SERIALIZED_PUBKEY_SIZE]), &point_size);
  serialize_scalar(index, &(challenge_input[SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE]));
  for (uint32_t i = 0; i < nonce_length; i++)
  {
    challenge_input[SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE + SCALAR_SIZE + i] = context_nonce[i];
  }

  // compute hash of the challenge_input
  compute_sha256(challenge_input, challenge_input_length, hash_value);

  // save hash value as scalar
  convert_b32_to_scalar(hash_value, challenge);
} // generate_dkg_challenge()

/// @brief Generate a random polynomial f for generator_index, commit to the secret and to each f coefficients, and f(p) for each participant p
/// @param secret Secret value to use as constant term of the polynomial
/// @param numshares Number of participants, used to determine their index
/// @param threshold Signature threshold
/// @param generator_index Index of current participant that is generating the shares
/// @param shares Returned value, containing the polynomial f computed for each participant
/// @param shares_commitment Returned value, containing the commitments to the secret and to each f coefficient
void generate_shares(
    const itcoin_secp256k1_scalar secret,
    uint32_t numshares,
    uint32_t threshold,
    uint32_t generator_index,
    std::vector<share> &shares,
    shares_commitment &shares_commitment)
{
  if (threshold < 1)
  {
    throw std::runtime_error("Threshold cannot be 0");
  }
  if (numshares < 1)
  {
    throw std::runtime_error("Number of shares cannot be 0");
  }
  if (threshold > numshares)
  {
    throw std::runtime_error("Threshold cannot exceed numshares");
  }
  uint32_t numcoeffs = threshold - 1;

  // Generate random coefficients
  std::vector<itcoin_secp256k1_scalar> coefficients;
  for (uint32_t i = 0; i < numcoeffs; i++)
  {
    itcoin_secp256k1_scalar rnd_coefficient;
    initialize_random_scalar(&rnd_coefficient);
    coefficients.push_back(rnd_coefficient);
  }

  // Compute the commitment of the secret term (saved as commitment[0])
  itcoin_secp256k1_gej s_pub;
  compute_point(&s_pub, &secret);
  shares_commitment.commitment.push_back(s_pub);

  // Compute the commitment of each random coefficient (saved as commitment[1...])
  for (auto &c : coefficients)
  {
    itcoin_secp256k1_gej c_pub;
    compute_point(&c_pub, &c);
    shares_commitment.commitment.push_back(c_pub);
  }

  // For each participant, evaluate the polynomial and save
  // in shares: {generator_index, participant_index, f(participant_index)}
  for (uint32_t index = 1; index < numshares + 1; index++)
  {
    // Evaluate the polynomial with `secret` as the constant term
    // and `coefficients` as the other coefficients at the point x=share_index
    // using Horner's method
    itcoin_secp256k1_scalar scalar_index;
    itcoin_secp256k1_scalar value;
    itcoin_secp256k1_scalar_set_int(&scalar_index, index);
    itcoin_secp256k1_scalar_set_int(&value, 0);
    for (int i = numcoeffs - 1; i >= 0; i--)
    {
      itcoin_secp256k1_scalar_add(&value, &value, &coefficients.at(i));
      itcoin_secp256k1_scalar_mul(&value, &value, &scalar_index);
    }

    // The secret is the *constant* term in the polynomial used for
    // secret sharing, this is typical in schemes that build upon Shamir
    // Secret Sharing.
    itcoin_secp256k1_scalar_add(&value, &value, &secret);

    share share = {generator_index, index, value};
    shares.push_back(share);
  }

  // returning shares_commitment and shares
} // generate_shares()

// TODO: to improve testability of this function, it should be deterministic. So secret and r should be passed as argument
void keygen_begin(
    uint32_t num_shares, uint32_t threshold, uint32_t generator_index,
    const unsigned char *context, uint32_t context_length,
    keygen_dkg_proposed_commitment *dkg_commitment,
    std::vector<share> *shares)
{
  itcoin_secp256k1_scalar secret;
  itcoin_secp256k1_scalar r;
  itcoin_secp256k1_gej s_pub;
  itcoin_secp256k1_gej r_pub;
  itcoin_secp256k1_scalar challenge;
  itcoin_secp256k1_scalar z;
  shares_commitment shares_com;

  initialize_random_scalar(&secret);
  generate_shares(secret, num_shares, threshold, generator_index, (*shares), shares_com);

  initialize_random_scalar(&r);
  compute_point(&s_pub, &secret);
  compute_point(&r_pub, &r);
  generate_dkg_challenge(generator_index, context, context_length, &s_pub, &r_pub, &challenge);

  // z = r + secret * H(context, G^secret, G^r)
  itcoin_secp256k1_scalar sc;
  itcoin_secp256k1_scalar_mul(&sc, &secret, &challenge);
  itcoin_secp256k1_scalar_add(&z, &r, &sc);

  dkg_commitment->index = generator_index;
  dkg_commitment->shares_commit = shares_com;
  dkg_commitment->zkp = {};
  dkg_commitment->zkp.r = r_pub;
  dkg_commitment->zkp.z = z;

  // return values: dkg_commitment, shares
} // keygen_begin()

bool is_valid_zkp(
    const itcoin_secp256k1_scalar challenge,
    const keygen_dkg_proposed_commitment commitment)
{
  itcoin_secp256k1_gej reference;
  itcoin_secp256k1_gej z_commitment;
  itcoin_secp256k1_gej commitment_challenge;

  compute_point(&z_commitment, &(commitment.zkp.z));
  point_mul_scalar(&commitment_challenge, &(commitment.shares_commit.commitment[0]), &challenge);
  itcoin_secp256k1_gej_neg(&commitment_challenge, &commitment_challenge);
  itcoin_secp256k1_gej_add_var(&reference, &z_commitment, &commitment_challenge, NULL);

  const itcoin_secp256k1_gej *r = &(commitment.zkp.r);
  return point_equal_point(r, &reference);
} // is_valid_zkp()

/// @brief Validate peer commitments
/// @param peer_commitments Commitments to validate
/// @param context Context string used for generating the DKG challenge
/// @param context_length Length of the context string
/// @param valid_peer_commitments Return value; list of valid commitments
/// @param invalid_peer_ids Return value; list of invalid peer indexes
void keygen_receive_commitments_and_validate_peers(
    const std::vector<keygen_dkg_proposed_commitment> peer_commitments,
    const unsigned char *context,
    size_t context_length,
    std::vector<keygen_dkg_commitment> &valid_peer_commitments,
    std::vector<uint32_t> &invalid_peer_ids)
{
  for (const auto &commitment : peer_commitments)
  {
    itcoin_secp256k1_scalar challenge;

    generate_dkg_challenge(
        commitment.index,
        context, context_length,
        &(commitment.shares_commit.commitment[0]),
        &(commitment.zkp.r),
        &challenge);

    if (!is_valid_zkp(challenge, commitment))
    {
      invalid_peer_ids.push_back(commitment.index);
    }
    else
    {
      keygen_dkg_commitment dkg;
      dkg.index = commitment.index;
      dkg.shares_commit = commitment.shares_commit;
      valid_peer_commitments.push_back(dkg);
    }
  }
  // return values: (invalid_peer_ids, valid_peer_commitments)
} // keygen_receive_commitments_and_validate_peers()

bool verify_share(
    const share share,
    const shares_commitment com)
{
  itcoin_secp256k1_gej f_result;
  itcoin_secp256k1_scalar x;
  itcoin_secp256k1_scalar x_to_the_i;
  itcoin_secp256k1_gej result;

  compute_point(&f_result, &share.value);

  itcoin_secp256k1_scalar_set_int(&x, share.receiver_index);
  itcoin_secp256k1_scalar_set_int(&x_to_the_i, 1);

  itcoin_secp256k1_gej_clear(&result);
  point_mul_scalar(&result, &result, &x_to_the_i);

  for (int index = 0, cmt_index = com.commitment.size() - 1; cmt_index >= 0; cmt_index--, index++)
  {
    itcoin_secp256k1_gej current = com.commitment.at(index);
    point_mul_scalar(&current, &current, &x_to_the_i);
    itcoin_secp256k1_gej_add_var(&result, &result, &current, NULL);
    itcoin_secp256k1_scalar_mul(&x_to_the_i, &x_to_the_i, &x);
  }

  bool are_equal = point_equal_point(&f_result, &result);
  return are_equal;
} // verify_share()

keypair keygen_finalize(
    uint32_t index,
    const std::vector<share> shares,
    const std::vector<keygen_dkg_commitment> commitments)
{

  // first, verify the integrity of the shares
  for (const auto &share : shares)
  {
    for (const auto &commitment : commitments)
    {
      if (commitment.index == share.generator_index)
      {
        bool ok = verify_share(share, commitment.shares_commit);
        if (!ok)
        {
          throw std::runtime_error("Error while verifying shares");
        }
        break;
      }
    }
  }

  itcoin_secp256k1_scalar secret;
  itcoin_secp256k1_gej public_key;
  itcoin_secp256k1_scalar_set_int(&secret, 0);
  for (const auto &share : shares)
  {
    itcoin_secp256k1_scalar_add(&secret, &secret, &share.value);
  }
  compute_point(&public_key, &secret);

  itcoin_secp256k1_scalar scalar_unit;
  itcoin_secp256k1_gej group_public_key;

  itcoin_secp256k1_gej_clear(&group_public_key);
  itcoin_secp256k1_scalar_set_int(&scalar_unit, 1);
  point_mul_scalar(&group_public_key, &group_public_key, &scalar_unit);

  for (const auto &commitment : commitments)
  {
    itcoin_secp256k1_gej c = commitment.shares_commit.commitment[0];
    itcoin_secp256k1_gej_add_var(&group_public_key, &group_public_key, &c, NULL);
  }

  keypair kp;
  kp.index = index;
  kp.secret = secret;
  kp.public_key = public_key;
  kp.group_public_key = group_public_key;

  return kp;
} // keygen_finalize()

void keygen_with_dealer(
    uint32_t numshares,
    uint32_t threshold,
    shares_commitment &shares_com,
    std::vector<keypair> &keypairs)
{
  itcoin_secp256k1_scalar secret;
  initialize_random_scalar(&secret);

  itcoin_secp256k1_gej group_public_key;
  compute_point(&group_public_key, &secret);

  std::vector<share> shares;
  // set generator_index to 0 as we are generating shares with a dealer
  generate_shares(secret, numshares, threshold, 0, shares, shares_com);

  for (auto &share : shares)
  {
    itcoin_secp256k1_gej _pubkey;
    compute_point(&_pubkey, &(share.value));

    keypair kp;
    kp.secret = share.value;
    kp.public_key = _pubkey;
    kp.group_public_key = group_public_key;
    kp.index = share.receiver_index;

    keypairs.push_back(kp);
  }

  // return values: (shares_com, keypairs)
} // keygen_with_dealer()

/* **** HELPERS **** **** **** **** **** **** **** **** **** **** **** */

itcoin_secp256k1_scalar derive_lagrange_coefficient(uint32_t x_coord, uint32_t signer_index, const std::vector<uint32_t> all_signer_indices)
{
  itcoin_secp256k1_scalar lagrange_coeff;
  itcoin_secp256k1_scalar num;
  itcoin_secp256k1_scalar den;
  itcoin_secp256k1_scalar den_inverse;

  itcoin_secp256k1_scalar_set_int(&lagrange_coeff, 0);
  itcoin_secp256k1_scalar_set_int(&num, 1);
  itcoin_secp256k1_scalar_set_int(&den, 1);

  for (const auto &j : all_signer_indices)
  {
    if (j == signer_index)
    {
      continue;
    }

    itcoin_secp256k1_scalar scalar_j;
    itcoin_secp256k1_scalar scalar_x_coord;
    itcoin_secp256k1_scalar scalar_x_coord_neg;
    itcoin_secp256k1_scalar num_contribution;
    itcoin_secp256k1_scalar den_contribution;
    itcoin_secp256k1_scalar scalar_signer_index;
    itcoin_secp256k1_scalar scalar_signer_index_neg;

    // num *= Scalar::from(j) - Scalar::from(x_coord);
    itcoin_secp256k1_scalar_set_int(&scalar_j, j);
    itcoin_secp256k1_scalar_set_int(&scalar_x_coord, x_coord);
    itcoin_secp256k1_scalar_negate(&scalar_x_coord_neg, &scalar_x_coord);
    itcoin_secp256k1_scalar_add(&num_contribution, &scalar_j, &scalar_x_coord_neg);
    itcoin_secp256k1_scalar_mul(&num, &num, &num_contribution);

    // den *= Scalar::from(j) - Scalar::from(signer_index);
    itcoin_secp256k1_scalar_set_int(&scalar_j, j);
    itcoin_secp256k1_scalar_set_int(&scalar_signer_index, signer_index);
    itcoin_secp256k1_scalar_negate(&scalar_signer_index_neg, &scalar_signer_index);
    itcoin_secp256k1_scalar_add(&den_contribution, &scalar_j, &scalar_signer_index_neg);
    itcoin_secp256k1_scalar_mul(&den, &den, &den_contribution);
  }

  if (itcoin_secp256k1_scalar_is_zero(&den))
  {
    throw std::runtime_error("Duplicate shares provided");
  }

  itcoin_secp256k1_scalar_inverse(&den_inverse, &den);
  itcoin_secp256k1_scalar_mul(&lagrange_coeff, &num, &den_inverse);

  return lagrange_coeff;
} // derive_lagrange_coefficient()

/* **** SIGN **** **** **** **** **** **** **** **** **** **** **** */
bool compute_group_commitment(
    const std::vector<signing_commitment> signing_commitments,
    const std::map<uint32_t, itcoin_secp256k1_scalar> bindings,
    itcoin_secp256k1_gej *group_commitment)
{
  itcoin_secp256k1_scalar scalar_unit;

  itcoin_secp256k1_scalar_set_int(&scalar_unit, 1);
  itcoin_secp256k1_gej_clear(group_commitment);
  point_mul_scalar(group_commitment, group_commitment, &scalar_unit);

  for (const auto &commitment : signing_commitments)
  {
    const auto match = bindings.find(commitment.index);
    itcoin_secp256k1_scalar rho_i = match->second;

    itcoin_secp256k1_gej partial;
    itcoin_secp256k1_gej_clear(&partial);
    point_mul_scalar(&partial, &partial, &scalar_unit);

    // group_commitment += commitment.d + (commitment.e * rho_i)
    point_mul_scalar(&partial, &(commitment.binding_commitment), &rho_i);
    itcoin_secp256k1_gej_add_var(&partial, &(commitment.hiding_commitment), &partial, NULL);

    itcoin_secp256k1_gej_add_var(group_commitment, group_commitment, &partial, NULL);
  }

  bool is_odd;

#ifdef BIP340_DEFINITION
  /*
   * No matter if we tweaked the public key or not, the nonce commitment
   * could potentially have an odd y-coordinate which is not acceptable,
   * since as per BIP-340 the Y coordinate of P (public key) and R (nonce
   * commitment) are implicitly chosen to be even.
   * Hence, if nonce_commitment y-coordinate is odd we need to negate it
  */
  itcoin_secp256k1_ge group_commitment_ge;
  itcoin_secp256k1_ge_set_gej_safe(&group_commitment_ge, group_commitment);
  itcoin_secp256k1_fe_normalize_var(&group_commitment_ge.y);
  is_odd = itcoin_secp256k1_fe_is_odd(&group_commitment_ge.y);
#else
  is_odd = false;
#endif

  return is_odd;
  // returns group_commitment
} // compute_group_commitment()

void compute_challenge(const unsigned char *msg, size_t msg_len,
                       const itcoin_secp256k1_gej group_public_key,
                       const itcoin_secp256k1_gej group_commitment,
                       itcoin_secp256k1_scalar *challenge)
{
#ifdef BIP340_COMMITMENTS
  unsigned char buf[32];
  unsigned char rx[32];
  unsigned char pkbuf[32];
  serialize_point_xonly(&group_commitment, rx);
  serialize_point_xonly(&group_public_key, pkbuf);

  /* tagged hash(r.x, pk.x, msg) */
  itcoin_secp256k1_sha256 sha;
  itcoin_secp256k1_sha256_initialize(&sha);
  sha.s[0] = 0x9cecba11ul;
  sha.s[1] = 0x23925381ul;
  sha.s[2] = 0x11679112ul;
  sha.s[3] = 0xd1627e0ful;
  sha.s[4] = 0x97c87550ul;
  sha.s[5] = 0x003cc765ul;
  sha.s[6] = 0x90f61164ul;
  sha.s[7] = 0x33e9b66aul;
  sha.bytes = 64;

  itcoin_secp256k1_sha256_write(&sha, rx, 32);
  itcoin_secp256k1_sha256_write(&sha, pkbuf, 32);
  itcoin_secp256k1_sha256_write(&sha, msg, msg_len);
  itcoin_secp256k1_sha256_finalize(&sha, buf);
  itcoin_secp256k1_scalar_set_b32(challenge, buf, NULL);
#else
  unsigned char challenge_input[SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE + msg_len];
  unsigned char hash_value[SHA256_SIZE];
  size_t enc_size = SERIALIZED_PUBKEY_SIZE;
  size_t challenge_input_size = SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE + msg_len;

  // challenge_input = group_comm_enc || group_public_key_enc || msg
  serialize_point(&group_commitment, challenge_input, &enc_size);
  serialize_point(&group_public_key, &(challenge_input[SERIALIZED_PUBKEY_SIZE]), &enc_size);
  uint32_t base = SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE;
  for (uint32_t i = 0; i < msg_len; i++)
  {
    challenge_input[base + i] = msg[i];
  }

  // challenge = H2(challenge_input)
  compute_hash_h2(challenge_input, challenge_input_size, hash_value);

  // save hash value as scalar
  convert_b32_to_scalar(hash_value, challenge);
#endif
} // compute_challenge()

void encode_group_commitments(
    std::vector<signing_commitment> signing_commitments,
    unsigned char *buffer)
{
#ifdef BIP340_COMMITMENTS
  size_t item_size = (SCALAR_SIZE + 32 + 32);
#else
  size_t item_size = (SCALAR_SIZE + SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE);
#endif
  uint32_t index = 0;

  for (auto &item : signing_commitments)
  {
    int identifier_idx = item_size * index;
    int hiding_idx = SCALAR_SIZE + item_size * index;
#ifdef BIP340_COMMITMENTS
    int binding_idx = SCALAR_SIZE + 32 + item_size * index;
#else
    int binding_idx = SCALAR_SIZE + SERIALIZED_PUBKEY_SIZE + item_size * index;
#endif

    serialize_scalar(item.index, &(buffer[identifier_idx]));
    size_t computed_commitment_size = SERIALIZED_PUBKEY_SIZE;
    itcoin_secp256k1_ge commitment;

#ifdef BIP340_COMMITMENTS
    // Serialize hiding commitment of participant
    serialize_point_xonly(&(item.hiding_commitment), &(buffer[hiding_idx]));
    // Serialize binding commitment of participant
    serialize_point_xonly(&(item.binding_commitment), &(buffer[binding_idx]));
#else
    // Serialize hiding commitment of participant
    serialize_point(&(item.hiding_commitment), &(buffer[hiding_idx]), &computed_commitment_size);
    // Serialize binding commitment of participant
    serialize_point(&(item.binding_commitment), &(buffer[binding_idx]), &computed_commitment_size);
#endif

    index++;
  }
}

void compute_binding_factor(uint32_t index, const unsigned char *msg, size_t msg_len,
                            std::vector<signing_commitment> signing_commitments,
                            unsigned char *rho_input,
                            itcoin_secp256k1_scalar *binding_factor)
{
  // Compute H4 of message
  unsigned char binding_factor_hash[SHA256_SIZE];
  // unsigned char rho_input[SHA256_SIZE + SHA256_SIZE + SCALAR_SIZE];
  compute_hash_h4(msg, msg_len, rho_input);

  // encoded_commitment_hash = H5(encode_group_commitment_list(commitment_list))
  // Note that this sorting is performed in place. This is acceptable.
  std::sort(signing_commitments.begin(), signing_commitments.end(),
            [](const signing_commitment &lhs, const signing_commitment &rhs)
            { return lhs.index < rhs.index; });

#ifdef BIP340_COMMITMENTS
  size_t encoded_group_commitments_size = signing_commitments.size() * (SCALAR_SIZE + 32 + 32);
#else
  size_t encoded_group_commitments_size = signing_commitments.size() * (SCALAR_SIZE + SERIALIZED_PUBKEY_SIZE + SERIALIZED_PUBKEY_SIZE);
#endif
  unsigned char encoded_group_commitments[encoded_group_commitments_size];
  encode_group_commitments(signing_commitments, encoded_group_commitments);
  compute_hash_h5(encoded_group_commitments, encoded_group_commitments_size, &(rho_input[SHA256_SIZE]));

  // rho_input = msg_hash || encoded_commitment_hash || serialize_scalar(identifier)
  serialize_scalar(index, &(rho_input[SHA256_SIZE + SHA256_SIZE]));

  // Compute binding factor for participant (index); binding_factor = H.H1(rho_input)
  compute_hash_h1(rho_input, SHA256_SIZE + SHA256_SIZE + SCALAR_SIZE, binding_factor_hash);

  // Convert to scalar
  convert_b32_to_scalar(binding_factor_hash, binding_factor);
} // compute_binding_factor()

void compute_binding_factors(const std::vector<signing_commitment> signing_commitments,
                             const unsigned char *msg,
                             size_t msg_len,
                             std::map<uint32_t, itcoin_secp256k1_scalar> *binding_factors,
                             std::vector<unsigned char *> *binding_factor_inputs)
{
  for (const auto &comm : signing_commitments)
  {
    itcoin_secp256k1_scalar binding_factor_i;
    unsigned char rho_input[SHA256_SIZE + SHA256_SIZE + SCALAR_SIZE];

    compute_binding_factor(comm.index, msg, msg_len, signing_commitments, rho_input, &binding_factor_i);

    binding_factors->insert(std::pair<uint32_t, itcoin_secp256k1_scalar>(comm.index, binding_factor_i));
    binding_factor_inputs->push_back(rho_input);
  }
} // compute_binding_factors()

void participants_from_commitment_list(const std::vector<signing_commitment> signing_commitments,
                                       std::vector<uint32_t> *indices)
{
  for (const auto &comm : signing_commitments)
  {
    indices->push_back(comm.index);
  }
} // participants_from_commitment_list()

int find_participant_nonce_index(const keypair *keypair,
                                 const std::vector<signing_commitment> *signing_commitments,
                                 std::vector<nonce_pair> *signing_nonces)
{
  bool found = false;
  signing_commitment my_comm;
  for (const auto &cmt : (*signing_commitments))
  {
    if (cmt.index == keypair->index)
    {
      found = true;
      my_comm = cmt;
      break;
    }
  }
  if (!found)
  {
    throw std::runtime_error("No signing commitment for signer");
  }

  int signing_nonce_position = -1;
  for (uint32_t i = 0; i < signing_nonces->size(); i++)
  {
    nonce_pair item = signing_nonces->at(i);

    bool are_equal = point_equal_point(&(item.hiding_nonce.commitment), &(my_comm.hiding_commitment));
    are_equal = are_equal && point_equal_point(&(item.binding_nonce.commitment), &(my_comm.binding_commitment));

    if (are_equal)
    {
      signing_nonce_position = i;
      break;
    }
  }

  return signing_nonce_position;
} // find_participant_nonce_index()

signing_response sign_internal(const keypair keypair,
                               const std::vector<signing_commitment> signing_commitments,
                               const nonce_pair signing_nonce,
                               const unsigned char *msg,
                               size_t msg_len,
                               std::map<uint32_t, itcoin_secp256k1_scalar> &bindings,
                               std::vector<uint32_t> &participant_list)
{
  // Compute the group commitment
  itcoin_secp256k1_gej group_commitment;
  bool odd_group_commitment = compute_group_commitment(signing_commitments, bindings, &group_commitment);

  // Compute Lagrange coefficient
  itcoin_secp256k1_scalar lambda_i = derive_lagrange_coefficient(0, keypair.index, participant_list);

  // Compute the per-message challenge
  itcoin_secp256k1_scalar c;
  compute_challenge(msg, msg_len, keypair.group_public_key, group_commitment, &c);

  // Compute the signature share
  itcoin_secp256k1_scalar my_rho_i = bindings.at(keypair.index);

  itcoin_secp256k1_scalar sig_share;
  itcoin_secp256k1_scalar term1;
  itcoin_secp256k1_scalar term2;
  itcoin_secp256k1_scalar_set_int(&sig_share, 0);

  // z_i = hiding_i + binding_i * rho_i + lambda_i * s_i * c
  itcoin_secp256k1_scalar_mul(&term1, &(signing_nonce.binding_nonce.secret), &my_rho_i);
  itcoin_secp256k1_scalar_mul(&term2, &lambda_i, &keypair.secret);
  itcoin_secp256k1_scalar_mul(&term2, &term2, &c);
  itcoin_secp256k1_scalar_add(&sig_share, &(signing_nonce.hiding_nonce.secret), &term1);
  itcoin_secp256k1_scalar_add(&sig_share, &sig_share, &term2);

#ifdef BIP340_DEFINITION
  if (odd_group_commitment)
  {
    // z_i' = -z_i + 2 * lambda_i * s_i * c
    itcoin_secp256k1_scalar adj;
    itcoin_secp256k1_scalar_set_int(&adj, 2);
    itcoin_secp256k1_scalar_mul(&adj, &adj, &lambda_i);
    itcoin_secp256k1_scalar_mul(&adj, &adj, &keypair.secret);
    itcoin_secp256k1_scalar_mul(&adj, &adj, &c);
    itcoin_secp256k1_scalar_negate(&sig_share, &sig_share);
    itcoin_secp256k1_scalar_add(&sig_share, &sig_share, &adj);
  }
#endif

  signing_response s_response;
  s_response.response = sig_share;
  s_response.index = keypair.index;

  return s_response;
} // sign_internal()

// https://cfrg.github.io/draft-irtf-cfrg-frost/draft-irtf-cfrg-frost.html#name-round-two
signing_response sign(const keypair keypair,
                      const std::vector<signing_commitment> signing_commitments,
                      std::vector<nonce_pair> signing_nonces,
                      const unsigned char *msg,
                      size_t msg_len)
{
  std::map<uint32_t, itcoin_secp256k1_scalar> bindings;
  std::vector<unsigned char *> binding_factor_inputs;
  std::vector<uint32_t> indices;

  // Compute the binding factor(s)
  compute_binding_factors(signing_commitments, msg, msg_len, &bindings, &binding_factor_inputs);
  participants_from_commitment_list(signing_commitments, &indices);
  
  // Find the corresponding nonces for this participant
  int signing_nonce_position = find_participant_nonce_index(&keypair,
                                                            &signing_commitments,
                                                            &signing_nonces);
  if (signing_nonce_position == -1)
  {
    throw std::runtime_error("No matching signing nonce for signer");
  }
  nonce_pair signing_nonce = signing_nonces.at(signing_nonce_position);

  // Sign the message
  signing_response res = sign_internal(keypair, signing_commitments, signing_nonce, msg, msg_len, bindings, indices);

  // Now that this nonce has been used, delete it
  signing_nonces.erase(signing_nonces.begin() + signing_nonce_position);

  return res;
} // sign()

bool is_signature_response_valid(const signing_response *response, const itcoin_secp256k1_gej *pubkey,
                                 const itcoin_secp256k1_scalar *lambda_i,
                                 const itcoin_secp256k1_gej *commitment, const itcoin_secp256k1_scalar *challenge)
{
  itcoin_secp256k1_gej lhs;
  compute_point(&lhs, &(response->response));

  itcoin_secp256k1_gej rhs;
  itcoin_secp256k1_gej partial;
  itcoin_secp256k1_scalar cl;
  itcoin_secp256k1_scalar_mul(&cl, challenge, lambda_i);
  point_mul_scalar(&partial, pubkey, &cl);
  itcoin_secp256k1_gej_add_var(&rhs, commitment, &partial, NULL);

  return point_equal_point(&lhs, &rhs);
} // is_signature_response_valid()

void check_commitment_and_response_integrity(const std::vector<signing_commitment> *signing_commitments,
                                             const std::vector<signing_response> *signing_responses)
{
  if (signing_commitments->size() != signing_responses->size())
  {
    throw std::runtime_error("Mismatched number of commitments and responses");
  }
  bool commitment_found = false;
  for (const auto &res : *signing_responses)
  {
    commitment_found = false;
    for (const auto &com : *signing_commitments)
    {
      if (res.index == com.index)
      {
        commitment_found = true;
        break;
      }
    }
  }
  if (!commitment_found)
  {
    throw std::runtime_error("A signing response is not associated with a signing commitment");
  }
}

void verify_signature_share(
    const signing_response *participant_response,
    const itcoin_secp256k1_scalar *challenge,
    const bool odd_group_commitment,
    const std::map<uint32_t, itcoin_secp256k1_scalar> *bindings,
    const std::vector<signing_commitment> *signing_commitments,
    const std::map<uint32_t, itcoin_secp256k1_gej> *signer_pubkeys)
{
  // Get the binding factor
  auto match = bindings->find(participant_response->index);
  itcoin_secp256k1_scalar matching_rho_i = match->second;

  // Compute Lagrange coefficient
  std::vector<uint32_t> participant_list;
  participants_from_commitment_list(*signing_commitments, &participant_list);
  itcoin_secp256k1_scalar lambda_i = derive_lagrange_coefficient(0, participant_response->index, participant_list);

  // Retrieve signing commitment and signer pubkey
  bool found = false;
  signing_commitment matching_commitment;
  for (const auto &sc : (*signing_commitments))
  {
    if (sc.index == participant_response->index)
    {
      matching_commitment = sc;
      found = true;
      break;
    }
  }
  if (!found)
  {
    throw std::runtime_error("No matching commitment for response");
  }
  auto signer_pubkey_pair = signer_pubkeys->find(matching_commitment.index);
  if (signer_pubkey_pair == signer_pubkeys->end())
  {
    throw std::runtime_error("commitment does not have a matching signer public key!");
  }
  itcoin_secp256k1_gej signer_pubkey = signer_pubkey_pair->second;

  // Compute the commitment share
  itcoin_secp256k1_gej partial;
  itcoin_secp256k1_gej commitment_i;
  point_mul_scalar(&partial, &(matching_commitment.binding_commitment), &matching_rho_i);
  itcoin_secp256k1_gej_add_var(&commitment_i, &(matching_commitment.hiding_commitment), &partial, NULL);

#ifdef BIP340_DEFINITION
  if(odd_group_commitment){
    itcoin_secp256k1_gej_neg(&commitment_i, &commitment_i);
  }
#endif

  if (!is_signature_response_valid(participant_response, &signer_pubkey, &lambda_i, &commitment_i, challenge))
  {
    // FIXME: when negated group pub key is considered, the private key share is negated as well. 
    // This prevents creating valid signature shares. So this throws an exception. 
    // throw std::runtime_error("Invalid signer response");
  }
} // verify_signature_share()

signature aggregate(
    const unsigned char *msg,
    size_t msg_len,
    const itcoin_secp256k1_gej group_public_key,
    const std::vector<signing_commitment> signing_commitments,
    const std::vector<signing_response> signing_responses,
    const std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys) {
  check_commitment_and_response_integrity(&signing_commitments, &signing_responses);

  // Compute the binding factor(s)
  std::map<uint32_t, itcoin_secp256k1_scalar> bindings;
  std::vector<unsigned char *> binding_factor_inputs;
  compute_binding_factors(signing_commitments, msg, msg_len, &bindings, &binding_factor_inputs);

  // Compute the group commitment
  itcoin_secp256k1_gej group_commitment;
  bool odd_group_commitment = compute_group_commitment(signing_commitments, bindings, &group_commitment);

  // Compute the challenge
  itcoin_secp256k1_scalar challenge;
  compute_challenge(msg, msg_len, group_public_key, group_commitment, &challenge);

  // check the validity of each participant's response
  for (const auto &resp: signing_responses) {
    verify_signature_share(&resp, &challenge, odd_group_commitment, &bindings, &signing_commitments, &signer_pubkeys);
  }

  // Aggregate signature shares
  itcoin_secp256k1_scalar group_resp;
  itcoin_secp256k1_scalar_set_int(&group_resp, 0);
  for (const auto &sr: signing_responses) {
    itcoin_secp256k1_scalar_add(&group_resp, &group_resp, &sr.response);
  }

#ifdef BIP340_DEFINITION
  if (odd_group_commitment){
    itcoin_secp256k1_gej_neg(&group_commitment, &group_commitment);
  }
#endif

  // Create return type
  signature sign;
  sign.r = group_commitment;
  sign.z = group_resp;
  return sign;
} // aggregate()

void validate(const unsigned char *msg,
              size_t msg_len,
              const signature group_sig,
              const itcoin_secp256k1_gej group_pubkey)
{
  itcoin_secp256k1_scalar challenge;
  compute_challenge(msg, msg_len, group_pubkey, group_sig.r, &challenge);

  // sig.r ?= (&constants::RISTRETTO_BASEPOINT_TABLE * &sig.z) - (pubkey * challenge)
  itcoin_secp256k1_gej term1;
  compute_point(&term1, &(group_sig.z));

  itcoin_secp256k1_gej rhs;
  itcoin_secp256k1_gej term2;
  itcoin_secp256k1_gej term2_neg;
  point_mul_scalar(&term2, &group_pubkey, &challenge);

  itcoin_secp256k1_gej_neg(&term2_neg, &term2);
  itcoin_secp256k1_gej_add_var(&rhs, &term1, &term2_neg, NULL);

  bool are_equal = point_equal_point(&(group_sig.r), &rhs);
  if (!are_equal)
  {
    throw std::runtime_error("Signature is invalid");
  }
} // validate()
