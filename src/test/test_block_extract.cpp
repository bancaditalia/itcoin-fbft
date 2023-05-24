#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <boost/log/trivial.hpp>
#include <util/strencodings.h>
#include <streams.h>
#include <version.h>

#include "../block/extract.h"
#include "../block/generate.h"
#include "../block/psbt_utils.h"

#include "fixtures.h"

namespace utf = boost::unit_test;


BOOST_AUTO_TEST_SUITE(test_block_extract, *utf::enabled())

using namespace itcoin;

/*
 * MUXATOR: integration test with Bitcoind for the block extraction.
 *
 * This is a complete example of block creation for itcoin-core (bypassing the
 * PBFT machinery):
 *
 * 1. bitcoind0 creates a block template;
 * 2. each of bitcoind{0,1,2,3} put its signature on the (unsigned) block
 *    template;
 *    At the end of this stage we have four partially signed block templates,
 *    each signed by a different itcoin-core daemon;
 * 3. bitcoind0 combines the 4 partially signed block templates invoking its
 *    combinepsbt() JSON-RPC method;
 * 4. bitcoind0 finalizes the block calling its finalizepsbt() JSON-RPC method
 *    and obtaining the definitive signed block;
 * 5. bitcoind0 adds the freshly mined block to its blockchain.
 *
 * Please note that in the complete (PBFT-distributed) version of the algorithm,
 * each itcoin-core should obtain the PSBTs from the other nodes, combine it
 * locally and add the resulting block to its local blockchain.
 */
BOOST_FIXTURE_TEST_CASE(test_block_extract_00, BitcoinInfraFixture)
{
  string address0 = address_at(0);
  string address1 = address_at(1);
  string address2 = address_at(2);
  string address3 = address_at(3);

  itcoin::transport::BtcClient& bitcoind0 = *m_bitcoinds.at(0);
  itcoin::transport::BtcClient& bitcoind1 = *m_bitcoinds.at(1);
  itcoin::transport::BtcClient& bitcoind2 = *m_bitcoinds.at(2);
  itcoin::transport::BtcClient& bitcoind3 = *m_bitcoinds.at(3);

  itcoin::PbftConfig& cfgNode0 = *m_configs.at(0);
  itcoin::PbftConfig& cfgNode1 = *m_configs.at(1);
  itcoin::PbftConfig& cfgNode2 = *m_configs.at(2);
  itcoin::PbftConfig& cfgNode3 = *m_configs.at(3);

  const CBlock block = block::generateBlock(bitcoind0, address0, get_next_block_time());
  const std::string psbt = block::createPsbt(block, cfgNode0.getSignetChallenge());

  const auto [ psbt0, isComplete0 ] = block::signPsbt(bitcoind0, psbt);
  const auto [ psbt1, isComplete1 ] = block::signPsbt(bitcoind1, psbt);
  const auto [ psbt2, isComplete2 ] = block::signPsbt(bitcoind2, psbt);
  const auto [ psbt3, isComplete3 ] = block::signPsbt(bitcoind3, psbt);

  Json::Value list;
  list.append(psbt0);
  list.append(psbt1);
  list.append(psbt2);
  list.append(psbt3);

  const std::string combinedPsbt = bitcoind0.combinepsbt(list);
  BOOST_LOG_TRIVIAL(debug) << "combined PSBT: " << combinedPsbt;

  const Json::Value finalizeResponse = bitcoind0.finalizepsbt(combinedPsbt, false);
  const std::string finalizedPsbt = finalizeResponse["psbt"].asString();
  bool isComplete = finalizeResponse["complete"].asBool();

  BOOST_TEST(isComplete);
  BOOST_LOG_TRIVIAL(debug) << "finalized PSBT: " << finalizedPsbt;

  const CBlock finalBlock = block::extractBlock(bitcoind0, finalizedPsbt);

  /*
   * in order to call submitblock, we have to serialize the block to network
   * format and encode it as an hex string.
   */
  CDataStream ssBlock = CDataStream(SER_NETWORK, PROTOCOL_VERSION);
  ssBlock << finalBlock;

  const std::string blockHex = HexStr(ssBlock);
  BOOST_LOG_TRIVIAL(debug) << "block hex: " << blockHex;

  // submit the block to itcoin-core
  const Json::Value response = bitcoind0.submitblock(blockHex);

  // When submitblock is successful, bitcoind JSON-RPC API returns null
  BOOST_TEST_INFO("response to 'submitblock': " << response.toStyledString());
  BOOST_TEST(response.isNull());
}

BOOST_AUTO_TEST_SUITE_END()
