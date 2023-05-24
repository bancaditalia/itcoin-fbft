#include <boost/test/unit_test.hpp>

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "fixtures.h"

namespace utf = boost::unit_test;
namespace state = itcoin::pbft::state;

struct V5FrostedBftFixture: ReplicaStateFixture<DummyRoastWallet> { V5FrostedBftFixture(): ReplicaStateFixture(itcoin::SIGNATURE_ALGO_TYPE::ROAST,4,0,60) {} };

BOOST_AUTO_TEST_SUITE(test_5frosted_bft, *utf::enabled())

BOOST_FIXTURE_TEST_CASE(test_5frosted_bft_00, V5FrostedBftFixture)
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::trace
  );

  // Receive REQUEST at all replicas
  uint32_t REQ_TS = 60;
  Request request = Request(m_configs[0]->genesis_block_timestamp(), m_configs[0]->target_block_time(), REQ_TS);
  for (int i=0; i<4; i++)
  {
    m_states[i]->Apply(ReceiveRequest(i, request));
  }

  // Advance replica time, so that they process the request
  set_synthetic_time(REQ_TS);

  // Step 3. We apply the results of the Send PRE-PREPARE at replica 0
  // Now there should be no more active actions, but we should have a message in the output buffer

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  // Receive PRE_PREPARE at all backups

  PrePrepare pre_prepare_0{dynamic_cast<PrePrepare&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();

  for (int rid=1; rid<4; rid++)
  {
    m_states[rid]->ReceiveIncomingMessage(std::make_unique<PrePrepare>(pre_prepare_0));
    m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
  }

  // Step 7. We apply the SEND_PREPARE to all replicas

  for (int rid=1; rid<4; rid++)
  {
    m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
  }

  // Receive PREPARE at all replica

  for (int sid=1; sid<4; sid++)
  {
    Prepare prepare{dynamic_cast<Prepare&>(*m_states[sid]->out_msg_buffer().at(0))};
    for (int rid=0; rid<4; rid++)
    {
      if (rid != sid)
      {
        m_states[rid]->ReceiveIncomingMessage(std::make_unique<Prepare>(prepare));
        m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
      }
    }
    m_states[sid]->ClearOutMessageBuffer();
  }

  // Step 7. We apply the SEND_PREPARE to all replicas

  for (int rid=0; rid<4; rid++)
  {
    m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
  }

  // Receive COMMIT at all replica

  for (int sid=0; sid<4; sid++)
  {
    Commit commit{dynamic_cast<Commit&>(*m_states[sid]->out_msg_buffer().at(0))};
    for (int rid=0; rid<4; rid++)
    {
      if (rid != sid)
      {
        m_states[rid]->ReceiveIncomingMessage(std::make_unique<Commit>(commit));
        m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
      }
    }
    m_states[sid]->ClearOutMessageBuffer();
  }

  // At this stage PRE_ROAST_INIT should be active for all replica

  for (int rid=0; rid<4; rid++)
  {
    BOOST_TEST(m_states[rid]->out_msg_buffer().size() == 0u);
    BOOST_TEST(m_states[rid]->active_actions().size() == 1u);
    BOOST_CHECK(m_states[rid]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_INIT);
    m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
  }

  //

  // msg_out_roast_pre_signature(0, [0, 1], "pre_sig_share_0+pre_sig_share_1").
  // msg_out_roast_pre_signature(0, [2, 3], "pre_sig_share_2+pre_sig_share_3").
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 2u);
  RoastPreSignature roast_pre_sig_0_1{dynamic_cast<RoastPreSignature&>(*m_states[0]->out_msg_buffer().at(0))};
  RoastPreSignature roast_pre_sig_2_3{dynamic_cast<RoastPreSignature&>(*m_states[0]->out_msg_buffer().at(1))};
  m_states[0]->ClearOutMessageBuffer();

  BOOST_TEST(roast_pre_sig_0_1.signers().at(0) == 0u);
  BOOST_TEST(roast_pre_sig_0_1.signers().at(1) == 1u);
  BOOST_TEST(roast_pre_sig_2_3.signers().at(0) == 2u);
  BOOST_TEST(roast_pre_sig_2_3.signers().at(1) == 3u);

  // Receive ROAST_PRE_SIGNATURE at R0 and R2
  // Assuming R1 and R3 do not reply with their own signature shares

  m_states[0]->ReceiveIncomingMessage(std::make_unique<RoastPreSignature>(roast_pre_sig_0_1));
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_PRE_SIGNATURE);

  m_states[2]->ReceiveIncomingMessage(std::make_unique<RoastPreSignature>(roast_pre_sig_2_3));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_PRE_SIGNATURE);

  // We will need a new session, with replica 0 and replica 2

  m_states[0]->Apply(*m_states[0]->active_actions().at(0));
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[0]->out_msg_buffer().at(0)->type() == MSG_TYPE::ROAST_SIGNATURE_SHARE);
  RoastSignatureShare roast_sig_share_0{dynamic_cast<RoastSignatureShare&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::ROAST_SIGNATURE_SHARE);
  RoastSignatureShare roast_sig_share_2{dynamic_cast<RoastSignatureShare&>(*m_states[2]->out_msg_buffer().at(0))};
  m_states[2]->ClearOutMessageBuffer();

  // Coordinator R0 receives signature share from R0
  m_states[0]->ReceiveIncomingMessage(std::make_unique<RoastSignatureShare>(roast_sig_share_0));
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_SIGNATURE_SHARE);
  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  // Coordinator R0 receives a signature share from R2
  m_states[0]->ReceiveIncomingMessage(std::make_unique<RoastSignatureShare>(roast_sig_share_2));
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_SIGNATURE_SHARE);
  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  // Coordinator R0 initiates a new session with R0 and R2
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[0]->out_msg_buffer().at(0)->type() == MSG_TYPE::ROAST_PRE_SIGNATURE);
  RoastPreSignature roast_pre_sig_0_2{dynamic_cast<RoastPreSignature&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();
  BOOST_TEST(roast_pre_sig_0_2.signers().at(0) == 0u);
  BOOST_TEST(roast_pre_sig_0_2.signers().at(1) == 2u);

  m_states[0]->ReceiveIncomingMessage(std::make_unique<RoastPreSignature>(roast_pre_sig_0_2));
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_PRE_SIGNATURE);

  m_states[2]->ReceiveIncomingMessage(std::make_unique<RoastPreSignature>(roast_pre_sig_0_2));
  BOOST_TEST(m_states[2]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[2]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_PRE_SIGNATURE);

  // Coordinator receives signature shares from R0 and R2
  m_states[0]->Apply(*m_states[0]->active_actions().at(0));
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[0]->out_msg_buffer().at(0)->type() == MSG_TYPE::ROAST_SIGNATURE_SHARE);
  RoastSignatureShare roast_sig_share_0_bis{dynamic_cast<RoastSignatureShare&>(*m_states[0]->out_msg_buffer().at(0))};
  m_states[0]->ClearOutMessageBuffer();

  m_states[2]->Apply(*m_states[2]->active_actions().at(0));
  BOOST_TEST(m_states[2]->out_msg_buffer().size() == 1u);
  BOOST_CHECK(m_states[2]->out_msg_buffer().at(0)->type() == MSG_TYPE::ROAST_SIGNATURE_SHARE);
  RoastSignatureShare roast_sig_share_2_bis{dynamic_cast<RoastSignatureShare&>(*m_states[2]->out_msg_buffer().at(0))};
  m_states[2]->ClearOutMessageBuffer();

  m_states[0]->ReceiveIncomingMessage(std::make_unique<RoastSignatureShare>(roast_sig_share_0_bis));
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_SIGNATURE_SHARE);
  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  m_states[0]->ReceiveIncomingMessage(std::make_unique<RoastSignatureShare>(roast_sig_share_2_bis));
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::ROAST_RECEIVE_SIGNATURE_SHARE);
  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  // R0 activates EXECUTE
  BOOST_TEST(m_states[0]->active_actions().size() == 1u);
  BOOST_TEST(m_states[0]->out_msg_buffer().size() == 0u);
  BOOST_CHECK(m_states[0]->active_actions().at(0)->type() == ACTION_TYPE::EXECUTE);
  m_states[0]->Apply(*m_states[0]->active_actions().at(0));

  // Other replica should not have active actions
  for (int rid=0; rid<4; rid++)
  {
    BOOST_TEST(m_states[rid]->active_actions().size() == 0u);
    BOOST_TEST(m_states[rid]->out_msg_buffer().size() == 0u);
  }

  // Finally propagate the block

  Block propagated_block{pre_prepare_0.seq_number(), pre_prepare_0.proposed_block().nTime, pre_prepare_0.proposed_block().GetHash().GetHex()};

  for (int rid=0; rid<4; rid++)
  {
    m_states[rid]->ReceiveIncomingMessage(std::make_unique<Block>(propagated_block));
    BOOST_TEST(m_states[rid]->active_actions().size() == 1u);
    BOOST_TEST(m_states[rid]->out_msg_buffer().size() == 0u);
    BOOST_CHECK(m_states[rid]->active_actions().at(0)->type() == ACTION_TYPE::RECEIVE_BLOCK);
    m_states[rid]->Apply(*m_states[rid]->active_actions().at(0));
    BOOST_TEST(m_states[rid]->active_actions().size() == 0u);
    BOOST_TEST(m_states[rid]->out_msg_buffer().size() == 0u);
  }

  // BOOST_FAIL("Test DID NOT FAIL. This is a failure placeholder used just to print all dynamic predicates");
}

BOOST_AUTO_TEST_SUITE_END()
