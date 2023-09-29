// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "wallet.h"

#include <base58.h>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <script/interpreter.h>

#include "../blockchain/extract.h"
#include "../fbft/messages/messages.h"
#include "../transport/btcclient.h"
#include "config/FbftConfig.h"

#include <cstring>
#include <secp256k1/include/secp256k1.h>
#include <secp256k1/include/secp256k1_frost.h>
#include <sys/random.h>

#define DELIM_PRESIG "+"

using Message = itcoin::fbft::messages::Message;

namespace itcoin::wallet {
static std::string const DELIM_SIG = "::";
static std::string const DELIM_COMMITMENTS = "::";

RoastWalletImpl::RoastWalletImpl(const itcoin::FbftConfig& conf, transport::BtcClient& bitcoind)
    : Wallet(conf), BitcoinRpcWallet(conf, bitcoind), RoastWallet(conf),
      m_keypair(InitializeKeyPair(conf, bitcoind)) {
  BOOST_LOG_TRIVIAL(debug) << boost::str(
      boost::format("R%1% RoastWalletImpl will sign using pubkey address %2%.") % m_conf.id() %
      m_pubkey_address);

  m_frost_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
  m_valid_nonce = false;
  m_nonce = NULL;

  // Declare the dynamic wallets predicates
  PlCall("assertz",
         PlTermv(PlCompound(
             "(roast_crypto_pre_sig_aggregate(Replica_id, Pre_signature_shares, Pre_signature) :- "
             "roast_crypto_pre_sig_aggregate_impl(Replica_id, Pre_signature_shares, Pre_signature))")));
}

secp256k1_frost_keypair* RoastWalletImpl::InitializeKeyPair(const itcoin::FbftConfig& conf,
                                                            transport::BtcClient& bitcoind) const {
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

  secp256k1_frost_keypair* kp;
  kp = (secp256k1_frost_keypair*)malloc(sizeof(secp256k1_frost_keypair));
  memcpy(kp->secret, raw_privkey.data(), 32);
  BOOST_LOG_TRIVIAL(debug) << "Private key correctly parsed!";

  {
    unsigned char raw_pubkey[33] = {0};
    unsigned char raw_group_pubkey[33] = {0};

    this->DeserializePublicKey(m_conf.replica_set_v().at(m_conf.id()).pubkey(), raw_pubkey);
    this->DeserializePublicKey(m_conf.group_public_key(), raw_group_pubkey);

    if (secp256k1_frost_pubkey_load(&kp->public_keys, m_conf.id() + 1, m_conf.cluster_size(), raw_pubkey,
                                    raw_group_pubkey) == 0) {
      std::string errorMsg = boost::str(boost::format("R%1% error while loading pubkey.") % m_conf.id());
      BOOST_LOG_TRIVIAL(error) << errorMsg;
      throw std::runtime_error(errorMsg);
    }
  }

  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% has correctly initialized its keypair.") %
                                         m_conf.id());

  return kp;
}

void RoastWalletImpl::DeserializePublicKey(std::string serialized_public_key, unsigned char* output33) const {
  if (serialized_public_key.empty()) {
    throw std::runtime_error("Unable to deserialize an empty public key.");
  }
  // If the public key is represented by 32bytes, then it implicitly encodes a positive Y-coord
  if (serialized_public_key.length() == 2 * (33 - 1)) {
    serialized_public_key = "02" + serialized_public_key;
  }
  this->HexToCharArray(serialized_public_key, output33);
}

void RoastWalletImpl::HexToCharArray(const std::string str, unsigned char* retval) const {
  int d;
  for (uint32_t i = 0, p = 0; p < str.length(); p = p + 2, i++) {
    sscanf(&str[p], "%02x", &d);
    retval[i] = (unsigned char)d;
  }
}

std::string RoastWalletImpl::CharArrayToHex(const unsigned char* bytearray, size_t size) const {
  int i = 0;
  char buffer[2 * size];
  for (uint32_t idx = 0; idx < size; idx++) {
    sprintf(buffer + i, "%02x", bytearray[idx]);
    i += 2;
  }
  std::string _str(buffer);
  return _str;
}

void RoastWalletImpl::FillRandom(unsigned char* data, size_t size) {
  // simplified from
  // https://github.com/bitcoin/bitcoin/blob/747cdf1d652d8587e9f2e3d4436c3ecdbf56d0a5/src/secp256k1/examples/random.h
  /* If `getrandom(2)` is not available you should fall back to /dev/urandom */
  ssize_t res = getrandom(data, size, 0);
  if (res < 0 || (size_t)res != size) {
    throw std::runtime_error("Failed to generate seed");
  }
}

std::string
RoastWalletImpl::SerializeSigningCommitment(const secp256k1_frost_nonce_commitment* commitments) const {
  // Serialize to string format: index::binding_commitment::hiding_commitment
  return std::to_string(commitments->index) + DELIM_COMMITMENTS +
         this->CharArrayToHex(commitments->binding, 64) + DELIM_COMMITMENTS +
         this->CharArrayToHex(commitments->hiding, 64);
}
secp256k1_frost_nonce_commitment
RoastWalletImpl::DeserializeSigningCommitment(const std::string& serialized) const {
  // Expected string: index::binding_commitment::hiding_commitment
  auto end = serialized.find(DELIM_COMMITMENTS);
  std::string raw_index = serialized.substr(0, end);

  auto start = end + DELIM_COMMITMENTS.length();
  end = serialized.find(DELIM_COMMITMENTS, start);
  std::string raw_binding_commitment = serialized.substr(start, end);

  start = end + DELIM_COMMITMENTS.length();
  std::string raw_hiding_commitment = serialized.substr(start, serialized.length());

  // Parse raw elements for creating signing_commitment
  secp256k1_frost_nonce_commitment sc = {};
  sc.index = static_cast<uint32_t>(std::stoul(raw_index));
  this->HexToCharArray(raw_binding_commitment, sc.binding);
  this->HexToCharArray(raw_hiding_commitment, sc.hiding);

  return sc;
}

std::string RoastWalletImpl::GetPreSignatureShare() {
  BOOST_LOG_TRIVIAL(debug) << "Generating nonces and returning commitments";

  unsigned char binding_seed[32] = {0};
  unsigned char hiding_seed[32] = {0};
  this->FillRandom(binding_seed, 32);
  this->FillRandom(hiding_seed, 32);

  if (m_valid_nonce) {
    // TODO: should we throw an exception if m_nonces exists already?
    // Can multiple signing request happen at the same time?
    BOOST_LOG_TRIVIAL(warning) << "Replacing a valid nonce ";
  }
  if (m_nonce != NULL) {
    secp256k1_frost_nonce_destroy(m_nonce);
  }
  m_nonce = secp256k1_frost_nonce_create(m_frost_ctx, m_keypair, binding_seed, hiding_seed);
  m_valid_nonce = true;

  std::string serialized_commitments = this->SerializeSigningCommitment(&m_nonce->commitments);

  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Generated presignature: %2%.") % m_conf.id() %
                                         serialized_commitments);

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
std::string RoastWalletImpl::GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signatures,
                                               const CBlock& block) {
  BOOST_LOG_TRIVIAL(debug) << "Computing signature shares";

  // Checking parameters and internal state
  if (pre_signatures.empty()) {
    std::string errorMsg =
        boost::str(boost::format("R%1% received an empty set of presignatures.") % m_conf.id());
    BOOST_LOG_TRIVIAL(error) << errorMsg;
    throw std::runtime_error(errorMsg);
  }
  if (!m_valid_nonce) {
    std::string errorMsg =
        boost::str(boost::format("R%1% is going to sing with an already used signing nonce.") % m_conf.id());
    BOOST_LOG_TRIVIAL(error) << errorMsg;
    throw std::runtime_error(errorMsg);
  }

  // Step 1: retrieve: keypair, nonce, commitments;
  std::vector<secp256k1_frost_nonce_commitment> signing_commitments;
  std::vector<std::string> raw_presignatures = this->SplitPreSignatures(pre_signatures);

  for (auto& raw_presignature : raw_presignatures) {
    secp256k1_frost_nonce_commitment cmt = this->DeserializeSigningCommitment(raw_presignature);
    signing_commitments.push_back(cmt);
  }

  // Step 2: Get Signature Hash
  CBlock _block(block);
  auto [spendTx, toSpendTx] = itcoin::blockchain::signetTxs(_block, m_conf.getSignetChallenge());
  uint256 hash_out = this->TaprootSignatureHash(spendTx, toSpendTx);

  // Step 3: Sign
  std::string block_as_string = hash_out.GetHex();
  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% block hash: %2%") % m_conf.id() %
                                         block_as_string);

  secp256k1_frost_signature_share signature_share;
  if (secp256k1_frost_sign(&signature_share, hash_out.begin(), signing_commitments.size(), m_keypair, m_nonce,
                           signing_commitments.data()) == 0) {
    std::string errorMsg = boost::str(boost::format("R%1% error while signing message.") % m_conf.id());
    BOOST_LOG_TRIVIAL(error) << errorMsg;
    throw std::runtime_error(errorMsg);
  }

  // Step 4: invalidate nonce
  m_valid_nonce = false;

  // Step 5: serialize signature
  std::string serialized_res = this->SerializeSignatureShare(&signature_share);

  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Generated signature share: %2%.") % m_conf.id() %
                                         serialized_res);

  return serialized_res;
}

std::string RoastWalletImpl::SerializeSignatureShare(const secp256k1_frost_signature_share* signature) const {
  // Serialize to string format: index::signature
  unsigned char response_bytes[32];
  return std::to_string(signature->index) + DELIM_SIG + this->CharArrayToHex(signature->response, 32);
}

secp256k1_frost_signature_share
RoastWalletImpl::DeserializeSignatureShare(const std::string& serialized) const {
  secp256k1_frost_signature_share res;

  // Expected: index::signature
  auto end = serialized.find(DELIM_SIG);
  std::string raw_participant_index = serialized.substr(0, end);
  res.index = static_cast<uint32_t>(std::stoul(raw_participant_index));

  auto start = end + DELIM_SIG.length();
  std::string raw_response = serialized.substr(start, serialized.length());

  this->HexToCharArray(raw_response, res.response);
  return res;
}

void RoastWalletImpl::AggregateSignatureShares(unsigned char* signature64, const unsigned char* message32,
                                               const std::string& pre_signatures,
                                               const std::vector<std::string>& signature_shares) const {
  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% is aggregating signature shares") % m_conf.id());

  // Step 1: Retrieve signature shares (signing_response)
  std::vector<uint32_t> signer_indexes;
  std::vector<secp256k1_frost_signature_share> all_responses;
  for (auto& signature : signature_shares) {
    secp256k1_frost_signature_share response = this->DeserializeSignatureShare(signature);
    signer_indexes.push_back(response.index);
    all_responses.push_back(response);
  }

  // Step 2: Retrieve signing commitments
  std::vector<secp256k1_frost_nonce_commitment> signing_commitments;
  std::vector<std::string> serializedPreSignatures = this->SplitPreSignatures(pre_signatures);
  for (auto& serializedPreSignature : serializedPreSignatures) {
    secp256k1_frost_nonce_commitment commitment = this->DeserializeSigningCommitment(serializedPreSignature);
    signing_commitments.push_back(commitment);
  }

  // Step 3: Retrieve public keys of other participants
  std::vector<secp256k1_frost_pubkey> participant_pubkeys_vec;
  unsigned char raw_group_pubkey[33];
  this->DeserializePublicKey(m_conf.group_public_key(), raw_group_pubkey);
  for (auto& rep_conf : m_conf.replica_set_v()) {
    bool found = false;
    unsigned int replica_index = rep_conf.id() + 1;
    for (auto& si : signer_indexes) {
      if (si == replica_index) {
        found = true;
        break;
      }
    }
    if (!found) {
      continue;
    }
    unsigned char raw_pubkey[33] = {0};
    secp256k1_frost_pubkey pubkey;
    this->DeserializePublicKey(rep_conf.pubkey(), raw_pubkey);
    if (secp256k1_frost_pubkey_load(&pubkey, replica_index, m_conf.cluster_size(), raw_pubkey,
                                    raw_group_pubkey) == 0) {
      std::string errorMsg = boost::str(boost::format("R%1% error while loading pubkey of replica R%2%.") %
                                        m_conf.id() % replica_index);
      BOOST_LOG_TRIVIAL(error) << errorMsg;
      throw std::runtime_error(errorMsg);
    }
    participant_pubkeys_vec.push_back(pubkey);
  }
  BOOST_LOG_TRIVIAL(debug) << boost::str(
      boost::format(
          "R%1% has correctly retrieved signature shares, presignatures, and participant public keys") %
      m_conf.id());

  // Step 4: Aggregate signature shares
  if (secp256k1_frost_aggregate(m_frost_ctx, signature64, message32, m_keypair,
                                participant_pubkeys_vec.data(), signing_commitments.data(),
                                all_responses.data(), all_responses.size()) == 0) {
    std::string errorMsg = boost::str(boost::format("R%1% error aggregating signature.") % m_conf.id());
    BOOST_LOG_TRIVIAL(error) << errorMsg;
    throw std::runtime_error(errorMsg);
  }
  BOOST_LOG_TRIVIAL(debug) << boost::str(
      boost::format("R%1% has correctly aggregating the signature and is going to validate it") %
      m_conf.id());

  // Additional Step 5: Validate aggregated signature locally (not needed)
  if (secp256k1_frost_verify(m_frost_ctx, signature64, message32, &m_keypair->public_keys) == 0) {
    std::string errorMsg =
        boost::str(boost::format("R%1% error while validating aggregated signature.") % m_conf.id());
    BOOST_LOG_TRIVIAL(error) << errorMsg;
    throw std::runtime_error(errorMsg);
  }

  BOOST_LOG_TRIVIAL(debug) << boost::str(
      boost::format("R%1% has correctly validated the aggregated signature") % m_conf.id());
}

// SignAgg
CBlock RoastWalletImpl::FinalizeBlock(const CBlock& block, const std::string pre_signatures,
                                      const std::vector<std::string> signature_shares) const {
  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Finalizing block with presignatures: %2%.") %
                                         m_conf.id() % pre_signatures);

  // Step 1: Create digest signed by participants
  CBlock _block(block);
  auto [spendTx, toSpendTx] = itcoin::blockchain::signetTxs(_block, m_conf.getSignetChallenge());
  uint256 hash_out = TaprootSignatureHash(spendTx, toSpendTx);

  // Step 2: Aggregate signature shares
  std::string block_as_string = hash_out.GetHex();
  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Aggregating signature shares on block: %2%.") %
                                         m_conf.id() % block_as_string);
  unsigned char signature64[64];
  this->AggregateSignatureShares(signature64, hash_out.begin(), pre_signatures, signature_shares);
  std::string serialized_signature = this->CharArrayToHex(signature64, 64);

  // Step 3: Derive the signet solution
  unsigned char serialized_signature_buf[3 + 32 + 32];
  serialized_signature_buf[0] = OP_0;
  serialized_signature_buf[1] = 0x01; // Serialize the number of elements in the vector (1 item)
  serialized_signature_buf[2] = 0x40; // Serialize the item[0] length (64 bytes)
  memcpy(&(serialized_signature_buf[3]), signature64, 64);

  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Adding signature %2% to block: %3%.") %
                                         m_conf.id() % serialized_signature % block_as_string);

  // Step 4: Add signet solution to spent transaction
  spendTx.vin[0].scriptSig = CScript(&(serialized_signature_buf[3]), &(serialized_signature_buf[3]) + 64);

  // Step 5: Append the signet solution
  std::vector<unsigned char> serialized_signatureBytes{serialized_signature_buf,
                                                       serialized_signature_buf + 3 + 64};
  itcoin::blockchain::appendSignetSolution(&_block, serialized_signatureBytes);

  BOOST_LOG_TRIVIAL(debug) << boost::str(boost::format("R%1% Block finalized.") % m_conf.id());
  return _block;
}

uint256 RoastWalletImpl::TaprootSignatureHash(const CMutableTransaction& spendTx,
                                              const CMutableTransaction& toSpendTx) const {
  PrecomputedTransactionData cache;
  cache.Init(spendTx, {toSpendTx.vout[0]}, true);
  ScriptExecutionData execdata;
  execdata.m_annex_init = true;
  execdata.m_annex_present = false;
  uint256 hash_out;
  bool computed = SignatureHashSchnorr(hash_out, execdata, spendTx, 0, SIGHASH_DEFAULT, SigVersion::TAPROOT,
                                       cache, MissingDataBehavior::FAIL);
  if (!computed) {
    std::string error_msg =
        boost::str(boost::format("R%1% Cannot compute SignatureHashSchnorr!") % m_conf.id());
    BOOST_LOG_TRIVIAL(error) << error_msg;
    throw std::runtime_error(error_msg);
  }
  return hash_out;
}

RoastWalletImpl::~RoastWalletImpl() {
  if (m_nonce != NULL) {
    secp256k1_frost_nonce_destroy(m_nonce);
  }
  if (m_frost_ctx != NULL) {
    secp256k1_context_destroy(m_frost_ctx);
  }
}
} // namespace itcoin::wallet

// This is PreAggr
PREDICATE(roast_crypto_pre_sig_aggregate_impl, 3) {
  BOOST_LOG_TRIVIAL(debug) << "I am in the prolog predicate roast_crypto_pre_sig_aggregate";
  std::string a3_result = "";
  std::string delimiter{DELIM_PRESIG};
  PlTail Presig_shares{PL_A2};
  PlTerm Presig_elem;
  while (Presig_shares.next(Presig_elem)) {
    std::string presig = std::string{(const char*)Presig_elem};
    a3_result += presig;
    a3_result += delimiter;
  }
  a3_result.pop_back();
  PlTerm Pre_signature = PL_A3;
  PL_A3 = PlString((const char*)a3_result.c_str());
  return true;
}