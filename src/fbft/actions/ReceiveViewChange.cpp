// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <SWI-cpp.h>
#include <boost/format.hpp>

using namespace std;

namespace messages = itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

int ReceiveViewChange::effect() const {
  PlTermv args(PlTerm((long)m_msg.view()), PlTerm((long)m_msg.hi()), PlString((const char*)m_msg.c().c_str()),
               m_msg.pi_as_plterm(), m_msg.qi_as_plterm(), PlTerm((long)m_msg.sender_id()),
               PlString((const char*)m_msg.signature().c_str()), PlTerm((long)m_replica_id));

  return PlCall("effect_RECEIVE_VIEW_CHANGE", args);
}

std::string ReceiveViewChange::identify() const {
  return str(boost::format("<%1%, S=%2%, V=%3%, R=%4%>") % name() % m_msg.sender_id() % m_msg.view() %
             m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
