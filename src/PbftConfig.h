#ifndef PBFT_CONFIG_H_
#define PBFT_CONFIG_H_

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace itcoin {

enum SIGNATURE_ALGO_TYPE : unsigned int {
  NAIVE = 0,
  ROAST = 1,
  THREEFBFT = 2,
};

const std::string SIGNATURE_ALGO_TYPE_AS_STRING[] = {
  "NAIVE",
  "ROAST",
  "THREEFBFT",
};

class TransportConfig
{
  public:
    TransportConfig(unsigned int id, std::string host, std::string port, std::string p2pkh, std::string pubkey):
      id_(id),
      host_(host),
      port_(port),
      p2pkh_(p2pkh),
      pubkey_(pubkey)
    {}

    const unsigned int id() const { return id_; }
    const std::string host() const { return host_; }
    const std::string port() const { return port_; }
    const std::string p2pkh() const { return p2pkh_; }
    const std::string pubkey() const { return pubkey_; }

    const std::string grpc_server_uri() const {  return host_ + ":" + port_; }

  private:
    unsigned int id_;
    std::string host_;
    std::string port_;
    std::string p2pkh_;
    std::string pubkey_;
}; // class TransportConfig

class PbftConfig
{
  public:
    PbftConfig(std::string datadir, std::string configFileName = "miner.conf.json");

    const uint32_t id() const {return id_;}
    const std::string genesis_block_hash() const {return m_genesis_block_hash;}
    uint32_t genesis_block_timestamp() const {return m_genesis_block_timestamp;}
    double target_block_time() const {return m_target_block_time;}
    SIGNATURE_ALGO_TYPE signature_algorithm() const {return m_signature_algorithm;};
    std::string group_public_key() const {return m_group_public_key;}

    // Timing constants
    double C_REQUESTS_GENERATE_UNTIL_CURRENT_TIME_PLUS() const { return target_block_time(); }
    double C_PRE_PREPARE_ACCEPT_UNTIL_CURRENT_TIME_PLUS() const { return target_block_time()/10; }

    // Setters
    void set_replica_id(uint32_t replica_id){ id_ = replica_id; };
    void set_cluster_size(uint32_t cluster_size){ m_cluster_size = cluster_size; };
    void set_genesis_block_hash(std::string genesis_block_hash){m_genesis_block_hash = genesis_block_hash;}
    void set_genesis_block_timestamp(uint32_t genesis_block_timestamp){m_genesis_block_timestamp = genesis_block_timestamp;}
    void set_target_block_time(uint64_t target_block_time){m_target_block_time = target_block_time;}
    void set_pbft_db_reset(bool reset){ m_pbft_db_reset=reset; }
    void set_pbft_db_filename(std::string filename){ m_pbft_db_filename=filename; }
    void set_signature_algorithm(SIGNATURE_ALGO_TYPE sig_algo) { m_signature_algorithm = sig_algo; }
    const std::string test_rpc_server_uri() const { return "localhost:" + test_rpc_port_;}

    // If set, zmq messages from this replica will also be sent to this dish
    const std::optional<std::string> sniffer_dish_connection_string() const { return m_sniffer_dish_connection_string; }

    const std::string bitcoindJsonRpcEndpoint() const {
      return itcoin_rpchost_ + ":" + itcoin_rpcport_;
    }

    const std::string itcoin_uri() const {
      return "http://" + itcoin_rpc_auth_ + "@" + this->bitcoindJsonRpcEndpoint();
    }

    std::string getSignetChallenge() const;

    /**
     * Connection string to the itcoinblock topic exposed by the itcoin-core
     * process local to this replica.
     *
     * This value is computed from the bind string contained in the item
     * zmqpubitcoinblock in bitcoind.conf.
     *
     * For reference, see the "-zmqpubitcoinblock" option in itcoin-core and the
     * ZMQ_PUBITCOINBLOCK_PORT in the startup scripts initialize-itcoin-*.sh.
     *
     * See:
     *     http://api.zeromq.org/4-3:zmq-connect
     *     https://github.com/bancaditalia/itcoin-core/blob/itcoin/doc/zmq.md
     *
     * EXAMPLE:
     *     tcp://127.0.0.1:8080
     */
    std::string getItcoinblockConnectionString() const { return m_itcoinblock_connection_string; }

    const std::vector<TransportConfig> replica_set_v() const { return replica_set_v_; }

    const unsigned int cluster_size() const;

    std::string pbft_db_filename() const { return m_pbft_db_filename; }
    bool pbft_db_reset() const { return m_pbft_db_reset; }

  private:
    unsigned int id_;
    uint32_t m_cluster_size;
    std::string m_genesis_block_hash;
    uint32_t m_genesis_block_timestamp;
    double m_target_block_time;
    std::string m_group_public_key;

    std::string itcoin_rpchost_;
    std::string itcoin_rpcport_;
    std::string itcoin_rpc_auth_;
    std::string itcoin_signet_challenge_;
    std::string m_itcoinblock_connection_string;

    std::vector<TransportConfig> replica_set_v_;

    // Signature algorithm
    SIGNATURE_ALGO_TYPE m_signature_algorithm;

    // Pbft engine persistence
    bool m_pbft_db_reset;
    std::string m_pbft_db_filename;

    // Debug utilities
    // std::atomic<std::chrono::seconds> debug_delay_;
    std::string test_rpc_port_;

    // If set, zmq messages from this replica will also be sent to this dish
    std::optional<std::string> m_sniffer_dish_connection_string;

};

} // namespace itcoin

#endif //PBFT_CONFIG_H_
