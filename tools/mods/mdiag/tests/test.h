/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation. 
 * All rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TEST_H_
#define _TEST_H_

#include "core/include/lwrm.h"
#include "core/include/utility.h"
#include "gpu/utility/ecov_verifier.h"

#include "mdiag/smc/smcengine.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/ITestStateReport.h"
#include "mdiag/utils/TestInfo.h"

class ArgDatabase;
class TestStateReport;
struct ParamDecl;
class SmcEngine;
class LwRm;
class LWGpuResource;
class IGpuSurfaceMgr;
class TraceFileMgr;

class UtlGpuVerif;
class LWGpuChannel;

// RULES FOR TESTS:

// all tests in the conlwrrent diag environment are C++ class that inherit from
//  class Test, defined below.

// the test is added to the list of available tests by including the code
//  contained inside the two 'ifdef DEFINE_YOUR_OWN_FACTORY_FUNCTION' blocks
//  AND including your test's .h file from "test_list.h"

// the only methods you NEED to override in your subclass are Run() and the
//  factory function (not really overriding, but you do need to define it)

// The Run() method must include at least one call to Yield().  That's the only
//  way cooperative multitasking works.  Any busy-wait loop MUST contain a call
//  to Yield(), and additional calls are appreciated.

// ALL output from a test should be done via the {Err,Warn,Info,Debug,Raw}Printf()
//  functions defined in sysspec/common.h (actually obtained by
//  #include'ing "mdiag/sysspec.h"

// ALL random numbers should be obtained from a instance of the RandomStream class
//  defined in "randstrm.h"

// Actually, I can't think of any reason to use any libc functions other than
//  file IO, and even those have restrictions on them (i.e. try not to use them
//  during the Run() method).  If you have other functions you want to call,
//  please run them by me (Sean Treichler - sean@lwpu.com).  Some are probably
//  fine, but others will cause lots of grief in a multi-threaded environment...

class Test {
 public:
  // the test constructor can set up local variables and read data files, but it
  //  shouldn't try talking to HW
  Test();

  // please clean up any mess you make
  virtual ~Test(void);

  static void FailAllTests(const char *msg);

  // test status - a test will start out with its status set to TEST_NOT_STARTED -
  //   it's up to the Run() function to update the status (typically to
  //   TEST_INCOMPLETE at the beginning, and to _SUCCEEDED or _FAILED at the end)
  enum TestStatus { TEST_NOT_STARTED        = TestEnums::TEST_NOT_STARTED,
                    TEST_INCOMPLETE         = TestEnums::TEST_INCOMPLETE,
                    TEST_SUCCEEDED          = TestEnums::TEST_SUCCEEDED,
                    TEST_FAILED             = TestEnums::TEST_FAILED,
                    TEST_FAILED_TIMEOUT     = TestEnums::TEST_FAILED_TIMEOUT,
                    TEST_FAILED_CRC         = TestEnums::TEST_FAILED_CRC,
                    TEST_NO_RESOURCES       = TestEnums::TEST_NO_RESOURCES,
                    TEST_CRC_NON_EXISTANT   = TestEnums::TEST_CRC_NON_EXISTANT,
                    TEST_FAILED_PERFORMANCE = TestEnums::TEST_FAILED_PERFORMANCE,
                    TEST_FAILED_ECOV        = TestEnums::TEST_FAILED_ECOV,
                    TEST_ECOV_NON_EXISTANT  = TestEnums::TEST_ECOV_NON_EXISTANT
  };

 protected:
  TestStatus status;

 public:

  void SetStatus(TestStatus new_status) { status = new_status; }
  void SetStatus(TestEnums::TEST_STATUS new_status) { status = (TestStatus)new_status; }
  TestStatus GetStatus(void) const { return(status); }

  // factory function : Takes a pointer to an arg database for command-line args
#ifdef DEFINE_YOUR_OWN_FACTORY_FUNCTION
  static Test *FactoryFunction(ArgDatabase *args);
#endif

  // when this is called, a test should do all set-up, 
  // hopefully including all file I/O, but not actually run the test - 
  // it should return 1 if everything goes ok, and 0 if the setup fails 
  // (if the setup fails, neither Run() nor CleanUp() will be called)
  virtual int Setup(void) = 0;
  virtual int Setup(int stage);
  virtual int NumSetupStages();

  // a test that runs in an endless loop should rely on the three methods below
  //  to know when to run and when to be quiet
  // NOTE: these methods should NOT touch the HW - they are likely to be called
  //  in a different thread from the Run() method - instead they should just set
  //  flag(s) that the Run() method will look for
  virtual void EndTest(void) {}

  // This method is called at just before Run to enable tests to do generic pre run work
  // This allows all tests to override the Run method, but have common post run work done
  virtual void PreRun(void) {};

  // this is the main exelwtion function for the test - it should run whatever
  //  the test entails, using only resources allocating during the Setup() task,
  //  and preferably avoiding any file IO.  If the test is self-checking, it
  //  can update the status as it goes.  If it needs to consult files to determine
  //  whether or not the test succeeded, it should wait until CleanUp() is called.
  virtual void Run(void) = 0;

  // This method is called at the end of a run to enable tests to do generic post run work
  // This allows all tests to override the Run method, but have common post run work done
  virtual void PostRun(void);

  // when this is called, the test should release any resources it grabbed during
  //  the Setup() method, after checking the result of the test if necessary
  virtual void CleanUp(void) = 0;
  virtual void CleanUp(UINT32 stage);
  virtual void BeforeCleanup(void);
  virtual UINT32 NumCleanUpStages() const;

  // Return the name of the test - used for informational prints
  virtual const char* GetTestName();

  // Returns if the test supports Ecover checking
  // Only v2d and trace_3d support Ecover checking now
  virtual bool IsEcoverSupported() { return false; }
  virtual RC SetupEcov
  (
      unique_ptr<EcovVerifier>* pEcovVerifier,
      const ArgReader* params,
      bool isCtxSwitchTest,
      const string& testName,
      const string& testChipName
  );
  virtual void VerifyEcov
  (
      EcovVerifier* pEcovVerifier,
      const ArgReader* params,
      const LWGpuResource* lwgpu,
      bool isCtxSwitchTest,
      TestStatus* status
  );

  // Setup ecover for all Mdiag tests
  virtual void EnableEcoverDumpInSim(bool enabled);

  virtual void AbortTest() {};
  virtual bool AbortedTest() const { return false; }
  virtual IGpuSurfaceMgr * GetSurfaceMgr() { return nullptr; }
  virtual TraceFileMgr* GetTraceFileMgr() { return nullptr; }

  void StartTimer()   {start_time = Platform::GetTimeUS();}
  void EndTimer()     {end_time = Platform::GetTimeUS();}

  UINT64 GetStartTime() {return start_time;}
  UINT64 GetEndTime()   {return end_time;}
  UINT32 GetNumYields()  {return num_yields;}

  int GetSeqId() const  {return sequential_id;}
  static int GetNextSeqId()   {return next_sequential_id;}

  // This function is to set smcengine for trace3dtest
  virtual void SetSmcEngine(SmcEngine * pSmcEngine) {};

  virtual SmcEngine* GetSmcEngine();
  virtual LwRm* GetLwRmPtr();
  virtual UINT32 GetTestId() const { return _UINT32_MAX; }
  virtual LWGpuResource* GetGpuResource() const { return nullptr; }

  static const ParamDecl Params[];

  virtual UtlGpuVerif * CreateGpuVerif(LwRm::Handle hVaSpace, LWGpuChannel * pCh) { 
      MASSERT(!"Not support test type"); 
      return nullptr; 
  }
 protected:
  // protected to make it a little harder to call the wrong one
  // if set, 'min_delay_in_usec' guarantees at least that amount of time will
  //  pass before Yield() returns
  void Yield(unsigned min_delay_in_usec = 0);

  // Note: To be truly global, this should be placed in testdir
  typedef enum GlobalValueName
  {   GLOBAL_PERFMON_INIT = 0
  ,   GLOBAL_PERFMON_START
  ,   GLOBAL_PERFMON_END
  ,   NUM_GLOBALS
  } GlobalValueName;

  int GlobalTest(GlobalValueName global_name) {return global_values[global_name];}
  int GlobalTestAndSet(GlobalValueName global_name, int value=1)
  {
      int ret = global_values[global_name];
      global_values[global_name] = value;
      return ret;
  }

  int GlobalIncrement(GlobalValueName global_name) {return ++global_values[global_name];}
  int GlobalDecrement(GlobalValueName global_name) {return --global_values[global_name];}

 private:
  static int global_values[NUM_GLOBALS];

  // Each time is a sec and usec component
  UINT64 start_time, end_time;
  UINT32 num_yields;

  int sequential_id;
  static int next_sequential_id;

  TestStateReport *stateReport;
  ITestStateReport *istateReport;

  static map <UINT32, Test *> s_ThreadId2Test;
  static UINT32 s_NumEcoverSupportedTests;
  static UINT32 s_SmcEngineTraceIndex;
  static bool s_IsSmcNamePresent;

 public:
   TestStateReport *getStateReport() {  return stateReport;  }
   ITestStateReport *getIStateReport() {  return istateReport;  }

   static void IncreaseNumEcoverSupportedTests() { s_NumEcoverSupportedTests++; }
   static UINT32 GetNumEcoverSupportedTests() { return s_NumEcoverSupportedTests; }

   static Test *FindTestForThread(UINT32 threadId);
   static Test *GetFirstTestFromMap();
   void SetThreadId2TestMap(UINT32 threadId);
   virtual const ArgReader* GetParam() const { return nullptr; }
   string ParseSmcEngArg();
};

typedef struct TestListEntry *TestListEntryPtr;

struct TestListEntry {
  const char *name;
  const char *desc;
  const ParamDecl *params;
  Test *(*factory_fn)(ArgDatabase *args);
  TestListEntryPtr next;
};

#define CREATE_TEST_LIST_ENTRY(testname, classname, description) \
extern const ParamDecl testname##_params[]; \
static TestListEntry testname##_testentry = { #testname, \
                                              description, \
                                              testname##_params, \
                                              classname::Factory, \
                                              TEST_LIST_HEAD}

// Create a boilerplate factory function for a test.  Assumes some naming
// colwentions, but you probably should follow those colwentions anyway.
#define STD_TEST_FACTORY(testname, classname) \
Test *classname::Factory(ArgDatabase *args) \
{ \
    ArgReader *reader = new ArgReader(testname##_params); \
    if (!reader || !reader->ParseArgs(args)) \
    { \
        ErrPrintf(#testname ": Factory() error reading parameters!\n"); \
        return(0); \
    } \
    return new classname(reader); \
}

// Create a boilerplate factory function for a test that may operate on a
// specific device.  Note that any tests which operate on a specific device
// must add PARAM_SUBDECL(test_device_params) to their parameter list Assumes
// some naming colwentions, but you probably should follow those colwentions
// anyway.
#define STD_DEV_TEST_FACTORY(testname, classname) \
Test *classname::Factory(ArgDatabase *args) \
{ \
    ArgReader  *reader = new ArgReader(testname##_params); \
    if (!reader || !reader->ParseArgs(args)) \
    { \
        ErrPrintf(#testname ": Factory() error reading parameters!\n"); \
        return(0); \
    } \
    if (reader->ParamPresent("-usedev") || reader->ParamPresent("-multi_gpu")) \
    { \
        UINT32 deviceInst = 0; \
        string scope; \
        deviceInst = reader->ParamUnsigned("-usedev", 0); \
        if (!reader->ParamPresent("-usedev") && reader->ParamPresent("-multi_gpu")) \
        { \
            WarnPrintf(#testname " : -multi_gpu specified without -usedev.  Testing device 0!\n"); \
        } \
        scope = Utility::StrPrintf("dev%d", deviceInst); \
        delete reader; \
        reader = new ArgReader(testname##_params); \
        if (!reader || !reader->ParseArgs(args, scope.c_str())) \
        { \
            ErrPrintf(#testname ": Factory() error reading device scoped parameters!\n"); \
            return(0); \
        } \
    } \
    return new classname(reader); \
}

#define test_params Test::Params

#ifdef MAKE_TEST_LIST

#ifdef DEFINE_YOUR_OWN_FACTORY_FUNCTION

extern const ParamDecl your_test_params[];

static TestListEntry template_testentry = { "test name here",
                                            "test description here",
                                            your_test_params,
                                            YourTest::FactoryFunction,
                                            TEST_LIST_HEAD };
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &template_testentry

#else

#define TEST_LIST_HEAD 0

#endif

#endif

#endif
