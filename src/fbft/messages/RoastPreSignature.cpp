// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <streams.h>
#include <version.h>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"

#include "config/FbftConfig.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;

namespace itcoin {
namespace fbft {
namespace messages {

RoastPreSignature::RoastPreSignature(uint32_t sender_id, vector<uint32_t> signers, string pre_signature)
    : Message(NODE_TYPE::REPLICA, sender_id) {
  m_signers = signers;
  m_pre_signature = pre_signature;
}

RoastPreSignature::RoastPreSignature(PlTerm Sender_id, PlTerm Signers, PlTerm Pre_signature)
    : Message(NODE_TYPE::REPLICA, Sender_id) {
  PlTail SignersTail{Signers};
  PlTerm SignerElem;
  while (SignersTail.next(SignerElem)) {
    m_signers.emplace_back((long)SignerElem);
  }
  m_pre_signature = std::string{(const char*)Pre_signature};
}

RoastPreSignature::~RoastPreSignature() {}

std::vector<std::unique_ptr<messages::RoastPreSignature>>
RoastPreSignature::BuildToBeSent(uint32_t replica_id) {
  std::vector<unique_ptr<messages::RoastPreSignature>> results{};
  PlTerm Replica_id{(long)replica_id}, Signers, Pre_signature;
  PlQuery query("msg_out_roast_pre_signature", PlTermv(Replica_id, Signers, Pre_signature));
  while (query.next_solution()) {
    std::unique_ptr<messages::RoastPreSignature> msg =
        std::make_unique<messages::RoastPreSignature>(Replica_id, Signers, Pre_signature);
    results.emplace_back(std::move(msg));
  }
  return results;
}

std::unique_ptr<Message> RoastPreSignature::clone() {
  std::unique_ptr<Message> msg = std::make_unique<RoastPreSignature>(*this);
  return msg;
}

const std::string RoastPreSignature::digest() const {
  PlTerm Digest;
  PlTermv args(signers_as_plterm(), PlString((const char*)m_pre_signature.c_str()), PlTerm{(long)m_sender_id},
               Digest);

  int result = PlCall("digest_roast_pre_signature", args);
  if (result) {
    string digest{(const char*)Digest};
    return digest;
  } else {
    string error_msg = str(boost::format("Unable to calculate digest of %1%") % identify());
    throw(std::runtime_error(error_msg));
  }
}

bool RoastPreSignature::equals(const Message& other) const {
  if (typeid(*this) != typeid(other))
    return false;
  auto typed_other = static_cast<const RoastPreSignature&>(other);

  if (m_signers != typed_other.m_signers)
    return false;
  if (m_pre_signature != typed_other.m_pre_signature)
    return false;
  return Message::equals(other);
}

std::string RoastPreSignature::identify() const {
  std::stringstream signers_stream;
  std::copy(m_signers.begin(), m_signers.end(), std::ostream_iterator<int>(signers_stream, ","));
  string signers_str = signers_stream.str();
  signers_str.pop_back();

  return str(boost::format("<%1%, Signers=[%2%], Pre_sig=%3%, S=%4%>") % name() % signers_str %
             m_pre_signature.substr(0, 5) % m_sender_id);
}

std::string RoastPreSignature::pre_signature() const { return m_pre_signature; }

std::vector<uint32_t> RoastPreSignature::signers() const { return m_signers; }

PlTerm RoastPreSignature::signers_as_plterm() const {
  PlTerm result;
  PlTail Pi_tail(result);
  for (uint32_t s : m_signers) {
    Pi_tail.append(PlTerm((long)s));
  }
  Pi_tail.close();
  return result;
}

// Serialization and deserialization

RoastPreSignature::RoastPreSignature(const Json::Value& root) : Message(root) {
  m_pre_signature = root["payload"]["pre_signature"].asString();
  const Json::Value signers = root["payload"]["signers"];
  m_signers.reserve(signers.size());
  for (Json::Value signr_elem_json : signers) {
    uint32_t signr = signr_elem_json.asUInt();
    m_signers.emplace_back(signr);
  }
}

std::string RoastPreSignature::ToBinBuffer() const {
  Json::Value payload;
  payload["pre_signature"] = m_pre_signature;
  Json::Value signers;
  for (uint32_t signr : m_signers) {
    signers.append(signr);
  }
  payload["signers"] = signers;
  return this->FinalizeJsonRoot(payload);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
