/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//Mdiag_Rm_Test Infrastructure
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/cmdline.h"
#include "mdiag/utils/mdiag_xml.h"
#include "core/utility/trep.h"
#include "core/include/version.h"

#include "core/include/gpu.h"

#include "core/include/mrmtestinfra.h"

//Need to find suitable RC error code, till then using software error
#define ERRCODE RC::SOFTWARE_ERROR

#define CHECK_NULL(ptr, msg) {if(!ptr){Printf(Tee::PriNormal,msg); \
                              return ERRCODE;}}

#define ERRORIF(cond, msg) {if(cond){Printf(Tee::PriNormal,msg); \
                              return ERRCODE;}}

/*
 *MrmTest Class methods
 */

MrmTest::MrmTest(string tname, bool worker)
{
    name            = tname;
    ImWorker         = worker;
    indTestProgress = tsNotStarted;
}

//Start a particular test. Allocates mutex which will be used to modify the status of this test.
void MrmTest::StartTest()
{
    indTestProgress = tsInProgress;
}

//Set the test status.. Four states are possible.
void MrmTest::SetTestStatus(TestState Status)
{
    indTestProgress = Status;
}

//Get status
TestState MrmTest::GetTestStatus()
{
    return indTestProgress;
}

string MrmTest::GetName()
{
    return name;
}

/*
 *MrmTestInfra namespace methods
 */
namespace MrmTestInfra
{
    TestState TestProgress    = tsNotStarted;
    UINT32 TotalNumberOfTests = 0;
    UINT32 TestsLoopsArg = 1;

    vector<MrmTest*> testObjects;
    vector<MrmMutex *> availableMutexes;
    vector<MrmEvent> alloctedEvents;
    string g_RmTestName;

    RC            StartInitializeTest();
    RC            FinishInitializeTest();
    void          SetTestStatus(TestState);
    RC            InitializeObject(string, bool);
    MrmTest*      GetThreadObject(string);
    RC            StartTest(string, bool);
    RC            FinishTest(string);
    RC            FinishAllTests();
    bool          CheckAllSetupsDone(string, UINT32*);
    bool          CheckAllTestsDone(string, UINT32*);
    RC            CreateMutexes(UINT32);
    RC            DeleteMutexes();
    MrmMutex*     GetMutexObject(string);
    MrmEvent*     GetEventObject(string);
    RC            SetTestLoops(UINT32);
}

void MrmTestInfra::SetTestStatus(TestState status)
{
    MrmTestInfra::TestProgress = status;
}

RC MrmTestInfra::SetTestLoops(UINT32 loops)
{
    RC rc = OK;
    ERRORIF(MrmTestInfra::TestProgress != tsNotStarted, "MRMI: Test already initialized\n");
    MrmTestInfra::TestsLoopsArg = loops;
    return rc;
}

RC MrmTestInfra::StartInitializeTest()
{
    RC rc = OK;
    ERRORIF(MrmTestInfra::TestProgress != tsNotStarted, "MRMI: Test already initialized\n");
    MrmTestInfra::SetTestStatus(tsConstructingTests);
    return rc;
}

RC MrmTestInfra::FinishInitializeTest()
{
    RC rc = OK;
    ERRORIF(MrmTestInfra::TestProgress != tsConstructingTests, "MRMI: Should start initialize before finishing it in FinishInitializeTest\n");
    MrmTestInfra::SetTestStatus(tsInProgress);
    return rc;
}

//Initialize individual test object per thread.
RC MrmTestInfra::InitializeObject(string threadName, bool IsWorker)
{
    MrmTestInfra::testObjects.push_back(new MrmTest(threadName, IsWorker));
    return OK;

}

//Get object based on thread name
MrmTest* MrmTestInfra::GetThreadObject(string threadName)
{
    UINT32 ind = 0;
    while(ind < MrmTestInfra::testObjects.size())
    {
        if(threadName == MrmTestInfra::testObjects[ind]->GetName())
             return MrmTestInfra::testObjects[ind];
        ++ind;
    }
    return NULL;
}

//Start a specific test. This will initialize the object for given thread and starts the test.
RC MrmTestInfra::StartTest(string threadName, bool IsWorker)
{
    RC rc = OK;
    MrmTest *threadObj;
    if(MrmTestInfra::TestProgress == tsNotStarted)
        return rc;//Not running rm/mdiag multithreaded mode, skip.

    ERRORIF(MrmTestInfra::TestProgress != tsConstructingTests,"MRMI: Should have initialized the test before calling StartTest\n");
    Printf(Tee::PriNormal, "MRMI: Starting test for thread %s\n",threadName.c_str());
    CHECK_RC(MrmTestInfra::InitializeObject(threadName, IsWorker));
    threadObj = MrmTestInfra::GetThreadObject(threadName);
    CHECK_NULL(threadObj, "MRMI: GetThreadObject returns NULL in StartTest");
    threadObj->StartTest();
    return rc;
}

//Finish a test by setting a state.
RC MrmTestInfra::FinishTest(string threadName)
{
    RC rc = OK;
    if(MrmTestInfra::TestProgress == tsNotStarted)
        return rc;//Not running rm/mdiag multithreaded mode, skip.
    ERRORIF(MrmTestInfra::TestProgress != tsInProgress, "MRMI: Should have initialized/started the test before calling FinishTest\n");
    Printf(Tee::PriNormal, "MRMI: Finishing test for thread %s\n",threadName.c_str());
    MrmTest *threadObj = MrmTestInfra::GetThreadObject(threadName);
    CHECK_NULL(threadObj, "MRMI: GetThreadObject returns NULL in FinishTest\n");
    threadObj->SetTestStatus(tsComplete);
    return rc;
}

//Cleanup objects and finish all the tests
RC MrmTestInfra::FinishAllTests()
{
    RC rc = OK;
    UINT32 ind = 0;
    MrmTestInfra::DeleteMutexes();
    if(MrmTestInfra::TestProgress == tsNotStarted)
        return rc; //Not running rm/mdiag multithreaded mode, skip.
    Printf(Tee::PriNormal, "MRMI: Finishing all the tests\n");
    MrmTestInfra::SetTestStatus(tsComplete);
    ind = (UINT32)MrmTestInfra::testObjects.size();
    while(!MrmTestInfra::testObjects.empty())
    {
        MrmTest *thread = MrmTestInfra::testObjects[--ind];
        if(!thread)
            continue;
        if (thread->GetTestStatus() != tsComplete)
        {
            Printf(Tee::PriNormal,"MRMI: Warning! Finishing All tests when a test is not complete yet? Either TestDoneWaitingForOthers is not used or test never started.\n");
        }
        thread->SetTestStatus(tsComplete);
        delete thread;
        MrmTestInfra::testObjects.pop_back();
    }
    return rc;
}

bool MrmTestInfra::CheckAllSetupsDone(string threadName, UINT32 *startPos)
{
    MrmTest *threadObj = NULL;
    UINT32 status;
    while(*startPos < MrmTestInfra::testObjects.size())
    {
        threadObj = testObjects[*startPos];
        status = threadObj->GetTestStatus();
        if((threadObj->GetName()==threadName)||(status == tsSetupDone) || (status == tsComplete))
        {
            (*startPos)++;
            continue;
        }
        return false;
    }
    return true;
}

bool MrmTestInfra::CheckAllTestsDone(string threadName, UINT32 *startPos)
{
    MrmTest *threadObj = NULL;
    UINT32 status;
    while(*startPos < MrmTestInfra::testObjects.size())
    {
        threadObj = testObjects[*startPos];
        status = threadObj->GetTestStatus();
        if((threadObj->GetName()==threadName)||(status == tsComplete))
        {
            (*startPos)++;
            continue;
        }
        return false;
    }
    return true;
}

//Create mutexes and assign unique to them. Name starts with Mutex_
RC MrmTestInfra::CreateMutexes(UINT32 noOfMutexes)
{
    RC rc = OK;
    UINT32 ind=0;
    Printf(Tee::PriNormal,"MRMI: Creating %d mutexes:  ",noOfMutexes);
    while((ind++) < noOfMutexes)
    {
        string name;
        MrmMutex *lwrrentMutex = new MrmMutex;
        name = Utility::StrPrintf("%s%u","Mutex_",ind);
        lwrrentMutex->name = name;
        lwrrentMutex->mutex = Tasker::AllocMutex(name.c_str(),
                                                 Tasker::mtxUnchecked);
        Printf(Tee::PriNormal,"%s, ",name.c_str());
        MrmTestInfra::availableMutexes.push_back(lwrrentMutex);
    }
    Printf(Tee::PriNormal,"\n");
    return rc;
}

//Delete the mutexes in the end
RC MrmTestInfra::DeleteMutexes()
{
    RC rc = OK;
    UINT32 ind=0;
    Printf(Tee::PriNormal,"MRMI: Deleting mutexes\n");
    while(ind < MrmTestInfra::availableMutexes.size())
    {
        Tasker::FreeMutex(MrmTestInfra::availableMutexes[ind]->mutex);
        delete MrmTestInfra::availableMutexes[ind];
        ++ind;
    }
    MrmTestInfra::availableMutexes.clear();
    return rc;
}

//Get mutex object using the mutex name
MrmMutex* MrmTestInfra::GetMutexObject(string mutexName)
{
    UINT32 ind=0;
    while(ind < MrmTestInfra::availableMutexes.size())
    {
        if(mutexName == MrmTestInfra::availableMutexes[ind]->name)
            return MrmTestInfra::availableMutexes[ind];
        ++ind;
    }
    return NULL;
}

//Functions exposed to CPP code
//Acquire mutex. Pass mutex name
RC MrmCppInfra::AcquireMutex(const char *cmutexName)
{
    RC rc = OK;
    string mutexName(cmutexName);
    MrmMutex *mutexObj = MrmTestInfra::GetMutexObject(mutexName);

    Printf(Tee::PriNormal,"MRMI: Acquiring mutex %s\n",cmutexName);
    CHECK_NULL(mutexObj,"MRMI: GetMutexObject returns NULL in AcquireMutex\n");
    Tasker::AcquireMutex(mutexObj->mutex);
    Printf(Tee::PriNormal,"MRMI: Mutex %s acquired\n",cmutexName);
    return rc;
}

//Release a mutex
RC MrmCppInfra::ReleaseMutex(const char *cmutexName)
{
    RC rc = OK;
    string mutexName(cmutexName);
    MrmMutex *mutexObj = MrmTestInfra::GetMutexObject(mutexName);

    Printf(Tee::PriNormal,"MRMI: Releasing mutex %s\n",cmutexName);
    CHECK_NULL(mutexObj,"MRMI: GetMutexObject returns NULL in ReleaseMutex\n");
    Tasker::ReleaseMutex(mutexObj->mutex);
    return rc;
}

RC MrmCppInfra::AllocEvent(string eventName)
{
    RC rc;

    ModsEvent *pEvent = Tasker::AllocEvent(eventName.c_str());
    if (!pEvent)
    {
        return RC::SOFTWARE_ERROR;
    }

    MrmEvent mrmEvent;
    mrmEvent.eventName = eventName;
    mrmEvent.pModsEvent = pEvent;
    MrmTestInfra::alloctedEvents.push_back(mrmEvent);

    return rc;
}

RC MrmCppInfra::FreeEvent(string eventName)
{
    RC rc;

    for (UINT32 pos = 0 ; pos < MrmTestInfra::alloctedEvents.size(); pos++)
    {
        if (eventName == MrmTestInfra::alloctedEvents[pos].eventName)
        {
            Tasker::FreeEvent(MrmTestInfra::alloctedEvents[pos].pModsEvent);
            MrmTestInfra::alloctedEvents.erase(MrmTestInfra::alloctedEvents.begin() + pos);
            return rc;
        }
    }

    rc = RC::SOFTWARE_ERROR;
    Printf(Tee::PriNormal, "MRMI %s Event not found", __FUNCTION__);

    return rc;
}

void MrmCppInfra::FreeAllEvents()
{
    string eventName;

    for (UINT32 pos = 0 ; pos < MrmTestInfra::alloctedEvents.size(); pos++)
    {
        eventName = MrmTestInfra::alloctedEvents[pos].eventName;
        Tasker::FreeEvent(MrmTestInfra::alloctedEvents[pos].pModsEvent);
        MrmTestInfra::alloctedEvents.erase(MrmTestInfra::alloctedEvents.begin() + pos);
    }
}

RC MrmCppInfra::SetEvent(string eventName)
{
    RC rc;

    for (UINT32 pos = 0; pos < MrmTestInfra::alloctedEvents.size(); pos++)
    {
        if (eventName == MrmTestInfra::alloctedEvents[pos].eventName)
        {
            Tasker::SetEvent(MrmTestInfra::alloctedEvents[pos].pModsEvent);
            return rc;
        }
    }

    rc = RC::SOFTWARE_ERROR;

    Printf(Tee::PriNormal, "MRMI %s Event not found",__FUNCTION__);
    return rc;
}

RC MrmCppInfra::ResetEvent(string eventName)
{
    RC rc;

    for (UINT32 pos = 0; pos < MrmTestInfra::alloctedEvents.size(); pos++)
    {
        if (eventName == MrmTestInfra::alloctedEvents[pos].eventName)
        {
            Tasker::ResetEvent(MrmTestInfra::alloctedEvents[pos].pModsEvent);
            return rc;
        }
    }

    rc = RC::SOFTWARE_ERROR;

    Printf(Tee::PriNormal, "MRMI %s Event not found",__FUNCTION__);
    return rc;
}

RC MrmCppInfra::WaitOnEvent
(
    string eventName,
    FLOAT64 timeoutMs
)
{
    RC rc;
    Printf(Tee::PriNormal, "MRMI Waiting for event %s\n", eventName.c_str());
    for (UINT32 pos = 0 ; pos < MrmTestInfra::alloctedEvents.size(); pos++)
    {
        if (eventName == MrmTestInfra::alloctedEvents[pos].eventName)
        {
            CHECK_RC(Tasker::WaitOnEvent(MrmTestInfra::alloctedEvents[pos].pModsEvent, timeoutMs));
            return rc;
        }
    }

    rc = RC::SOFTWARE_ERROR;
    Printf(Tee::PriNormal, "MRMI %s Event not found", __FUNCTION__);
    return rc;
}

bool MrmCppInfra::IsEventSet (string eventName)
{
   for (UINT32 pos = 0 ; pos < MrmTestInfra::alloctedEvents.size(); pos++)
    {
        if (eventName == MrmTestInfra::alloctedEvents[pos].eventName)
        {
            return Tasker::IsEventSet(MrmTestInfra::alloctedEvents[pos].pModsEvent);
        }
    }

    Printf(Tee::PriNormal, "MRMI %s Event not found", __FUNCTION__);
    return false;
}

//Setup done. Check if other threads finished it else wait.
RC MrmCppInfra::SetupDoneWaitingForOthers(string threadName)
{
    RC rc = OK;
    UINT32 setupCheckInd  = 0;
    UINT32 SleepMsPerLoop = 100;
    MrmTest *threadObj;
    UINT32 status = 0;
    if(MrmTestInfra::TestProgress == tsNotStarted)
        return rc;//Not running rm/mdiag multithreaded mode, skip.

    ERRORIF(MrmTestInfra::TestProgress != tsInProgress, "MRMI: Should have initialized/started the test before calling SetupDoneWaitingForOthers\n");

    threadObj = MrmTestInfra::GetThreadObject(threadName);
    CHECK_NULL(threadObj, "MRMI: GetThreadObject returns NULL in SetupDoneWaitingForOthers\n");
    status = threadObj->GetTestStatus();
    Printf(Tee::PriNormal,"MRMI: Setup Done for thread '%s'. Waiting for others\n",threadName.c_str());
    if(status == tsSetupDone)
    {
        Printf(Tee::PriNormal,"MRMI: Setup request repeated for thread %s\n",threadName.c_str());
        return rc;
    }
    if(status != tsInProgress)
    {
        Printf(Tee::PriNormal,"MRMI: Setup requested when thread %s is not in progress\n",threadName.c_str());
        return ERRCODE;
    }
    threadObj->SetTestStatus(tsSetupDone);
    while(!MrmTestInfra::CheckAllSetupsDone(threadName, &setupCheckInd))
    {
        Tasker::Sleep(SleepMsPerLoop);
    }
    Printf(Tee::PriNormal,"MRMI: Thread '%s' finished waiting for others in SetupDoneWaitingForOthers\n",threadName.c_str());
    return rc;
}

RC MrmCppInfra::ShouldContinueTest(string threadName, bool *result)
{
    RC rc = OK;
    MrmTest *threadObj;
    UINT32 TestCompleteCheckInd = 0;
    UINT32 status = 0;
    if(MrmTestInfra::TestProgress == tsNotStarted)
    {
        *result = false;
        return rc;//Not running rm/mdiag multithreaded mode, skip.
    }
    ERRORIF(MrmTestInfra::TestProgress != tsInProgress, "MRMI: Should have initialized/started the test before calling ShouldContinueTest\n");
    threadObj = MrmTestInfra::GetThreadObject(threadName);
    CHECK_NULL(threadObj,"MRMI: GetThreadObject returns NULL in ShouldContinueTest\n");
    status = threadObj->GetTestStatus();
    if(status != tsSetupDone)
    {
        return ERRCODE;
    }
    if(threadObj->amIWorker())
    {
        *result = MrmTestInfra::CheckAllTestsDone(threadName, &TestCompleteCheckInd);
        return rc;
    }
    threadObj->SetTestStatus(tsComplete);
    *result = false;
    return rc;
}

//This is to wait for the other tests to finish.
RC MrmCppInfra::TestDoneWaitingForOthers(string threadName)
{
    RC rc = OK;
    UINT32 SleepMsPerLoop = 100;
    MrmTest *threadObj;
    UINT32 TestCompleteCheckInd = 0;
    UINT32 status = 0;
    if(MrmTestInfra::TestProgress == tsNotStarted)
    {
        return rc;//Not running rm/mdiag multithreaded mode, skip.
    }
    ERRORIF(MrmTestInfra::TestProgress != tsInProgress, "MRMI: Should have initialized/started the test before calling TestDoneWaitingForOthers\n");
    threadObj = MrmTestInfra::GetThreadObject(threadName);
    CHECK_NULL(threadObj,"MRMI: GetThreadObject returns NULL in TestDoneWaitingForOthers\n");
    status = threadObj->GetTestStatus();
    Printf(Tee::PriNormal,"MRMI: Test Done for thread '%s'. Waiting for others\n",threadName.c_str());
    if(status != tsSetupDone)
    {
        return ERRCODE;
    }
    if(threadObj->amIWorker())
    {
        while(!MrmTestInfra::CheckAllTestsDone(threadName, &TestCompleteCheckInd))
        {
            Tasker::Sleep(SleepMsPerLoop);
        }
    }
    threadObj->SetTestStatus(tsComplete);
    Printf(Tee::PriNormal,"MRMI: Thread '%s' finished waiting for others in TestDoneWaitingForOthers\n",threadName.c_str());
    return rc;
}

bool MrmCppInfra::TestsRunningOnMRMI()
{
    bool TestsOnMrm = (MrmTestInfra::TestProgress == tsNotStarted) ?
                        false : true;
    return TestsOnMrm;

}

string MrmCppInfra::GetRmTestName()
{
    return MrmTestInfra::g_RmTestName;
}

void MrmCppInfra::SetRmTestName(string rmTestName)
{
    MrmTestInfra::g_RmTestName = rmTestName;
}

UINT32 MrmCppInfra::GetTestLoopsArg()
{
    return MrmTestInfra::TestsLoopsArg;
}

//Functions exposed to JS code
JS_CLASS(MrmJsInfra);

static SObject MrmJsInfra_Object
(
    "MrmJsInfra",
    MrmJsInfraClass,
    0,
    0,
    "Trace3d/Rmtest infrastructure"
);

//Methods

C_(MrmJsInfra_StartTest);
static STest StartTest
(
    MrmJsInfra_Object,
    "StartTest",
    C_MrmJsInfra_StartTest,
    2,
    "Starts a particular test"
);

C_(MrmJsInfra_MrmTestInfraStartInitialize);
static STest MrmTestInfraStartInitialize
(
    MrmJsInfra_Object,
    "MrmTestInfraStartInitialize",
    C_MrmJsInfra_MrmTestInfraStartInitialize,
    0,
    "Start initializing MrmJsInfra Threads"
);

C_(MrmJsInfra_MrmTestInfraFinishInitialize);
static STest MrmTestInfraFinishInitialize
(
    MrmJsInfra_Object,
    "MrmTestInfraFinishInitialize",
    C_MrmJsInfra_MrmTestInfraFinishInitialize,
    0,
    "Finish initializing MrmJsInfra Threads"
);

C_(MrmJsInfra_FinishTest);
static STest FinishTest
(
    MrmJsInfra_Object,
    "FinishTest",
    C_MrmJsInfra_FinishTest,
    1,
    "Finish a particular test"
);

C_(MrmJsInfra_FinishAllTests);
static STest FinishAllTests
(
    MrmJsInfra_Object,
    "FinishAllTests",
    C_MrmJsInfra_FinishAllTests,
    0,
    "Finish All Tests"
);

C_(MrmJsInfra_SetTestLoops);
static STest SetTestLoops
(
    MrmJsInfra_Object,
    "SetTestLoops",
    C_MrmJsInfra_SetTestLoops,
    1,
    "Set Loops Arg"
);

C_(MrmJsInfra_CreateMutexes);
static STest CreateMutexes
(
    MrmJsInfra_Object,
    "CreateMutexes",
    C_MrmJsInfra_CreateMutexes,
    1,
    "Create mutexes"
);

C_(MrmJsInfra_AcquireMutex);
static STest AcquireMutex
(
    MrmJsInfra_Object,
    "AcquireMutex",
    C_MrmJsInfra_AcquireMutex,
    1,
    "Acquire Mutex"
);

C_(MrmJsInfra_ReleaseMutex);
static STest ReleaseMutex
(
    MrmJsInfra_Object,
    "ReleaseMutex",
    C_MrmJsInfra_ReleaseMutex,
    1,
    "Release Mutex"
);

C_(MrmJsInfra_SetRMTestName);
static STest SetRMTestName
(
    MrmJsInfra_Object,
    "SetRMTestName",
    C_MrmJsInfra_SetRMTestName,
    1,
    "SetRMTestName"
);

C_(MrmJsInfra_MrmTestInfraStartInitialize)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);
    rc = MrmTestInfra::StartInitializeTest();
    RETURN_RC(rc);
}

C_(MrmJsInfra_MrmTestInfraFinishInitialize)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);
    rc = MrmTestInfra::FinishInitializeTest();
    RETURN_RC(rc);
}

C_(MrmJsInfra_StartTest)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.StartTest(threadName, IsWorker)\n";
    string threadName;
    bool IsWorker;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 2) ||
    (OK != pJs->FromJsval(pArguments[0], &threadName)) ||
    (OK != pJs->FromJsval(pArguments[1], &IsWorker)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    rc = MrmTestInfra::StartTest(threadName, IsWorker);
    RETURN_RC(rc);
}

C_(MrmJsInfra_FinishTest)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.FinishTest()\n";
    string threadName;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &threadName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    rc = MrmTestInfra::FinishTest(threadName);
    RETURN_RC(rc);
}

C_(MrmJsInfra_FinishAllTests)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);
    rc = MrmTestInfra::FinishAllTests();
    RETURN_RC(rc);
}

C_(MrmJsInfra_SetTestLoops)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.SetTestLoops(number of loops needed)\n";
    UINT32 noOfLoops;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &noOfLoops)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    rc = MrmTestInfra::SetTestLoops(noOfLoops);
    RETURN_RC(rc);
}

C_(MrmJsInfra_CreateMutexes)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.CreateMutexes(number of mutexes needed)\n";
    UINT32 noOfMutexes;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &noOfMutexes)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    rc = MrmTestInfra::CreateMutexes(noOfMutexes);
    RETURN_RC(rc);
}

C_(MrmJsInfra_AcquireMutex)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.AcquireMutex(mutex name)\n";
    string mutexName;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &mutexName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    rc = MrmCppInfra::AcquireMutex(mutexName.c_str());
    RETURN_RC(rc);
}

C_(MrmJsInfra_ReleaseMutex)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.ReleaseMutex(mutex name)\n";
    string mutexName;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &mutexName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    rc = MrmCppInfra::ReleaseMutex(mutexName.c_str());
    RETURN_RC(rc);
}

C_(MrmJsInfra_SetRMTestName)
{
    RC rc = OK;
    const char   *usage = "Usage: MrmJsInfra.SetRMTestName(mutex name)\n";
    string rmTestName;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &rmTestName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    MrmCppInfra::SetRmTestName(rmTestName);
    RETURN_RC(rc);
}

