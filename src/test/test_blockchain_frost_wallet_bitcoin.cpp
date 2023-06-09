// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <thread>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "fixtures/fixtures.h"

namespace utf = boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_blockchain_frost_wallet_bitcoin, *utf::enabled())

using namespace itcoin::blockchain;
using namespace itcoin::fbft::messages;

struct Replica4Fixture: BitcoinInfraFixture { Replica4Fixture(): BitcoinInfraFixture() {} };

BOOST_FIXTURE_TEST_CASE(test_blockchain_frost_wallet_bitcoin_00, Replica4Fixture)
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::trace
  );

  std::vector<std::unique_ptr<itcoin::wallet::RoastWalletImpl>> frost_wallets;
  for (int i = 0; i < CLUSTER_SIZE; i++)
  {
    auto wallet = std::make_unique<itcoin::wallet::RoastWalletImpl>(*m_configs[i], *m_bitcoinds[i]);
    frost_wallets.emplace_back(move(wallet));
  }

  // GetPreSignatureShare()
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing presignature generation";
    std::string preSignature = frost_wallets[0]->GetPreSignatureShare();
    BOOST_TEST( preSignature.empty() == false );
  }
 {
    BOOST_LOG_TRIVIAL(debug) << "Testing subsequent presignatures to be different";
    std::string preSignature1 = frost_wallets[0]->GetPreSignatureShare();
    std::string preSignature2 = frost_wallets[0]->GetPreSignatureShare();
    BOOST_TEST( preSignature1 != preSignature2 );
  }
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing presignature serialization format";
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      std::string preSignature = frost_wallets[i]->GetPreSignatureShare();
      BOOST_TEST( preSignature.empty() == false );
      BOOST_LOG_TRIVIAL(debug) << "Deserializing commitments: " << preSignature;
      BOOST_TEST(preSignature.find(std::to_string(i + 1) + "::") != std::string::npos);
    }
  }

  // GetSignatureShare(signers, pre_signature, block)
  // GetSignatureShare: try to sign without generating nonce
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing GetSignatureShare without nonce, should throw an error";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "0::a::b+1::c::d";
    CBlock block;
    auto wallet = itcoin::wallet::RoastWalletImpl(*m_configs[0], *m_bitcoinds[0]);
    BOOST_CHECK_THROW(wallet.GetSignatureShare(signers, pre_signatures, block), std::runtime_error);
  }

  // GetSignatureShare: try to sign with empty presignatures
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing GetSignatureShare without presignatures, should throw an error";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block;
    BOOST_CHECK_THROW(frost_wallets[0]->GetSignatureShare(signers, pre_signatures, block), std::runtime_error);
  }
  // GetSignatureShare: try to sign with less presignatures that needed
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing GetSignatureShare: generate signature share with below threshold presignatures (expect no error)";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block;

    // Add only a single commitment
    pre_signatures.append(frost_wallets[0]->GetPreSignatureShare());
    // GetSignatureShare
    std::string nodeSignature = frost_wallets[0]->GetSignatureShare(signers, pre_signatures, block);
    BOOST_TEST(nodeSignature.empty() == false);
    BOOST_LOG_TRIVIAL(debug) << "Node #" << 0 << " signature: " << nodeSignature;
  }
  // GetSignatureShare: check that each participant generates its signature_response
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing GetSignatureShare: each participant should generate its signature_response";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block;

    // Concatenate presignatures as done in prolog
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      if (i > 0) pre_signatures.append("+");
      pre_signatures.append(frost_wallets[i]->GetPreSignatureShare());
    }
    // GetSignatureShare
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      // FIXME: why do we need the set of signers? pre_signature already includes it.

      std::string nodeSignature = frost_wallets[i]->GetSignatureShare(signers, pre_signatures, block);
      BOOST_TEST(nodeSignature.empty() == false);
      BOOST_LOG_TRIVIAL(debug) << "Node #" << i << " signature: " << nodeSignature;
      BOOST_TEST(nodeSignature.find(std::to_string(i + 1) + "::") != std::string::npos);
    }
  }

  // FinalizeBlock: aggregate signatures and update the blockchain
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures and update the block";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block = m_blockchains.at(0)->GenerateBlock(get_present_block_time());

    // Concatenate PreSignatures as done in prolog
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      if (i > 0) pre_signatures.append("+");
      pre_signatures.append(frost_wallets[i]->GetPreSignatureShare());
    }
    // GetSignatureShare
    std::vector<std::string> signature_shares;
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      std::string nodeSignature = frost_wallets[i]->GetSignatureShare(signers, pre_signatures, block);
      BOOST_TEST(nodeSignature.empty() == false);
      signature_shares.push_back(nodeSignature);
    }

    CBlock final_block = frost_wallets[0]->FinalizeBlock(block, pre_signatures, signature_shares);

    auto info_0 = m_bitcoinds.at(0)->getblockchaininfo();
    auto height_0 = info_0["blocks"].asUInt();

    m_blockchains.at(0)->SubmitBlock(0, final_block);

    auto info_1 = m_bitcoinds.at(0)->getblockchaininfo();
    auto height_1 = info_1["blocks"].asUInt();

    BOOST_TEST(height_1 == height_0 + 1);
  }

    // FinalizeBlock: primary aggregates signatures of other participants
    { // Assumption: we generated keys to allow a 2-4 threshold signature. Two participants can generate a valid signature.
      BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures from other nodes but itself";
      std::vector<uint32_t> signers;
      std::string pre_signatures = "";
      CBlock block = m_blockchains.at(0)->GenerateBlock(get_present_block_time());

      // Concatenate PreSignatures as done in prolog
      for (int i = 0; i < CLUSTER_SIZE; i++)
      {
        auto presignature = frost_wallets[i]->GetPreSignatureShare();
        if (i == 1)
          pre_signatures.append(presignature);
        if (i == 2)
        {
          pre_signatures.append("+");
          pre_signatures.append(presignature);
        }
      }
      // GetSignatureShare: only 1 node signs
      std::vector<std::string> signature_shares;
      std::string nodeSignature1 = frost_wallets[1]->GetSignatureShare(signers, pre_signatures, block);
      signature_shares.push_back(nodeSignature1);
      std::string nodeSignature2 = frost_wallets[2]->GetSignatureShare(signers, pre_signatures, block);
      signature_shares.push_back(nodeSignature2);

      CBlock final_block = frost_wallets[0]->FinalizeBlock(block, pre_signatures, signature_shares);

      auto info_0 = m_bitcoinds.at(0)->getblockchaininfo();
      auto height_0 = info_0["blocks"].asUInt();

      m_blockchains.at(0)->SubmitBlock(0, final_block);

      auto info_1 = m_bitcoinds.at(0)->getblockchaininfo();
      auto height_1 = info_1["blocks"].asUInt();

      BOOST_TEST(height_1 == height_0 + 1);
    }

    // FinalizeBlock: out of order presignatures
    { // Assumption: we generated keys to allow a 2-4 threshold signature. Two participants can generate a valid signature.
      BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures from other nodes but itself";
      std::vector<uint32_t> signers;
      std::string pre_signatures = "";
      CBlock block = m_blockchains.at(0)->GenerateBlock(get_present_block_time());

      // Concatenate PreSignatures as done in prolog
      for (int i = 0; i < CLUSTER_SIZE; i++)
      {
        auto presignature = frost_wallets[i]->GetPreSignatureShare();
        if (i == 1)
        {
          pre_signatures.append("+");
          pre_signatures.append(presignature);
        }
        if (i == 2)
        {
          pre_signatures = presignature + pre_signatures;
        }
      }
      // GetSignatureShare: only 1 node signs
      std::vector<std::string> signature_shares;
      std::string nodeSignature1 = frost_wallets[1]->GetSignatureShare(signers, pre_signatures, block);
      signature_shares.push_back(nodeSignature1);
      std::string nodeSignature2 = frost_wallets[2]->GetSignatureShare(signers, pre_signatures, block);
      signature_shares.push_back(nodeSignature2);

      CBlock final_block = frost_wallets[0]->FinalizeBlock(block, pre_signatures, signature_shares);

      auto info_0 = m_bitcoinds.at(0)->getblockchaininfo();
      auto height_0 = info_0["blocks"].asUInt();

      m_blockchains.at(0)->SubmitBlock(0, final_block);

      auto info_1 = m_bitcoinds.at(0)->getblockchaininfo();
      auto height_1 = info_1["blocks"].asUInt();

      BOOST_TEST(height_1 == height_0 + 1);
    }

    // FinalizeBlock: primary aggregates signatures of other participants
    { // Assumption: we generated keys to allow a 2-4 threshold signature. Two participants can generate a valid signature.
      BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures from other nodes but itself";
      std::vector<uint32_t> signers;
      std::string pre_signatures = "";
      CBlock block = m_blockchains.at(0)->GenerateBlock(get_present_block_time());

      // Concatenate PreSignatures as done in prolog
      for (int i = 0; i < CLUSTER_SIZE; i++)
      {
        auto presignature = frost_wallets[i]->GetPreSignatureShare();
        if (i == 1)
          pre_signatures.append(presignature);
      }
      // GetSignatureShare: only 1 node signs
      std::vector<std::string> signature_shares;
      std::string nodeSignature1 = frost_wallets[1]->GetSignatureShare(signers, pre_signatures, block);
      signature_shares.push_back(nodeSignature1);

      BOOST_CHECK_THROW(frost_wallets[0]->FinalizeBlock(block, pre_signatures, signature_shares), std::runtime_error);
    }

    // AppendSignature and VerifySignature
    {
      // Message signature
      BOOST_LOG_TRIVIAL(debug) << "Testing AppendSignature to sign messages";
      uint32_t msg_sender_id = 0;
      Prepare msg{msg_sender_id, 0, 0, "req_digest"};
      BOOST_CHECK_NO_THROW(frost_wallets[0]->AppendSignature(msg));

      BOOST_LOG_TRIVIAL(debug) << "Testing VerifySignature to verify messages";
      bool is_sig_valid = frost_wallets[1]->VerifySignature(msg);
      BOOST_TEST( is_sig_valid == true );
    }

} // test_blockchain_frost_wallet_bitcoin_00

BOOST_AUTO_TEST_SUITE_END() // test_blockchain_frost_wallet_bitcoin
