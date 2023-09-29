// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include <boost/test/unit_test.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/log/trivial.hpp>
#include <boost/test/data/test_case.hpp>

#include "../utils/utils.h"
#include "boilerplate.h"

using namespace boost::unit_test;

using std::string;
using std::tuple;
using std::vector;

namespace bdata = boost::unit_test::data;

enum class SOME_ENUM {
  ZERO = 0,
  ONE,
  TWO,
  LAST = 99,
};

vector<tuple<string, vector<uint8_t>>> STRING_2_BYTEVECTOR = {
  { "",                    {},             },
  { string("\x00", 1),     { 0x00 },       },
  { string("\x01", 1),     { 0x01 },       },
  { string("\x00\x01", 2), { 0x00, 0x01 }, },
  { string("\x01\x00", 2), { 0x01, 0x00 }, },
  { string("\xfe\xff", 2), { 0xfe, 0xff }, },
};

vector<tuple<string, string>> STRING_2_HASH = {
  { string(""),            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", },
  { string("A"),           "559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd", },
  { string("\x00", 1),     "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d", },
  { string("\x01", 1),     "4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a", },
  { string("\x00\x00", 2), "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7", },
  { string("hello"),       "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824", },
  { string("hello🥸"),     "38251e8670dfad8e88f3a63881e1a0c1e91170648bb26cf8300b8f5f17f79953", },
  { string("🥸hello"),     "c6ebe1d2ad358fd30e720170d95a7216cf54c5eaf141d6d3c9f9ee049d1aadf0", },
};

BOOST_AUTO_TEST_SUITE(test_utils, *enabled())

BOOST_DATA_TEST_CASE(
  test_stringToHex,
  bdata::make(vector<tuple<string, string>>{
    { "",                        "",                               },
    { "A",                       "41",                             },
    { "utf-8 đ熃🥸",             "7574662d3820c491e78683f09fa5b8", },
    { string("\x00", 1),         "00",                             },
    { string("\x01", 1),         "01",                             },
    { string("\x00\x01", 2),     "0001",                           },
    { string("\x01\x00", 2),     "0100",                           },
    { string("\x01\x00\x01", 3), "010001",                         },
    { string("\x00\x01\x00", 3), "000100",                         },
    { string("\x01\x02\x03", 3), "010203",                         },
  }),
  inputString,
  expectedHex
) {
  using itcoin::utils::stringToHex;

  BOOST_TEST(stringToHex(inputString) == expectedHex);
} // test_stringToHex

BOOST_DATA_TEST_CASE(
  test_string2byteVector,
  bdata::make(STRING_2_BYTEVECTOR),
  inputString,
  expectedByteVector
) {
  using itcoin::utils::string2byteVector;

  BOOST_TEST(string2byteVector(inputString) == expectedByteVector);
} // test_string2byteVector

BOOST_DATA_TEST_CASE(
  test_byteVector2string,
  bdata::make(STRING_2_BYTEVECTOR),
  expectedString,
  inputByteVector
) {
  using itcoin::utils::byteVector2string;

  BOOST_TEST(byteVector2string(inputByteVector) == expectedString);
} // test_byteVector2string

BOOST_DATA_TEST_CASE(
  test_string2byteVector_round_trip,
  bdata::make({
    // clang-format off
    //
    // clang-format <= 15 is not able to deal with this emoticon as-is.
    // Let's disable it.
    // This exception could be removed once we migrate to clang-format >= 16.
    string("🥸hello"),         // emoticon
    // clang-format on
    string("\x00\x00\x00", 3), // manyNulls
    string("\x00\xfe\xff", 3), // nonNullTerminated
  }),
  binBuffer
) {
  using itcoin::utils::byteVector2string;
  using itcoin::utils::string2byteVector;

  BOOST_TEST(byteVector2string(string2byteVector(binBuffer)) == binBuffer);
} // test_string2byteVector_round_trip

BOOST_AUTO_TEST_CASE(test_enumToUnderlying)
{
  using itcoin::utils::enumToUnderlying;

  BOOST_TEST(enumToUnderlying<SOME_ENUM>(SOME_ENUM::ZERO) == 0);
  BOOST_TEST(enumToUnderlying<SOME_ENUM>(SOME_ENUM::ONE) == 1);
  BOOST_TEST(enumToUnderlying<SOME_ENUM>(SOME_ENUM::TWO) == 2);
  BOOST_TEST(enumToUnderlying<SOME_ENUM>(SOME_ENUM::LAST) == 99);
} // test_enumToUnderlying

BOOST_AUTO_TEST_CASE(test_enumValueToString)
{
  using itcoin::utils::enumValueToString;

  BOOST_TEST(enumValueToString<SOME_ENUM>(SOME_ENUM::ZERO) == "0");
  BOOST_TEST(enumValueToString<SOME_ENUM>(SOME_ENUM::ONE) == "1");
  BOOST_TEST(enumValueToString<SOME_ENUM>(SOME_ENUM::TWO) == "2");
  BOOST_TEST(enumValueToString<SOME_ENUM>(SOME_ENUM::LAST) == "99");
} // test_enumValueToString

BOOST_DATA_TEST_CASE(
  test_join,
  bdata::make(vector<tuple<vector<string>, string, string>>{
    { { "",            }, "",      "",        },
    { { "",            }, ",",     "",        },
    { { "a",           }, ",",     "a",       },
    { { "",  "a",      }, ",",     ",a",      },
    { { "a", "b",      }, ",",     "a,b",     },
    { { "a", "",  "c", }, ",",     "a,,c",    },
    { { "a", "b", "c", }, ",",     "a,b,c",   },
    { { "a", "b",      }, "-SEP-", "a-SEP-b", },
    { { "a",           }, "",      "a",       },
    { { "a", "",       }, "",      "a",       },
    { { "a", "b",      }, "",      "ab",      },
  }),
  v,
  separator,
  joined
) {
  using itcoin::utils::join;

  BOOST_TEST(join(v, separator) == joined);
} // test_join

BOOST_DATA_TEST_CASE(
  test_stoui_positive_base16,
  // uIntAsString, base, expectedUInt
  bdata::make(vector<tuple<string, unsigned int, unsigned int>>{
    {"0x0",           16,   0, },
    {"0x01",          16,   1, },
    {"0x010",         16,   16, },
    {"0xffffffff",    16,   std::numeric_limits<unsigned int>::max(), },
    {"0x0ffffffff",   16,   std::numeric_limits<unsigned int>::max(), },
  }),
  uIntAsString,
  base,
  expectedUInt
) {
  using itcoin::utils::stoui;
  BOOST_TEST(stoui(uIntAsString, nullptr, base) == expectedUInt);
} // test_stoui_positive_base16

BOOST_DATA_TEST_CASE(
  test_stoui_negative_base16,
  bdata::make(vector<tuple<string, unsigned int>>{
    {"0x100000000", 16, },
    {"0xffffffff1", 16, },
  }),
  uIntAsString,
  base
) {
  using itcoin::utils::stoui;
  BOOST_CHECK_THROW(stoui(uIntAsString, nullptr, base), std::out_of_range);
} // test_stoui_negative_base16

BOOST_DATA_TEST_CASE(
  test_checkHex_positive,
  bdata::make({
    "0123456789abcdef",
    "0123456789ABCDEF",
    "FEDCDBA9876543210",
    "AbCdEf",
    "0",
    "00",
    "1",
    "01",
    "000000000000",
    "000000100000",
  }),
  hexStr
) {
  using namespace itcoin::utils;
  BOOST_TEST(checkHex(hexStr) == hexStr, "hex str not valid");
} // test_checkHex_positive

BOOST_DATA_TEST_CASE(
  test_checkHex_negative,
  bdata::make({
    "0x123456789abcdefg",
    "123456789abcdefg",
    "AbCdEfG",
    "0x",
    "0x 1",
    "0x00000000000z",
  }),
  hexStr
) {
  using namespace itcoin::utils;
  BOOST_CHECK_THROW(checkHex(hexStr), std::invalid_argument);
} // test_checkHex_negative

BOOST_DATA_TEST_CASE(
  test_checkHash_positive,
  bdata::make({
    "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6",
    "0000000000000000000000000000000000000000000000000000000000000000",
    "0000000000000000000000000000000000000000000000000000000000ABCDEF",
  }),
  hexStr
) {
  using namespace itcoin::utils;
  BOOST_TEST(checkHash(hexStr) == hexStr, "hex str not valid");
} // test_checkHash_positive

BOOST_DATA_TEST_CASE(
  test_checkHash_negative,
  bdata::make({
    "g0000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6",
    "00000000000000000000000000000000000000000000000000000000000000001",
    "000000000000000000000000000000000000000000000000000000000000000",
    "",
    "0"
  }),
  hexStr
) {
  using namespace itcoin::utils;
  BOOST_CHECK_THROW(checkHash(hexStr), std::invalid_argument);
} // test_checkHash_negative

BOOST_AUTO_TEST_SUITE_END()
