// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <SWI-cpp.h>

namespace messages=itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

int ReceiveRequest::effect() const
{
  PlTermv args(
    PlString((const char*) m_msg.digest().c_str()),
    PlTerm((long) m_msg.timestamp()),
    PlTerm((long) m_replica_id)
  );

  return PlCall("effect_RECEIVE_REQUEST", args);
}

std::string ReceiveRequest::identify() const
{
  return str(
    boost::format( "<%1%, T=%2%, H=%3%, R=%4%>" )
      % name()
      % m_msg.timestamp()
      % m_msg.height()
      % m_replica_id
  );
}

}
}
}
