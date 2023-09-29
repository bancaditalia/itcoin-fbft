// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "network.h"

namespace itcoin {
namespace network {

NetworkTransport::NetworkTransport(const itcoin::FbftConfig& conf) : m_conf(conf) {}

} // namespace network
} // namespace itcoin
