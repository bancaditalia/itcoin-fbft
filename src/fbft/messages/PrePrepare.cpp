// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <json/json.h>

#include <streams.h>
#include <version.h>

#include "../../utils/utils.h"
#include "config/FbftConfig.h"

using namespace std;
using namespace itcoin::blockchain;

namespace itcoin {
namespace fbft {
namespace messages {

PrePrepare::PrePrepare(uint32_t sender_id, uint32_t view, uint32_t seq_number, std::string req_digest,
                       CBlock proposed_block)
    : Message(NODE_TYPE::REPLICA, sender_id) {
  m_view = view;
  m_seq_number = seq_number;
  m_req_digest = req_digest;
  m_proposed_block = HexSerializableCBlock(proposed_block);
}

PrePrepare::PrePrepare(PlTerm Sender_id, PlTerm V, PlTerm N, PlTerm Req_digest, PlTerm Proposed_block)
    : Message(NODE_TYPE::REPLICA, Sender_id) {
  m_view = (long)V;
  m_seq_number = (long)N;
  m_req_digest = (char*)Req_digest;

  std::string proposed_block_hex{(char*)Proposed_block};
  this->set_proposed_block(proposed_block_hex);
}

PrePrepare::~PrePrepare(){};

void PrePrepare::set_proposed_block(std::string proposed_block_hex) {
  m_proposed_block = HexSerializableCBlock(proposed_block_hex);
}

std::vector<std::unique_ptr<messages::PrePrepare>> PrePrepare::BuildToBeSent(uint32_t replica_id) {
  std::vector<unique_ptr<messages::PrePrepare>> results{};
  PlTerm Replica_id{(long)replica_id}, V, N, Req_digest, Proposed_block;
  PlQuery query("msg_out_pre_prepare", PlTermv(Replica_id, V, N, Req_digest, Proposed_block));
  while (query.next_solution()) {
    messages::PrePrepare msg{Replica_id, V, N, Req_digest, Proposed_block};
    results.emplace_back(std::move(std::make_unique<messages::PrePrepare>(msg)));
  }
  return results;
}

messages::PrePrepare PrePrepare::FindByV_N_Req(uint32_t replica_id, uint32_t v, uint32_t n,
                                               std::string req_digest) {
  PlString Req_digest{(const char*)req_digest.c_str()};
  PlTerm Replica_id{(long)replica_id}, V, N, Proposed_block, Sender_id, Sender_sig;
  int result = PlCall("msg_log_pre_prepare",
                      PlTermv(Replica_id, V, N, Req_digest, Proposed_block, Sender_id, Sender_sig));
  if (result) {
    messages::PrePrepare msg{Sender_id, V, N, Req_digest, Proposed_block};
    if ((long)Sender_id != replica_id) {
      string sender_sig{(const char*)Sender_sig};
      msg.set_signature(sender_sig);
    }
    return msg;
  } else {
    throw(std::runtime_error("Unable to find PrePrepare with V and N"));
  }
}

bool PrePrepare::equals(const Message& other) const {
  if (typeid(*this) != typeid(other))
    return false;
  auto typed_other = static_cast<const PrePrepare&>(other);

  if (m_view != typed_other.m_view)
    return false;
  if (m_seq_number != typed_other.m_seq_number)
    return false;
  if (m_req_digest != typed_other.m_req_digest)
    return false;
  if (m_proposed_block.GetHash() != typed_other.m_proposed_block.GetHash())
    return false;
  return Message::equals(other);
}

std::unique_ptr<Message> PrePrepare::clone() {
  std::unique_ptr<Message> msg = std::make_unique<PrePrepare>(*this);
  return msg;
}

const std::string PrePrepare::digest() const {
  PlTerm Digest;
  int pl_ok =
      PlCall("digest_pre_prepare", PlTermv(PlTerm{(long)m_view}, PlTerm{(long)m_seq_number},
                                           PlString({(const char*)m_req_digest.c_str()}),
                                           PlString({(const char*)proposed_block_hex().c_str()}), Digest));
  if (!pl_ok) {
    string error_msg = str(boost::format("Unable to calculate %1% digest") % name());
    throw(std::runtime_error(error_msg));
  }
  string digest{(const char*)Digest};
  return digest;
}

std::string PrePrepare::proposed_block_hex() const { return m_proposed_block.GetHex(); }

std::string PrePrepare::identify() const {
  return str(boost::format("<%1%, req=%2%, V=%3%, N=%4%, S=%5%>") % name() % req_digest() % m_view %
             m_seq_number % m_sender_id);
}

// Serialization and deserialization

PrePrepare::PrePrepare(const Json::Value& root) : Message(root) {
  m_view = root["payload"]["v"].asUInt();
  m_seq_number = root["payload"]["n"].asUInt();
  m_req_digest = root["payload"]["req_digest"].asString();
  string proposed_block_hex = root["payload"]["data"].asString();
  this->set_proposed_block(proposed_block_hex);
}

std::string PrePrepare::ToBinBuffer() const {
  Json::Value payload;
  payload["n"] = m_seq_number;
  payload["v"] = m_view;
  payload["req_digest"] = m_req_digest;
  payload["data"] = proposed_block_hex();
  return this->FinalizeJsonRoot(payload);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
