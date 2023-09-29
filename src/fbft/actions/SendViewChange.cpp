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

std::vector<std::unique_ptr<actions::SendViewChange>>
SendViewChange::BuildActives(const itcoin::FbftConfig& config, Blockchain& blockchain, RoastWallet& wallet) {
  std::vector<unique_ptr<actions::SendViewChange>> results{};
  PlTerm V, Replica_id{(long)config.id()};
  PlQuery query("pre_SEND_VIEW_CHANGE", PlTermv(V, Replica_id));
  while (query.next_solution()) {
    uint32_t v = (long)V;
    std::unique_ptr<actions::SendViewChange> action =
        std::make_unique<actions::SendViewChange>(config.id(), v);
    results.emplace_back(std::move(action));
  }
  return results;
}

int SendViewChange::effect() const {
  PlTermv args(PlTerm((long)m_view), PlTerm((long)m_replica_id));
  return PlCall("effect_SEND_VIEW_CHANGE", args);
}

std::string SendViewChange::identify() const {
  return str(boost::format("<%1%, V=%2%, R=%3%>") % name() % m_view % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
