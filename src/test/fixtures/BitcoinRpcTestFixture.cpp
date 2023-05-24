// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures.h"

BitcoinRpcTestFixture::BitcoinRpcTestFixture()
{
  for (int i = 0; i < CLUSTER_SIZE; i++)
  {
    std::string config_suffix = str(boost::format("infra/node0%1%") % i);
    std::string config_path = (boost::filesystem::current_path() / config_suffix).string();
    std::unique_ptr<itcoin::FbftConfig> config = std::make_unique<itcoin::FbftConfig>(config_path);
    std::unique_ptr<itcoin::transport::BtcClient> bitcoin = std::make_unique<itcoin::transport::BtcClient>(config->itcoin_uri());
    std::unique_ptr<itcoin::wallet::BitcoinRpcWallet> wallet = std::make_unique<itcoin::wallet::BitcoinRpcWallet>(*config, *bitcoin);
    std::unique_ptr<itcoin::blockchain::BitcoinBlockchain> blockchain = std::make_unique<itcoin::blockchain::BitcoinBlockchain>(*config, *bitcoin);

    m_configs.emplace_back(move(config));
    m_bitcoinds.emplace_back(move(bitcoin));
    m_wallets.emplace_back(move(wallet));
    m_blockchains.emplace_back(move(blockchain));
  }
}

BitcoinRpcTestFixture::~BitcoinRpcTestFixture()
{
  BOOST_LOG_TRIVIAL(info) << "Teardown BitcoinRpcTestFixture";
}

std::string BitcoinRpcTestFixture::address_at(uint32_t replica_id)
{
  auto &config = *m_configs.at(replica_id);
  return config.replica_set_v().at(config.id()).p2pkh();
}
