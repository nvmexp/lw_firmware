/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017, 2019, 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "stdtest.h"

#include "testdir.h"
#include "test_state_report.h"

#include "core/utility/trep.h"

#include <sstream>
#include "mdiag/sysspec.h"

#include "mdiag/utils/mdiag_xml.h"
#include "core/include/simclk.h"

// ---------------------------------------------------------------------------
// Programmer's Note: MAKE_TEST_LIST / TEST_LIST_HEAD
// In addition to 'standard' kind of #include work, the includes below
// construct a linked list of TestListEntry objects which will
// be pointed to by TEST_LIST_HEAD.
// The process of building the list is a little unusual. Each object is
// added to the front of the list as the object's definition is encountered in
// the .h files. (See file test_list.h).  The list is effectively built by the
// link / loader process and 'exists' when the main C program gets control.
// So, adding a new test requires creating a .h file containing declares
// needed to add the object to the linked list and #including the .h file in
// test_list.h
// --------------------------------------------------------------------------
#define MAKE_TEST_LIST
#include "test.h"

#include "mdiag/tests/gpu/testlist.h"

#undef MAKE_TEST_LIST

#include "core/include/platform.h"

#include "mdiag/utl/utl.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"

#include <map>
#include <list>
#include <algorithm>
#include <functional>

static map< int, list<TestDirectory::Listener*> > listeners;

/* static */ int TestDirectory::test_id_being_setup = 0;
/* static */ const char * TestDirectory::test_name_being_setup = "";

void TestDirectory::add_listener(TestDirectory::Listener* l)
{
    listeners[test_id_being_setup].push_back(l);
}

void TestDirectory::remove_listener(TestDirectory::Listener* l)
{
    listeners[test_id_being_setup].remove(l);
}

static void activate_test_event()
{
    for (list<TestDirectory::Listener*>::const_iterator
            iter = listeners[TestDirectory::Get_test_id_being_setup()].begin();
            iter != listeners[TestDirectory::Get_test_id_being_setup()].end();
            ++iter)
    {
        (*iter)->activate_test();
    }
}

static void deactivate_test_event()
{
    for (list<TestDirectory::Listener*>::const_iterator
            iter = listeners[TestDirectory::Get_test_id_being_setup()].begin();
            iter != listeners[TestDirectory::Get_test_id_being_setup()].end();
            ++iter)
    {
        (*iter)->deactivate_test();
    }
}

// ---------------------------------------------------------------------------
// Tests in mdiag/tests/gpu/misc are built in another static library called
// libmisctests.a whose linked list header is misctest_list_head
// ---------------------------------------------------------------------------
extern TestListEntryPtr misctest_list_head;

// ---------------------------------------------------------------------------
// Connect two test lists and return the list head.
// Two lists: 1. TEST_LIST_HEAD in testlist.h
//            2. misctest_list_heads in misctestlist.h
// Return connected test lists s_TestListHead
// ---------------------------------------------------------------------------
TestListEntryPtr GetTestListHead()
{
    static bool s_Connected = false;
    static TestListEntryPtr s_TestListHead = TEST_LIST_HEAD;

    // connect two test lists
    if (!s_Connected)
    {
        TestListEntryPtr pos;
        pos = s_TestListHead;
        if (pos)
        {
            while (pos->next) pos = pos->next;
            pos->next = misctest_list_head;
        }
        else
        {
            s_TestListHead = misctest_list_head;
        }
        s_Connected = true;
    }

    return s_TestListHead;
}

//
// The only time we really need a deep stack is when we are running
// on the cmodel.  Since the cmodel runs on our stack, it could
// need a deep stack.  But when we are running cmodel, this is just
// a big block of virtual memory.  So it shouldn't hurt to make it
// really big.
//
// Actually, some tests are getting pretty hefty (like HIC), and they
// need a larger stack.  And the only place where we don't have VM is
// on DOS.  So we will only default to a small stack size on DOS.
//
enum {
    DEFAULT_STACK_SIZE = 2*KILOBYTE,
    DOS_STACK_SIZE = 64,
};

static const ParamDecl testdir_params[] = {
  { "-test_mode_flag","u", (ParamDecl::PARAM_ENFORCE_RANGE |
                            ParamDecl::GROUP_START |
                            ParamDecl::GROUP_OVERRIDE_OK),   0,  1,  "0=parallel, 1=serial" },
  { "-parallel",   "0", ParamDecl::GROUP_MEMBER,   0,  0,  "forces all tests to run in parallel" },
  { "-serial",     "1", ParamDecl::GROUP_MEMBER,   0,  0,  "forces all tests to run serially" },
  SIMPLE_PARAM("-force_conlwrrent", "Reports an error if some tests don't run conlwrrently"),
  SIMPLE_PARAM("-yield_debug", "print debugging info around Yield() calls"),
  UNSIGNED_PARAM("-stack_size", "stack size (in KB) for each thread [default=64 (2048 on cmodel)]"),
  SIMPLE_PARAM("-stack_check", "enable stack usage checking"),
  UNSIGNED_PARAM("-rtl_clk_count", "number of clocks to let RTL tick between Yield's [default=25]"),

  FOREVER_PARAM,
  UNSIGNED_PARAM("-timelimit", "stop -forever tests after this many micron seconds (us)"),
  UNSIGNED_PARAM("-timelimit_sec", "stop -forever tests after this many seconds (s)"),
  SIMPLE_PARAM("-enable_ecov_checking", "Enable ECover checking"),
  SIMPLE_PARAM("-enable_ecov_checking_only", "Enable ECover checking only if a valid ecov data file exists"),
  SIMPLE_PARAM("-nullEcover", "indicates meaningless ecoverage checking -- this happens sometimes when running test with certain priv registers"),
  LAST_PARAM
};

TestDirectory::TestDirectory
(
    ArgDatabase *args,
    Trep *trep
)
    : m_Trep(trep),
      m_Params(testdir_params)
{
    LWGpuResource::SetTestDirectory(this);

    num_tests = 0;
    lwrrent_thread = -1;

    if (m_Params.ParseArgs(args, "testdir")) 
    {
        serial_exelwtion = m_Params.ParamUnsigned("-test_mode_flag");

        Sys::SetThreadNamePrintEnable(true);

        require_conlwrrent = (m_Params.ParamPresent("-force_conlwrrent") > 0);
        yield_debug = m_Params.ParamPresent("-yield_debug");
        stack_size = m_Params.ParamUnsigned("-stack_size", DEFAULT_STACK_SIZE);

        stack_check = m_Params.ParamPresent("-stack_check") > 0;

        enable_ecover = m_Params.ParamPresent("-enable_ecov_checking") > 0 &&
            m_Params.ParamPresent("-nullEcover") == 0;

        time_limit = m_Params.ParamUnsigned("-timelimit_sec");
        if (time_limit) 
        {
            time_limit *= 1000000; // colwert to micron second
        }
        else 
        { 
            // this is mostly used for rtl simulation:
            // default time limit depends on whether -forever was specified
            //  if it is, 60 micron seconds seems reasonable
            //  if it isn't, the default is 0 - no time limit
            time_limit = m_Params.ParamUnsigned("-timelimit",
                (m_Params.ParamPresent("-forever") ? 60 : 0));
        }
        rtl_clk_count = m_Params.ParamUnsigned("-rtl_clk_count",
            Platform::GetSimulationMode() == Platform::Fmodel ? 1 : 25);
    } 
    else 
    {
        serial_exelwtion = 0;
        yield_debug = 0;
        stack_size = 32;
        time_limit = 0;
    }
}

TestDirectory::~TestDirectory()
{
    for ( unsigned i=0; i < num_tests; i++ )
    {
        tests[i].reset(0);
    }
    LWGpuResource::SetTestDirectory(0);
}

Test *TestDirectory::ParseTestLine(const char *line, ArgDatabase *args_so_far)
{
    char test_name[KILOBYTE], *pos;
    Test *new_test;
    unique_ptr<ArgDatabase> test_args;

    // skip over leading whitespace, if any
    while (*line && isspace(*line))
        line++;

    pos = test_name;
    while(*line && !isspace(*line)) *pos++ = *line++;
    *pos = 0;

    if (args_so_far)
    {
        test_args.reset(new ArgDatabase(args_so_far));
    }
    else
    {
        test_args.reset(new ArgDatabase);
    }

    if (OK != test_args->AddArgLine(line, ArgDatabase::ARG_MUST_USE))
    {
        ErrPrintf("Unable to add arguments for test '%s'.\n", test_name);
        return NULL;
    }

    new_test = CreateTest(test_name, test_args);
    if (new_test)
    {
        InfoPrintf("Test '%s' created.\n", test_name);

        new_test->getStateReport()->init(test_name);

        if (gXML != NULL)
        {
            XD->XMLStartStdInline("<MDiagTest");
            XD->outputAttribute("SeqId", new_test->GetSeqId());
            XD->outputAttribute("TestName", test_name);
            XD->outputAttribute("TestArgs", line);
            XD->XMLEndLwrrent();
        }

        return new_test;
    }
    else
    {
        ErrPrintf("Unrecognized test name '%s'.\n", test_name);
        return NULL;
    }
}

Test *TestDirectory::CreateTest(const char *newtest_name, unique_ptr<ArgDatabase>& newtest_args)
{
  Test *new_test;
  TestListEntryPtr pos;

  // if we've got too many tests running, don't create a new one
  if(num_tests >= MAX_RUNNING_TESTS) return(0);

  pos = GetTestListHead();
  while(pos) {
    if(!strcmp(newtest_name, pos->name)) {
      new_test = (pos->factory_fn)(newtest_args.get());

      if ( new_test == NULL ) {
          return NULL;
      }

      tests[num_tests].reset(new_test);
      test_entries[num_tests] = pos;
      test_args[num_tests] = std::move(newtest_args);
      test_state[num_tests] = TEST_STATE_INITIAL;

      num_tests++;

      if (new_test->IsEcoverSupported())
      {
          Test::IncreaseNumEcoverSupportedTests();
      }

      if (Utl::HasInstance())
      {
          Utl::Instance()->AddTest(new_test);
      }

      return(new_test);
    }

    pos = pos->next;
  }

  return(0);
}

void TestDirectory::ListTests(int long_descriptions)
{
  TestListEntryPtr pos;

  pos = GetTestListHead();
  while(pos) {
    InfoPrintf(" %-20s %c : %s\n",
               pos->name, (ArgReader::ParamDefined(pos->params, "-forever") ? '*' : ' '),
               pos->desc);

    pos = pos->next;
  }
}

static bool first_setup_pass = true;

// returns number of tests successfully set up (zero if no tests)
int TestDirectory::SetupAllTests(void)
{
    unsigned i;
    int count;

    InfoPrintf("TestDirectory::SetupAllTests: num_tests=%d\n", num_tests);
    SimClk::EventWrapper event(SimClk::Event::TEST_SETUP);
    count = 0;
    int  stage;
    bool bLastStage = false;

    // For serial exelwtion, loop through the tests setting up all stages of
    // the test until a test successfully passes all stages of its setup
    if ( serial_exelwtion )
    {
        for (i = 0; i < num_tests; i++)
        {
            stage = 0;
            if (test_state[i] != TEST_STATE_INITIAL)
            {
                InfoPrintf("TestDirectory::SetupAllTests: skip setup of test=%d\n", i);
                continue;
            }

            while (test_state[i] == TEST_STATE_INITIAL)
            {
                SetupTest(i, stage++);
            }

            if (test_state[i] == TEST_STATE_SET_UP)
            {
                count ++;
                break;
            }
        }
    }
    else
    {
        // For non-serial exelwtion, perform a stage-by-stage setup of all
        // tests, setting up stage 0 for all tests, then stage 1 for all tests
        // (that require more than one stage), etc.  Count and return the
        // total number of tests that pass all stages of their setup
        bLastStage = false;
        for (stage = 0; !bLastStage; stage++)
        {
            bLastStage = true;
            for (i = 0; i < num_tests; i++)
            {
                if (test_state[i] == TEST_STATE_INITIAL)
                {
                    SetupTest(i, stage);

                    if (test_state[i] == TEST_STATE_INITIAL)
                    {
                        bLastStage = false;
                    }
                    else if (test_state[i] == TEST_STATE_SET_UP)
                    {
                        count++;
                    }
                }
                else
                {
                    InfoPrintf("TestDirectory::SetupAllTests: skip setup of test=%d\n", i);
                }
            }
        }
        first_setup_pass = false;
    }

    VmiopMdiagElwManager* pMdiagElwMgr = VmiopMdiagElwManager::Instance();
    if (count < 1 && Platform::IsPhysFunMode())
    {
        InfoPrintf(SRIOVTestSyncTag "TestDirectory::SetupAllTests: no tests will run, disable test sync-up.\n");
        // Maybe setup errors, need disable test sync-up.
        pMdiagElwMgr->StopTestSync();
    }
    // Update num tests.
    pMdiagElwMgr->SetNumT3dTests(count);

    return(count);
}

// used by resources to yield control to another thread
void TestDirectory::Yield(unsigned min_delay_in_usec /*= 0*/)
{
    Thread::Yield();
}

// used by tests and resources to block until all registered threads have blocked
// when blocked, yielding to other threads skips over blocked threads
void TestDirectory::RegisterForBlock(Thread::BlockSyncPoint* syncPoint)
{
    if (serial_exelwtion) {
        // makes no sense to block
        // this is probably an error
        assert(!"cannot block during serialized exelwtion\n");
    }

    // Thread::RegisterForBlock calls Thread::Yield, so we have to do like in
    // TestDirectory::Yield and restore the 'lwrrent_thread' member
    // variable to keep things consistent.
    volatile int my_thread = lwrrent_thread;

    if(yield_debug) InfoPrintf("REGISTER_FOR_BLOCK: %3d ->", my_thread);
    Thread::RegisterForBlock(syncPoint);
    if(yield_debug) InfoPrintf("REGISTER_FOR_BLOCK: %3d ->", my_thread);

    lwrrent_thread = my_thread;
}

void TestDirectory::Block(Thread::BlockSyncPoint* syncPoint, Thread::BlockSyncPoint* nextSyncPoint)
{
    if (serial_exelwtion) {
        // makes no sense to block
        // this is probably an error
        assert(!"cannot block during serialized exelwtion\n");
    }

    // Thread::Block calls Thread::Yield, so we have to do like in
    // TestDirectory::Yield and restore the 'lwrrent_thread' member
    // variable to keep things consistent.
    volatile int my_thread = lwrrent_thread;

    if(yield_debug) InfoPrintf("BLOCK: %3d ->", my_thread);
    Thread::Block(syncPoint, nextSyncPoint);
    if(yield_debug) InfoPrintf("BLOCK: %3d ->", my_thread);

    lwrrent_thread = my_thread;
}

// static method for running C++ stuff via C++-oblivious scheduler
//  argc contains test number, argv is the TestDirectory
int TestDirectory::RunSingleTest(int argc, const char **argv)
{
    TestDirectory *testdir;
    int test_num;

    testdir = (TestDirectory *) argv;
    test_num = argc;

    if (Tee::WillPrint(Tee::PriNormal))
    {
        InfoPrintf("Parsing %-20s\n", testdir->test_entries[test_num]->name);
        ArgReader test_param(testdir->test_entries[test_num]->params);
        test_param.ParseArgs(testdir->test_args[test_num].get());
        InfoPrintf("Running %-20s (", testdir->test_entries[test_num]->name);
        test_param.PrintParsedArgs();
        InfoPrintf("):\n");
    }

    InfoPrintf("TestDirectory::RunOneTest: test_num=%d\n", test_num);

    testdir->tests[test_num]->getStateReport()->beforeRun();

    testdir->lwrrent_thread = test_num;
    if(testdir->yield_debug) InfoPrintf(" %3d\n", test_num);
    if(testdir->require_conlwrrent) testdir->tests[test_num]->StartTimer();

    //Putting timestamp instrumentation to time run of the test on silicon

    UINT64 SAT_Start = 0;
    if (Platform::GetSimulationMode() == Platform::Hardware)
        SAT_Start = Platform::GetTimeUS();

    Test *test = testdir->tests[test_num].get();
    test->SetThreadId2TestMap(Tasker::LwrrentThread());
    test_id_being_setup = test->GetSeqId();
    test_name_being_setup = test->GetTestName();

    // Setup Ecover for non-trace3d/v2d tests.
    test->EnableEcoverDumpInSim(testdir->enable_ecover);

    activate_test_event();
    test->PreRun();
    test->Run();
    test->PostRun();
    deactivate_test_event();
    test_id_being_setup = 0;
    test_name_being_setup = "";

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        UINT64 SAT_End = Platform::GetTimeUS();
        InfoPrintf("TIMESTAMP: Test %s = %g usec\n", testdir->test_entries[test_num]->name, double(SAT_End-SAT_Start));
    }
    else
    {
        InfoPrintf("TEST DONE: Test %s has completed\n", testdir->test_entries[test_num]->name );
    }

    if(testdir->require_conlwrrent) testdir->tests[test_num]->EndTimer();
    if(testdir->yield_debug) InfoPrintf("YIELD* %3d ->", test_num);
    testdir->lwrrent_thread = -1;

    testdir->tests[test_num]->getStateReport()->afterRun();

    return(1);
}

static int ForeverTimeout(int argc, const char **argv)
{
    UINT64 elapsed, delay_amount;
    volatile int *end_flag_ptr;
    UINT64 then_usec = 0, now_usec, last_usec = 0;

    delay_amount = argc;
    end_flag_ptr = (int *)argv;

    last_usec = then_usec = Platform::GetTimeUS();
    while (!*end_flag_ptr)
    {
        Thread::Yield();

        now_usec = Platform::GetTimeUS();
        elapsed = now_usec - then_usec;

        if (elapsed >= delay_amount)
        {
            InfoPrintf("Time limit reached.\n");
            return(1);
        }

        if ( now_usec == last_usec ) 
        {
            Platform::Delay(1);  // We've got to let the simulator run...
        }

        last_usec = now_usec;
    }
    return(0);
}

void TestDirectory::TerminateAllTests()
{
    end_flag = 1;
    for(unsigned i = 0; i < num_tests; i++)
    {
      if(test_state[i] == TEST_STATE_RUN)
      {
        test_id_being_setup = tests[i]->GetSeqId();
        test_name_being_setup = tests[i]->GetTestName();
        activate_test_event();
        tests[i]->EndTest();
        deactivate_test_event();
        test_id_being_setup = 0;
        test_name_being_setup = "";
      }
    }
}

// runs all tests that have been correctly set up, returns 0 if no
//   tests were run, 1 otherwise
int TestDirectory::RunAllTests(void)
{
    unsigned i, count;
    SimClk::EventWrapper event(SimClk::Event::TEST_RUN);
    count = 0;
    if(!serial_exelwtion) {
        InfoPrintf("TestDirectory::RunAllTests: conlwrrent exelwtion of all tests: num_tests=%d\n", num_tests);

        for(i = 0; i < num_tests; i++) {
            if(test_state[i] == TEST_STATE_SET_UP) {
                test_state[i] = TEST_STATE_RUN;

                Thread::Fork(TestDirectory::RunSingleTest, i, (const char **) this,
                    stack_size * KILOBYTE, stack_check, tests[i]->GetTestName());

                count++;
            }
        }

        // to support tests that run forever, we wait until the first
        //  test finishes, and then tell all the rest to stop
        // if a time limit has been set, spawn one thread that just waits that
        //  amount of time (or until end_flag is set) and then returns
        end_flag = 0;
        if(time_limit > 0)
            Thread::Fork(ForeverTimeout, time_limit, (const char **)&end_flag,
                stack_size * KILOBYTE, stack_check);

        Thread::JoinAny();

        InfoPrintf("Testdir: first test finished - waiting for other tests to finish\n");
        TerminateAllTests();

        Thread::JoinAll();
    } else {

        InfoPrintf("TestDirectory::RunAllTests: serial exelwtion of all tests: num_tests=%d\n", num_tests);

        for(i = 0; i < num_tests; i++) {
            if(test_state[i] == TEST_STATE_SET_UP) {
                InfoPrintf("TestDirectory::RunAllTests: serial exelwtion of test %d %s\n", i, tests[i]->GetTestName());

                test_state[i] = TEST_STATE_RUN;

                test_id_being_setup = tests[i]->GetSeqId();
                test_name_being_setup = tests[i]->GetTestName();
                RunSingleTest(i, (const char **) this);
                test_name_being_setup = "";
                test_id_being_setup = 0;

                count++;
            }
            else
            {
                InfoPrintf("TestDirectory::RunAllTests: skip test %d (not set up\n", i);
            }
        }
    }

    return((count > 0) ? 1 : 0);
}

// extern int MemGetTotal();

// cleans up all tests that have been correctly set up, returns 1 if
//   such tests exist, 0 if there were none
//   quick_exit indicates whether memory managed by RM should be freed at cleanup
int TestDirectory::CleanUpAllTests(bool quick_exit)
{
    vector<UINT32> testsToCleanUp;
    vector<UINT32>::const_iterator pTestNum;
    SimClk::EventWrapper event(SimClk::Event::TEST_CLEAN);
    // Get list of tests to clean up.  Clean up tests that have only
    // just set up in addition to tests that have been run.
    //
    for (UINT32 testNum = 0; testNum < num_tests; ++testNum)
    {
        if ((test_state[testNum] == TEST_STATE_SET_UP) ||
            (test_state[testNum] == TEST_STATE_RUN))
        {
            testsToCleanUp.push_back(testNum);
        }
    }

    // Clean up the tests
    //
    if (serial_exelwtion)
    {
        for (pTestNum = testsToCleanUp.begin();
             pTestNum != testsToCleanUp.end(); ++pTestNum)
        {
            if (Utl::HasInstance())
            {
                Utl::Instance()->TriggerTestCleanupEvent(tests[*pTestNum].get());
            }

            tests[*pTestNum]->getStateReport()->beforeCleanup();
            tests[*pTestNum]->BeforeCleanup();
            lwrrent_thread = *pTestNum;
            // Do not clean up resources on quick_exit.
            if (!quick_exit ||
                (*pTestNum < num_tests - 1))
            {
                for (UINT32 stage = 0; stage < tests[*pTestNum]->NumCleanUpStages(); ++stage)
                {
                    tests[*pTestNum]->CleanUp(stage);
                }
            }

            UpdateTestStatus(*pTestNum);
            lwrrent_thread = -1;
            tests[*pTestNum]->getStateReport()->afterCleanup();
            test_state[*pTestNum] = TEST_STATE_CLEANED;

            if (*pTestNum != num_tests - 1 && // not the last test
                OK != ReinitRegSettingsBetweenTests())
            {
                ErrPrintf("Error: failed to reinitialize between each test.");
                break;
            }
        }
    }
    else
    {
        for (pTestNum = testsToCleanUp.begin();
             pTestNum != testsToCleanUp.end(); ++pTestNum)
        {
            if (Utl::HasInstance())
            {
                Utl::Instance()->TriggerTestCleanupEvent(tests[*pTestNum].get());
            }

            tests[*pTestNum]->getStateReport()->beforeCleanup();
            tests[*pTestNum]->BeforeCleanup();
        }

        // Do not clean up resources on quick_exit
        if (!quick_exit)
        {
            bool bLastStage = false;
            for (UINT32 stage = 0; !bLastStage; ++stage)
            {
                bLastStage = true;
                for (pTestNum = testsToCleanUp.begin();
                     pTestNum != testsToCleanUp.end(); ++pTestNum)
                {
                    if (stage < tests[*pTestNum]->NumCleanUpStages())
                    {
                        lwrrent_thread = *pTestNum;
                        tests[*pTestNum]->CleanUp(stage);
                        lwrrent_thread = -1;
                        bLastStage = false;
                    }
                }
            }
        }

        for (pTestNum = testsToCleanUp.begin();
             pTestNum != testsToCleanUp.end(); ++pTestNum)
        {
            lwrrent_thread = *pTestNum;
            UpdateTestStatus(*pTestNum);
            lwrrent_thread = -1;
            tests[*pTestNum]->getStateReport()->afterCleanup();
            test_state[*pTestNum] = TEST_STATE_CLEANED;
        }
    }

    return !testsToCleanUp.empty();
}

RC TestDirectory::ReinitRegSettingsBetweenTests()
{
    InfoPrintf("Reinitialize priv/conf register settings between tests.\n");

    LWGpuResource* lwgpu = NULL;
    lwgpu = LWGpuResource::FindFirstResource();

    if (lwgpu == NULL)
    {
        ErrPrintf("Error: cannot find LWGpuResouce or it has been freed.");
        return RC::SOFTWARE_ERROR;
    }

    StickyRC src;

    for (auto pLWGpuResource : LWGpuResource::GetAllResources())
    {
        src = pLWGpuResource->ReinitializeRegSettings();
    }

    return src;
}

// Setup a stage of a specific test
void TestDirectory::SetupTest(int testIndex, int stage)
{
    InfoPrintf("TestDirectory::SetupAllTests: do setup of test=%d, stage=%d\n",
               testIndex, stage);

    if (stage == 0)
    {
        tests[testIndex]->getStateReport()->beforeSetup();
    }

    if (tests[testIndex]->NumSetupStages() > stage)
    {
        // Setup the thread variable during setup so mem allocation check
        // can match up thread id's
        lwrrent_thread = testIndex;
        tests[testIndex]->SetThreadId2TestMap(Tasker::LwrrentThread());
        test_id_being_setup = tests[testIndex]->GetSeqId();
        test_name_being_setup = tests[testIndex]->GetTestName();
        activate_test_event();
        int status = tests[testIndex]->Setup(stage);
        deactivate_test_event();
        test_id_being_setup = 0;
        test_name_being_setup = "";

        if ((status == 0) || (tests[testIndex]->NumSetupStages() <= (stage + 1)))
        {
            // if this is the first pass through setup, and only one test is in the run,
            //  and setup failed, raise a critical error
            if ((status == 0) && first_setup_pass && (num_tests == 1))
            {
                tests[testIndex]->getStateReport()->error("Test failed to initialize correctly.");
                CritPrintf("Single test specified failed to initialize correctly\n");
                UpdateTestStatus(testIndex);
            }

            tests[testIndex]->getStateReport()->afterSetup();
            if (status)
            {
                test_state[testIndex] = TEST_STATE_SET_UP;
            }
            else
            {
                // If we fail to Setup, just mark it cleaned
                // so we don't keep trying to setup the test.
                // The "CLEANED" state will no longer be updated
                // when doing cleanup, so update the status here
                test_state[testIndex] = TEST_STATE_CLEANED;
                UpdateTestStatus(testIndex);
            }
        }

        lwrrent_thread = -1;
    }
}

// updates trep file with test status
void TestDirectory::UpdateTestStatus(int i)
{
    stringstream ss;
    ss << "test #" << i << ": ";
    m_Trep->AppendTrepString( ss.str() );

    bool isGold = false;
    switch (tests[i]->GetStatus())
    {
        case Test::TEST_NOT_STARTED:
            m_Trep->AppendTrepString( "TEST_NOT_STARTED\n" );
            break;

        case Test::TEST_INCOMPLETE:
            m_Trep->AppendTrepString( "TEST_INCOMPLETE\n" );
            break;

        case Test::TEST_SUCCEEDED:
            isGold = tests[i]->getStateReport()->isStatusGold();
            ss.str("");  // clear previous output to the stream
            ss << "TEST_SUCCEEDED" << (isGold ? " (gold)" : "") << "\n";
            m_Trep->AppendTrepString( ss.str() );
            break;

        case Test::TEST_FAILED:
            m_Trep->AppendTrepString( "TEST_FAILED\n" );
            break;

        case Test::TEST_NO_RESOURCES:
            m_Trep->AppendTrepString( "TEST_NO_RESOURCES\n" );
            break;

        case Test::TEST_FAILED_CRC:
            m_Trep->AppendTrepString( "TEST_FAILED_CRC\n" );
            break;

        case Test::TEST_CRC_NON_EXISTANT:
            m_Trep->AppendTrepString( "TEST_CRC_NON_EXISTANT\n" );
            break;

        case Test::TEST_FAILED_PERFORMANCE:
            m_Trep->AppendTrepString( "TEST_CRC_NON_EXISTANT\n" );
            break;

        case Test::TEST_FAILED_ECOV:
            m_Trep->AppendTrepString( "TEST_FAILED_ECOV_MATCH\n" );
            break;

        case Test::TEST_ECOV_NON_EXISTANT:
            m_Trep->AppendTrepString( "TEST_FAILED_ECOV_MISSING\n" );
            break;

        default:
            m_Trep->AppendTrepString( "unknown\n" );
            break;
    }
}

// reports status of every test - returns 1 if everything passed, 0 otherwise
RC TestDirectory::ReportTestResults(void)
{
    unsigned i;

    UINT64 max_start = 0, min_end = 0; // XXX initializing min_end to 0 looks wrong, but this preserves old behavior
    UINT32 min_num_yields = 0xffffffff;
    bool conlwrrency_failed = false;
    if (require_conlwrrent) {
        UINT32 num_yields;
        double total_times = 0.;
        UINT32 total_yields = 0;
        for (i = 0; i < num_tests; i++) {
            UINT64 start = tests[i]->GetEndTime();
            UINT64 end = tests[i]->GetStartTime();
            num_yields = tests[i]->GetNumYields();
            total_times += double(start - end);
            total_yields += num_yields;
            if (max_start < start) {
                max_start = start;
            }
            if (end < min_end) {
                min_end = end;
            }
            if (num_yields < min_num_yields) {
                min_num_yields = num_yields;
            }
        }

        // Callwlate average time slice

        double ave_timeslice = total_times/total_yields;
        if (max_start < min_end) {
            InfoPrintf("Conlwrrency not achieved, some test run serially\n");
            conlwrrency_failed = true;
        }
        if (ave_timeslice > 1e-3) {
            InfoPrintf("Conlwrrency not effective, average timeslice too long\n");
            conlwrrency_failed = true;
        }
        if (!conlwrrency_failed) {
            InfoPrintf("Conlwrrency successful\n");
        }
    }

    RC rc = OK;
    for (i = 0; i < num_tests; i++) {
        if (Tee::WillPrint(Tee::PriNormal))
        {
            ArgReader test_param(test_entries[i]->params);
            test_param.ParseArgs(test_args[i].get());
            InfoPrintf("%-20s (", test_entries[i]->name);
            test_param.PrintParsedArgs();
            InfoPrintf(") : ");
        } else {
            InfoPrintf("%-30s :", test_entries[i]->name);
        }

        RC testrc = OK;
        if (conlwrrency_failed) {
            ErrPrintf("conlwrrency failed\n");
            testrc = RC::SOFTWARE_ERROR;
        } else if (test_state[i] != TEST_STATE_INITIAL) {
            switch (tests[i]->GetStatus()) {
            case Test::TEST_NOT_STARTED:
                ErrPrintf("not started!\n");
                testrc = RC::SPECIFIC_TEST_NOT_RUN;
                break;

            case Test::TEST_INCOMPLETE:
                ErrPrintf("incomplete!\n");
                testrc = RC::SOFTWARE_ERROR;
                break;

            case Test::TEST_SUCCEEDED:
                InfoPrintf("passed\n");
                break;

            case Test::TEST_FAILED:
                ErrPrintf("failed!\n");
                testrc = RC::GOLDEN_VALUE_MISCOMPARE;
                break;

            case Test::TEST_NO_RESOURCES:
                ErrPrintf("no resources!\n");
                testrc = RC::SOFTWARE_ERROR;
                break;

            case Test::TEST_FAILED_CRC:
                ErrPrintf("image CRCs didn't match!\n");
                testrc = RC::GOLDEN_VALUE_MISCOMPARE;
                break;

            case Test::TEST_CRC_NON_EXISTANT:
                ErrPrintf("CRC profile not present!\n");
                testrc = RC::GOLDEN_VALUE_NOT_FOUND;
                break;

            case Test::TEST_FAILED_PERFORMANCE:
                ErrPrintf("Performance too low!\n");
                testrc = RC::GOLDEN_VALUE_MISCOMPARE;
                break;

            case Test::TEST_FAILED_ECOV:
                ErrPrintf("Ecov events does not match!\n");
                testrc = RC::GOLDEN_VALUE_MISCOMPARE;
                break;

            case Test::TEST_ECOV_NON_EXISTANT:
                ErrPrintf("Failed to process ecov data!\n");
                testrc = RC::FILE_DOES_NOT_EXIST;
                break;

            default:
                ErrPrintf("unknown result (%d)!\n", tests[i]->GetStatus());
                testrc = RC::SOFTWARE_ERROR;
                break;
            }
        } else {
            ErrPrintf("set up failed\n");
            testrc = RC::SOFTWARE_ERROR;
        }
        if (OK == rc)
        {
            rc = testrc;
        }
    }

    if (OK == rc) {
        InfoPrintf("Mdiag: all tests passed\n");
        m_Trep->AppendTrepString( "summary = all tests passed\n" );
    } else {
        ErrPrintf("Mdiag: some tests failed\n");
        m_Trep->AppendTrepString( "summary = some tests failed\n" );
    }
    m_Trep->AppendTrepString(
        Utility::StrPrintf("expected results = %u\n", num_tests).c_str());

    return rc;
}

// describes expected params for all tests who match "name"
/*static*/ void TestDirectory::DescribeTestParams(const char *name)
{
    TestListEntryPtr pos;

    // Always print the test directory's params
    InfoPrintf(" test directory:\n");
    ArgReader::DescribeParams(testdir_params);
    InfoPrintf("\n");

    pos = GetTestListHead();
    while (pos)
    {
        if (pos->params &&
            (!name || !strcmp(name, pos->name) || !strcmp(name, "all")))
        {
            InfoPrintf(" %-10s (test):\n", pos->name);
            ArgReader::DescribeParams(pos->params);
            InfoPrintf("\n");
        }

        pos = pos->next;
    }
}

RC TestDirectory::ReportResultsAndClean()
{
    RC rc = ReportTestResults();
    for ( unsigned i=0; i < num_tests; i++ )
    {
        tests[i].reset(0);
    }
    num_tests = 0;
    return rc;
}
