// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "utils.h"

#include <iomanip>
#include <limits>
#include <sstream>

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/detail/default_attribute_names.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <util/strencodings.h>

namespace itcoin {
namespace utils {

void configure_boost_logging()
{
  namespace bl = boost::log;

  bl::add_common_attributes();

  bl::core::get()->set_filter
  (
    bl::trivial::severity >= bl::trivial::debug
  );

  bl::add_console_log (
    std::clog,
    bl::keywords::format = (
     bl::expressions::stream
      << bl::expressions::attr<unsigned int>(bl::aux::default_attribute_names::line_id())
      << " ["   << bl::expressions::attr<boost::posix_time::ptime>(bl::aux::default_attribute_names::timestamp()) << "]"
      //<< " ["   << bl::expressions::attr<bl::aux::process::id>(bl::aux::default_attribute_names::process_id()) << ""
      //<< " "   << bl::expressions::attr<bl::aux::thread::id>(bl::aux::default_attribute_names::thread_id()) << "]"
      << " [" << bl::trivial::severity << "]"
      << " " << bl::expressions::smessage
    )
  );
}

/*
 * Slight modification from:
 *     https://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa/16125797#16125797
 */
std::string stringToHex(const std::string in)
{
  std::stringstream ss;

  ss << std::hex << std::setfill('0');
  for (size_t i = 0; in.length() > i; ++i) {
    ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(in[i]));
  }

  return ss.str();
} // stringToHex()

std::string hexToString(const std::string in) {
    std::string output;

    if ((in.length() % 2) != 0) {
        throw std::runtime_error("hexToString, input string is not valid length ...");
    }

    size_t cnt = in.length() / 2;

    for (size_t i = 0; cnt > i; ++i) {
        uint32_t s = 0;
        std::stringstream ss;
        ss << std::hex << in.substr(i * 2, 2);
        ss >> s;

        output.push_back(static_cast<unsigned char>(s));
    }

    return output;
} //hexToString

std::vector<uint8_t> string2byteVector(const std::string& s)
{
  return std::vector<uint8_t>{s.begin(), s.end()};
} // string2charVector()

std::string byteVector2string(const std::vector<uint8_t>& charVector)
{
  return std::string{charVector.begin(), charVector.end()};
} // charVector2string()

std::string join(const std::vector<std::string>& sequence, const std::string& separator)
{
  std::string result;

  for (size_t i = 0; i < sequence.size(); i++) {
    result += sequence[i] + ((i != sequence.size()-1) ? separator : "");
  }

  return result;
} // join()

unsigned int stoui(const std::string& str, std::size_t* pos, int base) {
  unsigned long result = std::stoul(str, pos, base);
  auto uiMax = std::numeric_limits<unsigned int>::max();

  if (result > uiMax) {
    throw std::out_of_range("stoui");
  }

  return (unsigned int) result;
} // stoui()

std::string checkHash(const std::string &hashStr) {
  // a hash string must be a hex string
  checkHex(hashStr);

  // a hash string in hexadecimal must have size 64 (256 bits)
  if (hashStr.size() != 64) {
    throw std::invalid_argument("hash string not valid");
  }

  return hashStr;
} // checkHash()

std::string checkHex(const std::string &hashStr) {
  bool allHexDigits = std::all_of(hashStr.begin(), hashStr.end(), [](char c) { return HexDigit(c) >= 0;});

  if (hashStr.size() > 0 && !allHexDigits){
    throw std::invalid_argument("hex string not valid");
  }

  return hashStr;
} // checkHex()

} // namespace utils
} // namespace itcoin
