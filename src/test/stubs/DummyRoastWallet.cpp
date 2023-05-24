#include "stubs.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include "../../PbftConfig.h"

namespace msgs = itcoin::pbft::messages;

using namespace std;

namespace itcoin {
namespace test {

DummyRoastWallet::DummyRoastWallet(const itcoin::PbftConfig& conf):
Wallet(conf), RoastWallet(conf)
{
  // Init state
  state_i = 0;

  // Declare the dynamic wallets predicates
  PlCall("assertz", PlTermv(PlCompound(
    "(roast_crypto_pre_sig_aggregate(Replica_id, Pre_signature_shares, Pre_signature) :- roast_crypto_pre_sig_aggregate_dummy(Replica_id, Pre_signature_shares, Pre_signature))"
  )));
}

void DummyRoastWallet::AppendSignature(msgs::Message& message) const
{
  string sig = str(
      boost::format("Sig_%1%")
      % message.sender_id()
  );
  message.set_signature(sig);
}

bool DummyRoastWallet::VerifySignature(const msgs::Message& message) const
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

CBlock DummyRoastWallet::FinalizeBlock(const CBlock& block, const std::string pre_sig, const std::vector<std::string> sig_shares) const
{
  string msg = str(
    boost::format("FinalizeBlock Replica_id=%1%, state=%2%, presig=%3%, shares=%4%, block=%5%")
    % m_conf.id()
    % state_i
    % pre_sig
    % boost::algorithm::join(sig_shares, ",")
    % block.GetHash().GetHex()
  );
  BOOST_LOG_TRIVIAL(trace) << msg;

  CBlock signed_block(block);
  return signed_block;
}

std::string DummyRoastWallet::GetPreSignatureShare()
{
  string presig = str(
      boost::format("Pre_share_%1%_%2%")
      % m_conf.id()
      % state_i
  );

  string msg = str(
    boost::format("GetPreSignatureShare Replica_id=%1%, State_i=%2%, Presig=%3%")
    % m_conf.id()
    % state_i
    % presig
  );
  BOOST_LOG_TRIVIAL(trace) << msg;

  state_i++;
  return presig;
}

std::string DummyRoastWallet::GetSignatureShare(std::vector<uint32_t> signers, std::string pre_signature, const CBlock& block)
{
  std::stringstream signers_stream;
  std::copy(signers.begin(), signers.end(), std::ostream_iterator<int>(signers_stream, ","));
  string signers_str = signers_stream.str();
  signers_str.pop_back();

  string sig_share = str(
      boost::format("Sig_share_%1%_[%2%]")
      % m_conf.id()
      % signers_str
  );

  string msg = str(
    boost::format("GetSignatureShare Replica_id=%1%, State_i=%2%, Signers=%3%, pre_signature=%4%, block=%5%, sigshare=%6%")
    % m_conf.id()
    % state_i
    % signers_str
    % pre_signature
    % block.GetHash().GetHex()
    % sig_share
  );
  BOOST_LOG_TRIVIAL(trace) << msg;
  return sig_share;
}

PREDICATE(roast_crypto_pre_sig_aggregate_dummy, 3)
{
  BOOST_LOG_TRIVIAL(debug) << "I am in the prolog predicate roast_crypto_pre_sig_aggregate";
  string a3_result = "";
  PlTail Presig_shares{PL_A2}; PlTerm Presig_elem;
  while(Presig_shares.next(Presig_elem))
  {
    string presig = std::string{(const char*)  Presig_elem};
    a3_result += presig;
    a3_result += "+";
  }
  a3_result.pop_back();
  PlTerm Pre_signature = PL_A3;
  PL_A3 = PlString((const char*) a3_result.c_str());
  return true;
}

}
}
