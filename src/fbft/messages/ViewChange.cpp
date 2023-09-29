// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <streams.h>
#include <version.h>

#include "config/FbftConfig.h"

using namespace std;

namespace itcoin {
namespace fbft {
namespace messages {

ViewChange::ViewChange(uint32_t sender_id, uint32_t view, uint32_t hi, string c, view_change_prepared_t pi,
                       view_change_pre_prepared_t qi)
    : Message(NODE_TYPE::REPLICA, sender_id) {
  m_view = view;
  m_hi = hi;
  m_c = c;
  m_pi = pi;
  m_qi = qi;
}

ViewChange::ViewChange(PlTerm Sender_id, PlTerm V, PlTerm Hi, PlTerm C, PlTerm Pi, PlTerm Qi)
    : Message(NODE_TYPE::REPLICA, Sender_id) {
  m_view = (long)V;
  m_hi = (long)Hi;

  // Parse value of C
  m_c = std::string{(const char*)C};

  PlTail P_tail{Pi};
  PlTerm P_elem;
  while (P_tail.next(P_elem)) {
    if (string("[|]").compare(P_elem.name()) != 0 || P_elem.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message P_elem term must have arity = 2");
    uint32_t p_n = (long)P_elem[1];
    PlTerm P_elem_2 = P_elem[2];

    if (string("[|]").compare(P_elem_2.name()) != 0 || P_elem_2.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message P_elem_2 term must have arity = 2");
    std::string p_req_digest{(char*)P_elem_2[1]};
    PlTerm P_elem_3 = P_elem_2[2];

    if (string("[|]").compare(P_elem_3.name()) != 0 || P_elem_3.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message P_elem_3 term must have arity = 2");
    uint32_t p_v = (long)P_elem_3[1];
    PlTerm P_elem_4 = P_elem_3[2];
    assert(P_elem_4.type() == PL_NIL); // Prolog lists end with empty list, the constant []

    m_pi.emplace_back(make_tuple(p_n, p_req_digest, p_v));
  }

  PlTail Q_tail{Qi};
  PlTerm Q_elem;
  while (Q_tail.next(Q_elem)) {
    if (string("[|]").compare(Q_elem.name()) != 0 || Q_elem.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message Q_elem term must have arity = 2");
    uint32_t q_n = (long)Q_elem[1];
    PlTerm Q_elem_2 = Q_elem[2];

    if (string("[|]").compare(Q_elem_2.name()) != 0 || Q_elem_2.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message Q_elem_2 term must have arity = 2");
    std::string q_req_digest{(char*)Q_elem_2[1]};
    PlTerm Q_elem_3 = Q_elem_2[2];

    if (string("[|]").compare(Q_elem_3.name()) != 0 || Q_elem_3.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message Q_elem_3 term must have arity = 2");
    std::string q_prep_block{(char*)Q_elem_3[1]};
    PlTerm Q_elem_4 = Q_elem_3[2];

    if (string("[|]").compare(Q_elem_4.name()) != 0 || Q_elem_4.arity() != 2)
      throw std::runtime_error("VIEW_CHANGE message Q_elem_4 term must have arity = 2");
    uint32_t q_v = (long)Q_elem_4[1];
    PlTerm Q_elem_5 = Q_elem_4[2]; // Prolog lists end with empty list, the constant []
    assert(Q_elem_5.type() == PL_NIL);

    m_qi.emplace_back(make_tuple(q_n, q_req_digest, q_prep_block, q_v));
  }
}

ViewChange::~ViewChange(){};

std::vector<std::unique_ptr<messages::ViewChange>> ViewChange::BuildToBeSent(uint32_t replica_id) {
  std::vector<unique_ptr<messages::ViewChange>> results{};
  PlTerm Replica_id{(long)replica_id}, V, Hi, C, Pi, Qi;
  PlQuery query("msg_out_view_change", PlTermv(Replica_id, V, Hi, C, Pi, Qi));
  while (query.next_solution()) {
    std::unique_ptr<messages::ViewChange> msg =
        std::make_unique<messages::ViewChange>(Replica_id, V, Hi, C, Pi, Qi);
    results.emplace_back(std::move(msg));
  }
  return results;
}

messages::ViewChange ViewChange::FindByDigest(uint32_t replica_id, uint32_t sender_id, std::string digest) {
  PlString Vc_digest{(const char*)digest.c_str()};
  PlTerm Replica_id{(long)replica_id}, V, Hi, C, Pi, Qi, Sender_id{(long)sender_id}, Sender_sig;
  int result =
      PlCall("msg_log_view_change", PlTermv(Replica_id, Vc_digest, V, Hi, C, Pi, Qi, Sender_id, Sender_sig));
  if (result) {
    messages::ViewChange msg{Sender_id, V, Hi, C, Pi, Qi};
    if ((long)Sender_id != replica_id) {
      string sender_sig{(const char*)Sender_sig};
      msg.set_signature(sender_sig);
    }
    return msg;
  } else {
    string error_msg = str(boost::format("Unable to find VIEW_CHANGE with digest %1%") % digest);
    throw(std::runtime_error(error_msg));
  }
}

bool ViewChange::equals(const Message& other) const {
  if (typeid(*this) != typeid(other))
    return false;
  auto typed_other = static_cast<const ViewChange&>(other);

  if (m_view != typed_other.m_view)
    return false;
  if (m_hi != typed_other.m_hi)
    return false;
  if (m_c != typed_other.m_c)
    return false;
  if (m_pi != typed_other.m_pi)
    return false;
  if (m_qi != typed_other.m_qi)
    return false;
  return Message::equals(other);
}

std::unique_ptr<Message> ViewChange::clone() {
  std::unique_ptr<Message> msg = std::make_unique<ViewChange>(*this);
  return msg;
}

PlTerm ViewChange::pi_as_plterm() const {
  PlTerm result;
  PlTail Pi_tail(result);
  for (messages::view_change_prepared_elem_t elem : m_pi) {
    uint32_t n = get<0>(elem);
    string digest = get<1>(elem);
    uint32_t v = get<2>(elem);
    Pi_tail.append(PlCompound(
        "[|]",
        PlTermv(PlTerm((long)n),
                PlCompound("[|]", PlTermv(PlString((const char*)digest.c_str()),
                                          PlCompound("[|]", PlTermv(PlTerm((long)v), PlCompound("[]"))))))));
  }
  Pi_tail.close();
  return result;
}

PlTerm ViewChange::qi_as_plterm() const {
  PlTerm result;
  PlTail Qi_tail(result);
  for (messages::view_change_pre_prepared_elem_t elem : m_qi) {
    uint32_t n = get<0>(elem);
    string digest = get<1>(elem);
    string prep_block = get<2>(elem);
    uint32_t v = get<3>(elem);
    Qi_tail.append(PlCompound(
        "[|]",
        PlTermv(
            PlTerm((long)n),
            PlCompound("[|]",
                       PlTermv(PlString((const char*)digest.c_str()),
                               PlCompound("[|]", PlTermv(PlString((const char*)prep_block.c_str()),
                                                         PlCompound("[|]", PlTermv(PlTerm((long)v),
                                                                                   PlCompound("[]"))))))))));
  }
  Qi_tail.close();
  return result;
}

const std::string ViewChange::digest() const {
  PlTerm Digest;
  PlTermv args(PlTerm((long)m_view), PlTerm((long)m_hi), PlString((const char*)m_c.c_str()),
               this->pi_as_plterm(), this->qi_as_plterm(), PlTerm((long)m_sender_id), Digest);

  int result = PlCall("digest_view_change", args);
  if (result) {
    string digest{(const char*)Digest};
    return digest;
  } else {
    string error_msg = str(boost::format("Unable to calculate VIEW_CHANGE digest"));
    throw(std::runtime_error(error_msg));
  }
}

std::string ViewChange::identify() const {
  return str(boost::format("<%1%, V=%2%, Hi=%3%, S=%4%>") % name() % m_view % m_hi % m_sender_id);
}

// Serialization and deserialization

ViewChange::ViewChange(const Json::Value& root) : Message(root) {
  m_view = root["payload"]["v"].asUInt();
  m_hi = root["payload"]["hi"].asUInt();
  m_c = root["payload"]["c"].asString();

  const Json::Value pi = root["payload"]["pi"];
  m_pi.reserve(pi.size());
  for (Json::Value pi_elem_json : pi) {
    uint32_t n = pi_elem_json["n"].asUInt();
    string req_digest = pi_elem_json["req_digest"].asString();
    uint32_t v = pi_elem_json["v"].asUInt();
    view_change_prepared_elem_t pi_elem = make_tuple(n, req_digest, v);
    m_pi.emplace_back(pi_elem);
  }

  const Json::Value qi = root["payload"]["qi"];
  m_qi.reserve(qi.size());
  std::transform(qi.begin(), qi.end(), std::back_inserter(m_qi), [](const auto& e) {
    uint32_t n = e["n"].asUInt();
    string req_digest = e["req_digest"].asString();
    string data = e["data"].asString();
    uint32_t v = e["v"].asUInt();
    view_change_pre_prepared_elem_t qi_elem = make_tuple(n, req_digest, data, v);
    return qi_elem;
  });
}

std::string ViewChange::ToBinBuffer() const {
  Json::Value payload;
  payload["v"] = m_view;
  payload["hi"] = m_hi;
  payload["c"] = m_c;

  Json::Value pi;
  for (view_change_prepared_elem_t m_pi_elem : m_pi) {
    Json::Value pi_elem;
    pi_elem["n"] = get<0>(m_pi_elem);
    pi_elem["req_digest"] = get<1>(m_pi_elem);
    pi_elem["v"] = get<2>(m_pi_elem);
    pi.append(pi_elem);
  }
  payload["pi"] = pi;

  Json::Value qi;
  for (view_change_pre_prepared_elem_t m_qi_elem : m_qi) {
    Json::Value qi_elem;
    qi_elem["n"] = get<0>(m_qi_elem);
    qi_elem["req_digest"] = get<1>(m_qi_elem);
    qi_elem["data"] = get<2>(m_qi_elem);
    qi_elem["v"] = get<3>(m_qi_elem);
    qi.append(qi_elem);
  }
  payload["qi"] = qi;

  return this->FinalizeJsonRoot(payload);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
