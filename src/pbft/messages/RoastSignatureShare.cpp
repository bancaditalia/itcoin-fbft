#include "messages.h"

#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

#include <streams.h>
#include <version.h>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"

#include "../../PbftConfig.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;

namespace itcoin {
namespace pbft {
namespace messages {

RoastSignatureShare::RoastSignatureShare(uint32_t sender_id, string signature_share, string next_pre_signature_share):
Message(NODE_TYPE::REPLICA, sender_id)
{
  m_signature_share = signature_share;
  m_next_pre_signature_share = next_pre_signature_share;
}

RoastSignatureShare::RoastSignatureShare(PlTerm Sender_id, PlTerm Signature_share, PlTerm Next_pre_signature_share):
Message(NODE_TYPE::REPLICA, Sender_id)
{
  m_signature_share = std::string{(const char*)  Signature_share};
  m_next_pre_signature_share = std::string{(const char*)  Next_pre_signature_share};
}

RoastSignatureShare::~RoastSignatureShare()
{
}

std::vector<std::unique_ptr<messages::RoastSignatureShare>> RoastSignatureShare::BuildToBeSent(uint32_t replica_id)
{
  std::vector<unique_ptr<messages::RoastSignatureShare>> results{};
  PlTerm Replica_id{(long) replica_id}, Signature_share, Next_pre_signature_share;
  PlQuery query("msg_out_roast_signature_share", PlTermv(Replica_id, Signature_share, Next_pre_signature_share));
  while ( query.next_solution() )
  {
    std::unique_ptr<messages::RoastSignatureShare> msg = std::make_unique<messages::RoastSignatureShare>(
      Replica_id, Signature_share, Next_pre_signature_share
    );
    results.emplace_back(std::move(msg));
  }
  return results;
}

std::unique_ptr<Message> RoastSignatureShare::clone()
{
  std::unique_ptr<Message> msg = std::make_unique<RoastSignatureShare>(*this);
  return msg;
}

const std::string RoastSignatureShare::digest() const
{
  PlTerm Digest;
  PlTermv args(
    PlString((const char *) m_signature_share.c_str()),
    PlString((const char *) m_next_pre_signature_share.c_str()),
    PlTerm{(long) m_sender_id},
    Digest
  );

  int result = PlCall("digest_roast_signature_share", args);
  if (result)
  {
    string digest{(const char *) Digest};
    return digest;
  }
  else
  {
    string error_msg = str(
      boost::format("Unable to calculate digest of %1%")
        % identify()
    );
    throw(std::runtime_error(error_msg));
  }
}

bool RoastSignatureShare::equals(const Message& other) const
{
  if (typeid(*this) != typeid(other)) return false;
  auto typed_other = static_cast<const RoastSignatureShare&>(other);

  if (m_signature_share != typed_other.m_signature_share) return false;
  if (m_next_pre_signature_share != typed_other.m_next_pre_signature_share) return false;
  return Message::equals(other);
}

std::string RoastSignatureShare::identify() const
{
  return str(
    boost::format( "<%1%, Sig_share=%2%, Next_pre_sig_share=%3%, S=%4%>" )
      % name()
      % m_signature_share.substr(0,5)
      % m_next_pre_signature_share.substr(0,5)
      % m_sender_id
  );
}

std::string RoastSignatureShare::signature_share() const
{
  return m_signature_share;
}

std::string RoastSignatureShare::next_pre_signature_share() const
{
  return m_next_pre_signature_share;
}

// Serialization and deserialization

RoastSignatureShare::RoastSignatureShare(const Json::Value& root):
Message(root)
{
  m_signature_share = root["payload"]["signature_share"].asString();
  m_next_pre_signature_share = root["payload"]["next_pre_sig_share"].asString();
}

std::string RoastSignatureShare::ToBinBuffer() const
{
  Json::Value payload;
  payload["signature_share"] = m_signature_share;
  payload["next_pre_sig_share"] = m_next_pre_signature_share;
  return this->FinalizeJsonRoot(payload);
}

}
}
}
