// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_FBFT_FIXTURES_H
#define ITCOIN_FBFT_FIXTURES_H

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/results_collector.hpp>
/*
 * As of boost 1.75, boost::process relies on boost::filesystem and is not able
 * to deal with std::filesystem
 */
#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <SWI-cpp.h>

#include <iomanip>

#include "config/FbftConfig.h"
#include "../../fbft/Replica2.h"
#include "../../fbft/actions/actions.h"
#include "../../fbft/state/state.h"
#include "../../transport/btcclient.h"

#include "../stubs/stubs.h"

namespace utf = boost::unit_test;

using namespace itcoin::fbft;
using namespace itcoin::fbft::actions;
using namespace itcoin::fbft::messages;
using namespace itcoin::fbft::state;
using namespace itcoin::test;

struct BtcClientFixture
{
  itcoin::FbftConfig cfgNode0;
  itcoin::transport::BtcClient bitcoind0;

  BtcClientFixture();
  ~BtcClientFixture();
};

struct BitcoinRpcTestFixture
{
  BitcoinRpcTestFixture();
  ~BitcoinRpcTestFixture();
  std::string address_at(uint32_t replica_id);

  const uint32_t CLUSTER_SIZE = 4;
  std::vector<std::unique_ptr<itcoin::FbftConfig>> m_configs;
  std::vector<std::unique_ptr<itcoin::transport::BtcClient>> m_bitcoinds;
  std::vector<std::unique_ptr<itcoin::wallet::BitcoinRpcWallet>> m_wallets;
  std::vector<std::unique_ptr<itcoin::blockchain::BitcoinBlockchain>> m_blockchains;
};

struct PrologTestFixture
{
  PrologTestFixture();
  ~PrologTestFixture();
};

struct ReplicaStateFixture : PrologTestFixture
{
  ReplicaStateFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time);

  void set_synthetic_time(double time);

  itcoin::SIGNATURE_ALGO_TYPE SIG_ALGO;
  uint32_t CLUSTER_SIZE;
  uint32_t GENESIS_BLOCK_TIMESTAMP;
  uint32_t TARGET_BLOCK_TIME;

  std::unique_ptr<itcoin::FbftConfig> m_blockchain_config;
  std::unique_ptr<DummyBlockchain> m_blockchain;

  std::vector<std::unique_ptr<itcoin::FbftConfig>> m_configs;
  std::vector<std::unique_ptr<itcoin::wallet::RoastWallet>> m_wallets;
  std::vector<std::unique_ptr<ReplicaState>> m_states;
}; // ReplicaStateFixture

struct ReplicaSetFixture : ReplicaStateFixture
{
  ReplicaSetFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time);

  void kill(uint32_t replica_id);
  void wake(uint32_t replica_id);
  void move_forward(int time_delta);

  std::vector<std::unique_ptr<DummyNetwork>> m_transports;
  std::vector<std::shared_ptr<Replica2>> m_replica;
};

struct BitcoinInfraFixture : public BitcoinRpcTestFixture
{

  std::vector<boost::process::child> nodes;
  boost::filesystem::path currentDirectory;
  uint32_t m_latest_block_time;
  bool m_reset = false;

  BitcoinInfraFixture();
  ~BitcoinInfraFixture();

  std::string getBitcoinNodeDirName(size_t nodeId);

  void resetBlockchain(boost::filesystem::path bitcoinDir);

  void stopProc(boost::process::child &nodeProc);

  uint32_t get_present_block_time();

}; // struct BitcoinInfraFixture

#endif // ITCOIN_FBFT_FIXTURES_H
