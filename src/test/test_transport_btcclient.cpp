#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <boost/log/trivial.hpp>

// Bitcoin-core
#include <psbt.h>
#include <util/strencodings.h>

#include "fixtures.h"

namespace utf = boost::unit_test;
namespace bdata = boost::unit_test::data;

using std::tuple;
using std::vector;

const std::string helloWorld{"hello world"};

// how bitcoind 0 would sign helloWorld using the wallet address of client00
const std::string helloSignature{"IOzKScUZOETfXTwQFFNP+xMigAXMMTEZhLglGAe197ldMC15bnokhgMPR/l1QrCsJIgK0gCUW1NyemCy1ChHhfU="};

/*
* The string "bWFsZm9ybWVkIHNpZ25hdHVyZQ==" is the base64 encoding of
* "malformed signature". It can be verified with:
*
*     echo -n "malformed signature" | base64
*/
const std::string malformedSignature = "bWFsZm9ybWVkIHNpZ25hdHVyZQ==";

std::string buildEmptyPsbt() {
  // build an empty PSBT
  auto rawTx = CMutableTransaction();
  rawTx.nVersion = 0;
  rawTx.nLockTime = 0;
  PartiallySignedTransaction psbtx;
  psbtx.tx = rawTx;
  // Serialize the PSBT
  CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
  ssTx << psbtx;
  auto psbtBase64 = EncodeBase64(MakeUCharSpan(ssTx));

  return psbtBase64;
} // buildEmptyPsbt()

BOOST_AUTO_TEST_SUITE(test_transport_btcclient, *utf::enabled())

using namespace itcoin;

// Integration test with Bitcoind for the getblockchaininfo RPC method
BOOST_FIXTURE_TEST_CASE(test_getblockchaininfo, BtcClientFixture)
{
  auto response = bitcoind0.getblockchaininfo();

  // Test that the response is a Json::Value object
  BOOST_TEST(response.isObject());

  // Test that some important fields are set
  BOOST_TEST(response.isMember("bestblockhash"));
  BOOST_TEST(response.isMember("blocks"));
  BOOST_TEST(response.isMember("chain"));
  BOOST_TEST(response.isMember("difficulty"));
  BOOST_TEST(response.isMember("headers"));
}

// Integration test with Bitcoind for the getblocktemplate RPC method
BOOST_FIXTURE_TEST_CASE(test_getblocktemplate, BtcClientFixture)
{
  Json::Value root;
  Json::Value rules;

  rules.append("segwit");
  rules.append("signet");
  root["rules"] = rules;

  auto response = bitcoind0.getblocktemplate(root);

  // Test that the response is a Json::Value object
  BOOST_TEST(response.isObject());

  // Test that some important fields are set
  BOOST_TEST(response.isMember("previousblockhash"));
  BOOST_TEST(response.isMember("height"));
  BOOST_TEST(response.isMember("coinbasevalue"));
  BOOST_TEST(response.isMember("version"));
  BOOST_TEST(response.isMember("curtime"));
  BOOST_TEST(response.isMember("mintime"));
}

// Integration test with Bitcoind for the walletprocesspsbt RPC method
BOOST_FIXTURE_TEST_CASE(test_walletprocesspsbt, BtcClientFixture)
{
  auto psbtBase64 = buildEmptyPsbt();
  BOOST_LOG_TRIVIAL(info) << "Sending a walletprocesspsbt request with argument " << psbtBase64;
  // send the walletprocesspsbt request
  auto response = bitcoind0.walletprocesspsbt(psbtBase64);

  // check the response value is correct.
  BOOST_TEST(response.isObject());
  std::string newPsbt = response["psbt"].asString();
  bool is_complete = response["complete"].asBool();
  BOOST_LOG_TRIVIAL(info) << "Received a response: psbt=" << newPsbt << " complete=" << is_complete;
  BOOST_TEST(is_complete);
  BOOST_TEST(psbtBase64 == newPsbt);
}

// Integration test with Bitcoind for the combinepsbt RPC method
BOOST_FIXTURE_TEST_CASE(test_combinepsbt, BtcClientFixture)
{
  auto psbtBase64 = buildEmptyPsbt();
  Json::Value list;
  list.append(psbtBase64);
  list.append(psbtBase64);
  BOOST_LOG_TRIVIAL(info) << "Sending a combinepsbt request with arguments " << psbtBase64 << " and " << psbtBase64;
  std::string response = bitcoind0.combinepsbt(list);
  BOOST_TEST(psbtBase64 == response);
}

// Integration test with Bitcoind for the finalizepsbt RPC method
BOOST_DATA_TEST_CASE_F(
  BtcClientFixture,
  test_finalizepsbt,
  bdata::make({
    true, false
  }),
  extractHex
) {
  auto psbtBase64 = buildEmptyPsbt();
  Json::Value response;

  BOOST_LOG_TRIVIAL(info) << "Sending a finalizepsbt request with arguments psbt=" << psbtBase64 << " and extract=" << extractHex;
  response = bitcoind0.finalizepsbt(psbtBase64, extractHex);
  BOOST_TEST(response.isObject());
  BOOST_TEST(response.isMember("psbt") == (!extractHex));
  BOOST_TEST(response.isMember("complete"));
  BOOST_TEST(response.isMember("hex") == extractHex);
}

// Integration test with Bitcoind for the analyzepsbt RPC method
BOOST_FIXTURE_TEST_CASE(test_analyzepsbt, BtcClientFixture)
{
  auto psbtBase64 = buildEmptyPsbt();
  BOOST_LOG_TRIVIAL(info) << "Sending a finalizepsbt request with arguments psbt=" << psbtBase64;
  auto response = bitcoind0.analyzepsbt(psbtBase64);
  BOOST_LOG_TRIVIAL(info) << "Response: " << response.toStyledString();
  BOOST_TEST(response.isObject());

  // Test that some important fields are set
  // "inputs" is not set because PSBT was empty
  BOOST_TEST(!response.isMember("inputs"));
  BOOST_TEST(response.isMember("next"));
}

// Integration test with Bitcoind for the submitblock RPC method (failure)
BOOST_FIXTURE_TEST_CASE(test_submitblock_fails, BtcClientFixture)
{
  const int expectedCode = -32603;

  auto isInvalidBlockError = [](jsonrpc::JsonRpcException e) -> bool {
    if (e.GetCode() != expectedCode) {
      BOOST_LOG_TRIVIAL(error) << "Wrong code in exception: " + std::to_string(e.GetCode()) + " instead of " + std::to_string(expectedCode);
      return false;
    }

    if (e.GetMessage().find("Block decode failed") == std::string::npos) {
      BOOST_LOG_TRIVIAL(error) << "Wrong message in exception: " + e.GetMessage();
      return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "OK: received a 'Block decode failed' error";
    return true;
  }; // isInvalidBlockError()

  BOOST_LOG_TRIVIAL(info) << "Submitting an invalid block";
  BOOST_CHECK_EXCEPTION(bitcoind0.submitblock("INVALID BLOCK"), jsonrpc::JsonRpcException, isInvalidBlockError);
}

// Integration test with Bitcoind for the testblockvalidity RPC method
BOOST_FIXTURE_TEST_CASE(testblockvalidity, BtcClientFixture)
{
    const int expectedCode = -32603;

    auto isInvalidBlockError = [](const jsonrpc::JsonRpcException& e) -> bool {
        if (e.GetCode() != expectedCode) {
            BOOST_LOG_TRIVIAL(error) << "Wrong code in exception: " + std::to_string(e.GetCode()) + " instead of " + std::to_string(expectedCode);
            return false;
        }

        if (e.GetMessage().find("Block decode failed") == std::string::npos) {
            BOOST_LOG_TRIVIAL(error) << "Wrong message in exception: " + e.GetMessage();
            return false;
        }

        BOOST_LOG_TRIVIAL(debug) << "OK: received a 'Block decode failed' error";
        return true;
    }; // isInvalidBlockError()

    BOOST_LOG_TRIVIAL(info) << "Test validity of an invalid block";
    BOOST_CHECK_EXCEPTION(bitcoind0.testblockvalidity("INVALID BLOCK"), jsonrpc::JsonRpcException, isInvalidBlockError);
    BOOST_CHECK_EXCEPTION(bitcoind0.testblockvalidity("INVALID BLOCK", true), jsonrpc::JsonRpcException, isInvalidBlockError);
    BOOST_CHECK_EXCEPTION(bitcoind0.testblockvalidity("INVALID BLOCK", false), jsonrpc::JsonRpcException, isInvalidBlockError);
}

BOOST_AUTO_TEST_SUITE_END()
