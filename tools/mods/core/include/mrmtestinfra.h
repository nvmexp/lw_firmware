/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

enum TestState
{
     tsNotStarted
    ,tsConstructingTests
    ,tsInProgress
    ,tsSetupDone
    ,tsComplete
    ,tsNUM_TEST_STATES
};

class MrmTest
{
    private:
        string    name; //Name which indicates the type of thread holding this object
        TestState indTestProgress;//Test status
        bool      ImWorker;//Set to true is the thread holding this object has to keep running until other one finishes
    public:
        MrmTest(string, bool);
        void StartTest();//Allocates mutex and acquires it.
        void SetTestStatus(TestState);//Set the status
        bool amIWorker(){return ImWorker;}
        void SetImWorker(){ImWorker = true;}
        string GetName();
        TestState GetTestStatus();
};

typedef struct
{
    string name;
    void *mutex;
}MrmMutex;

struct MrmEvent
{
    string eventName;
    ModsEvent*  pModsEvent;
};

//MrmJsInfra JS class implementation
//It doesnt use any properties..
namespace MrmJsInfra
{
    RC MrmTestInfraStartInitialize(void);
    RC MrmTestInfraFinishInitialize(void);
    RC StartTest(string, bool);
    RC FinishAllTests();
    RC FinishTest(string);
    RC CreateMutexes(UINT32);
    RC AcquireMutex(string);
    RC ReleaseMutex(string);
    RC SetRMTestName(string);
}

//Functions exposed to Rmtest and Mdiag CPP code..
class MrmCppInfra
{

public:
    static bool TestsRunningOnMRMI();
    static UINT32 GetTestLoopsArg();
    static RC SetupDoneWaitingForOthers(string);
    static RC ShouldContinueTest(string, bool *);
    static RC TestDoneWaitingForOthers(string);
    static RC AcquireMutex(const char *);
    static RC ReleaseMutex(const char *);

    static RC AllocEvent(string eventName);
    static RC FreeEvent(string eventName);
    static void FreeAllEvents(void);
    static RC SetEvent(string eventName);
    static RC ResetEvent(string eventName);

    static RC WaitOnEvent
    (
        string eventName,
        FLOAT64     timeoutMs = Tasker::NO_TIMEOUT
    );

    static bool IsEventSet(string eventName);

    static void SetRmTestName(string);
    static string GetRmTestName(void);
};
