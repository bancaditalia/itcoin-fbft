// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "stubs.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "config/FbftConfig.h"

namespace msgs = itcoin::fbft::messages;

using namespace std;
using namespace itcoin::network;

namespace itcoin {
namespace test {

NetworkStub::NetworkStub() : active(true) {}

DummyNetwork::DummyNetwork(const itcoin::FbftConfig& conf) : NetworkTransport(conf) {}

void DummyNetwork::BroadcastMessage(std::unique_ptr<msgs::Message> p_msg) {
  if (!active)
    return;

  BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% Transport, broadcasting %2% to other replicas.") %
                                  p_msg->sender_id() % p_msg->identify());
  m_buffer.emplace_back(move(p_msg));
}

void DummyNetwork::SimulateReceiveMessages() {
  if (!active)
    return;

  for (auto& p_msg : m_buffer) {
    for (shared_ptr<NetworkListener> p_listener : listeners) {
      if (p_listener->id() != m_conf.id()) {
        unique_ptr<msgs::Message> p_msg_clone = p_msg->clone();
        p_listener->ReceiveIncomingMessage(move(p_msg_clone));
      }
    }
  }
  m_buffer.clear();
}

} // namespace test
} // namespace itcoin
