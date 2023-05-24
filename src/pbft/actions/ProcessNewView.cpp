#include "actions.h"

#include <boost/format.hpp>
#include <SWI-cpp.h>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"
#include "../../PbftConfig.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;
using namespace itcoin::pbft::messages;

namespace itcoin {
namespace pbft {
namespace actions {

ProcessNewView::ProcessNewView(PlTerm Replica_id, PlTerm Hi, PlTerm Nu, PlTerm Chi)
:Action(Replica_id)
{
  m_hi = (long) Hi;
  m_nu = NewView::nu_from_plterm(Nu);
  m_chi = NewView::chi_from_plterm(Chi);
};

std::vector<std::unique_ptr<actions::ProcessNewView>> ProcessNewView::BuildActives(
  const itcoin::PbftConfig& config,
  Blockchain& blockchain,
  Wallet& wallet
)
{
  std::vector<unique_ptr<actions::ProcessNewView>> results{};
  PlTerm Hi, Nu, Chi, Replica_id{(long) config.id()};
  PlQuery query("pre_PROCESS_NEW_VIEW", PlTermv(Hi, Nu, Chi, Replica_id));
  while ( query.next_solution() )
  {
    unique_ptr<actions::ProcessNewView> action =
      std::make_unique<actions::ProcessNewView>(Replica_id, Hi, Nu, Chi);
    results.emplace_back(std::move(action));
  }
  return results;
}

int ProcessNewView::effect() const
{
  PlTermv args(
    PlTerm((long) m_hi),
    NewView::chi_as_plterm(m_chi),
    PlTerm((long) m_replica_id)
  );

  return PlCall("effect_PROCESS_NEW_VIEW", args);
}

std::string ProcessNewView::identify() const
{
  return str(
    boost::format( "<%1%, Hi=%2%, Nu=%3%, Chi=%4%, R=%5%>" )
      % name()
      % m_hi
      % "m_nu"
      % "m_chi"
      % m_replica_id
  );
}

}
}
}
