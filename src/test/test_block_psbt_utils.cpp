#include <boost/test/unit_test.hpp>

#include <boost/log/trivial.hpp>

#include "../block/generate.h"
#include "../block/psbt_utils.h"

#include "fixtures.h"

namespace utf = boost::unit_test;


BOOST_AUTO_TEST_SUITE(test_block_psbt_utils, *utf::enabled())

using namespace itcoin;

// Integration test with Bitcoind for the psbt creation and signing
BOOST_FIXTURE_TEST_CASE(test_block_psbt_utils_00, BitcoinInfraFixture)
{
  itcoin::PbftConfig& cfgNode0 = *m_configs.at(0);
  itcoin::transport::BtcClient& bitcoind0 = *m_bitcoinds.at(0);
  string address0 = address_at(0);

  const CBlock block = block::generateBlock(bitcoind0, address0, get_next_block_time());
  const std::string psbt = block::createPsbt(block, cfgNode0.getSignetChallenge());

  // analysis gives 4 remaining signers
  const auto analyzeResult = bitcoind0.analyzepsbt(psbt);
  BOOST_TEST(analyzeResult["inputs"][0]["missing"]["signatures"].size() == 4);

  const auto [ signedPsbt, isComplete ] = block::signPsbt(bitcoind0, psbt);

  BOOST_LOG_TRIVIAL(debug) << "PSBT partially-signed (base64): " << signedPsbt;

  // 'complete' must be false, since at least other two signers are needed
  BOOST_TEST(isComplete == false);
  // the new psbt is different from the original as it contains the signature
  BOOST_TEST(psbt != signedPsbt);
  BOOST_TEST(psbt.length() < signedPsbt.length());

  // analysis gives 3 remaining signers (instead of 4)
  const auto analyzeSignedResult = bitcoind0.analyzepsbt(signedPsbt);
  BOOST_TEST(analyzeSignedResult["inputs"][0]["missing"]["signatures"].size() == 3);
}

BOOST_AUTO_TEST_SUITE_END()
