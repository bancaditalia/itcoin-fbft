#include "extract.h"

#include <boost/log/trivial.hpp>

#include <signet.h>
#include <util/strencodings.h>

#include "../transport/btcclient.h"
#include "psbt_utils.h"

namespace itcoin { namespace block {

const std::vector<unsigned char> SCRIPT_SIG_INDEX{7};
const std::vector<unsigned char> SCRIPT_WIT_INDEX{8};
const std::vector<unsigned char> DEFAULT_SCRIPT_SIG{};
const std::vector<unsigned char> DEFAULT_SCRIPT_WIT{0x00};

static std::vector<unsigned char> extractScriptWithDefault(const std::map<std::vector<unsigned char>, std::vector<unsigned char>>& inputMap, const std::vector<unsigned char>& index, const std::vector<unsigned char>& defaultValue)
{
  std::vector<unsigned char> scriptVec;

  if (inputMap.find(index) != inputMap.end()) {
    scriptVec = inputMap.find(index)->second;
  } else {
    scriptVec = defaultValue;
  }

  return scriptVec;
} // extractScriptWithDefault()

std::pair<CBlock, std::vector<unsigned char>> decodePsbt(std::string psbtBase64)
{
  PartiallySignedTransaction psbt = deserializePsbt(psbtBase64);

  const auto& inputMap = psbt.inputs[0].unknown;

  // deserialize scripts
  const std::vector<unsigned char> scriptSigVec = extractScriptWithDefault(inputMap, SCRIPT_SIG_INDEX, DEFAULT_SCRIPT_SIG);
  const std::vector<unsigned char> scriptWitVec = extractScriptWithDefault(inputMap, SCRIPT_WIT_INDEX, DEFAULT_SCRIPT_WIT);

  // deserialize block
  CBlock block;
  CDataStream dataStream {
    psbt.unknown[PSBT_SIGNET_BLOCK], SER_NETWORK, PROTOCOL_VERSION
  };
  block.Unserialize(dataStream);

  std::vector<unsigned char> finalScript;
  CVectorWriter writer(SER_NETWORK, INIT_PROTO_VERSION, finalScript, 0);
  writer << scriptSigVec;
  finalScript.insert(finalScript.end(), scriptWitVec.begin(), scriptWitVec.end());

  return std::make_pair(block, finalScript);
} // decodePsbt()

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

CBlock extractBlock(transport::BtcClient& bitcoindClient, std::string psbtBase64)
{
  auto analysis = bitcoindClient.analyzepsbt(psbtBase64);

  if (!analysis["inputs"][0]["is_final"]) {
    throw std::invalid_argument("PSBT is not complete");
  }

  CBlock block;
  std::vector<unsigned char> signetSolution;
  std::tie(block, signetSolution) = decodePsbt(psbtBase64);

  if (signetSolution.size() == 0) {
    std::string msg = "signet solution is empty";
    BOOST_LOG_TRIVIAL(error) << msg;
    throw std::runtime_error(msg);
  }

  BOOST_LOG_TRIVIAL(trace) << "block::extractBlock Signet solution (len: " << signetSolution.size() << "): " << HexStr(signetSolution);

  appendSignetSolution(&block, signetSolution);

  return block;
} // extractBlock()

}} // namespace itcoin::block
