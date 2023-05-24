#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <thread>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "fixtures.h"

namespace utf = boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_blockchain_threefbft_wallet_bitcoin, *utf::enabled())

using namespace itcoin::blockchain;
using namespace itcoin::pbft::messages;

struct ReplicaFBFT4Fixture: ThreeFBFTWalletTestFixture { ReplicaFBFT4Fixture(): ThreeFBFTWalletTestFixture(4,0,60) {} };

BOOST_FIXTURE_TEST_CASE(test_blockchain_threefbft_wallet_bitcoin_00, ReplicaFBFT4Fixture)
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::trace
  );

  // GetBlockSignature()
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing GetBlockSignature";
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      CBlock block;
      std::string preSignature = m_wallets[i]->GetBlockSignature(block);
      BOOST_TEST( preSignature.empty() == false );
    }
  }

  // FinalizeBlock: aggregate signatures and update the blockchain
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures and update the block";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block = m_blockchains.at(0)->GenerateBlock(get_next_block_time());

    // GetSignatureShare
    std::vector<std::string> signature_shares;
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
        std::string nodeSignature = m_wallets[i]->GetBlockSignature(block);
        BOOST_TEST(nodeSignature.empty() == false);
        signature_shares.push_back(nodeSignature);
    }

    CBlock final_block = m_wallets[0]->FinalizeBlock(block, signature_shares);

    auto info_0 = m_bitcoinds.at(0)->getblockchaininfo();
    auto height_0 = info_0["blocks"].asUInt();

    m_blockchains.at(0)->SubmitBlock(0, final_block);

    auto info_1 = m_bitcoinds.at(0)->getblockchaininfo();
    auto height_1 = info_1["blocks"].asUInt();

    BOOST_TEST(height_1 == height_0 + 1);
  }

  // FinalizeBlock: throw error if current participant is not included
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures and update the block";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block = m_blockchains.at(0)->GenerateBlock(get_next_block_time());

    // GetSignatureShare
    std::vector<std::string> signature_shares;
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
        std::string nodeSignature = m_wallets[i]->GetBlockSignature(block);
        BOOST_TEST(nodeSignature.empty() == false);
        if (i > 0)
        {
          signature_shares.push_back(nodeSignature);
        }
    }
    BOOST_CHECK_THROW(m_wallets[0]->FinalizeBlock(block, signature_shares), std::runtime_error);
  }

  // FinalizeBlock: throw error if current participant is not included
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures and update the block";
    std::vector<uint32_t> signers;
    std::string pre_signatures = "";
    CBlock block = m_blockchains.at(0)->GenerateBlock(get_next_block_time());

    // GetSignatureShare
    std::vector<std::string> signature_shares;
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
        std::string nodeSignature = m_wallets[i]->GetBlockSignature(block);
        BOOST_TEST(nodeSignature.empty() == false);
        if (i > 1)
        {
          signature_shares.push_back(nodeSignature);
        }
    }
    BOOST_CHECK_THROW(m_wallets[3]->FinalizeBlock(block, signature_shares), std::runtime_error);
  }

  // FinalizeBlock: try to aggregate signatures with more shares than needed
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures and update the block";
    std::vector<uint32_t> signers;
    CBlock block = m_blockchains.at(0)->GenerateBlock(get_next_block_time());

    // GetSignatureShare
    std::vector<std::string> signature_shares123;
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      std::string nodeSignature = m_wallets[i]->GetBlockSignature(block);
      BOOST_TEST(nodeSignature.empty() == false);
      if (i > 0) signature_shares123.push_back(nodeSignature);
    }

    CBlock final_block = m_wallets[3]->FinalizeBlock(block, signature_shares123);

    auto info_0 = m_bitcoinds.at(0)->getblockchaininfo();
    auto height_0 = info_0["blocks"].asUInt();

    m_blockchains.at(0)->SubmitBlock(0, final_block);

    auto info_1 = m_bitcoinds.at(0)->getblockchaininfo();
    auto height_1 = info_1["blocks"].asUInt();

    BOOST_TEST(height_1 == height_0 + 1);
  }

  // FinalizeBlock: try to aggregate signatures with more shares than needed
  {
    BOOST_LOG_TRIVIAL(debug) << "Testing FinalizeBlock to aggregate signatures and update the block";
    std::vector<uint32_t> signers;
    CBlock block = m_blockchains.at(3)->GenerateBlock(get_next_block_time());

    // GetSignatureShare
    std::vector<std::string> signature_shares;
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      std::string nodeSignature = m_wallets[i]->GetBlockSignature(block);
      BOOST_TEST(nodeSignature.empty() == false);
      signature_shares.push_back(nodeSignature);
    }

    CBlock final_block = m_wallets[3]->FinalizeBlock(block, signature_shares);

    auto info_0 = m_bitcoinds.at(3)->getblockchaininfo();
    auto height_0 = info_0["blocks"].asUInt();

    m_blockchains.at(3)->SubmitBlock(0, final_block);

    auto info_1 = m_bitcoinds.at(3)->getblockchaininfo();
    auto height_1 = info_1["blocks"].asUInt();

    BOOST_TEST(height_1 == height_0 + 1);
  }

} // test_blockchain_frost_wallet_bitcoin_00

BOOST_AUTO_TEST_SUITE_END() // test_blockchain_frost_wallet_bitcoin
