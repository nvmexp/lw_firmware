/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  errloggr.h
 * @brief Error Logger - originally designed for "expected" HW errors from tests
 *        designed to cause them
 */

#ifndef INCLUDED_ERROR_LOGGER_H
#define INCLUDED_ERROR_LOGGER_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_TASKER_H
#include "core/include/tasker.h"
#endif

#ifndef INCLUDED_TEEBUF_H
#include "core/include/tee.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

namespace ErrorLogger
{
    enum { ILWALID_GRIDX = 0xFFFFFFFF };

    enum CheckGrIdxMode
    {
        // Do not match test's and interrupt's GrIdx
        None,               
        // Match test's and interrupt's GrIdx
        IdxMatch,              
        // Match test's and interrupt's GrIdx and also match untagged interrupts
        IdxAndUntaggedMatch     
    };

    enum CompareMode
    {
        Exact,
        OneToOne
    };

    enum LogType
    {
        LT_ERROR = 0xFE,
        LT_INFO  = 0xFF
    };

    // Returns true if a true miscompare oclwrred between lwrErr and expectedErr
    typedef bool (*ErrorFilterProc)(UINT64 intr, const char *lwrErr, const char *expectedErr);
    // Return true to log this error string, false to ignore it
    typedef bool (*ErrorLogFilterProc)(const char *errStr);

    RC Initialize();
    RC Shutdown();

    RC StartingTest();
    void LogError(const char *errStr, LogType errType);
    void FlushError();
    RC FlushLogBuffer();

    // Allow the test to filter miscompares...this is needed for legacy
    // support, but it could also prove useful in the future.
    void InstallErrorFilter(ErrorFilterProc proc);
    void InstallErrorLogFilter(ErrorLogFilterProc proc);
    void InstallErrorLogFilterForThisTest(ErrorLogFilterProc proc);
    bool ErrorCanBeFiltered(const char *errStr);

    void AddEvent(ModsEvent *pEvent);
    void RemoveEvent(ModsEvent *pEvent);

    // Test needs to be able to tell ErrorLogger if it ran into trouble and
    // can't handle the errors (write to file or compare) properly...for example
    // if the expected errors file is missing.
    void TestUnableToHandleErrors();

    void IgnoreErrorsForThisTest();

    bool HasErrors(CheckGrIdxMode checkGrIdx = None, UINT32 testGrIdx = ILWALID_GRIDX);
    RC TestCompleted();

    RC WriteFile(const char *fileName, CheckGrIdxMode checkGrIdx = None, UINT32 testGrIdx = ILWALID_GRIDX);
    RC CompareErrorsWithFile(const char *fileName, CompareMode comp, CheckGrIdxMode checkGrIdx = None, UINT32 testGrIdx = ILWALID_GRIDX);

    void PrintErrors(Tee::Priority pri);

    //! Returns number of unhandled LogError calls since mods started.
    UINT32 UnhandledErrorCount();

    UINT32 GetErrorCount();
    string GetErrorString(UINT32 index);
    bool GrIdxMatches(CheckGrIdxMode checkGrIdx, UINT32 testGrIdx, UINT32 errorGrIdx);
};

#endif // INCLUDED_ERROR_LOGGER_H

