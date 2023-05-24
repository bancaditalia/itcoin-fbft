// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <json/json.h>

#include "../../wallet/wallet.h"
#include "../../utils/utils.h"

using namespace std;
using namespace itcoin::wallet;

namespace itcoin {
namespace fbft {
namespace messages {

Message::Message(NODE_TYPE sender_role, uint32_t sender_id)
: m_sender_role(sender_role), m_sender_id(sender_id)
{
}

Message::Message(NODE_TYPE sender_role, PlTerm Sender_id)
: m_sender_role(sender_role), m_sender_id((long) Sender_id)
{
}

Message::Message(const Json::Value& root)
{
  // This is always a replica, because we never send requests on the network
  m_sender_role = NODE_TYPE::REPLICA;
  m_sender_id = root["payload"]["sender_id"].asUInt();
  m_signature = root["signature"].asString();
};

Message::~Message()
{
};

string Message::name() const
{
  return MSG_TYPE_AS_STRING[static_cast<uint32_t>(this->type())];
}

std::optional<uint32_t> Message::seq_number_as_opt() const
{
  return std::nullopt;
}

const std::string Message::digest() const
{
  throw(std::runtime_error("Message::digest() not available for message type: "+name()));
}

bool Message::equals(const Message& other) const
{
  if (typeid(*this) != typeid(other)) return false;
  if (m_sender_role != other.m_sender_role) return false;
  if (m_sender_id != other.m_sender_id) return false;
  if (m_signature != other.m_signature) return false;
  return true;
}

std::string Message::signature() const {
  return this->m_signature;
}

void Message::set_signature(std::string signature)
{
  this->m_signature = signature;
}

// Operations

void Message::Sign(const Wallet& wallet)
{
  wallet.AppendSignature(*this);
}

bool Message::VerifySignatures(const Wallet& wallet)
{
  return wallet.VerifySignature(*this);
}

//

bool Message::operator==(const Message& other) const
{
    return this->equals(other);
}

std::ostream& operator<<(std::ostream& Str, const Message& message)
{
  // print something from v to str, e.g: Str << v.getX();
  string message_str = message.identify();
  Str << message_str;
  return Str;
}

// Serialization and deserialization

std::string Message::FinalizeJsonRoot(Json::Value& payload) const
{
  payload["type"] = static_cast<uint32_t>(type());
  payload["sender_id"] = m_sender_id;

  Json::Value root;
  root["payload"] = payload;
  root["signature"] = signature();

  std::string result = Json::FastWriter().write(root);;
  BOOST_LOG_TRIVIAL(trace) << result;
  return result;
}

std::string Message::ToBinBuffer() const
{
  throw(std::runtime_error("Message::ToBinBuffer() not available for message type: "+name()));
}

optional<unique_ptr<Message>> Message::BuildFromBinBuffer(const std::string& bin_buffer)
{
  optional<unique_ptr<Message>> result = nullopt;

  Json::Reader reader;
  Json::Value root;
  bool parsing_ok = reader.parse( bin_buffer, root );
  if(!parsing_ok)
  {
    string error_msg = str(
      boost::format( "Message::BuildFromBinBuffer unable parse json: %1%." )
        % bin_buffer
    );
    BOOST_LOG_TRIVIAL(error) << error_msg;
  }
  else
  {
    uint32_t msg_type = root["payload"]["type"].asUInt();
    if (msg_type == MSG_TYPE::COMMIT)
    {
      result = make_unique<Commit>(root);
    }
    else if (msg_type == MSG_TYPE::NEW_VIEW)
    {
      result = make_unique<NewView>(root);
    }
    else if (msg_type == MSG_TYPE::PREPARE)
    {
      result = make_unique<Prepare>(root);
    }
    else if (msg_type == MSG_TYPE::PRE_PREPARE)
    {
      result = make_unique<PrePrepare>(root);
    }
    else if (msg_type == MSG_TYPE::VIEW_CHANGE)
    {
      result = make_unique<ViewChange>(root);
    }
    else if (msg_type == MSG_TYPE::ROAST_SIGNATURE_SHARE)
    {
      result = make_unique<RoastSignatureShare>(root);
    }
    else if (msg_type == MSG_TYPE::ROAST_PRE_SIGNATURE)
    {
      result = make_unique<RoastPreSignature>(root);
    }
    else
    {
      string error_msg = str(
        boost::format( "Message::BuildFromBinBuffer unable to identify message type in json: %1%." )
          % bin_buffer
      );
      BOOST_LOG_TRIVIAL(error) << error_msg;
    }

  }

  return result;
}

}
}
}
