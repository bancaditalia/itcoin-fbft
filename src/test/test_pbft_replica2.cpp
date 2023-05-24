#include "fixtures.h"

#include <algorithm>
#include <cmath>

#include <boost/format.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "../pbft/Replica2.h"

using namespace std;

struct Replica2Fixture: ReplicaSetFixture { Replica2Fixture(): ReplicaSetFixture(4,0,60) {} };

BOOST_AUTO_TEST_SUITE(test_pbft_replica2, *utf::enabled())

BOOST_FIXTURE_TEST_CASE(test_pbft_replica2_00, Replica2Fixture)
{
try
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::debug
  );

  // ... At the beginning of the test, we kill a single, random replica each KILL_PERIOD_DELTA.
  // Say KILL_PERIOD_DELTA=3*TBT, and we want to kill 5 replica, i.e. MAX_KILLS=5.
  // We need 5 kill periods to kill 5 replica.
  // Then, we need 15*TBT time to kill a replica at most 5 times, i.e. KILL_WINDOW = 15*TBT.
  // ... Then we wait until the whole network recoveries.
  // We killed the primary at most MAX_KILLS times.
  // This means that the max view change timeout is 2^MAX_KILLS*(TBT/2) = 16*TBT
  // In order for the algorithm to converge, three replica should work without interruption for at least 16*TBT
  // The residual time should be bigger than the max timeout, we set RECOVERY_WINDOW = 16*TBT +1*TBT = 17*TBT
  // In total, we have (15 + 17)*TBT =  total test time.
  int KILL_PERIOD_DELTA = 3*TARGET_BLOCK_TIME;
  int MAX_KILLS = 5;

  //
  int KILL_WINDOW = MAX_KILLS*KILL_PERIOD_DELTA;
  int RECOVERY_WINDOW = pow(2, MAX_KILLS)*TARGET_BLOCK_TIME/2 + 1*TARGET_BLOCK_TIME;
  int MAX_SYNTHETIC_TIME = KILL_WINDOW+RECOVERY_WINDOW;
  int TARGET_HEIGHT = MAX_SYNTHETIC_TIME/TARGET_BLOCK_TIME -1;

  BOOST_LOG_TRIVIAL(debug) << str(
    boost::format("KILL_WINDOW = %1%, RECOVERY_WINDOW = %2%, MAX_SYNTHETIC_TIME = %3%, TARGET_HEIGHT = %4%.")
      % KILL_WINDOW
      % RECOVERY_WINDOW
      % MAX_SYNTHETIC_TIME
      % TARGET_HEIGHT
  );

  // Dead replica
  int dead_replica = -1;
  int dead_replica_time = 0;

  // Move time at the beginning of the block round
  int test_time = 0;
  set_synthetic_time(test_time);
  while(test_time<MAX_SYNTHETIC_TIME)
  {
    if (test_time<KILL_WINDOW)
    {
      if (test_time - dead_replica_time < KILL_PERIOD_DELTA)
      {
        // Do nothing
      }
      else
      {
        int new_dead_replica = (std::rand() % CLUSTER_SIZE);
        if (new_dead_replica != dead_replica)
        {
          if (dead_replica != -1) wake(dead_replica);
          kill(new_dead_replica);
          dead_replica = new_dead_replica;
          dead_replica_time = test_time;
        }
      }
    }
    else
    {
      // Eventually
      wake(dead_replica);
    }

    move_forward(10);
    test_time = m_replica[0]->current_time();
  }

  BOOST_TEST(m_blockchain->height() == TARGET_HEIGHT);
  // BOOST_FAIL("Test DID NOT FAIL. This is a failure placeholder used just to print all dynamic predicates");
}
catch(const std::exception& e)
{
  BOOST_LOG_TRIVIAL(error) << e.what();
  BOOST_CHECK_NO_THROW( throw e );
}
}

BOOST_FIXTURE_TEST_CASE(test_pbft_replica2_01, Replica2Fixture)
{
try
{
  boost::log::core::get()->set_filter (
    boost::log::trivial::severity >= boost::log::trivial::debug
  );

  m_replica[0]->CheckTimedActions();
  BOOST_TEST( m_replica[0]->active_actions().size() == 0u);
  BOOST_TEST( m_replica[0]->out_msg_buffer().size() == 0u);
  BOOST_TEST( m_replica[0]->latest_request_time() == 5*TARGET_BLOCK_TIME);

  set_synthetic_time(1);
  m_replica[0]->CheckTimedActions();
  BOOST_TEST( m_replica[0]->active_actions().size() == 0u);
  BOOST_TEST( m_replica[0]->out_msg_buffer().size() == 0u);
  BOOST_TEST( m_replica[0]->latest_request_time() == 5*TARGET_BLOCK_TIME);

  // Test that even if multiple requests are generated, pre_prepare are sent one by one;
  set_synthetic_time(TARGET_BLOCK_TIME+1);
  m_replica[0]->CheckTimedActions();
  m_replica[1]->CheckTimedActions();
  m_replica[2]->CheckTimedActions();
  m_replica[3]->CheckTimedActions();
  BOOST_TEST( m_replica[0]->latest_request_time() == 5*TARGET_BLOCK_TIME);

  for (int i=0; i<10; i++)
  {
    m_transports[0]->SimulateReceiveMessages();
    m_transports[1]->SimulateReceiveMessages();
    m_transports[2]->SimulateReceiveMessages();
    m_transports[3]->SimulateReceiveMessages();
  }

  BOOST_TEST(m_blockchain->height() == 1);

  // Mine other blocks
  int TARGET_HEIGHT = 9;
  int MAX_SYNTHETIC_TIME = (TARGET_HEIGHT+1)*TARGET_BLOCK_TIME;

  // Move time at the beginning of the block round
  int test_time = 2*TARGET_BLOCK_TIME+1;
  set_synthetic_time(test_time);
  while(test_time<MAX_SYNTHETIC_TIME)
  {
    if (test_time>=121 && test_time<181)
    {
      // Killing R1, a non primary
      kill(1);
    }
    else if (test_time>=181 && test_time<241)
    {
      // Killing the primary R0, triggers VC, new primary will be R1
      wake(1);
      kill(0);
    }
    else if (test_time>=241 && test_time<301)
    {
      // Killing the new primary R1, triggers VC, new primary will be R2
      wake(0);
      kill(1);
    }
    else if (test_time>=301)
    {
      // Killing R3 a non primary
      wake(1);
      kill(3);
    }

    move_forward(10);
    test_time = m_replica[0]->current_time();
  }

  BOOST_TEST(m_blockchain->height() == TARGET_HEIGHT);
  // BOOST_FAIL("Test DID NOT FAIL. This is a failure placeholder used just to print all dynamic predicates");
}
catch(const std::exception& e)
{
  BOOST_LOG_TRIVIAL(error) << e.what();
  BOOST_CHECK_NO_THROW( throw e );
}
}

BOOST_AUTO_TEST_SUITE_END() // test_pbft_replica2
