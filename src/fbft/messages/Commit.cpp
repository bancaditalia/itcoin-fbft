// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/format.hpp>

#include "config/FbftConfig.h"
#include "../../utils/utils.h"

using namespace std;
using namespace itcoin::blockchain;

namespace itcoin {
namespace fbft {
namespace messages {

Commit::Commit(uint32_t sender_id,
  uint32_t view, uint32_t seq_number,
  std::string block_signature)
:Message(NODE_TYPE::REPLICA, sender_id)
{
  m_view = view;
  m_seq_number = seq_number;
  m_pre_signature = block_signature;
}

Commit::Commit(PlTerm Sender_id, PlTerm V, PlTerm N, PlTerm Block_signature)
:Message(NODE_TYPE::REPLICA, Sender_id)
{
  m_view = (long) V;
  m_seq_number = (long) N;
  std::string block_signature_hex{ (const char*) Block_signature };
  this->set_pre_signature(block_signature_hex);
}

void Commit::set_pre_signature(std::string pre_signature_hex)
{
  m_pre_signature = pre_signature_hex;
}

Commit::~Commit()
{
};

std::vector<std::unique_ptr<messages::Commit>> Commit::BuildToBeSent(uint32_t replica_id)
{
  std::vector<unique_ptr<messages::Commit>> results{};
  PlTerm Replica_id{(long) replica_id}, V, N, Block_signature;
  PlQuery query("msg_out_commit", PlTermv(Replica_id, V, N, Block_signature));
  while ( query.next_solution() )
  {
    messages::Commit msg{Replica_id, V, N, Block_signature};
    results.emplace_back(std::move(std::make_unique<messages::Commit>(msg)));
  }
  return results;
}

std::vector<messages::Commit> Commit::FindByV_N(uint32_t replica_id, uint32_t v, uint32_t n)
{
  std::vector<messages::Commit> results{};
  PlTerm Replica_id{(long) replica_id}, V{(long) v}, N{(long) n}, Block_signature, Sender_id, Sender_sig;
  PlQuery query{"msg_log_commit", PlTermv(Replica_id, V, N, Block_signature, Sender_id, Sender_sig)};
  while ( query.next_solution() )
  {
    messages::Commit msg{Sender_id, V, N, Block_signature};
    if ((long) Sender_id != replica_id)
    {
      string sender_sig_hex{(const char*)  Sender_sig};
      msg.set_signature(sender_sig_hex);
    }
    results.emplace_back(msg);
  }
  return results;
}

bool Commit::equals(const Message& other) const
{
  if (typeid(*this) != typeid(other)) return false;
  auto typed_other = static_cast<const Commit&>(other);

  if (m_view != typed_other.m_view) return false;
  if (m_seq_number != typed_other.m_seq_number) return false;
  return Message::equals(other);
}

std::unique_ptr<Message> Commit::clone()
{
  std::unique_ptr<Message> msg = std::make_unique<Commit>(*this);
  return msg;
}

const std::string Commit::digest() const
{
  PlTerm Digest;
  int pl_ok = PlCall("digest_commit", PlTermv(
    PlTerm{(long) m_view},
    PlTerm{(long) m_seq_number},
    PlString({(const char*) pre_signature().c_str()}),
    PlTerm{(long) m_sender_id},
    Digest
  ));
  if (!pl_ok)
  {
    string error_msg = str(
      boost::format("Unable to calculate %1% digest")
      % name()
    );
    throw(std::runtime_error(error_msg));
  }
  string digest{(const char *) Digest};
  return digest;
}

std::string Commit::identify() const
{
  return str(
    boost::format( "<%1%, V=%2%, N=%3%, S=%4%>" )
      % name()
      % m_view
      % m_seq_number
      % m_sender_id
  );
}

// Serialization and deserialization

Commit::Commit(const Json::Value& root):
Message(root)
{
  m_view = root["payload"]["v"].asUInt();
  m_seq_number = root["payload"]["n"].asUInt();
  string block_signature_hex = root["payload"]["data"].asString();
  this->set_pre_signature(block_signature_hex);
}

std::string Commit::ToBinBuffer() const
{
  Json::Value payload;
  payload["n"] = m_seq_number;
  payload["v"] = m_view;
  payload["data"] = pre_signature();
  return this->FinalizeJsonRoot(payload);
}

}
}
}
