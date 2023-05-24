#include "actions.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
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

SendCommit::SendCommit(Wallet& wallet,
  PlTerm Replica_id, PlTerm Req_digest, PlTerm V, PlTerm N)
:Action(Replica_id), m_wallet(wallet)
{
  m_view = (long) V;
  m_seq_number = (long) N;

  // Retrieve payload from the msg store
  std::string req_digest = (const char*) Req_digest;
  m_request = Request::FindByDigest(m_replica_id, req_digest);
}

std::vector<std::unique_ptr<actions::SendCommit>> SendCommit::BuildActives(
  const itcoin::PbftConfig& config,
  Blockchain& blockchain,
  Wallet& wallet
)
{
  std::vector<unique_ptr<actions::SendCommit>> results{};
  PlTerm Req_digest, V, N, Replica_id{(long) config.id()};
  PlQuery query("pre_SEND_COMMIT", PlTermv(Req_digest, V, N, Replica_id));
  while ( query.next_solution() )
  {
    std::unique_ptr<actions::SendCommit> p_action =
      std::make_unique<actions::SendCommit>(wallet, Replica_id, Req_digest, V, N);
    results.emplace_back(move(p_action));
  }
  return results;
}

int SendCommit::effect() const
{
  // We need to find out the block to sing
  PrePrepare ppp = PrePrepare::FindByV_N_Req(m_replica_id, m_view, m_seq_number, m_request.digest());

  // We sign the block
  std::string block_signature = m_wallet.GetBlockSignature(ppp.proposed_block());

  BOOST_LOG_TRIVIAL(debug) << str(
    boost::format( "%1% effect(), signature that will be sent by R%2% = %3%" )
      % this->identify()
      % m_replica_id
      % block_signature.substr(0,5)
  );

  // We build the commit to send
  Commit msg{ m_replica_id,
    m_view, m_seq_number, block_signature
  };

  PlTermv args(
    PlTerm((long) m_view),
    PlTerm((long) m_seq_number),
    PlString((const char*) msg.block_signature().c_str()),
    PlTerm((long) m_replica_id)
  );
  return PlCall("effect_SEND_COMMIT", args);
}

std::string SendCommit::identify() const
{
  return str(
    boost::format( "<%1%, Req=%2%, V=%3%, N=%4%, R=%5%>" )
      % name()
      % m_request.digest()
      % m_view
      % m_seq_number
      % m_replica_id
  );
}

}
}
}
