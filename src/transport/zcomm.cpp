// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "zcomm.h"

#include <csignal>

#include <boost/endian/conversion.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../fbft/messages/messages.h"

namespace {
static volatile std::sig_atomic_t s_interrupted = 0;
}

void signal_handler(int signal_value) {
  if ((signal_value != SIGINT) && (signal_value != SIGTERM)) {
    BOOST_LOG_TRIVIAL(error) << "Unsupported signal " << signal_value << ", ignoring";
  }
  s_interrupted = 1;
} // signal_handler()

#if BOOST_VERSION < 107200 // v1.72
/*
 * boost::endian::load_little_u32() and boost::endian::load_little_s32() were
 * introduced in boost 1.72. If we are on a previous version, we have to
 * polyfill them.
 */
namespace boost {
namespace endian {

inline boost::uint32_t load_little_u32(unsigned char const* p) BOOST_NOEXCEPT {
  return boost::endian::endian_load<boost::uint32_t, 4, order::little>(p);
}

inline boost::int32_t load_little_s32(unsigned char const* p) BOOST_NOEXCEPT {
  return boost::endian::endian_load<boost::int32_t, 4, order::little>(p);
}

} // namespace endian
} // namespace boost
#endif // polyfill for boost < 1.72

namespace itcoin {
namespace transport {

/*
 * Slight modification from:
 *     https://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa/16125797#16125797
 */
std::string stringToHex(const std::string& in) {
  std::stringstream ss;

  ss << std::hex << std::setfill('0');
  for (char i : in) {
    ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(i));
  }

  return ss.str();
} // stringToHex()

// From Little Endian hex string to integer
uint32_t bytesToInt(void* data, size_t size) {
  assert(size == 4);
  uint32_t result;
  std::memcpy(&result, data, size);
  return result;
}

std::optional<std::tuple<std::string, int32_t, uint32_t>>
decode_itcoinblock_payload(const std::string& bin_buffer) {
  if (bin_buffer.size() != ZComm::ITCOINBLOCK_MSG_SIZE) {
    BOOST_LOG_TRIVIAL(error) << "The message payload must be exactly " << ZComm::ITCOINBLOCK_MSG_SIZE
                             << " bytes. This one is " << bin_buffer.size() << " bytes";
    return std::nullopt;
  }

  const std::string hash_bin_buffer{(const char*)bin_buffer.data(), 32};

  // TODO: hash_bin_buffer è in little endian: controllare se lo sto decodificando bene
  const std::string hash_hex_string = stringToHex(hash_bin_buffer);

  int32_t block_height = boost::endian::load_little_s32(&(((const unsigned char*)bin_buffer.data())[32]));
  uint32_t block_time = boost::endian::load_little_u32(&(((const unsigned char*)bin_buffer.data())[36]));

  return {{hash_hex_string, block_height, block_time}};
} // decode_itcoinblock_payload()

ZComm::ZComm(const itcoin::FbftConfig& conf)
    : NetworkTransport{conf}, ctx{std::make_unique<zmq::context_t>()},
      my_group{std::string{"replica" + std::to_string(conf.id())}}, itcoinblock_topic_name{"itcoinblock"} {
  // setup dish (rx)
  this->dish_socket = std::make_unique<zmq::socket_t>(*(this->ctx), zmq::socket_type::dish);

  // build dish_bind_string
  std::string dish_bind_string = "tcp://*:" + m_conf.replica_set_v()[m_conf.id()].port();
  BOOST_LOG_TRIVIAL(info) << "Binding dish (for receiving broadcast messages from other replicas) to: "
                          << dish_bind_string;
  this->dish_socket->bind(dish_bind_string);

  /*
   * this replica must subscribe the topics produced by each of its peers,
   * excluding itself
   */
  for (auto replica_id = 0; replica_id < m_conf.cluster_size(); replica_id++) {
    if (replica_id == m_conf.id()) {
      continue;
    }
    const std::string group_name = "replica" + std::to_string(replica_id);
    BOOST_LOG_TRIVIAL(info) << "Joining group " << group_name;
    this->dish_socket->join(group_name.c_str());
  } // for (each node id)

  // setup radio (tx)
  this->radio_socket = std::make_unique<zmq::socket_t>(*(this->ctx), zmq::socket_type::radio);
  for (auto replica_id = 0; replica_id < m_conf.cluster_size(); replica_id++) {
    if (replica_id == m_conf.id()) {
      continue;
    }
    const TransportConfig replica_data = m_conf.replica_set_v()[replica_id];
    const std::string connection_string = "tcp://" + replica_data.host() + ":" + replica_data.port();
    BOOST_LOG_TRIVIAL(info) << "Connecting to: " << connection_string;
    this->radio_socket->connect(connection_string.c_str());
  } // for (each connection string)

  if (m_conf.sniffer_dish_connection_string().has_value()) {
    std::string sniffer_dish_connection_string{m_conf.sniffer_dish_connection_string().value()};

    BOOST_LOG_TRIVIAL(warning) << "Outgoing messages will also be sent to: "
                               << sniffer_dish_connection_string;
    this->radio_socket->connect(sniffer_dish_connection_string.c_str());
  }

  // setup rawblock (block notifications from itcoin-core)
  this->itcoin_sub_socket = std::make_unique<zmq::socket_t>(*(this->ctx), zmq::socket_type::sub);
  BOOST_LOG_TRIVIAL(info) << "itcoinblock: subscribing topic " << this->itcoinblock_topic_name << " on "
                          << conf.getItcoinblockConnectionString();
  this->itcoin_sub_socket->connect(conf.getItcoinblockConnectionString());
  this->itcoin_sub_socket->set(zmq::sockopt::subscribe, this->itcoinblock_topic_name);
} // ZComm::ZComm()

void ZComm::handler_dish(zmq::event_flags e) {
  if ((e & zmq::event_flags::pollin) != zmq::event_flags::none) {
    // event_flags::pollin bit is set in e
    zmq::message_t msg;
    zmq::recv_result_t res = this->dish_socket->recv(msg, zmq::recv_flags::none);
    if (res.has_value()) {
      BOOST_LOG_TRIVIAL(info) << "Received " << res.value() << " bytes from network on group " << msg.group();
      // emit the replica_message_received() signal
      this->replica_message_received(std::string{msg.group()}, msg.to_string());
    } else {
      BOOST_LOG_TRIVIAL(error) << "Errore durante la ricezione";
    }
  } else if (zmq::event_flags::none != (e & ~zmq::event_flags::pollout)) {
    throw std::runtime_error("Unexpected event type " + std::to_string(static_cast<short>(e)));
  }
} // ZComm::handler_dish()

void ZComm::handler_itcoin_block(zmq::event_flags e) {
  if ((e & zmq::event_flags::pollin) != zmq::event_flags::none) {
    // event_flags::pollin bit is set in e
    std::vector<zmq::message_t> recv_msgs;
    zmq::recv_result_t res;

    res = zmq::recv_multipart(*(this->itcoin_sub_socket), std::back_inserter(recv_msgs));

    // sanity checks on message structure
    if (res.has_value() == false) {
      BOOST_LOG_TRIVIAL(error) << "Errore durante la ricezione del messaggio multipart";
      return;
    }
    if (res.value() != 3) {
      BOOST_LOG_TRIVIAL(error) << "Ricevuto messaggio composto di " << res.value()
                               << " parti. Deve essere di 3";
      return;
    }
    if (res.value() != recv_msgs.size()) {
      BOOST_LOG_TRIVIAL(error) << "Res.value vale " << res.value() << ", mentre recv_msgs ha "
                               << recv_msgs.size() << " elementi. Inaccettabile.";
      return;
    }

    // part 1: topic name
    const std::string topic_name = recv_msgs[0].to_string();
    if (topic_name != this->itcoinblock_topic_name) {
      BOOST_LOG_TRIVIAL(error) << "Ricevuto nome topic inatteso: " << topic_name << " anziché "
                               << this->itcoinblock_topic_name;
      return;
    }

    // part 2: payload (hash,height,time)
    auto payload_decode_result = decode_itcoinblock_payload(recv_msgs[1].to_string());
    if (payload_decode_result.has_value() == false) {
      return;
    }
    auto [hash_hex_string, block_height, block_time] = payload_decode_result.value();

    // part 3: sequence number
    uint32_t seqNumber = bytesToInt(recv_msgs[2].data(), recv_msgs[2].size());

    // emit the rawblock_received() signal
    BOOST_LOG_TRIVIAL(trace) << "new block received. Hash: " << hash_hex_string
                             << ", height: " << block_height << ", time: " << block_time
                             << ", seqnum: " << seqNumber;
    this->itcoinblock_received(hash_hex_string, block_height, block_time, seqNumber);
  } else if (zmq::event_flags::none != (e & ~zmq::event_flags::pollout)) {
    throw std::runtime_error("Unexpected event type " + std::to_string(static_cast<short>(e)));
  }
} // ZComm::handler_itcoin_block()

int ZComm::run_forever() {
  /*
   * Setup custom signal handlers for SIGINT and SIGTERM. We will restore them
   * to their previous values if we ever exit this function.
   */
  sighandler_t prev_handler_sigint = std::signal(SIGINT, signal_handler);
  if (prev_handler_sigint == SIG_ERR) {
    BOOST_LOG_TRIVIAL(error) << "Cannot set signal handler for SIGINT. Quitting run_forever()";
    return EXIT_FAILURE;
  }

  sighandler_t prev_handler_sigterm = std::signal(SIGTERM, signal_handler);
  if (prev_handler_sigterm == SIG_ERR) {
    BOOST_LOG_TRIVIAL(error) << "Cannot set signal handler for SIGTERM. Quitting run_forever()";

    // restore the old handler for SIGINT
    std::signal(SIGINT, prev_handler_sigint);
    return EXIT_FAILURE;
  }

  zmq::active_poller_t poller;

  poller.add(*(this->dish_socket), zmq::event_flags::pollin,
             [&](zmq::event_flags e) { this->handler_dish(e); });
  poller.add(*(this->itcoin_sub_socket), zmq::event_flags::pollin,
             [&](zmq::event_flags e) { this->handler_itcoin_block(e); });

  // TODO: m_conf.target_block_time() should be a std::duration already, not a double
  std::chrono::milliseconds target_block_time{int(this->m_conf.target_block_time() * 1000)};

  std::chrono::milliseconds network_timeout;
  std::chrono::milliseconds elapsed{0};
  while (true) {
    try {
      std::chrono::steady_clock::time_point before_wait = std::chrono::steady_clock::now();

      std::chrono::milliseconds time_margin = (target_block_time - elapsed);
      if (time_margin.count() < 1) {
        BOOST_LOG_TRIVIAL(warning) << boost::format(
                                          "ACHTUNG: the system is operating without any time margin. "
                                          "target_block_time: %1% ms, cycle elapsed time: %2% ms") %
                                          target_block_time.count() % elapsed.count();
        network_timeout = std::chrono::milliseconds{1};
      } else {
        network_timeout = std::chrono::milliseconds{time_margin / 2};
      }
      // TODO REMOVE ME: Added to make experiments deterministic
      if (is_first_time) {
        // the first time is better to have bigger timeout in order to trigger view changes
        network_timeout = std::chrono::milliseconds{10000};
        is_first_time = false;
      } else {
        // A determinist and small timeout
        network_timeout = std::chrono::milliseconds{5};
      }
      // TODO REMOVE ME END
      BOOST_LOG_TRIVIAL(trace) << "WAITING AT MOST " << network_timeout.count() << " ms";

      auto event_count = poller.wait(network_timeout);
      std::chrono::steady_clock::time_point after_wait = std::chrono::steady_clock::now();

      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(after_wait - before_wait);

      BOOST_LOG_TRIVIAL(trace) << "ELAPSED: " << elapsed.count() << " ms, EVENT COUNT: " << event_count
                               << ", POLLING TIMEOUT WAS: " << network_timeout.count() << " ms";

      if (event_count > 0) {
        // at least an event happened on the network: start the cycle again
        continue;
      }

      /*
       * Nothing happened on the network: emit the network_timeout_expired()
       * signal
       */
      std::chrono::steady_clock::time_point before_idle = std::chrono::steady_clock::now();
      this->network_timeout_expired();
      std::chrono::steady_clock::time_point after_idle = std::chrono::steady_clock::now();
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(after_idle - before_idle);
    } catch (zmq::error_t& e) {
      BOOST_LOG_TRIVIAL(info) << "Interrupt received: " << e.what();
    }
    if (s_interrupted) {
      BOOST_LOG_TRIVIAL(info) << "interrupt received, exiting run_forever()";
      break;
    }
  } // while (true)

  /*
   * Restore the old handlers for SIGTERM and SIGINT.
   *
   * We ignore the return code here because there is no further cleanup we can
   * do.
   */
  std::signal(SIGINT, prev_handler_sigint);
  std::signal(SIGTERM, prev_handler_sigterm);

  return EXIT_SUCCESS;
} // ZComm::run_forever()

void ZComm::BroadcastMessage(std::unique_ptr<fbft::messages::Message> p_msg) {
  this->broadcast(p_msg->ToBinBuffer());
} // ZComm::BroadcastMessage()

void ZComm::broadcast(const std::string& bin_buffer) {
  zmq::message_t msg(bin_buffer);
  msg.set_group(this->my_group.c_str());

  BOOST_LOG_TRIVIAL(info) << "broadcasting " << bin_buffer.length() << " bytes on group " << msg.group();
  zmq::send_result_t res = this->radio_socket->send(msg, zmq::send_flags::none);
  if (res.has_value() == false) {
    BOOST_LOG_TRIVIAL(error) << "Error while trying to broadcast " << bin_buffer.length() << "bytes on group "
                             << msg.group();
  }
} // ZComm::broadcast()

ZComm::~ZComm() {} // ZComm::~ZComm()

} // namespace transport
} // namespace itcoin
