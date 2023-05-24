#include "generate.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <consensus/merkle.h>
#include <hash.h>
#include <signet.h>
#include <streams.h>
#include <util/strencodings.h>

#include "../transport/btcclient.h"
#include <grind.h>
#include "../utils.h"

namespace itcoin { namespace block {

const std::vector<unsigned char> WITNESS_COMMITMENT_HEADER = {0xaa, 0x21, 0xa9, 0xed};

/**
 *
 * Get block template from the Bitcoin node with Signet and SegWit rules.
 *
 * This code mimics https://github.com/bancaditalia/itcoin-core/blob/2f37bb2000665da31e4f45ebcdbfd059b1f3b2df/contrib/signet/miner.py#L368
 *
 * @param bitcoindClient
 * @return the block template
 */
Json::Value getSignetAndSegwitBlockTemplate(transport::BtcClient& bitcoindClient)
{
  Json::Value root;
  Json::Value rules;

  rules.append("segwit");
  rules.append("signet");
  root["rules"] = rules;

  return bitcoindClient.getblocktemplate(root);
} // getSignetAndSegwitBlockTemplate()

/**
 * Bitcoin script opcodes can represent numeric literals between 0 and 16
 * inclusive (0 is a special case). This function performs the encoding.
 */
uint64_t encodeOpN(uint64_t number)
{
  if (not (0 <= number && number <= 16)) {
    throw std::runtime_error("Only numbers between 0 and 16 inclusive can be represented as OP_XX opcodes. Got " + std::to_string(number));
  }

  if (number == 0) {
    return OP_0;
  }

  return OP_1 + number - 1;
} // encodeOpN()

CScript getScriptBIP34CoinbaseHeight(uint64_t height)
{
  CScript result = CScript();

  if (height <= 16) {
    const uint32_t res = encodeOpN(height);

    // Append dummy to increase scriptSig size above 2 (see bad-cb-length consensus rule)
    CScript result = CScript();
    result.push_back(res);
    result.push_back(OP_1);

    return result;
  }

  result << height;

  return result;
} // getScriptBIP34CoinbaseHeight()

CTransactionRef buildCoinbaseTransaction(uint64_t height, CAmount value, CScript scriptPubKey)
{
  auto tx = CMutableTransaction();
  tx.nVersion = 1;
  tx.vin.resize(1);

  tx.vin[0].prevout.SetNull();
  tx.vin[0].scriptSig = getScriptBIP34CoinbaseHeight(height);
  tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;

  tx.vout.resize(1);
  tx.vout[0].scriptPubKey = scriptPubKey;
  tx.vout[0].nValue = value;

  return MakeTransactionRef(tx);
} // buildCoinbaseTransaction()

CScript getScriptPubKey(transport::BtcClient& bitcoindClient, const std::string& address)
{
  // TODO avoid 'getaddressinfo' request by adding scriptPubKey of address in the configuration file
  const Json::Value addressInfo = bitcoindClient.getaddressinfo(address);
  const std::string scriptPubKeyHex = addressInfo["scriptPubKey"].asString();
  const std::vector<unsigned char> scriptPubKeyBytes = ParseHex(scriptPubKeyHex);

  const CScript scriptPubKey = CScript(scriptPubKeyBytes.begin(), scriptPubKeyBytes.end());

  return scriptPubKey;
} // getScriptPubKey()

CMutableTransaction TxFromHex(const std::string& str)
{
  CMutableTransaction tx;
  SpanReader{SER_NETWORK, PROTOCOL_VERSION, ParseHex(str)} >> tx;

  return tx;
} // TxFromHex()

CScript GetWitnessScript(uint256 witnessRoot, uint256 witnessNonce)
{
  const std::vector<unsigned char> witnessRootData(witnessRoot.begin(), witnessRoot.end());
  const std::vector<unsigned char> witnessNonceData(witnessNonce.begin(), witnessNonce.end());
  std::vector<unsigned char> concat;

  concat.reserve(witnessRootData.size() + witnessNonceData.size());
  concat.insert(concat.end(), witnessRootData.begin(), witnessRootData.end());
  concat.insert(concat.end(), witnessNonceData.begin(), witnessNonceData.end());

  const uint256 hash = Hash(concat);

  const std::vector<unsigned char> commitmentData(hash.begin(), hash.end());
  std::vector<unsigned char> data(WITNESS_COMMITMENT_HEADER.begin(), WITNESS_COMMITMENT_HEADER.end());

  data.reserve(WITNESS_COMMITMENT_HEADER.size() + commitmentData.size());
  data.insert(data.end(), commitmentData.begin(), commitmentData.end());

  return CScript() << OP_RETURN << data;
} // GetWitnessScript()

CBlock generateBlock(transport::BtcClient& bitcoindClient, const std::string& address, uint32_t block_timestamp)
{
  // create block template - START
  Json::Value blockTemplate;
  std::string previousBlockHash;
  {
    blockTemplate = getSignetAndSegwitBlockTemplate(bitcoindClient);
    previousBlockHash = utils::checkHash(blockTemplate["previousblockhash"].asString());

    BOOST_LOG_TRIVIAL(trace) << "Block template: " << blockTemplate.toStyledString();
  } // create block template - END

  // build coinbase transaction - START
  CTransactionRef coinbaseTx;
  {
    /*
     * This code mimics:
     *
     * https://github.com/bancaditalia/itcoin-core/blob/2f37bb2000665da31e4f45ebcdbfd059b1f3b2df/contrib/signet/miner.py#L377
     * https://github.com/bancaditalia/itcoin-core/blob/2f37bb2000665da31e4f45ebcdbfd059b1f3b2df/contrib/signet/miner.py#L130-L134
     */
    const uint64_t height = blockTemplate["height"].asUInt64();
    const CAmount value = blockTemplate["coinbasevalue"].asUInt64();
    const CScript scriptPubKey = getScriptPubKey(bitcoindClient, address);
    coinbaseTx = buildCoinbaseTransaction(height, value, scriptPubKey);

    BOOST_LOG_TRIVIAL(trace) << "coinbase tx hash: " << coinbaseTx->GetHash().GetHex();
  } // build coinbase transaction - END

  // create block - START
  CBlock block;
  {
    block.nVersion = blockTemplate["version"].asInt();

    uint256 previousBlockHashInt256;
    previousBlockHashInt256.SetHex(previousBlockHash);
    block.hashPrevBlock = previousBlockHashInt256;

    const uint32_t minTime = blockTemplate["mintime"].asUInt();
    /*
      A timestamp is accepted as valid if it is greater than the median timestamp of previous 11 blocks,
      and less than the network-adjusted time + 2 hours.
      "Network-adjusted time" is the median of the timestamps returned by all nodes connected to you.
      As a result block timestamps are not exactly accurate, and they do not need to be.
      Block times are accurate only to within an hour or two.
      Whenever a node connects to another node, it gets a UTC timestamp from it, and stores its offset from node-local UTC.
      The network-adjusted time is then the node-local UTC plus the median offset from all connected nodes.
      Network time is never adjusted more than 70 minutes from local system time, however.

      WAS:

      const uint32_t curTime = blockTemplate["curtime"].asUInt();
      block.nTime = curTime < minTime? minTime: curTime;
    */
    if (block_timestamp<minTime)
    {
      std::string error_msg = str(
        boost::format("generate::generateBlock timestamp below minTime: %1%, block_timestamp %2%")
          % minTime
          % block_timestamp
      );
      throw std::runtime_error(error_msg);
    }
    block.nTime = block_timestamp;

    block.nBits = utils::stoui(blockTemplate["bits"].asString(), nullptr, 16);
    block.nNonce = 0;

    const Json::Value transactionJson = blockTemplate["transactions"];

    // +1 because of the coinbase transaction
    block.vtx.resize(1 + transactionJson.size());
    block.vtx[0] = coinbaseTx;

    uint32_t index = 0;
    for (auto it = transactionJson.begin(); it != transactionJson.end(); ++it) {
      /*
       * vtx[0] is already filled with the coinbase transaction, thus we have to
       * start from 1
       */
      index = index + 1;

      const std::string transactionData = utils::checkHex((*it)["data"].asString());

      CMutableTransaction currentTx = TxFromHex(transactionData);
      block.vtx[index] = MakeTransactionRef(currentTx);
    }

    BOOST_LOG_TRIVIAL(trace) << "Block merkle root (function which includes signatures) after block creation: " << BlockMerkleRoot(block).GetHex();
  } // create block - END

  // append the witness commitment - START
  CScript newOutScript;
  {
    const uint256 witNonce = uint256(0);
    const uint256 witRoot = BlockWitnessMerkleRoot(block);

    BOOST_LOG_TRIVIAL(trace) << "BlockWitnessMerkleRoot: " << witRoot.GetHex();

    newOutScript = GetWitnessScript(witRoot, witNonce);

    CScriptWitness cbwit;
    {
      const std::vector<unsigned char> witNonceVec(witNonce.begin(), witNonce.end());

      cbwit.stack.resize(1);
      cbwit.stack[0] = witNonceVec;
    }

    // update coinbase transaction
    {
      auto tempMutableTx = CMutableTransaction(*coinbaseTx);

      tempMutableTx.vin[0].scriptWitness = cbwit;
      const CTxOut newOut = CTxOut(CAmount(0), newOutScript);
      tempMutableTx.vout.push_back(newOut);
      coinbaseTx = MakeTransactionRef(tempMutableTx);

      block.vtx[0] = coinbaseTx;
    }

    BOOST_LOG_TRIVIAL(trace) << "Block merkle root (function which includes signatures) after appending witness commitment: " << BlockMerkleRoot(block).GetHex();
  } // append the witness commitment - END

  // append the SIGNET_HEADER - START
  {
    /*
     * ITCOIN_SPECIFIC: in itCoin this is made here, because the merkle root
     * should include the signet header but not the signet solution.
     *
     * In bitcoin's signet, the signet header is appendend later, together with
     * the signet solution. The Merkle Root included both.
     */
    newOutScript = newOutScript << SIGNET_HEADER_VEC;

    const CTxOut newOut = CTxOut(CAmount(0), newOutScript);
    auto tempMutableTx = CMutableTransaction(*coinbaseTx);

    tempMutableTx.vout[coinbaseTx->vout.size() - 1] = newOut;
    coinbaseTx = MakeTransactionRef(tempMutableTx);

    block.vtx[0] = coinbaseTx;

    // transactions updated: recalculate block merkle root
    const uint256 newBlockMerkleRoot = BlockMerkleRoot(block);
    block.hashMerkleRoot = newBlockMerkleRoot;

    BOOST_LOG_TRIVIAL(trace) << "Block witness commitment after appending signet header: " << HexStr(newOutScript);
    BOOST_LOG_TRIVIAL(trace) << "Block merkle root (function which includes signatures) after appending signet header: " << newBlockMerkleRoot.GetHex();
  } // append the SIGNET_HEADER - END

  // mine block - START
  {
    const CBlockHeader header(block);
    CDataStream dataStream{SER_NETWORK, PROTOCOL_VERSION};
    header.Serialize(dataStream);

    const std::string blockHeaderHex = HexStr(dataStream.str());
    BOOST_LOG_TRIVIAL(trace) << "block header hex " << blockHeaderHex;

    BOOST_LOG_TRIVIAL(debug) << "Start grinding block... ";
    const std::string newHeaderHex = Grind(blockHeaderHex);

    dataStream = CDataStream(ParseHex(newHeaderHex), SER_NETWORK, PROTOCOL_VERSION);

    CBlockHeader newHeader;

    dataStream >> newHeader;
    block.nNonce = newHeader.nNonce;

    BOOST_LOG_TRIVIAL(trace) << "Block merkle root (function which includes signatures) after mining: " << BlockMerkleRoot(block).GetHex();
    BOOST_LOG_TRIVIAL(trace) << "Grinded block: " << block.ToString();
  } // mine block - END

  return block;
} // generateBlock()

}} // namespace itcoin::block
