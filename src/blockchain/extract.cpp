// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "extract.h"

#include <boost/log/trivial.hpp>

#include <signet.h>
#include <streams.h>
#include <util/strencodings.h>
#include <version.h>


namespace itcoin { namespace blockchain {

void appendSignetSolution(CBlock *block, std::vector<unsigned char> signetSolution)
{
  /*
   * Append the signet solution
   *
   * We remove the last 5 bytes, in order to remove the previously appended
   * SIGNET_HEADER.
   * We append SIGNET_HEADER and signet_solution again as a single pushdata
   * operation.
   *
   * We have to copy the CTransaction object into a CMutableTransaction object
   * as we want to modify it.
   */
  CMutableTransaction tx;

  {
    const CTransaction firstTransaction = *(block->vtx).at(0);

    tx.vin = firstTransaction.vin;
    tx.vout = firstTransaction.vout;
    tx.nVersion = firstTransaction.nVersion;
    tx.nLockTime = firstTransaction.nLockTime;
  }

  const auto lastOutput = tx.vout[tx.vout.size() - 1];
  auto oldScriptPubKey = lastOutput.scriptPubKey;
  oldScriptPubKey.resize(oldScriptPubKey.size() - 5);
  auto newScriptPubKey = oldScriptPubKey;

  std::vector<unsigned char> signetSolutionVec;
  signetSolutionVec.insert(signetSolutionVec.end(), SIGNET_HEADER, SIGNET_HEADER + 4);
  signetSolutionVec.insert(signetSolutionVec.end(), signetSolution.begin(), signetSolution.end());
  newScriptPubKey << signetSolutionVec;

  tx.vout[tx.vout.size() - 1].scriptPubKey = newScriptPubKey;
  block->vtx[0] = MakeTransactionRef(tx);
}


std::pair<CMutableTransaction, CMutableTransaction> signetTxs(const CBlock& block, const std::string& signetChallengeHex)
{
  // assumes signet solution has not been added yet so does not need to be removed
  // ITCOIN_SPECIFIC START
  // assumes SIGNET_HEADER has already been added so does not need to be added here

  // In itCoin, the transaction is first pow-ed and then signed
  // the signature covers the whole block header, including the pow part, namely nBits and nNonce
  CScript scriptSig;
  {
    CDataStream dataStream{SER_NETWORK, PROTOCOL_VERSION};
    dataStream << CBlockHeader(block);
    std::string blockHeaderHex = HexStr(dataStream.str());

    scriptSig << OP_0 << ParseHex(blockHeaderHex);
  }
  // ITCOIN_SPECIFIC END

  CMutableTransaction to_spend;
  to_spend.nVersion = 0;
  to_spend.nLockTime = 0;
  to_spend.vin.resize(1);
  uint256 txid = uint256(0);
  to_spend.vin[0].prevout = COutPoint(txid, 0xFFFFFFFF);
  to_spend.vin[0].scriptSig = scriptSig;
  to_spend.vin[0].nSequence = 0;
  to_spend.vout.resize(1);

  /*
   * Don't use the stream operator to populate CScript() object with the signet
   * challenge (i.e.: do not do "CScript() << ParseHex(signetChallengeHex)").
   *
   * The signet challenge _is_ the script. Whereas, using the stream operator
   * would have prepended some commands to push the data into the stack.
   *
   * We use the CScript constructor to avoid that.
   */
  auto signetChallengeBytes = ParseHex(signetChallengeHex);
  const CScript scriptPubKey = CScript(signetChallengeBytes.begin(), signetChallengeBytes.end());

  to_spend.vout[0].nValue = CAmount(0);
  to_spend.vout[0].scriptPubKey = scriptPubKey;

  CMutableTransaction spend;
  spend.nVersion = 0;
  spend.nLockTime = 0;
  spend.vin.resize(1);
  spend.vin[0].prevout = COutPoint(to_spend.GetHash(), 0);
  spend.vin[0].scriptSig = CScript();
  spend.vin[0].nSequence = 0;
  spend.vout.resize(1);
  spend.vout[0].nValue = CAmount(0);
  spend.vout[0].scriptPubKey = CScript() << OP_RETURN;

  BOOST_LOG_TRIVIAL(trace) << "spend tx: " << MakeTransactionRef(spend)->ToString();
  BOOST_LOG_TRIVIAL(trace) << "to_spend tx: " << MakeTransactionRef(to_spend)->ToString();

  return std::make_pair(spend, to_spend);
} // signetTxs()


}} // namespace itcoin::blockchain
