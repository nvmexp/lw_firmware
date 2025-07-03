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

/**
 * @file  threadsync.h
 * @brief Synchronize multiple gpu tests thread using sync points and tasker
 */

namespace ThreadSync
{
    // Methods
    RC        DisableSyncPoint(string);
    RC        WaitOnSyncPoint(string);
    RC        PrintStats();
    RC        PrintConlwrrencyMap();
    RC        TestCompleted(string);
    MODS_BOOL IsConlwrrencyEnabled();
    MODS_BOOL IsConlwrrentTestRunning(string);
    MODS_BOOL IsSyncPointEnabled(string);
};

/**
 * Functions exposed to JavaScript Code
 */
namespace JsThreadSync
{
    RC DisableSyncPoint(string);
    RC SetConlwrrentTestRunning(string);
    RC EnableThreadSync();
};

/**
 * Defines
 */
#define SYNC_POINT(spName)            ThreadSync::WaitOnSyncPoint(spName)
#define IS_SYNC_POINT_ENABLED(spName) ThreadSync::IsSyncPointEnabled(spName)
#define DISABLE_SYNC_POINT(spName)    ThreadSync::DisableSyncPoint(spName)
#define IS_CONLWRRENT()               ThreadSync::IsConlwrrencyEnabled()
#define IS_TEST_RUNNING(testName)     ThreadSync::IsConlwrrentTestRunning(testName)
#define TEST_COMPLETE(testName)       ThreadSync::TestCompleted(testName)
