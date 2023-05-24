// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <SWI-cpp.h>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"
#include "../state/state.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;
using namespace itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

ReceivePrePrepare::ReceivePrePrepare(uint32_t replica_id, Blockchain& blockchain,
  double current_time, double pre_prepare_time_tolerance_delta,
  PrePrepare msg)
:
Action(replica_id),
m_current_time(current_time),
m_pre_prepare_time_tolerance_delta(pre_prepare_time_tolerance_delta),
m_blockchain(blockchain),
m_msg(msg)
{

};

int ReceivePrePrepare::effect() const
{
  /*
   * Here we check that the block proposed by the primary is valid. We exclude
   * the signet solution from the check (hence the third parameter set to
   * "false").
   */
  if(!m_blockchain.TestBlockValidity(m_msg.seq_number(), m_msg.proposed_block(), false))
  {
    BOOST_LOG_TRIVIAL(error) << "A received PRE_PREPARE contains an invalid block, and will be ignored!";
    return 0;
  }

  // Check that the block has the expected timestamp
  messages::Request req;
  if (!messages::Request::TryFindByDigest(m_replica_id, m_msg.req_digest(), req))
  {
    BOOST_LOG_TRIVIAL(error) << "A received PRE_PREPARE references an unknown request, and will be ignored.";
    return 0;
  }

  if (!m_msg.proposed_block().nTime == req.timestamp())
  {
    BOOST_LOG_TRIVIAL(error) << "A received PRE_PREPARE has mismatching block and request timestamp, and will be ignored.";
    return 0;
  }

  if (req.timestamp() > m_current_time + m_pre_prepare_time_tolerance_delta)
  {
    string error_msg = str(
      boost::format("R%1% PRE_PREPARE received for a future request (request time = %2%) has been received too early (current time = %3%, max timestamp accepted = %4%), and will be ignored.")
        % m_replica_id
        % req.timestamp()
        % m_current_time
        % (m_current_time + m_pre_prepare_time_tolerance_delta)
    );
    BOOST_LOG_TRIVIAL(error) << error_msg;
    return 0;
  }

  PlTermv args(
    PlTerm((long) m_msg.view()),
    PlTerm((long) m_msg.seq_number()),
    PlString((const char*) m_msg.req_digest().c_str()),
    PlString((const char*) m_msg.proposed_block_hex().c_str() ),
    PlTerm((long) m_msg.sender_id()),
    PlString((const char*) m_msg.signature().c_str()),
    PlTerm((long) m_replica_id)
  );
  return PlCall("effect_RECEIVE_PRE_PREPARE", args);
}

std::string ReceivePrePrepare::identify() const
{
  return str(
    boost::format( "<%1%, V=%2%, N=%3%, R=%4%>" )
      % name()
      % m_msg.view()
      % m_msg.seq_number()
      % m_replica_id
  );
}

}
}
}
