// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_BLOCKCHAIN_EXTRACT_H
#define ITCOIN_BLOCKCHAIN_EXTRACT_H

#include <primitives/block.h>

namespace itcoin { namespace transport {
  class BtcClient;
}} // namespace itcoin::transport

namespace itcoin { namespace blockchain {

void appendSignetSolution(CBlock *block, std::vector<unsigned char> signetSolution);
std::pair<CMutableTransaction, CMutableTransaction> signetTxs(const CBlock& block, const std::string& signetChallengeHex);

}} // namespace itcoin::blockchain

#endif // ITCOIN_BLOCKCHAIN_EXTRACT_H
