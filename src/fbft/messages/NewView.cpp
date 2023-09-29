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

NewView::NewView(uint32_t sender_id, uint32_t view, std::vector<ViewChange> vc_messages,
                 std::vector<PrePrepare> ppp_messages)
    : Message(NODE_TYPE::REPLICA, sender_id) {
  m_view = view;
  m_vc_messages = vc_messages;
  m_ppp_messages = ppp_messages;
}

NewView::NewView(PlTerm Sender_id, PlTerm V, PlTerm Nu, PlTerm Chi) : Message(NODE_TYPE::REPLICA, Sender_id) {
  m_view = (long)V;

  // nu is an array of <replica_id, view change msg digest>
  // when this constructor is used, we assume the replica is the primary
  // and the view change messages are in the log and we can retrieve them
  new_view_nu_t nu = NewView::nu_from_plterm(Nu);
  for (new_view_nu_elem_t& nu_elem : nu) {
    uint32_t nu_n = get<0>(nu_elem);
    std::string nu_digest{get<1>(nu_elem)};
    ViewChange vc = ViewChange::FindByDigest(m_sender_id, nu_n, nu_digest);
    m_vc_messages.emplace_back(vc);
  }

  new_view_chi_t chi = NewView::chi_from_plterm(Chi);
  for (new_view_chi_elem_t& chi_elem : chi) {
    uint32_t chi_n = get<0>(chi_elem);
    std::string chi_digest{get<1>(chi_elem)};
    std::string chi_block{get<2>(chi_elem)};

    // Further elements of chi propagate prepared requests from one view to the other
    PrePrepare ppp =
        PrePrepare(PlTerm((long)m_sender_id), PlTerm((long)m_view), PlTerm((long)chi_n),
                   PlString((const char*)chi_digest.c_str()), PlString((const char*)chi_block.c_str()));
    m_ppp_messages.emplace_back(ppp);
  }
}

NewView::~NewView(){};

std::vector<std::unique_ptr<messages::NewView>> NewView::BuildToBeSent(uint32_t replica_id) {
  std::vector<unique_ptr<messages::NewView>> results{};
  PlTerm Replica_id{(long)replica_id}, V, Nu, Chi;
  PlQuery query("msg_out_new_view", PlTermv(Replica_id, V, Nu, Chi));
  while (query.next_solution()) {
    std::unique_ptr<messages::NewView> msg = std::make_unique<messages::NewView>(Replica_id, V, Nu, Chi);
    results.emplace_back(std::move(msg));
  }
  return results;
}

new_view_nu_t NewView::nu_from_plterm(PlTerm Nu) {
  new_view_nu_t nu{};
  PlTail Nu_tail{Nu};
  PlTerm Nu_elem;
  while (Nu_tail.next(Nu_elem)) {
    if (string("[|]").compare(Nu_elem.name()) != 0 || Nu_elem.arity() != 2)
      throw std::runtime_error("NEW_VIEW Nu element term must have arity = 2");
    uint32_t nu_n = (long)Nu_elem[1];
    PlTerm Nu_elem_2 = Nu_elem[2];
    if (string("[|]").compare(Nu_elem_2.name()) != 0 || Nu_elem_2.arity() != 2)
      throw std::runtime_error("NEW_VIEW Nu_elem_2 term must have arity = 2");
    std::string nu_digest{(const char*)Nu_elem_2[1]};
    PlTerm Nu_elem_3 = Nu_elem_2[2]; // Prolog lists end with empty list, the constant []
    assert(Nu_elem_3.type() == PL_NIL);
    nu.emplace_back(make_tuple(nu_n, nu_digest));
  }
  return nu;
}

new_view_chi_t NewView::chi_from_plterm(PlTerm Chi) {
  new_view_chi_t chi{};
  PlTail Chi_tail{Chi};
  PlTerm Chi_elem;
  while (Chi_tail.next(Chi_elem)) {
    if (string("[|]").compare(Chi_elem.name()) != 0 || Chi_elem.arity() != 2)
      throw std::runtime_error("SEND_NEW_VIEW Chi_elem term must have arity = 2");
    uint32_t chi_n = (long)Chi_elem[1];
    PlTerm Chi_elem_2 = Chi_elem[2];

    if (string("[|]").compare(Chi_elem_2.name()) != 0 || Chi_elem_2.arity() != 2)
      throw std::runtime_error("SEND_NEW_VIEW Chi_elem_2 term must have arity = 2");
    std::string chi_digest{(const char*)Chi_elem_2[1]};
    PlTerm Chi_elem_3 = Chi_elem_2[2];

    if (string("[|]").compare(Chi_elem_3.name()) != 0 || Chi_elem_3.arity() != 2)
      throw std::runtime_error("SEND_NEW_VIEW Chi_elem_3 term must have arity = 2");
    std::string chi_block{(const char*)Chi_elem_3[1]};
    PlTerm Chi_elem_4 = Chi_elem_3[2]; // Prolog lists end with empty list, the constant []
    assert(Chi_elem_4.type() == PL_NIL);

    chi.emplace_back(make_tuple(chi_n, chi_digest, chi_block));
  }
  return chi;
}

PlTerm NewView::nu_as_plterm(new_view_nu_t nu) {
  PlTerm result;
  PlTail Nu_tail(result);
  for (new_view_nu_elem_t elem : nu) {
    uint32_t n = get<0>(elem);
    string digest = get<1>(elem);
    Nu_tail.append(PlCompound(
        "[|]", PlTermv(PlTerm((long)n),
                       PlCompound("[|]", PlTermv(PlString((const char*)digest.c_str()), PlCompound("[]"))))));
  }
  Nu_tail.close();
  return result;
}

PlTerm NewView::chi_as_plterm(new_view_chi_t chi) {
  PlTerm result;
  PlTail Chi_tail(result);
  for (new_view_chi_elem_t elem : chi) {
    uint32_t n = get<0>(elem);
    string digest = get<1>(elem);
    string prepared_block = get<2>(elem);
    Chi_tail.append(PlCompound(
        "[|]",
        PlTermv(
            PlTerm((long)n),
            PlCompound("[|]", PlTermv(PlString((const char*)digest.c_str()),
                                      PlCompound("[|]", PlTermv(PlString((const char*)prepared_block.c_str()),
                                                                PlCompound("[]"))))))));
  }
  Chi_tail.close();
  return result;
}

new_view_nu_t NewView::nu() const {
  new_view_nu_t result;
  for (messages::ViewChange elem : m_vc_messages) {
    uint32_t n = elem.sender_id();
    string digest = elem.digest();
    result.emplace_back(make_tuple(n, digest));
  }
  return result;
}

new_view_chi_t NewView::chi() const {
  new_view_chi_t result;
  for (messages::PrePrepare elem : m_ppp_messages) {
    uint32_t n = elem.seq_number();
    string digest = elem.req_digest();
    string prepared_block = elem.proposed_block_hex();
    result.emplace_back(make_tuple(n, digest, prepared_block));
  }
  return result;
}

void NewView::Sign(const RoastWallet& wallet) {
  // Sign the view change messages not having a signature
  for (auto& vc : m_vc_messages) {
    if (vc.sender_id() == m_sender_id) {
      wallet.AppendSignature(vc);
    }
  }

  wallet.AppendSignature(*this);
}

bool NewView::VerifySignatures(const RoastWallet& wallet) {
  bool result = true;
  for (const messages::ViewChange& vc : m_vc_messages) {
    bool vc_valid = wallet.VerifySignature(vc);
    if (!vc_valid) {
      BOOST_LOG_TRIVIAL(error) << "A received NEW_VIEW contains an invalid view change, and will be ignored!";
      return false;
    }
    result = result && vc_valid;
  }
  result = result && wallet.VerifySignature(*this);
  return result;
}

bool NewView::equals(const Message& other) const {
  if (typeid(*this) != typeid(other))
    return false;
  auto typed_other = static_cast<const NewView&>(other);

  if (m_view != typed_other.m_view)
    return false;
  if (m_vc_messages != typed_other.m_vc_messages)
    return false;
  if (m_ppp_messages != typed_other.m_ppp_messages)
    return false;
  return Message::equals(other);
}

std::unique_ptr<Message> NewView::clone() {
  std::unique_ptr<Message> msg = std::make_unique<NewView>(*this);
  return msg;
}

const std::string NewView::digest() const {
  PlTerm Digest;
  PlTermv args(PlTerm((long)m_view), NewView::nu_as_plterm(this->nu()), NewView::chi_as_plterm(this->chi()),
               Digest);

  int result = PlCall("digest_new_view", args);
  if (result) {
    string digest{(const char*)Digest};
    return digest;
  } else {
    string error_msg = str(boost::format("Unable to calculate NEW_VIEW digest"));
    throw(std::runtime_error(error_msg));
  }
}

std::string NewView::identify() const {
  return str(boost::format("<%1%, V=%2%, S=%3%>") % name() % m_view % m_sender_id);
}

// Serialization and deserialization

NewView::NewView(const Json::Value& root) : Message(root) {
  m_view = root["payload"]["v"].asUInt();
  const Json::Value nu = root["payload"]["nu"];
  for (Json::Value nu_elem_json : nu) {
    ViewChange vc_elem{nu_elem_json};
    m_vc_messages.emplace_back(vc_elem);
  }
  const Json::Value chi = root["payload"]["chi"];
  for (Json::Value chi_elem_json : chi) {
    PrePrepare ppp_elem{chi_elem_json};
    m_ppp_messages.emplace_back(ppp_elem);
  }
}

std::string NewView::ToBinBuffer() const {
  Json::Reader reader;

  Json::Value payload;
  payload["v"] = m_view;
  Json::Value nu;
  for (ViewChange m_vc_elem : m_vc_messages) {
    Json::Value nu_elem;
    reader.parse(m_vc_elem.ToBinBuffer(), nu_elem);
    nu.append(nu_elem);
  }
  payload["nu"] = nu;
  Json::Value chi;
  for (PrePrepare m_ppp_elem : m_ppp_messages) {
    Json::Value chi_elem;
    reader.parse(m_ppp_elem.ToBinBuffer(), chi_elem);
    chi.append(chi_elem);
  }
  payload["chi"] = chi;
  return this->FinalizeJsonRoot(payload);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
