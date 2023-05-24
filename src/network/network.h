#ifndef ITCOIN_NETWORK_NETWORK_H
#define ITCOIN_NETWORK_NETWORK_H

#include "../pbft/messages/messages.h"

namespace itcoin {
  class PbftConfig;
}
namespace messages = itcoin::pbft::messages;

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
    NetworkTransport(const itcoin::PbftConfig& conf);
    virtual void BroadcastMessage(std::unique_ptr<messages::Message> p_msg) = 0;

  protected:
    const itcoin::PbftConfig& m_conf;
};

}
}

#endif // ITCOIN_NETWORK_NETWORK_H