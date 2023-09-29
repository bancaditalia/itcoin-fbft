// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "messages.h"

#include <boost/format.hpp>

#include "../../utils/utils.h"

using namespace std;

namespace itcoin {
namespace fbft {
namespace messages {

Block::Block(uint32_t block_height, uint32_t block_time, std::string block_hash)
    : Message(NODE_TYPE::CLIENT, 8888) {
  m_block_height = block_height;
  m_block_time = block_time;
  m_block_hash = block_hash;
}

Block::~Block(){};

bool Block::equals(const Message& other) const {
  if (typeid(*this) != typeid(other)) {
    return false;
  }
  auto typed_other = static_cast<const Block&>(other);

  if (m_block_height != typed_other.m_block_height) {
    return false;
  }
  if (m_block_time != typed_other.m_block_time) {
    return false;
  }
  if (m_block_hash != typed_other.m_block_hash) {
    return false;
  }
  return Message::equals(other);
}

std::unique_ptr<Message> Block::clone() {
  std::unique_ptr<Message> msg = std::make_unique<Block>(*this);
  return msg;
}

std::string Block::identify() const {
  return str(boost::format("<BLOCK, height=%1%, time=%2%, hash=%3%>") % m_block_height % m_block_time %
             m_block_hash);
}

} // namespace messages
} // namespace fbft
} // namespace itcoin
