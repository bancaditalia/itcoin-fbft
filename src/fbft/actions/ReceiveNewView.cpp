// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <SWI-cpp.h>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;
using namespace itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

ReceiveNewView::ReceiveNewView(RoastWallet& wallet, uint32_t replica_id, messages::NewView msg):
Action(replica_id),
m_wallet(wallet),
m_msg(msg)
{
};

int ReceiveNewView::effect() const
{

  PlTermv args(
    PlTerm((long) m_msg.view()),
    NewView::nu_as_plterm(m_msg.nu()),
    NewView::chi_as_plterm(m_msg.chi()),
    PlTerm((long) m_msg.sender_id()),
    PlString((const char*) m_msg.signature().c_str()),
    PlTerm((long) m_replica_id)
  );

  return PlCall("effect_RECEIVE_NEW_VIEW", args);
}

std::string ReceiveNewView::identify() const
{
  return str(
    boost::format( "<%1%, V=%2%, Sender=%3%, R=%4%>" )
      % name()
      % m_msg.view()
      % m_msg.sender_id()
      % m_replica_id
  );
}

}
}
}
