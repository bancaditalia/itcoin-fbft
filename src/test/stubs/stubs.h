// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_TEST_STUBS_STUBS_H
#define ITCOIN_TEST_STUBS_STUBS_H

#include <boost/log/trivial.hpp>

#include "../../blockchain/blockchain.h"
#include "../../transport/network.h"
#include "../../wallet/wallet.h"

namespace messages = itcoin::fbft::messages;

namespace blockchain = itcoin::blockchain;
namespace network = itcoin::network;
namespace wallet = itcoin::wallet;

namespace itcoin {
namespace test {

class NetworkStub {
public:
  NetworkStub();

  std::vector<std::shared_ptr<network::NetworkListener>> listeners;
  bool active;
};

class DummyNetwork : public network::NetworkTransport, public NetworkStub {
public:
  DummyNetwork(const itcoin::FbftConfig& conf);
  void BroadcastMessage(std::unique_ptr<messages::Message> p_msg);
  void SimulateReceiveMessages();

private:
  std::vector<std::unique_ptr<messages::Message>> m_buffer;
};

class DummyBlockchain : public blockchain::Blockchain, public NetworkStub {
public:
  DummyBlockchain(const itcoin::FbftConfig& conf);

  void set_genesis_block_timestamp(double genesis_block_timestamp);

  CBlock GenerateBlock(uint32_t block_timestamp);
  bool TestBlockValidity(const uint32_t height, const CBlock&, bool check_signet_solution);
  void SubmitBlock(const uint32_t height, const CBlock&);
  uint32_t height() { return chain.size() - 1; }

  // Public attributes
  std::vector<CBlock> chain;

private:
  void Init();
};

class DummyWallet : public wallet::Wallet {
public:
  DummyWallet(const itcoin::FbftConfig& conf);
  void AppendSignature(messages::Message& message) const;
  bool VerifySignature(const messages::Message& message) const;

  std::string GetBlockSignature(const CBlock&);
  CBlock FinalizeBlock(const CBlock&, const std::vector<std::string>) const;
};

class DummyRoastWallet : public wallet::RoastWallet {
  int state_i;

public:
  DummyRoastWallet(const itcoin::FbftConfig& conf);

  void AppendSignature(messages::Message& message) const override;
  bool VerifySignature(const messages::Message& message) const override;

  CBlock FinalizeBlock(const CBlock&, const std::string, const std::vector<std::string>) const override;

  std::string GetPreSignatureShare() override;
  std::string GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature,
                                const CBlock&) override;
};

} // namespace test
} // namespace itcoin

#endif
