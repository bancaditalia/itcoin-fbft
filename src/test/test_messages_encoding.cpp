#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/test/unit_test.hpp>

#include "../pbft/messages/messages.h"

#include "fixtures.h"

using namespace std;
using namespace boost::unit_test;
using namespace itcoin::pbft::messages;

struct MessagesEncodingFixture: ReplicaStateFixture<> { MessagesEncodingFixture(): ReplicaStateFixture(itcoin::SIGNATURE_ALGO_TYPE::NAIVE,4,0,60) {} };

BOOST_AUTO_TEST_SUITE(test_messages_encoding, *enabled())

BOOST_FIXTURE_TEST_CASE(test_messages_encoding_00, MessagesEncodingFixture)
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::trace
  );

  //
  // Commit
  //
  {
  uint32_t sender_id = 3, v = 11, n = 17;
  string block_signature =
    m_wallets[sender_id]->GetBlockSignature(CBlock());
  Commit msg = Commit(sender_id, v, n, block_signature);

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::COMMIT);

  Commit typed_msg_built{ dynamic_cast<Commit&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.view() == v);
  BOOST_CHECK(typed_msg_built.seq_number() == n);
  BOOST_CHECK(typed_msg_built.block_signature() == msg.block_signature());
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }

  //
  // NewView
  //
  {
  uint32_t sender_id = 3, v = 11;

  uint32_t sender_id_vc = 2, v_vc = 11, hi = 17;
  std::string c = "This is the checkpoint digest";
  view_change_prepared_elem_t pi_elem = make_tuple(1, "req_digest", 10);
  view_change_prepared_t pi = {pi_elem};
  view_change_pre_prepared_elem_t q_elem = make_tuple(1, "req_digest", "block_hex", 10);
  view_change_pre_prepared_t qi = {q_elem};
  ViewChange vc_0 = ViewChange(sender_id_vc, v_vc, hi, c, pi, qi);

  uint32_t sender_id_ppp = 0, v_ppp = 11, n = 17;
  string req_digest{"abcdef"};
  CBlock block = m_blockchain->GenerateBlock(0);
  PrePrepare ppp_0 = PrePrepare(
    sender_id_ppp, v_ppp, n, req_digest, block
  );

  NewView msg = NewView(sender_id, v,
    vector<ViewChange>{vc_0},
    vector<PrePrepare>{ppp_0}
  );

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::NEW_VIEW);

  NewView typed_msg_built{ dynamic_cast<NewView&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.view() == v);
  BOOST_CHECK(typed_msg_built.view_changes() == msg.view_changes());
  BOOST_CHECK(typed_msg_built.pre_prepares() == msg.pre_prepares());
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }

  //
  // Prepare
  //
  {
  uint32_t sender_id = 3, v = 11, n = 17;
  string req_digest{"abcdef"};
  Prepare msg = Prepare(sender_id, v, n, req_digest);

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::PREPARE);

  Prepare typed_msg_built{ dynamic_cast<Prepare&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.view() == v);
  BOOST_CHECK(typed_msg_built.seq_number() == n);
  BOOST_CHECK(typed_msg_built.req_digest() == req_digest);
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }

  //
  // PrePrepare
  //
  {
  uint32_t sender_id = 3, v = 11, n = 17;
  string req_digest{"abcdef"};
  CBlock block = m_blockchain->GenerateBlock(666);
  PrePrepare msg = PrePrepare(sender_id, v, n, req_digest, block);

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::PRE_PREPARE);

  PrePrepare typed_msg_built{ dynamic_cast<PrePrepare&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.view() == v);
  BOOST_CHECK(typed_msg_built.seq_number() == n);
  BOOST_CHECK(typed_msg_built.req_digest() == req_digest);
  BOOST_CHECK(typed_msg_built.proposed_block_hex() == msg.proposed_block_hex());
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }

  //
  // ViewChange
  //
  {
  uint32_t sender_id = 3, v = 11, hi = 17;
  std::string c = "This is the checkpoint digest";
  view_change_prepared_elem_t pi_elem = make_tuple(1, "req_digest", 10);
  view_change_prepared_t pi = {pi_elem};
  view_change_pre_prepared_elem_t q_elem = make_tuple(1, "req_digest", "block_hex", 10);
  view_change_pre_prepared_t qi = {q_elem};

  ViewChange msg = ViewChange(sender_id, v, hi, c, pi, qi);

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::VIEW_CHANGE);

  ViewChange typed_msg_built{ dynamic_cast<ViewChange&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.view() == v);
  BOOST_CHECK(typed_msg_built.hi() == hi);
  BOOST_CHECK(typed_msg_built.c() == c);
  BOOST_CHECK(typed_msg_built.pi() == pi);
  BOOST_CHECK(typed_msg_built.qi() == qi);
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }

  //
  // RoastSignatureShare
  //
  {
  uint32_t sender_id = 3;
  std::string signature_share = "Sigshare";
  std::string next_presignature_share = "Presigshare";

  RoastSignatureShare msg = RoastSignatureShare(sender_id, signature_share, next_presignature_share);

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::ROAST_SIGNATURE_SHARE);

  RoastSignatureShare typed_msg_built{ dynamic_cast<RoastSignatureShare&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.sender_id() == sender_id);
  BOOST_CHECK(typed_msg_built.signature_share() == signature_share);
  BOOST_CHECK(typed_msg_built.next_pre_signature_share() == next_presignature_share);
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }

  //
  // RoastPreSignature
  //
  {
  uint32_t sender_id = 3;
  std::string pre_signature = "Sigshare";
  std::vector<uint32_t> signers = {0,1,2};

  RoastPreSignature msg = RoastPreSignature(sender_id, signers, pre_signature);

  m_wallets[sender_id]->AppendSignature(msg);
  string msg_as_bin = msg.ToBinBuffer();

  optional<unique_ptr<Message>> msg_built_opt = Message::BuildFromBinBuffer(msg_as_bin);
  BOOST_TEST(msg_built_opt.has_value());

  unique_ptr<Message>& msg_built = msg_built_opt.value();
  BOOST_CHECK(msg_built->type() == MSG_TYPE::ROAST_PRE_SIGNATURE);

  RoastPreSignature typed_msg_built{ dynamic_cast<RoastPreSignature&>(*msg_built) };
  BOOST_CHECK(typed_msg_built.sender_id() == sender_id);
  BOOST_CHECK(typed_msg_built.pre_signature() == pre_signature);
  BOOST_CHECK(typed_msg_built.signers() == signers);
  BOOST_CHECK(typed_msg_built.signature() == msg.signature());
  BOOST_CHECK(typed_msg_built.digest() == msg.digest());
  }
} // test_messages_encoding_prepare

BOOST_AUTO_TEST_SUITE_END()
