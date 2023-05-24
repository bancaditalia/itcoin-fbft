#include "fixtures.h"

#include <boost/log/expressions.hpp>

using namespace std;
using namespace itcoin::pbft::actions;
using namespace itcoin::pbft::messages;

namespace state = itcoin::pbft::state;

struct ViewChangePreparedFixture: ReplicaSetFixture { ViewChangePreparedFixture(): ReplicaSetFixture(4,0,60) {} };

BOOST_AUTO_TEST_SUITE(test_pbft_view_change_prepared, *utf::enabled())

BOOST_FIXTURE_TEST_CASE(test_pbft_view_change_prepared_00, ViewChangePreparedFixture)
{
try
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::trace
  );

  // Simulate receipt of a REQUEST message
  uint32_t req_timestamp = 60;
  Request request = Request(m_configs[0]->genesis_block_timestamp(), m_configs[0]->target_block_time(), req_timestamp);

  BOOST_LOG_TRIVIAL(debug) << "---------- Create Request with digest " << request.digest() << "at R0, R1, R2, R3";
  m_states[0]->set_synthetic_time(req_timestamp);
  m_states[1]->set_synthetic_time(req_timestamp);
  m_states[2]->set_synthetic_time(req_timestamp);
  m_states[3]->set_synthetic_time(req_timestamp);

  m_states[0]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[1]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[2]->ReceiveIncomingMessage(std::make_unique<Request>(request));
  m_states[3]->ReceiveIncomingMessage(std::make_unique<Request>(request));

  // We apply the results of the Receive REQUEST at all replicas

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  // Step 3. We apply the results of the Send PRE-PREPARE at replica 0
  // Now there should be no more active actions, but we should have a message in the output buffer

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  // Step 4. We send the PRE-PREPARE to other replicas
  PrePrepare pre_prepare_0{dynamic_cast<PrePrepare&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();

  m_states[1]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));
  m_states[2]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));
  m_states[3]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));

  // Step 6. We Receive the PRE_PREPARE to other replicas

  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  // Step 7. We apply the SEND_PREPARE to all replicas

  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  // Step 8. We receive PREPARE by Replica1 and Replica2 on Replica3, then we trigger a view change

  Prepare prepare_1{dynamic_cast<Prepare&>(*m_states[1]->out_msg_buffer().at(0))};
  m_states[1]->ClearOutMessageBuffer();

  Prepare prepare_2{dynamic_cast<Prepare&>(*m_states[2]->out_msg_buffer().at(0))};
  m_states[2]->ClearOutMessageBuffer();

  m_states[3]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare_1));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  m_states[3]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare_2));
  m_states[3]->Apply(*m_states[3]->active_actions().at(0));

  BOOST_LOG_TRIVIAL(debug) << "---------- Trigger VIEW_CHANGE";
  m_states[0]->set_synthetic_time(req_timestamp+m_configs[0]->target_block_time()/2+1);
  m_states[1]->set_synthetic_time(req_timestamp+m_configs[1]->target_block_time()/2+1);
  m_states[2]->set_synthetic_time(req_timestamp+m_configs[2]->target_block_time()/2+1);
  m_states[3]->set_synthetic_time(req_timestamp+m_configs[3]->target_block_time()/2+1);

  m_states[1]->UpdateActiveActions();
  m_states[2]->UpdateActiveActions();
  m_states[3]->UpdateActiveActions();

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::SEND_VIEW_CHANGE);
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::SEND_VIEW_CHANGE);
  BOOST_TEST(m_states[3]->active_actions().size() == 2u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::SEND_COMMIT);
  BOOST_CHECK(m_states[3]->active_actions().at(1)->type() == ACTION_TYPE::SEND_VIEW_CHANGE);

  BOOST_LOG_TRIVIAL(debug) << "---------- Apply SEND_VIEW_CHANGE at R1 and R2";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  m_states[3]->Apply(*m_states[3]->active_actions().at(1));

  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[1]->out_msg_buffer().at(0)->type() == MSG_TYPE::VIEW_CHANGE);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::VIEW_CHANGE);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 2u);
  BOOST_CHECK(m_states[3]->out_msg_buffer().at(0)->type() == MSG_TYPE::PREPARE);
  BOOST_CHECK(m_states[3]->out_msg_buffer().at(1)->type() == MSG_TYPE::VIEW_CHANGE);

  BOOST_LOG_TRIVIAL(debug) << "---------- R1 and R2 receive each other VIEW_CHANGE";
  // NB: ViewChange messages need to be signed, otherwise they will be forwarded without signatures
  ViewChange view_change_1{dynamic_cast<ViewChange&>(*m_states[1]->out_msg_buffer().at(0))};
  m_wallets[1]->AppendSignature(view_change_1);
  ViewChange view_change_2{dynamic_cast<ViewChange&>(*m_states[2]->out_msg_buffer().at(0))};
  m_wallets[2]->AppendSignature(view_change_2);
  ViewChange view_change_3{dynamic_cast<ViewChange&>(*m_states[3]->out_msg_buffer().at(1))};
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

  BOOST_LOG_TRIVIAL(debug) << "---------- Apply the receive view change at R1, R2";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));
  m_states[2]->Apply(*m_states[2]->active_actions().at(0));

  BOOST_LOG_TRIVIAL(debug) << "---------- R1 and R2 and R3 receive VIEW_CHANGEs from each other";
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

  BOOST_LOG_TRIVIAL(debug) << "---------- Apply SEND_NEW_VIEW at R1";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[1]->active_actions().at(0)->type() == ACTION_TYPE::PROCESS_NEW_VIEW);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[1]->out_msg_buffer().at(0)->type() == MSG_TYPE::NEW_VIEW);

  BOOST_LOG_TRIVIAL(debug) << "---------- R2 and R3 receive the NEW_VIEW message";
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

  BOOST_LOG_TRIVIAL(debug) << "---------- Apply RECEIVE_NEW_VIEW at R2 and R3";

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::PROCESS_NEW_VIEW);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 0u);

  m_states[3]->Apply(*m_states[3]->active_actions().at(0));
  BOOST_TEST(m_states[3]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[3]->active_actions().at(0)->type() == ACTION_TYPE::PROCESS_NEW_VIEW);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 0u);

  m_states[3]->Apply(*m_states[1]->active_actions().at(0));

  BOOST_LOG_TRIVIAL(debug) << "---------- Apply PROCESS_NEW_VIEW at R1";
  m_states[1]->Apply(*m_states[1]->active_actions().at(0));

  BOOST_TEST(m_states[1]->active_actions().size() == 0u);
  BOOST_TEST(m_states[1]->out_msg_buffer().size() == 0u);

  BOOST_LOG_TRIVIAL(debug) << "---------- Apply PROCESS_NEW_VIEW at R2 and R3";

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->active_actions().size() == 0u);
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::PREPARE);

  m_states[3]->Apply(*m_states[3]->active_actions().at(0));
  BOOST_TEST(m_states[3]->active_actions().size() == 0u);
  BOOST_TEST(m_states[3]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[3]->out_msg_buffer().at(0)->type() == MSG_TYPE::PREPARE);

  // BOOST_FAIL("Test DID NOT FAIL. This is a failure placeholder used just to print all dynamic predicates");
}
catch(const std::exception& e)
{
  BOOST_LOG_TRIVIAL(error) << e.what();
  BOOST_CHECK_NO_THROW( throw e );
}
}

BOOST_AUTO_TEST_SUITE_END() // test_pbft_view_change_prepared
