// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "fixtures.h"

PrologTestFixture::PrologTestFixture()
{
}

PrologTestFixture::~PrologTestFixture()
{
  utf::test_case::id_t id = utf::framework::current_test_case().p_id;
  utf::test_results rez = utf::results_collector.results(id);
  if (!rez.passed())
  {
    BOOST_LOG_TRIVIAL(debug) << "Test did not pass, dumping all the dynamic facts that I know...";
    std::cout << std::endl << std::endl;
    PlCall("print_all_dynamics");
    std::cout << std::endl << std::endl;
  };
}
