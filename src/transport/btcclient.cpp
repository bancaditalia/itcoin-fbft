// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "btcclient.h"

namespace itcoin {
namespace transport {

BtcClient::BtcClient(const std::string itcoinJsonRpcUri):
  httpClient(itcoinJsonRpcUri),
  bitcoind(httpClient, jsonrpc::JSONRPC_CLIENT_V1)
{
} // BtcClient::BtcClient()

std::string BtcClient::sendtoaddress(const std::string& address, int amount)
{
  std::scoped_lock lock(this->mtx);
  return this->bitcoind.sendtoaddress(address, amount, "comment", "comment_to", false, true, "null", "unset", false, 25);
}

std::string BtcClient::signmessage(const std::string& address, const std::string& message)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.signmessage(address, message);
} // BtcClient::signmessage()

bool BtcClient::verifymessage(const std::string& address, const std::string& signature, const std::string& message)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.verifymessage(address, signature, message);
} // BtcClient::verifymessage()

Json::Value BtcClient::getblockchaininfo()
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.getblockchaininfo();
} // BtcClient::getblockchaininfo()

Json::Value BtcClient::getaddressinfo(const std::string& address)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.getaddressinfo(address);
} // BtcClient::getaddressinfo()

Json::Value BtcClient::getblocktemplate(const Json::Value& templateRequest)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.getblocktemplate(templateRequest);
} // BtcClient::getblocktemplate()

Json::Value BtcClient::submitblock(const std::string &hexData)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.submitblock(hexData);
} // BtcClient::getblocktemplate()

Json::Value BtcClient::testblockvalidity(const std::string &hexData, bool check_signet_solution)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.testblockvalidity(hexData, check_signet_solution);
} // BtcClient::testblockvalidity()

Json::Value BtcClient::walletprocesspsbt(const std::string& psbt)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.walletprocesspsbt(psbt, true, "ALL");
} // BtcClient::walletprocesspsbt()

std::string BtcClient::combinepsbt(const Json::Value& psbts)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.combinepsbt(psbts);
} // BtcClient::combinepsbt()

Json::Value BtcClient::finalizepsbt(const std::string& psbt, bool extract)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.finalizepsbt(psbt, extract);
} // BtcClient::finalizepsbt()

Json::Value BtcClient::analyzepsbt(const std::string& psbt)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.analyzepsbt(psbt);
} // BtcClient::analyzepsbt()

std::string BtcClient::dumpprivkey(const std::string& address)
{
  std::scoped_lock lock(this->mtx);

  return this->bitcoind.dumpprivkey(address);
} // BtcClient::dumpprivkey()

} // namespace transport
} // namespace itcoin
