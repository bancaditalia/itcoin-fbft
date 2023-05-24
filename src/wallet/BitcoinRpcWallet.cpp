// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "wallet.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "config/FbftConfig.h"
#include "../blockchain/extract.h"
#include "../fbft/messages/messages.h"
#include "../transport/btcclient.h"

using namespace std;
using Message = itcoin::fbft::messages::Message;

namespace itcoin {
namespace wallet {

BitcoinRpcWallet::BitcoinRpcWallet(const itcoin::FbftConfig& conf, transport::BtcClient& bitcoind):
Wallet(conf), m_bitcoind(bitcoind)
{
  m_pubkey_address = m_conf.replica_set_v().at(m_conf.id()).p2pkh();
  BOOST_LOG_TRIVIAL(debug) << str(
    boost::format("R%1% BitcoinRpcWallet will sign using pubkey address %2%.")
      % m_conf.id()
      % m_pubkey_address
  );
}

void BitcoinRpcWallet::AppendSignature(Message& message) const
{
  if(message.sender_id() != m_conf.id())
  {
    string error_msg = str(
      boost::format("R%1% BitcoinRpcWallet cannot sign message with sender_id = %2%.")
        % m_conf.id()
        % message.sender_id()
    );
    throw runtime_error(error_msg);
  }

  string msg_digest = message.digest();
  string sig = m_bitcoind.signmessage(m_pubkey_address, msg_digest);

  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% BitcoinRpcWallet signing message with digest = %2%.")
      % m_conf.id()
      % msg_digest
  );
  message.set_signature(sig);
}

bool BitcoinRpcWallet::VerifySignature(const Message& message) const
{
  string msg_digest = message.digest();
  string msg_sig = message.signature();
  string msg_pubkey_address =
    m_conf.replica_set_v().at(message.sender_id()).p2pkh();

  return m_bitcoind.verifymessage(
    msg_pubkey_address,
    msg_sig,
    msg_digest
  );
}

}
}
