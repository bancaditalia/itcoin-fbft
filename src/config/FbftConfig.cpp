// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "FbftConfig.h"

#include <boost/log/trivial.hpp>

#include <cmath>
#include <fstream>
#include <json/json.h>

#include <chainparamsbase.h>
#include <rpc/request.h>
#include <util/system.h>

#include "utils/utils.h"

using namespace std;
using namespace std::chrono_literals;

namespace itcoin {

const string DEFAULT_MINER_CONF_FILENAME = "miner.conf.json";
const string DEFAULT_FBFT_DB_FILENAME = "miner.fbft.db";

FbftConfig::FbftConfig(std::string datadir, std::string configFileName) {
  string bitcoin_config_file = datadir + "/" + BITCOIN_CONF_FILENAME;
  string miner_config_file = datadir + "/" + DEFAULT_MINER_CONF_FILENAME;

  BOOST_LOG_TRIVIAL(debug) << "Reading bitcoind configuration from " << bitcoin_config_file;
  BOOST_LOG_TRIVIAL(debug) << "Reading miner configuration from " << miner_config_file;

  // Engine reset and database
  m_fbft_db_reset = false;
  m_fbft_db_filename = datadir + "/" + DEFAULT_FBFT_DB_FILENAME;

  // Clear args
  gArgs.ClearArgs();

  // Read bitcoind configuration
  gArgs.AddArg("-datadir=<dir>",
               "Specify data directory. The miner will read its configuration from " + configFileName +
                   ", and the bitcoind specific data from " + miner_config_file,
               ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
  SetupChainParamsBaseOptions(gArgs);
  gArgs.AddArg("-rpcpassword=<pw>", "Password for JSON-RPC connections", ArgsManager::ALLOW_ANY,
               OptionsCategory::OPTIONS);
  gArgs.AddArg("-rpcport=<port>", strprintf("Connect to JSON-RPC on <port>"),
               ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::OPTIONS);
  gArgs.AddArg("-rpcuser=<user>", "Username for JSON-RPC connections", ArgsManager::ALLOW_ANY,
               OptionsCategory::OPTIONS);
  gArgs.AddArg("-signetchallenge=<signetchallenge>", "The signet challenge.", ArgsManager::ALLOW_ANY,
               OptionsCategory::OPTIONS);
  gArgs.AddArg("-zmqpubitcoinblock=<address>",
               "Enable publish hash block, height and time in <address> (ITCOIN_SPECIFIC)",
               ArgsManager::ALLOW_ANY, OptionsCategory::ZMQ);

  // Parse bitcoind config file
  gArgs.ForceSetArg("-datadir", datadir);
  std::string error;
  if (!gArgs.ReadConfigFiles(error, true)) {
    BOOST_LOG_TRIVIAL(debug) << "Error reading bitcoin configuration at " << bitcoin_config_file;
  }

  // Select the bitcoin config file section for the signet network
  string chain_name = gArgs.GetChainName();
  if (chain_name != "signet") {
    std::string msg =
        "chain_name's value is \"" + chain_name + "\", but the only allowed value is \"signet\"";

    BOOST_LOG_TRIVIAL(error) << msg;
    throw std::runtime_error(msg);
  }

  SelectBaseParams(chain_name);
  gArgs.SelectConfigNetwork(chain_name);

  // Parse parameters from config file
  itcoin_rpchost_ = "localhost";
  itcoin_rpcport_ = gArgs.GetArg("-rpcport", to_string(BaseParams().RPCPort()));

  {
    // extract the TCP port from the zmqpubitcoinblock bind string in bitcoin.conf
    std::string itcoinblock_bind_string = gArgs.GetArg("-zmqpubitcoinblock", "");
    if (itcoinblock_bind_string == "") {
      std::string msg = "itcoin-core is not configured to send new blocks notifications via "
                        "\"-zmqpubitcoinblock\" parameter. Please configure bitcoind.conf accordingly.";
      BOOST_LOG_TRIVIAL(error) << msg;
      throw std::runtime_error(msg);
    }

    std::size_t colon_pos = itcoinblock_bind_string.find_last_of(":");
    if (colon_pos == std::string::npos) {
      std::string msg = "cannot find TCP port in \"-zmqpubitcoinblock\" parameter: \"" +
                        itcoinblock_bind_string + "\" contains no \":\"";
      BOOST_LOG_TRIVIAL(error) << msg;
      throw std::runtime_error(msg);
    }
    unsigned long remaining_chars = itcoinblock_bind_string.length() - colon_pos - 1;

    std::size_t consumed_chars;
    long unsigned int itcoinblock_port = 0;
    try {
      itcoinblock_port = std::stoul(itcoinblock_bind_string.substr(colon_pos + 1), &consumed_chars);
    } catch (std::invalid_argument const& e) {
      BOOST_LOG_TRIVIAL(error) << e.what();
      consumed_chars = 0;
    } catch (std::out_of_range const& e) {
      BOOST_LOG_TRIVIAL(error) << e.what();
      consumed_chars = 0;
    }
    if ((consumed_chars == 0) || (consumed_chars < remaining_chars) ||
        (itcoinblock_port > std::numeric_limits<uint16_t>::max())) {
      std::string msg = "cannot extract a meaningful TCP port from \"-zmqpubitcoinblock\" (" +
                        itcoinblock_bind_string + ")";
      BOOST_LOG_TRIVIAL(error) << msg;
      throw std::runtime_error(msg);
    }

    m_itcoinblock_connection_string = "tcp://" + itcoin_rpchost_ + ":" + std::to_string(itcoinblock_port);
  }
  BOOST_LOG_TRIVIAL(debug) << "The value computed for connecting to the itcoinblock topic is "
                           << m_itcoinblock_connection_string;

  // Parse cookie file or rpcuser and pass
  if (gArgs.GetArg("-rpcpassword", "") == "") {
    BOOST_LOG_TRIVIAL(info)
        << "No \"-rpcpassword\" parameter was given: falling back to cookie-based authentication";
    // Try fall back to cookie-based authentication if no password is provided

    // GetAuthCookieFile
    std::string arg = gArgs.GetArg("-rpccookiefile", ".cookie");
    fs::path filepath = AbsPathForConfigVal(fs::PathFromString(arg));

    std::ifstream file;
    file.open(filepath);
    if (file.is_open() == false) {
      std::string msg = "could not open " + fs::PathToString(filepath) + ". Is the bitcoind daemon running?";

      BOOST_LOG_TRIVIAL(error) << msg;
      throw std::runtime_error(msg);
    }

    std::getline(file, itcoin_rpc_auth_);
    file.close();
    BOOST_LOG_TRIVIAL(info) << "JSON-RPC auth data has been read from " + fs::PathToString(filepath);
  } else {
    itcoin_rpc_auth_ = gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
    BOOST_LOG_TRIVIAL(info)
        << "JSON-RPC auth data has been taken from command line parameters \"-rpcuser\" and \"-rpcpassword\"";
  }

  if ((itcoin_signet_challenge_ = gArgs.GetArg("-signetchallenge", "")) == "") {
    std::string msg = "signetchallenge not set";
    BOOST_LOG_TRIVIAL(error) << msg;
    throw std::runtime_error(msg);
  }
  BOOST_LOG_TRIVIAL(debug) << "Signet challenge: " << itcoin_signet_challenge_;

  // Read fbftd configuration
  Json::Value config;
  ifstream configFile(miner_config_file, ifstream::binary);

  try {
    configFile >> config;
  } catch (const std::exception& e) {
    std::string msg = "Could not read configuration from " + miner_config_file + ". Is a valid json file?";
    throw std::runtime_error(msg);
  }

  id_ = config["id"].asUInt();
  m_genesis_block_hash = config["genesis_block_hash"].asString();
  m_genesis_block_timestamp = config["genesis_block_timestamp"].asUInt();
  m_target_block_time = config["target_block_time"].asDouble();

  // Remove prefix 5120 from signet challenge to retrieve the group public key
  m_group_public_key = itcoin_signet_challenge_.substr(4);

  // Configure networking
  if (config["sniffer_dish_connection_string"].isNull()) {
    m_sniffer_dish_connection_string = std::nullopt;
    BOOST_LOG_TRIVIAL(debug) << "This replica will not send its message to any sniffer.";
  } else {
    m_sniffer_dish_connection_string = config["sniffer_dish_connection_string"].asString();
    BOOST_LOG_TRIVIAL(warning) << "Messages from this replica will also be sent to "
                               << m_sniffer_dish_connection_string.value();
  }

  // Read the replica config
  Json::Value replica_config_a = config["fbft_replica_set"];
  for (unsigned int i = 0; i < replica_config_a.size(); ++i) {
    Json::Value replica_config_json = replica_config_a[i];
    string host = replica_config_json["host"].asString();
    string port = replica_config_json["port"].asString();
    string p2pkh = replica_config_json["p2pkh"].asString();
    string pubkey = replica_config_json["pubkey"].asString();
    TransportConfig replica_config(i, host, port, p2pkh, pubkey);

    BOOST_LOG_TRIVIAL(trace) << "Read replica #" << i << " - host: " << host << ", port: " << port
                             << ", p2pkh: " << p2pkh;
    replica_set_v_.push_back(replica_config);
  }

  m_cluster_size = replica_set_v_.size();
}

const unsigned int FbftConfig::cluster_size() const { return m_cluster_size; }

std::string FbftConfig::getSignetChallenge() const {
  return this->itcoin_signet_challenge_;
} // FbftConfig::getSignetChallenge()

} // namespace itcoin
