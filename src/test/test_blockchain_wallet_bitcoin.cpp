// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <thread>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "fixtures/fixtures.h"
#include "../blockchain/blockchain.h"

namespace utf = boost::unit_test;

BOOST_AUTO_TEST_SUITE(test_blockchain_wallet_bitcoin, *utf::enabled())

using namespace itcoin::blockchain;
using namespace itcoin::fbft::messages;

BOOST_FIXTURE_TEST_CASE(test_blockchain_wallet_bitcoin_00, BitcoinInfraFixture)
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::trace
  );

  { // Blockchain block creation and validity testing
  BOOST_LOG_TRIVIAL(debug) << "Testing blockchain bitcoin with replica = 0";
  CBlock new_block = m_blockchains.at(0)->GenerateBlock(get_present_block_time());
  // check_signet_solution=false because at this point we do not have the final signature
  bool is_new_block_valid = m_blockchains.at(1)->TestBlockValidity(0, new_block, false);
  BOOST_TEST( is_new_block_valid == true );
  }

  { // Message signature
  uint32_t msg_sender_id = 0;
  Prepare msg{msg_sender_id, 0, 0, "req_digest"};
  msg.Sign(*m_wallets[0]);
  bool is_sig_valid = m_wallets[1]->VerifySignature(msg);
  BOOST_TEST( is_sig_valid == true );
  }

} // test_blockchain_wallet_bitcoin_00

BOOST_AUTO_TEST_SUITE_END() // test_blockchain_wallet_bitcoin
