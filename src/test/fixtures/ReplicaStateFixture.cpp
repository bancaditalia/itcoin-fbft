// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures.h"

ReplicaStateFixture::ReplicaStateFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp,
                                         uint32_t target_block_time)
    : PrologTestFixture(), CLUSTER_SIZE(cluster_size), GENESIS_BLOCK_TIMESTAMP(genesis_block_timestamp),
      TARGET_BLOCK_TIME(target_block_time) {
  BOOST_LOG_TRIVIAL(trace) << "Setup fixture ReplicaSetFixture";
  m_blockchain_config = std::make_unique<itcoin::FbftConfig>("infra/node00");
  m_blockchain_config->set_genesis_block_timestamp(GENESIS_BLOCK_TIMESTAMP);
  m_blockchain = std::make_unique<DummyBlockchain>(*m_blockchain_config);

  for (int i = 0; i < CLUSTER_SIZE; i++) {
    auto config = std::make_unique<itcoin::FbftConfig>("infra/node00");
    config->set_replica_id(i);
    config->set_cluster_size(CLUSTER_SIZE);
    config->set_genesis_block_hash("genesis");
    config->set_genesis_block_timestamp(0);
    config->set_target_block_time(TARGET_BLOCK_TIME);
    config->set_fbft_db_reset(true);
    config->set_fbft_db_filename("/tmp/miner.fbft.db");
    m_configs.emplace_back(move(config));

    auto wallet = std::make_unique<DummyRoastWallet>(*m_configs.at(i));
    m_wallets.emplace_back(move(wallet));

    std::string start_hash = m_configs.at(i)->genesis_block_hash();
    uint32_t start_height = 0;
    uint32_t start_time = m_configs.at(i)->genesis_block_timestamp();
    std::unique_ptr<ReplicaState> state = std::make_unique<ReplicaState>(
        *m_configs.at(i), *m_blockchain, *m_wallets.at(i), start_height, start_hash, start_time);
    state->set_synthetic_time(0);

    m_states.emplace_back(move(state));
  }
}

void ReplicaStateFixture::set_synthetic_time(double time) {
  for (auto& p_state : m_states) {
    p_state->set_synthetic_time(time);
  }
}
