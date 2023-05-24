#ifndef FROST_TESTS_HPP
#define FROST_TESTS_HPP

void test_keygen_with_dkg_simple();
void test_valid_keypair_from_dkg();
void test_keygen_with_dkg_invalid_secret_commitment();
void test_share_simple();
void test_share_not_enough();
void test_share_dup();
void test_share_badparam_threshold_not_zero();
void test_share_badparam_shares_not_zero();
void test_share_badparam_thresholds_gt_shares();
void test_share_commitment_valid();
void test_share_commitment_invalid();

void test_preprocess_generates_values();
void test_valid_sign_with_single_dealer();
void test_valid_sign_with_dkg_threshold_signers();
void test_valid_sign_with_dkg_larger_than_threshold_signers();
void test_valid_sign_with_dkg_larger_params();
void test_invalid_sign_too_few_responses_with_dkg();
void test_invalid_sign_invalid_response_with_dkg();
void test_invalid_sign_bad_group_public_key_with_dkg();
void test_invalid_sign_used_nonce_with_dkg();
void test_invalid_sign_with_dealer();
void test_valid_validate_single_party();
void test_invalid_validate_single_party();
void test_invalid_validate_bad_group_public_key_with_dkg();

#endif /* FROST_TESTS_HPP */
