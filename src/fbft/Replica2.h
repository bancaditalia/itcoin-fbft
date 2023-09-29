// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_FBFT_REPLICA_2_H
#define ITCOIN_FBFT_REPLICA_2_H

#include "../blockchain/blockchain.h"
#include "../transport/network.h"
#include "../wallet/wallet.h"

#include "state/state.h"

namespace blockchain = itcoin::blockchain;
namespace network = itcoin::network;
namespace wallet = itcoin::wallet;
namespace state = itcoin::fbft::state;

namespace itcoin {
namespace fbft {

class Replica2 : public network::NetworkListener, public state::ReplicaState {
public:
  Replica2(const itcoin::FbftConfig& config, blockchain::Blockchain& blockchain, wallet::RoastWallet& wallet,
           network::NetworkTransport& transport, uint32_t start_height, std::string start_hash,
           uint32_t start_time);

  // Getters
  const uint32_t id() const;

  // Operations
  void ReceiveIncomingMessage(std::unique_ptr<messages::Message> msg);
  void CheckTimedActions();

private:
  network::NetworkTransport& m_transport;

  void GenerateRequests();
  void ApplyActiveActions();
};

} // namespace fbft
} // namespace itcoin

#endif // ITCOIN_FBFT_REPLICA_2_H
