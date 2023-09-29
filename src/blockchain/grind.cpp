#include "grind.h"

#include <thread>

#include <arith_uint256.h>
#include <core_io.h>
#include <primitives/block.h>
#include <streams.h>
#include <util/strencodings.h>
#include <version.h>

void grind_task(uint32_t nBits, CBlockHeader& header_orig, uint32_t offset, uint32_t step,
                std::atomic<bool>& found) {
  arith_uint256 target;
  bool neg, over;
  target.SetCompact(nBits, &neg, &over);
  if (target == 0 || neg || over) {
    return;
  }
  CBlockHeader header = header_orig; // working copy
  header.nNonce = offset;

  uint32_t finish = std::numeric_limits<uint32_t>::max() - step;
  finish = finish - (finish % step) + offset;

  while (!found && header.nNonce < finish) {
    const uint32_t next = (finish - header.nNonce < 5000 * step) ? finish : header.nNonce + 5000 * step;
    do {
      if (UintToArith256(header.GetHash()) <= target) {
        if (!found.exchange(true)) {
          header_orig.nNonce = header.nNonce;
        }
        return;
      }
      header.nNonce += step;
    } while (header.nNonce != next);
  }
}

std::string Grind(std::string hexHeader) {
  CBlockHeader header;
  if (!DecodeHexBlockHeader(header, hexHeader)) {
    throw std::invalid_argument("Could not decode block header");
  }

  uint32_t nBits = header.nBits;
  std::atomic<bool> found{false};

  grind_task(nBits, std::ref(header), 0, 1, std::ref(found));

  if (!found) {
    throw std::runtime_error("Could not satisfy difficulty target");
  }

  CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
  ss << header;

  return HexStr(ss);
}
