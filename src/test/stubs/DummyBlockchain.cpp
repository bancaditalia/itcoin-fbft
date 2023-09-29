// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "stubs.h"

#include <boost/log/trivial.hpp>

#include "config/FbftConfig.h"

namespace msgs = itcoin::fbft::messages;

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::network;

namespace itcoin {
namespace test {

DummyBlockchain::DummyBlockchain(const itcoin::FbftConfig& conf) : Blockchain(conf) { Init(); }

void DummyBlockchain::Init() {
  chain.clear();
  CBlock genesis = CBlock{};
  genesis.nTime = m_conf.genesis_block_timestamp();
  chain.emplace_back(genesis);
}

CBlock DummyBlockchain::GenerateBlock(uint32_t block_timestamp) {
  CBlock block{};
  block.nTime = block_timestamp;
  return block;
}

bool DummyBlockchain::TestBlockValidity(const uint32_t height, const CBlock& block,
                                        bool check_signet_solution) {
  return true;
}

void DummyBlockchain::SubmitBlock(const uint32_t height, const CBlock& block) {
  BOOST_LOG_TRIVIAL(debug) << "Submitting a block to blockchain";
  CBlock b_copy{block};

  if (height < chain.size() && chain[height].GetHash() != block.GetHash()) {
    throw runtime_error("submitting a different block at same height, double spending!");
  }
  if (height < chain.size() && chain[height].GetHash() == block.GetHash()) {
    BOOST_LOG_TRIVIAL(debug) << "Block already present in the blockchain";
    return;
  }
  if (height > chain.size()) {
    throw runtime_error("submitting a block at height too far in the future, invalid chain!");
  }

  chain.emplace_back(b_copy);
  for (shared_ptr<NetworkListener> p_listener : listeners) {
    unique_ptr<msgs::Block> p_msg = make_unique<msgs::Block>(height, b_copy.nTime, b_copy.GetHash().GetHex());
    p_listener->ReceiveIncomingMessage(move(p_msg));
  }
}

} // namespace test
} // namespace itcoin
