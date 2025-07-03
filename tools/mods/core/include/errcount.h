/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ERRCOUNT_H
#define INCLUDED_ERRCOUNT_H

#ifndef INCLUDED_LOG_H
#include "log.h"
#endif

#ifndef INCLUDED_TASKER_H
#include "tasker.h"
#endif

#ifndef INCLUDED_TEE_H
#include "tee.h"
#endif

#ifndef INCLUDED_ERROR_LOGGER_H
#include "core/utility/errloggr.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

struct JSObject;
struct JSStackFrame;
class ModsTest;

//--------------------------------------------------------------------
//! \brief Infrastructure for causing mods errors on an error counter.
//!
//! This class provides an infrastructure for periodically checking an
//! error counter(s), and making mods fail if the counter(s) gets
//! incremented.
//!
//! This class can check for errors on every JavaScript function, or
//! on every ModsTest.  In the JavaScript function case, it reads the
//! counter(s) when the function begins and ends, and returns an error
//! if the counter(s) incremented during the function.  Similarly, in
//! the ModsTest case, it returns an error if the counter(s)
//! incremented during the ModsTest.
//!
//! The usage model for checking a new error counter(s) is:
//!
//! - Create a subclass of ErrCounter.
//! - Override ReadErrorCount() in the subclass, to read the count(s)
//!   you want to check.
//! - Optionally override PrintErrorMessage() and/or
//!   PrintErrorCount(), to print a message when an error oclwrs
//!   and/or a periodic message with the Verbose flag, respectively.
//! - Create a instance of the subclass, and call Initialize() to
//!   start monitoring the counter(s), and ShutDown() to stop.
//!
//! Optionally, this class can be controlled by some JavaScript
//! variables, if a name "Xxx" is passed to Initialize():
//!
//! - g_XxxVerboseFlags: Flags that determine when additional data
//!   is printed about current counter status.  Defaults to 0.
//!       - VF_ON_CP : if set, print the error counter every time
//!                    it's checked.
//!       - VF_ON_COUNT_CHANGE : if set, print the error counter
//!                              when checked if the counter has changed
//!       - VF_EPM   : if set, print elapsed time in ms and errors per minute
//!
//! - g_XxxAllowed: Number of allowed errors per JavaScript function
//!   or ModsTest.  Defaults to 0.
//! - g_XxxAllowedTotal: Number of allowed errors since mods started.
//!   Defaults to 0.
//! - g_XxxDisabled: If true, disables the error check.  Defaults to
//!   false.
//! - (test).XxxVerboseFlags:  Overrides g_XxxVerboseFlags for
//!   one ModsTest.
//! - (test).XxxAllowed: Overrides g_XxxAllowed for one ModsTest.
//! - (test).Disabled: Overrides g_XxxDisabled for one ModsTest.
//!
//! Note that a subclass can monitor an array of counters.  This
//! feature is for cases in which several counters are all returned
//! from a single RmControl call.  The XxxAllowed JavaScript
//! variables can be arrays (one per counter) or scalars (applied to
//! all counters).
//!
//! If multiple nested tests/functions are running in parallel
//! threads, this class keeps track of the errors on a per-thread
//! basis, and assigns the error to the "innermost" test/function in
//! each thread.
//!
class ErrCounter
{
public:
    enum FrameType
    {
        MODS_TEST,   //!< Check for an error every time a ModsTest is called
        JS_FUNCTION  //!< Check for an error every time a JS function is called
    };

    enum VerboseFlags
    {
        VF_ON_CP           = (1 << 0),
        VF_ON_COUNT_CHANGE = (1 << 1),
        VF_EPM             = (1 << 2)
    };

    // Priority determines order to check ErrCounters for error.  The
    // first pri in this list gets checked first, so if two errors
    // occur at the same time, the first one gets precedence.
    //
    enum Priority
    {
        ECC_PRI,
        MASSERT_PRI,
        UNEXPECTED_INTERRUPT_PRI,
        TEX_PRI,
        //
        // From here up, the RC returned by ErrCounter should override the RC returned by the test
        //
        OVERRIDE_TEST_RC_PRI,
        EDC_PRI,
        OVERTEMP_PRI,
        POWER_READING_PRI,
        POWER_RAIL_LEAK_PRI,
        GPIO_ACTIVITY_PRI,
        CLOCKS_CHANGE_PRI,
        PSTATE_CHANGE_PRI,
        PRINTQ_OVERFLOW_PRI,
    };

    ErrCounter();
    virtual ~ErrCounter();

    RC Initialize(const string &Name, UINT32 NumErrCounts,
                  UINT32 NumHiddenCounts, vector<UINT32> *pEPMCntInds,
                  FrameType Coverage, Priority aPriority);
    RC ShutDown(bool CheckForErrorsOnShutdown);
    bool IsInitialized() const;
    bool IsDisabled() { InitFromJs(); return m_OuterStackFrame.Disabled; }
    void *GetMutex() const { return m_Mutex; }
    virtual bool WatchInStackFrame(ModsTest *pTest) const;
    bool operator<(const ErrCounter &rhs) const;

    static RC EnterStackFrame(ModsTest *pTest, void *pFrameId, bool SaveRc);
    static RC ExitStackFrame(ModsTest *pTest, void *pFrameId,
                             bool *pOverrideTestRc);
    static RC CheckExitStackFrame(ModsTest *pTest, void *pFrameId);
    virtual RC CheckCounterExitStackFrame(ModsTest *pTest, void *pFrameId);
    static RC CheckEPMAllCounters(ModsTest *pTest, void *pFrameId, bool *pOverrideTestRc);
    static void PrintJsProperties(Tee::Priority Pri, ModsTest *pTest);
    virtual string DumpStats() {return string();}

protected:
    virtual RC ReadErrorCount(UINT64 *pCount) = 0;

    struct ErrorData
    {
        const UINT64 *pNewErrors;     //!< Num new errors, not counting ones
                                      //!< already reported
        const UINT64 *pAllowedErrors; //!< Num errors allowed (threshold)
        const UINT64 *pNewCount;      //!< Total err count
        const UINT64 *pOldCount;      //!< Err count last time we reported
        bool bCheckExitOnly;
        bool Verbose;
    };
    virtual RC OnError(const ErrorData *pData) = 0;

    struct CountChangeData
    {
        const UINT64 *pNewCount;      //!< Total error count
        const UINT64 *pOldCount;      //!< Previous error count
        bool          Verbose;
    };
    virtual void OnCountChange(const CountChangeData *pData);

    struct CheckpointData
    {
        const UINT64 *pCount;         //!< Total error count
        const UINT64 *pAllowedErrors; //!< Num errors allowed (threshold)
        bool Disabled;                //!< If true, error checks are disabled
    };
    virtual void OnCheckpoint(const CheckpointData *pData) const;

    vector<UINT64> GetLwrrentCount() { return m_LwrrentCount; }
    vector<UINT64> GetAllowedErrors() { return m_OuterStackFrame.AllowedErrors; }
    // Do some reset before entering a MODS test
    // Can be overrided by sub-classes to decide what they want to reset
    virtual RC RunBeforeEnterTest();
    virtual void InitFromJs();
    static bool OverrideFromJs(JSObject *pJSObject, string PropertyName,
                               bool *pBool);
    static bool OverrideFromJs(JSObject *pJSObject, string PropertyName,
                               UINT32 *pUint32);
    static bool OverrideFromJs(JSObject *pJSObject, string PropertyName,
                               vector<UINT64> *pArray);

    struct PerMinErrorData
    {
        const UINT64 *pLwrrEPM;
        const UINT64 *pAllowedEPM;
        const UINT64  elapsedTimeMS;
    };

private:
    //! Used to keep track of nested function calls in a thread.  If
    //! an error oclwrs, it's assigned to the innermost function
    struct StackFrame
    {
        void *pFrameId; //!< Opaque pointer to match the function entry/exit
        RC rc;           //!< Stores the error when a JS function enters
        UINT32 VerboseFlags;    //!< Verbosity flags for printing the counter
        vector<UINT64> AllowedErrors; //!< Number of allowed errors
        vector<UINT64> AllowedErrorsPerMin; //!< Number of errors per min allowed
        bool Disabled;   //!< If true, error checks are disabled
        vector<UINT64> EnterCount; //!< Error count when the frame was entered
        UINT64 EnterTimeMs; //!< Frame enter time in ms
        vector<UINT64> LastError;  //!< Count when last error was reported for
                                   //!< this frame
        vector<UINT64> InnerErrors; //!< Number of counts that were reported
                                    //!< as errors by inner frames after
                                    //!< LastError
    };
    typedef vector<StackFrame> Stack;

    void Clear();
    RC DoEnterStackFrame(ModsTest *pTest, void *pFrameId, bool SaveRc);
    RC DoExitStackFrame(ModsTest *pTest, void *pFrameId, bool bCheckExitOnly);
    void DoPrintJsProperties(Tee::Priority Pri, ModsTest *pTest);
    RC UpdateLwrrentCount(bool bVerbose);
    RC CheckForError(Stack *pStack, ModsTest *pTest, bool bCheckExitOnly);
    void PopStack(Stack *pStack);
    static void GetInitializedErrCounters(ModsTest *pTest,
                                          vector<ErrCounter*> *pErrCounters);
    static UINT32 GetStackDepth(FrameType Coverage);
    virtual void UpdateJsonItem(ModsTest* pTest, ErrorData * pData);
    RC CheckEPM(void *pFrameId);
    StackFrame* GetStackFrame(void *pFrameId);
    virtual void ReportLwrrEPM(PerMinErrorData *pErrData,
                               UINT64 elapsedTimeMS,
                               bool tolExceeded)
    { }
    virtual void OverwriteAllowedErrors(UINT64 *pAllowedErrors, size_t bufSize)
    { }
    virtual void OverwriteAllowedEPM(UINT64 *pAllowedEPM, size_t bufSize)
    { }

    void *m_Mutex;         //!< Mutex to protect ErrCounter objects
    string m_Name;         //!< "Xxx" name used to generate JS variables
    UINT32 m_NumErrCounts; //!< Number of error counts in the arrays
    UINT32 m_NumAllCounts; //!< Total of error and aux counts in the arrays
    FrameType m_Coverage;  //!< Tells when to check for errors
    Priority m_Priority;   //!< Tells order to check ErrCounters for errors

    vector<UINT64> m_AllowedErrorsPerStackFrame;
                           //!< Num. errors allowed per JS function or ModsTest
    vector<UINT64> m_AllowedErrorsPerMinPerStackFrame;
                           //!< Num. errors per minute allowed per ModsTest
    bool m_InitializedFromJs; //!< Tells whether we read the "g_" JS vars yet

    map<Tasker::ThreadID, Stack> m_Stacks;
                                    //!< All stacks with pending functions,
                                    //!< indexed by ThreadID
    StackFrame m_OuterStackFrame;   //!< Phony stack frame treated as the
                                    //!< outermost frame of all stacks.  Used
                                    //!< report errors in "all of mods" that
                                    //!< weren't reported by a real frame.
    bool       m_bIgnoreOuterFrameErrors; //!< Ignore any errors that occur on
                                          //!< the outer stack frame
    vector<UINT64> m_LwrrentCount; //!< Most recent counts read by subclass
    vector<UINT32> m_EPMCountIndices; //!< Indices of err counts used to track EPM
    UINT32 m_ExpectedExitsSansEnter;//!< Num. ExitStackFrame() calls we expect
                                    //!< to get without a corresponding
                                    //!< EnterStackFrame()

    static set<ErrCounter*> *s_pInitializedCounters;
                                    //!< All ErrCounter objects on which
                                    //!< Initialize() was called
    static void *s_InitializedCountersMutex;
                                    //!< Mutex to prevent changing
                                    //!< s_pInitializedCounters
    static UINT32 s_NumErrCounters; //!< Used to initialize
                                    //!< s_pInitializeCounters
    static bool s_RespectTestRc;    //!< Stops the error counters
                                    //!< from overriding a test RC
};

//--------------------------------------------------------------------
//! \brief ErrCounter subclass that throws an RC when an MASSERT happens.
//!
//! This class is instantiated in ModsMain, to catch MASSERTs in
//! elwironments where the MASSERT-breakpoint mechanism doesn't work.
//! But it's also a nice example of a simple ErrCounter subclass.
//!
class MassertErrCounter : public ErrCounter
{
protected:
    virtual RC ReadErrorCount(UINT64 *pCount)
    {
        pCount[0] = Log::GetLastAssertCount();
        return OK;
    }
    virtual RC OnError(const ErrorData *pData)
    {
       return Log::GetLastAssertCode();
    }
};

//--------------------------------------------------------------------
//! \brief ErrCounter subclass for unexpected HW interrupts.
//!
//! This class is instantiated in ModsMain.
//!
class UnexpectedInterruptErrCounter : public ErrCounter
{
protected:
    virtual RC ReadErrorCount(UINT64 *pCount)
    {
        pCount[0] = static_cast<UINT64>(ErrorLogger::UnhandledErrorCount());
        return OK;
    }

    virtual RC OnError(const ErrorData *pData)
    {
        return RC::UNEXPECTED_INTERRUPTS;
    }

    virtual void OnCountChange(const CountChangeData *pData)
    {
        Printf (Tee::PriHigh,
            "New unexpected HW interrupts have oclwrred, count = %llu (%llu new)\n",
            pData->pNewCount[0],
            pData->pNewCount[0] - pData->pOldCount[0]);
    }
};

#endif // INCLUDED_ERRCOUNT_H
