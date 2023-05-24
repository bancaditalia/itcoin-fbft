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
#include "../frost/frost_helpers.hpp"

#define DELIM_PRESIG "+"

using Message = itcoin::pbft::messages::Message;

namespace itcoin::wallet {

    RoastWalletImpl::RoastWalletImpl(const itcoin::PbftConfig &conf, transport::BtcClient &bitcoind)
        : Wallet(conf), BitcoinRpcWallet(conf, bitcoind), RoastWallet(conf),
          m_keypair(InitializeKeyPair(conf, bitcoind)) {
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% RoastWalletImpl will sign using pubkey address %2%.") % m_conf.id() % m_pubkey_address);

      m_valid_nonce = false;
      m_nonce = {};

      // Declare the dynamic wallets predicates
      PlCall("assertz", PlTermv(PlCompound(
          "(roast_crypto_pre_sig_aggregate(Replica_id, Pre_signature_shares, Pre_signature) :- roast_crypto_pre_sig_aggregate_impl(Replica_id, Pre_signature_shares, Pre_signature))")));
    }

    keypair RoastWalletImpl::InitializeKeyPair(const itcoin::PbftConfig &conf, transport::BtcClient &bitcoind) const {
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

    std::string RoastWalletImpl::GetPreSignatureShare() {
      BOOST_LOG_TRIVIAL(debug) << "Generating nonces and returning commitments";

      nonce_pair np = create_nonce();
      if (m_valid_nonce) {
        // TODO: should we throw an exception if m_nonces exists already?
        // Can multiple signing request happen at the same time?
        BOOST_LOG_TRIVIAL(warning) << "Replacing a valid nonce ";
      }
      m_nonce = np;
      m_valid_nonce = true;

      signing_commitment sc;
      sc.index = m_conf.id() + 1;
      sc.hiding_commitment = np.hiding_nonce.commitment;
      sc.binding_commitment = np.binding_nonce.commitment;

      std::string serialized_commitments = serialize_signing_commitment(sc);

      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Generated presignature: %2%.") % m_conf.id() % serialized_commitments);

      return serialized_commitments;
    }

    std::vector<std::string> RoastWalletImpl::SplitPreSignatures(std::string serializedList) const {
      std::string delimiter{DELIM_PRESIG};
      uint32_t start = 0;
      uint32_t end = 0;
      std::vector<std::string> items;
      while (end < serializedList.size()) {
        end = serializedList.find(delimiter, start);
        std::string item = serializedList.substr(start, end - start);
        items.push_back(item);
        start = end + delimiter.length();
      }
      return items;
    }

    // This is SignRound
    std::string
    RoastWalletImpl::GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signatures, const CBlock &block) {
      BOOST_LOG_TRIVIAL(debug) << "Computing signature shares";

      // Checking parameters and internal state
      if (pre_signatures.empty()) {
        std::string errorMsg = boost::str(boost::format("R%1% received an empty set of presignatures.") % m_conf.id());
        BOOST_LOG_TRIVIAL(error) << errorMsg;
        throw std::runtime_error(errorMsg);
      }
      if (!m_valid_nonce) {
        std::string errorMsg = boost::str(
            boost::format("R%1% is going to sing with an already used signing nonce.") % m_conf.id());
        BOOST_LOG_TRIVIAL(error) << errorMsg;
        throw std::runtime_error(errorMsg);
      }

      // Step 1: retrieve: keypair, nonces, commitments;
      std::vector<signing_commitment> signing_commitments;
      std::vector<nonce_pair> my_signing_nonces;
      std::vector<std::string> raw_presignatures = this->SplitPreSignatures(pre_signatures);

      for (auto &raw_presignature: raw_presignatures) {
        signing_commitment signing_commitment = deserialize_signing_commitment(raw_presignature);
        signing_commitments.push_back(signing_commitment);
      }

      // FIXME:
      my_signing_nonces.push_back(m_nonce);

      // Step 2: Get Signature Hash
      CBlock _block(block);
      auto [spendTx, toSpendTx] = itcoin::block::signetTxs(_block, m_conf.getSignetChallenge());
      uint256 hash_out = TaprootSignatureHash(spendTx, toSpendTx);

      // Step 3: Sign
      std::string block_as_string = hash_out.GetHex();
      BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% block hash: %2%") % m_conf.id() % block_as_string);

      signing_response res = sign(m_keypair,
                                  signing_commitments, my_signing_nonces,
                                  hash_out.begin(), 32);

      // Step 4: invalidate nonce
      m_valid_nonce = false;

      // Step 5: serialize signature
      std::string serialized_res = serialize_signing_response(res);

      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Generated signature share: %2%.") % m_conf.id() % serialized_res);

      return serialized_res;
    }

    signature RoastWalletImpl::AggregateSignatureShares(const unsigned char *message,
                                                        const size_t message_size,
                                                        const std::string &pre_signatures,
                                                        const std::vector<std::string> &signature_shares) const {
      BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% is aggregating signature shares") % m_conf.id());

      // Step 1: Retrieve signature shares (signing_response)
      std::vector<signing_response> all_responses;
      for (auto &signature: signature_shares) {
        signing_response response = deserialize_signing_response(signature);
        all_responses.push_back(response);
      }

      // Step 2: Retrieve signing commitments
      std::vector<signing_commitment> signing_commitments;
      std::vector<std::string> serializedPreSignatures = this->SplitPreSignatures(pre_signatures);
      for (auto &serializedPreSignature: serializedPreSignatures) {
        signing_commitment signing_commitment = deserialize_signing_commitment(serializedPreSignature);
        signing_commitments.push_back(signing_commitment);
      }

      // Step 3: Retrieve public keys of other participants
      std::vector<participant_pubkeys> participant_pubkeys_vec;
      for (auto &rep_conf: m_conf.replica_set_v()) {
        participant_pubkeys pubkey;
        pubkey.index = rep_conf.id() + 1;
        pubkey.public_key = deserialize_public_key(rep_conf.pubkey());
        pubkey.group_public_key = m_keypair.group_public_key;
        participant_pubkeys_vec.push_back(pubkey);
      }
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% has correctly retrieved signature shares, presignatures, and participant public keys") %
            m_conf.id());

      try {
        // Step 4: Aggregate signature shares
        signature signature = aggregate_helper(message, message_size,
                                               signing_commitments,
                                               all_responses,
                                               participant_pubkeys_vec);
        BOOST_LOG_TRIVIAL(debug) << boost::str(
              boost::format("R%1% has correctly aggregating the signature and is going to validate it") % m_conf.id());

        // Additional Step 5: Validate aggregated signature locally (not needed)
        validate(message, 32, signature, m_keypair.group_public_key);
        BOOST_LOG_TRIVIAL(debug)
          << boost::str(boost::format("R%1% has correctly validated the aggregated signature") % m_conf.id());

        return signature;
      } catch (std::runtime_error const& e) {
        BOOST_LOG_TRIVIAL(error)
          << boost::str(boost::format("R%1% Error while aggregating signature: %2%") % m_conf.id() % e.what());
        throw e;
      }
    }

    // SignAgg
    CBlock RoastWalletImpl::FinalizeBlock(const CBlock &block,
                                          const std::string pre_signatures,
                                          const std::vector<std::string> signature_shares) const {
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Finalizing block with presignatures: %2%.") % m_conf.id() % pre_signatures);

      // Step 1: Create digest signed by participants
      CBlock _block(block);
      auto [spendTx, toSpendTx] = itcoin::block::signetTxs(_block, m_conf.getSignetChallenge());
      uint256 hash_out = TaprootSignatureHash(spendTx, toSpendTx);

      // Step 2: Aggregate signature shares
      std::string block_as_string = hash_out.GetHex();
      BOOST_LOG_TRIVIAL(debug) << boost::str(
            boost::format("R%1% Aggregating signature shares on block: %2%.") % m_conf.id() % block_as_string);
      signature signature = this->AggregateSignatureShares(hash_out.begin(), 32, pre_signatures, signature_shares);

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

    uint256 RoastWalletImpl::TaprootSignatureHash(const CMutableTransaction &spendTx,
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

    void RoastWalletImpl::AppendSignature(itcoin::pbft::messages::Message &message) const {
      BitcoinRpcWallet::AppendSignature(message);
    }

    bool RoastWalletImpl::VerifySignature(const itcoin::pbft::messages::Message &message) const {
      return BitcoinRpcWallet::VerifySignature(message);
    }

    CBlock RoastWalletImpl::FinalizeBlock(const CBlock &block, const std::vector<std::string> signatures) const {
      return RoastWallet::FinalizeBlock(block, signatures);
    }

    std::string RoastWalletImpl::GetBlockSignature(const CBlock &block) {
      return RoastWallet::GetBlockSignature(block);
    }

    RoastWalletImpl::~RoastWalletImpl() {}
}

// This is PreAggr
PREDICATE(roast_crypto_pre_sig_aggregate_impl, 3) {
  BOOST_LOG_TRIVIAL(debug) << "I am in the prolog predicate roast_crypto_pre_sig_aggregate";
  std::string a3_result = "";
  std::string delimiter{DELIM_PRESIG};
  PlTail Presig_shares{PL_A2};
  PlTerm Presig_elem;
  while (Presig_shares.next(Presig_elem)) {
    std::string presig = std::string{(const char *) Presig_elem};
    a3_result += presig;
    a3_result += delimiter;
  }
  a3_result.pop_back();
  PlTerm Pre_signature = PL_A3;
  PL_A3 = PlString((const char *) a3_result.c_str());
  return true;
}