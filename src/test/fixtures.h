#ifndef ITCOIN_PBFT_FIXTURES_H
#define ITCOIN_PBFT_FIXTURES_H

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/results_collector.hpp>
/*
 * As of boost 1.75, boost::process relies on boost::filesystem and is not able
 * to deal with std::filesystem
 */
#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <SWI-cpp.h>

#include <iomanip>

#include "../PbftConfig.h"
#include "../pbft/Replica2.h"
#include "../pbft/actions/actions.h"
#include "../pbft/state/state.h"
#include "../transport/btcclient.h"

#include "stubs/stubs.h"

namespace utf = boost::unit_test;

using namespace std;

using namespace itcoin::pbft;
using namespace itcoin::pbft::actions;
using namespace itcoin::pbft::messages;
using namespace itcoin::pbft::state;
using namespace itcoin::test;

struct BtcClientFixture
{
  itcoin::PbftConfig cfgNode0;
  itcoin::transport::BtcClient bitcoind0;

  BtcClientFixture() : cfgNode0(itcoin::PbftConfig((boost::filesystem::current_path() / "infra/node00").string())),
                       bitcoind0(cfgNode0.itcoin_uri())
  {
    BOOST_LOG_TRIVIAL(info) << "Setup fixture BtcClientFixture";
  }

  ~BtcClientFixture()
  {
    BOOST_LOG_TRIVIAL(info) << "Teardown fixture BtcClientFixture";
  } // ~BtcClientFixture()
};

struct BitcoinRpcTestFixture
{
  BitcoinRpcTestFixture()
  {
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      string config_suffix = str(boost::format("infra/node0%1%") % i);
      string config_path = (boost::filesystem::current_path() / config_suffix).string();
      unique_ptr<itcoin::PbftConfig> config = make_unique<itcoin::PbftConfig>(config_path);
      unique_ptr<itcoin::transport::BtcClient> bitcoin = make_unique<itcoin::transport::BtcClient>(config->itcoin_uri());
      unique_ptr<itcoin::wallet::BitcoinRpcWallet> wallet = make_unique<itcoin::wallet::BitcoinRpcWallet>(*config, *bitcoin);
      unique_ptr<itcoin::blockchain::BitcoinBlockchain> blockchain = make_unique<itcoin::blockchain::BitcoinBlockchain>(*config, *bitcoin);

      m_configs.emplace_back(move(config));
      m_bitcoinds.emplace_back(move(bitcoin));
      m_wallets.emplace_back(move(wallet));
      m_blockchains.emplace_back(move(blockchain));
    }
  }

  ~BitcoinRpcTestFixture()
  {
    BOOST_LOG_TRIVIAL(info) << "Teardown BitcoinRpcTestFixture";
  }

  string address_at(uint32_t replica_id)
  {
    auto &config = *m_configs.at(replica_id);
    return config.replica_set_v().at(config.id()).p2pkh();
  }

  const uint32_t CLUSTER_SIZE = 4;
  vector<unique_ptr<itcoin::PbftConfig>> m_configs;
  vector<unique_ptr<itcoin::transport::BtcClient>> m_bitcoinds;
  vector<unique_ptr<itcoin::wallet::BitcoinRpcWallet>> m_wallets;
  vector<unique_ptr<itcoin::blockchain::BitcoinBlockchain>> m_blockchains;
};

struct PrologTestFixture
{
  PrologTestFixture()
  {
  }

  ~PrologTestFixture()
  {
    utf::test_case::id_t id = utf::framework::current_test_case().p_id;
    utf::test_results rez = utf::results_collector.results(id);
    if (!rez.passed())
    {
      BOOST_LOG_TRIVIAL(debug) << "Test did not pass, dumping all the dynamic facts that I know...";
      cout << endl
           << endl;
      PlCall("print_all_dynamics");
      cout << endl
           << endl;
    };
  }
};

template <class WalletClass = DummyWallet>
struct ReplicaStateFixture : PrologTestFixture
{
  ReplicaStateFixture(itcoin::SIGNATURE_ALGO_TYPE sig_algo, uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time) : PrologTestFixture(),
                                                                                                                                                   SIG_ALGO(sig_algo), CLUSTER_SIZE(cluster_size), GENESIS_BLOCK_TIMESTAMP(genesis_block_timestamp), TARGET_BLOCK_TIME(target_block_time)
  {
    static_assert(std::is_base_of<itcoin::wallet::Wallet, WalletClass>::value, "Wallet type parameter of this class must derive from BaseClass");

    BOOST_LOG_TRIVIAL(trace) << "Setup fixture ReplicaSetFixture";
    m_blockchain_config = make_unique<itcoin::PbftConfig>("infra/node00");
    m_blockchain_config->set_genesis_block_timestamp(GENESIS_BLOCK_TIMESTAMP);
    m_blockchain = make_unique<DummyBlockchain>(*m_blockchain_config);

    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      auto config = make_unique<itcoin::PbftConfig>("infra/node00");
      config->set_replica_id(i);
      config->set_cluster_size(CLUSTER_SIZE);
      config->set_genesis_block_hash("genesis");
      config->set_genesis_block_timestamp(0);
      config->set_target_block_time(TARGET_BLOCK_TIME);
      config->set_pbft_db_reset(true);
      config->set_pbft_db_filename("/tmp/miner.pbft.db");
      config->set_signature_algorithm(sig_algo);
      m_configs.emplace_back(move(config));

      auto wallet = make_unique<WalletClass>(*m_configs.at(i));
      m_wallets.emplace_back(move(wallet));

      string start_hash = m_configs.at(i)->genesis_block_hash();
      uint32_t start_height = 0;
      uint32_t start_time = m_configs.at(i)->genesis_block_timestamp();
      unique_ptr<ReplicaState> state =
          make_unique<ReplicaState>(*m_configs.at(i), *m_blockchain, *m_wallets.at(i), start_height, start_hash, start_time);
      state->set_synthetic_time(0);

      m_states.emplace_back(move(state));
    }
  }

  void set_synthetic_time(double time)
  {
    for (auto &p_state : m_states)
    {
      p_state->set_synthetic_time(time);
    }
  }

  itcoin::SIGNATURE_ALGO_TYPE SIG_ALGO;
  uint32_t CLUSTER_SIZE;
  uint32_t GENESIS_BLOCK_TIMESTAMP;
  uint32_t TARGET_BLOCK_TIME;

  unique_ptr<itcoin::PbftConfig> m_blockchain_config;
  unique_ptr<DummyBlockchain> m_blockchain;

  vector<unique_ptr<itcoin::PbftConfig>> m_configs;
  vector<unique_ptr<itcoin::wallet::Wallet>> m_wallets;
  vector<unique_ptr<ReplicaState>> m_states;
}; // ReplicaStateFixture

struct ReplicaSetFixture : ReplicaStateFixture<>
{
  ReplicaSetFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time) : ReplicaStateFixture(itcoin::SIGNATURE_ALGO_TYPE::NAIVE, cluster_size, genesis_block_timestamp, target_block_time)
  {
    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      auto transport = make_unique<DummyNetwork>(*m_configs.at(i));
      m_transports.emplace_back(move(transport));

      string start_hash = m_configs.at(i)->genesis_block_hash();
      uint32_t start_height = 0;
      uint32_t start_time = m_configs.at(i)->genesis_block_timestamp();
      shared_ptr<Replica2> replica =
          make_shared<Replica2>(*m_configs.at(i), *m_blockchain, *m_wallets.at(i), *m_transports.at(i), start_height, start_hash, start_time);
      m_replica.emplace_back(replica);
    }

    for (auto p_replica : m_replica)
    {
      p_replica->set_synthetic_time(0);
      m_blockchain->listeners.push_back(p_replica);
      for (int i = 0; i < CLUSTER_SIZE; i++)
      {
        m_transports.at(i)->listeners.push_back(p_replica);
      }
    }
  }

  void kill(uint32_t replica_id)
  {
    if (!m_transports.at(replica_id)->active)
    {
      BOOST_LOG_TRIVIAL(debug) << str(
          boost::format("R%1% is already sleeping.") % replica_id);
      return;
    }

    BOOST_LOG_TRIVIAL(info) << str(
        boost::format("R%1% going to sleep now.") % replica_id);

    auto replica_to_remove = m_replica[replica_id];

    m_blockchain->listeners.erase(
        std::remove(m_blockchain->listeners.begin(), m_blockchain->listeners.end(), replica_to_remove),
        m_blockchain->listeners.end());

    m_transports.at(replica_id)->active = false;

    for (auto &transport : m_transports)
    {
      transport->listeners.erase(
          std::remove(transport->listeners.begin(), transport->listeners.end(), replica_to_remove),
          transport->listeners.end());
    }
  }

  void wake(uint32_t replica_id)
  {
    if (m_transports.at(replica_id)->active)
    {
      BOOST_LOG_TRIVIAL(debug) << str(
          boost::format("R%1% is already awake.") % replica_id);
      return;
    }

    BOOST_LOG_TRIVIAL(debug) << str(
        boost::format("R%1% wakes up.") % replica_id);

    auto replica_to_add = m_replica[replica_id];

    m_transports.at(replica_id)->active = true;
    for (auto &transport : m_transports)
    {
      transport->listeners.insert(transport->listeners.begin() + replica_id, replica_to_add);
    }

    // Reactivate blockchain and sync last block
    m_blockchain->listeners.insert(m_blockchain->listeners.begin() + replica_id, m_replica[replica_id]);
    for (size_t i = max((int)m_blockchain->chain.size() - 3, 0); i < m_blockchain->chain.size(); i++)
    {
      unique_ptr<Block> p_msg = make_unique<Block>(i, m_blockchain->chain[i].nTime, m_blockchain->chain[i].GetHash().GetHex());
      m_blockchain->listeners[replica_id]->ReceiveIncomingMessage(move(p_msg));
    }
  }

  void move_forward(int time_delta)
  {
    for (size_t i = 0; i < CLUSTER_SIZE; i++)
    {
      if (m_transports[i]->active)
      {
        m_replica[i]->CheckTimedActions();
      }
    }

    // SimulateReceiveMessages all transports N times
    for (uint32_t N = 0; N < 10; N++)
    {
      for (size_t i = 0; i < CLUSTER_SIZE; i++)
      {
        m_transports[i]->SimulateReceiveMessages();
      }
    }

    double current_time = m_replica[0]->current_time();
    current_time += time_delta;
    set_synthetic_time(current_time);
  }

  vector<unique_ptr<DummyNetwork>> m_transports;
  vector<shared_ptr<Replica2>> m_replica;
};

struct BitcoinInfraFixture : public BitcoinRpcTestFixture
{

  vector<boost::process::child> nodes;
  boost::filesystem::path currentDirectory;
  bool m_reset = false;

  BitcoinInfraFixture()
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

  std::string getBitcoinNodeDirName(size_t nodeId)
  {
    std::stringstream ss;

    ss << "node" << std::setw(2) << std::setfill('0') << nodeId;
    return ss.str();
  } // getBitcoinNodeDirName()

  void resetBlockchain(boost::filesystem::path bitcoinDir)
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

  void stopProc(boost::process::child &nodeProc)
  {
    auto pid = nodeProc.id();
    BOOST_LOG_TRIVIAL(info) << "Sending SIGINT to pid " << pid;
    kill(pid, SIGINT);
  } // stopProc()

  uint32_t get_current_time()
  {
    const auto p1 = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
  }

  uint32_t get_next_block_time()
  {
    uint32_t target_block_time = m_configs[0]->target_block_time(); // 60 seconds
    Json::Value blockchain_info = m_bitcoinds[0]->getblockchaininfo();
    uint32_t latest_block_time = blockchain_info["time"].asUInt();
    return latest_block_time + target_block_time;
  }

  ~BitcoinInfraFixture()
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

}; // struct BitcoinInfraFixture

struct RoastWalletTestFixture
{
  RoastWalletTestFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time) 
      : CLUSTER_SIZE(cluster_size), GENESIS_BLOCK_TIMESTAMP(genesis_block_timestamp), TARGET_BLOCK_TIME(target_block_time)
  {
    BOOST_LOG_TRIVIAL(trace) << "Setup fixture RoastWalletTestFixture";

    for (int i = 0; i < CLUSTER_SIZE; i++)
    {
      string config_suffix = str(boost::format("infra/node0%1%") % i);
      string config_path = (boost::filesystem::current_path() / config_suffix).string();
      unique_ptr<itcoin::PbftConfig> config = make_unique<itcoin::PbftConfig>(config_path);

      config->set_replica_id(i);
      config->set_cluster_size(CLUSTER_SIZE);
      config->set_genesis_block_timestamp(0);
      config->set_target_block_time(TARGET_BLOCK_TIME);
      config->set_pbft_db_reset(true);
      config->set_pbft_db_filename("/tmp/miner.pbft.db");
      config->set_signature_algorithm(itcoin::SIGNATURE_ALGO_TYPE::ROAST);

      auto bitcoin = make_unique<itcoin::transport::BtcClient>(config->itcoin_uri());
      auto wallet = make_unique<itcoin::wallet::RoastWalletImpl>(*config, *bitcoin);
      auto blockchain = make_unique<itcoin::blockchain::BitcoinBlockchain>(*config, *bitcoin);

      m_wallets.emplace_back(move(wallet));
      m_blockchains.emplace_back(move(blockchain));
      m_configs.emplace_back(move(config));
      m_bitcoinds.emplace_back(move(bitcoin));
    }
  }

  ~RoastWalletTestFixture() { }

  void set_synthetic_time(double time)
  {
    for (auto &p_state : m_states)
    {
      p_state->set_synthetic_time(time);
    }
  }

  uint32_t get_next_block_time()
  {
    uint32_t target_block_time = m_configs[0]->target_block_time(); // 60 seconds
    Json::Value blockchain_info = m_bitcoinds[0]->getblockchaininfo();
    uint32_t latest_block_time = blockchain_info["time"].asUInt();
    return latest_block_time + target_block_time;
  }

  uint32_t get_current_time()
  {
    const auto p1 = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
  }

  itcoin::SIGNATURE_ALGO_TYPE SIG_ALGO;
  uint32_t CLUSTER_SIZE;
  uint32_t GENESIS_BLOCK_TIMESTAMP;
  uint32_t TARGET_BLOCK_TIME;

  vector<unique_ptr<itcoin::PbftConfig>> m_configs;
  vector<unique_ptr<itcoin::transport::BtcClient>> m_bitcoinds;
  vector<unique_ptr<itcoin::wallet::RoastWalletImpl>> m_wallets;
  vector<unique_ptr<ReplicaState>> m_states;
  vector<unique_ptr<itcoin::blockchain::BitcoinBlockchain>> m_blockchains;

}; // ReplicaStateFixture


struct ThreeFBFTWalletTestFixture
{
    ThreeFBFTWalletTestFixture(uint32_t cluster_size, uint32_t genesis_block_timestamp, uint32_t target_block_time)
        : CLUSTER_SIZE(cluster_size), GENESIS_BLOCK_TIMESTAMP(genesis_block_timestamp), TARGET_BLOCK_TIME(target_block_time)
    {
      BOOST_LOG_TRIVIAL(trace) << "Setup fixture ThreeFBFTWalletTestFixture";

      for (int i = 0; i < CLUSTER_SIZE; i++)
      {
        string config_suffix = str(boost::format("infra/node0%1%") % i);
        string config_path = (boost::filesystem::current_path() / config_suffix).string();
        unique_ptr<itcoin::PbftConfig> config = make_unique<itcoin::PbftConfig>(config_path);

        config->set_replica_id(i);
        config->set_cluster_size(CLUSTER_SIZE);
        config->set_genesis_block_timestamp(0);
        config->set_target_block_time(TARGET_BLOCK_TIME);
        config->set_pbft_db_reset(true);
        config->set_pbft_db_filename("/tmp/miner.pbft.db");
        config->set_signature_algorithm(itcoin::SIGNATURE_ALGO_TYPE::ROAST);

        auto bitcoin = make_unique<itcoin::transport::BtcClient>(config->itcoin_uri());
        auto wallet = make_unique<itcoin::wallet::ThreeFBFTWalletImpl>(*config, *bitcoin);
        auto blockchain = make_unique<itcoin::blockchain::BitcoinBlockchain>(*config, *bitcoin);

        m_wallets.emplace_back(move(wallet));
        m_blockchains.emplace_back(move(blockchain));
        m_configs.emplace_back(move(config));
        m_bitcoinds.emplace_back(move(bitcoin));
      }
    }

    ~ThreeFBFTWalletTestFixture() { }

    void set_synthetic_time(double time)
    {
      for (auto &p_state : m_states)
      {
        p_state->set_synthetic_time(time);
      }
    }

    uint32_t get_next_block_time()
    {
      uint32_t target_block_time = m_configs[0]->target_block_time(); // 60 seconds
      Json::Value blockchain_info = m_bitcoinds[0]->getblockchaininfo();
      uint32_t latest_block_time = blockchain_info["time"].asUInt();
      return latest_block_time + target_block_time;
    }

    uint32_t get_current_time()
    {
      const auto p1 = std::chrono::system_clock::now();
      return std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
    }

    itcoin::SIGNATURE_ALGO_TYPE SIG_ALGO;
    uint32_t CLUSTER_SIZE;
    uint32_t GENESIS_BLOCK_TIMESTAMP;
    uint32_t TARGET_BLOCK_TIME;

    vector<unique_ptr<itcoin::PbftConfig>> m_configs;
    vector<unique_ptr<itcoin::transport::BtcClient>> m_bitcoinds;
    vector<unique_ptr<itcoin::wallet::ThreeFBFTWalletImpl>> m_wallets;
    vector<unique_ptr<ReplicaState>> m_states;
    vector<unique_ptr<itcoin::blockchain::BitcoinBlockchain>> m_blockchains;

}; // ReplicaStateFixture

#endif // ITCOIN_PBFT_FIXTURES_H
