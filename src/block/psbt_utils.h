#ifndef ITCOIN_PSBT_UTILS_H
#define ITCOIN_PSBT_UTILS_H

#include <string>
#include <vector>

#include <psbt.h>

namespace itcoin { namespace transport {
  class BtcClient;
}} // namespace itcoin::transport

namespace itcoin { namespace block {

const std::vector<unsigned char> PSBT_SIGNET_BLOCK = {0xfc, 0x06, 's', 'i', 'g', 'n', 'e', 't', 'b'};

CScript signetSolutionScript(const std::string& signetSolutionHex);
std::pair<CMutableTransaction, CMutableTransaction> signetTxs(const CBlock& block, const std::string& signetChallengeHex);

void serializePsbtToStream(CDataStream& s, const PartiallySignedTransaction& psbt);
std::string serializePsbt(const PartiallySignedTransaction& psbt);
PartiallySignedTransaction deserializePsbtFromStream(CDataStream& s);
PartiallySignedTransaction deserializePsbt(const std::string& psbtBase64);

std::string createPsbt(const CBlock& block, const std::string& signetChallengeHex);
std::pair<std::string, bool> signPsbt(transport::BtcClient& bitcoindClient, const std::string& psbt);
void appendSignetSolution(CBlock *block, std::vector<unsigned char> signetSolution);

template <class PSBTMap>
PSBTMap deserializePsbtMapFromStream(CDataStream& s) {
  PSBTMap psbtMap;

  // Read loop
  bool found_sep = false;
  while(!s.empty()) {
    // Read
    std::vector<unsigned char> key;
    s >> key;

    // the key is empty if that was actually a separator byte
    // This is a special case for key lengths 0 as those are not allowed (except for separator)
    if (key.empty()) {
      found_sep = true;
      break;
    }

    if (psbtMap.unknown.count(key) > 0) {
      throw std::ios_base::failure("Duplicate Key, key for unknown value already provided");
    }
    // Read in the value
    std::vector<unsigned char> val_bytes;
    s >> val_bytes;
    psbtMap.unknown.emplace(std::move(key), std::move(val_bytes));
  }

  if (!found_sep) {
    throw std::ios_base::failure("Separator is missing at the end of an output map");
  }

  return psbtMap;
} // deserializePsbtMapFromStream()

}} // namespace itcoin::block

#endif // ITCOIN_PSBT_UTILS_H
