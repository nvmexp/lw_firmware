/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011,2014,2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ERRORMAP_H
#define INCLUDED_ERRORMAP_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#endif

#include "core/include/tasker.h"
#include "core/include/heartbeatmonitor.h"
#include <set>
#include <map>

/**
 * @class(ErrorMap)
 *
 * Map return codes to error code and help messages.
 *
 * Each return code is a unique number from 1 to 999.
 * The return codes and their corresponding help messages are defined
 * in error.in. Clients use rc.h to get access to the RC's.
 *
 * Each test will have a unique number assigned to it in JavaScript
 * via the ErrorMap.Test property.
 * The test designations will begin at 1000 and increment by 1000.
 *
 */

static constexpr INT32 s_UnknownTestId = -1;

class ErrorMap
{
public:
    ErrorMap();
    explicit ErrorMap(RC Code);
    ErrorMap& operator=(const ErrorMap& Src)
    {
        m_rc          = Src.Code()% RC::TEST_BASE;
        m_FailedTest  = Src.FailedTest();
        m_FailedPhase = Src.FailedPhase();
        return *this;
    };

    struct TestContext
    {
        explicit TestContext(INT32 testId) : TestId(testId) { }

        UINT64 Progress = 0;
        UINT64 ProgressMax = 0;
        UINT64 LastUpdateMs = 0;
        INT32  TestId = -1;
        HeartBeatMonitor::MonitorHandle HeartBeatRegistrationId = 0;
        bool IsTestProgressSupported = false;
    };

    bool          IsValid() const;
    INT32         Code() const;
    const char *  Message() const;
    string ToDecimalStr() const;

    static void InitTestProgress(UINT64 maxTestProgress, UINT64 heartBeatPeriodSec);
    static RC EndTestProgress();

    static HeartBeatMonitor::MonitorHandle HeartBeatRegistrationId();
    static void SetHeartBeatRegistrationId(HeartBeatMonitor::MonitorHandle lwrRegistrationId);
    static bool IsTestProgressSupported();

    static INT32  Test();
    static void   SetTest(INT32 lwrTestNumThisThread);

    static void   EnableTestProgress(bool enable);
    static bool   IsTestProgressEnabled();

    static UINT64 TestProgress();
    static void   SetTestProgress(UINT64 lwrTestProgressThisThread);

    static UINT64 TestProgressMax();
    static void   SetTestProgressMax(UINT64 lwrTestProgressMaxThisThread);

    static UINT64 TestProgressLastUpdateMs();
    static void   SetTestProgressLastUpdateMs(UINT64 lwrTestProgressLastUpdateThisThread);

    static UINT32 MinTestProgressIntervalMs();
    static void   SetMinTestProgressIntervalMs(UINT32 newInterval);

    static INT32  DevInst();
    static void   SetDevInst(INT32 devInst);

    static void*  PopTestContext();
    static void   RestoreTestContext(void* pTestContext);

    //! Get the test offset (global).
    static UINT32 TestOffset();
    static void   SetTestOffset(UINT32 testOffset);

    static UINT32 TestPhase();
    static void   SetTestPhase(UINT32 lwrTestPhaseThisThread);

    static void GetRunningTests(INT32 gpuid, set<INT32> *runningTests);

    static void PrintRcDesc(RC rc);

    UINT32 FailedTest() const { return m_FailedTest; }
    UINT32 FailedPhase() const { return m_FailedPhase; }

    void SetRC(RC rc);
    void Clear();

    class ScopedTestNum
    {
        public:
            explicit ScopedTestNum(INT32 testNum)
            : m_TestNum(ErrorMap::Test())
            {
                ErrorMap::SetTest(testNum);
            }
            ~ScopedTestNum()
            {
                ErrorMap::SetTest(m_TestNum);
            }

        private:
            INT32 m_TestNum;
    };

    class ScopedDevInst
    {
        public:
            explicit ScopedDevInst(INT32 devInst)
            : m_DevInst(ErrorMap::DevInst())
            {
                ErrorMap::SetDevInst(devInst);
            }
            ~ScopedDevInst()
            {
                ErrorMap::SetDevInst(m_DevInst);
            }

        private:
            INT32 m_DevInst;
    };

private:
    // We are using a nested class here because we want the mutex to be initialized and destroyed
    // only once. Having a static object of this nested class ensures that.
    class ErrorMapMutex
    {
    public:
        void* mutex;

        ErrorMapMutex()
        {
            // Initialize Running Tests mutex
            mutex = Tasker::AllocMutex("mutex", Tasker::mtxUnchecked);
        }

        ~ErrorMapMutex()
        {
            Tasker::FreeMutex(mutex);
            mutex = nullptr;
        }
    };

    static ErrorMapMutex s_RTMutex;

    ErrorMap(const ErrorMap&) = delete;

    RC m_rc;
    UINT32 m_FailedTest;
    UINT32 m_FailedPhase;

    static UINT32 s_MinTestProgressIntervalMs;

    static void AllocTlsIndex();

    static TestContext *GetTestContextPtr();

    // Mapping between GPU ID and set of running tests
    static map<INT32, set<INT32>> s_RunningTests;
};

#endif // ! INCLUDED_ERRORMAP_H
