// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

// Without custom main
// #define BOOST_TEST_MAIN

// With custom main
// https://www.boost.org/doc/libs/1_65_0/libs/test/doc/html/boost_test/adv_scenarios/single_header_customizations/entry_point.html
#define BOOST_TEST_MODULE custom_main
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API

#include <boost/test/included/unit_test.hpp>

#include <boost/log/trivial.hpp>

#include <util/translation.h>

#include <generated/prolog_fbft_engine/resource_db_mem.h>

#include <SWI-cpp.h>

#include "utils/utils.h"

using namespace boost::unit_test;

// This definition is required by itcoin-core
// NB: This line was placed just after the util/translation.h include, but it's now moved there
// The first program instruction should always follow all includes,
// otherwise cmake depend.make will not include the header files that follow the first program instruction.
const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

int BOOST_TEST_CALL_DECL main( int argc, char* argv[] )
{
  // Setup logger
  itcoin::utils::configure_boost_logging();

  // Print boost version
  BOOST_LOG_TRIVIAL(trace) << "Using Boost "
    << BOOST_VERSION / 100000     << "."  // major version
    << BOOST_VERSION / 100 % 1000 << "."  // minor version
    << BOOST_VERSION % 100;                // patch level

  // Init prolog engine
  PL_set_resource_db_mem(resource_db_mem_bin, resource_db_mem_bin_len);
  char *argv2[] = {(char*)"thisisnonsense", (char*)"-f", (char*)"none", (char*)"-F", (char*)"none", (char*)"-g", (char*)"true"};
  PlEngine engine(7, argv2);

  return ::boost::unit_test::unit_test_main( init_unit_test, argc, argv );
}
