#include "wallet.h"

using namespace std;

namespace itcoin {
namespace wallet {

RoastWallet::RoastWallet(const itcoin::PbftConfig& conf):
Wallet(conf)
{
}

std::string RoastWallet::GetBlockSignature(const CBlock& block)
{
  return GetPreSignatureShare();
}

CBlock RoastWallet::FinalizeBlock(const CBlock& block, const std::vector<std::string> signatures) const
{
  throw runtime_error("RoastWallet does not implement FinalizeBlock with 2 input parameters. \
Use FinalizeBlock with 3 input parameters, and pass the presignature.");
}

}
}
