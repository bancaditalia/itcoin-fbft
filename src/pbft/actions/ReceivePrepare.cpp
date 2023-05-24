#include "actions.h"

#include <boost/format.hpp>
#include <SWI-cpp.h>

namespace itcoin {
namespace pbft {
namespace actions {

int ReceivePrepare::effect() const
{
  PlTermv args(
    PlTerm((long) m_msg.view()),
    PlTerm((long) m_msg.seq_number()),
    PlString((const char*) m_msg.req_digest().c_str()),
    PlTerm((long) m_msg.sender_id()),
    PlString((const char*) m_msg.signature().c_str()),
    PlTerm((long) m_replica_id)
  );

  return PlCall("effect_RECEIVE_PREPARE", args);
}

std::string ReceivePrepare::identify() const
{
  return str(
    boost::format( "<%1%, R=%2%>" )
      % name()
      % m_replica_id
  );
}

}
}
}
