#ifndef ITCOIN_WALLET_WALLET_H
#define ITCOIN_WALLET_WALLET_H

#include <psbt.h>
#include <primitives/block.h>
#include "../frost/frost.hpp"
#include "../frost/three_fbft_helpers.hpp"

namespace itcoin{ namespace pbft{ namespace messages {
  class Message;
}}}

namespace itcoin {
  class PbftConfig;
}

namespace itcoin { namespace transport {
  class BtcClient;
}} // namespace itcoin::transport

namespace itcoin {
namespace wallet {

class Wallet
{
  public:
    Wallet(const itcoin::PbftConfig& conf);

    // These methods are used to sign and verify the signatures on the messages
    virtual void AppendSignature(itcoin::pbft::messages::Message& message) const = 0;
    virtual bool VerifySignature(const itcoin::pbft::messages::Message& message) const = 0;

    virtual std::string GetBlockSignature(const CBlock&) = 0;
    virtual CBlock FinalizeBlock(const CBlock&, const std::vector<std::string>) const = 0;

  protected:
    const itcoin::PbftConfig& m_conf;
};

class RoastWallet : virtual public Wallet
{
  public:
    RoastWallet(const itcoin::PbftConfig& conf);

    virtual std::string GetPreSignatureShare() = 0;
    virtual std::string GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature, const CBlock&) = 0;
    virtual CBlock FinalizeBlock(const CBlock&, const std::string pre_sig, const std::vector<std::string> sig_shares) const = 0;

    // This is a concrete method that just calls GetPreSignatureShare
    std::string GetBlockSignature(const CBlock&);

    // This is a concrete method that throws a runtime exception
    CBlock FinalizeBlock(const CBlock&, const std::vector<std::string>) const;
};

class BitcoinRpcWallet: virtual public Wallet
{
  public:
    BitcoinRpcWallet(const itcoin::PbftConfig& conf, transport::BtcClient& bitcoind);

    void AppendSignature(itcoin::pbft::messages::Message& message) const;
    bool VerifySignature(const itcoin::pbft::messages::Message& message) const;

    std::string GetBlockSignature(const CBlock&);
    CBlock FinalizeBlock(const CBlock&, const std::vector<std::string>) const;

  protected:
    transport::BtcClient& m_bitcoind;
    std::string m_pubkey_address;
};
class RoastWalletImpl: public BitcoinRpcWallet, public RoastWallet
{
  public:
    RoastWalletImpl(const itcoin::PbftConfig& conf, transport::BtcClient& bitcoind);
    ~RoastWalletImpl();

    CBlock FinalizeBlock(const CBlock&, const std::string, const std::vector<std::string>) const override;
    std::string GetPreSignatureShare() override;
    std::string GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature, const CBlock& block) override;

    // This concrete method just calls RoastWallet::GetPreSignatureShare()
    std::string GetBlockSignature(const CBlock&);
    // This concrete method just calls RoastWallet::FinalizeBlock()
    CBlock FinalizeBlock(const CBlock&, const std::vector<std::string>) const;
    // This concrete method just calls BitcoinRpcWallet::AppendSignature()
    void AppendSignature(itcoin::pbft::messages::Message &message) const;
    // This concrete method just calls BitcoinRpcWallet::VerifySignature()
    bool VerifySignature(const itcoin::pbft::messages::Message &message) const;

  private:
    keypair InitializeKeyPair(const itcoin::PbftConfig &conf, transport::BtcClient &bitcoind) const;
    std::vector<std::string> SplitPreSignatures(std::string serializedList) const;
    signature AggregateSignatureShares(const unsigned char *message, const size_t message_size,
                                       const std::string &pre_signatures,
                                       const std::vector<std::string> &signature_shares) const;
    uint256 TaprootSignatureHash(const CMutableTransaction &spendTx, const CMutableTransaction &toSpendTx) const;

    const keypair m_keypair;
    nonce_pair m_nonce;
    bool m_valid_nonce;
};

class ThreeFBFTWalletImpl: public BitcoinRpcWallet, public RoastWallet
{
public:
    ThreeFBFTWalletImpl(const itcoin::PbftConfig& conf, transport::BtcClient& bitcoind);
    ~ThreeFBFTWalletImpl();


    CBlock FinalizeBlock(const CBlock&, const std::string, const std::vector<std::string>) const override;
    std::string GetPreSignatureShare() override;
    std::string GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature, const CBlock& block) override;

    // This concrete method just calls RoastWallet::GetPreSignatureShare()
    std::string GetBlockSignature(const CBlock&);
    // This concrete method just calls RoastWallet::FinalizeBlock()
    CBlock FinalizeBlock(const CBlock&, const std::vector<std::string>) const;
    // This concrete method just calls BitcoinRpcWallet::AppendSignature()
    void AppendSignature(itcoin::pbft::messages::Message &message) const;
    // This concrete method just calls BitcoinRpcWallet::VerifySignature()
    bool VerifySignature(const itcoin::pbft::messages::Message &message) const;

private:
    keypair InitializeKeyPair(const itcoin::PbftConfig &conf, transport::BtcClient &bitcoind) const;
    std::vector<std::string> SplitPreSignatures(std::string serializedList) const;
    signature AggregateSignatureShares(const uint256 &message_digest,
                                       const std::vector<std::string> &signature_shares) const;
    uint256 TaprootSignatureHash(const CMutableTransaction &spendTx, const CMutableTransaction &toSpendTx) const;

    std::map<uint32_t, PublicCommitmentDerivation> m_commitments_derivations;
    std::vector<std::vector<uint32_t>> m_signers_combinations;
    PrivateNonceDerivation m_nonce_derivation_;
    const keypair m_keypair;

    static const int kBindingCommitmentChildIndex = 0;
    static const int kHidingCommitmentChildIndex = 1;
    nonce_pair DeriveNoncePair(const uint256 &hash_out);

    signing_commitment DeriveParticipantCommitments(const uint256 &hash_out, const uint32_t &participant_index) const;
    int RetrieveSignerCombination(const std::vector<std::string> &signature_shares) const;

    std::string ExtractSignatureShareByCombinationIndex(const std::string &_signature_share, const std::string &delimiter,
                                                        const uint32_t combination_index) const;

    bool ParticipantInCombination(int combination_index, uint32_t participant_index) const;
};

}
}

#endif // ITCOIN_WALLET_WALLET_H
