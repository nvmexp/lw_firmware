/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file errloggr.cpp
 * @brief Error Logger - originally designed for "expected" HW errors from tests
 *        designed to cause them
 */

#include "errloggr.h"
#include "core/include/fileholder.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/evntthrd.h"
#include "core/include/xp.h"
#include <string>
#include <vector>
#include <map>
#include <list>
#include <string.h>
#include "core/include/platform.h"
#include <queue>
#include <memory>
#include <cctype>

// "Private" members of ErrorLogger namespace

namespace ErrorLogger
{
    static const UINT32 BUFFER_SIZE = 2000;

    struct ErrLogEntry
    {
        LogType  Type;
        UINT32   GrIdx;
        string   ErrString;
    };
    vector<ErrLogEntry> d_LwrrentErrorLogEntries;

    ErrorFilterProc d_ErrorFilter = NULL;
    vector<ErrorLogFilterProc> d_ErrorLogFilters;

    list<ModsEvent*> d_ErrorEvents;

    static const int UNSPECIFIED = -1;
    class Range
    {
        int m_Min;
        int m_Max;
        int m_ErrorCount;
    public:
        Range() : m_Min(UNSPECIFIED), m_Max(UNSPECIFIED), m_ErrorCount(0) {}
        Range(int min, int max)
                : m_Min(min), m_Max(max), m_ErrorCount(0) {}
        void SetMin(int min) { this->m_Min = min; }
        void SetMax(int max) { this->m_Max = max; }
        int GetMax() const { return  m_Max; }
        int GetMin() const { return  m_Min; }
        string GetRangeStr() const
        {
            string minStr = (m_Min == UNSPECIFIED) ? "-" : Utility::StrPrintf("%d", m_Min);
            string maxStr = (m_Max == UNSPECIFIED) ? "-" : Utility::StrPrintf("%d", m_Max);
            return Utility::StrPrintf("[%s,%s]", minStr.c_str(), maxStr.c_str());
        }
        void IncErrorCount() { ++m_ErrorCount; }
        bool CheckMin() const { return !(m_Min != UNSPECIFIED && m_ErrorCount < m_Min); }
        bool CheckMax() const { return !(m_Max != UNSPECIFIED && m_ErrorCount > m_Max); }
        bool IsAtMax() const { return (m_Max != UNSPECIFIED && m_ErrorCount == m_Max); }
        void Merge(const Range &other)
        {
            if (m_Min != UNSPECIFIED)
            {
                m_Min = (other.GetMin() == UNSPECIFIED) ? m_Min : m_Min + other.GetMin();
            }
            if (m_Max != UNSPECIFIED)
            {
                m_Max = (other.GetMax() == UNSPECIFIED) ? UNSPECIFIED : m_Max + other.GetMax();
            }
        }
    };

    class IRangeChecker
    {
    public:
        IRangeChecker() : m_Min(0), m_Max(0) {}

        // A template method that logs an expected error line for later comparision.
        // The behavior of AddRange() is determined in subclass.
        RC LogString(const string &errLine)
        {
            string error;
            Range range;
            if (ExtractErrAndRangeFromLine(errLine, &error, &range))
            {
                AddRange(error, range);
                return OK;
            }
            else
            {
                return RC::GOLDEN_VALUE_MISCOMPARE;
            }
        }

        // A template method that compares the errors in errorLogEntries with already
        // logged expected errors. Return OK if the sequece of errors happened matches
        // that speciefied in INT file, by the comparing strategy which varies in different
        // subclasses. CheckPerformance(), CheckString() and EndCheck() is determined
        // in subclass.
        RC CheckErrors(const vector<ErrLogEntry> &errorLogEntries, CheckGrIdxMode checkGrIdx, UINT32 testGrIdx)
        {
            RC rc;
            if (m_Min == 0)
            {
                Printf(Tee::PriHigh, "Illegal interrupt file!"
                    " Min oclwrances of total expected interrupts should not be 0\n");
                return RC::GOLDEN_VALUE_MISCOMPARE;
            }
            CheckPerformance(errorLogEntries.size());
            for(size_t i = 0; i < errorLogEntries.size(); ++i)
            {
                // Check only those errors whose GrIdx match
                if (GrIdxMatches(checkGrIdx, testGrIdx, errorLogEntries[i].GrIdx))
                {
                    if (OK != (rc = CheckString(i, errorLogEntries[i].ErrString)))
                    {
                        break;
                    }
                }
            }
            if (rc == OK)
            {
                rc = EndCheck();
            }
            return rc;
        }

        string GetExpectedErrorRange()
        {
            string minStr = (m_Min == UNSPECIFIED) ? "" : Utility::StrPrintf(">=%d", m_Min);
            string maxStr = (m_Max == UNSPECIFIED) ? "" : Utility::StrPrintf("<=%d", m_Max);
            return Utility::StrPrintf("%s %s", minStr.c_str(), maxStr.c_str());
        }

        virtual ~IRangeChecker() {};

    private:
        static bool ExtractErrAndRangeFromLine(const string &errLine, string* errString,
                                            Range* errRange);

        // Add an expected error with a range to m_ExpectedErrors defined in subclass. The type of
        // m_ExpectedErrors used in OrderedRangeChecker is queue, while that in UnOrderedRangeChecker
        // is map.
        virtual void AddRange(const string &error, const Range &range) = 0;

        // Compare a error with the logged expected errors. The comparison strategy is specified
        // in subclass: OrderedRangeChecker tries to match the front of the queue to ensure ordered
        // check, while UnOrderedRangeChecker always does a global match throughout the map.
        virtual RC CheckString(size_t index, const string &errLine) = 0;

        // After comparing all errors, check whether oclwrances of all the expected errors are within
        // the corresponding ranges specied in INT file.
        virtual RC EndCheck() = 0;

        virtual void CheckPerformance(size_t totalErrorNum) = 0;

    protected:
        int m_Min; // min value of the total oclwrances of all expected errors
        int m_Max; // max value of the total oclwrances of all expected errors
    };

    class OrderedRangeChecker : public IRangeChecker
    {
    public:
        typedef queue< pair<string, Range> > ErrorArray;

    private:
        void AddRange(const string &error, const Range &range);
        RC CheckString(size_t index ,const string &errLine);
        RC EndCheck();
        void CheckPerformance(size_t totalErrorNum) { /* Not needed in OrderedRangeChecker */ }
        bool CheckFront();

        ErrorArray m_ExpectedErrors; // where the expected errors parsed from INT file store
    };

    class UnorderedRangeChecker : public IRangeChecker
    {
    public:
        typedef map<string, Range> ErrorArray;

    private:
        void AddRange(const string &error, const Range &range);
        RC CheckString(size_t index, const string &errLine);
        RC EndCheck();
        void CheckPerformance(size_t totalErrorNum);

        ErrorArray m_ExpectedErrors; // where the expected errors parsed from INT file store
    };

    IRangeChecker* InstanceRangeChecker(CompareMode comp);

    vector<ErrorLogFilterProc> d_ErrorLogFiltersForThisTest;

     // Make sure that tests are aware of any errors that were logged
    bool d_HandledErrors = false;
    UINT32 d_TestsInCheckingCount = 0;

    UINT32 d_NumUnexpectedErrors = 0;

    //! EventThread used for copying data out of the static interrupt safe
    //! buffer into local storage for the error logger
    EventThread d_EventThread(Tasker::MIN_STACK_SIZE, "ErrorLogEventThread");

    char * d_LogBuffer = NULL;             //!< Interrupt safe log buffer
    char * d_LogBufferHead = NULL;         //!< Head (read position) in the log buffer
    char * d_LogBufferTail = NULL;         //!< Tail (write position) in the log buffer
    char * d_LogBufferLwrrent = NULL;      //!< End of next entry being appended to tail
    char * d_LogBufferLastGrIdx = nullptr; //!< Pointer to last GrIdx
    bool   d_AddingLogBufferEntry = false; //!< true when adding a log entry to the buffer
    bool   d_ReentrantLogError = false;    //!< true if modifying a log buffer entry
                                           //!< was reentrant (this will cause a loss of
                                           //!< the reentrant error)
    bool   d_LogBufferOverflow = false;    //!< true if the interrupt safe log buffer
                                           //!< overflowed due to either an error storm
                                           //!< or the event thread not being yielded to
                                           //!< frequently enough
    bool   d_UnterminatedError = false;    //!< true if the log ended with an unterminated
                                           //!< error
    UINT32 d_MaxErrorLogUsed = 0;          //!< Keep stats on peak usage of the interrupt
                                           //!< safe log buffer

    // Works like FlushError(), but sets d_UnterminatedError if an
    // unterminated error got flushed.
    void FlushUnterminatedError();

    // Internal implementation of LogError()
    void   LogErrorInternal(const char *errStr, LogType errType);

    // Event thread function used to empty the interrupt safe log
    void   ErrorLogEventFunc();

    // Poll function used to wait for the log to empty
    bool   PollLogBufferEmpty(void *pvArgs);

    // Add a new entry to the log
    void AddNewLogEntry(const ErrLogEntry &newLogEntry);

    // Copy data to/from d_LogBuffer
    char *CopyToLogBuffer(char *pDst, const char *pSrc, size_t Size);
    void CopyFromLogBuffer(char *pDst, const char *pSrc, size_t Size);

    static const UINT32 MAX_UNEXPECTED_ERRORS = 1000;
}

//! Size of the interrupt safe log.  At least one compute test was identified
//! that utilizes 57k of memory without yielding so that the emptying thread
//! can run
#define LOG_SIZE            (64*1024)

//! Macro used to increment a pointer into the log buffer with wrapping
#define INC_LOG_BUFFER_PTR(ptr) {               \
        ptr++;                                  \
        if (ptr == (d_LogBuffer + LOG_SIZE))    \
            ptr = d_LogBuffer;                  \
    }

//! Macro used to increment a pointer  by size into the log buffer with wrapping
#define INC_BY_SIZE_LOG_BUFFER_PTR(ptr, size) {                                         \
        ptr = (((char*)(ptr) + size - (char*)(d_LogBuffer)) % LOG_SIZE) + d_LogBuffer;  \
    }

//! Return the number of bytes between endPtr and startPtr in the log buffer
#define LOG_BUFFER_DIFF(endPtr, startPtr)                               \
    ((UINT32)((LOG_SIZE + (char*)(endPtr) - (char*)(startPtr)) % LOG_SIZE))

//! Return the amount of free space in the log buffer.
//! (Head == Current) implies that the log is empty.
//! ((Head - 1) == Current) implies that the log is full.
#define LOG_BUFFER_REMAIN()                                             \
    (LOG_SIZE - 1 - LOG_BUFFER_DIFF(d_LogBufferLwrrent, d_LogBufferHead))

//! Time to wait for the log buffer to be empty
#define EMPTY_LOG_BUFFER_TIMEOUT 10000

//! Call this macro at the start of functions that call
//! LogErrorInternal(), to prevent it from being reentrant.  We cannot
//! use a mutex to ensure this because LogErrorInternal() can be
//! called from an interrupt context
#define BEGIN_LOG_ERROR()                       \
    do                                          \
    {                                           \
        if (d_AddingLogBufferEntry)             \
        {                                       \
            d_ReentrantLogError = true;         \
            return;                             \
        }                                       \
        d_AddingLogBufferEntry = true;          \
    } while (false)

//! Call this macro at the end of functions that call LogErrorInternal()
#define END_LOG_ERROR()                         \
    d_AddingLogBufferEntry = false

//------------------------------------------------------------------------------
//! \brief Check if the test's GrIdx matches to the error's GrIdx
//!
//! \param checkGrIdx : GrIdx check mode
//! \param testGrIdx : Test's GrIdx
//! \param errorGrIdx : Error's GrIdx
//!
//! \return true if checkGrIdx is false or GrIdxs match, false otherwise
bool ErrorLogger::GrIdxMatches(CheckGrIdxMode checkGrIdx, UINT32 testGrIdx, UINT32 errorGrIdx)
{
    if (checkGrIdx == None) // If GrIdx matching is not needed return true always
    {
        return true;
    }
    else if (checkGrIdx == IdxMatch)
    {
        if (testGrIdx == errorGrIdx)
        {
            return true;
        }
    }
    else if (checkGrIdx == IdxAndUntaggedMatch)
    {
        if ((testGrIdx == errorGrIdx) ||
            (errorGrIdx == ILWALID_GRIDX))
        {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
//! \brief Poll function for determining when the interrupt safe buffer is empty
//!
//! \param pvArgs : Args for the polling function
//!
//! \return true if the buffer is empty, false otherwise
bool ErrorLogger::PollLogBufferEmpty(void *pvArgs)
{
    return (d_LogBufferHead == d_LogBufferTail);
}

//------------------------------------------------------------------------------
//! \brief Add a new entry to the log
//!
//! \param newLogEntry : New log entry to add
void ErrorLogger::AddNewLogEntry(const ErrLogEntry &newLogEntry)
{
    // If the test is checking the error log, then log the error
    if(d_TestsInCheckingCount)
    {
        d_LwrrentErrorLogEntries.push_back(newLogEntry);
        Printf(Tee::PriHigh, "\nLogged a new %s %d [GrIdx:0x%08x]: %s",
               (newLogEntry.Type == LT_ERROR) ? "error" : "message",
               (UINT32)d_LwrrentErrorLogEntries.size(),
               newLogEntry.GrIdx,
               newLogEntry.ErrString.c_str());
        return;
    }

    // stop spewing errors after 1000 errors
    if (MAX_UNEXPECTED_ERRORS < d_NumUnexpectedErrors)
    {
        return;
    }
    else if (MAX_UNEXPECTED_ERRORS == d_NumUnexpectedErrors)
    {
        Printf(Tee::PriNormal, "LAST UNEXPECTED ERROR BEING LOGGED. MUTING AFTER THIS ONE:\n");
    }

    // If the test is not checking the error log and the log entry is not a
    // real error, then print a message and return
    if (newLogEntry.Type != LT_ERROR)
    {
        Printf(Tee::PriLow, "\nIgnoring a new info message : %s",
               newLogEntry.ErrString.c_str());
        return;
    }

    // For verif diags, print a special message
    if ((Xp::GetOperatingSystem() == Xp::OS_WINSIM) ||
        (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM))
    {
        // This message can occur in verif diags for several reasons:
        //  1) The test is intended to provoke gpu interrupts, but is not
        //     properly configured to log/compare them.
        //  2) A minor config problem (i.e. mislabeled GPIOs in VBIOS tables).
        //  3) The RM is missing code for new sim-gpu interrupt handling.
        //  4) A bug in the simulated GPU.
        Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh,
               "ERROR: ErrorLogger::LogError is called to log HW "
               "interrupts\n");
        Printf(Tee::PriHigh,
               "ERROR: Your test didn't announce that it expects to "
               "generate interrupts\n");
        Printf(Tee::PriHigh,
               "ERROR: so ErrorLogger has to fire an assert.  Run under the "
               "debugger\n");
        Printf(Tee::PriHigh,
               "ERROR: and get a backtrace to see what interrupt the RM is "
               "logging\n");
        Printf(Tee::PriHigh, "\n");
    }

    // Note that this message can occur in mfg diags for several reasons:
    //  1) A minor config problem (i.e. mislabeled GPIOs in VBIOS tables).
    //  2) The RM mistakenly logging a harmless interrupt that it has
    //     already handled properly (common during new chip bringup).
    //  3) A real HW problem.

    Printf(Tee::PriHigh, "mods/core/utility/errloggr.cpp: GPU interrupt: %s\n",
           newLogEntry.ErrString.c_str());
    MASSERT(!"ErrorLogger::LogError called due to unexpected HW interrupt!");
    d_NumUnexpectedErrors++;
}

//------------------------------------------------------------------------------
//! \brief Works like memcpy(), but copies into the cirlwlar d_LogBuffer.
//!
//! \return A pointer to the next byte in d_LogBuffer after the copy
//!
char *ErrorLogger::CopyToLogBuffer(char *pDst, const char *pSrc, size_t Size)
{
    MASSERT(pDst >= d_LogBuffer && pDst < d_LogBuffer + LOG_SIZE);
    MASSERT(Size < LOG_SIZE - 1);
    size_t WrapAroundSize = d_LogBuffer + LOG_SIZE - pDst;

    if (Size > WrapAroundSize)
    {
        memcpy(pDst, pSrc, WrapAroundSize);
        memcpy(d_LogBuffer, pSrc + WrapAroundSize, Size - WrapAroundSize);
    }
    else
    {
        memcpy(pDst, pSrc, Size);
    }

    if (Size >= WrapAroundSize)
        return pDst + Size - LOG_SIZE;
    else
        return pDst + Size;
}

//------------------------------------------------------------------------------
//! \brief Works like memcpy(), but copies from the cirlwlar d_LogBuffer.
//!
void ErrorLogger::CopyFromLogBuffer(char *pDst, const char *pSrc, size_t Size)
{
    MASSERT(pSrc >= d_LogBuffer && pSrc < d_LogBuffer + LOG_SIZE);
    MASSERT(Size < LOG_SIZE - 1);
    size_t WrapAroundSize = d_LogBuffer + LOG_SIZE - pSrc;

    if (Size > WrapAroundSize)
    {
        memcpy(pDst, pSrc, WrapAroundSize);
        memcpy(pDst + WrapAroundSize, d_LogBuffer, Size - WrapAroundSize);
    }
    else
    {
        memcpy(pDst, pSrc, Size);
    }
}

//------------------------------------------------------------------------------
//! \brief Event function for emptying the interrupt safe log into the STL
//!        based log
void ErrorLogger::ErrorLogEventFunc()
{
    // We can't handle IRQ inside this function,
    // because an ISR may relwrsively call this function,
    // which may mess up the pointers on the stack.
    Platform::Irql oldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);

    char *pLwrHead = d_LogBufferHead;
    char *pLwrTail = d_LogBufferTail;
    char *pLwrEntryStart = d_LogBufferHead;

    while (pLwrHead != pLwrTail)
    {
        ErrLogEntry newLogEntry;

        // Find the first oclwrance of "\n" that marks the end of an entry
        while ((*pLwrHead != '\n') && (pLwrHead != pLwrTail))
        {
            INC_LOG_BUFFER_PTR(pLwrHead);
        }
        MASSERT(*pLwrHead == '\n');
        INC_LOG_BUFFER_PTR(pLwrHead);

        // The first byte of a new entry contains the type of entry, ensure
        // that the type is valid
        newLogEntry.Type = (LogType)((UINT08)(*pLwrEntryStart));
        MASSERT((newLogEntry.Type == LT_ERROR) ||
                (newLogEntry.Type == LT_INFO));

        INC_LOG_BUFFER_PTR(pLwrEntryStart);

        // The next 4 bytes store the GrIdx
        CopyFromLogBuffer((char*)&(newLogEntry.GrIdx), pLwrEntryStart, sizeof(UINT32));
        INC_BY_SIZE_LOG_BUFFER_PTR(pLwrEntryStart, sizeof(UINT32));

        // Extract the string from the entry
        newLogEntry.ErrString.resize(LOG_BUFFER_DIFF(pLwrHead,
                                                     pLwrEntryStart));
        CopyFromLogBuffer(&newLogEntry.ErrString[0], pLwrEntryStart,
                          newLogEntry.ErrString.size());

        // Abort if the error string doesn't pass the installed
        // ErrorLogFilterProc(s).  Don't bother checking if the string
        // is smaller than BUFFER_SIZE, since the ISR already checked
        // in that case.
        //
        if ((newLogEntry.ErrString.size() < BUFFER_SIZE) &&
            !ErrorCanBeFiltered(newLogEntry.ErrString.c_str()))
        {
            AddNewLogEntry(newLogEntry);
        }

        pLwrEntryStart = pLwrHead;
    }

    // Update the log buffer head so that it points to the first byte of the
    // next entry which was not pulled out of the buffer
    d_LogBufferHead = pLwrEntryStart;

    Platform::LowerIrql(oldIrql);
}

//------------------------------------------------------------------------------
//! \brief Initialize the error logger (and startup the log emptying thread)
//!
//! \return OK if successful, not ok otherwise
RC ErrorLogger::Initialize()
{
    d_LogBuffer = new char[LOG_SIZE];
    if (NULL == d_LogBuffer)
        return RC::CANNOT_ALLOCATE_MEMORY;

    d_LogBufferHead = d_LogBuffer;
    d_LogBufferTail = d_LogBuffer;
    d_LogBufferLwrrent = d_LogBuffer;

    d_EventThread.SetHandler(ErrorLogger::ErrorLogEventFunc);
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the error logger (and terminate the log emptying thread)
//!
//! \return OK if successful, not ok otherwise
RC ErrorLogger::Shutdown()
{
    StickyRC firstRc;

    firstRc = d_EventThread.SetHandler(NULL);

    if (d_LogBufferOverflow)
    {
        Printf(Tee::PriHigh,
               "ErrorLogger finishing with log buffer overflow\n");
    }

    if (d_ReentrantLogError)
    {
        Printf(Tee::PriHigh,
               "ErrorLogger finishing with reentrant call to LogError\n");
    }

    if (d_UnterminatedError)
    {
        Printf(Tee::PriHigh,
               "ErrorLogger finishing with unterminated error "
               "string (no newline at end)\n");
    }

    delete [] d_LogBuffer;

    return firstRc;
}

//------------------------------------------------------------------------------
//! \brief Called at the start of a test to reinitialize the error logger and
//!        clear out any remnants of the previous test
//!
//! \return OK if successful, not ok otherwise
RC ErrorLogger::StartingTest()
{
    StickyRC rc;

    // init is needed only by the first test
    if (d_TestsInCheckingCount)
    {
        d_TestsInCheckingCount ++;
        return rc;
    }

    // Force the buffer to be empty before starting a new test
    rc = FlushLogBuffer();

    if(!d_LwrrentErrorLogEntries.empty())
    {
        vector<ErrLogEntry>::iterator pLogEntry;
        Printf(Tee::PriHigh,
               "Previous test left errors in the InterruptLog:\n");
        for (pLogEntry = d_LwrrentErrorLogEntries.begin();
             pLogEntry != d_LwrrentErrorLogEntries.end(); ++pLogEntry)
        {
            Printf(Tee::PriHigh, "    %s (%s)", pLogEntry->ErrString.c_str(),
                   (pLogEntry->Type == LT_ERROR) ? "err" : "info");
        }
        MASSERT(!"Previous test left errors in the InterruptLog!");
        rc = RC::SOFTWARE_ERROR;
    }

    CHECK_RC(rc);

    d_HandledErrors = false;
    d_TestsInCheckingCount = 1;
    d_UnterminatedError = false;
    d_LogBufferOverflow = false;
    d_ReentrantLogError = false;
    d_MaxErrorLogUsed = 0;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Log an error (or info message).  Typically (although not always)
//!        called from an interrupt
void ErrorLogger::LogError(const char *errStr, LogType errType)
{
    BEGIN_LOG_ERROR();
    LogErrorInternal(errStr, errType);
    END_LOG_ERROR();
}

//------------------------------------------------------------------------------
//! \brief Flush an error (or info message).  Typically (although not always)
//!        called from an interrupt
void ErrorLogger::FlushError()
{
    BEGIN_LOG_ERROR();
    if (d_LogBufferLwrrent != d_LogBufferTail)
    {
        LogErrorInternal("\n", (LogType)(*d_LogBufferTail));
    }
    END_LOG_ERROR();
}

//------------------------------------------------------------------------------
//! \brief Works like FlushError(), but sets d_UnterminatedError if an
//!        unterminated error got flushed.
void ErrorLogger::FlushUnterminatedError()
{
    BEGIN_LOG_ERROR();
    if (d_LogBufferLwrrent != d_LogBufferTail)
    {
        d_UnterminatedError = true;
        LogErrorInternal("\n", (LogType)(*d_LogBufferTail));
    }
    END_LOG_ERROR();
}

//-----------------------------------------------------------------------------
//! \brief Internal implementation of LogError and FlushError.
//!
//! This method appends errStr to d_LogBuffer, and optionally flushes
//! the string so that it can be read by ErrorLogEventFunc().
//!
//! This method does not protect from reentrancy.  The caller must
//! provide reentrency protection via the BEGIN_LOG_ERROR() and
//! END_LOG_ERROR() macros.
//!
//! Since this method can be appending to d_LogBuffer at the same time
//! that the ErrorLogEventFunc() thread is reading d_LogBuffer,
//! there's a strict separation of responsibilities to keep them from
//! interfering with each other:
//!
//! - ErrorLogEventFunc() reads entries from d_LogBufferHead to
//!   d_LogBufferTail, and updates d_LogBufferHead as it goes.
//! - LogErrorInternal() writes entries from d_LogBufferTail to
//!   d_LogBufferLwrrent, and updates d_LogBufferLwrrent as it goes.
//! - When LogErrorInternal() finishes an entry, it must either:
//!   (a) Flush the entry by setting d_LogBufferTail = d_LogBufferLwrrent.
//!   (b) Cancel the entry by setting d_LogBufferLwrrent = d_LogBufferTail.
//! - All entries must start with a LogType, and end with a '\n'.  If
//!   there isn't enough room in the buffer for a full entry,
//!   including the '\n', then LogErrorInternal() must cancel the
//!   entry as described above and set d_LogBufferOverflow.
//! - Aside from Initialize() and Shutdown(), only ErrorLogEventFunc()
//!   is allowed to write d_LogBufferHead, and only LogErrorInternal()
//!   is allowed to write d_LogBufferTail and d_LogBufferLwrrent.
//!
void ErrorLogger::LogErrorInternal(const char *errStr, LogType errType)
{
    UINT32 strSize = (UINT32)strlen(errStr);
    bool   bAddSpace = false;
    bool   bAddErrorType = false;
    bool   bEndsWithNewline = false;
    UINT32 grIdx = ILWALID_GRIDX;

    if (strSize == 0)
    {
        return;
    }

    bEndsWithNewline = (errStr[strSize - 1] == '\n');

    // If any characters have been added between Tail and Current,
    // then this log entry is a continuation of the previous log
    // entry.  Add a space between between the strings for
    // continuation (necessary to keep compatibility with current
    // interrupt log files)
    if (d_LogBufferLwrrent != d_LogBufferTail)
    {
        // Don't add space if just newline
        if ((strSize > 1) || !bEndsWithNewline)
        {
            strSize++;
            bAddSpace = true;
        }
    }
    else
    {
        // Otherwise this is a new error and we need to add the error type
        // before starting the string
        strSize++;
        bAddErrorType = true;
    }

    // For Sim platforms, even interrupts are called at a MODS thread level
    // Force a log flush on these platforms if the buffer will be overrun
    // On all other platforms, this will be an error.
    if ((LOG_BUFFER_REMAIN() < strSize) &&
        ((Xp::GetOperatingSystem() == Xp::OS_WINSIM) ||
         (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM)))
    {
        Printf(Tee::PriLow,
               "ErrorLogger::LogError : Buffer overrun, forcing flush\n");
        ErrorLogEventFunc();
    }

    if (strSize > LOG_BUFFER_REMAIN())
    {
        // If there is not enough space, set the overflow flag and
        // discard the entry in progress
        d_LogBufferOverflow = true;
        d_LogBufferLwrrent = d_LogBufferTail;
        return;
    }

    // Add the string (prepending any necessary special characters) to
    // the interrupt log buffer
    if (bAddSpace)
    {
        *d_LogBufferLwrrent = ' ';
        INC_LOG_BUFFER_PTR(d_LogBufferLwrrent);
        --strSize;
        // If this error string has the GrIdx then use it to overwrite the
        // GrIdx at the start of the previous error string (d_LogBufferLastGrIdx)
        // since it will be merged to the previous string
        if (sscanf(errStr, "[GrIdx:0x%08x]", &grIdx) == 1)
        {
            CopyToLogBuffer(d_LogBufferLastGrIdx, (char*)&grIdx, sizeof(UINT32));
            // Remove the GrIdx:<idx> part from the string
            errStr = strchr(errStr, ']') + 1;
            strSize = (UINT32)strlen(errStr);
        }
    }
    if (bAddErrorType)
    {
        *d_LogBufferLwrrent = (char)errType;
        INC_LOG_BUFFER_PTR(d_LogBufferLwrrent);
        --strSize;

        // New error: If there is a GrIdx write that value else
        // write ILWALID_GRIDX to the logbuffer
        if (sscanf(errStr, "[GrIdx:0x%08x]", &grIdx) == 1)
        {
            // Remove the GrIdx:<idx> part from the string
            errStr = strchr(errStr, ']') + 1;
            strSize = (UINT32)strlen(errStr);
        }
        // Store the pointer to GrIdx value's start
        // To be used if there are more errors which be in continuation
        // to this error and the newer one has GrIdx
        d_LogBufferLastGrIdx = d_LogBufferLwrrent;
        // Write GrIdx to LogBuffer
        d_LogBufferLwrrent = CopyToLogBuffer(d_LogBufferLwrrent, (char*)&grIdx, sizeof(UINT32));
    }

    d_LogBufferLwrrent = CopyToLogBuffer(d_LogBufferLwrrent, errStr, strSize);

    if ((LOG_SIZE - LOG_BUFFER_REMAIN()) > d_MaxErrorLogUsed)
    {
        d_MaxErrorLogUsed = LOG_SIZE - LOG_BUFFER_REMAIN();
    }

    // Flush or cancel the buffer if errStr ends with a newline
    if (bEndsWithNewline)
    {
        // If the new entry fits in a BUFFER_SIZE array, then check to
        // see if it can be filtered.  If so, cancel the entry.
        char *pEntryStart = d_LogBufferTail;
        INC_LOG_BUFFER_PTR(pEntryStart);
        UINT32 EntrySize = LOG_BUFFER_DIFF(d_LogBufferLwrrent, pEntryStart);
        if (EntrySize < BUFFER_SIZE)
        {
            char Entry[BUFFER_SIZE];
            CopyFromLogBuffer(Entry, pEntryStart, EntrySize);
            Entry[EntrySize] = '\0';
            if (ErrorCanBeFiltered(Entry))
            {
                d_LogBufferLwrrent = d_LogBufferTail;
                return;
            }
        }

        // Flush the entry by updating the Tail, and set the flag to
        // tell the event thread to pull data out of the log
        d_LogBufferTail = d_LogBufferLwrrent;
        Tasker::SetEvent(d_EventThread.GetEvent());

        // Also set any externally added events to notify of new errors
        for (const auto pEvent : d_ErrorEvents)
        {
            Tasker::SetEvent(pEvent);
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Flush the log buffer
RC ErrorLogger::FlushLogBuffer()
{
    RC rc;

    FlushUnterminatedError();

    // Set the flag to tell the event thread to pull data out of the log
    Tasker::SetEvent(d_EventThread.GetEvent());

    CHECK_RC(POLLWRAP(ErrorLogger::PollLogBufferEmpty, NULL,
                      EMPTY_LOG_BUFFER_TIMEOUT));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Return whether or not the error can be filtered (i.e. is a valid
//!        error)
//!
//! \param errStr : Error string to check for filtering
//!
//! \return true if the error should be filtered (i.e. discarded), false
//!         otherwise
bool ErrorLogger::ErrorCanBeFiltered(const char *errStr)
{
    for (UINT32 ii = 0; ii < d_ErrorLogFilters.size(); ++ii)
        if (!d_ErrorLogFilters[ii](errStr))
            return true;
    for (UINT32 ii = 0; ii < d_ErrorLogFiltersForThisTest.size(); ++ii)
        if (!d_ErrorLogFiltersForThisTest[ii](errStr))
            return true;

    return false;
}

//------------------------------------------------------------------------------
//! \brief Install an error filter that is applied only when comparing errors to
//!        errors that have oclwrred.
//!
//! Note that this error filter proc, does not filter errors from being added to
//! to the log, but only filters them out during the compare.  The provided
//! function should return true if a true miscompare oclwrred between lwrErr
//! and expectedErr that are parameters to the function
//!
//! \param proc : Error filter processing function
void ErrorLogger::InstallErrorFilter(ErrorFilterProc proc)
{
    d_ErrorFilter = proc;
}

//-----------------------------------------------------------------------------
//! \brief Install a procedure to filter errors when processing the log buffer
//!
//! Install a filter procedure that will be run when pulling errors out of the
//! interrupt safe log buffer.  If the filter returns true, then log the error
//! message.  If it returns false, then discard the error message.
//!
//! More than one filter can be installed.  They must all return true in order
//! for the error message to be logged. The filters last until mods exits.
//!
//! \param proc : Error filter processing function
void ErrorLogger::InstallErrorLogFilter(ErrorLogFilterProc proc)
{
    d_ErrorLogFilters.push_back(proc);
}

//-----------------------------------------------------------------------------
//! \brief Install a procedure to filter errors when processing the log buffer
//!        for this test
//!
//! Same as InstallErrorLogFilter(), but only lasts until the end of
//! the current test.
//!
//! \param proc : Error filter processing function
void ErrorLogger::InstallErrorLogFilterForThisTest(ErrorLogFilterProc proc)
{
    d_ErrorLogFiltersForThisTest.push_back(proc);
}

//-----------------------------------------------------------------------------
//! \brief Install an event that will be set when an error gets logged.
//!
void ErrorLogger::AddEvent(ModsEvent *pEvent)
{
    MASSERT(pEvent != nullptr);
    d_ErrorEvents.push_back(pEvent);
}

//-----------------------------------------------------------------------------
//! \brief Remove an event added by AddEvent()
//!
void ErrorLogger::RemoveEvent(ModsEvent *pEvent)
{
    MASSERT(pEvent != nullptr);
    d_ErrorEvents.remove(pEvent);
}

//------------------------------------------------------------------------------
//! \brief Ignore any errors that oclwrred because the test cant handle them
//!
//! This sort of thing comes up when, for example, the test can't find the
//! expected interrupts file
void ErrorLogger::TestUnableToHandleErrors()
{
    Printf(Tee::PriHigh,
           "WARNING: Test announced that it was unable to handle HW engine "
           "errors\n");

    d_HandledErrors = true; // avoid the assert... in TestCompleted
}

//------------------------------------------------------------------------------
//! \brief Explicitly ignore any errors that occur
//!
//! Set the handled errors flag to avoid the assert in test completed
//! This shouldn't be used unless you know what you're doing
void ErrorLogger::IgnoreErrorsForThisTest()
{
    Printf(Tee::PriHigh, "Ignoring any HW errors due to manual override\n");
    d_HandledErrors = true;
}

//------------------------------------------------------------------------------
//! \brief Return whether the test contains errors or not
//!
//! \return true if the test has errors, false otherwise
bool ErrorLogger::HasErrors(CheckGrIdxMode checkGrIdx, UINT32 testGrIdx)
{
    for (auto const & errorLogEntry : d_LwrrentErrorLogEntries)
    {
        if (GrIdxMatches(checkGrIdx, testGrIdx, errorLogEntry.GrIdx))
        {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
//! \brief Called at the end of a test to terminate test processing
//!
//! Any logged errors that occur outside of a StartingTest() / TestCompleted()
//! bracketed section of code are to be considered real unexpected errors and
//! cause test failures
//!
//! \return OK if the test completed successfully, not OK otherwise
RC ErrorLogger::TestCompleted()
{
    StickyRC rc;

    // Clear log messages until no other running tests
    if (d_TestsInCheckingCount > 1)
    {
        d_TestsInCheckingCount --;
        return rc;
    }
    else
    {
        if(d_TestsInCheckingCount != 1)
        {
            Printf(Tee::PriHigh, "%s: Invalid reference count!\n",
                __FUNCTION__);
            rc = RC::SOFTWARE_ERROR;
        }
    }

    // Force the log buffer to be empty before ending the test
    rc = FlushLogBuffer();

    Printf(Tee::PriLow, "Maximum Log Used : %d bytes\n", d_MaxErrorLogUsed);
    d_MaxErrorLogUsed = 0;

    // If we have errors that the test hasn't dumped to file or compared
    // against a file...fail!
    if(!d_LwrrentErrorLogEntries.empty() && !d_HandledErrors)
    {
        MASSERT(!"Test generated HW errors but did not record or check them!");
        rc = RC::SOFTWARE_ERROR;
    }

    if (d_LogBufferOverflow)
    {
        Printf(Tee::PriHigh,
               "ErrorLogger finishing with log buffer overflow\n");
        rc = RC::SOFTWARE_ERROR;
    }

    if (d_ReentrantLogError)
    {
        Printf(Tee::PriHigh,
               "ErrorLogger finishing with reentrant call to LogError\n");
        rc = RC::SOFTWARE_ERROR;
    }

    if (d_UnterminatedError)
    {
        Printf(Tee::PriHigh,
               "ErrorLogger finishing with unterminated error "
               "string (no newline at end):\n");
        Printf(Tee::PriHigh, "    %s",
               d_LwrrentErrorLogEntries.back().ErrString.c_str());
        rc = RC::SOFTWARE_ERROR;
    }

    // Must have completed the last error we logged for this to be valid.
    MASSERT(!d_UnterminatedError);
    MASSERT(!d_LogBufferOverflow);
    MASSERT(!d_ReentrantLogError);

    d_LwrrentErrorLogEntries.clear();
    d_HandledErrors = false;
    d_TestsInCheckingCount = 0;
    d_ErrorFilter = NULL;
    d_ErrorLogFiltersForThisTest.clear();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Write the error log file
//!
//! This function creates an error log file used to gild subsequent runs of the
//! test
//!
//! \param fileName : Filename to log the errors to
//!
//! \return OK if the successfull, not OK otherwise
RC ErrorLogger::WriteFile(const char *fileName, CheckGrIdxMode checkGrIdx, UINT32 testGrIdx)
{
    RC rc;
    FileHolder file;

    // Force the log buffer to be flushed before writing the file
    rc = FlushLogBuffer();
    CHECK_RC(rc);

    // Must have completed the last error we logged for this to be valid.
    MASSERT(!d_UnterminatedError);
    MASSERT(!d_LogBufferOverflow);
    MASSERT(!d_ReentrantLogError);

    if(HasErrors(checkGrIdx, testGrIdx))
    {
        Printf(Tee::PriLow,
            "No HW engine errors logged, skipping write to %s\n", fileName);
        return OK;
    }

    CHECK_RC(file.Open(fileName, "w"));

    for(size_t i = 0; i < d_LwrrentErrorLogEntries.size(); ++i)
    {
        if (GrIdxMatches(checkGrIdx, testGrIdx, d_LwrrentErrorLogEntries[i].GrIdx))
        {
            const char *cString = d_LwrrentErrorLogEntries[i].ErrString.c_str();

            // Make sure we won't overrun the buffer when we read this back
            // in CompareErrorsWithFile
            MASSERT(strlen(cString) + 1 < BUFFER_SIZE);

            // Make sure that the string is well formed (ends in \n as we expect)
            MASSERT(cString[strlen(cString) - 1] == '\n');

            fputs(cString, file.GetFile());
        }
    }

    d_HandledErrors = true;

    Printf(Tee::PriHigh, "Wrote ErrorLogger file to %s\n", fileName);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Compare the current errors with the specified file
//!
//! This function effectively gilds the test to be sure that the logged errors
//! and/or messages are the same across multiple runs of the test
//!
//! \param fileName : Filename to compare the errors to
//! \param comp     : comparison mode to use for the error lgo
//!
//! \return OK if the successfull, not OK otherwise
RC ErrorLogger::CompareErrorsWithFile(const char *fileName, CompareMode comp, CheckGrIdxMode checkGrIdx, UINT32 testGrIdx)
{
    RC rc;
    FileHolder expected;

    // Force the interrupt log to be flushed before comparison
    rc = FlushLogBuffer();
    CHECK_RC(rc);

    // Must have completed the last error we logged for this to be valid.
    MASSERT(!d_UnterminatedError);
    MASSERT(!d_LogBufferOverflow);
    MASSERT(!d_ReentrantLogError);

    CHECK_RC(expected.Open(fileName, "r"));

    char line[BUFFER_SIZE];

    size_t numErrorsThisRun = 0;
    for (auto const & errorLogEntry : d_LwrrentErrorLogEntries)
    {
        // Count only those errors whose GrIdx match
        if (GrIdxMatches(checkGrIdx, testGrIdx, errorLogEntry.GrIdx))
        {
            ++numErrorsThisRun;
        }
    }
    unique_ptr<IRangeChecker> rangeChecker(InstanceRangeChecker(comp));

    while(fgets(line, BUFFER_SIZE, expected.GetFile()) && !feof(expected.GetFile()))
    {
        // Ignore empty lines.
        if (line[0] == '\n') continue;

        line[strlen(line)] = '\0'; // get rid of \n
        if (OK != (rc = rangeChecker->LogString(line)))
        {
            break;
        }
    }

    Printf(Tee::PriNormal, "Expect %s error, found %d\n",
            rangeChecker->GetExpectedErrorRange().c_str(), (int)numErrorsThisRun);

    //Do error range change
    if (rc == OK)
    {
        rc = rangeChecker->CheckErrors(d_LwrrentErrorLogEntries, checkGrIdx, testGrIdx);
    }

    d_HandledErrors = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Print the errors that lwrrently exist in the log
//!
//! \param pri : Priority to print the errors at
void ErrorLogger::PrintErrors(Tee::Priority pri)
{
    if(!d_LwrrentErrorLogEntries.empty())
    {
        Printf(pri, "ErrorLogger recorded the following errors:\n");
        for(size_t i = 0; i < d_LwrrentErrorLogEntries.size(); ++i)
        {
            Printf(pri, "   %s", d_LwrrentErrorLogEntries[i].ErrString.c_str());
        }
    }
    else
    {
        Printf(pri, "ErrorLogger is empty\n");
    }
}

//------------------------------------------------------------------------------
//! \brief Return the number of unexpected errors
//!
//! \return Current number of unexpected errors
UINT32 ErrorLogger::UnhandledErrorCount()
{
    return d_NumUnexpectedErrors;
}

//------------------------------------------------------------------------------
//! \brief Return the current number of errors
//!
//! (Ugh) A compute trace_3d plugin was designed to process error log
//! entries using a special gilding mechanism and plugged directly into
//! the error log
//!
//! \return Current number of errors
UINT32 ErrorLogger::GetErrorCount()
{
    return (UINT32)d_LwrrentErrorLogEntries.size();
}

//------------------------------------------------------------------------------
//! \brief Return the error string at the specified index
//!
//! (Ugh) A compute trace_3d plugin was designed to process error log
//! entries using a special gilding mechanism and plugged directly into
//! the error log
//!
//! \param index : index in the error strings to return
//!
//! \return Error string at the specified index
string ErrorLogger::GetErrorString(UINT32 index)
{
    MASSERT(index < d_LwrrentErrorLogEntries.size());
    return d_LwrrentErrorLogEntries[index].ErrString;
}

//------------------------------------------------------------------------------
// Private stuff
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Parse the line in terms of errstring with range if the line starts with '[', otherwise regard the
//!        line as errstring with the range [1,1].
//!
//! The function tries to match the line with the format "[<min-oclwrrences>:<max-oclwrrences>]errstring"
//! <min-oclwrrences> and <max-oclwrrences> are optional and 0 by default. The variables extracted from
//! the line are stored in errString and errRange respectively.
//!
//! \param errLine   : expected string in the gild file
//! \param errString : a pointer to string where the result is stored
//! \param errRange  : a pointer to Range where the result is stored
//! \return true if errLine matches the pattern and the datas extracted are legal
bool ErrorLogger::IRangeChecker::ExtractErrAndRangeFromLine
(
    const string &errLine,
    string* errString,
    Range* errRange
)
{
    if (errLine[0] != '[') //line with range specified
    {
        *errString = errLine;
        errRange->SetMin(1);
        errRange->SetMax(1);
        return true;
    }
    else
    {
        size_t left = 0, right = 0;
        char* illegal = NULL;
        int min = 0, max = 0;
        bool minSpecified = false, maxSpecified = false;
        bool success = true;

        //extract min
        if (success)
        {
            left++;
            if (string::npos == (right = errLine.find(':')) || right < left)
            {
                success = false;
            }
            min = strtol(errLine.c_str() + left, &illegal, 10);
            if (illegal != errLine.c_str() + right)
            {
                // *illegal is the first non-numeric char after errLine[left],
                // which should have been errLine[right]
                success = false;
            }
            if (success && left != right)
            {
                errRange->SetMin(min);
                minSpecified = true;
            }
        }

        //extract max
        if (success)
        {
            left = right + 1;
            if (string::npos == (right = errLine.find(']')) || right < left)
            {
                success = false;
            }
            max = strtol(errLine.c_str() + left, &illegal, 10);
            if (illegal != errLine.c_str() + right)
            {
                // *illegal is the first non-numeric char after errLine[left],
                // which should have been errLine[right]
                success = false;
            }
            if (success && left != right)
            {
                errRange->SetMax(max);
                maxSpecified = true;
            }
        }

        //extract errstring
        if (success)
        {
            do
            {
                right++;
            } while (isspace(errLine[right])); // ignore spaces beteen range spe and interrupt spe
            *errString = errLine.substr(right, errLine.length() - right);
            if (errString->empty())
            {
                success = false;
            }
        }

        //check whether min is not more than max
        if (minSpecified && maxSpecified && min > max)
        {
            Printf(Tee::PriHigh,
               "Nonsensical range specified at the line: %s !\n",
               errLine.c_str());
            success = false;
        }

        if (!success)
        {
            Printf(Tee::PriHigh,
               "Unable to parse the line because of illegal format: %s !\n",
               errLine.c_str());
        }
        return success;
    }
}

ErrorLogger::IRangeChecker* ErrorLogger::InstanceRangeChecker
(
    ErrorLogger::CompareMode comp
)
{
    IRangeChecker *rangeChecker;
    switch(comp)
    {
        default:
            MASSERT(!"invalid CompareMode specified");

        case Exact:
            rangeChecker = new OrderedRangeChecker();
            break;

        case OneToOne:
            rangeChecker = new UnorderedRangeChecker();
            break;
    }
    return rangeChecker;
}

RC ErrorLogger::OrderedRangeChecker::CheckString
(
    size_t index,
    const string &errLine
)
{
    while(!m_ExpectedErrors.empty() &&
        !(Utility::MatchWildCard(errLine.c_str(), m_ExpectedErrors.front().first.c_str())))
    {
        if ( d_ErrorFilter &&
                !d_ErrorFilter(index,
                    errLine.c_str(),
                    m_ExpectedErrors.front().first.c_str())
              )
        {
            break;
        }
        if (!CheckFront())
        {
            Printf(Tee::PriHigh, "Error index %lld:\n", static_cast<UINT64>(index));
            Printf(Tee::PriHigh, "   Expected: %s", m_ExpectedErrors.front().first.c_str());
            Printf(Tee::PriHigh, "   Actual:   %s\n", errLine.c_str());
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
        else
        {
            m_ExpectedErrors.pop();
        }
    }
    if (m_ExpectedErrors.empty())
    {
        Printf(Tee::PriHigh,
               "Current run had unexpected error: %s !Stop comparing!\n",
               errLine.c_str());
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    m_ExpectedErrors.front().second.IncErrorCount();

    // If the current expected error is already at the maximum count,
    // move on to the next expected error.  This is necessary because it's
    // possible for expected error entries to be duplicates due to the error
    // filter above. (See bug 1477249.)
    if (m_ExpectedErrors.front().second.IsAtMax())
    {
        m_ExpectedErrors.pop();
    }

    return OK;
}

RC ErrorLogger::OrderedRangeChecker::EndCheck()
{
    while(!m_ExpectedErrors.empty())
    {
        if (!CheckFront())
        {
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
        m_ExpectedErrors.pop();
    }
    return OK;
}

void ErrorLogger::OrderedRangeChecker::AddRange
(
    const string &error,
    const Range &range
)
{
    if (m_Min != UNSPECIFIED)
    {
        m_Min = (range.GetMin() == UNSPECIFIED) ? m_Min : m_Min + range.GetMin();
    }
    if (m_Max != UNSPECIFIED)
    {
        m_Max = (range.GetMax() == UNSPECIFIED) ? UNSPECIFIED : m_Max + range.GetMax();
    }
    m_ExpectedErrors.push(make_pair(error, range));
}

bool ErrorLogger::OrderedRangeChecker::CheckFront()
{
    if (!m_ExpectedErrors.front().second.CheckMax())
    {
        Printf(Tee::PriHigh,
               "Current run had more than the max expected number of: %s"
               " errors. Stop comparing!\n",
               m_ExpectedErrors.front().first.c_str());
        return false;
    }
    if (!m_ExpectedErrors.front().second.CheckMin())
    {
        Printf(Tee::PriHigh,
               "Current run had less than the min expected number of: %s"
               " errors. Stop comparing!\n",
               m_ExpectedErrors.front().first.c_str());
        return false;
    }
    return true;
}

RC ErrorLogger::UnorderedRangeChecker::CheckString
(
    size_t index,
    const string &errLine
)
{
    ErrorArray::iterator iter;
    for(iter = m_ExpectedErrors.begin(); iter != m_ExpectedErrors.end(); ++iter)
    {
        if (Utility::MatchWildCard(errLine.c_str(), iter->first.c_str()))
        {
            break;
        }
    }

    if (iter == m_ExpectedErrors.end())
    {
        Printf(Tee::PriHigh,
               "Current run had unexpected error %s. Stop comparing!\n",
               errLine.c_str());
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    else
    {
        iter->second.IncErrorCount(); // Expected this error, increment count
        if (!iter->second.CheckMax())
        {
            Printf(Tee::PriHigh,
                   "Current run had more then the max expected number of: %s"
                   " errors. Stop comparing!\n",
                   errLine.c_str());
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }
    return OK;
}

RC ErrorLogger::UnorderedRangeChecker::EndCheck()
{
    for(ErrorArray::const_iterator iter = m_ExpectedErrors.begin(); iter != m_ExpectedErrors.end(); ++iter)
    {
        if (!iter->second.CheckMin())
        {
            Printf(Tee::PriHigh,
                   "Current run had less then the min expected number of: %s"
                   " errors. Stop comparing!\n",
                iter->first.c_str());
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }
    return OK;
}

void ErrorLogger::UnorderedRangeChecker::CheckPerformance(size_t totalErrorNum)
{
    const size_t performanceWarningLimit = 500;
    if (totalErrorNum > performanceWarningLimit)
    {
        Printf(Tee::PriHigh,
            "%s: The following compare algorithm is not efficient enough "
            "for interrupt test with more than %lld gilded interrupt entries!\n",
            __FUNCTION__, static_cast<UINT64>(performanceWarningLimit));
    }
}

void ErrorLogger::UnorderedRangeChecker::AddRange
(
    const string &error,
    const Range &range
)
{
    if (m_Min != UNSPECIFIED)
    {
        m_Min = (range.GetMin() == UNSPECIFIED) ? m_Min : m_Min + range.GetMin();
    }
    if (m_Max != UNSPECIFIED)
    {
        m_Max = (range.GetMax() == UNSPECIFIED) ? UNSPECIFIED : m_Max + range.GetMax();
    }
    for(ErrorArray::iterator iter = m_ExpectedErrors.begin(); iter != m_ExpectedErrors.end(); ++iter)
    {
        if (iter->first == error)
        {
            iter->second.Merge(range);
            return;
        }
    }
    m_ExpectedErrors[error] = range;
}
