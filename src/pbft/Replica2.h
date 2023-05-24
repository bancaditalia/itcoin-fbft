#ifndef ITCOIN_PBFT_REPLICA_2_H
#define ITCOIN_PBFT_REPLICA_2_H

#include "../blockchain/blockchain.h"
#include "../network/network.h"
#include "../wallet/wallet.h"

#include "state/state.h"

namespace blockchain = itcoin::blockchain;
namespace network = itcoin::network;
namespace wallet = itcoin::wallet;
namespace state = itcoin::pbft::state;

namespace itcoin {
namespace pbft {

class Replica2: public network::NetworkListener, public state::ReplicaState
{
  public:
    Replica2(
      const itcoin::PbftConfig& config,
      blockchain::Blockchain& blockchain,
      wallet::Wallet& wallet,
      network::NetworkTransport& transport,
      uint32_t start_height,
      std::string start_hash,
      uint32_t start_time
    );

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

}
}

#endif // ITCOIN_PBFT_REPLICA_2_H