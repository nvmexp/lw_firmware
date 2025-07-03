/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2012, 2015-2019, 2021 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/errcount.h"
#include "core/tests/modstest.h"
#include "jsdbgapi.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/log.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include <algorithm>

//! Set of all ErrCounter objects on which Initialize() was called.
//! The set is allocated when the first ErrCounter is constructed, and
//! freed when the last ErrCounter is destructed.  (We store the set
//! as a pointer to ensure that it's constructed before it's used; we
//! can't guarantee the order that static objects are constructed.
//!
/* static */ set<ErrCounter*> *ErrCounter::s_pInitializedCounters = NULL;

//! Mutex to prevent objects from getting inserted or erased from
//! s_pInitializedCounters.  Allocated and freed at the same time as
//! s_pInitializedCounters.  To prevent deadlock, any method that
//! locks both this mutex and the m_Mutex of one of the ErrCounter
//! objects should lock this mutex first.
//!
/* static */ void *ErrCounter::s_InitializedCountersMutex = NULL;

//! Number of constructed ErrCounters, used to allocate
//! s_pInitializedCounters and s_InitializedCountersMutex.
//!
/* static */ UINT32 ErrCounter::s_NumErrCounters = 0;

//! Flag to prevent high-priority error counters from overriding
//! non-OK RCs from tests with their own RCs.
/* static */ bool ErrCounter::s_RespectTestRc = false;

//--------------------------------------------------------------------
//! \brief ErrCounter constructor.
//!
ErrCounter::ErrCounter()
  : m_bIgnoreOuterFrameErrors(false)
{
    if (s_NumErrCounters++ == 0)
    {
        s_pInitializedCounters = new set<ErrCounter*>;
        s_InitializedCountersMutex = Tasker::AllocMutex(
            "ErrCounter::s_InitializedCountersMutex",
            Tasker::mtxUnchecked);
    }
    m_Mutex = Tasker::AllocMutex("ErrCounter::m_Mutex",
                                 Tasker::mtxUnchecked);
    Clear();
}

//--------------------------------------------------------------------
//! \brief ErrCounter destructor.  Clear it and ignore any pending errors.
//!
ErrCounter::~ErrCounter()
{
    ShutDown(false);
    Tasker::FreeMutex(m_Mutex);

    if (--s_NumErrCounters == 0)
    {
        delete s_pInitializedCounters;
        Tasker::FreeMutex(s_InitializedCountersMutex);
        s_pInitializedCounters = NULL;
        s_InitializedCountersMutex = NULL;
    }
}

//--------------------------------------------------------------------
//! \brief Initialize the ErrCounter, to start checking for errors.
//!
//! This method initializes the ErrCounter, and adds it to the static
//! s_pInitializedCounters list so that it'll get checked on every JS
//! function or every ModsTest.
//!
//! It's safe to call this method before the JavaScript parser is
//! initialized, since this method does not read the "g_" JavaScript
//! variables directly.  (They're read on the first JS function or
//! ModsTest.)  However, this method does call ReadErrorCount(), so
//! it's not safe to use if the ReadErrorCount() method isn't callable
//! yet.
//!
//! \param Name The "Xxx" name used to read JavaScript variables; see
//!     the ErrCounter documentation.  May be "", in which case this
//!     object does not read any JS variables.
//! \param NumErrCounts The number of elements in the error-count
//!     arrays that get checked for errors.  The "AllowedErrors"
//!     arrays have this many elements.
//! \param NumHiddenCounts Any additional elements in the error-count
//!     arrays that get recorded and updated just like the error
//!     counts, but don't get checked.  Used for things like the
//!     per-partition breakdown of errors.  All the UINT64 arrays
//!     except "AllowedERrors" have NumErrCounts+NumHiddenCounts
//!     elements.
//! \param pEPMCountIndices If not nullptr, the vector pointed to contains a list of
//!     indices into m_LwrrentCount that tell us which error counts should also
//!     be used to callwlate EPM (errors per minute).
//! \params Coverage Determines when the count(s) get checked.
//! \return OK on success, non-OK if the initial call of
//!     ReadErrorCount() fails.  On failure, this object will not be
//!     checked on subsequent JS functions or ModsTests.
//!
RC ErrCounter::Initialize
(
    const string &Name,
    UINT32 NumErrCounts,
    UINT32 NumHiddenCounts,
    vector<UINT32> *pEPMCountIndices,
    FrameType Coverage,
    Priority aPriority
)
{
    Tasker::MutexHolder lock1(s_InitializedCountersMutex);
    Tasker::MutexHolder lock2(m_Mutex);
    RC rc;

    MASSERT(!IsInitialized());
    MASSERT(NumErrCounts > 0);

    // RAII object to clean up this object on error
    //
    Utility::CleanupOnReturn<ErrCounter>
        ErrorCleanup(this, &ErrCounter::Clear);

    // Load object with initial values
    //
    Clear();
    m_Name = Name;
    m_NumErrCounts = NumErrCounts;
    m_NumAllCounts = NumErrCounts + NumHiddenCounts;
    m_Coverage = Coverage;
    m_Priority = aPriority;
    m_AllowedErrorsPerStackFrame.resize(m_NumErrCounts, 0);
    m_OuterStackFrame.AllowedErrors.resize(m_NumErrCounts, 0);
    m_OuterStackFrame.InnerErrors.resize(m_NumAllCounts, 0);
    m_ExpectedExitsSansEnter = GetStackDepth(Coverage);

    // Read the initial counts
    //
    m_LwrrentCount.resize(m_NumAllCounts, 0);
    CHECK_RC(ReadErrorCount(&m_LwrrentCount[0]));
    if (pEPMCountIndices)
    {
        m_EPMCountIndices = *pEPMCountIndices;
    }
    m_AllowedErrorsPerMinPerStackFrame.resize(m_EPMCountIndices.size(), 0);
    m_OuterStackFrame.EnterCount = m_LwrrentCount;
    m_OuterStackFrame.LastError = m_LwrrentCount;

    // Insert this object into s_pInitializedCounters, so that it'll
    // get automatically checked.
    //
    s_pInitializedCounters->insert(this);
    ErrorCleanup.Cancel();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Shut down the ErrCounter.
//!
//! \param CheckForErrorsOnShutdown If true, then make a final call to
//!     ReadErrorCount() and check for any pending errors.  If false,
//!     then do a "quick and dirty" shutdown without checking for
//!     errors.
//! \return OK on success, non-OK if the final error-check fails.
//!     Guaranteed to return OK if CheckForErrorsOnShutdown is false.
//!
RC ErrCounter::ShutDown(bool CheckForErrorsOnShutdown)
{
    if (!s_InitializedCountersMutex || !m_Mutex)
    {
        return RC::OK;
    }

    Tasker::MutexHolder lock1(s_InitializedCountersMutex);
    Tasker::MutexHolder lock2(m_Mutex);
    StickyRC firstRc;

    // Do a final error-check if CheckForErrorsOnShutdown is set, and
    // the object is initialized from a successful Initialize() call.
    //
    if (CheckForErrorsOnShutdown && IsInitialized())
    {
        // Read the global JS variables if we never called a JS
        // function or ModsTest, and read the new count(s).
        //
        InitFromJs();
        firstRc =
            UpdateLwrrentCount((m_OuterStackFrame.VerboseFlags &
                                VF_ON_COUNT_CHANGE) ? true : false);

        // Call the OnCheckpoint callback if the Verbose flag is set
        //
        if (m_OuterStackFrame.VerboseFlags & VF_ON_CP)
        {
            CheckpointData Data = { &m_LwrrentCount[0],
                                    &m_OuterStackFrame.AllowedErrors[0],
                                    m_OuterStackFrame.Disabled };
            OnCheckpoint(&Data);
        }

        // Check each pending thread, to see if any of them exceeds
        // AllowedErrors.
        //
        for (map<Tasker::ThreadID, Stack>::iterator iter = m_Stacks.begin();
             iter != m_Stacks.end(); ++iter)
        {
            Stack *pStack = &iter->second;
            while (!pStack->empty())
            {
                firstRc = CheckForError(pStack, NULL, false);
                PopStack(pStack);
            }
        }

        // Check whether we've exceeded g_XxxAllowedTotal since mods
        // started, which weren't already reported elsewhere.
        //
        firstRc = CheckForError(NULL, NULL, false);
    }

    // Remove the object from s_pInitializedCounters and clear it to a
    // quiescent state.
    //
    Clear();
    return firstRc;
}

//--------------------------------------------------------------------
//! Return true if this object is initialized (i.e. it's on the
//! s_pInitializedCounters list so that it gets checked for errors at
//! each JsFunction or ModsTest).
//!
bool ErrCounter::IsInitialized() const
{
    return s_pInitializedCounters->count(const_cast<ErrCounter*>(this)) > 0;
}

//--------------------------------------------------------------------
//! Return true if this counter should be used in the current stack
//! frame, false if not.
//!
/* virtual */ bool ErrCounter::WatchInStackFrame(ModsTest *pTest) const
{
    FrameType Type = pTest ? MODS_TEST : JS_FUNCTION;
    return (Type == m_Coverage);
}

//--------------------------------------------------------------------
//! \brief Comparison function
//!
//! This less-than comparison function is used to sort ErrCounter
//! objects in the order they should be checked for errors in
//! ExitStackFrame et al:
//! * Low m_Priority comes first
//! * Otherwise, low m_Name comes first
//! * Otherwise, the ErrCounter at a lower memory address comes first
//!
bool ErrCounter::operator<(const ErrCounter &rhs) const
{
    if (m_Priority != rhs.m_Priority)
        return (m_Priority < rhs.m_Priority);
    else if (m_Name != rhs.m_Name)
        return (m_Name < rhs.m_Name);
    else
        return (this < &rhs);
}

//--------------------------------------------------------------------
//! This static function should be called whenever a test, function,
//! or whatever that we want to check for errors.  It loops through
//! all initialized ErrCounter objects, and records the appropriate
//! data.
//!
//! \param pTest The ModsTest that we're entering, or NULL if we're
//!     entering a JavaScript function.
//! \param pFrameId Opaque pointer used to identify this stack frame.
//!     Used to match ExitStackFrame() calls with the corresponding
//!     EnterStackFrame().
//! \praam SaveRc If true, then save any errors that occur in this
//!     method in the StackFrame, and return them in the corresponding
//!     ExitStackFrame().  Useful for JS_FUNCTION calls, since that
//!     interface only support returning an RC when the function is
//!     exited, not when it's entered.
//! \return OK on success, non-OK if any ReadErrorCount() fails.
//!     Always returns OK if SaveRc is true.
//!
/* static */ RC ErrCounter::EnterStackFrame
(
    ModsTest *pTest,
    void *pFrameId,
    bool SaveRc
)
{
    vector<ErrCounter*> counters;
    StickyRC firstRc;

    GetInitializedErrCounters(pTest, &counters);
    for (vector<ErrCounter*>::iterator ppCounter = counters.begin();
         ppCounter != counters.end(); ++ppCounter)
    {
        firstRc = (*ppCounter)->DoEnterStackFrame(pTest, pFrameId, SaveRc);
        Tasker::ReleaseMutex((*ppCounter)->GetMutex());
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! This static function should be called when we reach the end of a
//! test/function in which we called EnterStackFrame().  It loops
//! through all initialized ErrCounter objects, and checks for error
//! on each.
//!
//! \param pTest See EnterStackFrame()
//! \param pFrameId See EnterStackFrame()
//! \param[out] pOverrideTestRc If non-NULL, return a flag telling
//!     whether ther error returned by this func should override the
//!     error returned by the test.
//!
/* static */ RC ErrCounter::ExitStackFrame
(
    ModsTest *pTest,
    void *pFrameId,
    bool *pOverrideTestRc
)
{
    vector<ErrCounter*> counters;
    StickyRC firstRc;
    bool overrideTestRc = false;

    GetInitializedErrCounters(pTest, &counters);
    for (vector<ErrCounter*>::iterator ppCounter = counters.begin();
         ppCounter != counters.end(); ++ppCounter)
    {
        firstRc = (*ppCounter)->DoExitStackFrame(pTest, pFrameId, false);
        Tasker::ReleaseMutex((*ppCounter)->GetMutex());
        if ((firstRc != OK) &&
            ((*ppCounter)->m_Priority < OVERRIDE_TEST_RC_PRI) &&
            !s_RespectTestRc)
        {
            overrideTestRc = true;
        }
    }
    if (pOverrideTestRc)
    {
        *pOverrideTestRc = overrideTestRc;
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! This static function should can be called anytime to check whether
//! exiting from the current stack frame will cause an error.  It loops
//! through all initialized ErrCounter objects, and checks for error
//! on each.
//!
//! Callers should be aware that, if pFrameId != frame id of the innermost
//! frame, this method will pop frames until the frame with the matching
//! pFrameId becomes the innermost frame.
//!
//! \param pTest         See EnterStackFrame().
//! \param pFrameId      See EnterStackFrame().
//!
/* static */ RC ErrCounter::CheckExitStackFrame
(
    ModsTest *pTest,
    void *pFrameId
)
{
    vector<ErrCounter*> counters;
    StickyRC firstRc;

    GetInitializedErrCounters(pTest, &counters);
    for (vector<ErrCounter*>::iterator ppCounter = counters.begin();
         ppCounter != counters.end(); ++ppCounter)
    {
        firstRc = (*ppCounter)->DoExitStackFrame(pTest, pFrameId, true);
        Tasker::ReleaseMutex((*ppCounter)->GetMutex());
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! This function checks whether exiting from the current stack frame
//! will cause an error for a specific error counter. It is similar
//! to the static function CheckExitStackFrame, but only checks *this*
//! ErrCounter, rather than all initialized ErrCounters. Assumes that
//! this ErrCounter is initialized.
//!
//! \param pTest         See EnterStackFrame().
//! \param pFrameId      See EnterStackFrame().
//!
RC ErrCounter::CheckCounterExitStackFrame
(
    ModsTest *pTest,
    void *pFrameId
)
{
    RC rc;
    if (WatchInStackFrame(pTest))
    {
        Tasker::MutexHolder lock(GetMutex());
        rc = DoExitStackFrame(pTest, pFrameId, true);
    }
    return rc;
}

//--------------------------------------------------------------------
//! Helper function to retrieve a stack frame based on frame id
//!
//! \param pFrameId FrameId of the stack frame we want
//!
ErrCounter::StackFrame* ErrCounter::GetStackFrame(void *pFrameId)
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();
    MASSERT(m_Stacks.count(threadId));
    Stack *pStack = &m_Stacks[threadId];

    // Start search in reverse, since we usually want recently pushed frames
    for (auto it = pStack->rbegin(); it != pStack->rend(); ++it)
    {
        StackFrame *pStackFrame = &*it;
        if (pStackFrame->pFrameId == pFrameId)
        {
            return pStackFrame;
        }
    }

    return nullptr;
}

//--------------------------------------------------------------------
//! Function to check errors per minute of a specific stack frame.
//! Usually, the stack frame we want to check corresponds to a "Run"
//! function of a test
//!
//! \param pTest Pointer to the test that called this method
//! \param pFrameId FrameId of the stack frame we want
//! \param[out] pOverrideTestRc If non-NULL, return a flag telling
//!     whether the error returned by this func should override the
//!     error returned by the test.
//!
/* static */ RC ErrCounter::CheckEPMAllCounters
(
    ModsTest *pTest,
    void *pFrameId,
    bool *pOverrideTestRc
)
{
    RC rc;

    vector<ErrCounter*> counters;
    StickyRC firstRc;
    bool overrideTestRc = false;
    GetInitializedErrCounters(pTest, &counters);
    for (auto pCounter : counters)
    {
        firstRc = pCounter->CheckEPM(pFrameId);
        Tasker::ReleaseMutex(pCounter->GetMutex());
        if (firstRc != OK &&
            pCounter->m_Priority < OVERRIDE_TEST_RC_PRI &&
            !s_RespectTestRc)
        {
            overrideTestRc = true;
        }
    }
    if (pOverrideTestRc)
    {
        *pOverrideTestRc = overrideTestRc;
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Check EPM of a stack frame
//!
//! If elapsed time between stack frame entrance and when error counts
//! were read is < 0ms, don't callwlate or check EPM
//!
RC ErrCounter::CheckEPM(void *pFrameId)
{
    RC rc;

    StackFrame *pStackFrame = GetStackFrame(pFrameId);
    if (!pStackFrame || !pStackFrame->EnterTimeMs)
    {
        Printf(Tee::PriError, "Corrupted stack frame or invalid frame id passed in!\n");
        return RC::SOFTWARE_ERROR;
    }

    // Update error counts before callwlating EPM
    CHECK_RC(UpdateLwrrentCount((pStackFrame->VerboseFlags & VF_ON_COUNT_CHANGE) ? true : false));
    const UINT64 lwrrTimeMs = Xp::GetWallTimeMS();
    MASSERT(lwrrTimeMs >= pStackFrame->EnterTimeMs);
    const UINT64 elapsedTimeMS = lwrrTimeMs - pStackFrame->EnterTimeMs;
    if (elapsedTimeMS == 0ULL)
    {
        return RC::OK;
    }

    vector<UINT64> lwrrEPMs;
    bool hasError = false;
    if (!pStackFrame->Disabled)
    {
        // To callwlate EPM, we take
        // (lwrrErrCnt - frameEnterErrCnt) / (lwrrTime - frameEnterTime)
        for (UINT32 i = 0; i < m_EPMCountIndices.size(); ++i)
        {
            UINT32 countIdx = m_EPMCountIndices[i];
            UINT64 errDiff = m_LwrrentCount[countIdx] - pStackFrame->EnterCount[countIdx];
            FLOAT64 errPerMs = static_cast<FLOAT64>(errDiff) / elapsedTimeMS;
            UINT64 errPerMin = static_cast<UINT64>(ceil(errPerMs * 1000 * 60));
            lwrrEPMs.push_back(errPerMin);
            if (errPerMin > pStackFrame->AllowedErrorsPerMin[i])
            {
                hasError = true;
                rc = RC::EDC_ERROR;
            }
        }
    }

    if (hasError)
    {
        Printf(Tee::PriNormal, "\nOne or more EPM exceeds allowed EPM!\n");
    }

    // Report current EPM if there's an error OR the corresponding
    // verbose flag is set.
    if (hasError || (pStackFrame->VerboseFlags & VF_EPM))
    {
        // Consolidate and report EPM info to user
        PerMinErrorData perMinData =
        {
            lwrrEPMs.data(),
            pStackFrame->AllowedErrorsPerMin.data(),
            elapsedTimeMS
        };
        ReportLwrrEPM(&perMinData, elapsedTimeMS, hasError);
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Print a ModsTest's ErrCounter properties.
//!
//! This function is called from ModsTest::PrintJsProperties to expose
//! the JavaScript properties automatically created by this class.
//! See the ErrCounter documentation for the list of properties.
//!
//! \param Pri The priority to pass to Printf()
//! \param pTest The ModsTest that we're printing the properties of.
//!
/* static */ void ErrCounter::PrintJsProperties
(
    Tee::Priority Pri,
    ModsTest *pTest
)
{
    MASSERT(pTest != NULL);
    vector<ErrCounter*> counters;

    GetInitializedErrCounters(pTest, &counters);
    for (vector<ErrCounter*>::iterator ppCounter = counters.begin();
         ppCounter != counters.end(); ++ppCounter)
    {
        (*ppCounter)->DoPrintJsProperties(Pri, pTest);
        Tasker::ReleaseMutex((*ppCounter)->GetMutex());
    }
}

//--------------------------------------------------------------------
//! \fn RC ErrCounter::OnError(const ErrorData *pData)
//!
//! This virtual method is called whenever the error oclwrs because
//! the counter incremented past AllowedErrors.  The return value
//! indicates the error than should be returned by ErrCounter.

//! Note: some tests periodically call ErrCounter::CheckExitStackFrame
//! to see if they should abort early.  So that this method may be
//! called twice: once during the early-abort check to preview the
//! error code, and once when the test actually exits.  If this method
//! does anything beside return an error code, such as print an error
//! message, it should probably check pData->bCheckExitOnly and only
//! print a message if bCheckExitOnly == false.
//!
/* virtual RC ErrCounter::OnError(const ErrorData *pData) = 0 */

//--------------------------------------------------------------------
//! This virtual method is called whenever the counts change,
//! regardless of whether it triggers an error.  Override this method
//! in the subclass to take some action when that happens.
//!
/* virtual */ void ErrCounter::OnCountChange
(
    const CountChangeData *pData
)
{
}

//--------------------------------------------------------------------
//! This virtual method is called whenever we enter/exit a JsFunction
//! or ModsTest (depending on Coverage) with the Verbose flag enabled.
//! Override this method in the subclass to take some action when that
//! happens, such as print the counters.
//!
//! Note: Overriding this method to print the counters is what makes
//! the Verbose flag work.  If you don't override this, then setting
//! Verbose doesn't really do anything.
//!
/* virtual */ void ErrCounter::OnCheckpoint(const CheckpointData *pData) const
{
}

//--------------------------------------------------------------------
//! Clear the object to a quiescent state, and remove it from the
//! s_pInitializedCounters set so that it no longer gets checked for
//! errors.
//!
//! Calling this method is basically the same as calling
//! ShutDown(false), except that it doesn't lock the mutexes first.
//! The caller should make sure the mutexes are locked.
//!
void ErrCounter::Clear()
{
    s_pInitializedCounters->erase(this);

    m_Name = "";
    m_NumErrCounts = 0;
    m_NumAllCounts = 0;
    m_Coverage = MODS_TEST;
    m_Priority = static_cast<Priority>(0);

    m_AllowedErrorsPerStackFrame.clear();
    m_AllowedErrorsPerMinPerStackFrame.clear();
    m_InitializedFromJs = false;

    m_Stacks.clear();

    m_OuterStackFrame.pFrameId = NULL;
    m_OuterStackFrame.rc.Clear();
    m_OuterStackFrame.VerboseFlags = 0;
    m_OuterStackFrame.AllowedErrors.clear();
    m_OuterStackFrame.Disabled = false;
    m_OuterStackFrame.EnterCount.clear();
    m_OuterStackFrame.LastError.clear();
    m_OuterStackFrame.InnerErrors.clear();

    m_LwrrentCount.clear();
    m_ExpectedExitsSansEnter = 0;
}

//--------------------------------------------------------------------
//! \brief Initialize this object from JavaScript.
//!
//! If the Name parameter was passed to Initialize(), then this method
//! reads global JavaScript properties to override some members of
//! this object.  See the "g_" properties in the ErrCounter description.
//!
//! This method only reads the JS properties the first time it's
//! called after Initialize().  After that, it sets a flag so that
//! subsequent calls are no-ops.  See Initialize() for more
//! information.
//!
void ErrCounter::InitFromJs()
{
    // Abort if we've already called this function since Initialize()
    //
    if (m_InitializedFromJs || !JavaScriptPtr::IsInstalled())
    {
        return;
    }
    m_InitializedFromJs = true;

    JSObject *pGlobalObject = JavaScriptPtr()->GetGlobalObject();

    OverrideFromJs(pGlobalObject, ENCJSENT("g_ErrCounterRespectTestRc"),
                   &s_RespectTestRc);

    // Abort if the "Xxx" name is empty, in which case this object
    // never reads any JS variables.
    //
    if (m_Name == "")
    {
        return;
    }

    string verboseFlagsProp = "g_" + m_Name + "VerboseFlags";
    string allowedProp = "g_" + m_Name + "Allowed";
    string allowedPerMinProp = allowedProp + "PerMin";
    string allowedTotalProp = "g_" + m_Name + "AllowedTotal";
    string disabledProp = "g_" + m_Name + "Disabled";

    // Override members of this object from JavaScript
    OverrideFromJs(pGlobalObject, verboseFlagsProp, &m_OuterStackFrame.VerboseFlags);
    bool setXxxAllowed = OverrideFromJs(pGlobalObject, allowedProp, &m_AllowedErrorsPerStackFrame);
    bool setXxxAllowedTotal = OverrideFromJs(pGlobalObject, allowedTotalProp,
                                             &m_OuterStackFrame.AllowedErrors);
    OverrideFromJs(pGlobalObject, allowedPerMinProp, &m_AllowedErrorsPerMinPerStackFrame);
    OverrideFromJs(pGlobalObject, disabledProp, &m_OuterStackFrame.Disabled);

    // Overwrite allowed errors in subclass
    OverwriteAllowedErrors(m_OuterStackFrame.AllowedErrors.data(),
                           m_OuterStackFrame.AllowedErrors.size());

    // If JS specified either g_XxxAllowed (the per-test error
    // threshold) or g_XxxAllowedTotal (the cumulative threshold), but
    // not both, then set the other value to something reasonable.
    //
    if (setXxxAllowed && !setXxxAllowedTotal)
    {
        m_bIgnoreOuterFrameErrors = true;
    }
    else if (setXxxAllowedTotal && !setXxxAllowed)
    {
        // Set per-test errors to the total errors
        m_AllowedErrorsPerStackFrame = m_OuterStackFrame.AllowedErrors;
    }
}

//--------------------------------------------------------------------
//! \brief Body of EnterStackFrame() for a single ErrCounter object.
//!
//! This method reads the current error count(s), and creates a
//! StackFrame struct to record the error count at the start of the
//! function/ModsTest.  Later, in ExitStackFrame(), we will compare
//! this count with the count on exit to see if the difference exceeds
//! AllowedErrors.
//!
//! See EnterStackFrame() for the description of the parameters.
//!
RC ErrCounter::DoEnterStackFrame
(
    ModsTest *pTest,
    void *pFrameId,
    bool SaveRc
)
{
    RC rc;
    StickyRC firstRc;

    // Read the global JS variables, and read the new error count.
    //
    InitFromJs();

    // Create the new StackFrame object, and load it with the current
    // error count and default values.
    //
    StackFrame NewStackFrame;
    NewStackFrame.pFrameId = pFrameId;
    NewStackFrame.VerboseFlags = m_OuterStackFrame.VerboseFlags;
    NewStackFrame.AllowedErrors = m_AllowedErrorsPerStackFrame;
    NewStackFrame.AllowedErrorsPerMin = m_AllowedErrorsPerMinPerStackFrame;
    NewStackFrame.Disabled = m_OuterStackFrame.Disabled;
    NewStackFrame.InnerErrors.resize(m_NumAllCounts, 0);
    NewStackFrame.EnterTimeMs = Xp::GetWallTimeMS();

    // If m_Name and pTest are both set, then read test-specific JS
    // properties to override members of the stack frame.
    //
    if ((m_Name != "") && (pTest != NULL))
    {
        JSObject *pTestObj = pTest->GetJSObject();
        if (pTestObj != NULL)
        {
            string verboseFlagsProp = m_Name + "VerboseFlags";
            string allowedProp = m_Name + "Allowed";
            string allowedPerMinProp = allowedProp + "PerMin";
            string disabledProp = m_Name + "Disabled";

            OverrideFromJs(pTestObj, verboseFlagsProp, &NewStackFrame.VerboseFlags);
            OverrideFromJs(pTestObj, allowedProp, &NewStackFrame.AllowedErrors);
            OverrideFromJs(pTestObj, disabledProp, &NewStackFrame.Disabled);
            OverrideFromJs(pTestObj, allowedPerMinProp, &NewStackFrame.AllowedErrorsPerMin);
            // For EDC, we might have different tolerance for each test, which is set by -testarg
            if (m_Name == "EdcErrCount")
            {
                UINT64 testEdcTol = pTest->GetEdcTol();
                UINT64 testEdcTotalTol = pTest->GetEdcTotalTol();
                UINT64 testEdcRdTol = pTest->GetEdcRdTol();
                UINT64 testEdcWrTol = pTest->GetEdcWrTol();
                UINT64 testEdcTotalPerMinTol = pTest->GetEdcTotalPerMinTol();

                // If EDC tolerance is set by -testarg for this Test, just override the original tolerance
                // got from JS
                if (testEdcTol != UINT64_MAX)
                {
                    NewStackFrame.AllowedErrors.assign(m_NumErrCounts, testEdcTol);
                }

                // If EDC Total/Read/Write tolerance is set by -testarg for this Test,
                // Override the corresponding value
                NewStackFrame.AllowedErrors[0] = testEdcTotalTol != UINT64_MAX ? testEdcTotalTol : NewStackFrame.AllowedErrors[0];
                NewStackFrame.AllowedErrors[1] = testEdcRdTol != UINT64_MAX ? testEdcRdTol : NewStackFrame.AllowedErrors[1];
                NewStackFrame.AllowedErrors[2] = testEdcWrTol != UINT64_MAX ? testEdcWrTol : NewStackFrame.AllowedErrors[2];
                NewStackFrame.AllowedErrorsPerMin[0] = testEdcTotalPerMinTol != UINT64_MAX ? testEdcTotalPerMinTol : NewStackFrame.AllowedErrorsPerMin[0];
            }
            // Do some stuff before entering a MODS test, if needed
            CHECK_RC(RunBeforeEnterTest());
        }
    }

    // Overwrite allowed errors / allowed EPM in subclass
    OverwriteAllowedErrors(NewStackFrame.AllowedErrors.data(),
                           NewStackFrame.AllowedErrors.size());
    OverwriteAllowedEPM(NewStackFrame.AllowedErrorsPerMin.data(),
                        NewStackFrame.AllowedErrorsPerMin.size());

    firstRc = UpdateLwrrentCount((NewStackFrame.VerboseFlags &
                                  VF_ON_COUNT_CHANGE) ? true : false);
    NewStackFrame.EnterCount = m_LwrrentCount;
    NewStackFrame.LastError = m_LwrrentCount;

    // If SaveRc is set, then save the error in the new StackFrame
    // instead of returning it.
    //
    if (SaveRc)
    {
        NewStackFrame.rc = firstRc;
        firstRc.Clear();
    }

    // Push the new StackFrame on the stack for this thread.
    //
    m_Stacks[Tasker::LwrrentThread()].push_back(NewStackFrame);

    // Call the OnCheckpoint callback if the Verbose flag is set
    //
    if (NewStackFrame.VerboseFlags & VF_ON_CP)
    {
        CheckpointData Data = { &m_LwrrentCount[0],
                                &NewStackFrame.AllowedErrors[0],
                                NewStackFrame.Disabled };
        OnCheckpoint(&Data);
    }

    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Body of ExitStackFrame() for a single ErrCounter object.
//!
//! This method pops the StackFrame struct created by
//! EnterStackFrame(), and returns an error if the difference exceeds
//! AllowedErrors.
//!
//! Ideally, there should be one call to ExitStackFrame for every
//! EnterModsTest.  However, to account for error cases, it's legal to
//! call EnterModsTest without a matching ExitStackFrame.  It's *not*
//! legal to call ExitStackFrame without a corresponding
//! EnterModsTest.
//!
//! See EnterStackFrame() for the description of the parameters.
//!
RC ErrCounter::DoExitStackFrame
(
    ModsTest *pTest,
    void *pFrameId,
    bool  bCheckExitOnly
)
{
    Tasker::ThreadID threadID = Tasker::LwrrentThread();
    StickyRC firstRc;

    // Find the Stack for the corresponding thread, and pop stack
    // frames until the top-of-stack was created by the corresponding
    // EnterStackFrame().  Set pStack = NULL if we can't find the
    // corresponding EnterStackFrame().
    //
    Stack *pStack = NULL;
    bool bUpdatedLwrrentCount = false;
    if (m_Stacks.count(threadID))
    {
        pStack = &m_Stacks[threadID];

        // Update the error count(s) using the verbosity flags from the last
        // stack frame
        //
        if (!pStack->empty())
        {
            firstRc = UpdateLwrrentCount((pStack->back().VerboseFlags &
                                          VF_ON_COUNT_CHANGE) ? true : false);
            bUpdatedLwrrentCount = true;
        }

        size_t DesiredStackSize = 0;
        for (vector<StackFrame>::iterator iter = pStack->begin();
             iter != pStack->end(); ++iter)
        {
            if (iter->pFrameId == pFrameId)
            {
                DesiredStackSize = (iter - pStack->begin()) + 1;
                break;
            }
        }
        while (pStack->size() > DesiredStackSize)
        {
            firstRc = pStack->back().rc;
            PopStack(pStack);
            // If we popped the stack during a check for exit then ensure the
            // RC from the popped stack frame is not lost
            if (bCheckExitOnly && (firstRc != OK))
            {
                if (pStack->empty())
                {
                    m_OuterStackFrame.rc = firstRc;
                }
                else
                {
                    pStack->back().rc = firstRc;
                }
            }
        }
        if (pStack->empty())
        {
            pStack = NULL;
            m_Stacks.erase(threadID);
        }
        else
        {
            MASSERT(pStack->back().pFrameId == pFrameId);
        }
    }

    // If the error count(s) were not updated, update them using the verbosity
    // flags from the outer stack frame
    //
    if (!bUpdatedLwrrentCount)
    {
        firstRc = UpdateLwrrentCount((m_OuterStackFrame.VerboseFlags &
                                      VF_ON_COUNT_CHANGE) ? true : false);
    }

    // If the corresponding EnterStackFrame() wasn't found, then
    // either we were in the middle of a stack frame when this object
    // was initialized, or there's a SW error.  Assume the former the
    // first m_ExpectedExitsSansEnter times, and the latter the
    // remaining times.
    //
    if (pStack == NULL)
    {
        if (m_ExpectedExitsSansEnter == 0)
        {
            MASSERT(!"Called ExitStackFrame without EnterStackFrame");
            return RC::SOFTWARE_ERROR;
        }
        if (!bCheckExitOnly)
        {
            --m_ExpectedExitsSansEnter;
        }
        firstRc = CheckForError(NULL, NULL, bCheckExitOnly);
        return firstRc;
    }

    // Call the OnCheckpoint callback if the Verbose flag is set
    //
    if (pStack->back().VerboseFlags & VF_ON_CP)
    {
        CheckpointData Data = { &m_LwrrentCount[0],
                                &pStack->back().AllowedErrors[0],
                                pStack->back().Disabled };
        OnCheckpoint(&Data);
    }

    // Check whether the errors in this stack exceeded AllowedErrors.
    //
    firstRc = CheckForError(pStack, pTest, bCheckExitOnly);

    // Only manage the stack if we are *really* exiting
    if (!bCheckExitOnly)
    {
        // Pop the stack, and erase the stack if that was the outermost
        // frame for this thread.
        //
        PopStack(pStack);
        if (pStack->empty())
        {
            pStack = NULL;
            m_Stacks.erase(threadID);
        }

        // If we've exited all pending stacks, check whether the
        // unreported errors exceeds g_XxxAllowedTotal.
        //
        if (m_Stacks.empty())
        {
            firstRc = CheckForError(NULL, NULL, false);
        }
    }

    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Body of PrintJsProperties() for a single ErrCounter object.
//!
//! See PrintJsProperties() for the description of the parameters.
//!
void ErrCounter::DoPrintJsProperties
(
    Tee::Priority Pri,
    ModsTest *pTest
)
{
    MASSERT(pTest != NULL);
    JSObject *pTestObj = pTest->GetJSObject();

    // Abort if m_Name isn't set, in which case this object has no JS
    // properties
    //
    if (m_Name == "")
    {
        return;
    }

    // Print the properties
    //
    UINT32 VerboseFlags = m_OuterStackFrame.VerboseFlags;
    vector<UINT64> AllowedErrors = m_AllowedErrorsPerStackFrame;
    bool Disabled = m_OuterStackFrame.Disabled;

    string verboseFlagsProp = m_Name + "VerboseFlags";
    string allowedProp = m_Name + "Allowed";
    string disabledProp = m_Name + "Disabled";

    OverrideFromJs(pTestObj, verboseFlagsProp, &VerboseFlags);
    OverrideFromJs(pTestObj, allowedProp, &AllowedErrors);
    OverrideFromJs(pTestObj, disabledProp, &Disabled);

    Printf(Pri, "\t%s0x%04x\n",
            Utility::ExpandStringWithTabs(m_Name+"VerboseFlags:", 32, 8).c_str(),
            VerboseFlags);
    string whatsAllowed;
    for (UINT32 ii = 0; ii < AllowedErrors.size(); ++ii)
    {
        if (ii)
        {
            whatsAllowed += ", ";
        }
        whatsAllowed += Utility::StrPrintf("0x%llx", AllowedErrors[ii]);
    }
    Printf(Pri, "\t%s%s\n",
            Utility::ExpandStringWithTabs(m_Name+"Allowed:", 32, 8).c_str(),
            whatsAllowed.c_str());
    Printf(Pri, "\t%s%s\n",
            Utility::ExpandStringWithTabs(m_Name+"Disabled:", 32, 8).c_str(),
            Disabled ? "true" : "false");
}

//--------------------------------------------------------------------
//! \brief Call ReadErrorCount() to update m_LwrrentCount.
//!
//! Thin wrapper around ReadErrorCount() to set m_LwrrentCount.  This
//! method makes sure not to change m_LwrrentCount if ReadErrorCount
//! fails.  It also calls the OnCountChange() callback if the count
//! changed.
//!
//! \param bVerbose : true to print count changes at verbose level
//!
RC ErrCounter::UpdateLwrrentCount(bool bVerbose)
{
    RC rc;
    vector<UINT64> NewCount(m_NumAllCounts, 0);

    CHECK_RC(ReadErrorCount(&NewCount[0]));
    if (NewCount != m_LwrrentCount)
    {
        CountChangeData Data = { &NewCount[0], &m_LwrrentCount[0], bVerbose };
        OnCountChange(&Data);
        m_LwrrentCount = NewCount;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return non-OK if the new counts exceed AllowedError.
//!
//! This method checks whether the error count(s) in a Stack has
//! exceeded the AllowedError threshold, i.e. whether
//! (m_LwrrentCount[ii] - LastError[ii] - InnerErrors[ii] >
//! AllowedErrors[ii]) for any index ii.  If this condition is met,
//! and if error-checking is not disabled by the Disabled flag, then
//! this method returns the rc returned by OnError().
//!
//! This method checks the threshold at each frame in the stack, and
//! in the m_OuterStackFrame, but it only records the error and prints
//! a message if the threshold is exceeded in the top frame and
//! bCheckExitOnly is false.  Otherwise, it just returns an "advisory"
//! RC, telling the caller to abort the current operation.  Once the
//! caller has aborted the current operation and called
//! ExitStackFrame() enough times, this method should get called again
//! with the right conditions to record the error (i.e. threshold at
//! TOS, and !bCheckExitOnly).
//!
//! Note: it's occasionally possible for (m_LwrrentCount[ii] -
//! LastError[ii] - InnerErrors[ii]) to be negative.  This is because
//! m_OuterStackFrame counts errors that oclwrred in parallel threads
//! as InnerErrors, and if parallel threads can't tell which thread
//! the error happened in then they both take the blame.  So the
//! errors get counted twice.  This method counts NewErrors as 0 in
//! that case.
//!
//! This method has some important side-effects when it records an
//! error:
//!
//! - It sets pStackFrame.LastError to the current count.
//! - It clears pStackFrame.InnerErrors, since InnerErrors only keeps
//!   track of inner-frame errors *since* LastError.
//! - Write a JSON entry.
//! - It calls the OnError() callback with CheckExitOnly == false,
//!   which usually tells the subclass to print a message.
//!
//! \param pStack The stack to check for errors.  This method
//!     implicitly appends m_OuterStackFrame to the bottom of pStack.  If
//!     pStack is empty or NULL, then this method only checks
//!     m_OuterStackFrame.
//! \param pTest The pTest parameter passed to EnterStackFrame(),
//!     ExitStackFrame(), or CheckExitStackFrame().
//! \param bCheckExitOnly If true, then check for errors but don't
//!     record them or print a message.  Used by CheckExitStackFrame().
//!
RC ErrCounter::CheckForError
(
    Stack *pStack,
    ModsTest *pTest,
    bool   bCheckExitOnly
)
{
    RC rc;
    MASSERT(m_NumAllCounts == m_LwrrentCount.size());

    // If pStack is NULL, point it at an empty stack frame
    //
    Stack EmptyStack;
    if (pStack == NULL)
    {
        pStack = &EmptyStack;
    }

    // Check stack entry 0 to determine if any new errors have been reported
    // since the stack for the current frame was created.  If no new errors
    // have oclwrred since the current thread's stack frame was created then
    // simply exit.
    //
    // This prevents the following
    //    1.  Thread A starts
    //    2.  An error oclwrs in the counter
    //    3.  Thread B starts
    //    4.  Thread B has a stack frame exit
    //    5.  An error is reported in Thread B when the OuterStackFrame is
    //        checked since Thread A is still running
    //
    // The above issue happens for *every single* stack frame exit in Thread B
    // until Thread A exits completely and pops its stack onto the outer
    // stack frame
    if (pStack->size())
    {
        StackFrame *pStackFrame = &(*pStack)[0];
        if (!pStackFrame->Disabled)
        {
            bool bNewError = false;
            for (UINT32 jj = 0; (jj < m_NumErrCounts) && !bNewError; ++jj)
            {
                if (m_LwrrentCount[jj] > pStackFrame->EnterCount[jj])
                    bNewError = true;
            }
            if (!bNewError)
                return OK;
        }
    }

    // Loop through the stack from pStack->back() to pStack->front(),
    // followed by m_OuterStackFrame, checking for errors at each
    // level.
    //
    // This is done in order to report the error sooner and cause
    // MODS to exit quicker.  For instance, an error oclwrs
    // after entering stack depth 2 but before stack frame 3 is run.
    // Since errors are only checked at stack frame exit, stack frame
    // 3 is then created.  When stack frame 3 exits if only its
    // frame was checked(instead of looping through the whole stack)
    // then stack frame 2 could continually create lower stack frames
    // without any of them erroring.  This can create a *long* time
    // between an error oclwring and detection if (for instance) stack
    // frame 2 is the top level test run function and stack frame 3
    // is the level at which the individual tests are run.  In that case,
    // if an error oclwrred prior to the first test starting, MODS would
    // continue to run every test before reporting an error when stack
    // frame 2 finally exits
    //
    // InnerErrorsInStack[] holds the values that would get added to
    // pStackFrame->InnerErrors[] if we called PopStack() until
    // pStackFrame was at the top.
    //
    vector<UINT64> InnerErrorsInStack(m_NumAllCounts);
    for (UINT32 ii = 0; ii < pStack->size() + 1; ++ii)
    {
        bool HasError = false;
        StackFrame *pStackFrame = (  (ii < pStack->size())
                                   ? &(*pStack)[pStack->size() - ii - 1]
                                     : &m_OuterStackFrame );

        if (m_bIgnoreOuterFrameErrors && (pStackFrame == &m_OuterStackFrame))
            continue;

        bool DoCheckExitOnly = bCheckExitOnly || (ii > 0);

        MASSERT(m_NumErrCounts == pStackFrame->AllowedErrors.size());
        MASSERT(m_NumAllCounts == pStackFrame->LastError.size());
        MASSERT(m_NumAllCounts == pStackFrame->InnerErrors.size());

        // Check whether any element exceeds AllowedError
        //
        if (!pStackFrame->Disabled)
        {
            for (UINT32 ii = 0; ii < m_NumErrCounts; ++ii)
            {
                // If AllowedErrors == UINT64_MAX, this indicates the
                // corresponding error type should be ignored.
                if (pStackFrame->AllowedErrors[ii] == UINT64_MAX)
                {
                    continue;
                }
                if (m_LwrrentCount[ii] > (pStackFrame->LastError[ii]
                                          + pStackFrame->InnerErrors[ii]
                                          + InnerErrorsInStack[ii]
                                          + pStackFrame->AllowedErrors[ii]))
                {
                    HasError = true;
                    break;
                }
            }
        }

        // If a threshold was exceeded, then call OnError(), update
        // LastError & InnerErrors & JSON if we're recording the
        // error, and abort the loop.
        //
        if (HasError)
        {
            vector<UINT64> NewErrors = m_LwrrentCount;
            for (UINT32 ii = 0; ii < m_NumAllCounts; ++ii)
            {
                NewErrors[ii] -= min(pStackFrame->LastError[ii]
                                     + pStackFrame->InnerErrors[ii]
                                     + InnerErrorsInStack[ii],
                                     NewErrors[ii]);
            }
            ErrorData Data = { &NewErrors[0],
                               &pStackFrame->AllowedErrors[0],
                               &m_LwrrentCount[0],
                               &pStackFrame->LastError[0],
                               DoCheckExitOnly,
                               !!(pStackFrame->VerboseFlags & VF_ON_CP)};
            rc = OnError(&Data);
            MASSERT(rc != OK);

            if (!DoCheckExitOnly)
            {
                if (pTest)
                    UpdateJsonItem(pTest, &Data);

                pStackFrame->LastError = m_LwrrentCount;
                fill_n(pStackFrame->InnerErrors.begin(), m_NumAllCounts, 0);
            }

            break;
        }

        // Update InnerErrorsInStack
        //
        for (UINT32 ii = 0; ii < m_NumAllCounts; ++ii)
        {
            InnerErrorsInStack[ii] += (pStackFrame->LastError[ii] -
                                       pStackFrame->EnterCount[ii]);
            InnerErrorsInStack[ii] += pStackFrame->InnerErrors[ii];
        }
    }

    // Return
    //
    return rc;
}

//--------------------------------------------------------------------
//! Pop the top element of a Stack, and update the InnerErrors member
//! of the underlying StackFrame so that it won't report errors that
//! were already reported by an inner frame.
//!
void ErrCounter::PopStack(Stack *pStack)
{
    MASSERT(pStack->size() > 0);
    StackFrame *pTopFrame = &pStack->back();
    StackFrame *pNextFrame = ((pStack->size() > 1) ?
                              &(*pStack)[pStack->size() - 2] :
                              &m_OuterStackFrame);

    for (UINT32 ii = 0; ii < m_NumAllCounts; ++ii)
    {
        pNextFrame->InnerErrors[ii] += (pTopFrame->LastError[ii] -
                                        pTopFrame->EnterCount[ii]);
        pNextFrame->InnerErrors[ii] += pTopFrame->InnerErrors[ii];
    }
    pStack->pop_back();
}

//--------------------------------------------------------------------
// Used by ErrCounter::GetInitializedErrCounters to sort the vector
//
static bool SortErrCounters(const ErrCounter *lhs, const ErrCounter *rhs)
{
    return *lhs < *rhs;
}

//--------------------------------------------------------------------
//! \brief Return all the initialized ErrCounters, with mutexes locked.
//!
//! This static method retrieves all the lwrrently-initialized
//! ErrCounter objects that match the indicated FrameType, in a
//! thread-safe manner.  This method locks the mutexes of all the
//! returned ErrCounters, so that no other thread can call ShutDown on
//! them until the caller is done with them.  It is up to the caller
//! to unlock the mutexes.  The ErrCounter objects are sorted in
//! priority order.
//!
//! \param pTest The ModsTest that we're entering/exiting, or NULL for
//!     a JavaScript function.
//! \param[out] pErrCounters On return, this is filled with the
//!     desired counters.
//!
/* static */ void ErrCounter::GetInitializedErrCounters
(
    ModsTest *pTest,
    vector<ErrCounter*> *pErrCounters
)
{
    pErrCounters->clear();
    if (s_pInitializedCounters != NULL)
    {
        Tasker::MutexHolder lock(s_InitializedCountersMutex);
        pErrCounters->reserve(s_pInitializedCounters->size());
        for (set<ErrCounter*>::iterator ppCounter =
                 s_pInitializedCounters->begin();
             ppCounter != s_pInitializedCounters->end(); ++ppCounter)
        {
            if ((*ppCounter)->WatchInStackFrame(pTest))
            {
                Tasker::AcquireMutex((*ppCounter)->GetMutex());
                pErrCounters->push_back(*ppCounter);
            }
        }
    }
    sort(pErrCounters->begin(), pErrCounters->end(), SortErrCounters);
}

//--------------------------------------------------------------------
//! \brief Colwenience function to override a bool from a JS property.
//!
//! \param pJSObject The JSObject to get the property from.
//! \param PropertyName The name of the property.
//! \param[in,out] pBool If the property exists, overwrite *pBool with the
//!     property value.  If not, leave *pBool unchanged.
//! \return true if the property was set in JS, false if it remained
//!     at default.
//!
/* static */ bool ErrCounter::OverrideFromJs
(
    JSObject *pJSObject,
    string PropertyName,
    bool *pBool
)
{
    MASSERT(pJSObject != NULL);
    MASSERT(pBool != NULL);
    JavaScriptPtr pJs;

    bool TmpBool;
    if (OK == pJs->GetProperty(pJSObject, PropertyName.c_str(), &TmpBool))
    {
        *pBool = TmpBool;
        return true;
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Colwenience function to override a UINT32 from a JS property.
//!
//! \param pJSObject The JSObject to get the property from.
//! \param PropertyName The name of the property.
//! \param[in,out] pUint32 If the property exists, overwrite *pUint32 with the
//!     property value.  If not, leave *pUint32 unchanged.
//! \return true if the property was set in JS, false if it remained
//!     at default.
//!
/* static */ bool ErrCounter::OverrideFromJs
(
    JSObject *pJSObject,
    string PropertyName,
    UINT32 *pUint32
)
{
    MASSERT(pJSObject != NULL);
    MASSERT(pUint32 != NULL);
    JavaScriptPtr pJs;

    UINT32 TmpUint32;
    if (OK == pJs->GetProperty(pJSObject, PropertyName.c_str(), &TmpUint32))
    {
        *pUint32 = TmpUint32;
        return true;
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Colwenience function to override a vector<UINT64> from a JS property.
//!
//! The JS property may be a scalar or array.  If it's a scalar, then
//! the scalar is copied into each element of the output array.  If
//! it's an array, then the property is truncated or repeated to make
//! it the same size as the output array, and then copied to the
//! output array.
//!
//! \param pJSObject The JSObject to get the property from.
//! \param PropertyName The name of the property.
//! \param[in,out] pArray If the property exists, overwrite *pArray
//!     with the property value as described above.  If not, leave
//!     *pArray unchanged.  Either way, pArray->size() is not changed.
//! \return true if the property was set in JS, false if it remained
//!     at default.
//!
/* static */ bool ErrCounter::OverrideFromJs
(
    JSObject *pJSObject,
    string PropertyName,
    vector<UINT64> *pArray
)
{
    MASSERT(pJSObject != NULL);
    MASSERT(pArray != NULL);
    JavaScriptPtr pJs;

    // Read the property into a jsval.  Abort on error.
    //
    jsval TmpJsval;
    if (OK != pJs->GetPropertyJsval(pJSObject, PropertyName.c_str(), &TmpJsval))
    {
        return false;
    }

    // If the jsval is a number, then fill the array with the number
    // and return.  Otherwise, try to read it as an array.
    //
    if (JSVAL_IS_NUMBER(TmpJsval))
    {
        UINT64 TmpNumber;
        if (OK == pJs->FromJsval(TmpJsval, &TmpNumber))
        {
            fill(pArray->begin(), pArray->end(), TmpNumber);
            return true;
        }
    }
    else
    {
        JsArray TmpJsArray;
        if (OK == pJs->FromJsval(TmpJsval, &TmpJsArray) &&
            !TmpJsArray.empty())
        {
            // Colwert the JsArray to C++ UINT64's.  If the JsArray is
            // shorter than pArray, then wrap around as needed.
            // Abort if there are any errors.
            //
            vector<UINT64> TmpArray(pArray->size());
            for (UINT32 ii = 0; ii < TmpArray.size(); ++ii)
            {
                if (OK != pJs->FromJsval(TmpJsArray[ii % TmpJsArray.size()],
                                         &TmpArray[ii]))
                {
                    return false;
                }
            }

            // If we get this far, then TmpArray has the values we
            // want.  Copy it to *pArray.
            //
            *pArray = TmpArray;
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Get the stack depth at initialization time
//!
//! This method is called at initialization time to predict how many
//! times we should expect to get an ExitStackFrame() without a
//! corresponding EnterStackFrame().  The idea is that we may already
//! be several stack frames deep when Initialize() is called.  Used to
//! set m_ExpectedExitsSansEnter.
//!
//! This method assumes that ErrCounter objects are initialized near
//! the start of mods, before any tests have started and before we've
//! launched JavaScript threads.
//!
UINT32 ErrCounter::GetStackDepth(FrameType Coverage)
{
    switch (Coverage)
    {
        case MODS_TEST:
        {
            // Assume we haven't started any tests yet, so the depth
            // is 0.
            //
            return 0;
        }
        case JS_FUNCTION:
        {
            // Assume that we haven't launched a bunch of other
            // JavaScript threads yet, so the depth is the number of
            // stack frames in the global context.
            //
            JSContext *pJSContext = NULL;
            if (JavaScriptPtr()->GetContext(&pJSContext) != OK)
            {
                MASSERT(!"Cannot get JS context in ErrCounter::GetStackDepth");
                return 0;
            }

            JSStackFrame *pFrame = NULL;
            UINT32 StackDepth = 0;
            while (JS_FrameIterator(pJSContext, &pFrame) != NULL)
            {
                ++StackDepth;
            }
            return StackDepth;
        }
        default:
        {
            MASSERT(!"Illegal case in GetStackDepth");
            return 0;
        }
    }
}

//--------------------------------------------------------------------
//! \brief Add err-count record to a ModsTest's "exit" JSON record.
//!
/*virtual*/ void ErrCounter::UpdateJsonItem
(
    ModsTest* pModsTest,
    ErrorData * pData
)
{
    if (m_Name == "")
        return;

    MASSERT(pModsTest);
    MASSERT(pData);

    JsArray jsa;
    JavaScriptPtr pJs;

    for (UINT32 ii = 0; ii < m_NumAllCounts; ++ii)
    {
        const double newCount = static_cast<double>(
                pData->pNewCount[ii] - pData->pOldCount[ii]);
        jsval jsv;
        pJs->ToJsval(newCount, &jsv);
        jsa.push_back(jsv);
    }
    pModsTest->GetJsonExit()->SetField(m_Name.c_str(), &jsa);
}

RC ErrCounter::RunBeforeEnterTest()
{
    // For ErrCounter class, we lwrrently have nothing to do
    // The subclass would override this function to do something
    return RC::OK;
}
