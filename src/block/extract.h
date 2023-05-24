#ifndef ITCOIN_BLOCK_EXTRACT_H
#define ITCOIN_BLOCK_EXTRACT_H

#include <primitives/block.h>

namespace itcoin { namespace transport {
  class BtcClient;
}} // namespace itcoin::transport

namespace itcoin { namespace block {

/**
 * Extracts the block contained in psbtBase64, provided it contains a sufficient
 * number of signatures. Puts the signet solution in the first block, replacing
 * anything that was eventually there.
 *
 * Verifies that the signet solution is not empty.
 */
CBlock extractBlock(transport::BtcClient& bitcoindClient, std::string psbtBase64);

void appendSignetSolution(CBlock *block, std::vector<unsigned char> signetSolution);

}} // namespace itcoin::block

#endif // ITCOIN_BLOCK_EXTRACT_H
