// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "wallet.h"

using namespace std;

namespace itcoin {
namespace wallet {

RoastWallet::RoastWallet(const itcoin::FbftConfig& conf) : Wallet(conf) {}

} // namespace wallet
} // namespace itcoin
