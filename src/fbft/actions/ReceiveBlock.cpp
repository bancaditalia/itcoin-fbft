// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <SWI-cpp.h>

namespace itcoin {
namespace fbft {
namespace actions {

int ReceiveBlock::effect() const
{
  PlTermv args(
    PlTerm((long) m_replica_id),
    PlTerm((long) m_msg.block_height()),
    PlTerm((long) m_msg.block_time()),
    PlString((const char*) m_msg.block_hash().c_str())
  );
  return PlCall("effect_RECEIVE_BLOCK", args);
}

std::string ReceiveBlock::identify() const
{
  return str(
    boost::format( "<%1%, H=%2%, R=%3%>" )
      % name()
      % m_msg.block_height()
      % m_replica_id
  );
}

}
}
}
