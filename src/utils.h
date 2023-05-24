#ifndef PBFT_UTILS_H_
#define PBFT_UTILS_H_

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace itcoin {
namespace utils {

// Generic utils
void configure_boost_logging();

/**
 * Considers the input string as a binary buffer, and returns another string
 * with the binary hex representation of that buffer.
 *
 * EXAMPLE:
 *     stringToHex("hello") = "68656c6c6f"
 *     stringToHex(std::string("\x00\x01", 2)) == "0001"
 */
std::string stringToHex(const std::string in);
std::string hexToString(const std::string in);

/**
 * Computes the sha256sum of the concatenation of the two strings given as
 * parameters.
 *
 * The input strings are interpreted as binary buffers.
 * The result is returned as a 32 byte long binary buffer stored in a string
 * (with no encoding), like the inputs.
 *
 * In itcoin, this function is used to compute the message hash over which the
 * message signature is computed:
 * - the first string is going to be the proto3 serialized version of the
 *   payloadType of a SignedMessage
 * - the second string is the raw payload field (also a proto3 serialization) of
 *   a SignedMessage
 *
 * The code used for computing sha256sum is from
 * https://github.com/stbrumme/hash-library, with zlib license. Exact source
 * version:
 *     https://raw.githubusercontent.com/stbrumme/hash-library/f77b5646ca29bc4cfaa433996139d7d1c37cf650/sha256.h
 *     https://raw.githubusercontent.com/stbrumme/hash-library/f77b5646ca29bc4cfaa433996139d7d1c37cf650/sha256.cpp
 */
std::string concatenateAndComputeHash(std::string serializedType, std::string serializedMessage);

/**
 * Computes the sha256sum of data, calling concatenateAndComputeHash(data, "").
 */
std::string computeHash(std::string data);

/**
 * Overload that computes the sha256 hash of a binary buffer, returning the
 * value as an unencoded 32 bytes binary buffer.
 */
std::vector<uint8_t> computeHash(std::vector<uint8_t> inputBuffer);

/**
 * Converts a (binary) string to a byte vector.
 *
 * The string is treated like a binary buffer: if the string contains a null
 * character and then some bytes, those bytes are included in the returned
 * vector.
 *
 * This function is used to store BLOB data in SQLite via sqlite_modern_cpp.
 */
std::vector<uint8_t> string2byteVector(const std::string& s);

/**
 * Converts a char vector to a (binary) string.
 *
 * The string is treated like a binary buffer: if the vector contains a null
 * byte and then some other bytes, the null character and those bytes will be
 * included in the returned string.
 *
 * This function is used to retrieve BLOB data in SQLite via sqlite_modern_cpp.
 */
std::string byteVector2string(const std::vector<uint8_t>& byteVector);

/**
 * Converts a scoped enum to its underlying type (for example for printing its
 * value).
 *
 * C++23 will probably make this obsolete thanks to std::to_underlying().
 *
 * source: https://stackoverflow.com/questions/8357240/how-to-automatically-convert-strongly-typed-enum-into-int#33083231
 */
template <typename E>
constexpr auto enumToUnderlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
} // enumToUnderlying()

/**
 * Returns a string representation of value e of enum class type E.
 */
template <typename E>
constexpr auto enumValueToString(E e) noexcept
{
    return std::to_string(static_cast<std::underlying_type_t<E>>(e));
} // enumValueToString()

/**
 * Source:
 *
 * https://stackoverflow.com/questions/56184932/is-there-a-way-to-implement-pythons-join-in-c#56185530
 */
std::string join(const std::vector<std::string>& sequence, const std::string& separator);

unsigned int stoui(const std::string& str, std::size_t* pos = nullptr, int base = 10);

std::string checkHash(const std::string &hashStr);
std::string checkHex(const std::string &hexStr);


/**
 * source: https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c/56766138#56766138
 *
 * Returns a string_view of the type of the passed expression, computed at
 * compile time.
 *
 * Requires C++17.
 *
 * Given the origin of this module, do not use this function to control logic
 * steps: the values computed by it may vary depending on ISA, Operating
 * Systems, Compilers.
 *
 * Please limit the usage of this function to the generation of log messages.
 *
 * USAGE:
 *
 * int main() {
 *   int i = 0;
 *   const int ci = 0;
 *   std::cout << type_name<decltype(i)>()    << '\n'; // "int"
 *   std::cout << type_name<decltype((i))>()  << '\n'; // "int&"
 *   std::cout << type_name<decltype(ci)>()   << '\n'; // "const int"
 *   std::cout << type_name<decltype((ci))>() << '\n'; // "const int&"
 * }
 */
template <typename T>
constexpr auto type_name() {
  std::string_view name, prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto type_name() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto type_name() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl type_name<";
  suffix = ">(void)";
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
} // type_name()

} // namespace utils
} // namespace itcoin

#endif //PBFT_UTILS_H_
