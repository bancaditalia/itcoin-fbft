#ifndef ITCOIN_BITCOIN_CORE_GRIND_H
#define ITCOIN_BITCOIN_CORE_GRIND_H

#include <atomic>
#include <string>

class CBlockHeader;

/**
 * The implementation of this function is taken as-is from bitcoin-util.cpp:
 * https://raw.githubusercontent.com/bitcoin/bitcoin/master/src/bitcoin-util.cpp
 *
 * To check that the function stays consistent as bitcoin-core evolces, one can
 * run (for example in CI) scripts/check-code-consistency.py
 */
void grind_task(uint32_t nBits, CBlockHeader& header_orig, uint32_t offset, uint32_t step, std::atomic<bool>& found);

/**
 * Loosely based on Grind() in bitcoin-util.cpp
 */
std::string Grind(std::string hexHeader);

#endif //ITCOIN_BITCOIN_CORE_GRIND_H
