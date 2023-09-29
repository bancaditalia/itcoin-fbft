// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "blockchain.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../transport/btcclient.h"
#include "config/FbftConfig.h"
#include "generate.h"

using namespace std;
using namespace itcoin::blockchain;

namespace itcoin {
namespace blockchain {

BitcoinBlockchain::BitcoinBlockchain(const itcoin::FbftConfig& conf, transport::BtcClient& bitcoind)
    : Blockchain(conf), m_bitcoind(bitcoind) {
  m_reward_address = m_conf.replica_set_v().at(m_conf.id()).p2pkh();
  BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% BitcoinBlockchain, using reward address %2%.") %
                                  m_conf.id() % m_reward_address);
}

CBlock BitcoinBlockchain::GenerateBlock(uint32_t block_timestamp) {
  return generateBlock(m_bitcoind, m_reward_address, block_timestamp);
}

bool BitcoinBlockchain::TestBlockValidity(const uint32_t height, const CBlock& block,
                                          bool check_signet_solution) {
  auto block_ser = HexSerializableCBlock(block);
  const uint32_t block_size_bytes = block_ser.GetHex().length() / 2;
  const std::string block_hash = block.GetBlockHeader().GetHash().ToString();

  BOOST_LOG_TRIVIAL(debug) << str(
      boost::format("R%1% BitcoinBlockchain::TestBlockValidity invoking for "
                    "candidate block at height %2%, blocksize %3% bytes, block hash: %4%") %
      m_conf.id() % height % block_size_bytes % block_hash);

  Json::Value result;
  try {
    result = m_bitcoind.testblockvalidity(block_ser.GetHex(), check_signet_solution);
  } catch (jsonrpc::JsonRpcException& e) {
    BOOST_LOG_TRIVIAL(warning) << str(boost::format("R%1% BitcoinBlockchain::TestBlockValidity for candidate "
                                                    "block at height %2% with hash %3% raised %4%.") %
                                      m_conf.id() % height % block_hash % e.what());
    return false;
  }

  BOOST_LOG_TRIVIAL(debug) << str(
      boost::format("R%1% BitcoinBlockchain::TestBlockValidity for candidate "
                    "block at height %2% with hash %3%. Result = %4% (null means ok).") %
      m_conf.id() % height % block_hash % result);
  return true;
}

void BitcoinBlockchain::SubmitBlock(const uint32_t height, const CBlock& block) {
  const auto block_ser = HexSerializableCBlock(block);
  const std::string block_hash = block.GetBlockHeader().GetHash().ToString();
  try {
    const uint32_t block_size_bytes = block_ser.GetHex().length() / 2;
    BOOST_LOG_TRIVIAL(debug) << str(
        boost::format("R%1% BitcoinBlockchain::SubmitBlock submitting block at height %2% "
                      "block size: %3% bytes, block hash: %4%") %
        m_conf.id() % height % block_size_bytes % block_hash);

    auto result = m_bitcoind.submitblock(block_ser.GetHex());
    BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% BitcoinBlockchain::SubmitBlock for block at height "
                                                  "%2%, block hash: %3%. Result = %4% (null means ok).") %
                                    m_conf.id() % height % block_hash % result);
  } catch (const jsonrpc::JsonRpcException& e) {
    if (e.GetMessage() == "The response is invalid: \"duplicate\"\n") {
      BOOST_LOG_TRIVIAL(warning) << str(
          boost::format("R%1% BitcoinBlockchain::SubmitBlock the submitblock "
                        "invocation for block height %2% (hash %3%) failed because the block "
                        "was already in the blockchain. Most probably another replica already "
                        "submitted the same block and was propagated to the local node before "
                        "the submitblock call was attempted.") %
          m_conf.id() % height % block_hash);
    } else if (e.GetMessage() == "The response is invalid: \"inconclusive\"\n") {
      BOOST_LOG_TRIVIAL(warning) << str(
          boost::format("R%1% BitcoinBlockchain::SubmitBlock the submitblock "
                        "invocation for height %2% (hash %3%) returned 'inconclusive'. This "
                        "problem is temporarily ignored.") %
          m_conf.id() % height % block_hash);
    } else {
      BOOST_LOG_TRIVIAL(error) << str(
          boost::format("R%1% BitcoinBlockchain::SubmitBlock got exception "
                        "while trying to submit block at height %2% (hash %3%): %4%") %
          m_conf.id() % height % block_hash % e.what());
      throw e;
    }
  }
}

} // namespace blockchain
} // namespace itcoin
