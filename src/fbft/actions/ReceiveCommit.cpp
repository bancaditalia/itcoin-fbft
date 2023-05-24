// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <SWI-cpp.h>

namespace itcoin {
namespace fbft {
namespace actions {

int ReceiveCommit::effect() const
{
  PlTermv args(
    PlTerm((long) m_msg.view()),
    PlTerm((long) m_msg.seq_number()),
    PlString((const char*) m_msg.pre_signature().c_str()),
    PlTerm((long) m_msg.sender_id()),
    PlString((const char*) m_msg.signature().c_str()),
    PlTerm((long) m_replica_id)
  );

  return PlCall("effect_RECEIVE_COMMIT", args);
}

std::string ReceiveCommit::identify() const
{
  return str(
    boost::format( "<%1%, V=%2%, N=%3%, Data=%4% R=%5%>" )
      % name()
      % m_msg.view()
      % m_msg.seq_number()
      % m_msg.pre_signature().substr(0,5)
      % m_replica_id
  );
}

}
}
}
