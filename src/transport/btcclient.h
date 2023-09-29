// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_TRANSPORT_BTCCLIENT_H
#define ITCOIN_TRANSPORT_BTCCLIENT_H

#include <mutex>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <generated/bitcoin_jsonrpc/BitcoinClientStub.h>

namespace itcoin {
namespace transport {

/**
 * This class is a thread-safe wrapper to BitcoinClientStub, the JSON-RPC client
 * used by the itcoin miner to communicate with itcoin-core.
 *
 * Its methods can be safely invoked because they are protected by a scoped_lock
 * that serializes calls via the embedded mtx mutex.
 */
class BtcClient {
public:
  BtcClient(const std::string itcoinJsonRpcUri);

  std::string sendtoaddress(const std::string& address, int amount);
  std::string signmessage(const std::string& address, const std::string& message);
  bool verifymessage(const std::string& address, const std::string& signature, const std::string& message);
  Json::Value getblockchaininfo();
  Json::Value getaddressinfo(const std::string& address);
  Json::Value getblocktemplate(const Json::Value& templateRequest);
  Json::Value submitblock(const std::string& hexData);
  Json::Value testblockvalidity(const std::string& hexData, bool check_signet_solution = true);
  Json::Value walletprocesspsbt(const std::string& psbt);
  std::string combinepsbt(const Json::Value& psbts);
  Json::Value finalizepsbt(const std::string& psbt, bool extract);
  Json::Value analyzepsbt(const std::string& psbt);
  std::string dumpprivkey(const std::string& address);

private:
  std::mutex mtx;

  /*
   * ACHTUNG:
   *     BitcoinClientStub sends the json-rpc auth credentials for each call.
   *     They might also be dumped on the screen from time to time.
   *
   *     This is a potential security risk.
   *
   * EXAMPLE:
   *     [2021-Aug-09 15:58:10.295206] [0x0000d816 0x00007f4d5dec0a40] [...]
   *     terminate called after throwing an instance of 'jsonrpc::JsonRpcException'
   *     what():  Exception -32003 : Client connector error: libcurl error: 7 -> Could not connect to
   * http://user:2KVNeKYRVrOo3xLMnr4oBkzWuNFEqcCrRHcBF1hQy9w=@127.0.0.1:38232
   *
   * TODO: find a better way of securing the bitcoin-rpc credentials.
   */
  jsonrpc::HttpClient httpClient;
  BitcoinClientStub bitcoind;
}; // class Bitcoind

} // namespace transport
} // namespace itcoin

#endif // ITCOIN_TRANSPORT_BTCCLIENT_H
