// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "Replica2.h"

#include <thread>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::network;
using namespace itcoin::wallet;
using namespace itcoin::fbft::messages;

namespace itcoin {
namespace fbft {

Replica2::Replica2(
  const itcoin::FbftConfig& config,
  Blockchain& blockchain,
  RoastWallet& wallet,
  NetworkTransport& transport,
  uint32_t start_height,
  std::string start_hash,
  uint32_t start_time
):
ReplicaState(config, blockchain, wallet, start_height, start_hash, start_time),
m_transport(transport)
{
  // use current time as seed for random generator
  std::srand(std::time(nullptr));
}

const uint32_t Replica2::id() const
{
  return m_conf.id();
}

void Replica2::GenerateRequests()
{
  // Constants
  uint32_t genesis_block_time = m_conf.genesis_block_timestamp();
  uint32_t target_block_time = m_conf.target_block_time();

  // Generate the request, if needed
  uint32_t current_time = this->current_time();
  uint32_t last_req_time = this->latest_request_time();
  uint32_t last_rep_time = this->latest_reply_time();

  // Always ensure there is exactly MAX_NUM_GENERATED_REQUESTS request in the future
  // This number may need to become adaptive
  uint32_t MAX_NUM_GENERATED_REQUESTS = 5;

  while(
    last_req_time < current_time + MAX_NUM_GENERATED_REQUESTS*target_block_time &&
    last_req_time < last_rep_time + MAX_NUM_GENERATED_REQUESTS*target_block_time
  )
  {
    uint32_t req_timestamp = last_req_time + target_block_time;
    BOOST_LOG_TRIVIAL(debug) << str(
      boost::format("R%1% last_req_time=%2% < current_time + delta = %3% and < last_rep_time + delta = %4%, creating request with H=%5% and T=%6%.")
        % m_conf.id()
        % last_req_time
        % (current_time + MAX_NUM_GENERATED_REQUESTS*target_block_time)
        % (last_rep_time + MAX_NUM_GENERATED_REQUESTS*target_block_time)
        % ((req_timestamp-genesis_block_time) / target_block_time)
        % req_timestamp
    );
    messages::Request req = messages::Request(genesis_block_time, target_block_time, req_timestamp);
    actions::ReceiveRequest receive_req(m_conf.id(), req);
    this->Apply(receive_req);
    last_req_time = this->latest_request_time();
  }

  if (last_req_time >= current_time + MAX_NUM_GENERATED_REQUESTS*target_block_time)
  {
    BOOST_LOG_TRIVIAL(trace) << str(
      boost::format("R%1% last_req_time=%2% >= current_time + delta = %3%, stop creating requests.")
        % m_conf.id()
        % last_req_time
        % (current_time + MAX_NUM_GENERATED_REQUESTS*target_block_time)
    );
  }
  else
  {
    BOOST_LOG_TRIVIAL(trace) << str(
      boost::format("R%1% last_req_time=%2% >= last_rep_time + delta = %3%, stop creating requests.")
        % m_conf.id()
        % last_req_time
        % (last_rep_time + MAX_NUM_GENERATED_REQUESTS*target_block_time)
    );
  }
}

void Replica2::ApplyActiveActions()
{
  // We execute an active action
  uint32_t num_applied_actions = 0; uint32_t MAX_NUM_APPLIED_ACTIONS = 11;
  while (!m_active_actions.empty() && num_applied_actions<MAX_NUM_APPLIED_ACTIONS)
  {
    // We apply a random action
    size_t num_actions = m_active_actions.size();
    int index_random = std::rand() % num_actions;
    std::unique_ptr<actions::Action> action_to_apply= move(m_active_actions.at(index_random));
    this->Apply( *action_to_apply );
    num_applied_actions += 1;

    // We broadcast all messages in the output buffer
    if (!m_out_msg_buffer.empty())
    {
      std::vector<unique_ptr<messages::Message>> ready_to_be_sent{};
      for (auto& p_msg: m_out_msg_buffer)
      {
        // i-th element in out_msg_buffer is nullptr after move, but still present
        ready_to_be_sent.emplace_back(move(p_msg));
      }
      this->ClearOutMessageBuffer();

      for (auto& p_msg: ready_to_be_sent)
      {
        p_msg->Sign(m_wallet);

        // A special trick needs to be applied for ROAST_PRE_SIGNATURE message
        // This message is sent by the ROAST coordinator to all the candidate signers of the signature session
        // In 5FBFT the Primary may be both the ROAST coordinator and signer for that specific session
        // In that special case, we need to inject the message in the input buffer of the coordinator
        if (p_msg->type()==MSG_TYPE::ROAST_PRE_SIGNATURE)
        {
          unique_ptr<messages::RoastPreSignature> typed_msg = make_unique<messages::RoastPreSignature>( dynamic_cast<messages::RoastPreSignature&>(*p_msg) );

          bool replica_id_found = false;
          for (uint32_t signer_id : typed_msg->signers())
          {
            if (signer_id == m_conf.id())
            {
              replica_id_found = true;
              break;
            }
          }
          if (replica_id_found)
          {
            this->m_in_msg_buffer.emplace_back(std::move(typed_msg));
          }
        }
        else if (p_msg->type()==MSG_TYPE::ROAST_SIGNATURE_SHARE)
        {
          // Similarly, the ROAST_SIGNATURE_SHARE should be sent to the coordinator only
          // Here we ship the signature share also to current replica, in case it's the coordinator
          unique_ptr<messages::RoastSignatureShare> typed_msg = make_unique<messages::RoastSignatureShare>( dynamic_cast<messages::RoastSignatureShare&>(*p_msg) );
          this->m_in_msg_buffer.emplace_back(std::move(typed_msg));
        }

        // Actual broadcast
        m_transport.BroadcastMessage(move(p_msg));
      }
    }
  }
  if (num_applied_actions == MAX_NUM_APPLIED_ACTIONS)
  {
    string error_msg = str(
      boost::format("R%1% exceeded the number of applied actions!")
        % id()
    );
    // throw(std::runtime_error(error_msg));
    BOOST_LOG_TRIVIAL(error) << error_msg;
  }
  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% does not have further active actions to apply.")
      % m_conf.id()
  );
}

void Replica2::CheckTimedActions()
{
  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% cycle start.")
      % m_conf.id()
  );

  // Generate requests
  this->GenerateRequests();

  // Update active actions at the beginning, since a sleep
  // may trigger timeouts
  this->UpdateActiveActions();

  // Apply active actions resulting from timeout expired
  this->ApplyActiveActions();

  BOOST_LOG_TRIVIAL(trace) << str(
    boost::format("R%1% cycle end.")
      % m_conf.id()
  );
}

void Replica2::ReceiveIncomingMessage(std::unique_ptr<messages::Message> msg)
{
  BOOST_LOG_TRIVIAL(debug) << str(
    boost::format("R%1% receiving %2% from %3%.")
      % m_conf.id()
      % msg->identify()
      % msg->sender_id()
  );

  // Generate requests
  this->GenerateRequests();

  if( msg->type()==messages::MSG_TYPE::BLOCK )
  {
    // Checkpoint messages are not signed, since they are received upon
    // receipt of a valid (hence signed) block.
    messages::Block typed_msg{ dynamic_cast<messages::Block&>(*msg) };
    actions::ReceiveBlock receive_block(m_conf.id(), typed_msg);
    this->Apply(receive_block);

    // When we receive a block, we return to prevent a replica that is receiving blocks (e.g. resync)
    // to trigger view changes
  }
  // Here the message is not a block, we check signature
  else if ( msg->VerifySignatures(m_wallet) )
  {
    ReplicaState::ReceiveIncomingMessage(move(msg));
    // Apply active actions resulting from the received, non-block message
    this->ApplyActiveActions();
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << str(
      boost::format("R%1% received message %2% from R%3% with invalid signature, discarding.")
        % m_conf.id()
        % msg->name()
        % msg->sender_id()
    );
  }

  BOOST_LOG_TRIVIAL(debug) << str(
    boost::format("R%1% receive message end.")
      % m_conf.id()
  );
}

}
}
