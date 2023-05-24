#include <cassert>
#include <cstring>
#include <iostream>

#include "../../src/frost/frost.hpp"
#include "../../src/frost/frost_internals.hpp"
#include "../../src/frost/secp256k1_extension.h"

/* **** HELPERS **** **** **** **** **** **** **** **** */

void get_signer_pubkeys_from_keypairs(
    const std::vector<keypair> keypairs,
    std::map<uint32_t, itcoin_secp256k1_gej> *signer_pubkeys)
{
  for (auto &keypair : keypairs)
  {
    signer_pubkeys->insert(std::pair<uint32_t, itcoin_secp256k1_gej>(keypair.index, keypair.public_key));
  }
} // get_signer_pubkeys_from_keypairs()

itcoin_secp256k1_gej get_ith_pubkey(uint32_t index, std::vector<keygen_dkg_commitment> commitments /*, point &ith_pubkey */)
{
  itcoin_secp256k1_scalar term;
  itcoin_secp256k1_scalar scalar_unit;
  itcoin_secp256k1_gej ith_pubkey;

  itcoin_secp256k1_scalar_set_int(&term, index);
  itcoin_secp256k1_scalar_set_int(&scalar_unit, 1);

  itcoin_secp256k1_gej_clear(&ith_pubkey);
  point_mul_scalar(&ith_pubkey, &ith_pubkey, &scalar_unit);

  // iterate over each commitment
  for (auto &commitment : commitments)
  {
    itcoin_secp256k1_gej result;
    itcoin_secp256k1_gej_clear(&result);
    point_mul_scalar(&result, &result, &scalar_unit);

    int t = commitment.shares_commit.commitment.size();

    // iterate  over each element in the commitment
    for (int inner_index = 0, cmt_index = t - 1; cmt_index >= 0; cmt_index--, inner_index++)
    {
      itcoin_secp256k1_gej comm_i = commitment.shares_commit.commitment.at(cmt_index);
      itcoin_secp256k1_gej_add_var(&result, &result, &comm_i, NULL);

      // handle constant term
      if (inner_index != t - 1)
      {
        point_mul_scalar(&result, &result, &term);
      }
    }

    itcoin_secp256k1_gej_add_var(&ith_pubkey, &ith_pubkey, &result, NULL);
  }

  return ith_pubkey;
} // get_ith_pubkey()

// This function is used only to test if shares correctly reconstruct the secret
bool reconstruct_secret(const std::vector<share> &shares, itcoin_secp256k1_scalar *secret)
{
  size_t numshares = shares.size();
  if (numshares < 1)
  {
    throw std::runtime_error("No shares provided");
  }

  std::vector<itcoin_secp256k1_scalar> lagrange_coeffs;
  for (size_t i = 0; i < numshares; i++)
  {
    itcoin_secp256k1_scalar num;
    itcoin_secp256k1_scalar den;
    itcoin_secp256k1_scalar den_inverse;
    itcoin_secp256k1_scalar lagrange;

    itcoin_secp256k1_scalar_set_int(&num, 1);
    itcoin_secp256k1_scalar_set_int(&den, 1);

    for (size_t j = 0; j < numshares; j++)
    {
      if (j == i)
      {
        continue;
      }
      itcoin_secp256k1_scalar receiver_j_index;
      itcoin_secp256k1_scalar receiver_i_index;
      itcoin_secp256k1_scalar receiver_index_diff;
      itcoin_secp256k1_scalar receiver_i_index_neg;

      itcoin_secp256k1_scalar_set_int(&receiver_j_index, shares[j].receiver_index);
      itcoin_secp256k1_scalar_set_int(&receiver_i_index, shares[i].receiver_index);
      itcoin_secp256k1_scalar_mul(&num, &num, &receiver_j_index);

      itcoin_secp256k1_scalar_negate(&receiver_i_index_neg, &receiver_i_index);
      itcoin_secp256k1_scalar_add(&receiver_index_diff, &receiver_j_index, &receiver_i_index_neg);
      itcoin_secp256k1_scalar_mul(&den, &den, &receiver_index_diff);
    }

    if (itcoin_secp256k1_scalar_is_zero(&den))
    {
      throw std::runtime_error("Duplicate shares provided");
    }

    itcoin_secp256k1_scalar_inverse(&den_inverse, &den);
    itcoin_secp256k1_scalar_mul(&lagrange, &num, &den_inverse);
    lagrange_coeffs.push_back(lagrange);
  }

  itcoin_secp256k1_scalar_set_int(secret, 0);
  for (size_t i = 0; i < numshares; i++)
  {
    itcoin_secp256k1_scalar secret_share;
    itcoin_secp256k1_scalar_mul(&secret_share, &(lagrange_coeffs[i]), &(shares[i].value));
    itcoin_secp256k1_scalar_add(secret, secret, &secret_share);
  }

  return true;
} // reconstruct_secret()

void gen_signing_commitments_helper(
    uint32_t num_signers,
    std::vector<keypair> *keypairs,
    std::vector<signing_commitment> *signing_commitments,
    std::map<uint32_t, std::vector<nonce_pair>> *nonces)
{
  uint32_t number_nonces_to_generate = 1;
  for (uint32_t counter = 0; counter < num_signers; counter++)
  {
    keypair signing_keypair = keypairs->at(counter);

    std::vector<nonce_pair> participant_nonces;
    std::vector<signing_commitment> participant_commitments;
    preprocess(number_nonces_to_generate, signing_keypair.index, &participant_nonces, &participant_commitments);

    signing_commitments->push_back(participant_commitments.at(0));
    nonces->insert(std::pair<uint32_t, std::vector<nonce_pair>>(counter, participant_nonces));
  }

  assert(nonces->size() == num_signers);

  // returns (signing_commitments, nonces)
} // gen_signing_commitments_helper()

void gen_keypairs_dkg_helper(
    uint32_t num_shares,
    uint32_t threshold,
    std::vector<keypair> *participant_keypairs)
{
  std::map<uint32_t, std::vector<share>> participant_shares;
  std::vector<keygen_dkg_proposed_commitment> participant_commitments;

  // TODO: use some unpredictable string that everyone can derive,
  //       to protect against replay attacks.
  unsigned char context[] = "test";
  uint32_t context_length = 4;

  for (uint32_t counter = 0; counter < num_shares; counter++)
  {
    uint32_t participant_index = counter + 1;

    keygen_dkg_proposed_commitment com;
    std::vector<share> shares;

    keygen_begin(num_shares, threshold, participant_index, context, context_length, &com, &shares);

    for (auto &share_item : shares)
    {
      std::vector<share> _list;
      auto match = participant_shares.find(share_item.receiver_index);
      if (match != participant_shares.end())
      {
        match->second.push_back(share_item);
      }
      else
      {
        _list.push_back(share_item);
        participant_shares.insert(
            std::pair<uint32_t, std::vector<share>>(share_item.receiver_index, _list));
      }
    }

    participant_commitments.push_back(com);
  }

  std::vector<keygen_dkg_commitment> valid_commitments;
  std::vector<uint32_t> invalid_peer_ids;
  keygen_receive_commitments_and_validate_peers(participant_commitments,
                                                context, context_length,
                                                valid_commitments, invalid_peer_ids);
  assert(invalid_peer_ids.size() == 0);

  // now, finalize the protocol
  for (uint32_t index = 1; index < num_shares + 1; index++)
  {
    auto participant_share = participant_shares.find(index);
    keypair kp = keygen_finalize(index, (participant_share->second), valid_commitments);

    participant_keypairs->push_back(kp);
  }
  // returns participant_keypairs
} // gen_keypairs_dkg_helper()

/* **** TESTS **** **** **** **** **** **** **** **** **** **** **** */
void test_keygen_with_dkg_simple()
{
  uint32_t num_shares = 5;
  uint32_t threshold = 3;

  std::map<uint32_t, std::vector<share>> participant_shares;
  std::vector<keygen_dkg_proposed_commitment> participant_commitments;

  unsigned char context[] = "test";
  uint32_t context_length = 4;

  for (uint32_t index = 1; index < num_shares + 1; index++)
  {
    keygen_dkg_proposed_commitment shares_com;
    std::vector<share> shares;

    keygen_begin(num_shares, threshold, index, context, context_length, &shares_com, &shares);
    assert(shares.size() == num_shares);

    for (auto &share_item : shares)
    {
      std::vector<share> _list;
      auto match = participant_shares.find(share_item.receiver_index);
      if (match != participant_shares.end())
      {
        match->second.push_back(share_item);
      }
      else
      {
        _list.push_back(share_item);
        participant_shares.insert(
            std::pair<uint32_t, std::vector<share>>(share_item.receiver_index, _list));
      }
    }
    participant_commitments.push_back(shares_com);
  }

  std::vector<keygen_dkg_commitment> valid_commitments;
  std::vector<uint32_t> invalid_peer_ids;
  keygen_receive_commitments_and_validate_peers(participant_commitments,
                                                context, context_length, valid_commitments, invalid_peer_ids);

  assert(invalid_peer_ids.size() == 0);

  // now, finalize the protocol
  for (uint32_t index = 1; index < num_shares + 1; index++)
  {
    auto participant_share = participant_shares.find(index);
    keypair kp = keygen_finalize(index, (participant_share->second), valid_commitments);
    assert(kp.index == index);
  }

  std::cout << " test_keygen_with_dkg_simple: completed! " << std::endl;
} // test_keygen_with_dkg_simple()

void test_valid_keypair_from_dkg()
{
  uint32_t num_shares = 3;
  uint32_t threshold = 2;

  std::map<uint32_t, std::vector<share>> participant_shares;
  std::vector<keygen_dkg_proposed_commitment> participant_commitments;

  unsigned char context[] = "test";
  uint32_t context_length = 4;

  for (uint32_t index = 1; index < num_shares + 1; index++)
  {
    keygen_dkg_proposed_commitment shares_com;
    std::vector<share> shares;

    keygen_begin(num_shares, threshold, index, context, context_length, &shares_com, &shares);
    assert(shares.size() == num_shares);

    for (auto &share_item : shares)
    {
      std::vector<share> _list;
      auto match = participant_shares.find(share_item.receiver_index);
      if (match != participant_shares.end())
      {
        match->second.push_back(share_item);
      }
      else
      {
        _list.push_back(share_item);
        participant_shares.insert(
            std::pair<uint32_t, std::vector<share>>(share_item.receiver_index, _list));
      }
    }
    participant_commitments.push_back(shares_com);
  }

  std::vector<keygen_dkg_commitment> valid_commitments;
  std::vector<uint32_t> invalid_peer_ids;
  keygen_receive_commitments_and_validate_peers(participant_commitments,
                                                context, context_length, valid_commitments, invalid_peer_ids);

  assert(invalid_peer_ids.size() == 0);

  // now, finalize the protocol
  std::vector<keypair> final_keypairs;
  std::vector<uint32_t> indices;

  for (uint32_t index = 1; index < num_shares + 1; index++)
  {
    auto participant_share = participant_shares.find(index);
    keypair kp = keygen_finalize(index, (participant_share->second), valid_commitments);

    // test our helper function, first.
    const itcoin_secp256k1_gej expected = get_ith_pubkey(index, valid_commitments);
    bool are_equal = point_equal_point(&expected, &(kp.public_key));

    assert(are_equal);

    final_keypairs.push_back(kp);
    indices.push_back(kp.index);
  }

  // now ensure that we can reconstruct the secret, given all the secret keys
  itcoin_secp256k1_scalar output;
  itcoin_secp256k1_scalar_set_int(&output, 0);

  for (auto &keypair : final_keypairs)
  {
    itcoin_secp256k1_scalar zero_coeff = derive_lagrange_coefficient(0, keypair.index, indices);
    itcoin_secp256k1_scalar output_partial;
    itcoin_secp256k1_scalar_mul(&output_partial, &(keypair.secret), &zero_coeff);
    itcoin_secp256k1_scalar_add(&output, &output, &output_partial);
  }

  itcoin_secp256k1_gej received_public;
  compute_point(&received_public, &output);

  // ensure that the secret terms interpolate to the correct public key
  bool are_equal = point_equal_point(&received_public, &(final_keypairs.at(0).group_public_key));
  assert(are_equal);

  std::cout << " test_valid_keypair_from_dkg: completed! " << std::endl;
} // test_valid_keypair_from_dkg()

void test_keygen_with_dkg_invalid_secret_commitment()
{
  uint32_t num_shares = 5;
  uint32_t threshold = 3;

  // std::map<uint32_t, std::vector<share>> participant_shares;
  std::vector<keygen_dkg_proposed_commitment> participant_commitments;

  unsigned char context[] = "test";
  uint32_t context_length = 4;

  for (uint32_t index = 1; index < num_shares + 1; index++)
  {
    keygen_dkg_proposed_commitment shares_com;
    std::vector<share> shares;

    keygen_begin(num_shares, threshold, index, context, context_length, &shares_com, &shares);
    participant_commitments.push_back(shares_com);
  }

  // now, set the first commitments to be invalid
  itcoin_secp256k1_gej _identityPoint;
  itcoin_secp256k1_gej_clear(&_identityPoint);
  participant_commitments[0].shares_commit.commitment[0] = _identityPoint;
  uint32_t invalid_participant_id = participant_commitments[0].index;

  // now, ensure that this participant is marked as invalid
  std::vector<keygen_dkg_commitment> valid_commitments;
  std::vector<uint32_t> invalid_peer_ids;
  keygen_receive_commitments_and_validate_peers(participant_commitments,
                                                context, context_length, valid_commitments, invalid_peer_ids);

  assert(invalid_peer_ids.size() == 1);
  assert(invalid_peer_ids[0] == invalid_participant_id);
  assert(valid_commitments.size() == (num_shares - 1));

  std::cout << " test_keygen_with_dkg_invalid_secret_commitment: completed! " << std::endl;
} // test_keygen_with_dkg_invalid_secret_commitment()

void test_share_simple()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 42);
  generate_shares(s, 5, 2, 0, shares, commitments);

  assert(shares.size() == 5);
  assert(commitments.commitment.size() == 2);

  std::vector<share> recshares;
  recshares.push_back(shares[1]);
  recshares.push_back(shares[3]);

  itcoin_secp256k1_scalar secret;
  bool reconstruct_ok = reconstruct_secret(recshares, &secret);
  assert(reconstruct_ok);
  int are_secret_equal = itcoin_secp256k1_scalar_eq(&secret, &s);
  assert(are_secret_equal == 1);

  std::cout << " test_share_simple: completed! " << std::endl;
} // test_share_simple()

void test_share_not_enough()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 313);
  generate_shares(s, 5, 2, 0, shares, commitments);

  std::vector<share> recshares;
  recshares.push_back(shares[1]);

  itcoin_secp256k1_scalar secret;
  itcoin_secp256k1_scalar_set_int(&secret, 0);

  bool reconstruct_ok = reconstruct_secret(recshares, &secret);
  assert(reconstruct_ok);

  int are_secret_equal = itcoin_secp256k1_scalar_eq(&secret, &s);
  assert(are_secret_equal == 0);

  std::cout << " test_share_not_enough: completed! " << std::endl;
} // test_share_not_enough()

void test_share_dup()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 42);
  generate_shares(s, 5, 2, 0, shares, commitments);

  std::vector<share> recshares;
  recshares.push_back(shares[1]);
  recshares.push_back(shares[1]);

  itcoin_secp256k1_scalar secret;
  itcoin_secp256k1_scalar_set_int(&secret, 0);

  try
  {
    bool reconstruct_ok = reconstruct_secret(recshares, &secret);
    assert(!reconstruct_ok);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Duplicate shares provided") == 0);
  }

  std::cout << " test_share_dup: completed! " << std::endl;
} // test_share_dup()

void test_share_badparam_threshold_not_zero()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 42);
  try
  {
    generate_shares(s, 5, 0, 0, shares, commitments);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Threshold cannot be 0") == 0);
  }
  std::cout << " test_share_badparam_threshold_not_zero: completed! " << std::endl;
} // test_share_badparam_threshold_not_zero()

void test_share_badparam_shares_not_zero()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 42);
  try
  {
    generate_shares(s, 0, 2, 0, shares, commitments);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Number of shares cannot be 0") == 0);
  }
  std::cout << " test_share_badparam_shares_not_zero: completed! " << std::endl;
} // test_share_badparam_shares_not_zero()

void test_share_badparam_thresholds_gt_shares()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 42);
  try
  {
    generate_shares(s, 2, 5, 0, shares, commitments);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Threshold cannot exceed numshares") == 0);
  }
  std::cout << " test_share_badparam_thresholds_gt_shares: completed! " << std::endl;
} // test_share_badparam_thresholds_gt_shares()

void test_share_commitment_valid()
{
  itcoin_secp256k1_scalar s;
  std::vector<share> shares;
  shares_commitment commitments;

  itcoin_secp256k1_scalar_set_int(&s, 42);
  generate_shares(s, 8, 3, 0, shares, commitments);

  for (size_t index = 0; index < shares.size(); index++)
  {
    bool is_valid = false;
    share current_share = shares.at(index);
    is_valid = verify_share(current_share, commitments);
    assert(is_valid);
  }

  std::cout << " test_share_commitment_valid: completed! " << std::endl;
} // test_share_commitment_valid()

void test_share_commitment_invalid()
{
  itcoin_secp256k1_scalar s1;
  itcoin_secp256k1_scalar s2;
  std::vector<share> shares1;
  shares_commitment _commitments1;
  std::vector<share> _shares2;
  shares_commitment commitments2;

  itcoin_secp256k1_scalar_set_int(&s1, 42);
  itcoin_secp256k1_scalar_set_int(&s2, 42);
  generate_shares(s1, 8, 3, 0, shares1, _commitments1);
  generate_shares(s1, 8, 3, 0, _shares2, commitments2);

  for (auto &current_share1 : shares1)
  {
    bool is_valid = false;
    is_valid = verify_share(current_share1, commitments2);
    assert(is_valid == false);
  }

  std::cout << " test_share_commitment_invalid: completed! " << std::endl;
} // test_share_commitment_invalid()

// Test sign and aggregate
void test_preprocess_generates_values()
{
  std::vector<nonce_pair> signing_nonces;
  std::vector<signing_commitment> signing_commitments;
  preprocess(5, 1, &signing_nonces, &signing_commitments);

  assert(signing_commitments.size() == 5);
  assert(signing_nonces.size() == 5);

  uint32_t expected_length = signing_nonces.size() * 2;
  std::vector<itcoin_secp256k1_scalar> seen_nonces;
  for (auto &nonce : signing_nonces)
  {
    seen_nonces.push_back(nonce.hiding_nonce.secret);
    seen_nonces.push_back(nonce.binding_nonce.secret);
  }

  // TODO: seen_nonces.dedup();
  assert(seen_nonces.size() == expected_length);

  std::cout << " test_preprocess_generates_values: completed! " << std::endl;
} // test_preprocess_generates_values()

void test_valid_sign_with_single_dealer()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  shares_commitment _shares_com;
  std::vector<keypair> keypairs;
  keygen_with_dealer(num_signers, threshold, _shares_com, keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;
  // TODO: aggregate get params by ref
  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);

  // Throws an exception if not valid
  validate(msg, msglen, group_sig, group_pubkey);

  std::cout << " test_valid_sign_with_single_dealer: completed! " << std::endl;
} // test_valid_sign_with_single_dealer()

void test_valid_sign_with_dkg_threshold_signers()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;

  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);

  // Throws an exception if not valid
  validate(msg, msglen, group_sig, group_pubkey);

  std::cout << " test_valid_sign_with_dkg_threshold_signers: completed! " << std::endl;
} // test_valid_sign_with_dkg_threshold_signers()

void test_valid_sign_with_dkg_larger_than_threshold_signers()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  uint32_t actual_signers = threshold + 1;

  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(actual_signers, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < actual_signers; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;

  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);

  // Throws an exception if not valid
  validate(msg, msglen, group_sig, group_pubkey);

  std::cout << " test_valid_sign_with_dkg_larger_than_threshold_signers: completed! " << std::endl;
} // test_valid_sign_with_dkg_larger_than_threshold_signers()

void test_valid_sign_with_dkg_larger_params()
{
  uint32_t num_signers = 10;
  uint32_t threshold = 6;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing larger params sign";
  uint32_t msglen = 26;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;

  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);

  // Throws an exception if not valid
  validate(msg, msglen, group_sig, group_pubkey);

  std::cout << " test_valid_sign_with_dkg_larger_params: completed! " << std::endl;
} // test_valid_sign_with_dkg_larger_params()

void test_invalid_sign_too_few_responses_with_dkg()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  // duplicate a share
  all_responses.push_back(all_responses.at(0));

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;

  try
  {
    aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Mismatched number of commitments and responses") == 0);
  }

  std::cout << " test_invalid_sign_too_few_responses_with_dkg: completed! " << std::endl;
} // test_invalid_sign_too_few_responses_with_dkg()

void test_invalid_sign_invalid_response_with_dkg()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  // create a totally invalid response
  itcoin_secp256k1_scalar invalid_response;
  itcoin_secp256k1_scalar_set_int(&invalid_response, 42);
  all_responses[0].response = invalid_response;

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;

  try
  {
    aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Invalid signer response") == 0);
  }

  std::cout << " test_invalid_sign_invalid_response_with_dkg: completed! " << std::endl;
} // test_invalid_sign_invalid_response_with_dkg()

void test_invalid_validate_bad_group_public_key_with_dkg()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);

  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;
  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);

  // use one of the participant's public keys instead
  group_pubkey = keypairs.at(0).public_key;
  try
  {
    validate(msg, msglen, group_sig, group_pubkey);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Signature is invalid") == 0);
  }

  std::cout << " test_invalid_validate_bad_group_public_key_with_dkg: completed! " << std::endl;
} // test_invalid_validate_bad_group_public_key_with_dkg()

void test_invalid_sign_bad_group_public_key_with_dkg()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);
  // use one of the participant's public keys instead
  itcoin_secp256k1_gej group_pubkey = keypairs.at(0).public_key;

  try
  {
    aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Invalid signer response") == 0);
  }

  std::cout << " test_invalid_sign_bad_group_public_key_with_dkg: completed! " << std::endl;
} // test_invalid_sign_bad_group_public_key_with_dkg()

void test_invalid_sign_used_nonce_with_dkg()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  std::vector<keypair> keypairs;
  gen_keypairs_dkg_helper(num_signers, threshold, &keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  auto match = signing_nonces.find(0);
  std::vector<nonce_pair> my_signing_nonces = match->second;
  my_signing_nonces.erase(my_signing_nonces.begin());

  keypair kp = keypairs.at(0);
  try
  {
    sign(kp, signing_package, my_signing_nonces, msg, msglen);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "No matching signing nonce for signer") == 0);
  }

  std::cout << " test_invalid_sign_used_nonce_with_dkg: completed! " << std::endl;
} // test_invalid_sign_used_nonce_with_dkg()

void test_invalid_sign_with_dealer()
{
  uint32_t num_signers = 5;
  uint32_t threshold = 3;
  shares_commitment _shares_com;
  std::vector<keypair> keypairs;
  keygen_with_dealer(num_signers, threshold, _shares_com, keypairs);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  std::vector<signing_commitment> signing_package;
  std::map<uint32_t, std::vector<nonce_pair>> signing_nonces;
  gen_signing_commitments_helper(threshold, &keypairs, &signing_package, &signing_nonces);

  // test duplicated participants
  std::vector<signing_response> all_responses;
  for (uint32_t counter = 0; counter < threshold; counter++)
  {
    auto match = signing_nonces.find(counter);
    std::vector<nonce_pair> my_signing_nonces = match->second;
    assert(my_signing_nonces.size() == 1);

    keypair kp = keypairs.at(counter);
    signing_response res = sign(kp, signing_package, my_signing_nonces, msg, msglen);

    all_responses.push_back(res);
  }

  std::map<uint32_t, itcoin_secp256k1_gej> signer_pubkeys;
  get_signer_pubkeys_from_keypairs(keypairs, &signer_pubkeys);

  itcoin_secp256k1_gej group_pubkey = keypairs.at(1).group_public_key;
  signature group_sig = aggregate(msg, msglen, group_pubkey, signing_package, all_responses, signer_pubkeys);

  try
  {
    itcoin_secp256k1_gej invalid_group_pubkey;
    itcoin_secp256k1_gej_clear(&invalid_group_pubkey);
    validate(msg, msglen, group_sig, invalid_group_pubkey);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Signature is invalid") == 0);
  }

  std::cout << " test_invalid_sign_with_dealer: completed! " << std::endl;
} // test_invalid_sign_with_dealer()

void test_valid_validate_single_party()
{
  itcoin_secp256k1_scalar privkey;
  itcoin_secp256k1_scalar nonce;
  itcoin_secp256k1_gej pubkey;
  itcoin_secp256k1_gej commitment;

  itcoin_secp256k1_scalar_set_int(&privkey, 42);
  compute_point(&pubkey, &privkey);
  itcoin_secp256k1_scalar_set_int(&nonce, 5);
  compute_point(&commitment, &nonce);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  itcoin_secp256k1_scalar challenge;

  compute_challenge(msg, msglen, pubkey, commitment, &challenge);

  itcoin_secp256k1_scalar z;
  itcoin_secp256k1_scalar_mul(&z, &privkey, &challenge);
  itcoin_secp256k1_scalar_add(&z, &nonce, &z);

  signature sign;
  sign.r = commitment;
  sign.z = z;
  try
  {
    validate(msg, msglen, sign, pubkey);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Signature is invalid") == 0);
  }
  std::cout << " test_valid_validate_single_party: completed! " << std::endl;
} // test_valid_validate_single_party()

void test_invalid_validate_single_party()
{
  itcoin_secp256k1_scalar privkey;
  itcoin_secp256k1_scalar nonce;
  itcoin_secp256k1_scalar invalid_nonce;
  itcoin_secp256k1_gej pubkey;
  itcoin_secp256k1_gej commitment;

  itcoin_secp256k1_scalar_set_int(&privkey, 42);
  compute_point(&pubkey, &privkey);
  itcoin_secp256k1_scalar_set_int(&nonce, 5);
  itcoin_secp256k1_scalar_set_int(&invalid_nonce, 100);
  compute_point(&commitment, &nonce);

  unsigned char msg[] = "testing sign";
  uint32_t msglen = 12;
  itcoin_secp256k1_scalar challenge;

  compute_challenge(msg, msglen, pubkey, commitment, &challenge);

  itcoin_secp256k1_scalar z;
  itcoin_secp256k1_scalar_mul(&z, &privkey, &challenge);
  itcoin_secp256k1_scalar_add(&z, &invalid_nonce, &z);

  signature sign;
  sign.r = commitment;
  sign.z = z;
  try
  {
    validate(msg, msglen, sign, pubkey);
  }
  catch (std::runtime_error const& error)
  {
    assert(strcmp(error.what(), "Signature is invalid") == 0);
  }
  std::cout << " test_invalid_validate_single_party: completed! " << std::endl;
} // test_invalid_validate_single_party()
