// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_TRANSPORT_ZCOMM_H
#define ITCOIN_TRANSPORT_ZCOMM_H

#include "config/FbftConfig.h"
#include "network.h"
#include <boost/signals2.hpp>

#define ZMQ_BUILD_DRAFT_API
#include <zmq_addon.hpp>

namespace bs2 = boost::signals2;

namespace itcoin {

namespace fbft {
namespace messages {
class Message;
} // namespace messages
} // namespace fbft

namespace transport {

using namespace std::chrono_literals;

/**
 * Decodes the itcoinblock payload according to the encoding defined in
 * https://github.com/bancaditalia/itcoin-core/blob/itcoin/doc/zmq.md.
 *
 * bin_buffer must be a 40-bytes string treated as binary buffer.
 *
 * Returns:
 * - block hash represented as hex string
 * - block height
 * - block time
 *
 * Please note that the values are already decoded: this function, for example,
 * will return numbers in the machine-native endiannes.
 *
 * In case of decoding errors, prints a log and returns std::nullopt.
 */
std::optional<std::tuple<std::string, int32_t, uint32_t>>
decode_itcoinblock_payload(const std::string& bin_buffer);

class ZComm : public network::NetworkTransport {
public:
  /**
   * Configures a ZComm object, binds the dish socket and connects to the
   * dishes of the other replicas and to the pub socket of the itcoin-core
   * process local to this replica.
   *
   * The object will automatically take care of reconnections,
   * retransmissions and discarding of new messages when the send queue is
   * full.
   *
   * conf:
   *     A FbftConfig object
   */
  ZComm(const itcoin::FbftConfig& conf);

  /**
   * Broadcasts a message to all the replicas configured in
   * connection_strings_towards_replicas (see constructor).
   *
   * If some replicas are offline, the messages will be delivered when they
   * come back online.
   *
   * The messages will be sent on a group named "replicaX", where X is my_id.
   */
  void broadcast(const std::string& bin_buffer);

  void BroadcastMessage(std::unique_ptr<fbft::messages::Message> p_msg);

  /**
   * Runs forever. Relevant events are published via the following
   * boost.signals2 signals:
   * - replica_message_received, if a message from a replica was received;
   * - itcoinblock_received, if the itcoin-core process local to this miner
   *   has notified us of the appearance of a new block;
   * - network_timeout_expired, if there was no network traffic for more than
   *   half the target_block_time.
   *
   * If SIGINT or SIGTERM are caught, returns EXIT_SUCCESS.
   *
   * If there are problems installing the unix signal handlers, writes an
   * error log and immediately returns EXIT_FAILURE.
   */
  int run_forever();

  /**
   * typedef for the signal emitted when receiving a message from a miner
   * replica: (group_name, bin_buffer)
   */
  typedef bs2::signal<void(std::string, std::string)> SigReplicaMessageReceived_t;

  /**
   * typedef for the signal emitted when receiving a itcoinblock: (block hash
   * as hex string, height, time, sequence_number)
   */
  typedef bs2::signal<void(const std::string&, int32_t, uint32_t, uint32_t)> SigItcoinBlockReceived_t;

  /**
   * typedef for the signal emitted when no events have happened on the
   * network for half the expected cycle time (target_block_time / 2)
   */
  typedef bs2::signal<void(void)> SigNetworkTimeoutExpired_t;

  SigReplicaMessageReceived_t replica_message_received;
  SigItcoinBlockReceived_t itcoinblock_received;
  SigNetworkTimeoutExpired_t network_timeout_expired;

  /**
   * Messages on the "itcoinblock" topic must be of a fixed size of 40 bits
   * (see https://github.com/bancaditalia/itcoin-core/blob/itcoin/doc/zmq.md).
   */
  static constexpr uint16_t ITCOINBLOCK_MSG_SIZE = 40;

  ~ZComm();

private:
  bool is_first_time =
      true; // TODO REMOVE: the first time is better to have bigger timeout in order to trigger view changes

  const std::string my_group;
  const std::string itcoinblock_topic_name;

  std::unique_ptr<zmq::context_t> ctx;

  /**
   * outgoing messages for the other replicas are sent through this zmq socket
   */
  std::unique_ptr<zmq::socket_t> radio_socket;

  /**
   * incoming messages from the other replicas are received here
   */
  std::unique_ptr<zmq::socket_t> dish_socket;

  /**
   * new block notifications from the itcoin-core process local to this
   * replica are received on this zmq sub socket
   */
  std::unique_ptr<zmq::socket_t> itcoin_sub_socket;

  void handler_dish(zmq::event_flags e);
  void handler_itcoin_block(zmq::event_flags e);
}; // class ZComm

} // namespace transport
} // namespace itcoin

#endif // ITCOIN_TRANSPORT_ZCOMM_H
