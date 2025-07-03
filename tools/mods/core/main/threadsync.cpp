/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/cmdline.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "core/include/threadsync.h"

#define SP_ENABLED                 0
#define SP_DISABLED                1
#define THREAD_SYNC_ENABLED        1
#define THREAD_SYNC_DISABLED       1
#define TEST_RUNNING_IN_CONLWRRENT 1

// Map to hold disabled sync points and test names
static map<string, UINT32> disSyncPointsMap;
static map<string, UINT32> conlwrrentTestsMap;

// ThreadSync state
static UINT32 threadSyncStatus;

/**
 * ThreadSync namespace methods
 */

RC ThreadSync::PrintStats()
{
    map<string, UINT32>::iterator it;

    Printf(Tee::PriHigh,"THREADSYNC: Stats\n");
    Printf(Tee::PriHigh,"**** Tests Running ****\n");
    for (it = conlwrrentTestsMap.begin(); it != conlwrrentTestsMap.end(); it++)
    {
        Printf(Tee::PriHigh, "%s  -  %d\n",(it->first).c_str(), it->second);
    }
    Printf(Tee::PriHigh,"**** Disabled syncpoints ****\n");
    for (it = disSyncPointsMap.begin(); it != disSyncPointsMap.end(); it++)
    {
        Printf(Tee::PriHigh, "%s  -  %d\n",(it->first).c_str(), it->second);
    }
    Printf(Tee::PriHigh,"*******\n");
    return OK;
}

RC ThreadSync::DisableSyncPoint (string syncPoint)
{
    Printf(Tee::PriHigh, "THREADSYNC: Disabling Sync Point \"%s\" \n",syncPoint.c_str());
    disSyncPointsMap.insert(pair<string, UINT32>(syncPoint, SP_DISABLED));
    return OK;
}

RC ThreadSync::WaitOnSyncPoint (string syncPoint)
{
    UINT64 startTime = 0;
    UINT64 endTime   = 0;

    startTime = Xp::GetWallTimeUS();
    Printf(Tee::PriHigh, "THREADSYNC: Time entering sync point \"%s\" = %llu. Syncpoint status %d\n",
                                          syncPoint.c_str(), startTime, disSyncPointsMap[syncPoint]);

    if ((threadSyncStatus == THREAD_SYNC_ENABLED) &&
        (disSyncPointsMap[syncPoint] != SP_DISABLED))
    {
        Tasker::WaitOnBarrier();
        endTime = Xp::GetWallTimeUS();
        Printf(Tee::PriHigh, "THREADSYNC: Total waiting time on Sync point \"%s\" = %llu\n", syncPoint.c_str(),
                                                                                         endTime - startTime);
    }
    else
    {
        Printf(Tee::PriHigh, "THREADSYNC: Sync point \"%s\" disabled. Continuing..\n", syncPoint.c_str());
    }
    return OK;
}

MODS_BOOL ThreadSync::IsSyncPointEnabled (string syncPoint)
{
    if (disSyncPointsMap[syncPoint] != SP_DISABLED)
        return MODS_TRUE;

    return MODS_FALSE;
}

MODS_BOOL ThreadSync::IsConlwrrencyEnabled()
{
    return (threadSyncStatus == THREAD_SYNC_ENABLED)?MODS_TRUE:MODS_FALSE;
}

MODS_BOOL ThreadSync::IsConlwrrentTestRunning(string pTestName)
{
    map<string, UINT32>::iterator it = conlwrrentTestsMap.find(pTestName);
    if (it != conlwrrentTestsMap.end() && it->second == TEST_RUNNING_IN_CONLWRRENT)
        return MODS_TRUE;

    return MODS_FALSE;
}

// Returns OK even if the test is not found..
RC ThreadSync::TestCompleted(string testName)
{
    if (conlwrrentTestsMap.count(testName))
        conlwrrentTestsMap.erase(testName);
    return OK;
}

// Functions exposed to Javascript Code
JS_CLASS(JsThreadSync);

static SObject JsThreadSync_Object
(
    "JsThreadSync",
    JsThreadSyncClass,
    0,
    0,
    "To run multiple rmtests conlwrrently"
);

//Methods

C_(JsThreadSync_DisableSyncPoint);
static STest DisableSyncPoint
(
    JsThreadSync_Object,
    "DisableSyncPoint",
    C_JsThreadSync_DisableSyncPoint,
    1,
    "Disbles a particular Sync point"
);

C_(JsThreadSync_SetConlwrrentTestRunning);
static STest SetConlwrrentTestRunning
(
    JsThreadSync_Object,
    "SetConlwrrentTestRunning",
    C_JsThreadSync_SetConlwrrentTestRunning,
    1,
    "Set running test"
);

C_(JsThreadSync_EnableThreadSync);
static STest EnableThreadSync
(
    JsThreadSync_Object,
    "EnableThreadSync",
    C_JsThreadSync_EnableThreadSync,
    0,
    "Enable Thread Sync"
);

C_(JsThreadSync_DisableSyncPoint)
{
    RC rc = OK;
    const char   *usage = "Usage: JsThreadSync.DisableSyncPoint(syncName)\n";
    string syncName;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &syncName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    ThreadSync::DisableSyncPoint(syncName);
    RETURN_RC(rc);
}

C_(JsThreadSync_SetConlwrrentTestRunning)
{
    RC rc = OK;
    const char   *usage = "Usage: JsThreadSync.SetConlwrrentTestRunning(testName)\n";
    string testName;
    JavaScriptPtr pJs;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if ((NumArguments != 1) ||
    (OK != pJs->FromJsval(pArguments[0], &testName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    Printf(Tee::PriHigh,"THREADSYNC: Test \"%s\" in conlwrrent exelwtion\n", testName.c_str());
    conlwrrentTestsMap.insert(pair<string , UINT32>(testName, TEST_RUNNING_IN_CONLWRRENT));
    RETURN_RC(rc);
}

C_(JsThreadSync_EnableThreadSync)
{
    RC rc = OK;

    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);

    threadSyncStatus = THREAD_SYNC_ENABLED;
    Printf(Tee::PriHigh,"THREADSYNC: Enabling ThreadSync\n");

    RETURN_RC(rc);
}
