#include "wallet.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../PbftConfig.h"
#include "../block/extract.h"
#include "../block/psbt_utils.h"
#include "../pbft/messages/messages.h"
#include "../transport/btcclient.h"

using namespace std;
using Message = itcoin::pbft::messages::Message;

namespace itcoin {
namespace wallet {

BitcoinRpcWallet::BitcoinRpcWallet(const itcoin::PbftConfig& conf, transport::BtcClient& bitcoind):
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

std::string BitcoinRpcWallet::GetBlockSignature(const CBlock& block)
{
  const std::string psbt = block::createPsbt(block, m_conf.getSignetChallenge());
  const auto [ psbt0, isComplete0 ] = block::signPsbt(m_bitcoind, psbt);
  return psbt0;
}

CBlock BitcoinRpcWallet::FinalizeBlock(const CBlock& block,
  const std::vector<std::string> signatures) const
{
  Json::Value psbt_list;

  for (string sig: signatures)
  {
    psbt_list.append(sig);

  }

  string combinedPsbt = m_bitcoind.combinepsbt(psbt_list);

  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% BitcoinRpcWallet combined PSBT = %2%.")
      % m_conf.id()
      % combinedPsbt
  );

  auto finalizeResponse = m_bitcoind.finalizepsbt(combinedPsbt, false);

  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% BitcoinRpcWallet finalized PSBT = %2%.")
      % m_conf.id()
      % finalizeResponse.toStyledString()
  );

  auto finalizedPsbt = finalizeResponse["psbt"].asString();
  auto isComplete = finalizeResponse["complete"].asBool();
  if (!isComplete) {
    throw std::runtime_error("R%1% BitcoinRpcWallet cannot complete the PSBT");
  }

  CBlock signed_block = block::extractBlock(m_bitcoind, finalizedPsbt);

  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% BitcoinRpcWallet succesfully extracted signed block from finalized PSBT.")
      % m_conf.id()
  );

  return signed_block;
}

}
}
