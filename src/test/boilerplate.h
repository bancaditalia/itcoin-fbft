// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef TEST_BOILERPLATE_H
#define TEST_BOILERPLATE_H

#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace std {

/**
 * teach Boost.Test how to print std::optional<T>
 *
 * modified from: https://gist.github.com/mhamrah/79ed07a00209754a0ab1
 */
inline std::ostream& operator<<(std::ostream &ostr, nullopt_t const &nope)
{
  ostr << "std::nullopt";

  return ostr;
} // operator<< for nullopt_t

template <typename T>
inline std::ostream &operator<<(std::ostream &ostr, std::optional<T> const &maybeItem)
{
  if (maybeItem.has_value() == false) {
    ostr << std::nullopt;

    return ostr;
  }

  ostr << maybeItem.value();

  return ostr;
} // operator<< for std::optional<T>

/**
 * teach Boost.Test how to print std::vector<T>
 *
 * source: https://gist.github.com/mhamrah/79ed07a00209754a0ab1
 */
template <typename T>
inline std::ostream &operator<<(std::ostream &str, std::vector<T> const &items)
{
    str << '[';
    bool first = true;
    for (auto const& element : items) {
        str << (!first ? "," : "") << element;
        first = false;
    }
    return str << ']';
} // operator<< for std::vector<T>

/**
 * teach Boost.Test how to print std::unique_ptr<T>
 */
template <typename T>
inline std::ostream &operator<<(std::ostream &ostr, std::unique_ptr<T> const &uniquePtr)
{
  if (uniquePtr == nullptr) {
    ostr << "nullptr";

    return ostr;
  }

  ostr << "unique_ptr(" << uniquePtr.get() << ")";

  return ostr;
} // operator<< for std::unique_ptr<T>

} // namespace std

#endif // TEST_BOILERPLATE_H