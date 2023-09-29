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

SendPrepare::SendPrepare(PlTerm Replica_id, PlTerm Req_digest, PlTerm V, PlTerm N) : Action(Replica_id) {
  m_view = (long)V;
  m_seq_number = (long)N;

  // Retrieve payload from the msg store
  std::string req_digest = (char*)Req_digest;
  m_request = messages::Request::FindByDigest(m_replica_id, req_digest);
}

std::vector<std::unique_ptr<actions::SendPrepare>>
SendPrepare::BuildActives(const itcoin::FbftConfig& config, Blockchain& blockchain, RoastWallet& wallet) {
  std::vector<unique_ptr<actions::SendPrepare>> results{};
  PlTerm Req_digest, V, N, Replica_id{(long)config.id()};
  PlQuery query("pre_SEND_PREPARE", PlTermv(Req_digest, V, N, Replica_id));
  while (query.next_solution()) {
    std::unique_ptr<actions::SendPrepare> action =
        std::make_unique<actions::SendPrepare>(Replica_id, Req_digest, V, N);
    results.emplace_back(std::move(action));
  }
  return results;
}

int SendPrepare::effect() const {
  PlTermv args(PlString((const char*)m_request.digest().c_str()), PlTerm((long)m_view),
               PlTerm((long)m_seq_number), PlTerm((long)m_replica_id));
  return PlCall("effect_SEND_PREPARE", args);
}

std::string SendPrepare::identify() const {
  return str(boost::format("<%1%, Request=%2%, V=%3%, N=%4%, R=%5%>") % name() % m_request.digest() % m_view %
             m_seq_number % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
