#include "blockchain.h"

#include <streams.h>
#include <version.h>

#include "../utils.h"

using namespace std;

namespace itcoin {
namespace blockchain {

HexSerializablePsbt::HexSerializablePsbt()
{

}

HexSerializablePsbt::HexSerializablePsbt(PartiallySignedTransaction tx):
PartiallySignedTransaction(tx)
{

}

HexSerializablePsbt::HexSerializablePsbt(std::string tx_hex)
{
  std::string tx_str = utils::hexToString(tx_hex);
  Span<uint8_t> tx_span {
    reinterpret_cast<uint8_t*>(&tx_str[0]),
    tx_str.size()
  };
  CDataStream dataStream {
    tx_span, SER_NETWORK, PROTOCOL_VERSION
  };
  this->Unserialize(dataStream);
}

std::string HexSerializablePsbt::GetHex() const
{
  CDataStream dataStream{SER_NETWORK, PROTOCOL_VERSION};
  this->Serialize(dataStream);
  return utils::stringToHex(dataStream.str());
}

}
}
