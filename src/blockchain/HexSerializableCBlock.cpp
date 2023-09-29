// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "blockchain.h"

#include <streams.h>
#include <version.h>

#include "../utils/utils.h"

using namespace std;

namespace itcoin {
namespace blockchain {

HexSerializableCBlock::HexSerializableCBlock() {}

HexSerializableCBlock::HexSerializableCBlock(CBlock block) : CBlock(block) {}

HexSerializableCBlock::HexSerializableCBlock(std::string block_hex) {
  std::string block_str = utils::hexToString(block_hex);
  Span<uint8_t> block_span{reinterpret_cast<uint8_t*>(&block_str[0]), block_str.size()};
  CDataStream dataStream{block_span, SER_NETWORK, PROTOCOL_VERSION};
  this->Unserialize(dataStream);
}

std::string HexSerializableCBlock::GetHex() const {
  CDataStream dataStream{SER_NETWORK, PROTOCOL_VERSION};
  this->Serialize(dataStream);
  return utils::stringToHex(dataStream.str());
}

} // namespace blockchain
} // namespace itcoin
