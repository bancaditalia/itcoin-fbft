// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "state.h"

#include <SWI-cpp.h>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <string>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"

#include "../../utils/utils.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;

// utilities

static int prolog_engine_one_shot_call(const string predicate, const PlTermv args) {
  try {
    return PlCall(predicate.c_str(), args);
  } catch (PlException& ex) {
    BOOST_LOG_TRIVIAL(error) << (char*)ex;
    throw ex;
  }
}

namespace itcoin {
namespace fbft {
namespace state {

ReplicaState::ReplicaState(const itcoin::FbftConfig& conf, Blockchain& blockchain, RoastWallet& wallet,
                           uint32_t start_height, std::string start_hash, uint32_t start_time)
    : m_conf(conf), m_blockchain(blockchain), m_wallet(wallet) {
  Init(start_height, start_hash, start_time);
}

void ReplicaState::Init(uint32_t start_height, std::string start_hash, uint32_t start_time) {
  uint32_t replica_id = m_conf.id();
  uint32_t cluster_size = m_conf.cluster_size();
  uint32_t target_block_time = m_conf.target_block_time();

  BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% creating PL database with \
    cluster_size=%2% \
    start_height=%3%, \
    start_hash=%4%, \
    start_time=%5%, \
    genesis_block_timestamp=%6%, \
    target_block_time=%7%, \
    db_filename=%8%, \
    db_reset=%9%") % m_conf.id() % m_conf.cluster_size() %
                                  start_height % start_hash % start_time % m_conf.genesis_block_timestamp() %
                                  m_conf.target_block_time() % m_conf.fbft_db_filename() %
                                  m_conf.fbft_db_reset());
  PlTermv args(PlTerm((long)replica_id), PlTerm((long)cluster_size), PlTerm((long)start_height),
               PlString(start_hash.c_str()), PlTerm((long)start_time),
               PlTerm((long)m_conf.genesis_block_timestamp()), PlTerm((long)target_block_time),
               PlString(m_conf.fbft_db_filename().c_str()), PlTerm(m_conf.fbft_db_reset()));
  prolog_engine_one_shot_call("init", args);
}

void ReplicaState::ReceiveIncomingMessage(std::unique_ptr<messages::Message> msg) {
  // Adds the received message to the input message buffer
  m_in_msg_buffer.emplace_back(std::move(msg));

  // Update active actions
  UpdateActiveActions();
}

void ReplicaState::UpdateActiveActions() {
  // Clear the current active actions vector.
  m_active_actions.clear();

  /*
   * Fill result actions that depend on the input message buffer
   * The switch in the loop translate from Message to the corresponding ReceiveMessage action.
   */
  for (auto& msg : m_in_msg_buffer) {
    switch (msg->type()) {
    case messages::MSG_TYPE::BLOCK: {
      messages::Block typed_msg{dynamic_cast<messages::Block&>(*msg)};
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::ReceiveBlock>(m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::REQUEST: {
      messages::Request typed_msg = messages::Request(dynamic_cast<messages::Request&>(*msg));
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::ReceiveRequest>(m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::PREPARE: {
      messages::Prepare typed_msg = messages::Prepare(dynamic_cast<messages::Prepare&>(*msg));
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::ReceivePrepare>(m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::PRE_PREPARE: {
      messages::PrePrepare typed_msg = messages::PrePrepare(dynamic_cast<messages::PrePrepare&>(*msg));
      double current_time = this->current_time();
      double pre_prepare_time_tolerance_delta = m_conf.C_PRE_PREPARE_ACCEPT_UNTIL_CURRENT_TIME_PLUS();
      std::unique_ptr<actions::Action> action = std::make_unique<actions::ReceivePrePrepare>(
          m_conf.id(), m_blockchain, current_time, pre_prepare_time_tolerance_delta, typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::COMMIT: {
      messages::Commit typed_msg = messages::Commit(dynamic_cast<messages::Commit&>(*msg));
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::ReceiveCommit>(m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::VIEW_CHANGE: {
      messages::ViewChange typed_msg = messages::ViewChange(dynamic_cast<messages::ViewChange&>(*msg));
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::ReceiveViewChange>(m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::NEW_VIEW: {
      messages::NewView typed_msg = messages::NewView(dynamic_cast<messages::NewView&>(*msg));
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::ReceiveNewView>(m_wallet, m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::ROAST_PRE_SIGNATURE: {
      messages::RoastPreSignature typed_msg =
          messages::RoastPreSignature(dynamic_cast<messages::RoastPreSignature&>(*msg));
      RoastWallet& wallet = dynamic_cast<wallet::RoastWallet&>(m_wallet);
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::RoastReceivePreSignature>(wallet, m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    case messages::MSG_TYPE::ROAST_SIGNATURE_SHARE: {
      messages::RoastSignatureShare typed_msg =
          messages::RoastSignatureShare(dynamic_cast<messages::RoastSignatureShare&>(*msg));
      std::unique_ptr<actions::Action> action =
          std::make_unique<actions::RoastReceiveSignatureShare>(m_conf.id(), typed_msg);
      m_active_actions.emplace_back(std::move(action));
      break;
    }
    } // switch(msg->Type())
  }

  /*
   * Fill result with actions that depend on the current state
   */
  try {
    for (auto& p_action : actions::Execute::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::SendCommit::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::SendPrepare::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::SendPrePrepare::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::SendViewChange::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::RecoverView::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::SendNewView::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::ProcessNewView::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }

    for (auto& p_action : actions::RoastInit::BuildActives(m_conf, m_blockchain, m_wallet)) {
      m_active_actions.emplace_back(std::move(p_action));
    }
  } catch (PlException& ex) {
    BOOST_LOG_TRIVIAL(error) << (char*)ex;
    throw ex;
  }

  // On 2022 Nov 30, we experienced once the following:
  // 15398 [2022-Nov-30 16:04:57.933528] [error] error(resource_error(stack), stack_overflow{
  // choicepoints:3,depth:2,environments:3,globalused:895048,localused:2692,
  // stack:[frame(2,user:msg_log_commit(2,2,62,_192953056,2,_192953060),[]),
  // frame(1,user:pre_SEND_COMMIT("(H=62,
  // T=1669820690)",2,62,2),[]),frame(0,system:'$c_call_prolog',[])],stack_limit:1048576,trailused:618
  // }) terminate called after throwing an instance of 'PlException'
  // Similar to:
  // https://discourse.swi-prolog.org/t/stack-overflow-problem/520/4
  prolog_engine_one_shot_call("garbage_collect", PlTermv(0));

  for (unique_ptr<actions::Action>& action : m_active_actions) {
    BOOST_LOG_TRIVIAL(debug) << "R" << m_conf.id() << " action " << *action << " is active.";
  }
}

void ReplicaState::UpdateOutMessageBuffer() {
  try {
    // Clear the current out buffer vector.
    m_out_msg_buffer.clear();

    for (auto& p_msg : messages::PrePrepare::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }

    for (auto& p_msg : messages::Prepare::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }

    for (auto& p_msg : messages::Commit::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }

    for (auto& p_msg : messages::ViewChange::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }

    for (auto& p_msg : messages::NewView::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }

    for (auto& p_msg : messages::RoastPreSignature::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }

    for (auto& p_msg : messages::RoastSignatureShare::BuildToBeSent(m_conf.id())) {
      m_out_msg_buffer.emplace_back(std::move(p_msg));
    }
  } catch (PlException& ex) {
    BOOST_LOG_TRIVIAL(error) << (char*)ex;
    throw ex;
  }

  for (unique_ptr<messages::Message>& message : m_out_msg_buffer) {
    BOOST_LOG_TRIVIAL(trace) << "R" << m_conf.id() << " has " << *message << " in the output buffer";
  }
}

void ReplicaState::Apply(const actions::Action& action) {
  /*
   * Apply the effect of the action
   */
  int action_execution_success = 0;
  try {
    BOOST_LOG_TRIVIAL(debug) << "R" << m_conf.id() << " applying " << action.identify() << " effect.";
    action_execution_success = action.effect();
    if (!action_execution_success) {
      string error_msg = str(boost::format("R%1% cannot execute %2%") % m_conf.id() % action.identify());
      BOOST_LOG_TRIVIAL(error) << error_msg;
    }
  } catch (PlException& ex) {
    BOOST_LOG_TRIVIAL(error) << (char*)ex;
    throw ex;
  }

  /*
   * If the action was a ReceiveMessage type, update the two input buffers (normal one and awaiting checkpoint
   * one) accordingly
   */
  std::optional<std::reference_wrapper<const messages::Message>> processed_msg_opt = action.message();
  if (processed_msg_opt.has_value()) {
    const messages::Message& processed_msg = processed_msg_opt.value().get();

    // If the message was a succesfully applied BLOCK, we move all messages from the awaiting_checkpoint
    // buffer to the input buffer
    if (action_execution_success && processed_msg.type() == messages::MSG_TYPE::BLOCK) {
      // Perhaps this could be improved by having a map (height -> set of messages), or a specialized data
      // structure, rather than a vector. Still, since the number of messages in the buffers will be
      // relatively low, this could possibly perform even better than a map, despite the worse complexity.
      for (size_t i = 0; i < m_in_msg_awaiting_checkpoint_buffer.size(); i++) {
        unique_ptr<messages::Message> msg = move(m_in_msg_awaiting_checkpoint_buffer.at(i));
        BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% moving %2% from the awaiting checkpoint buffer") %
                                        std::to_string(m_conf.id()) % msg->identify());
        m_in_msg_buffer.emplace_back(move(msg));
      }
      m_in_msg_awaiting_checkpoint_buffer.clear();
    }

    // In any case we remove the corresponding message from the input message buffer
    for (size_t i = 0; i < m_in_msg_buffer.size(); i++) {
      const messages::Message& msg = *m_in_msg_buffer.at(i);
      if (msg == processed_msg) {
        // If the action could not be applied because the message refers, via sequence number, to a future
        // block We move the message in the awaiting_checkpoint buffer
        uint32_t h = this->h();
        if (!action_execution_success && processed_msg.seq_number_as_opt().has_value() &&
            processed_msg.seq_number_as_opt().value() == h + 2) {
          BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% moving %2% to the awaiting checkpoint buffer") %
                                          std::to_string(m_conf.id()) % msg.identify());
          m_in_msg_awaiting_checkpoint_buffer.emplace_back(
              std::move(m_in_msg_buffer.at(i))); // This makes msg a nullptr, not an issue, it will be removed
        }
        m_in_msg_buffer.erase(m_in_msg_buffer.begin() + i);
        break;
      }
    }
  }

  /*
   * Retrieve messages that need to be sent and add them to the output buffer
   */
  UpdateOutMessageBuffer();

  /*
   * Update active actions
   */
  UpdateActiveActions();
}

void ReplicaState::ClearOutMessageBuffer() {
  BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% clearing the output buffer") %
                                  std::to_string(m_conf.id()));
  // Clean the message out buffer both on the engine and on the vector
  prolog_engine_one_shot_call("msg_out_clear_all", PlTermv(PlTerm{(long)m_conf.id()}));
  m_out_msg_buffer.clear();
}

double ReplicaState::latest_request_time() const {
  PlTerm Max_t;
  int result =
      prolog_engine_one_shot_call("get_latest_request_time", PlTermv(PlTerm{(long)m_conf.id()}, Max_t));
  if (result) {
    return (double)Max_t;
  } else {
    return m_conf.genesis_block_timestamp();
  }
}

double ReplicaState::latest_reply_time() const {
  PlTerm Last_rep_t;
  int result = prolog_engine_one_shot_call("last_rep", PlTermv(PlTerm{(long)m_conf.id()}, Last_rep_t));
  if (result) {
    return (double)Last_rep_t;
  } else {
    return m_conf.genesis_block_timestamp();
  }
}

double ReplicaState::current_time() const {
  PlTerm Synthetic_time;
  prolog_engine_one_shot_call("get_synthetic_time", PlTermv(PlTerm{(long)m_conf.id()}, Synthetic_time));
  return (double)Synthetic_time;
}

uint32_t ReplicaState::h() const {
  PlTerm H;
  prolog_engine_one_shot_call("get_h", PlTermv(PlTerm{(long)m_conf.id()}, H));
  return (long)H;
}

uint32_t ReplicaState::primary() const {
  PlTerm Primary;
  prolog_engine_one_shot_call("primary", PlTermv(PlTerm{(long)view()}, Primary));
  return (long)Primary;
}

uint32_t ReplicaState::view() const {
  PlTerm View_i;
  prolog_engine_one_shot_call("view", PlTermv(PlTerm{(long)m_conf.id()}, View_i));
  return (long)View_i;
}

const std::vector<std::unique_ptr<messages::Message>>& ReplicaState::in_msg_buffer() const {
  return m_in_msg_buffer;
}

const std::vector<std::unique_ptr<messages::Message>>& ReplicaState::out_msg_buffer() const {
  return m_out_msg_buffer;
}

const std::vector<std::unique_ptr<actions::Action>>& ReplicaState::active_actions() const {
  return m_active_actions;
}

// Setters

void ReplicaState::set_synthetic_time(double time) {
  BOOST_LOG_TRIVIAL(debug) << str(boost::format("R%1% setting synthetic time = %2%") %
                                  std::to_string(m_conf.id()) % std::to_string(time));
  prolog_engine_one_shot_call("set_synthetic_time", PlTermv(PlTerm{(long)m_conf.id()}, PlTerm{(double)time}));

  /*
   * Update active actions
   */

  UpdateActiveActions();
}

} // namespace state
} // namespace fbft
} // namespace itcoin
