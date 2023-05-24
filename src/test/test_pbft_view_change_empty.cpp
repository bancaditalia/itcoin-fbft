#include "fixtures.h"

using namespace std;
using namespace itcoin::pbft::actions;
using namespace itcoin::pbft::messages;

struct ViewChangeEmptyFixture: ReplicaStateFixture<> { ViewChangeEmptyFixture(): ReplicaStateFixture(itcoin::SIGNATURE_ALGO_TYPE::NAIVE,4,0,60) {} };

BOOST_AUTO_TEST_SUITE(test_pbft_view_change_empty, *utf::enabled())

BOOST_FIXTURE_TEST_CASE(test_pbft_view_change_empty_00, ViewChangeEmptyFixture)
{
try
{
  // Simulate receipt of a REQUEST message
  uint32_t req_timestamp = 60;
  Request request = Request(m_configs[0]->genesis_block_timestamp(), m_configs[0]->target_block_time(), req_timestamp);

  BOOST_LOG_TRIVIAL(debug) << "Create Request with digest " << request.digest() << "at R0, R1, R2, R3";
  m_states[0]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[1]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[2]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[3]->ReceiveIncomingMessage(std::make_unique<Request>(request));

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);
  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);
  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_REQUEST);

  BOOST_LOG_TRIVIAL(debug) << "Apply the receive request at R0, R1, R2, R3";
  m_states[0]->set_synthetic_time(req_timestamp);
  m_states[1]->set_synthetic_time(req_timestamp);
  m_states[2]->set_synthetic_time(req_timestamp);
  m_states[3]->set_synthetic_time(req_timestamp);

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PRE_PREPARE);
  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);

  BOOST_LOG_TRIVIAL(debug) << "After 10+30 seconds, all backups should activate the view change";
  m_states[1]->set_synthetic_time(req_timestamp+m_configs[1]->target_block_time()/2+1);
  m_states[2]->set_synthetic_time(req_timestamp+m_configs[2]->target_block_time()/2+1);
  m_states[3]->set_synthetic_time(req_timestamp+m_configs[3]->target_block_time()/2+1);

  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PRE_PREPARE);
  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::SEND_VIEW_CHANGE);
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::SEND_VIEW_CHANGE);
  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::SEND_VIEW_CHANGE);

  BOOST_LOG_TRIVIAL(debug) << "Apply SEND_VIEW_CHANGE at R1 and R2";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[1]->out_msg_buffer().at(0)->type() == MSG_TYPE::VIEW_CHANGE);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::VIEW_CHANGE);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[3]->out_msg_buffer().at(0)->type() == MSG_TYPE::VIEW_CHANGE);

  BOOST_LOG_TRIVIAL(debug) << "R1 and R2 receive each other VIEW_CHANGE";
  ViewChange view_change_1{dynamic_cast<ViewChange&>(*m_states[1]->out_msg_buffer().at(0))};
  m_wallets[1]->AppendSignature(view_change_1);
  ViewChange view_change_2{dynamic_cast<ViewChange&>(*m_states[2]->out_msg_buffer().at(0))};
  m_wallets[2]->AppendSignature(view_change_2);
  ViewChange view_change_3{dynamic_cast<ViewChange&>(*m_states[3]->out_msg_buffer().at(0))};
  m_wallets[3]->AppendSignature(view_change_3);
  m_states[1]->ClearOutMessageBuffer();
  m_states[2]->ClearOutMessageBuffer();
  m_states[3]->ClearOutMessageBuffer();

  m_states[1]->ReceiveIncomingMessage(std::make_unique<ViewChange>(view_change_2));
  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_VIEW_CHANGE);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);

  m_states[2]->ReceiveIncomingMessage(std::make_unique<ViewChange>(view_change_1));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_VIEW_CHANGE);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  BOOST_LOG_TRIVIAL(debug) << "Apply the receive view change at R1, R2";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));

  BOOST_LOG_TRIVIAL(debug) << "R1 and R2 and R3 receive VIEW_CHANGEs from each other";
  m_states[1]->ReceiveIncomingMessage(std::make_unique<ViewChange>(view_change_3));
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));

  m_states[2]->ReceiveIncomingMessage(std::make_unique<ViewChange>(view_change_3));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));

  m_states[3]->ReceiveIncomingMessage(std::make_unique<ViewChange>(view_change_1));
  m_states[3]->ReceiveIncomingMessage(std::make_unique<ViewChange>(view_change_2));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  // R1, being the primary of the new view, should send the new view
  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::SEND_NEW_VIEW);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);

  // R2 and R3, being backups of the new view, shoud do nothing
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  BOOST_TEST(m_states[3]->active_actions().size() == 0u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);

  BOOST_LOG_TRIVIAL(debug) << "Apply SEND_NEW_VIEW at R1";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::PROCESS_NEW_VIEW);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[1]->out_msg_buffer().at(0)->type() == MSG_TYPE::NEW_VIEW);

  BOOST_LOG_TRIVIAL(debug) << "R2 and R3 receive the NEW_VIEW message";
  NewView new_view_1{dynamic_cast<NewView&>(*m_states[1]->out_msg_buffer().at(0))};
  new_view_1.Sign(*m_wallets[1]);
  m_states[1]->ClearOutMessageBuffer();

  m_states[2]->ReceiveIncomingMessage(std::make_unique<NewView>(new_view_1));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_NEW_VIEW);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  m_states[3]->ReceiveIncomingMessage(std::make_unique<NewView>(new_view_1));
  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_NEW_VIEW);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  BOOST_LOG_TRIVIAL(debug) << "Apply RECEIVE_NEW_VIEW at R2 and R3";

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::PROCESS_NEW_VIEW);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  m_states[3]->Apply(*m_states[3]->active_actions().at(0));
  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::PROCESS_NEW_VIEW);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);

  m_states[3]->Apply(*m_states[1]->active_actions().at(0));

  BOOST_LOG_TRIVIAL(debug) << "Apply PROCESS_NEW_VIEW at R1";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::SEND_PRE_PREPARE);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);

  BOOST_LOG_TRIVIAL(debug) << "Apply PROCESS_NEW_VIEW at R2 and R3";

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  m_states[3]->Apply(*m_states[3]->active_actions().at(0));
  BOOST_TEST(m_states[3]->active_actions().size() == 0u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);

  // BOOST_FAIL("Test DID NOT FAIL. This is a failure placeholder used just to print all dynamic predicates");
}
catch(const std::exception& e)
{
  BOOST_LOG_TRIVIAL(error) << e.what();
  BOOST_CHECK_NO_THROW( throw e );
}
}

BOOST_AUTO_TEST_SUITE_END() // test_pbft_view_change_empty
