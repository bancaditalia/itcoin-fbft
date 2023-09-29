// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures/fixtures.h"

using namespace std;
using namespace itcoin::fbft::actions;
using namespace itcoin::fbft::messages;

namespace state = itcoin::fbft::state;

struct NormalOperationFixture : ReplicaStateFixture {
  NormalOperationFixture() : ReplicaStateFixture(4, 0, 60) {}
};

BOOST_AUTO_TEST_SUITE(test_fbft_normal_operation, *utf::enabled())

BOOST_FIXTURE_TEST_CASE(test_fbft_normal_operation_00, NormalOperationFixture) {
  // Step 1. Simulate Receipt of a REQUEST message by replica 0
  uint32_t req_timestamp = 60;
  Request request =
      Request(m_configs[0]->genesis_block_timestamp(), m_configs[0]->target_block_time(), req_timestamp);

  BOOST_LOG_TRIVIAL(debug) << "Simulating the creation of request with digest at all replicas = "
                           << request.digest();
  m_states[0]->set_synthetic_time(req_timestamp);
  m_states[1]->set_synthetic_time(req_timestamp);
  m_states[2]->set_synthetic_time(req_timestamp);
  m_states[3]->set_synthetic_time(req_timestamp);

  m_states[0]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[1]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[2]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[3]->ReceiveIncomingMessage(std::make_unique<Request>(request));

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);

  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);

  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);

  // We apply the results of the Receive REQUEST at all replicas

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PRE_PREPARE);

  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);

  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  BOOST_TEST(m_states[3]->active_actions().size() == 0u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);

  // Step 3. We apply the results of the Send PRE-PREPARE at replica 0
  // Now there should be no more active actions, but we should have a message in the output buffer

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  BOOST_TEST(m_states[0]->active_actions().size() == 0u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[0]->out_msg_buffer().at(0)->type() == MSG_TYPE::PRE_PREPARE);

  // Step 4. We send the PRE-PREPARE to other replicas

  // First increase timestamp otherwise PRE_PREPARE cannot be accepted
  m_states[0]->set_synthetic_time(60);
  m_states[1]->set_synthetic_time(60);
  m_states[2]->set_synthetic_time(60);
  m_states[3]->set_synthetic_time(60);

  PrePrepare pre_prepare_0{dynamic_cast<PrePrepare&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();

  m_states[1]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));
  m_states[2]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));
  m_states[3]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_PRE_PREPARE);

  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_PRE_PREPARE);

  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_PRE_PREPARE);

  // Step 6. We Receive the PRE_PREPARE to other replicas

  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PREPARE);

  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PREPARE);

  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PREPARE);

  // Step 7. We apply the SEND_PREPARE to all replicas

  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[1]->out_msg_buffer().at(0)->type() == MSG_TYPE::PREPARE);

  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::PREPARE);

  BOOST_TEST(m_states[3]->active_actions().size() == 0u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[3]->out_msg_buffer().at(0)->type() == MSG_TYPE::PREPARE);

  // Step 8. We receive PREPARE by Replica1 and Replica2 on Replica0, then we expect a commit

  Prepare prepare_1{dynamic_cast<Prepare&>(*m_states[1]->out_msg_buffer().at(0))};
  m_states[1]->ClearOutMessageBuffer();
  Prepare prepare_2{dynamic_cast<Prepare&>(*m_states[2]->out_msg_buffer().at(0))};
  m_states[2]->ClearOutMessageBuffer();
  // We assume Replica3 has failed
  m_states[3]->ClearOutMessageBuffer();

  m_states[0]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare_1));
  m_states[0]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare_2));

  BOOST_TEST(m_states[0]->active_actions().size() == 2u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_PREPARE);
  BOOST_CHECK(m_states[0]->active_actions().at(1)->type() == ACTION_TYPE::RECEIVE_PREPARE);

  ReceivePrepare receive_prepare_0 =
      ReceivePrepare(dynamic_cast<ReceivePrepare&>(*m_states[0]->active_actions().at(0)));
  ReceivePrepare receive_prepare_1 =
      ReceivePrepare(dynamic_cast<ReceivePrepare&>(*m_states[0]->active_actions().at(1)));

  m_states[0]->Apply(receive_prepare_0);
  m_states[0]->Apply(receive_prepare_1);

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::SEND_COMMIT);

  // Step 10. We apply the SEND_COMMIT on Replica0, we expect a Commit message in the output buffer

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  BOOST_TEST(m_states[0]->active_actions().size() == 0u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[0]->out_msg_buffer().at(0)->type() == MSG_TYPE::COMMIT);

  // Step 11. We receive the PREPARE by Replica2 on Replica1

  m_states[1]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare_2));
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_PREPARE);
  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);

  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::SEND_COMMIT);

  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[1]->out_msg_buffer().at(0)->type() == MSG_TYPE::COMMIT);

  // We receive the PREPARE by Replica1 on Replica2

  m_states[2]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare_1));
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_PREPARE);
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::SEND_COMMIT);

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::COMMIT);

  // We receive COMMIT by Replica1 and Replica2 on Replica0, then we expect an execute

  Commit commit_0{dynamic_cast<Commit&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();
  Commit commit_1{dynamic_cast<Commit&>(*m_states[1]->out_msg_buffer().at(0))};
  m_states[1]->ClearOutMessageBuffer();
  Commit commit_2{dynamic_cast<Commit&>(*m_states[2]->out_msg_buffer().at(0))};
  m_states[2]->ClearOutMessageBuffer();

  m_states[0]->ReceiveIncomingMessage(std::make_unique<Commit>(commit_1));
  m_states[0]->ReceiveIncomingMessage(std::make_unique<Commit>(commit_2));

  BOOST_TEST(m_states[0]->active_actions().size() == 2u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_COMMIT);
  BOOST_CHECK(m_states[0]->active_actions().at(1)->type() == ACTION_TYPE::RECEIVE_COMMIT);

  ReceiveCommit receive_commit_0 =
      ReceiveCommit(dynamic_cast<ReceiveCommit&>(*m_states[0]->active_actions().at(0)));
  ReceiveCommit receive_commit_1 =
      ReceiveCommit(dynamic_cast<ReceiveCommit&>(*m_states[0]->active_actions().at(1)));

  m_states[0]->Apply(receive_commit_0);
  m_states[0]->Apply(receive_commit_1);

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_INIT);

  // BOOST_FAIL("Test DID NOT FAIL. This is a failure placeholder used just to print all dynamic predicates");
} // test_fbft_normal_operation_00

BOOST_AUTO_TEST_SUITE_END() // test_fbft_normal_operation
