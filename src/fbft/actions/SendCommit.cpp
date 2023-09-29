// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <SWI-cpp.h>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"
#include "config/FbftConfig.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;
using namespace itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

SendCommit::SendCommit(RoastWallet& wallet, PlTerm Replica_id, PlTerm Req_digest, PlTerm V, PlTerm N)
    : Action(Replica_id), m_wallet(wallet) {
  m_view = (long)V;
  m_seq_number = (long)N;

  // Retrieve payload from the msg store
  std::string req_digest = (const char*)Req_digest;
  m_request = Request::FindByDigest(m_replica_id, req_digest);
}

std::vector<std::unique_ptr<actions::SendCommit>>
SendCommit::BuildActives(const itcoin::FbftConfig& config, Blockchain& blockchain, RoastWallet& wallet) {
  std::vector<unique_ptr<actions::SendCommit>> results{};
  PlTerm Req_digest, V, N, Replica_id{(long)config.id()};
  PlQuery query("pre_SEND_COMMIT", PlTermv(Req_digest, V, N, Replica_id));
  while (query.next_solution()) {
    std::unique_ptr<actions::SendCommit> p_action =
        std::make_unique<actions::SendCommit>(wallet, Replica_id, Req_digest, V, N);
    results.emplace_back(move(p_action));
  }
  return results;
}

int SendCommit::effect() const {
  std::string pre_signature = m_wallet.GetPreSignatureShare();

  BOOST_LOG_TRIVIAL(debug) << str(
      boost::format("%1% effect(), pre_signature that will be sent by R%2% = %3%") % this->identify() %
      m_replica_id % pre_signature.substr(0, 5));

  // We build the commit to send
  Commit msg{m_replica_id, m_view, m_seq_number, pre_signature};

  PlTermv args(PlTerm((long)m_view), PlTerm((long)m_seq_number),
               PlString((const char*)msg.pre_signature().c_str()), PlTerm((long)m_replica_id));
  return PlCall("effect_SEND_COMMIT", args);
}

std::string SendCommit::identify() const {
  return str(boost::format("<%1%, Req=%2%, V=%3%, N=%4%, R=%5%>") % name() % m_request.digest() % m_view %
             m_seq_number % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
