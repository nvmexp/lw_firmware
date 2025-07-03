/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation. All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "core/include/errormap.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/mle_protobuf.h"
#include "lwtypes.h"

#include <atomic>
#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <unordered_set>

//------------------------------------------------------------------------------
// Define the RC messages.
// These message are only accessible to functions in this file.
//

static map<int, const char*> s_MessageMap;

// HACK HACK
// We hold the error data in two parallel tables, one for the error code
// integers and another for the actual error messages.  When we first need
// this data, we insert it into a map of error code to message for faster
// access.
//
// Yes, this is a totally asinine way to go about doing this.  However,
// the djgpp compiler seems to have problems with #including large
// files in the middle of a function, and the Mac OS X compiler didn't like
// it when I created a struct of the form {int, char*} to hold the data in
// a single table.
//
// If memory usage becomes an issue, we could just leave the two tables in
// place and perform a search to find the message mapping to the
// error code each time at runtime.

static const int errorCodes[] =
{
    #undef  DEFINE_RC
    #define DEFINE_RC(errno, code, message) errno,
    #define DEFINE_RETURN_CODES
    #include "errors.h"
    RC::TEST_BASE
};

static const char *errorMessages[] =
{
    #undef  DEFINE_RC
    #define DEFINE_RC(errno, code, message) message,
    #define DEFINE_RETURN_CODES
    #include "errors.h"
    "Limit"
};

//------------------------------------------------------------------------------
// JavaScript ErrorMap object and properties.
//
JS_CLASS(ErrorMap);

static SObject ErrorMap_Object
(
   "ErrorMap",
   ErrorMapClass,
   0,
   0,
   "Mapper from RC to error and help message."
);

//------------------------------------------------------------------------------
// Get/Set functions for the per-thread "enable test progress" property.
JSBool GetErrorMap_EnableTestProgress
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    bool bEnabled = ErrorMap::IsTestProgressEnabled();

    if (OK != JavaScriptPtr()->ToJsval(bEnabled, pValue))
    {
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSBool SetErrorMap_EnableTestProgress
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    bool enable = 0;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &enable))
    {
        return JS_FALSE;
    }
    ErrorMap::EnableTestProgress(enable);
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get/Set functions for the minimum test progress interval
JSBool GetErrorMap_MinTestProgressIntervalMs
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    UINT32 lwrInterval = ErrorMap::MinTestProgressIntervalMs();

    if (OK != JavaScriptPtr()->ToJsval(lwrInterval, pValue))
    {
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSBool SetErrorMap_MinTestProgressIntervalMs
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    UINT32 newInterval = 0;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &newInterval))
    {
        return JS_FALSE;
    }
    ErrorMap::SetMinTestProgressIntervalMs(newInterval);
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get/Set functions for the per-thread "current test number" property.

JSBool GetErrorMap_Test
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    INT32 lwrTestNum = ErrorMap::Test();

    if (OK != JavaScriptPtr()->ToJsval(lwrTestNum, pValue))
        return JS_FALSE;

    return JS_TRUE;
}

JSBool SetErrorMap_Test
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    INT32 lwrTestNum = -1;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &lwrTestNum))
        return JS_FALSE;

    ErrorMap::SetTest(lwrTestNum);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get/Set functions for the "test phase" property.

JSBool GetErrorMap_TestPhase
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    const UINT32 lwrTestPhase = ErrorMap::TestPhase();

    if (OK != JavaScriptPtr()->ToJsval(lwrTestPhase, pValue))
        return JS_FALSE;

    return JS_TRUE;
}

JSBool SetErrorMap_TestPhase
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    UINT32 lwrTestPhase = 0;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &lwrTestPhase))
        return JS_FALSE;

    ErrorMap::SetTestPhase(lwrTestPhase);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get/Set functions for the "test offset" property.

JSBool GetErrorMap_TestOffset
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    const UINT32 lwrTestOffset = ErrorMap::TestOffset();

    if (OK != JavaScriptPtr()->ToJsval(lwrTestOffset, pValue))
        return JS_FALSE;

    return JS_TRUE;
}

JSBool SetErrorMap_TestOffset
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    UINT32 lwrTestOffset = 0;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &lwrTestOffset))
        return JS_FALSE;

    ErrorMap::SetTestOffset(lwrTestOffset);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get/Set functions for the per-thread "DevInst" property.
JSBool GetErrorMap_DevInst
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    INT32 devInst = ErrorMap::DevInst();

    if (OK != JavaScriptPtr()->ToJsval(devInst, pValue))
        return JS_FALSE;

    return JS_TRUE;
}

JSBool SetErrorMap_DevInst
(
    JSContext * pContext,
    JSObject * pObject,
    jsval PropertyId,
    jsval * pValue
)
{
    INT32 devInst = -1;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &devInst))
        return JS_FALSE;

    ErrorMap::SetDevInst(devInst);

    return JS_TRUE;
}
//------------------------------------------------------------------------------
static SProperty ErrorMap_EnableTestProgress
(
   ErrorMap_Object,
   "EnableTestProgress",
   0, // id
   GetErrorMap_EnableTestProgress,
   SetErrorMap_EnableTestProgress,
   0, // flags
   "Current EnableTestProgress state"
);

static SProperty ErrorMap_MinTestProgressIntervalMs
(
   ErrorMap_Object,
   "MinTestProgressIntervalMs",
   0, // id
   GetErrorMap_MinTestProgressIntervalMs,
   SetErrorMap_MinTestProgressIntervalMs,
   0, // flags
   "Current minimum interval to print TestProgress"
);

static SProperty ErrorMap_Test
(
   ErrorMap_Object,
   "Test",
   0,
   -1,
   GetErrorMap_Test,
   SetErrorMap_Test,
   0,
   "Current exelwting test (per-thread)."
);

static SProperty ErrorMap_TestPhase
(
   ErrorMap_Object,
   "TestPhase",
   0,
   0,
   GetErrorMap_TestPhase,
   SetErrorMap_TestPhase,
   0,
   "Current test phase."
);

static SProperty ErrorMap_TestOffset
(
   ErrorMap_Object,
   "TestOffset",
   0,
   0,
   GetErrorMap_TestOffset,
   SetErrorMap_TestOffset,
   0,
   "Test-number offset (to differentiate different testing modes)."
);

static SProperty ErrorMap_DevInst
(
   ErrorMap_Object,
   "DevInst",
   0,
   0,
   GetErrorMap_DevInst,
   SetErrorMap_DevInst,
   0,
   "DevInst for the current thread"
);

C_(ErrorMap_Code);
static SMethod ErrorMap_Code
(
   ErrorMap_Object,
   "Code",
   C_ErrorMap_Code,
   1,
   "Map an RC to an error code."
);

C_(ErrorMap_Message);
static SMethod ErrorMap_Message
(
   ErrorMap_Object,
   "Message",
   C_ErrorMap_Message,
   1,
   "Map an RC to an error message."
);

C_(ErrorMap_ErrorCodeStr);
static SMethod ErrorMap_ErrorCodeStr
(
   ErrorMap_Object,
   "ErrorCodeStr",
   C_ErrorMap_ErrorCodeStr,
   1,
   "Takes an RC and print out the formated errorcode that contains phase, test, error."
);

JS_SMETHOD_LWSTOM(ErrorMap,
                  PopTestContext,
                  0,
                  "PopTestContext")
{
    JavaScriptPtr pJs;
    void* pTestCtx = ErrorMap::PopTestContext();
    const UINT64 ptrAsNum = static_cast<UINT64>(reinterpret_cast<uintptr_t>(pTestCtx));
    vector<UINT32> rtnArray =
    {
        static_cast<UINT32>(ptrAsNum & 0xffffffff),
        static_cast<UINT32>(ptrAsNum >> 32)
    };

    if (OK != pJs->ToJsval(rtnArray, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in ErrorMap.PopTestContext()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(ErrorMap,
                  RestoreTestContext,
                  1,
                  "RestoreTestContext")
{
    vector<UINT32> arg;
    JavaScriptPtr pJs;
    if ((NumArguments != 1) || (OK != pJs->FromJsval(pArguments[0], &arg)))
    {
        JS_ReportError(pContext, "Usage: ErrorMap.RestoreTestContext(pOldCtx)");
        return JS_FALSE;
    }

    const UINT64 ptrAsNum = static_cast<UINT64>(arg[0]) |
                            static_cast<UINT64>(arg[1]) << 32;

    void* pOldCtx = reinterpret_cast<void*>(static_cast<uintptr_t>(ptrAsNum));
    ErrorMap::RestoreTestContext(pOldCtx);
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// ErrorMap
//
namespace
{
    bool   s_TestProgressEnabled = true;
    bool   s_TlsIndexAllocated = false;
    UINT32 s_LwrTestPhaseTlsIdx   = 0;
    UINT32 s_LwrTestContextPtrTlsIdx = 0;
    UINT32 s_DevInstTlsIdx           = 0;

    class TestContextSet
    {
    public:
        TestContextSet() : m_Mutex(nullptr)
        {
            m_Mutex = Tasker::AllocMutex("TestContextSet::m_Mutex",
                                         Tasker::mtxUnchecked);
        }

        ~TestContextSet()
        {
            m_TestContexts.clear();

            Tasker::FreeMutex(m_Mutex);
            m_Mutex = nullptr;
        }

        void Insert(ErrorMap::TestContext* pTestContext)
        {
            Tasker::MutexHolder lock(m_Mutex);
            m_TestContexts.insert(unique_ptr<ErrorMap::TestContext>(pTestContext));
        }

    private:
        TestContextSet(const TestContextSet& other) = delete;
        TestContextSet(TestContextSet&& other) = delete;

        unordered_set<unique_ptr<ErrorMap::TestContext>> m_TestContexts;
        void* m_Mutex;
    };

    // This container is used to keep track of allocated TextContext pointers,
    // and will destroy all those TestContext pointers when MODS exits.
    TestContextSet s_TestContexts;
}

ErrorMap::ErrorMap
(
   RC rc
)
{
   SetRC(rc);

   if (s_MessageMap.empty())
   {
      for (unsigned int i=0; i < sizeof(errorCodes) / sizeof(int); i++)
      {
          s_MessageMap[errorCodes[i]] = errorMessages[i];
      }
   }
}

ErrorMap::ErrorMap()
{
    m_rc = OK;
    m_FailedTest = 0;
    m_FailedPhase = 0;
}

void ErrorMap::SetRC(RC rc)
{
    if (OK == rc)
    {
        m_rc = OK;
        m_FailedTest = 0;
        m_FailedPhase = 0;
        return;
    }

    // If someone has already encoded the test number into the error code,
    // store the test number in m_FailedTest.
    m_rc = (rc % RC::TEST_BASE);
    m_FailedTest = (rc % RC::PHASE_BASE) / RC::TEST_BASE;
    if (m_FailedTest == 0)
    {
        // Non-test code uses TestId = -1 to distinguish from SetPState test
        // (which has TestId = 0). However, our error codes have historically
        // used 0 when reporting non-test specific failures (ex. failures from
        // JS). Therefore, if we're printing from code outside of a test, Test()
        // will return -1, so we need to change it to 0 for reporting purposes.
        m_FailedTest = max<INT32>(Test(), 0);
    }

    UINT32 testOffset = TestOffset();

    // Remove the TestOffset part if already embedded in rc
    if((testOffset > 0) && (m_FailedTest >= testOffset))
    {
        m_FailedTest = m_FailedTest % testOffset;
    }

    m_FailedPhase = TestPhase();
}

void ErrorMap::Clear()
{
    m_rc.Clear();
    m_FailedTest = 0;
    m_FailedPhase = 0;
}

bool ErrorMap::IsValid() const
{
   if (s_MessageMap[m_rc] != NULL)
   {
       return true;
   }
   return false;
}

INT32 ErrorMap::Code() const
{
   // Check if the error is valid.
   if (!IsValid())
      return -1;

   return m_rc;
}

const char * ErrorMap::Message() const
{
   // Check if the error is valid.
   if (!IsValid())
   {
      return "unmapped error";
   }

   return s_MessageMap[m_rc];
}

string ErrorMap::ToDecimalStr() const
{
    return Utility::StrPrintf("%06d%03d%03d",
                              m_FailedPhase,
                              m_FailedTest,
                              m_rc.Get());
}

/* static */
ErrorMap::TestContext* ErrorMap::GetTestContextPtr()
{
    AllocTlsIndex();
    return (TestContext*)(uintptr_t) Tasker::TlsGetValue(s_LwrTestContextPtrTlsIdx);
}

/* static */
void ErrorMap::AllocTlsIndex()
{
    if (!s_TlsIndexAllocated)
    {
        s_LwrTestPhaseTlsIdx      = Tasker::TlsAlloc(true);
        s_LwrTestContextPtrTlsIdx = Tasker::TlsAlloc(true);
        s_DevInstTlsIdx           = Tasker::TlsAlloc(true);

        Tasker::TlsSetValue(s_LwrTestContextPtrTlsIdx, nullptr);

        s_TlsIndexAllocated = true;

        // All platforms init per-thread value at the new index to (void*)NULL.
        // So an uninitialized per-thread "current test" is 0.
    }
}

// s_RunningTests exists solely for the purpose of testnummonitor, which is
// a background logger which monitors test numbers.  We should really delete
// this from ErrorMap and instead set up infrastructure in JS where tests are
// kicked off.  In fact this background logger is lwrrently broken, because
// it relies on intricate behavior of SetTest(), assuming GPU id is set before
// test number is set.  This assumption does not have to be always true, it
// is possible to end up in a situation where test number is set before GPU id.
/* static */ ErrorMap::ErrorMapMutex ErrorMap::s_RTMutex;
/* static */ map<INT32, set<INT32>> ErrorMap::s_RunningTests;

/* static */
void ErrorMap::GetRunningTests(INT32 gpuid, set<INT32> *runningTests)
{
    //map<INT32, set<INT32>>::iterator it;

    if (gpuid > -1)
    {
        //Get s_RTMutex
        Tasker::MutexHolder lock(ErrorMap::s_RTMutex.mutex);
        const auto it = s_RunningTests.find(gpuid);

        if (it != s_RunningTests.end())
        {
            *runningTests = it->second;
        }
    }
}

/* static */
INT32 ErrorMap::Test()
{
    TestContext *pTestContext = GetTestContextPtr();
    return pTestContext ? pTestContext->TestId : -1;
}

/* static */
void ErrorMap::SetTest(INT32 lwrTestNumThisThread)
{
    AllocTlsIndex();

    const INT32 devInst = DevInst();

    // Dereference any previous test context pointer
    TestContext *prevTestContext = GetTestContextPtr();
    if (prevTestContext != nullptr)
    {
        if (prevTestContext->TestId >= 0)
        {
            Tasker::PrintThreadStats();
        }
        Tasker::TlsSetValue(s_LwrTestContextPtrTlsIdx, nullptr);

        // Get s_RTMutex
        Tasker::MutexHolder lock(s_RTMutex.mutex);
        if (s_RunningTests.count(devInst))
        {
            s_RunningTests[devInst].erase(prevTestContext->TestId);
        }
    }

    // If we're setting up for a new test, allocate a new context pointer
    if (lwrTestNumThisThread != -1)
    {
        TestContext *pTestContext = new TestContext(lwrTestNumThisThread);
        MASSERT(pTestContext);

        s_TestContexts.Insert(pTestContext);

        Tasker::TlsSetValue(s_LwrTestContextPtrTlsIdx, (void*)(uintptr_t)pTestContext);

        if (devInst != -1)
        {
            // Get s_RTMutex
            Tasker::MutexHolder lock(s_RTMutex.mutex);
            s_RunningTests[devInst].insert(lwrTestNumThisThread);
        }
    }
}

/* static */
HeartBeatMonitor::MonitorHandle ErrorMap::HeartBeatRegistrationId()
{
    TestContext *pTestContext = GetTestContextPtr();
    return pTestContext ? pTestContext->HeartBeatRegistrationId : 0ULL;   
}

/* static */
bool ErrorMap::IsTestProgressSupported()
{
    TestContext *pTestContext = GetTestContextPtr();
    return pTestContext->IsTestProgressSupported;   
}


/* static */
void ErrorMap::SetHeartBeatRegistrationId(HeartBeatMonitor::MonitorHandle lwrRegistrationId)
{
    TestContext *pTestContext = GetTestContextPtr();
    if (pTestContext == nullptr)
    {
        Printf(Tee::PriError, "Trying to set HeartBeatRegistrationId on NULL pTestContext!\n");
        MASSERT(!"Trying to set HeartBeatRegistrationId on NULL pTestContext!");
        return;
    }

    if (pTestContext->HeartBeatRegistrationId != 0)
    {
        Printf(Tee::PriError, "Trying to set HeartBeatRegistrationId after it has"
                              " already been set!\n");
        MASSERT(!"Trying to set HeartBeatRegistrationId after it has already been set!");
        return;
    }
    pTestContext->HeartBeatRegistrationId = lwrRegistrationId;
    pTestContext->IsTestProgressSupported = true;
}

/* static */
RC ErrorMap::EndTestProgress()
{
    RC rc;
    CHECK_RC(HeartBeatMonitor::UnRegisterTest(ErrorMap::HeartBeatRegistrationId()));
    return RC::OK;
}

/* static */
void ErrorMap::EnableTestProgress(bool enable)
{
    s_TestProgressEnabled = enable;
}

/* static */
void ErrorMap::InitTestProgress(UINT64 maxTestProgress, UINT64 heartBeatPeriodSec)
{
    HeartBeatMonitor::MonitorHandle regId = HeartBeatMonitor::RegisterTest(ErrorMap::Test(), 
            ErrorMap::DevInst(), heartBeatPeriodSec);
    ErrorMap::SetHeartBeatRegistrationId(regId);
    ErrorMap::SetTestProgress(0);
    ErrorMap::SetTestProgressMax(maxTestProgress);
}

/* static */
bool ErrorMap::IsTestProgressEnabled()
{
    return s_TestProgressEnabled;
}

/* static */
UINT64 ErrorMap::TestProgress()
{
    TestContext *pTestContext = GetTestContextPtr();
    return pTestContext ? pTestContext->Progress : 0ULL;
}

/* static */
void ErrorMap::SetTestProgress(UINT64 lwrTestProgressThisThread)
{
    TestContext *pTestContext = GetTestContextPtr();
    if (pTestContext == nullptr)
    {
        Printf(Tee::PriError, "Trying to set TestProgress on NULL pTestContext!");
        MASSERT(0);
        return;
    }

    // Check that progress is always going forward, unless it's being reset
    if (lwrTestProgressThisThread > 0 && lwrTestProgressThisThread < pTestContext->Progress)
    {
        Printf(Tee::PriError, "Trying to set TestProgress to a lower value (%llu) "
                              "than what has already been set (%llu)!",
                              lwrTestProgressThisThread, pTestContext->Progress);
        MASSERT(0);
        return;
    }

    pTestContext->Progress = lwrTestProgressThisThread;
    
    HeartBeatMonitor::SendUpdate(ErrorMap::HeartBeatRegistrationId());
}

/* static */
UINT64 ErrorMap::TestProgressMax()
{
    TestContext *pTestContext = GetTestContextPtr();
    return pTestContext ? pTestContext->ProgressMax : 0ULL;
}

/* static */
void ErrorMap::SetTestProgressMax(UINT64 lwrTestProgressMaxThisThread)
{
    TestContext *pTestContext = GetTestContextPtr();
    if (pTestContext == nullptr)
    {
        Printf(Tee::PriError, "Trying to set TestProgressMax on NULL pTestContext!");
        MASSERT(0);
        return;
    }

    // Only allow changing ProgressMax if Progress is 0, meaning it was reset,
    // and therfore ProgressMax can be reset
    if (pTestContext->Progress > 0 && pTestContext->ProgressMax > 0)
    {
        Printf(Tee::PriError, "Can't st ProgressMax. it's already been set (%llu)!",
                              pTestContext->ProgressMax);
        MASSERT(0);
        return;
    }

    pTestContext->ProgressMax = lwrTestProgressMaxThisThread;
}

/* static */
UINT64 ErrorMap::TestProgressLastUpdateMs()
{
    TestContext *pTestContext = GetTestContextPtr();
    return pTestContext ? pTestContext->LastUpdateMs : 0ULL;
}

/* static */
void ErrorMap::SetTestProgressLastUpdateMs(UINT64 lwrTestProgressLastUpdateThisThread)
{
    TestContext *pTestContext = GetTestContextPtr();
    if (pTestContext == nullptr)
    {
        return;
    }

    pTestContext->LastUpdateMs = lwrTestProgressLastUpdateThisThread;
}

/* static */ UINT32 ErrorMap::s_MinTestProgressIntervalMs = 200;

/* static */
UINT32 ErrorMap::MinTestProgressIntervalMs()
{
    return s_MinTestProgressIntervalMs;
}

/* static */
void ErrorMap::SetMinTestProgressIntervalMs(UINT32 newInterval)
{
    s_MinTestProgressIntervalMs = newInterval;
}

/* static */
INT32 ErrorMap::DevInst()
{
    if (!s_TlsIndexAllocated)
    {
        return -1;
    }
    const auto tlsDevInst = reinterpret_cast<uintptr_t>(Tasker::TlsGetValue(s_DevInstTlsIdx));
    return static_cast<INT32>(tlsDevInst) - 1;
}

/* static */
void ErrorMap::SetDevInst(INT32 devInst)
{
    MASSERT(devInst >= -1);

    AllocTlsIndex();

    // -1 has a special meaning, it means no GPU id has been set.
    // We save GPU id plus 1 in TLS, so that the "no gpu id" value
    // is saved as 0, which is the default value for new TLS entries.
    Tasker::TlsSetValue(s_DevInstTlsIdx, reinterpret_cast<void*>(
                        static_cast<uintptr_t>(devInst + 1)));
}

/* static */
//! Removes the current test context and returns the pointer to it
void* ErrorMap::PopTestContext()
{
    void* pTestContext = Tasker::TlsGetValue(s_LwrTestContextPtrTlsIdx);
    Tasker::TlsSetValue(s_LwrTestContextPtrTlsIdx, nullptr);
    return pTestContext;
}

/* static */
//! Restore a previously popped test context
void ErrorMap::RestoreTestContext(void* pTestContext)
{
    Tasker::TlsSetValue(s_LwrTestContextPtrTlsIdx, pTestContext);
}

/* static */
UINT32 ErrorMap::TestPhase()
{
    AllocTlsIndex();
    return (UINT32)(uintptr_t) Tasker::TlsGetValue(s_LwrTestPhaseTlsIdx);
}

/* static */
void ErrorMap::SetTestPhase(UINT32 lwrTestPhaseThisThread)
{
    AllocTlsIndex();
    Tasker::TlsSetValue(s_LwrTestPhaseTlsIdx, (void*)(uintptr_t)lwrTestPhaseThisThread);
}

namespace
{
    atomic<UINT32> s_TestOffset(0);
}

/* static */
UINT32 ErrorMap::TestOffset()
{
    return s_TestOffset.load();
}

/* static */
void ErrorMap::PrintRcDesc(RC rc)
{
    static atomic<UINT32> rcDescWasPrinted[Utility::AlignUp<unsigned int, unsigned int>(1000, 32) / 32];
    MASSERT(static_cast<INT32>(rc) < 1000);
    const INT32  idx  = static_cast<INT32>(rc) >> 5;
    const UINT32 mask = 1U << (static_cast<INT32>(rc) & 31);
    if (!(rcDescWasPrinted[idx].fetch_or(mask) & mask))
    {
        Mle::Print(Mle::Entry::rc_desc)
            .rc(rc)
            .rc_str(ErrorMap(rc).Message());
    }
}

void ErrorMap::SetTestOffset(UINT32 testOffset)
{
    s_TestOffset.store(testOffset);
}

//------------------------------------------------------------------------------

// SMethod
C_(ErrorMap_Code)
{
   *pReturlwalue = JSVAL_NULL;

   JavaScriptPtr pJs;

   // Check the arguments.
   UINT32 rc;
   if
   (
         (NumArguments != 1)
      || (OK != pJs->FromJsval(pArguments[0], &rc))
   )
   {
      JS_ReportError(pContext, "Usage: ErrorMap.Code(rc)");
      return JS_FALSE;
   }

   ErrorMap em(rc);
   if (OK != pJs->ToJsval(em.Code(), pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in ErrorMap.Code(rc)");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// SMethod
C_(ErrorMap_Message)
{
   *pReturlwalue = JSVAL_NULL;

   JavaScriptPtr pJs;

   // Check the arguments.
   UINT32 rc;
   if
   (
         (NumArguments != 1)
      || (OK != pJs->FromJsval(pArguments[0], &rc))
   )
   {
      JS_ReportError(pContext, "Usage: ErrorMap.Message(rc)");
      return JS_FALSE;
   }

   ErrorMap em(rc);
   string str = em.Message();

   if (OK != pJs->ToJsval(str, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in ErrorMap.Message(rc)");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// SMethod
C_(ErrorMap_ErrorCodeStr)
{
   *pReturlwalue = JSVAL_NULL;
   JavaScriptPtr pJs;

   // Check the arguments.
   UINT32 rc;
   if ((NumArguments != 1) || (OK != pJs->FromJsval(pArguments[0], &rc)))
   {
      JS_ReportError(pContext, "Usage: ErrorMap.ErrorCodeStr(rc)");
      return JS_FALSE;
   }

   ErrorMap em(rc);
   string str = em.ToDecimalStr();
   if (OK != pJs->ToJsval(str, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in ErrorMap.ErrorCodeStr(rc)");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

