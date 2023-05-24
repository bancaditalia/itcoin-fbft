// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures.h"

ReplicaSetFixture::ReplicaSetFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time):
ReplicaStateFixture(cluster_size, genesis_block_timestamp, target_block_time) {
  for (int i = 0; i < CLUSTER_SIZE; i++)
  {
    auto transport = std::make_unique<DummyNetwork>(*m_configs.at(i));
    m_transports.emplace_back(move(transport));

    std::string start_hash = m_configs.at(i)->genesis_block_hash();
    uint32_t start_height = 0;
    uint32_t start_time = m_configs.at(i)->genesis_block_timestamp();
    std::shared_ptr<Replica2> replica =
        std::make_shared<Replica2>(*m_configs.at(i), *m_blockchain, *m_wallets.at(i), *m_transports.at(i), start_height, start_hash, start_time);
    m_replica.emplace_back(replica);
  }

  for (auto p_replica : m_replica)
  {
    p_replica->set_synthetic_time(0);
    m_blockchain->listeners.push_back(p_replica);
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      m_transports.at(i)->listeners.push_back(p_replica);
    }
  }
}

void ReplicaSetFixture::kill(uint32_t replica_id) {
  if (!m_transports.at(replica_id)->active)
  {
    BOOST_LOG_TRIVIAL(debug) << str(
        boost::format("R%1% is already sleeping.") % replica_id);
    return;
  }

  BOOST_LOG_TRIVIAL(info) << str(
      boost::format("R%1% going to sleep now.") % replica_id);

  auto replica_to_remove = m_replica[replica_id];

  m_blockchain->listeners.erase(
      std::remove(m_blockchain->listeners.begin(), m_blockchain->listeners.end(), replica_to_remove),
      m_blockchain->listeners.end());

  m_transports.at(replica_id)->active = false;

  for (auto &transport : m_transports)
  {
    transport->listeners.erase(
        std::remove(transport->listeners.begin(), transport->listeners.end(), replica_to_remove),
        transport->listeners.end());
  }
}

void ReplicaSetFixture::wake(uint32_t replica_id)
{
  if (m_transports.at(replica_id)->active)
  {
    BOOST_LOG_TRIVIAL(debug) << str(
        boost::format("R%1% is already awake.") % replica_id);
    return;
  }

  BOOST_LOG_TRIVIAL(debug) << str(
      boost::format("R%1% wakes up.") % replica_id);

  auto replica_to_add = m_replica[replica_id];

  m_transports.at(replica_id)->active = true;
  for (auto &transport : m_transports)
  {
    transport->listeners.insert(transport->listeners.begin() + replica_id, replica_to_add);
  }

  // Reactivate blockchain and sync last block
  m_blockchain->listeners.insert(m_blockchain->listeners.begin() + replica_id, m_replica[replica_id]);
  for (size_t i = std::max((int)m_blockchain->chain.size() - 3, 0); i < m_blockchain->chain.size(); i++)
  {
    std::unique_ptr<Block> p_msg = std::make_unique<Block>(i, m_blockchain->chain[i].nTime, m_blockchain->chain[i].GetHash().GetHex());
    m_blockchain->listeners[replica_id]->ReceiveIncomingMessage(move(p_msg));
  }
}

void ReplicaSetFixture::move_forward(int time_delta)
{
  for (size_t i = 0; i < CLUSTER_SIZE; i++)
  {
    if (m_transports[i]->active)
    {
      m_replica[i]->CheckTimedActions();
    }
  }

  // SimulateReceiveMessages all transports N times
  for (uint32_t N = 0; N < 10; N++)
  {
    for (size_t i = 0; i < CLUSTER_SIZE; i++)
    {
      m_transports[i]->SimulateReceiveMessages();
    }
  }

  double current_time = m_replica[0]->current_time();
  current_time += time_delta;
  set_synthetic_time(current_time);
}
