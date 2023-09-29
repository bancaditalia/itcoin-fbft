// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_WALLET_WALLET_H
#define ITCOIN_WALLET_WALLET_H

#include <primitives/block.h>
#include <psbt.h>
#include <secp256k1/include/secp256k1.h>
#include <secp256k1/include/secp256k1_frost.h>

namespace itcoin {
namespace fbft {
namespace messages {
class Message;
}
} // namespace fbft
} // namespace itcoin

namespace itcoin {
class FbftConfig;
}

namespace itcoin {
namespace transport {
class BtcClient;
}
} // namespace itcoin

namespace itcoin {
namespace wallet {

class Wallet {
public:
  Wallet(const itcoin::FbftConfig& conf);

  // These methods are used to sign and verify the signatures on the messages
  virtual void AppendSignature(itcoin::fbft::messages::Message& message) const = 0;
  virtual bool VerifySignature(const itcoin::fbft::messages::Message& message) const = 0;

protected:
  const itcoin::FbftConfig& m_conf;
};

class RoastWallet : virtual public Wallet {
public:
  RoastWallet(const itcoin::FbftConfig& conf);

  virtual std::string GetPreSignatureShare() = 0;
  virtual std::string GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature,
                                        const CBlock&) = 0;
  virtual CBlock FinalizeBlock(const CBlock&, const std::string pre_sig,
                               const std::vector<std::string> sig_shares) const = 0;
};

class BitcoinRpcWallet : virtual public Wallet {
public:
  BitcoinRpcWallet(const itcoin::FbftConfig& conf, transport::BtcClient& bitcoind);

  void AppendSignature(itcoin::fbft::messages::Message& message) const;
  bool VerifySignature(const itcoin::fbft::messages::Message& message) const;

protected:
  transport::BtcClient& m_bitcoind;
  std::string m_pubkey_address;
};

class RoastWalletImpl : public BitcoinRpcWallet, public RoastWallet {
public:
  RoastWalletImpl(const itcoin::FbftConfig& conf, transport::BtcClient& bitcoind);
  ~RoastWalletImpl();

  CBlock FinalizeBlock(const CBlock&, const std::string, const std::vector<std::string>) const override;
  std::string GetPreSignatureShare() override;
  std::string GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature,
                                const CBlock& block) override;

private:
  secp256k1_frost_keypair* InitializeKeyPair(const itcoin::FbftConfig& conf,
                                             transport::BtcClient& bitcoind) const;
  std::vector<std::string> SplitPreSignatures(std::string serializedList) const;
  void AggregateSignatureShares(unsigned char* signature64, const unsigned char* message32,
                                const std::string& pre_signatures,
                                const std::vector<std::string>& signature_shares) const;
  uint256 TaprootSignatureHash(const CMutableTransaction& spendTx,
                               const CMutableTransaction& toSpendTx) const;
  void DeserializePublicKey(std::string serialized_public_key, unsigned char* output33) const;
  void HexToCharArray(const std::string str, unsigned char* retval) const;
  std::string CharArrayToHex(const unsigned char* bytearray, size_t size) const;
  void FillRandom(unsigned char* data, size_t size);
  std::string SerializeSigningCommitment(const secp256k1_frost_nonce_commitment* commitments) const;
  secp256k1_frost_nonce_commitment DeserializeSigningCommitment(const std::string& serialized) const;
  std::string SerializeSignatureShare(const secp256k1_frost_signature_share* signature) const;
  secp256k1_frost_signature_share DeserializeSignatureShare(const std::string& serialized) const;

  bool m_valid_nonce;
  secp256k1_context* m_frost_ctx;
  secp256k1_frost_keypair* m_keypair;
  secp256k1_frost_nonce* m_nonce;
};

} // namespace wallet
} // namespace itcoin

#endif // ITCOIN_WALLET_WALLET_H
