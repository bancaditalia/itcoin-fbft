// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <SWI-cpp.h>

namespace itcoin {
namespace fbft {
namespace actions {

int RoastReceiveSignatureShare::effect() const
{
  PlTermv args(
    PlTerm((long) m_replica_id),
    PlTerm((long) m_msg.sender_id()),
    PlString((const char*) m_msg.signature_share().c_str()),
    PlString((const char*) m_msg.next_pre_signature_share().c_str())
  );
  return PlCall("effect_RECEIVE_SIG_SHARE", args);
}

std::string RoastReceiveSignatureShare::identify() const
{
  return str(
    boost::format( "<%1%, msg=%2%, R=%3%>" )
      % name()
      % m_msg.identify()
      % m_replica_id
  );
}

}
}
}
