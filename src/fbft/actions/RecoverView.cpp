// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <SWI-cpp.h>

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

RecoverView::RecoverView(uint32_t replica_id, uint32_t view):
Action(replica_id), m_view(view)
{

}

RecoverView::~RecoverView()
{

}

std::vector<std::unique_ptr<actions::RecoverView>> RecoverView::BuildActives(
  const itcoin::FbftConfig& config,
  Blockchain& blockchain,
  RoastWallet& wallet
)
{
  std::vector<unique_ptr<actions::RecoverView>> results{};
  PlTerm V, Replica_id{(long) config.id()};
  PlQuery query("pre_RECOVER_VIEW", PlTermv(Replica_id, V));
  while ( query.next_solution() )
  {
    uint32_t v = (long) V;
    std::unique_ptr<actions::RecoverView> action =
      std::make_unique<actions::RecoverView>(config.id(), v);
    results.emplace_back(std::move(action));
  }
  return results;
}

int RecoverView::effect() const
{
  PlTermv args(
    PlTerm((long) m_replica_id),
    PlTerm((long) m_view)
  );
  return PlCall("effect_RECOVER_VIEW", args);
}

std::string RecoverView::identify() const
{
  return str(
    boost::format( "<%1%, V=%2%, R=%3%>" )
      % name()
      % m_view
      % m_replica_id
  );
}

}
}
}
