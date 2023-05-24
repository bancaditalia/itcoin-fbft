// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures.h"

BtcClientFixture::BtcClientFixture():
cfgNode0(itcoin::FbftConfig((boost::filesystem::current_path() / "infra/node00").string())), bitcoind0(cfgNode0.itcoin_uri())
{
  BOOST_LOG_TRIVIAL(info) << "Setup fixture BtcClientFixture";
}

BtcClientFixture::~BtcClientFixture()
{
  BOOST_LOG_TRIVIAL(info) << "Teardown fixture BtcClientFixture";
} // ~BtcClientFixture()
