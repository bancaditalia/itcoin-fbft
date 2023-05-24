#include "stubs.h"

#include <boost/format.hpp>

#include "../../PbftConfig.h"

namespace msgs = itcoin::pbft::messages;

using namespace std;

namespace itcoin {
namespace test {

DummyWallet::DummyWallet(const itcoin::PbftConfig& conf):
Wallet(conf)
{

}

void DummyWallet::AppendSignature(msgs::Message& message) const
{
  string sig = str(
      boost::format("Sig_%1%")
      % message.sender_id()
  );
  message.set_signature(sig);
}

bool DummyWallet::VerifySignature(const msgs::Message& message) const
{
  string expected_sig = str(
      boost::format("Sig_%1%")
      % message.sender_id()
  );
  if ( message.signature() == expected_sig )
  {
    return true;
  }
  else
  {
    return false;
  }
}

std::string DummyWallet::GetBlockSignature(const CBlock& block)
{
  auto rawTx = CMutableTransaction();
  rawTx.nVersion = 0;
  rawTx.nLockTime = 0;
  PartiallySignedTransaction psbtx;
  psbtx.tx = rawTx;
  return "psbtx";
}

CBlock DummyWallet::FinalizeBlock(const CBlock& block,
  const std::vector<std::string> signatures) const
{
  CBlock signed_block(block);
  return signed_block;
}

}
}
