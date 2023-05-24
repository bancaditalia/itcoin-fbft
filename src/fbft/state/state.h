// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_FBFT_STATE_STATE_H
#define ITCOIN_FBFT_STATE_STATE_H

#include "config/FbftConfig.h"
#include "../actions/actions.h"
#include "../messages/messages.h"

namespace itcoin { namespace blockchain {
  class Blockchain;
}}

namespace itcoin { namespace wallet {
  class Wallet;
}}

namespace actions = itcoin::fbft::actions;
namespace messages = itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace state {

// A replica state
class ReplicaState {
  public:
    ReplicaState(
      const itcoin::FbftConfig& conf,
      blockchain::Blockchain& blockchain,
      wallet::RoastWallet& wallet,
      uint32_t start_height,
      std::string start_hash,
      uint32_t start_time
    );
    ~ReplicaState() {};

    // Getters
    const std::vector<std::unique_ptr<messages::Message>>& in_msg_buffer() const;
    const std::vector<std::unique_ptr<messages::Message>>& out_msg_buffer() const;
    const std::vector<std::unique_ptr<actions::Action>>& active_actions() const;
    double current_time() const;
    double latest_request_time() const;
    double latest_reply_time() const;
    uint32_t h() const;
    uint32_t primary() const;
    uint32_t view() const;

    // Setters
    // Synthetic time is a floating point number expressing the time in seconds since the Epoch at 1970-01-01.
    void set_synthetic_time(double time);

    // Operations
    void Init(uint32_t start_height, std::string start_hash, uint32_t start_time);
    void Apply(const actions::Action& action);
    void ClearOutMessageBuffer();
    void ReceiveIncomingMessage(std::unique_ptr<messages::Message> msg);
    void UpdateActiveActions();

  protected:
    // Configuration
    const itcoin::FbftConfig& m_conf;
    blockchain::Blockchain& m_blockchain;
    wallet::RoastWallet& m_wallet;

    // Buffer of incoming messages
    std::vector<std::unique_ptr<messages::Message>> m_in_msg_buffer;
    std::vector<std::unique_ptr<messages::Message>> m_in_msg_awaiting_checkpoint_buffer;

    // Buffer of outgoing messages
    std::vector<std::unique_ptr<messages::Message>> m_out_msg_buffer;

    // Active actions
    std::vector<std::unique_ptr<actions::Action>> m_active_actions;

  private:
    // Update the set of messages to be sent
    void UpdateOutMessageBuffer();
};

}
}
}

#endif // ITCOIN_FBFT_STATE_STATE_H
