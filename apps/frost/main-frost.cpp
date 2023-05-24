#include "frost_utils.hpp"
#include "frost_tests.hpp"
#include "../../src/frost/frost_helpers.hpp"
#include "../../src/frost/secp256k1_extension.h"

#include <util/translation.h>
// This definition is required by itcoin-core
// NB: This line was placed just after the util/translation.h include, but it's now moved there
// The first program instruction should always follow all includes,
// otherwise cmake depend.make will not include the header files that follow the first program instruction.
const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

std::vector<keypair> hardcodedKeys();
std::vector<keypair> generateKeys(uint32_t num_participants, uint32_t threshold, std::string &sig_context);
void run(std::vector<keypair> keypairs, uint32_t threshold, unsigned char *msg, uint32_t msglen);
void hex_to_char_array2(const std::string str, unsigned char *retval);
void run_tests();

int main()
{
/*  // uint32_t num_participants = 4;
  uint32_t threshold = 3;
  std::string sig_context = "itcoin";
  // auto keys = generateKeys(num_participants, threshold, sig_context);
  auto keys = hardcodedKeys();

  unsigned char msg[13] = "hello world!";
  uint32_t msglen = 13;

  run(keys, threshold, msg, msglen);

  std::cout << " -- Running tests -- " << std::endl;*/
  run_tests();

  return 0;
}
void run_tests()
{
  // keygen tests
  test_share_simple();
  test_share_not_enough();
  test_share_dup();
  test_share_badparam_threshold_not_zero();
  test_share_badparam_shares_not_zero();
  test_share_badparam_thresholds_gt_shares();
  test_share_commitment_valid();
  test_share_commitment_invalid();
  test_keygen_with_dkg_simple();
  test_keygen_with_dkg_invalid_secret_commitment();
  test_valid_keypair_from_dkg();

  // sign tests
  test_preprocess_generates_values();
  test_valid_sign_with_single_dealer();
  test_valid_sign_with_dkg_threshold_signers();
  test_valid_sign_with_dkg_larger_than_threshold_signers();
  test_valid_sign_with_dkg_larger_params();
  test_invalid_sign_too_few_responses_with_dkg();
  test_invalid_sign_invalid_response_with_dkg();
  test_invalid_sign_bad_group_public_key_with_dkg();
  test_invalid_sign_used_nonce_with_dkg();
  test_invalid_sign_with_dealer();
  test_valid_validate_single_party();
  test_invalid_validate_single_party();
  test_invalid_validate_bad_group_public_key_with_dkg();
}

void hex_to_char_array2(const std::string str, unsigned char *retval)
{
  int d;
  for (uint32_t i = 0, p = 0; p < str.length(); p = p + 2, i++)
  {
    sscanf(&str[p], "%02x", &d);
    retval[i] = (unsigned char)d;
  }
}

void run(std::vector<keypair> keypairs, uint32_t threshold, unsigned char *msg, uint32_t msglen) {
  // Step 1: commitment generation
  std::vector<signing_commitment> signing_commitments;
  std::map<uint32_t, std::vector<nonce_pair>> _private_signing_nonces;
  // TODO: remove this, to get nonce and commitments from HDW
  gen_signing_commitments_helper(threshold, &keypairs, &signing_commitments, &_private_signing_nonces);

  // Step 2: sign
  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < signing_commitments.size(); counter++)
  {
    std::vector<nonce_pair> my_signing_nonces = _private_signing_nonces.find(counter)->second;
    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_commitments, my_signing_nonces, msg, msglen);
    all_responses.push_back(res);
  }

  // Step 3: aggregate
  std::vector<participant_pubkeys> participant_pubkeyss;
  for(auto &kp : keypairs)
  {
    participant_pubkeys pkp;
    pkp.index = kp.index;
    pkp.public_key = kp.public_key;
    pkp.group_public_key = kp.group_public_key;
    participant_pubkeyss.push_back(pkp);
  }
  signature group_sig = aggregate_helper(msg, msglen, signing_commitments, all_responses, participant_pubkeyss);

  // Step 4: validate
  itcoin_secp256k1_gej group_pubkey = keypairs.at(0).group_public_key;
  validate(msg, msglen, group_sig, group_pubkey);

  // Print
  print_signature(group_sig);
}

std::vector<keypair> generateKeys(uint32_t num_participants, uint32_t threshold, std::string &sig_context) {
  std::vector<keypair> keypairs = generate_keypairs_helper(num_participants, threshold, sig_context);
  print_keypairs(keypairs);
  return keypairs;
}

std::vector<keypair> hardcodedKeys(){
  std::vector<keypair> keypairs;
  unsigned char buffer[32];
  itcoin_secp256k1_scalar s1;
  itcoin_secp256k1_scalar s2;
  itcoin_secp256k1_scalar s3;
  itcoin_secp256k1_scalar s4;

  itcoin_secp256k1_gej gpk = deserialize_public_key("02a30308432f861114611b4d93c22ed1fac63115e6ab7b585b2bd26239cf0403e5");

  itcoin_secp256k1_gej pk1 = deserialize_public_key("039aec038c40ea94438235fb784f73ae78337fc0e362aaef949c876ddcda976636");
  itcoin_secp256k1_gej pk2 = deserialize_public_key("035ec6cad80f4b9896f615f45b882aa78bbe6a6affed6de3a2895ea7242f1356a8");
  itcoin_secp256k1_gej pk3 = deserialize_public_key("0274574d52274c1c1edd5f692928fdd69200664ed7bcac990a9f463acdf939c625");
  itcoin_secp256k1_gej pk4 = deserialize_public_key("03c88d878087f6962d057806e723860164d620b4b7f8625f244b0a5318a56d2ae7");

  hex_to_char_array2("392316d5b4faf2febee412e159aac9aa62e4af191251fe146c7194eb20b9df43", buffer);
  convert_b32_to_scalar(buffer, &s1);
  hex_to_char_array2("0dd36e7627d5541307111ecc33327928ac331c90748190e38aee0b005b909684", buffer);
  convert_b32_to_scalar(buffer, &s2);
  hex_to_char_array2("e283c6169aafb5274f3e2ab70cba28a5b03066ee85f9c3ee693cdfa2669d8f06", buffer);
  convert_b32_to_scalar(buffer, &s3);
  hex_to_char_array2("b7341db70d8a163b976b36a1e641d823f97ed465e82956bd87b955b7a1744647", buffer);
  convert_b32_to_scalar(buffer, &s4);

  itcoin_secp256k1_scalar_negate(&s1, &s1);
  itcoin_secp256k1_scalar_negate(&s2, &s2);
  itcoin_secp256k1_scalar_negate(&s3, &s3);
  itcoin_secp256k1_scalar_negate(&s4, &s4);

  keypair kp1;
  kp1.index = 1;
  kp1.secret = s1;
  kp1.public_key = pk1;
  kp1.group_public_key = gpk;
  keypair kp2;
  kp2.index = 2;
  kp2.secret = s2;
  kp2.public_key = pk2;
  kp2.group_public_key = gpk;
  keypair kp3;
  kp3.index = 3;
  kp3.secret = s3;
  kp3.public_key = pk3;
  kp3.group_public_key = gpk;
  keypair kp4;
  kp4.index = 4;
  kp4.secret = s4;
  kp4.public_key = pk4;
  kp4.group_public_key = gpk;

  keypairs.push_back(kp1);
  keypairs.push_back(kp2);
  keypairs.push_back(kp3);
  keypairs.push_back(kp4);
  print_keypairs(keypairs);

  return keypairs;
}

// main()
