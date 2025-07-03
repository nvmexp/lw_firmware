/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation. All rights
 * reserved. All information contained herein is proprietary and confidential to
 * LWPU Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/cpu.h"
#include "core/include/help.h"
#include "modstest.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/massert.h"
#include "core/include/mle_protobuf.h"
#include "core/include/tee.h"
#include "core/include/errcount.h"
#include "core/include/jsonlog.h"
#include "core/include/errormap.h"
#include "core/utility/errloggr.h"
#include "core/include/log.h"
#include "core/include/xp.h"
#include "core/include/platform.h"

// class static variables

Callbacks ModsTest::s_CallbackList[NUM_CALLBACK_TYPE];

INT32 ModsTest::s_DoneSetup(0);
INT32 ModsTest::s_DoneRun(0);
UINT32 ModsTest::s_HeartBeatPeriodSec(0);

//------------------------------------------------------------------------------
ModsTest::ModsTest()
  : m_pPickers(NULL)
    ,m_SetupDone(false)
    ,m_SetupIlwoked(false)
    ,m_SetupSyncRequired(false)
    ,m_DeviceSync(false)
    ,m_pJsonEnter(NULL)
    ,m_pJsonExit(NULL)
    ,m_EdcTol(UINT64_MAX)
    ,m_EdcTotalTol(UINT64_MAX)
    ,m_EdcRdTol(UINT64_MAX)
    ,m_EdcWrTol(UINT64_MAX)
    ,m_EdcTotalPerMinTol(UINT64_MAX)
    ,m_IgnoreErrCountsPreRun(false)
    ,m_MainErrCounterDummy('\0')
    ,m_RunErrCounterDummy('\0')
{
    m_FpCtx.LoopNum = 0;
    m_FpCtx.RestartNum = 0;
    m_FpCtx.pJSObj = 0;
}

//------------------------------------------------------------------------------
ModsTest::~ModsTest()
{
    if (m_pPickers)
        delete m_pPickers;
    m_pPickers = NULL;

    // If m_SetupIlwoked is true at destruction, someone failed to call Cleanup.
    MASSERT(!m_SetupIlwoked);

    // Don't explicitly delete our JsonItems.
    // The JavaScript garbage collector will call C_JsonItem_finalize when the
    // test's JSObject is freed, which will indirectly call the C++ destructor
    // for the JsonItem.
}

static RC SyncDevices()
{
    if (Tasker::GetGroupSize() == Tasker::GetNumThreadsAliveInGroup())
    {
        Tasker::WaitOnBarrier();
        return OK;
    }
    else
    {
        return RC::USER_ABORTED_SCRIPT;
    }
}

//------------------------------------------------------------------------------
bool ModsTest::IsSupportedWrapper()
{
    CallbackArguments args;
    bool vfCheck = (Platform::IsVirtFunMode()) ? IsSupportedVf() : true;

    return vfCheck && IsSupported() &&
           FireCallbacksBool(IS_SUPPORTED, Callbacks::STOP_ON_ERROR, args, nullptr);
}

//------------------------------------------------------------------------------
RC ModsTest::SetupWrapper()
{
    PROFILE_ZONE(GENERIC)
#ifdef TRACY_ENABLE
    const string zoneName = GetName() + " (Setup)";
    PROFILE_ZONE_SET_NAME(zoneName.data(), zoneName.size())
#endif

    RC rc;
    CallbackArguments args;
    char dummy;   // Dummy var to provide a unique "frame ID" for ErrCounter

    if (m_SetupIlwoked)
    {
        CHECK_RC(CleanupWrapper());
    }

    // Set regardless of Setup success or failure
    m_SetupIlwoked = true;

    if (!m_IgnoreErrCountsPreRun)
    {
        CHECK_RC(ErrCounter::EnterStackFrame(this, &dummy, false));
    }

    CHECK_RC(FireCallbacks(PRE_SETUP, Callbacks::STOP_ON_ERROR,
                           args, "Pre-Setup"));
    if (m_DeviceSync)
    {
        CHECK_RC(SyncDevices());
    }
    CHECK_RC(Setup());
    CHECK_RC(ErrorLogger::FlushLogBuffer());
    if (!m_IgnoreErrCountsPreRun)
    {
        CHECK_RC(ErrCounter::ExitStackFrame(this, &dummy, NULL));
    }

    m_SetupDone = true;
    Cpu::AtomicAdd(&s_DoneSetup, 1);

    return OK;
}

//------------------------------------------------------------------------------
/*virtual*/
RC ModsTest::SetupJsonItems()
{
    MASSERT(m_pJsonEnter && m_pJsonExit);
    m_pJsonEnter->SetTag(ENCJSENT("enter"));
    m_pJsonEnter->SetLwrDateTimeField(ENCJSENT("datetime"));
    m_pJsonEnter->SetUniqueIntField(ENCJSENT("test_seq"));
    m_pJsonEnter->SetField(ENCJSENT("tnum"), ErrorMap::Test());
    m_pJsonEnter->SetCategory(JsonItem::JSONLOG_TESTENTRY);
    m_pJsonEnter->JsonLimitedSetAllowedField("tnum");
    m_pJsonEnter->JsonLimitedSetAllowedField("dev_insts");

    m_pJsonExit->SetTag(ENCJSENT("exit"));
    m_pJsonExit->RemoveAllFields();
    m_pJsonExit->CopyAllFields(*m_pJsonEnter);
    m_pJsonExit->SetCategory(JsonItem::JSONLOG_TESTEXIT);
    m_pJsonExit->JsonLimitedSetAllowedField("tnum");
    m_pJsonExit->JsonLimitedSetAllowedField("dev_insts");
    m_pJsonExit->JsonLimitedSetAllowedField("rc");

    JavaScriptPtr pJs;
    jsval value;
    if (pJs->GetPropertyJsval(GetJSObject(), "ParentNumber", &value) == RC::OK)
    {
        INT32 parentNumber = -1;
        if (JSVAL_IS_NUMBER(value))
        {
            if (pJs->FromJsval(value, &parentNumber) != RC::OK)
            {
                parentNumber = -1;
            }
        }
        m_pJsonEnter->SetField(ENCJSENT("parent_num"), parentNumber);
        m_pJsonExit->SetField(ENCJSENT("parent_num"), parentNumber);
    }

    return OK;
}

//------------------------------------------------------------------------------
RC ModsTest::RunWrapper()
{
    PROFILE_ZONE(GENERIC)
    PROFILE_ZONE_SET_NAME(GetName().data(), GetName().size())

    RunWrapperContext ctx;

    RunWrapperFirstHalf(&ctx);

    if (m_DeviceSync)
    {
        ctx.FirstRc = SyncDevices();
    }

    if (OK == ctx.FirstRc)
    {
        ctx.FirstRc = Run();
        Cpu::AtomicAdd(&s_DoneRun, 1);
    }

    RunWrapperSecondHalf (&ctx);

    return ctx.FirstRc;
}

void ModsTest::RunWrapperFirstHalf(ModsTest::RunWrapperContext * pCtx)
{
    const char * msg = "";

    // If errCountSaveRc is true, any errors in EnterStackFrame will be held until
    // the corresponding ExitStackFrame calls in RunWrapperSecondHalf. This allows
    // a test to run when errors are detected before Run.
    const bool errCountSaveRc = !m_SetupDone && m_IgnoreErrCountsPreRun;

    {
        msg = "Main ErrCounter::EnterStackFrame";
        pCtx->DidMainErrCounterEnter = true;
        pCtx->FirstRc = ErrCounter::EnterStackFrame(this, GetMainFrameId(), errCountSaveRc);
    }
    if (OK == pCtx->FirstRc)
    {
        msg = "SetupJsonItems";
        pCtx->FirstRc = SetupJsonItems();
    }
    if (OK == pCtx->FirstRc)
    {
        msg = "enter JsonItem";
        pCtx->DidLogEnter = true;
        pCtx->FirstRc = m_pJsonEnter->WriteToLogfile();
    }
    if ((OK == pCtx->FirstRc) && !m_SetupDone)
    {
        msg = "Setup";
        pCtx->DidSetup = true;
        pCtx->FirstRc = SetupWrapper();
    }
    if (OK == pCtx->FirstRc)
    {
        msg = "Run ErrCounter::EnterStackFrame";
        pCtx->DidRunErrCounterEnter = true;
        pCtx->FirstRc = ErrCounter::EnterStackFrame(this, GetRunFrameId(), errCountSaveRc);
    }
    if (OK == pCtx->FirstRc)
    {
        msg = "PRE_RUN callbacks";
        pCtx->DidPreRun = true;
        pCtx->FirstRc = FireCallbacks(PRE_RUN, Callbacks::STOP_ON_ERROR,
                                      CallbackArguments(), "Pre-Run");
    }
    if (OK != pCtx->FirstRc)
    {
        Printf(Tee::PriNormal, "%s %s failed.\n", GetName().c_str(), msg);
    }
}

void ModsTest::RunWrapperSecondHalf(ModsTest::RunWrapperContext * pCtx)
{
    StickyRC SecondHalfRc;
    const char * msg = "";

    if (pCtx->DidPreRun)
    {
        msg = "POST_RUN callbacks";
        CallbackArguments args;
        args.Push(pCtx->FirstRc);
        SecondHalfRc = FireCallbacks(POST_RUN, Callbacks::RUN_ON_ERROR,
                                     args, "Post-Run");
    }
    // If we successfully entered ErrCount stackframe in Run wrapper first half,
    // check for errors, then exit stackframe.
    if (pCtx->DidRunErrCounterEnter)
    {
        // Since test is done running, call CheckEPMAllCounters to check if
        // (number of error produced during test run) / (test run time)
        // doesn't exceed tolerance
        bool overrideTestRc = false;
        RC tmpRc = ErrCounter::CheckEPMAllCounters(this, GetRunFrameId(), &overrideTestRc);
        if (overrideTestRc)
        {
            pCtx->FirstRc.Clear();
            SecondHalfRc.Clear();
        }
        if (RC::OK == SecondHalfRc)
        {
            msg = "Run ErrCounter::CheckPerMinErr";
        }
        SecondHalfRc = tmpRc;

        // Check for errors and pop the stack frame we pushed in RunWrapperFirstHalf
        tmpRc = ErrCounter::ExitStackFrame(this, GetRunFrameId(), &overrideTestRc);
        if (overrideTestRc)
        {
            pCtx->FirstRc.Clear();
            SecondHalfRc.Clear();
        }
        if (RC::OK == SecondHalfRc)
        {
            msg = "Run ErrCounter::ExitStackFrame";
        }
        SecondHalfRc = tmpRc;
    }
    if (pCtx->DidSetup)
    {
        if (OK == SecondHalfRc)
            msg = "Cleanup";

        // Send heartbeat once before entering the cleanup where we unregister
        if (ErrorMap::IsTestProgressEnabled() && ErrorMap::IsTestProgressSupported())
        {
            HeartBeatMonitor::SendUpdate(ErrorMap::HeartBeatRegistrationId());
        }
        SecondHalfRc = CleanupWrapper();
    }
    if (pCtx->DidMainErrCounterEnter)
    {
        bool overrideTestRc = false;
        RC tmpRc = ErrCounter::ExitStackFrame(this, GetMainFrameId(), &overrideTestRc);
        if (overrideTestRc)
        {
            pCtx->FirstRc.Clear();
            SecondHalfRc.Clear();
        }
        if (RC::OK == SecondHalfRc)
        {
            msg = "Main ErrCounter::ExitStackFrame";
        }
        SecondHalfRc = tmpRc;
    }

    {
        if (OK == SecondHalfRc)
            msg = "ErrorLogger";
        SecondHalfRc = ErrorLogger::FlushLogBuffer();
    }
    if (pCtx->DidLogEnter)
    {
        if (OK == SecondHalfRc)
            msg = "exit JsonItem";
        m_pJsonExit->SetLwrDateTimeField(ENCJSENT("datetime"));
        ErrorMap em(pCtx->FirstRc);
        m_pJsonExit->SetField(ENCJSENT("rc"), UINT32(em.Code()));
        SecondHalfRc = m_pJsonExit->WriteToLogfile();
    }

    if (OK != SecondHalfRc)
    {
        Printf(Tee::PriNormal, "%s %s failed.\n", GetName().c_str(), msg);
        pCtx->FirstRc = SecondHalfRc;
    }
    Log::InsertErrorToHistory(pCtx->FirstRc);
}

//------------------------------------------------------------------------------
RC ModsTest::CleanupWrapper()
{
    PROFILE_ZONE(GENERIC)
#ifdef TRACY_ENABLE
    const string zoneName = GetName() + " (Cleanup)";
    PROFILE_ZONE_SET_NAME(zoneName.data(), zoneName.size())
#endif

    StickyRC firstRc;
    RC rc;
    CallbackArguments args;
    char dummy;   // Dummy var to provide a unique "frame ID" for ErrCounter

    if (!m_SetupIlwoked)
    {
        return OK;
    }

    if (ErrorMap::IsTestProgressEnabled() && ErrorMap::IsTestProgressSupported())
    {
        CHECK_RC(ErrorMap::EndTestProgress());
    }

    m_SetupIlwoked = false;
    m_SetupDone = false;

    firstRc = ErrCounter::EnterStackFrame(this, &dummy, false);
    if (m_DeviceSync)
    {
        firstRc = SyncDevices();
    }
    firstRc = Cleanup();
    if (OK != firstRc)
    {
        Printf(Tee::PriNormal, "Warning: %s.Cleanup failed (%s).\n",
               GetName().c_str(), firstRc.Message());
    }

    args.Push(firstRc);
    firstRc = FireCallbacks(POST_CLEANUP, Callbacks::RUN_ON_ERROR,
                            args, "Post-Cleanup");
    firstRc = ErrorLogger::FlushLogBuffer();
    bool overrideTestRc;
    RC tmpRc = ErrCounter::ExitStackFrame(this, &dummy, &overrideTestRc);
    if (overrideTestRc)
        firstRc.Clear();
    firstRc = tmpRc;

    return firstRc;
}

//! \brief Allow subclasses to access the JS context
//!
//------------------------------------------------------------------------------
/* virtual */ JSObject * ModsTest::GetJSObject()
{
    return m_FpCtx.pJSObj;
}

//-----------------------------------------------------------------------------
/* virtual */ RC ModsTest::SetJavaScriptObject(JSContext *cx, JSObject *obj)
{
    // Note: called from the JS_CLASS_INHERIT_FULL macro (JS constructor).

    // Give the FancyPickers a pointer to the JS object, so that any
    // "js" style picker functions will have a valid "this" object.
    m_FpCtx.pJSObj = obj;

    // Create "this.JsonEnter" and "JsonExit" properties.
    m_pJsonEnter = new JsonItem(cx, obj, ENCJSENT("JsonEnter"));
    m_pJsonExit  = new JsonItem(cx, obj, ENCJSENT("JsonExit"));

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC ModsTest::InitFromJs()
{
    return OK;
}

//-----------------------------------------------------------------------------
RC ModsTest::TestHelpWrapper(JSContext *pContext)
{
    // Compile the regular expression
    regex compiledRegExp = regex(".*",
        regex_constants::extended | regex_constants::nosubs | regex_constants::icase);

    TestHelp(pContext, &compiledRegExp);
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ void ModsTest::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "ModsTest Js Properties:\n");
    Printf(pri, "\tTest name:\t\t\t%s\n", GetName().c_str());
    Printf(pri, "\tIgnoreErrCountsPreRun:\t\t%s\n", m_IgnoreErrCountsPreRun ? "true" : "false");
    ErrCounter::PrintJsProperties(pri, this);
}

//-----------------------------------------------------------------------------
/* virtual */ void ModsTest::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
}

//-----------------------------------------------------------------------------
RC ModsTest::CreateFancyPickerArray(UINT32 NumPickers)
{
    RC rc;

    // CreateFancyPickerArray should only be called once per ModsTest
    MASSERT(m_pPickers == NULL);

    m_pPickers = new FancyPickerArray(NumPickers);
    MASSERT(m_pPickers);

    m_pPickers->SetContext(&m_FpCtx);

    if ((rc = SetDefaultPickers(0, NumPickers - 1)) != OK)
    {
        Printf(Tee::PriNormal,
                "There was a problem creating FancyPickerArray.\n");
        delete m_pPickers;
        m_pPickers = NULL;

        return rc;
    }
    else
    {
        // SW sanity check: is there a valid default for each picker?
        // If not assert in debug builds, set a safe garbage default.
        m_pPickers->CheckInitialized();
    }

    return OK;
}

//-----------------------------------------------------------------------------
FancyPickerArray * ModsTest::GetFancyPickerArray()
{
    // CreateFancyPickerArray should be called before this function
    MASSERT(m_pPickers != NULL);

    return m_pPickers;
}

//-----------------------------------------------------------------------------
FancyPicker::FpContext * ModsTest::GetFpContext()
{
    return &m_FpCtx;
}

//------------------------------------------------------------------------------
/* virtual */ RC ModsTest::SetDefaultsForPicker(UINT32 idx)
{
    Printf(Tee::PriNormal, "This test does not lwrrently implement FancyPickerArrays.\n");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
/* virtual */ RC ModsTest::SetDefaultPickers(UINT32 first, UINT32 last)
{
    RC rc;

    if (last >= GetNumPickers())
        last = GetNumPickers() - 1;

    if (first > last)
        return RC::SOFTWARE_ERROR;

    for(UINT32 i = first; i <= last; ++i)
    {
        if (OK != (rc = SetDefaultsForPicker(i)))
        {
            Printf(Tee::PriNormal, "%s: No default FancyPicker setup "
                                   "for index %d\n", m_Name.c_str(), i);
            return rc;
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 ModsTest::GetPick(UINT32 idx)
{
    if (idx >= GetNumPickers())
        return 0;

    return (*m_pPickers)[idx].GetPick();
}
/* virtual */ double ModsTest::FGetPick(UINT32 idx)
{
    if (idx >= GetNumPickers())
        return 0;

    return (*m_pPickers)[idx].FGetPick();
}

//-----------------------------------------------------------------------------
/* virtual */ RC ModsTest::GetPicker(UINT32 idx, jsval *val)
{
    RC rc;

    CHECK_RC(m_pPickers->PickerToJsval(idx, val));

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC ModsTest::SetPicker(UINT32 idx, jsval v)
{
    RC rc = m_pPickers->PickerFromJsval(idx, v);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s syntax error for picker %d\n",
                __FUNCTION__, idx);
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 ModsTest::GetLoop() const
{
    return m_FpCtx.LoopNum;
}

//------------------------------------------------------------------------------
Callbacks *ModsTest::GetCallbacks(CallbackIdx id)
{
    MASSERT(id >= 0 && id < NUM_CALLBACK_TYPE);
    return &m_CallbackList[id];
}

//------------------------------------------------------------------------------
Callbacks *ModsTest::GetGlobalCallbacks(CallbackIdx id)
{
    MASSERT(id >= 0 && id < NUM_CALLBACK_TYPE);
    return &s_CallbackList[id];
}

//------------------------------------------------------------------------------
RC ModsTest::FireCallbacks
(
    CallbackIdx id,
    Callbacks::Flags flags,
    const CallbackArguments &args,
    const char *displayName
)
{
    RC rc;
    if ((OK != (rc = GetCallbacks(id)->Fire(flags, args))) ||
        (OK != (rc = GetGlobalCallbacks(id)->Fire(flags, args))))
    {
        if (NULL != displayName)
        {
            Printf(Tee::PriHigh, "%s \"%s\" callback failed (%s).\n",
                   GetName().c_str(), displayName, rc.Message());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
bool ModsTest::FireCallbacksBool
(
    CallbackIdx id,
    Callbacks::Flags flags,
    const CallbackArguments &args,
    const char *displayName
)
{
    if (!GetCallbacks(id)->FireBool(flags, args) ||
        !GetGlobalCallbacks(id)->FireBool(flags, args))
    {
        if (NULL != displayName)
        {
            Printf(Tee::PriHigh, "%s \"%s\" callback failed.\n",
                   GetName().c_str(), displayName);
        }
        return false;
    }
    return true;
}

JsonItem * ModsTest::GetJsonEnter()
{
    return m_pJsonEnter;
}
JsonItem * ModsTest::GetJsonExit()
{
    return m_pJsonExit;
}

//-----------------------------------------------------------------------------
//Writes a (Performance or Coverage) metric to the performance regression checker's datalog
RC ModsTest::SetMetric(const char* metric, const char* name, FLOAT64 value)
{
    StickyRC rc;
    JSObject * testObj = GetJSObject();
    JavaScriptPtr pJs;
    JSObject * jsProp;
    //Create Coverage object if it does not exist yet.
    if ((OK != pJs->GetProperty(testObj, metric, &jsProp)) || (!jsProp))
    {
        rc = pJs->DefineProperty(testObj, metric, &jsProp);
    }

    if (jsProp)
    {
        rc = pJs->SetProperty(jsProp, name, value);

        //Write to the JSON ZONE!!
        string pfname = "pf_" + string(name);
        GetJsonExit()->SetField(pfname.c_str(), value);
    }
    Printf(Tee::MleOnly, "TestMetric: [%s, %s, %f]\n", metric, name, value);

    return rc;
}

//-----------------------------------------------------------------------------
//Writes a performance metric to the regression checker's datalog
RC ModsTest::SetPerformanceMetric(const char* name, FLOAT64 value)
{
    return (SetMetric("Performance", name, value));
}

//-----------------------------------------------------------------------------
//Writes a coverage metric to the regression checker's datalog
RC ModsTest::SetCoverageMetric(const char* name, FLOAT64 value)
{
    return (SetMetric("Coverage", name, value));
}

//-----------------------------------------------------------------------------
//! Perform any end of loop processing that may need to occur
RC ModsTest::EndLoop()
{
    bool bAbortAllTests = false;
    JavaScriptPtr pJs;

    if ((pJs->GetProperty(pJs->GetGlobalObject(),
                          "g_AbortAllTests",
                          &bAbortAllTests) == OK) &&
         bAbortAllTests)
    {
        return RC::USER_ABORTED_SCRIPT;
    }

    return OK;
}

// Initialize test progress for this test and provide a description
// for what progress X / Y indicates (e.g. "loops" or "frames")
RC ModsTest::PrintProgressInit
(
    UINT64 maxIterations,
    const string& progressDesc
)
{
    if (!ErrorMap::IsTestProgressEnabled())
    {
        return OK;
    }

    ErrorMap::InitTestProgress(maxIterations, s_HeartBeatPeriodSec);

    Mle::Print(Mle::Entry::test_info)
        .max_progress(maxIterations)
        .progress_desc(progressDesc);
    return OK;
}

RC ModsTest::PrintProgressUpdate
(
    UINT64 lwrrentIteration
)
{
    if (!ErrorMap::IsTestProgressEnabled())
    {
        return OK;
    }

    UINT64 maxIterations = ErrorMap::TestProgressMax();
    if (lwrrentIteration > maxIterations)
    {
        Printf(Tee::PriError, "Current progress iteration %llu is greater than max: %llu!\n", lwrrentIteration, maxIterations);
        MASSERT(!"PrintProgressUpdate error!");
        return RC::BAD_PARAMETER;
    }

    const UINT64 lastProgress = ErrorMap::TestProgress();
    ErrorMap::SetTestProgress(lwrrentIteration);

    UINT64 lastUpdateMs = ErrorMap::TestProgressLastUpdateMs();
    UINT64 lwrTimeMs = Xp::GetWallTimeMS();

    // Only print the progress update immediately to mle log if we haven't printed
    // for some minimum time, or if we're on the last iteration
    // Don't bother printing if we're somehow at the same progress as the last print
    if (lwrrentIteration != lastProgress &&
        (lwrrentIteration == maxIterations ||
         lwrTimeMs - lastUpdateMs >= ErrorMap::MinTestProgressIntervalMs()))
    {
        Mle::Print(Mle::Entry::progress_update).EmitValue(lwrrentIteration);
        ErrorMap::SetTestProgressLastUpdateMs(lwrTimeMs);
    }
    return OK;
}

RC ModsTest::PrintErrorUpdate
(
    UINT64 lwrrentIteration,
    RC errorRc
)
{
    if (!ErrorMap::IsTestProgressEnabled())
    {
        return OK;
    }

    UINT64 maxIterations = ErrorMap::TestProgressMax();
    // If our current progress exceeds our "Max" progress,
    // print a warning, but otherwise, let it go
    if (lwrrentIteration > maxIterations)
    {
        Printf(Tee::PriWarn,
            "Current progress iteration %llu is greater than max: %llu!\n",
            lwrrentIteration, maxIterations);
    }

    ErrorMap::SetTestProgress(lwrrentIteration);

    UINT64 lwrTimeMs = Xp::GetWallTimeMS();

    ByteStream bs;
    Mle::Entry::progress_update(&bs).EmitValue(lwrrentIteration);
    Mle::Entry::test_info(&bs).rc(errorRc.Get());
    Tee::PrintMle(&bs);
    ErrorMap::SetTestProgressLastUpdateMs(lwrTimeMs);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Identify the type of ModsTest subclass
//!
/* virtual */ bool ModsTest::IsTestType(TestType tt)
{
    return (tt == MODS_TEST);
}

//-----------------------------------------------------------------------------
//! \brief Enforce EDC arg mutual exclusivity then set args
//!
RC ModsTest::CheckAndSetEdcArg(UINT64 *pEdcArg, UINT64 val, bool shouldOverrideDefaultTol)
{
    RC rc;

    JavaScriptPtr pJs;

    // If g_EdcOverrideDefaultTol has already been set and it is true, that means
    // either Replay Retry tolerance or EDC errors per minute tolerance arguments
    // have been set. If it is false, one of the other default arguments have been
    // used. 
    bool lwrrOverrideSetting = false;
    if (RC::OK == pJs->GetProperty(pJs->GetGlobalObject(), "g_EdcOverrideDefaultTol", &lwrrOverrideSetting))
    {
        if (lwrrOverrideSetting != shouldOverrideDefaultTol)
        {
            Printf(Tee::PriWarn, "EDC replay retry tol and EDC errors per minutes "
                                 "set with other EDC tolerance arguments. Overriding "
                                 "default tolerance values.\n");
            shouldOverrideDefaultTol = true;
        }
    }
    CHECK_RC(pJs->SetProperty(pJs->GetGlobalObject(), "g_EdcOverrideDefaultTol", shouldOverrideDefaultTol));
    
    MASSERT(pEdcArg);
    *pEdcArg = val;

    return RC::OK;
}

RC ModsTest::SetEdcTol(UINT64 val)            { return CheckAndSetEdcArg(&m_EdcTol,            val, false); }
RC ModsTest::SetEdcTotalTol(UINT64 val)       { return CheckAndSetEdcArg(&m_EdcTotalTol,       val, false); }
RC ModsTest::SetEdcRdTol(UINT64 val)          { return CheckAndSetEdcArg(&m_EdcRdTol,          val, false); }
RC ModsTest::SetEdcWrTol(UINT64 val)          { return CheckAndSetEdcArg(&m_EdcWrTol,          val, false); }
RC ModsTest::SetEdcTotalPerMinTol(UINT64 val) { return CheckAndSetEdcArg(&m_EdcTotalPerMinTol, val, true);  }

//------------------------------------------------------------------------------
// JS Class, and the one object, which is used as a prototype object for other
// classes.
//
JS_CLASS(ModsTest);

//-----------------------------------------------------------------------------
/* virtual */ void ModsTest::PropertyHelp(JSContext *pCtx, regex *pRegex)
{
    JSObject *pLwrJObj = JS_NewObject(pCtx,&ModsTestClass,0,0);
    JS_SetPrivate(pCtx,pLwrJObj,this);
    Help::JsPropertyHelp(pLwrJObj,pCtx,pRegex,0);
    JS_SetPrivate(pCtx,pLwrJObj,nullptr);
}

// Make ModsTest_Object global so that other files can see it
SObject ModsTest_Object
(
    "ModsTest",
    ModsTestClass,
    0,
    0,
    "A MODS test, with Setup, Run, and Cleanup methods.",
    false // Don't add a default name property: this is a base class
);

//------------------------------------------------------------------------------
C_(ModsTest_Setup);
static STest ModsTest_Setup
(
    ModsTest_Object,
    "Setup",
    C_ModsTest_Setup,
    0,
    "Initialize the test."
);

C_(ModsTest_Run);
static STest ModsTest_Run
(
    ModsTest_Object,
    "Run",
    C_ModsTest_Run,
    0,
    "Run the test."
);

C_(ModsTest_Cleanup);
static STest ModsTest_Cleanup
(
    ModsTest_Object,
    "Cleanup",
    C_ModsTest_Cleanup,
    0,
    "Shut down/clean up the test."
);

namespace
{
    void EnsureTestNumberIsSet(JSContext* pContext, JSObject* pObject)
    {
        // Set test number from JS object if it hasn't been set already,
        // this is necessary for test progress monitoring.
        if (ErrorMap::Test() == -1)
        {
            INT32 defaultTestNum = 900;

            JavaScriptPtr()->GetProperty(pObject, "Number", &defaultTestNum);

            ErrorMap::SetTest(defaultTestNum);
        }
    }
}

//------------------------------------------------------------------------------
C_(ModsTest_Setup)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    EnsureTestNumberIsSet(pContext, pObject);

    ModsTest *pTest = (ModsTest *)JS_GetPrivate(pContext, pObject);
    RC rc = pTest->SetupWrapper();
    Log::InsertErrorToHistory(rc);
    RETURN_RC(rc);
}

C_(ModsTest_Run)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    EnsureTestNumberIsSet(pContext, pObject);

    ModsTest *pTest = (ModsTest *)JS_GetPrivate(pContext, pObject);
    RETURN_RC(pTest->RunWrapper());
}

C_(ModsTest_Cleanup)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    ModsTest *pTest = (ModsTest *)JS_GetPrivate(pContext, pObject);

    RC rc = pTest->CleanupWrapper();
    Log::InsertErrorToHistory(rc);
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(ModsTest,
                SetPicker,
                2,
                "Override a FancyPicker")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = 0;
    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: ModsTest.SetPicker (idx, val)");
        return JS_FALSE;
    }
    UINT32 value1;
    jsval value2;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &value1))
    {
        JS_ReportError(pContext, "Invalid value for idx");
        return JS_FALSE;
    }
    value2 = pArguments[1];

    ModsTest *pTest;
    if ((pTest = JS_GET_PRIVATE(ModsTest, pContext, pObject, NULL)) != NULL)
    {
        RETURN_RC(pTest->SetPicker(value1, value2));
    }

    return JS_FALSE;
}

JS_SMETHOD_BIND_ONE_ARG
(
    ModsTest,
    GetPick,
    idx,
    UINT32,
    "Get current value of picker"
);

JS_SMETHOD_BIND_ONE_ARG
(
    ModsTest,
    FGetPick,
    idx,
    UINT32,
    "Get current value of picker"
);

JS_STEST_BIND_TWO_ARGS
(
    ModsTest,
    SetDefaultPickers,
    firstIdx,
    UINT32,
    lastIdx,
    UINT32,
    "Set a range of pickers to defaults"
);

JS_SMETHOD_LWSTOM(ModsTest, SetDefaultPicker, 2,
        "Set element(s) in FancyPicker array to default values.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    ModsTest * pObj;
    if ((pObj = JS_GET_PRIVATE(ModsTest, pContext, pObject, NULL)) != 0)
    {

        JavaScriptPtr pJavaScript;
        UINT32 first;
        UINT32 last;

        switch (NumArguments)
        {
            case 0:
                first = 0;
                last = pObj->GetNumPickers();
                break;

            case 1:
                if (OK != pJavaScript->FromJsval(pArguments[0], &first))
                    goto error;
                last = first;
                break;

            case 2:
                if (OK != pJavaScript->FromJsval(pArguments[0], &first))
                    goto error;
                if (OK != pJavaScript->FromJsval(pArguments[1], &last))
                    goto error;
                break;

                error:
            default:
                JS_ReportError(pContext, "Usage: ModsTest.SetDefaultPicker(first, last)");
                return JS_FALSE;
        }

        RETURN_RC(pObj->SetDefaultPickers(first, last));
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(ModsTest,
                  GetPicker,
                  1,
                  "Get element from FancyPicker array.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: ModsTest.GetPicker(idx)");
        return JS_FALSE;
    }
    UINT32 value;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &value))
    {
        JS_ReportError(pContext, "Invalid value for idx");
        return JS_FALSE;
    }

    ModsTest * pObj;
    if ((pObj = JS_GET_PRIVATE(ModsTest, pContext, pObject, NULL)) != 0)
    {
        pObj->GetPicker(value, pReturlwalue);
        return JS_TRUE;
    }
    return JS_FALSE;
}

CLASS_PROP_READONLY (ModsTest,      IsSupported,        bool,   "True if the test is supported, and otherwise false.");
CLASS_PROP_READWRITE_FULL(ModsTest, Name,               string, "Name of the test.", 0, "");
CLASS_PROP_READONLY (ModsTest,      SetupDone,          bool,   "True if Setup has succeeded since last Cleanup.");
CLASS_PROP_READWRITE(ModsTest,      SetupSyncRequired,  bool,   "True if fg & bg tests require Setup to complete before starting Run.");
CLASS_PROP_READWRITE(ModsTest,      DeviceSync,         bool,   "True if conlwrrent devices should sync Setup, Run, and Cleanup.");
CLASS_PROP_READWRITE(ModsTest,      EdcTol,             UINT64, "Tolerance for each type of EDC CRC error of this test: total, read, write");
CLASS_PROP_READWRITE(ModsTest,      EdcTotalTol,        UINT64, "Tolerance for number of overall EDC CRC errors of this test.");
CLASS_PROP_READWRITE(ModsTest,      EdcRdTol,           UINT64, "Tolerance for number of Read EDC CRC errors of this test.");
CLASS_PROP_READWRITE(ModsTest,      EdcWrTol,           UINT64, "Tolerance for number of Write EDC CRC errors of this test.");
CLASS_PROP_READWRITE(ModsTest,      EdcTotalPerMinTol,  UINT64, "Tolerance for overall EDC CRC errors per minutes for this test.");
CLASS_PROP_READWRITE(ModsTest,      HeartBeatPeriodSec, UINT32, "Heartbeat period in sec for test");
CLASS_PROP_READWRITE(ModsTest,      IgnoreErrCountsPreRun, bool, "If true, error counters are ignored before (during setup) running a test");


PROP_CONST(ModsTest, PRE_SETUP,     ModsTest::PRE_SETUP);
PROP_CONST(ModsTest, PRE_RUN,       ModsTest::PRE_RUN);
PROP_CONST(ModsTest, POST_RUN,      ModsTest::POST_RUN);
PROP_CONST(ModsTest, POST_CLEANUP,  ModsTest::POST_CLEANUP);
PROP_CONST(ModsTest, IS_SUPPORTED,  ModsTest::IS_SUPPORTED);
PROP_CONST(ModsTest, PRE_PEX_CHECK, ModsTest::PRE_PEX_CHECK);
PROP_CONST(ModsTest, PEX_ERROR,     ModsTest::PEX_ERROR);
PROP_CONST(ModsTest, MISC_A,        ModsTest::MISC_A);
PROP_CONST(ModsTest, MISC_B,        ModsTest::MISC_B);
PROP_CONST(ModsTest, ISM_EXP,       ModsTest::ISM_EXP);
PROP_CONST(ModsTest, END_LOOP,      ModsTest::END_LOOP);

JS_STEST_LWSTOM(ModsTest, AddGlobalCallback, 2, "Set global/shared callback")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: ModsTest.AddGlobalCallback(idx, func)");
        return JS_FALSE;
    }

    UINT32 idx;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &idx) ||
        idx >= ModsTest::NUM_CALLBACK_TYPE)
    {
        JS_ReportWarning(pContext, "Callback idx not a valid ModsTest callback.\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    ModsTest::CallbackIdx cbId = ModsTest::CallbackIdx(idx);
    ModsTest::GetGlobalCallbacks(cbId)->Connect(pObject, pArguments[1]);

    RETURN_RC(OK);
}

JS_STEST_LWSTOM(ModsTest, AddCallback, 2, "Add local callback function")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: ModsTest.AddCallback(idx, func)");
        return JS_FALSE;
    }

    UINT32 idx;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &idx) ||
        idx >= ModsTest::NUM_CALLBACK_TYPE)
    {
        JS_ReportWarning(pContext, "Callback idx not a valid ModsTest callback.\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    ModsTest *pObj = JS_GET_PRIVATE(ModsTest, pContext, pObject, NULL);
    if (pObj)
    {
        ModsTest::CallbackIdx cbId = ModsTest::CallbackIdx(idx);
        pObj->GetCallbacks(cbId)->Connect(pObject, pArguments[1]);

        RETURN_RC(OK);
    }

    RETURN_RC(RC::SOFTWARE_ERROR);
}

JS_SMETHOD_LWSTOM(ModsTest, RemoveGlobalCallback, 2, "Remove shared callback function.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: ModsTest.RemoveGlobalCallback(idx, conn)");
        return JS_FALSE;
    }

    UINT32 idx;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &idx) ||
        idx >= ModsTest::NUM_CALLBACK_TYPE)
    {
        JS_ReportWarning(pContext, "Callback idx not a valid ModsTest callback.\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    ModsTest::CallbackIdx cbId = ModsTest::CallbackIdx(idx);
    ModsTest::GetGlobalCallbacks(cbId)->Disconnect(pArguments[1]);

    RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(ModsTest, RemoveCallback, 2, "Remove local callback function")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: ModsTest.RemoveCallback(idx, conn)");
        return JS_FALSE;
    }

    UINT32 idx;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &idx) ||
        idx >= ModsTest::NUM_CALLBACK_TYPE)
    {
        JS_ReportWarning(pContext, "Callback idx not a valid ModsTest callback.\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    ModsTest * pObj = JS_GET_PRIVATE(ModsTest, pContext, pObject, NULL);
    if (pObj)
    {
        ModsTest::CallbackIdx cbId = ModsTest::CallbackIdx(idx);
        pObj->GetCallbacks(cbId)->Disconnect(pArguments[1]);

        RETURN_RC(OK);
    }

    RETURN_RC(RC::SOFTWARE_ERROR);
}

JS_SMETHOD_LWSTOM(ModsTest, HasCallback, 1, "True if local callback is set")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: ModsTest.HasCallback(idx)");
        return JS_FALSE;
    }

    UINT32 idx;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &idx) ||
        idx >= ModsTest::NUM_CALLBACK_TYPE)
    {
        JS_ReportError(pContext, "Callback idx not a valid ModsTest callback.\n");
        return JS_FALSE;
    }

    bool isHooked = false;

    ModsTest * pObj = JS_GET_PRIVATE(ModsTest, pContext, pObject, NULL);
    if (pObj)
    {
        ModsTest::CallbackIdx cbId = ModsTest::CallbackIdx(idx);
        if (pObj->GetCallbacks(cbId)->GetCount() > 0)
            isHooked = true;
    }
    *pReturlwalue = BOOLEAN_TO_JSVAL(isHooked);

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(ModsTest, TestHelp, 0, "Provide help for the test")
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    ModsTest *pTest = (ModsTest*)JS_GetPrivate(pContext, pObject);

    RETURN_RC(pTest->TestHelpWrapper(pContext));
}

JS_STEST_LWSTOM(ModsTest, PrintProgressInit, 2,
    "Initialize test progress and describe what progress X/Y indicates")
{
    STEST_HEADER(1, 2,
        "Usage: ModsTest.PrintProgressInit(maxIterations, [progressDesc])");
    STEST_PRIVATE(ModsTest, pModsTest, nullptr);
    STEST_ARG(0, UINT64, maxIterations);
    STEST_OPTIONAL_ARG(1, string, progressDesc, "");
    RETURN_RC(pModsTest->PrintProgressInit(maxIterations, progressDesc));
}

JS_STEST_BIND_ONE_ARG(
        ModsTest, PrintProgressUpdate,
        lwrrentIteration, UINT64,
        "Update the current progress of the test");

JS_STEST_BIND_TWO_ARGS(
        ModsTest, PrintErrorUpdate,
        lwrrentIteration, UINT64,
        errorRc, INT32,
        "Update the error progress of the current test");

JS_SMETHOD_LWSTOM
(
    ModsTest,
    TestArgHelp,
    1,
    "Return help string for specified testarg"
)
{
    STEST_HEADER(1, 1, "TestArgHelp(testargname)");
    STEST_ARG(0, string, testargName);
    STEST_PRIVATE(ModsTest, pTest, NULL);

    JSClass* pClass = JS_GetClass(pTest->GetJSObject());

    const SObjectVec& Objects = GetSObjects();
    for (auto ipObject = Objects.begin();
         ipObject != Objects.end();
         ipObject++)
    {
        // Check to see if this SObject class matches the JS Object class
        if (strcmp(pClass->name, (*ipObject)->GetClassName()) == 0)
        {
            // Print help and value for the object's properties.
            const SPropertyVec& Properties = (*ipObject)->GetProperties();

            for (auto ipProperty = Properties.begin();
                 ipProperty != Properties.end();
                 ipProperty++)
            {
                if(strcmp(testargName.c_str(), (*ipProperty)->GetName()) == 0)
                {
                    JSString* pJSString = JS_NewStringCopyZ(pContext,
                                                            (*ipProperty)->GetHelpMessage());
                    if (nullptr == pJSString)
                    {
                        return JS_FALSE;
                    }

                    *pReturlwalue = STRING_TO_JSVAL(pJSString);
                    return JS_TRUE;
                }
            }
        }
    }

    return JS_FALSE;
}

JS_STEST_BIND_NO_ARGS(ModsTest, EndLoop,
                  "Perform any end of loop processing.");
