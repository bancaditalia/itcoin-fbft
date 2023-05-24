#ifndef FROST_UTILS_HPP
#define FROST_UTILS_HPP

#include <cassert>
#include <iostream>

#include "../../src/frost/frost.hpp"

std::vector<keypair> generate_keypairs_helper(uint32_t num_participants, uint32_t threshold, std::string sig_context);

void print_bytearray_as_hex(const unsigned char *bytearray, size_t size);
void print_keypairs(std::vector<keypair> keypairs);
void print_signature(signature signature);

void serialize_point(const itcoin_secp256k1_gej *point, unsigned char *output, size_t *size);

// TODO: delete, to use commitments from HDW
void gen_signing_commitments_helper(
    uint32_t num_signers,
    std::vector<keypair> *keypairs,
    std::vector<signing_commitment> *signing_commitments,
    std::map<uint32_t, std::vector<nonce_pair>> *nonces);

#endif /* FROST_UTILS_HPP */

