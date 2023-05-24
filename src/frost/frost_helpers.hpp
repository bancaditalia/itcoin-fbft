#ifndef FROST_HELPERS_HPP
#define FROST_HELPERS_HPP

#include "frost.hpp"
#include "frost_internals.hpp"

struct participant_pubkeys
{
  uint32_t index;
  itcoin_secp256k1_gej public_key;
  itcoin_secp256k1_gej group_public_key;
};

signature aggregate_helper(const unsigned char *msg, uint32_t msglen, std::vector<signing_commitment> signing_package,
                           std::vector<signing_response> all_responses, std::vector<participant_pubkeys> participant_pubkeys);

std::string serialize_signing_response(const signing_response signature);
signing_response deserialize_signing_response(const std::string& serialized);

std::string serialize_signing_commitment(const signing_commitment commitments);
signing_commitment deserialize_signing_commitment(const std::string &serialized);

std::string serialize_signature(const signature signature, const bool compact, unsigned char *output);
signature deserialize_signature(const std::string &serialized_signature);

itcoin_secp256k1_gej deserialize_public_key(std::string serialized_public_key);
void convert_b32_to_scalar(unsigned char *hash_value, itcoin_secp256k1_scalar *output);

#endif /* FROST_HELPERS_HPP */
