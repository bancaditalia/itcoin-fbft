#include "wallet.h"

#include <base58.h>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <script/interpreter.h>

#include "../PbftConfig.h"
#include "../block/extract.h"
#include "../block/psbt_utils.h"
#include "../pbft/messages/messages.h"
#include "../transport/btcclient.h"

#include <cmath>       /* floor */

#define DELIM_PRESIG "+"
#define DELIM_PARTICIPANT_INDEX "#"

using Message = itcoin::pbft::messages::Message;

namespace itcoin::wallet {

    ThreeFBFTWalletImpl::ThreeFBFTWalletImpl(const itcoin::PbftConfig &conf, transport::BtcClient &bitcoind)
        : Wallet(conf), BitcoinRpcWallet(conf, bitcoind), RoastWallet(conf),
          m_keypair(InitializeKeyPair(conf, bitcoind)) {
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% ThreeFBFTWalletImpl will sign using pubkey address %2%.") % m_conf.id() %
            m_pubkey_address);

      PrivateNonceDerivation nonce_derivation;
      nonce_derivation.index = conf.id() + 1;
      nonce_derivation.master_nonce = m_keypair.secret;
      nonce_derivation.valid = false;
      m_nonce_derivation_ = nonce_derivation;

      std::vector<uint32_t> participants;
      for (auto &rep_conf: m_conf.replica_set_v()) {
        PublicCommitmentDerivation cmt_derivation;
        cmt_derivation.index = rep_conf.id() + 1;
        cmt_derivation.master_commitment = deserialize_public_key(rep_conf.pubkey());
        cmt_derivation.valid = false;

        participants.push_back(cmt_derivation.index);
        m_commitments_derivations.insert({cmt_derivation.index, cmt_derivation});
      }

      int quorum = 1.0 + 2.0 * (floor((participants.size() - 1.0) / 3));
      ComputeCombinations(participants, m_signers_combinations, quorum);

    }

    keypair
    ThreeFBFTWalletImpl::InitializeKeyPair(const itcoin::PbftConfig &conf, transport::BtcClient &bitcoind) const {
      auto pubkey_address = conf.replica_set_v().at(conf.id()).p2pkh();
      std::string b58_privkey = bitcoind.dumpprivkey(pubkey_address);
      std::vector<unsigned char> raw_privkey;
      // private key is coded as base58check({90|ef} || privateKey)
      bool decoded = DecodeBase58Check(b58_privkey, raw_privkey, 256 + 8);
      if (!decoded) {
        std::string error = "Error parsing private key: " + b58_privkey;
        BOOST_LOG_TRIVIAL(error) << error;
        throw std::runtime_error(error);
      }
      // remove first byte that encodes if the private key is for main-net (90) or testnet (ef)
      raw_privkey.erase(raw_privkey.begin(), raw_privkey.begin() + 1);

      itcoin_secp256k1_scalar privkey;
      convert_b32_to_scalar(raw_privkey.data(), &privkey);
      BOOST_LOG_TRIVIAL(debug) << "Private key correctly parsed!";

      keypair kp = {};
      kp.index = m_conf.id() + 1;
      kp.secret = privkey;
      kp.public_key = deserialize_public_key(m_conf.replica_set_v().at(m_conf.id()).pubkey());
      kp.group_public_key = deserialize_public_key(m_conf.group_public_key());

      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% has correctly initialized its keypair.") % m_conf.id());

      return kp;
    }

    signature ThreeFBFTWalletImpl::AggregateSignatureShares(const uint256 &message_digest,
                                                            const std::vector<std::string> &signature_shares) const {
      BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% is aggregating signature shares") % m_conf.id());

      // Step 1: Extract combination index;
      int combination_index = RetrieveSignerCombination(signature_shares);
      bool current_signer_included = ParticipantInCombination(combination_index, m_keypair.index);

      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Found combination index: %2%. Current signer included? %3%") % m_conf.id() %
            std::to_string(combination_index) % current_signer_included);

      // Step 2: Throw exception if received a combination that does not include the current signer
      if (!current_signer_included) {
        std::string error_msg = boost::str(
            boost::format("R%1% received a combination of signature shares that do not include signer itself") %
            m_conf.id());
        BOOST_LOG_TRIVIAL(error) << error_msg;
        throw std::runtime_error(error_msg);
      }

      // Step 3: Retrieve all signature shares for the current combination of signers
      std::vector<signing_response> all_responses;
      for (auto &signature_shares_by_participant: signature_shares) {
        // Pop out the participant index from the beginning of the serialized signature shares
        std::string delimiter{DELIM_PARTICIPANT_INDEX};
        uint32_t end = signature_shares_by_participant.find(delimiter, 0);
        std::string raw_signature_participant_index = signature_shares_by_participant.substr(0, end);

        // Skip signature if participant is not included in current combination of signers
        auto signature_participant_index = static_cast<uint32_t>(std::stoul(raw_signature_participant_index));
        if (!ParticipantInCombination(combination_index, signature_participant_index)) {
          continue;
        }

        // Extract the signature share for the current combination of signer
        std::string signature_shares_by_combination_index = signature_shares_by_participant
            .substr(end + delimiter.size(), signature_shares_by_participant.size());
        auto serialized_signature_share = ExtractSignatureShareByCombinationIndex(
            signature_shares_by_combination_index, ";", combination_index);

        // Parse the signature share (of current participant, for given combination index)
        auto signing_response = deserialize_signing_response(serialized_signature_share);
        all_responses.push_back(signing_response);
      }

      // Step 3: Retrieve signing commitments
      std::vector<signing_commitment> signing_commitments;
      auto combination = m_signers_combinations.at(combination_index);
      for (auto &participant_index: combination) {
        signing_commitment _signing_commitment = this->DeriveParticipantCommitments(message_digest, participant_index);
        signing_commitments.push_back(_signing_commitment);
      }

      // Step 4: Retrieve public keys of other participants
      std::vector<participant_pubkeys> participant_pubkeys_vec;
      for (auto &rep_conf: m_conf.replica_set_v()) {
        uint32_t participant_index = rep_conf.id() + 1;
        if (!ParticipantInCombination(combination_index, participant_index)) {
          continue;
        }
        participant_pubkeys pubkey = {};
        pubkey.index = participant_index;
        pubkey.public_key = deserialize_public_key(rep_conf.pubkey());
        pubkey.group_public_key = m_keypair.group_public_key;
        participant_pubkeys_vec.push_back(pubkey);
      }
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% has correctly retrieved signature shares, presignatures, and participant public keys") %
            m_conf.id());

      try {
        // Step 4: Aggregate signature shares
        signature signature = aggregate_helper(message_digest.begin(), 32, signing_commitments,
                                               all_responses, participant_pubkeys_vec);
        BOOST_LOG_TRIVIAL(debug) << boost::str(
              boost::format("R%1% has correctly aggregating the signature and is going to validate it") % m_conf.id());

        // Additional Step 5: Validate aggregated signature locally (not needed)
        validate(message_digest.begin(), 32, signature, m_keypair.group_public_key);
        BOOST_LOG_TRIVIAL(debug)
          << boost::str(boost::format("R%1% has correctly validated the aggregated signature") % m_conf.id());

        return signature;
      } catch (std::runtime_error const &e) {
        BOOST_LOG_TRIVIAL(error)
          << boost::str(boost::format("R%1% Error while aggregating signature: %2%") % m_conf.id() % e.what());
        throw e;
      }
    }

    bool ThreeFBFTWalletImpl::ParticipantInCombination(int combination_index, uint32_t participant_index) const {
      return std::any_of(
          m_signers_combinations.at(combination_index).begin(),
          m_signers_combinations.at(combination_index).end(),
          [&participant_index](const uint32_t &selected_participant_index) {
              return selected_participant_index == participant_index;
          });
    }

    std::string ThreeFBFTWalletImpl::ExtractSignatureShareByCombinationIndex(const std::string &_signature_share,
                                                                             const std::string &delimiter,
                                                                             const uint32_t combination_index) const {
      uint32_t start = 0;
      uint32_t end = 0;
      std::string commitment_delimiter = ":::";
      while (end < _signature_share.size()) {
        end = _signature_share.find(delimiter, start);
        std::string item = _signature_share.substr(start, end - start);
        if (item.empty()) {
          break;
        }
        uint32_t cmt_delimiter_end = item.find(commitment_delimiter, 0);
        std::string raw_sig_combination_index = item.substr(0, cmt_delimiter_end);
        std::string sig_share = item.substr(cmt_delimiter_end + commitment_delimiter.size(), item.size());
        auto sig_combination_index = static_cast<uint32_t>(std::stoul(raw_sig_combination_index));
        if (combination_index == sig_combination_index) {
          return sig_share;
        }
        start = end + delimiter.length();
      }

      BOOST_LOG_TRIVIAL(error) << "Unable to find signature share for given combination index";
      throw std::runtime_error("Unable to find signature share for given combination index");
    }

    std::string ThreeFBFTWalletImpl::GetBlockSignature(const CBlock &block) {

      BOOST_LOG_TRIVIAL(debug) << boost::str(
        boost::format("R%1% Starts computing signature shares.")
        % m_conf.id()
      );

      std::string serialized_res = std::to_string(m_keypair.index) + DELIM_PARTICIPANT_INDEX;

      // Step 1: Get Signature Hash
      CBlock _block(block);
      auto [spendTx, toSpendTx] = itcoin::block::signetTxs(_block, m_conf.getSignetChallenge());
      uint256 hash_out = TaprootSignatureHash(spendTx, toSpendTx);

      // Step 2: retrieve: keypair, nonces, commitments;
      for (int combination_index = 0; combination_index < m_signers_combinations.size(); combination_index++) {
        auto combination = m_signers_combinations.at(combination_index);

        bool included = false;
        for (auto &participant_index: combination) {
          if (m_keypair.index == participant_index) {
            included = true;
            break;
          }
        }

        signing_response res = {};
        if (!included) {
          itcoin_secp256k1_scalar_set_int(&res.response, 0);
          res.index = m_keypair.index;
        } else {
          std::vector<signing_commitment> signing_commitments;
          std::vector<nonce_pair> my_signing_nonces;

          // Derive signing commitments
          for (auto &participant_index: combination) {
            signing_commitment _signing_commitment = DeriveParticipantCommitments(hash_out, participant_index);
            signing_commitments.push_back(_signing_commitment);
          }

          // Derive my nonce
          nonce_pair np = DeriveNoncePair(hash_out);
          my_signing_nonces.push_back(np);

          // Step 3: Sign
          std::string block_as_string = hash_out.GetHex();
          BOOST_LOG_TRIVIAL(trace) << boost::str(boost::format("R%1% block hash: %2%") % m_conf.id() % block_as_string);
          res = sign(m_keypair, signing_commitments, my_signing_nonces, hash_out.begin(), 32);
        }

        // Step 4: serialize signature
        std::string _serialized_res = serialize_signing_response(res);
        serialized_res += std::to_string(combination_index) + ":::" + _serialized_res + ";";
      }

      BOOST_LOG_TRIVIAL(debug) << boost::str(
        boost::format("R%1% Ends computing signature shares.")
        % m_conf.id()
      );

      BOOST_LOG_TRIVIAL(trace) << boost::str(
        boost::format("R%1% Generated signature share: %2%.")
        % m_conf.id()
        % serialized_res
      );

      // Step 5: Return a concatenated signature, with a share for each combination (id#[combination_index:::share;]*)
      return serialized_res;
    }

    signing_commitment ThreeFBFTWalletImpl::DeriveParticipantCommitments(const uint256 &hash_out,
                                                                         const uint32_t &participant_index) const {
      auto commitments_derivations_pair = m_commitments_derivations.find(participant_index);
      if (commitments_derivations_pair == m_commitments_derivations.end()) {
        auto error_msg = boost::str(
            boost::format("R%1% unable to find commitment derivation for participant with index %2%") % m_conf.id() %
            std::to_string(participant_index));
        BOOST_LOG_TRIVIAL(error) << error_msg;
        throw std::runtime_error(error_msg);
      }
      auto participant_commitment_derivation = commitments_derivations_pair->second;
      participant_commitment_derivation.chaincode = ChainCode(hash_out);
      unsigned char buffer[33];
      size_t buffer_size = 33;
      signing_commitment _signing_commitment;
      _signing_commitment.index = participant_index;
      DerivePublicFromPublic(participant_commitment_derivation, kBindingCommitmentChildIndex);
      serialize_point(&participant_commitment_derivation.child_commitment, buffer, &buffer_size);
      deserialize_point(&_signing_commitment.binding_commitment, buffer, buffer_size);
      DerivePublicFromPublic(participant_commitment_derivation, kHidingCommitmentChildIndex);
      serialize_point(&participant_commitment_derivation.child_commitment, buffer, &buffer_size);
      deserialize_point(&_signing_commitment.hiding_commitment, buffer, buffer_size);
      return _signing_commitment;
    }

    nonce_pair ThreeFBFTWalletImpl::DeriveNoncePair(const uint256 &hash_out) {
      m_nonce_derivation_.chaincode = ChainCode(hash_out);
      nonce_pair np = {};
      unsigned char buffer[32];
      int overflow = 0;
      DerivePrivateFromPrivate(m_nonce_derivation_, kBindingCommitmentChildIndex);
      itcoin_secp256k1_scalar_get_b32(buffer, &m_nonce_derivation_.child_nonce);
      itcoin_secp256k1_scalar_set_b32(&np.binding_nonce.secret, buffer, &overflow);
      compute_point(&np.binding_nonce.commitment, &np.binding_nonce.secret);
      DerivePrivateFromPrivate(m_nonce_derivation_, kHidingCommitmentChildIndex);
      itcoin_secp256k1_scalar_get_b32(buffer, &m_nonce_derivation_.child_nonce);
      itcoin_secp256k1_scalar_set_b32(&np.hiding_nonce.secret, buffer, &overflow);
      compute_point(&np.hiding_nonce.commitment, &np.hiding_nonce.secret);
      return np;
    }


    CBlock ThreeFBFTWalletImpl::FinalizeBlock(const CBlock &block,
                                              const std::vector<std::string> signature_shares) const {

      BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Finalizing block signature.") % m_conf.id());

      // Step 1: Create digest signed by participants
      CBlock _block(block);
      auto [spendTx, toSpendTx] = itcoin::block::signetTxs(_block, m_conf.getSignetChallenge());
      uint256 hash_out = TaprootSignatureHash(spendTx, toSpendTx);

      // Step 2: Aggregate signature shares
      std::string block_as_string = hash_out.GetHex();
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Aggregating signature shares on block: %2%.") % m_conf.id() % block_as_string);
      signature signature = this->AggregateSignatureShares(hash_out, signature_shares);

      // Step 3: Derive the signet solution
      unsigned char serialized_signature_buf[3 + 32 + 32];
      serialized_signature_buf[0] = OP_0;
      serialized_signature_buf[1] = 0x01; // Serialize the number of elements in the vector (1 item)
      serialized_signature_buf[2] = 0x40; // Serialize the item[0] length (64 bytes)
      std::string serialized_signature = serialize_signature(signature, true, &(serialized_signature_buf[3]));
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Adding signature %2% to block: %3%.") % m_conf.id() % serialized_signature %
            block_as_string);

      // Step 4: Add signet solution to spent transaction
      spendTx.vin[0].scriptSig = CScript(&(serialized_signature_buf[3]), &(serialized_signature_buf[3]) + 64);

      // Step 5: Append the signet solution
      std::vector<unsigned char> serialized_signatureBytes{serialized_signature_buf, serialized_signature_buf + 3 + 64};
      block::appendSignetSolution(&_block, serialized_signatureBytes);

      BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Block finalized.") % m_conf.id());
      return _block;
    }

    int ThreeFBFTWalletImpl::RetrieveSignerCombination(const std::vector<std::string> &signature_shares) const {
      int combination_index = -1;
      std::vector<uint32_t> participant_indexes;
      for (auto &signature_share: signature_shares) {
        std::string delimiter{DELIM_PARTICIPANT_INDEX};
        uint32_t end = signature_share.find(delimiter, 0);
        std::string participant_index_raw = signature_share.substr(0, end);
        auto participant_index = static_cast<uint32_t>(std::stoul(participant_index_raw));
        participant_indexes.push_back(participant_index);
      }
      std::sort(participant_indexes.begin(), participant_indexes.end(),
                [](const int &lhs, const int &rhs) { return lhs < rhs; });

      if (m_signers_combinations.at(0).size() > participant_indexes.size()) {
        std::string error_msg = boost::str(boost::format("R%1% unable to find the combination for the signature shares"
                                                         " received (by %2% participants)") % m_conf.id() %
                                           participant_indexes.size());
        BOOST_LOG_TRIVIAL(error) << error_msg;
        throw std::runtime_error(error_msg);
      }

      int toDelete = participant_indexes.size() - m_signers_combinations.at(0).size();
      // Shrink set of participant indexes if more than needed are provided
      if (toDelete > 0) {
        std::vector<uint32_t> _participant_indexes;
        for (int i = 0; i < participant_indexes.size(); i++) {
          // Shrink set of participant indexes if more than needed are provided
          // Include current participant among signers
          if (participant_indexes.at(i) != m_keypair.index && toDelete > 0) {
            toDelete--;
            continue;
          }
          _participant_indexes.push_back(participant_indexes.at(i));
        }
        participant_indexes = _participant_indexes;
      }


      for (int i = 0; i < m_signers_combinations.size(); i++) {
        auto combination = m_signers_combinations.at(i);
        bool goNext = false;
        for (int j = 0; j < combination.size(); j++) {
          if (combination.at(j) != participant_indexes.at(j)) {
            goNext = true;
            break;
          }
        }
        if (!goNext) {
          combination_index = i;
          break;
        }
      }

      return combination_index;
    }

    uint256 ThreeFBFTWalletImpl::TaprootSignatureHash(const CMutableTransaction &spendTx,
                                                      const CMutableTransaction &toSpendTx) const {
      PrecomputedTransactionData cache;
      cache.Init(spendTx, {toSpendTx.vout[0]}, true);
      ScriptExecutionData execdata;
      execdata.m_annex_init = true;
      execdata.m_annex_present = false;
      uint256 hash_out;
      bool computed = SignatureHashSchnorr(hash_out, execdata, spendTx, 0,
                                           SIGHASH_DEFAULT, SigVersion::TAPROOT, cache,
                                           MissingDataBehavior::FAIL);
      if (!computed) {
        std::string error_msg = boost::str(boost::format("R%1% Cannot compute SignatureHashSchnorr!") % m_conf.id());
        BOOST_LOG_TRIVIAL(error) << error_msg;
        throw std::runtime_error(error_msg);
      }
      return hash_out;
    }

    void ThreeFBFTWalletImpl::AppendSignature(itcoin::pbft::messages::Message &message) const {
      BitcoinRpcWallet::AppendSignature(message);
    }

    bool ThreeFBFTWalletImpl::VerifySignature(const itcoin::pbft::messages::Message &message) const {
      return BitcoinRpcWallet::VerifySignature(message);
    }

    CBlock ThreeFBFTWalletImpl::FinalizeBlock(const CBlock &, const std::string, const std::vector<std::string>) const {
      throw std::runtime_error("not implemented");
    }

    std::string ThreeFBFTWalletImpl::GetPreSignatureShare() { throw std::runtime_error("not implemented"); }

    std::string ThreeFBFTWalletImpl::GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature,
                                                       const CBlock &block) {
      throw std::runtime_error("not implemented");
    }

    ThreeFBFTWalletImpl::~ThreeFBFTWalletImpl() {}
}
