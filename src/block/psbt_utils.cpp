#include "psbt_utils.h"

#include <boost/log/trivial.hpp>

#include <util/strencodings.h>
#include <primitives/block.h>
#include <streams.h>
#include <version.h>
#include <psbt.h>

#include "../transport/btcclient.h"

namespace itcoin { namespace block {

CScript signetSolutionScript(const std::string& signetSolutionHex)
{
  CScript scriptSig;
  {
    scriptSig << OP_0 << ParseHex(signetSolutionHex);
  }
  return scriptSig;
} // signetSolutionScript()

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

std::pair<std::string, bool> signPsbt(transport::BtcClient& bitcoindClient, const std::string& psbt)
{
  auto partially_signed_psbt = bitcoindClient.walletprocesspsbt(psbt);
  return std::make_pair(partially_signed_psbt["psbt"].asString(), partially_signed_psbt["complete"].asBool());
} // signPsbt()

/**
 * Custom PSBT serialization function that ignores the main Tx, which now is
 * kept unset. The original serialization function read the tx field regardless
 * whether it was set or not.
 */
void serializePsbtToStream(CDataStream& s, const PartiallySignedTransaction& psbt)
{
  // magic bytes
  s << PSBT_MAGIC_BYTES;

  // Write the unknown things
  for (auto& entry : psbt.unknown) {
    s << entry.first;
    s << entry.second;
  }

  // Separator
  s << PSBT_SEPARATOR;

  // Write inputs
  for (auto& entry : psbt.inputs[0].unknown) {
    s << entry.first;
    s << entry.second;
  }
  s << PSBT_SEPARATOR;

  // Write outputs
  for (auto& entry : psbt.outputs[0].unknown) {
    s << entry.first;
    s << entry.second;
  }
  s << PSBT_SEPARATOR;
} // serializePsbtToStream()

std::string serializePsbt(const PartiallySignedTransaction& psbt)
{
  CDataStream dataStream{SER_NETWORK, PROTOCOL_VERSION};
  serializePsbtToStream(dataStream, psbt);
  std::string psbtBase64 = EncodeBase64(MakeUCharSpan(dataStream));

  return psbtBase64;
} // serializePsbt()

PartiallySignedTransaction deserializePsbtFromStream(CDataStream& s)
{
  PartiallySignedTransaction psbt;

  // Read the magic bytes
  uint8_t magic[5];
  s >> magic;
  if (!std::equal(magic, magic + 5, PSBT_MAGIC_BYTES)) {
    throw std::ios_base::failure("Invalid PSBT magic bytes");
  }

  // Used for duplicate key detection
  std::set<std::vector<unsigned char>> key_lookup;

  // Read global data
  while(!s.empty()) {
    // Read
    std::vector<unsigned char> key;
    s >> key;

    // the key is empty if that was actually a separator byte
    // This is a special case for key lengths 0 as those are not allowed (except for separator)
    if (key.empty()) {
      BOOST_LOG_TRIVIAL(trace) << "no keys anymore";
      break;
    }

    // Unknown stuff
    if (psbt.unknown.count(key) > 0) {
      throw std::ios_base::failure("Duplicate Key, key for unknown value already provided");
    }

    // Read in the value
    std::vector<unsigned char> val_bytes;
    s >> val_bytes;
    BOOST_LOG_TRIVIAL(trace) << "found key: " << HexStr(key) << " and val: " << HexStr(val_bytes);
    psbt.unknown.emplace(std::move(key), std::move(val_bytes));
  } // while(!s.empty())

  auto txBytes = psbt.unknown[std::vector<unsigned char>{0}];
  CDataStream dataStream{txBytes, SER_NETWORK, PROTOCOL_VERSION};
  CMutableTransaction tx;
  dataStream >> tx;
  psbt.tx = tx;

  // Read input data
  unsigned int i = 0;

  while (!s.empty() && i < psbt.tx->vin.size()) {
    BOOST_LOG_TRIVIAL(trace) << "reading input " << i;
    PSBTInput input = deserializePsbtMapFromStream<PSBTInput>(s);
    psbt.inputs.push_back(input);

    // Make sure the non-witness utxo matches the outpoint
    if (input.non_witness_utxo && input.non_witness_utxo->GetHash() != psbt.tx->vin[i].prevout.hash) {
      throw std::ios_base::failure("Non-witness UTXO does not match outpoint hash");
    }
    ++i;
  }

  // Make sure that the number of inputs matches the number of inputs in the transaction
  if (psbt.inputs.size() != psbt.tx->vin.size()) {
    throw std::ios_base::failure("Inputs provided does not match the number of inputs in transaction.");
  }

  // Read output data
  i = 0;

  while (!s.empty() && i < psbt.tx->vout.size()) {
    PSBTOutput output = deserializePsbtMapFromStream<PSBTOutput>(s);
    psbt.outputs.push_back(output);
    ++i;
  }

  // Make sure that the number of outputs matches the number of outputs in the transaction
  if (psbt.outputs.size() != psbt.tx->vout.size()) {
    throw std::ios_base::failure("Outputs provided does not match the number of outputs in transaction.");
  }

  if (!s.empty()) {
    throw std::runtime_error("stream is not empty");
  }

  return psbt;
} // deserializePsbtFromStream()

PartiallySignedTransaction deserializePsbt(const std::string& psbtBase64)
{
  std::string psbtByteString = DecodeBase64(psbtBase64);
  std::vector<unsigned char> psbtBytes{psbtByteString.begin(), psbtByteString.end()};
  CDataStream dataStream{psbtBytes, SER_NETWORK, PROTOCOL_VERSION};

  return deserializePsbtFromStream(dataStream);
} // deserializePsbt()

std::string createPsbt(const CBlock& block, const std::string& signetChallengeHex)
{
  auto [toSignTx, spendTx] = signetTxs(block, signetChallengeHex);
  PartiallySignedTransaction psbt;

  std::vector<unsigned char> data1;
  CVectorWriter writer1(SER_NETWORK, INIT_PROTO_VERSION, data1, 0);
  toSignTx.Serialize(writer1);
  psbt.unknown[std::vector<unsigned char>{0}] = data1;

  std::vector<unsigned char> data2;
  CVectorWriter writer2(SER_NETWORK, INIT_PROTO_VERSION, data2, 0);
  block.Serialize(writer2);
  psbt.unknown[PSBT_SIGNET_BLOCK] = data2;

  std::vector<unsigned char> data3;
  CVectorWriter writer3(SER_NETWORK, INIT_PROTO_VERSION, data3, 0);
  spendTx.Serialize(writer3);

  PSBTInput i;

  i.unknown[std::vector<unsigned char>{0}] = data3;
  i.unknown[std::vector<unsigned char>{3}] = std::vector<unsigned char>{1, 0, 0, 0};
  psbt.inputs.resize(1);
  psbt.inputs[0] = i;

  PSBTOutput o;

  psbt.outputs.resize(1);
  psbt.outputs[0] = o;

  return serializePsbt(psbt);
} // createPsbt()

}} // namespace itcoin::block
