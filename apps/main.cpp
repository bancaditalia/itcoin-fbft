#include <filesystem>

#include <boost/log/trivial.hpp>

#include "utils.h"
#include "../src/blockchain/blockchain.h"
#include "../src/transport/btcclient.h"
#include "../src/transport/zcomm.h"
#include "../src/wallet/wallet.h"
#include "../src/pbft/messages/messages.h"
#include "../src/pbft/Replica2.h"

#include <util/system.h>
#include <util/translation.h>

#include <generated/prolog_pbft_engine/resource_db_mem.h>
#include <SWI-cpp.h>

using namespace itcoin;

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

const std::string DEFAULT_DATADIR = (std::filesystem::current_path() / "infra" / "node02" ).string();

/**
 * Returns:
 * - datadir
 */
std::tuple<std::string> parse_cmdline(int argc, char* argv[])
{
  // Configure the command line args
  ArgsManager argsManager;

  argsManager.AddArg("-datadir=<dir>", "Specify data directory", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);

  // Parse the command line args
  std::string error;
  if (!argsManager.ParseParameters(argc, argv, error)) {
    BOOST_LOG_TRIVIAL(error) << "Error parsing command line arguments: " << error;
    throw std::runtime_error(error);
  }

  // Get config file path
  std::string datadir = argsManager.GetArg("-datadir", DEFAULT_DATADIR);

  return {datadir};
} // parse_cmdline()

int main(int argc, char* argv[])
{
  // Init prolog engine
  // TODO: replace with incbin
  PL_set_resource_db_mem(resource_db_mem_bin, resource_db_mem_bin_len);
  char *argv2[] = {(char*)"thisisnonsense", (char*)"-f", (char*)"none", (char*)"-F", (char*)"none", (char*)"-g", (char*)"true"};
  PlEngine engine(7, argv2);

  // Setup logger
  utils::configure_boost_logging();

  // read command line arguments
  auto [datadir] = parse_cmdline(argc, argv);

  PbftConfig config{datadir};

  BOOST_LOG_TRIVIAL(debug) << "The ID of this replica is: " << config.id();
  BOOST_LOG_TRIVIAL(debug) << "------------";

  transport::BtcClient btc_client{config.itcoin_uri()};

  blockchain::BitcoinBlockchain blockchain{config, btc_client};

  std::unique_ptr<wallet::Wallet > pWallet;
  if (config.signature_algorithm() == itcoin::SIGNATURE_ALGO_TYPE::ROAST) {
    pWallet = std::make_unique<itcoin::wallet::RoastWalletImpl>(config, btc_client);
  } else if (config.signature_algorithm() == itcoin::SIGNATURE_ALGO_TYPE::THREEFBFT) {
    pWallet = std::make_unique<itcoin::wallet::ThreeFBFTWalletImpl>(config, btc_client);
  } else {
    pWallet = std::make_unique<itcoin::wallet::BitcoinRpcWallet>(config, btc_client);
  }
  //network::NetworkTransport& transport

  transport::ZComm zcomm{config};

  // Bring replica in sync with the blockchain
  Json::Value current_blockchain_info = btc_client.getblockchaininfo();
  uint32_t start_height = current_blockchain_info["blocks"].asUInt();

  std::string start_hash = config.genesis_block_hash();
  uint32_t start_time = config.genesis_block_timestamp();
  if (start_height>0)
  {
    start_hash = current_blockchain_info["bestblockhash"].asString();
    start_time = current_blockchain_info["time"].asUInt();
  }

  pbft::Replica2 replica{
    config,
    blockchain,
    *pWallet,
    zcomm,
    start_height,
    start_hash,
    start_time
  };

  // Start the replica
  zcomm.replica_message_received.connect([&replica](const std::string& group_name, const std::string& bin_buffer) {
    auto p_msg = pbft::messages::Message::BuildFromBinBuffer(bin_buffer);
    // TODO: far ritornare direttamente unique_ptr e fare check per nullptr
    replica.ReceiveIncomingMessage(std::move(p_msg.value()));
  });

  zcomm.itcoinblock_received.connect([&replica](const std::string& hash_hex_string, int32_t block_height, uint32_t block_time, uint32_t seq_number) {
      BOOST_LOG_TRIVIAL(info) << "Ricevuto nuovo blocco. Hash: " << hash_hex_string << ", altezza: " << block_height << ", block_time: " << block_time << ", seq_number " << seq_number;
      auto p_msg = std::make_unique<pbft::messages::Block>(block_height, block_time, hash_hex_string);
      replica.ReceiveIncomingMessage(std::move(p_msg));
  });

  zcomm.network_timeout_expired.connect([&replica]() {
    BOOST_LOG_TRIVIAL(trace) << "Network timeout expired. Call replica::CheckTimedAction()";
    replica.CheckTimedActions();
  });

  zcomm.run_forever();

  BOOST_LOG_TRIVIAL(info) << "Terminating";
  return EXIT_SUCCESS;
}
