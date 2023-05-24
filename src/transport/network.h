// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_NETWORK_NETWORK_H
#define ITCOIN_NETWORK_NETWORK_H

#include "../fbft/messages/messages.h"

namespace itcoin {
  class FbftConfig;
}
namespace messages = itcoin::fbft::messages;

namespace itcoin {
namespace network {

class NetworkListener
{
  public:
    NetworkListener();
    virtual const uint32_t id() const = 0;
    virtual void ReceiveIncomingMessage(std::unique_ptr<messages::Message> msg) = 0;
};

class NetworkTransport
{
  public:
    NetworkTransport(const itcoin::FbftConfig& conf);
    virtual void BroadcastMessage(std::unique_ptr<messages::Message> p_msg) = 0;

  protected:
    const itcoin::FbftConfig& m_conf;
};

}
}

#endif // ITCOIN_NETWORK_NETWORK_H