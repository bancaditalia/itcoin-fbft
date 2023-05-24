// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include <boost/test/unit_test.hpp>
#include <boost/log/trivial.hpp>

#include <arith_uint256.h>
#include <consensus/merkle.h>

#include "../blockchain/generate.h"

#include "fixtures/fixtures.h"

namespace utf = boost::unit_test;


BOOST_AUTO_TEST_SUITE(test_blockchain_generate, *utf::enabled())

using namespace itcoin::blockchain;

bool isHashSmallerThanTarget(const CBlockHeader& header)
{
  uint32_t defaultNBits = 0x207fffff;
  arith_uint256 target;
  bool neg, over;

  target.SetCompact(defaultNBits, &neg, &over);

  auto headerHash = UintToArith256(header.GetHash());

  return (headerHash <= target);
} // isHashSmallerThanTarget()

// Integration test with Bitcoind for the block generation
BOOST_FIXTURE_TEST_CASE(test_block_generate_00, BitcoinInfraFixture)
{
  itcoin::transport::BtcClient& bitcoind0 = *m_bitcoinds.at(0);
  std::string address0 = address_at(0);

  // the generated block is a block with height = 1
  CBlock block = generateBlock(bitcoind0, address0, get_present_block_time());

  // test block
  {
    // check only coinbase tx (no tx in the pool)
    BOOST_TEST(block.vtx.size() == 1, "only coinbase tx expected (empty pool), got nb of transactions " << block.vtx.size());

    // check grinding worked
    BOOST_TEST(isHashSmallerThanTarget(CBlockHeader(block)), "block nonce is not valid");
  }

  // test coinbase transaction
  {
    CTransactionRef coinbaseTx = block.vtx[0];

    // test first tx is a coinbase tx, i.e.:
    BOOST_TEST(coinbaseTx->IsCoinBase(), "transaction is not a coinbase tx");

    // test first input scriptSig; see miner.cpp, CreateNewBlock function, around line 189
    unsigned int height = m_bitcoinds.at(0)->getblockchaininfo()["blocks"].asUInt() + 1;
    auto expectedScript = (CScript() << height);
    if(height<16)
    {
      expectedScript << OP_1;
    }
    auto& actualScript = coinbaseTx->vin[0].scriptSig;
    BOOST_TEST(expectedScript == actualScript, "actual script does not match with expected one");

    // test first input nSequence
    auto expectedNSequence = CTxIn::SEQUENCE_FINAL;
    auto& actualNSequence = coinbaseTx->vin[0].nSequence;
    BOOST_TEST(expectedNSequence == actualNSequence, "expected nSequence " << expectedNSequence << ", got " << actualNSequence);

    // test first input scriptWitness
    const uint256 witNonce = uint256(0);
    const uint256 witRoot = BlockWitnessMerkleRoot(block);
    CScript newOutScript = GetWitnessScript(witRoot, witNonce);
    CScriptWitness cbwit;
    cbwit.stack.resize(1);
    cbwit.stack[0] = std::vector<unsigned char>(witNonce.begin(), witNonce.end());
    BOOST_TEST((coinbaseTx->vin[0].scriptWitness.stack == cbwit.stack));

    // test nb outputs of the coinbase tx
    auto actualNbOutputs = coinbaseTx->vout.size();
    auto expectedNbOutputs = 2;
    BOOST_TEST(expectedNbOutputs == actualNbOutputs, "expected number of outputs is " << expectedNbOutputs << ", got " << actualNbOutputs);

    // test scriptPubKey in first output is set correctly
    const CScript expectedScriptPubKey = getScriptPubKey(bitcoind0, address0);
    BOOST_TEST(coinbaseTx->vout[0].scriptPubKey == expectedScriptPubKey, "actual scriptPubKey of first output does not match with expected one");

    // test coinbase value in first output is set correctly
    auto defaultCoinbaseValue = 10000000000;
    BOOST_TEST(coinbaseTx->vout[0].nValue == defaultCoinbaseValue, "expected value of first output is " << defaultCoinbaseValue << ", got " << coinbaseTx->vout[0].nValue);

    // test out script in second output is set correctly
    CScript newOutScriptWithSignetHeader = newOutScript << SIGNET_HEADER_VEC;
    BOOST_TEST((coinbaseTx->vout[1].scriptPubKey == newOutScriptWithSignetHeader), "actual scriptPubKey of second output does not match with expected one");

    // test value in second output is set correctly
    BOOST_TEST(coinbaseTx->vout[1].nValue == 0, "expected value of second output is 0, got " << coinbaseTx->vout[1].nValue);
  }
} // test_generate_block

BOOST_AUTO_TEST_SUITE_END()
