#include "frost_utils.hpp"

void print_bytearray_as_hex(const unsigned char *bytearray, size_t size)
{
  for (uint32_t i = 0; i < size; i++)
  {
    printf("%02x", bytearray[i]);
  }
  printf("\n");
}

void print_keypairs(std::vector<keypair> keypairs)
{
  for (auto &kp : keypairs)
  {
    size_t serialized_pubkey_size = SERIALIZED_PUBKEY_SIZE;
    unsigned char serialized_pubkey[SERIALIZED_PUBKEY_SIZE];
    unsigned char serialized_group_pubkey[SERIALIZED_PUBKEY_SIZE];
    unsigned char serialized_seckey[32];

    serialize_point(&(kp.group_public_key), serialized_group_pubkey, &serialized_pubkey_size);
    serialize_point(&(kp.public_key), serialized_pubkey, &serialized_pubkey_size);
    itcoin_secp256k1_scalar_get_b32(serialized_seckey, &(kp.secret));

    std::cout << " === Node: " << kp.index << " ===== ===== ===== ===== ===== ===== ===== ===== " << std::endl;
    std::cout << "  group pub key: ";
    print_bytearray_as_hex(serialized_group_pubkey, serialized_pubkey_size);
    std::cout << "  pub key: ";
    print_bytearray_as_hex(serialized_pubkey, serialized_pubkey_size);
    std::cout << "  secret key: ";
    print_bytearray_as_hex(serialized_seckey, 32);
    std::cout << " ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== " << std::endl
              << std::endl;
  }
}

void print_signature(signature signature)
{
  size_t serialized_point_size = SERIALIZED_PUBKEY_SIZE;
  unsigned char serialized_point[SERIALIZED_PUBKEY_SIZE];
  unsigned char serialized_scalar[32];

  serialize_point(&(signature.r), serialized_point, &serialized_point_size);
  itcoin_secp256k1_scalar_get_b32(serialized_scalar, &(signature.z));

  std::cout << " === Signature ===== ===== ===== ===== ===== ===== ===== ===== " << std::endl;
  std::cout << "  R: ";
  print_bytearray_as_hex(serialized_point, serialized_point_size);
  std::cout << "  z: ";
  print_bytearray_as_hex(serialized_scalar, 32);
  std::cout << " ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== " << std::endl
            << std::endl;
}

std::vector<keypair> generate_keypairs_helper(uint32_t num_participants, uint32_t threshold, std::string sig_context)
{
  std::map<uint32_t, std::vector<share>> participant_shares;
  std::vector<keygen_dkg_proposed_commitment> participant_commitments;

  const unsigned char *context = reinterpret_cast<const unsigned char *>(sig_context.c_str());
  uint32_t context_length = sig_context.length();

  // Step 1: Compute dkg_proposed_commitments and participant_shares
  for (uint32_t index = 1; index < num_participants + 1; index++)
  {
    keygen_dkg_proposed_commitment dkg_proposed_commitments;
    std::vector<share> shares;

    // KeyGen_Step1: Participant index generates the share commitments.
    //               Shares contains the polynomial computed for each (generator, participant) pair
    keygen_begin(num_participants, threshold, index, context, context_length, &dkg_proposed_commitments, &shares);
    assert(shares.size() == num_participants);

    // Save in participant_shares all shares (i.e., value of the polynom) computed by the other participants (generators)
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

    // Save the proposed dkg commitments to the participant_commitments list
    participant_commitments.push_back(dkg_proposed_commitments);
  }

  // Step 2a: Participants should exchange their dkg_proposed_commitments (network communication)
  // Step 2b: KeyGen_Step2: Each participant validates the received participant_commitments
  std::vector<keygen_dkg_commitment> valid_commitments;
  std::vector<uint32_t> invalid_peer_ids;
  keygen_receive_commitments_and_validate_peers(participant_commitments,
                                                context, context_length,
                                                valid_commitments, invalid_peer_ids);
  assert(invalid_peer_ids.size() == 0);

  // Step 3: Keygen finalization
  std::vector<keypair> final_keypairs;
  for (uint32_t index = 1; index < num_participants + 1; index++)
  {
    auto participant_share = participant_shares.find(index);
    keypair kp = keygen_finalize(index, (participant_share->second), valid_commitments);
    final_keypairs.push_back(kp);
  }

  return final_keypairs;
}
