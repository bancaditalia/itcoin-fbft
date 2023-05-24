#ifndef ITCOIN_BLOCKCHAIN_BLOCKCHAIN_H
#define ITCOIN_BLOCKCHAIN_BLOCKCHAIN_H

#include <psbt.h>

namespace itcoin {
  class PbftConfig;
}

namespace itcoin { namespace transport {
  class BtcClient;
}} // namespace itcoin::transport

namespace itcoin {
namespace blockchain {

class HexSerializableCBlock: public CBlock
{
  public:
    HexSerializableCBlock();
    HexSerializableCBlock(CBlock block);
    HexSerializableCBlock(std::string block_hex);
    std::string GetHex() const;
};

class HexSerializablePsbt: public PartiallySignedTransaction
{
  public:
    HexSerializablePsbt();
    HexSerializablePsbt(PartiallySignedTransaction tx);
    HexSerializablePsbt(std::string tx_hex);
    std::string GetHex() const;
};

class Blockchain
{
  public:
    Blockchain(const itcoin::PbftConfig& conf);

    virtual CBlock GenerateBlock(uint32_t block_timestamp) = 0;
    virtual bool TestBlockValidity(const uint32_t height, const CBlock&, bool check_signet_solution) = 0;
    virtual void SubmitBlock(const uint32_t height, const CBlock&) = 0;

  protected:
    const itcoin::PbftConfig& m_conf;
};

class BitcoinBlockchain: public Blockchain
{
  public:
    BitcoinBlockchain(const itcoin::PbftConfig& conf, transport::BtcClient& bitcoind);

    CBlock GenerateBlock(uint32_t block_timestamp);
    bool TestBlockValidity(const uint32_t height, const CBlock& block, bool check_signet_solution);
    void SubmitBlock(const uint32_t height, const CBlock& block);

  protected:
    transport::BtcClient& m_bitcoind;
    std::string m_reward_address;
};

}
}

#endif // ITCOIN_BLOCKCHAIN_BLOCKCHAIN_H
