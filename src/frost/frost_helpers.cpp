#include <iostream>

#include "frost_helpers.hpp"

static std::string const DELIM_SIG = "::";
static std::string const DELIM_NONCES = "::";

#define BIP340_DEFINITION 1

void get_signer_pubkeys(
    const std::vector<participant_pubkeys> &participant_pubkeys,
    std::map<uint32_t, itcoin_secp256k1_gej> *signer_pubkeys)
{
  for (auto &keypair : participant_pubkeys)
  {
    signer_pubkeys->insert(std::pair<uint32_t, itcoin_secp256k1_gej>(keypair.index, keypair.public_key));
  }
  // returns signer_pubkeys
} // get_signer_pubkeys()

signature aggregate_helper(
    const unsigned char *msg,
    uint32_t msglen,
    std::vector<signing_commitment> signing_package,
    std::vector<signing_response> all_responses,
    std::vector<participant_pubkeys> participant_pubkeys)
{
  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys(participant_pubkeys, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = participant_pubkeys.at(0).group_public_key;
  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);
  return group_sig;
}

std::string char_array_to_hex(const unsigned char *bytearray, size_t size)
{
  int i = 0;
  char buffer[2 * size];
  for (uint32_t idx = 0; idx < size; idx++)
  {
    sprintf(buffer + i, "%02x", bytearray[idx]);
    i += 2;
  }
  std::string _str(buffer);
  return _str;
}

void hex_to_char_array(const std::string str, unsigned char *retval)
{
  int d;
  for (uint32_t i = 0, p = 0; p < str.length(); p = p + 2, i++)
  {
    sscanf(&str[p], "%02x", &d);
    retval[i] = (unsigned char)d;
  }
}

std::string serialize_signing_response(const signing_response signature)
{
  // Serialize to string format: index::signature
  unsigned char response_bytes[32];
  itcoin_secp256k1_scalar_get_b32(response_bytes, &signature.response);
  return std::to_string(signature.index) + DELIM_SIG + char_array_to_hex(response_bytes, 32);
}

signing_response deserialize_signing_response(const std::string& serialized)
{
  // Expected: index::signature
  auto end = serialized.find(DELIM_SIG);
  std::string raw_participant_index = serialized.substr(0, end);
  auto participant_index =  static_cast<uint32_t>(std::stoul(raw_participant_index));

  auto start = end + DELIM_SIG.length();
  std::string raw_response = serialized.substr(start, serialized.length());

  unsigned char buffer[32];
  itcoin_secp256k1_scalar parsed_response;
  hex_to_char_array(raw_response, buffer);
  convert_b32_to_scalar(buffer, &parsed_response);

  signing_response res;
  res.index = participant_index;
  res.response = parsed_response;

  return res;
}

std::string serialize_signing_commitment(const signing_commitment commitments)
{
  // Serialize to string format: index::binding_commitment::hiding_commitment
  unsigned char hiding_commitment[SERIALIZED_PUBKEY_SIZE];
  unsigned char binding_commitment[SERIALIZED_PUBKEY_SIZE];
  size_t serialized_point_size = SERIALIZED_PUBKEY_SIZE;

  serialize_point(&(commitments.hiding_commitment), hiding_commitment, &serialized_point_size);
  serialize_point(&(commitments.binding_commitment), binding_commitment, &serialized_point_size);

  return std::to_string(commitments.index)
    + DELIM_NONCES + char_array_to_hex(binding_commitment, serialized_point_size)
    + DELIM_NONCES + char_array_to_hex(hiding_commitment, serialized_point_size);
}

signing_commitment deserialize_signing_commitment(const std::string &serialized)
{
  // Expected string: index::binding_commitment::hiding_commitment
  auto end = serialized.find(DELIM_NONCES);
  std::string raw_index = serialized.substr(0, end);

  auto start = end + DELIM_NONCES.length();
  end = serialized.find(DELIM_NONCES, start);
  std::string raw_binding_commitment = serialized.substr(start, end);

  start = end + DELIM_NONCES.length();
  std::string raw_hiding_commitment = serialized.substr(start, serialized.length());

  // Parse raw elements for creating signing_commitment
  signing_commitment sc = {};
  sc.index = static_cast<uint32_t>(std::stoul(raw_index));

  unsigned char buffer[SERIALIZED_PUBKEY_SIZE];
  hex_to_char_array(raw_binding_commitment, buffer);
  deserialize_point(&(sc.binding_commitment), buffer, SERIALIZED_PUBKEY_SIZE);
  hex_to_char_array(raw_hiding_commitment, buffer);
  deserialize_point(&(sc.hiding_commitment), buffer, SERIALIZED_PUBKEY_SIZE);

  return sc;
}

itcoin_secp256k1_gej deserialize_public_key(std::string serialized_public_key)
{
  if (serialized_public_key.empty())
  {
    throw std::runtime_error("Unable to deserialize an empty public key.");
  }
  // If the public key is represented by 32bytes, then it implicitly encodes a positive Y-coord
  if (serialized_public_key.length() == 2 * (SERIALIZED_PUBKEY_SIZE - 1))
  {
    serialized_public_key = "02" + serialized_public_key;
  }
  itcoin_secp256k1_gej point;
  unsigned char buffer[SERIALIZED_PUBKEY_SIZE];
  hex_to_char_array(serialized_public_key, buffer);
  deserialize_point(&point, buffer, SERIALIZED_PUBKEY_SIZE);
  return point;
}

std::string serialize_signature(const signature signature, const bool compact, unsigned char *output)
{
  if (compact)
  {
    serialize_point_xonly(&(signature.r), output);
    itcoin_secp256k1_scalar_get_b32(&output[32], &signature.z);
    return char_array_to_hex(output, 64);
  }
  size_t serialized_point_size = SERIALIZED_PUBKEY_SIZE;
  serialize_point(&(signature.r), output, &serialized_point_size);
  itcoin_secp256k1_scalar_get_b32(&output[SERIALIZED_PUBKEY_SIZE], &signature.z);
  return char_array_to_hex(output, SERIALIZED_PUBKEY_SIZE + 32);
}

signature deserialize_signature(const std::string &serialized_signature)
{
  signature signature = {};
  unsigned char r_buffer[SERIALIZED_PUBKEY_SIZE];
  unsigned char z_buffer[32];
  std::string _serializedSignature = serialized_signature;

  if (serialized_signature.length() == 2 * (32 + 32))
  {
    // Compact format R_x[32] || z[32]
    _serializedSignature = "02" + _serializedSignature;
  }

  // Serialized Signature is in extended format: {02|03} || R_x[32] || z[32]
  std::string raw_R = _serializedSignature.substr(0,2 * SERIALIZED_PUBKEY_SIZE);
  std::string raw_z = _serializedSignature.substr(2 * SERIALIZED_PUBKEY_SIZE,_serializedSignature.length());
  hex_to_char_array(raw_R, r_buffer);
  deserialize_point(&(signature.r), r_buffer, SERIALIZED_PUBKEY_SIZE);
  hex_to_char_array(raw_z, z_buffer);
  convert_b32_to_scalar(z_buffer, &(signature.z));

  return signature;
}