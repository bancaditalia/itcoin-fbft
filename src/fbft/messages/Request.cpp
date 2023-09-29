// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <chrono>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

using namespace std;

namespace itcoin {
namespace fbft {
namespace messages {

Request::Request() : Message(NODE_TYPE::CLIENT, 9999) {
  m_target_block_time = 0;
  m_timestamp = 0;
}

Request::Request(uint32_t genesis_block_timestamp, uint32_t target_block_time, uint32_t timestamp)
    : Message(NODE_TYPE::CLIENT, 9999) {
  m_genesis_block_timestamp = genesis_block_timestamp;
  m_target_block_time = target_block_time;
  m_timestamp = timestamp;
}

Request::~Request(){};

messages::Request Request::FindByDigest(uint32_t replica_id, std::string req_digest) {
  messages::Request msg;
  bool result = TryFindByDigest(replica_id, req_digest, msg);
  if (result) {
    return msg;
  } else {
    string error_msg = str(boost::format("Unable to find REQUEST with digest %1%") % req_digest);
    throw(std::runtime_error(error_msg));
  }
}

bool Request::TryFindByDigest(uint32_t replica_id, const std::string req_digest,
                              messages::Request& out_request) {
  PlString Req_digest{(const char*)req_digest.c_str()};
  PlTerm Replica_id{(long)replica_id}, Req_timestamp, Genesis_block_time, Target_block_time;
  int result = PlCall("msg_log_request", PlTermv(Replica_id, Req_digest, Req_timestamp)) &&
               PlCall("nb_getval", PlTermv(PlTerm("target_block_time"), Target_block_time)) &&
               PlCall("nb_getval", PlTermv(PlTerm("genesis_block_timestamp"), Genesis_block_time));
  if (result) {
    uint32_t genesis_block_time = (long)Genesis_block_time;
    uint32_t target_block_time = (long)Target_block_time;
    uint32_t req_timestamp = (long)Req_timestamp;
    out_request = messages::Request{genesis_block_time, target_block_time, req_timestamp};
    return true;
  } else {
    BOOST_LOG_TRIVIAL(debug) << str(boost::format("Unable to find REQUEST with digest %1%") % req_digest);
    return false;
  }
}

bool Request::equals(const Message& other) const {
  if (typeid(*this) != typeid(other))
    return false;
  auto typed_other = static_cast<const Request&>(other);

  if (m_timestamp != typed_other.m_timestamp)
    return false;
  return Message::equals(other);
}

std::unique_ptr<Message> Request::clone() {
  std::unique_ptr<Message> msg = std::make_unique<Request>(*this);
  return msg;
}

uint32_t Request::height() const { return (m_timestamp - m_genesis_block_timestamp) / m_target_block_time; }

const std::string Request::digest() const {
  return str(boost::format("(H=%1%, T=%2%)") % height() % m_timestamp);
}

std::string Request::identify() const {
  return str(boost::format("Request digest=%1%, timestamp=%3%") % digest() % m_timestamp);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
