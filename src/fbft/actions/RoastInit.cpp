// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "config/FbftConfig.h"

using namespace std;
using namespace itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

RoastInit::RoastInit(PlTerm Replica_id, PlTerm Req_digest, PlTerm V, PlTerm N):
Action(Replica_id)
{
  // Retrieve payload from the msg store
  std::string req_digest = (char*) Req_digest;
  m_request = Request::FindByDigest(m_replica_id, req_digest);

  m_view = (long) V;

  m_seq_number = (long) N;
}

RoastInit::~RoastInit()
{

}

std::string RoastInit::identify() const
{
  return str(
    boost::format( "<%1%, Request=%2%, V=%3%, N=%4%, R=%5%>" )
      % name()
      % m_request.digest()
      % m_view
      % m_seq_number
      % m_replica_id
  );
}

int RoastInit::effect() const
{
  PlTermv args(
    PlTerm((long) m_replica_id),
    PlTerm((long) m_view),
    PlTerm((long) m_seq_number),
    PlString((const char*) m_request.digest().c_str())
  );
  return PlCall("effect_ROAST_INIT", args);
}

std::vector<std::unique_ptr<actions::RoastInit>> RoastInit::BuildActives(
  const itcoin::FbftConfig& config,
  blockchain::Blockchain& blockchain,
  wallet::RoastWallet& wallet
)
{
  std::vector<unique_ptr<actions::RoastInit>> results{};
  PlTerm Req_digest, V, N, Replica_id{(long) config.id()};
  PlQuery query("pre_ROAST_INIT", PlTermv(Replica_id, Req_digest, V, N));
  while ( query.next_solution() )
  {
    actions::RoastInit action{Replica_id, Req_digest, V, N};
    std::unique_ptr<actions::RoastInit> p_action = std::make_unique<actions::RoastInit>(action);
    results.emplace_back(move(p_action));
  }
  return results;
}

}
}
}
