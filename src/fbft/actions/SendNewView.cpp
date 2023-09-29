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

std::vector<std::unique_ptr<actions::SendNewView>>
SendNewView::BuildActives(const itcoin::FbftConfig& config, Blockchain& blockchain, RoastWallet& wallet) {
  std::vector<unique_ptr<actions::SendNewView>> results{};
  PlTerm Nu, Chi, Replica_id{(long)config.id()};
  PlQuery query("pre_SEND_NEW_VIEW", PlTermv(Nu, Chi, Replica_id));
  while (query.next_solution()) {
    auto nu = NewView::nu_from_plterm(Nu);
    auto chi = NewView::chi_from_plterm(Chi);

    unique_ptr<SendNewView> action = std::make_unique<SendNewView>(config.id(), nu, chi);
    results.emplace_back(std::move(action));
  }
  return results;
}

int SendNewView::effect() const {
  PlTermv args(NewView::nu_as_plterm(m_nu),   // Nu
               NewView::chi_as_plterm(m_chi), // Chi
               PlTerm((long)m_replica_id));

  return PlCall("effect_SEND_NEW_VIEW", args);
}

std::string SendNewView::identify() const {
  return str(boost::format("<%1%, R=%2%>") % name() % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
