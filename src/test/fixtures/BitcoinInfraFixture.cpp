// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures.h"

BitcoinInfraFixture::BitcoinInfraFixture():
m_latest_block_time(0)
{
  BOOST_LOG_TRIVIAL(info) << "Setup fixture BitcoinInfraFixture";
  currentDirectory = boost::filesystem::absolute(boost::filesystem::path("."));
  for (size_t nodeId = 0; nodeId < CLUSTER_SIZE; ++nodeId)
  {
    try
    {
      // If the node is not started, this will raise an exception
      m_bitcoinds.at(nodeId)->getblockchaininfo();
    }
    catch (const std::exception &e)
    {
      // We start the node only if it is not yet started
      auto currentBitcoinDir = currentDirectory / "infra" / getBitcoinNodeDirName(nodeId) / "signet";
      // TODO, check how to handle reset=true;
      if (m_reset)
      {
        resetBlockchain(currentBitcoinDir);
      }
      nodes.emplace_back(
          boost::process::child(currentDirectory / "infra" / "bitcoind.sh", std::to_string(nodeId)));
    }
  }
  // let nodes to set up
  sleep(4);
} // BitcoinInfraFixture()

std::string BitcoinInfraFixture::getBitcoinNodeDirName(size_t nodeId)
{
  std::stringstream ss;

  ss << "node" << std::setw(2) << std::setfill('0') << nodeId;
  return ss.str();
} // getBitcoinNodeDirName()

void BitcoinInfraFixture::resetBlockchain(boost::filesystem::path bitcoinDir)
{
  boost::filesystem::directory_iterator end; // default construction yields past-the-end
  BOOST_LOG_TRIVIAL(info) << "Processing path " << bitcoinDir;

  for (auto &entry : boost::make_iterator_range(boost::filesystem::directory_iterator(bitcoinDir), {}))
  {
    auto curPath = entry.path();
    auto curPathFilename = curPath.filename().string();

    if (curPathFilename != "wallets" and curPathFilename != "settings.json")
    {
      BOOST_LOG_TRIVIAL(info) << "deleting " << curPathFilename;
      boost::filesystem::remove_all(curPath);
    }
  }
} // resetBlockchain()

void BitcoinInfraFixture::stopProc(boost::process::child &nodeProc)
{
  auto pid = nodeProc.id();
  BOOST_LOG_TRIVIAL(info) << "Sending SIGINT to pid " << pid;
  kill(pid, SIGINT);
} // stopProc()

uint32_t BitcoinInfraFixture::get_present_block_time()
{
  const auto p1 = std::chrono::system_clock::now();
  uint32_t candidate_block_time = std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
  if (candidate_block_time<=m_latest_block_time)
  {
    candidate_block_time=m_latest_block_time+1;
  }
  m_latest_block_time = candidate_block_time;
  return candidate_block_time;
}

BitcoinInfraFixture::~BitcoinInfraFixture()
{
  for (auto &nodeProc : nodes)
  {
    stopProc(nodeProc);
  }

  for (auto &nodeProc : nodes)
  {
    nodeProc.wait();
  }

  BOOST_LOG_TRIVIAL(info) << "Teardown fixture BitcoinInfraFixture";
} // ~BitcoinInfraFixture()