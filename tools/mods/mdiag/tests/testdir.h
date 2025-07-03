/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008, 2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TESTDIR_H_
#define _TESTDIR_H_

#include "mdiag/utils/types.h"
#include "core/include/argread.h"
#include "mdiag/thread.h"

#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif

class Test;
class ArgDatabase;
struct TestListEntry;
class Thread;
class Trep;
class RC;

class TestDirectory {
 public:

  TestDirectory(ArgDatabase *args, Trep *trep);
  ~TestDirectory();

  Test *ParseTestLine(const char *line, ArgDatabase *args_so_far);

  void ListTests(int long_descriptions);

  // returns the number of tests set up
  int SetupAllTests(void);

  // runs all tests that have been correctly set up, returns 0 if no
  //   tests were run, 1 otherwise
  int RunAllTests(void);

  // cleans up all tests that have been correctly set up, returns 1 if
  //   such tests exist, 0 if there were none
  int CleanUpAllTests(bool quick_exit);

  // reports status of every test - returns 1 if everything passed, 0 otherwise
  RC ReportTestResults(void);

  // reports status of every test and resets the testdir.
  RC ReportResultsAndClean(void);

  // friendly request to terminate (SIGTERM)
  void TerminateAllTests(void);

  // describes expected params for all tests who match "name"
  static void DescribeTestParams(const char *name);

  // used by tests and resources to yield control to another thread
  // if set, 'min_delay_in_usec' guarantees at least that amount of time will
  //  pass before Yield() returns
  void Yield(unsigned min_delay_in_usec = 0);

  // used by tests and resources to block until all registered threads have blocked
  // when blocked, yielding to other threads skips over blocked threads
  void RegisterForBlock(Thread::BlockSyncPoint* syncPoint);
  void Block(Thread::BlockSyncPoint* syncPoint, Thread::BlockSyncPoint* nextSyncPoint);

  int GetLwrrentThread() { return lwrrent_thread;};
  static int Get_test_id_being_setup() { return test_id_being_setup; }
  static const char * Get_test_name_being_setup() { return test_name_being_setup; }

  int GetNumTests(void) { return num_tests; }

  bool IsSerialExelwtion(void) { return serial_exelwtion != 0; }

  struct Listener
  {
      virtual void activate_test() {}
      virtual void deactivate_test() {}
      virtual ~Listener() {}
  };

  void add_listener(Listener* l);
  void remove_listener(Listener* l);

 private:
  void SetupTest(int testIndex, int stage);
  void UpdateTestStatus(int i);
  Test *CreateTest(const char *test_name, unique_ptr<ArgDatabase>& test_args);

  RC ReinitRegSettingsBetweenTests();

  // static method for running C++ stuff via C++-oblivious scheduler
  //  argc contains test number, argv is the TestDirectory
  static int RunSingleTest(int argc, const char **argv);

  enum { MAX_RUNNING_TESTS = 1024 };
  enum TEST_STATE {
         TEST_STATE_INITIAL = 0,
         TEST_STATE_SET_UP  = 1,
         TEST_STATE_RUN     = 2,
         TEST_STATE_CLEANED = 3,
  };

  unsigned num_tests;
  unique_ptr<Test> tests[MAX_RUNNING_TESTS];
  TestListEntry *test_entries[MAX_RUNNING_TESTS];
  unique_ptr<ArgDatabase> test_args[MAX_RUNNING_TESTS];
  TEST_STATE test_state[MAX_RUNNING_TESTS];
  int lwrrent_thread;
  int serial_exelwtion;
  bool require_conlwrrent;
  bool stack_check;
  int yield_debug;
  unsigned stack_size;
  unsigned time_limit, end_flag;
  unsigned rtl_clk_count;
  unique_ptr<Thread> main_thread;
  Trep* m_Trep;
  bool enable_ecover;
  static int test_id_being_setup;
  static const char *test_name_being_setup;

  ArgReader m_Params;
};

TestListEntry * GetTestListHead();

#endif
