// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_BLOCKCHAIN_BLOCKCHAIN_H
#define ITCOIN_BLOCKCHAIN_BLOCKCHAIN_H

#include <psbt.h>

namespace itcoin {
class FbftConfig;
}

namespace itcoin {
namespace transport {
class BtcClient;
}
} // namespace itcoin

namespace itcoin {
namespace blockchain {

class HexSerializableCBlock : public CBlock {
public:
  HexSerializableCBlock();
  HexSerializableCBlock(CBlock block);
  HexSerializableCBlock(std::string block_hex);
  std::string GetHex() const;
};

class Blockchain {
public:
  Blockchain(const itcoin::FbftConfig& conf);

  virtual CBlock GenerateBlock(uint32_t block_timestamp) = 0;
  virtual bool TestBlockValidity(const uint32_t height, const CBlock&, bool check_signet_solution) = 0;
  virtual void SubmitBlock(const uint32_t height, const CBlock&) = 0;

protected:
  const itcoin::FbftConfig& m_conf;
};

class BitcoinBlockchain : public Blockchain {
public:
  BitcoinBlockchain(const itcoin::FbftConfig& conf, transport::BtcClient& bitcoind);

  CBlock GenerateBlock(uint32_t block_timestamp);
  bool TestBlockValidity(const uint32_t height, const CBlock& block, bool check_signet_solution);
  void SubmitBlock(const uint32_t height, const CBlock& block);

protected:
  transport::BtcClient& m_bitcoind;
  std::string m_reward_address;
};

} // namespace blockchain
} // namespace itcoin

#endif // ITCOIN_BLOCKCHAIN_BLOCKCHAIN_H
