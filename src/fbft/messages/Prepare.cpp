// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <json/json.h>

#include "config/FbftConfig.h"

using namespace std;

namespace itcoin {
namespace fbft {
namespace messages {

Prepare::Prepare(uint32_t sender_id, uint32_t view, uint32_t seq_number, std::string req_digest)
    : Message(NODE_TYPE::REPLICA, sender_id) {
  m_view = view;
  m_seq_number = seq_number;
  m_req_digest = req_digest;
}

Prepare::Prepare(PlTerm Sender_id, PlTerm V, PlTerm N, PlTerm Req_digest)
    : Message(NODE_TYPE::REPLICA, Sender_id) {
  m_view = (long)V;
  m_seq_number = (long)N;
  m_req_digest = (char*)Req_digest;
}

Prepare::~Prepare(){};

std::vector<std::unique_ptr<messages::Prepare>> Prepare::BuildToBeSent(uint32_t replica_id) {
  std::vector<unique_ptr<messages::Prepare>> results{};
  PlTerm Replica_id{(long)replica_id}, V, N, Req_digest;
  PlQuery query("msg_out_prepare", PlTermv(Replica_id, V, N, Req_digest));
  while (query.next_solution()) {
    std::unique_ptr<messages::Prepare> msg =
        std::make_unique<messages::Prepare>(Replica_id, V, N, Req_digest);
    results.emplace_back(std::move(msg));
  }
  return results;
}

bool Prepare::equals(const Message& other) const {
  if (typeid(*this) != typeid(other))
    return false;
  auto typed_other = static_cast<const Prepare&>(other);

  if (m_view != typed_other.m_view)
    return false;
  if (m_seq_number != typed_other.m_seq_number)
    return false;
  if (m_req_digest != typed_other.m_req_digest)
    return false;
  return Message::equals(other);
}

std::unique_ptr<Message> Prepare::clone() {
  std::unique_ptr<Message> msg = std::make_unique<Prepare>(*this);
  return msg;
}

const std::string Prepare::digest() const {
  PlTerm Digest;
  int pl_ok = PlCall("digest_prepare", PlTermv(PlTerm{(long)m_view}, PlTerm{(long)m_seq_number},
                                               PlString({(const char*)m_req_digest.c_str()}),
                                               PlTerm{(long)m_sender_id}, Digest));
  if (!pl_ok) {
    string error_msg = str(boost::format("Unable to calculate %1% digest") % name());
    throw(std::runtime_error(error_msg));
  }

  string digest{(const char*)Digest};
  return digest;
}

std::string Prepare::identify() const {
  return str(boost::format("<%1%, Req=%2%, V=%3%, N=%4%, S=%5%>") % name() % m_req_digest % m_view %
             m_seq_number % m_sender_id);
}

// Serialization and deserialization

Prepare::Prepare(const Json::Value& root) : Message(root) {
  m_view = root["payload"]["v"].asUInt();
  m_seq_number = root["payload"]["n"].asUInt();
  m_req_digest = root["payload"]["req_digest"].asString();
}

std::string Prepare::ToBinBuffer() const {
  Json::Value payload;
  payload["n"] = m_seq_number;
  payload["v"] = m_view;
  payload["req_digest"] = m_req_digest;
  return this->FinalizeJsonRoot(payload);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
