/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or diss_Closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JavaScript language interface.

#include <array>
#include <csignal>
#include <cstring>
#include <ctime>
#include <map>
#include <stack>
#include <cerrno>
#include <memory>
#include <unordered_map>

#include "core/include/abort.h"
#include "core/include/cmdline.h"
#include "core/include/errcount.h"
#include "core/include/errormap.h"
#include "core/include/fileholder.h"
#include "core/include/globals.h"
#include "core/include/gpu.h"
#include "decrypt.h"
#include "js.h"
#include "jscntxt.h"
#include "jsatom.h"
#include "jsdbgapi.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsobj.h"
#include "jsstr.h"
#include "jsstub.h"
#include "core/include/log.h"
#include "core/include/massert.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "core/include/preprocess.h"
#include "random.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "core/include/xp.h"

#include "core/include/jscript.h"

#include "core/include/memcheck.h"

#include "core/tests/modstest.h"

#if defined(INCLUDE_BYTECODE)
#include "jsscriptio.h"
#include "objectidsmapper.h"
#endif

#if defined(INCLUDE_GPU)
#include "gpu/vmiop/vmiopelw.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#endif

// Define the pointer to a JS callback chain. Upon instantiation there will be
// an instance of m_Head per each used JS_Set*Hook.
template <typename JSSetHookFunType, JSSetHookFunType *JSSetHookFunPtr>
JSHookCallbackChain<JSSetHookFunType, JSSetHookFunPtr>*
JSHookCallbackChain<JSSetHookFunType, JSSetHookFunPtr>::m_Head = nullptr;

template <typename JSSetHookFunType, JSSetHookFunType *JSSetHookFunPtr>
JSHookCallbackChain<JSSetHookFunType, JSSetHookFunPtr>**
JSHookCallbackChain<JSSetHookFunType, JSSetHookFunPtr>::m_LwrNode = nullptr;

vector<void(*)()> AllJSHookChains::m_ShutDownRoutines;

class BeforeClosureMap : public unordered_map<JSStackFrame *, void *> {};

void JavaScript::JSCallHook::Init(JSRuntime *rt, JavaScript *js, CallHookPtr jsMethod)
{
    m_Js = js;
    m_JsMethod = jsMethod;
    m_BeforeClosure = make_unique<BeforeClosureMap>();
}

void JavaScript::JSCallHook::ShutDown()
{
    m_Js = nullptr;
    m_JsMethod = nullptr;
    m_BeforeClosure.reset();
}

JavaScript::JSCallHook::JSCallHook(JavaScript *js, CallHookPtr jsMethod)
  : m_Js(js)
  , m_JsMethod(jsMethod)
{}

void * JavaScript::JSCallHook::Hook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok)
{
    if (m_Js && m_JsMethod && m_BeforeClosure)
    {
        if (before)
        {
            (*m_BeforeClosure)[fp] = (m_Js->*m_JsMethod)(cx, fp, before, ok);
        }
        else
        {
            void *beforeClosure = (*m_BeforeClosure)[fp];
            // we call the hook after the call if before the call it returned
            // not null result
            if (nullptr != beforeClosure)
            {
                (m_Js->*m_JsMethod)(cx, fp, before, ok);
            }
            m_BeforeClosure->erase(fp);
        }
    }

    // We always return not null result, even if the real hook returned null. It
    // guarantees that all hooks in the chain will be processed. The JS
    // agreement that a hook is not called after the call if it returned null
    // before the call is resolved inside JSCallHook itself, not by the JS
    // engine. Also the engine will call the after call hook with the closure
    // equal to the value we return here in before call. Since we use closure to
    // pass the head of the chain, we have to return the head.
    return GetHead();
}

string DeflateJSString(JSContext *cx, JSString *inStr)
{
    MASSERT(nullptr != inStr);

    string retVal;

    size_t strLength = JSSTRING_LENGTH(inStr);
    jschar *strChars = JSSTRING_CHARS(inStr);

    // it's a design flaw in the JS engine: js_DeflateStringToBuffer has a
    // different contract when UTF-8 encoding is on
#if defined(JS_C_STRINGS_ARE_UTF8)
    size_t size = 0;
    js_DeflateStringToBuffer(cx, strChars, strLength, 0, &size);
    retVal.resize(size);
    js_DeflateStringToBuffer(cx, strChars, strLength, &retVal[0], &size);
#else
    retVal.resize(strLength);
    size_t size = strLength;
    js_DeflateStringToBuffer(cx, strChars, strLength, &retVal[0], &size);
#endif

    return retVal;
}

// Define the JavaScript singleton.
JavaScript * JavaScriptPtr::s_pInstance;
const char * JavaScriptPtr::s_Name = "JavaScript";

// Javascript default value function.  This gets called whenever the JS library
// uses its internal default printing routine to print out an object
static JSBool JsExceptionDefaultValue
(
    JSContext *pContext
   ,JSObject *pObject
   ,JSType type
   ,jsval *vp
)
{
    if (type == JSTYPE_STRING)
    {
        JavaScriptPtr pJs;
        string message;
        string rcMessage;
        INT32 rcInt;

        if ((pJs->GetProperty(pObject, "rc", &rcInt) == OK) &&
            (pJs->GetProperty(pObject, "message", &message) == OK))
        {
            RC rc(rcInt);
            string str = Utility::StrPrintf("%s : %s (%d)\n",
                                            message.c_str(),
                                            rc.Message(),
                                            rcInt);
            string stackTrace;
            if (pJs->GetProperty(pObject, "bt", &stackTrace) == OK)
            {
                str += stackTrace;
            }
            if (OK == pJs->ToJsval(str, vp))
                return JS_TRUE;
        }
    }
    return js_DefaultValue(pContext, pObject, type, vp);
}

// Custom object operations for JsException objects.  Used to override the
// defaultValue callback to provide a custom printing of JsExceptions.
static JSObjectOps s_JsExceptionObjectOps =
{
    js_NewObjectMap,        js_DestroyObjectMap,
    js_LookupProperty,      js_DefineProperty,
    js_GetProperty,         js_SetProperty,
    js_GetAttributes,       js_SetAttributes,
    js_DeleteProperty,      JsExceptionDefaultValue,
    js_Enumerate,           js_CheckAccess,
    NULL,                   NULL,
    NULL,                   js_Construct,
    NULL,                   js_HasInstance,
    js_SetProtoOrParent,    js_SetProtoOrParent,
    js_Mark,                js_Clear,
    js_GetRequiredSlot,     js_SetRequiredSlot
};

// Custom function for the JSExceptionClass to return our custom object ops
// for JS exception objects
JSObjectOps * JsExceptionGetObjectOps(JSContext *cx, JSClass *clasp)
{
    return &s_JsExceptionObjectOps;
}

//-----------------------------------------------------------------------------
static JSClass JsExceptionClass =
{
  "JsException",
  0,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ColwertStub,
  JS_FinalizeStub,
  JsExceptionGetObjectOps
};

//-----------------------------------------------------------------------------
//! \brief Constructor for a JsException object from JS.
//!
static JSBool C_JsException_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] = "Usage: new JsException(rc, message)";

    if (argc != 2)
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJs;
    INT32 rcInt;
    string message;
    if ((OK != pJs->FromJsval(argv[0], &rcInt)) ||
        (OK != pJs->FromJsval(argv[1], &message)))
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    string stackTrace = pJs->GetStackTrace();

    if ((pJs->SetProperty(obj, "rc", rcInt) != OK) ||
        (pJs->SetProperty(obj, "message", message) != OK) ||
        (pJs->SetProperty(obj, "bt", stackTrace) != OK))
    {
        Printf(Tee::PriError, "%s : Unable to update properties\n", __FUNCTION__);
        return JS_FALSE;
    }
    return JS_TRUE;
};

//-----------------------------------------------------------------------------
static SObject JsException_Object
(
    "JsException",
    JsExceptionClass,
    0,
    0,
    "JsException JS Object",
    C_JsException_constructor
);

//------------------------------------------------------------------------------
// Ctrl-C interrupt (SIGINT) handler.
//
static void CtrlCHandler
(
    int /* */
)
{
    Printf(Tee::PriDebug, "User Issued Control-C\n");
    Abort::HandleCtrlC();
}

//------------------------------------------------------------------------------
// JavaScript branch call back.
//
static JSBool BranchCallBack
(
    JSContext *    pContext,
    JSScript  * /* pScript  */
)
{
    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    // JS_MaybeGC() checks that about 75% of available space has already been
    // allocated to objects before peforming garbage collection.
    JS_MaybeGC(pContext);

    // If a critical event oclwrred return false to abort the script.
    if (GetCriticalEvent())
        return JS_FALSE;

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// JavaScript error reporter.
//
void ErrorReporter
(
    JSContext     * /* pContext */,
    const char    *    Message,
    JSErrorReport *    pReport
)
{
    INT32 Pri =
        (g_FieldDiagMode != FieldDiagMode::FDM_NONE) ? Tee::PriError : Tee::PriAlways;

    if (pReport != nullptr)
    {
        // With strict mode enabled we get error messages about
        // redeclaring functions.  Since we can't control that
        // behavior and don't have a viable solution around it we will
        // reduce print level of that message to prevent confusion from the
        // user community. see bug 1670100
        if (pReport->errorNumber == JSMSG_REDECLARED_VAR)
        {
            Pri = Tee::PriLow;
        }

        if (pReport->linebuf != 0)
        {
            Printf(Pri, "%s\n", pReport->linebuf);

            for (ptrdiff_t Tokens = pReport->tokenptr - pReport->linebuf; Tokens; --Tokens)
            {
                Printf(Pri, ".");
            }

            Printf(Pri, "^\n");
        }

        if (pReport->filename != 0)
        {
            Printf(Pri, "%s:%u: ", pReport->filename,
            pReport->lineno);
        }
    }

    if (Message != 0)
        Printf(Pri, "%s\n", Message);
    else
        Printf(Pri, "JavaScript error\n");
}

//------------------------------------------------------------------------------
// Retrieves test number from the object on the stack (if it's a test)
static bool GetTestNumber
(
    JSContext    *    pContext,
    JSStackFrame *    pStackFrame,
    int          *    pRetval
)
{
    JSObject* pObject = pStackFrame->thisp;
    jsval Value;
    int32 TestNumber;
    JSBool foundProp = JS_FALSE;

    if (JS_HasProperty(pContext, pObject, ENCJSENT("Number"), &foundProp) &&
        (foundProp == JS_TRUE) &&
        JS_GetProperty(pContext, pObject, ENCJSENT("Number"), &Value) &&
        JSVAL_IS_NUMBER(Value) &&
        (JS_ValueToECMAInt32(pContext, Value, &TestNumber)))
    {
        *pRetval = static_cast<int>(TestNumber);
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Retrieves error-code test number from the object on the stack (if
// it's a test).  The error-code test number is used for reporting
// which error the test oclwrred in, which may be different than the
// actual test number.  For example, if a test calls a sub-test, and
// an error happened in the sub-test, we report that the main test
// failed.)
static bool GetErrorTestNumber
(
    JSContext    *    pContext,
    JSStackFrame *    pStackFrame,
    int          *    pRetval
)
{
    JSObject* pObject = pStackFrame->thisp;
    jsval Value;
    int32 TestNumber;
    JSBool foundProp = JS_FALSE;

    if (JS_HasProperty(pContext, pObject, ENCJSENT("ErrorTestNum"), &foundProp) &&
       (foundProp == JS_TRUE) &&
       JS_GetProperty(pContext, pObject, ENCJSENT("ErrorTestNum"), &Value) &&
       JSVAL_IS_NUMBER(Value) &&
       (JS_ValueToECMAInt32(pContext, Value, &TestNumber)))
    {
        *pRetval = static_cast<int>(TestNumber);
        return true;
    }
    return GetTestNumber(pContext, pStackFrame, pRetval);
}

//------------------------------------------------------------------------------------------------
// Retrieves test name from the object on the stack (if it's a test)
// If the "Name" property doesn't exist of can not be colwerted from the jsval then use the
// supplied methodName.
static string GetTestName
(
    JSContext    *    pContext,
    JSStackFrame *    pStackFrame,
    string &          methodName
)
{
    RC rc;
    jsval Value;
    JSBool foundProp = JS_FALSE;
    string testName;
    if (JS_HasProperty(pContext, pStackFrame->thisp, ENCJSENT("Name"), &foundProp) &&
       (foundProp == JS_TRUE) &&
       JS_GetProperty(pContext, pStackFrame->thisp, ENCJSENT("Name"), &Value) &&
       JSVAL_IS_STRING(Value))
    {
        JavaScriptPtr pJs;
        rc = pJs->FromJsval(Value, &testName);
    }
    // failed to get the testName, so use the methodName
    if (testName.empty() || (rc != RC::OK))
    {
        return methodName;
    }

    // testName is valid so return encrypted or non-encrypted string.
    return testName;
}

//------------------------------------------------------------------------------
// JavaScript CallHook function.
// This function is called just before the JS or native function exelwtion
// begins and just after the exelwtion finishes.
//
static map<JSStackFrame*, UINT64> s_StartTimes;
void * JavaScript::CheckJSCalls
(
    JSContext    *    pContext,
    JSStackFrame *    pStackFrame,
    JSBool            IsBefore,
    JSBool       *    IsOk
)
{
    constexpr uint32 JSFRAME_INTERNAL_CONSTRUCTION = JSFRAME_CONSTRUCTING | JSFRAME_INTERNAL;
    if ((pStackFrame->flags & JSFRAME_INTERNAL_CONSTRUCTION) == JSFRAME_INTERNAL_CONSTRUCTION)
    {
        // Detect internal object construction in progress and don't
        // execute the rest of CheckJSCalls to prevent thread switch which
        // could call into the JS engine back.
        // The problem is described in bug 2604287.
        return 0;
    }

    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    // Check if this method is not logged.
    if (!Log::Next())
    {
        Log::SetNext(true);
        return 0;   // The 'after' hook will not be called if 0 is returned.

    }

    if (JSVAL_IS_PRIMITIVE(OBJECT_TO_JSVAL(pStackFrame->thisp)) || !pStackFrame->fun)
    {
        return 0;
    }

    JSString *funName = pStackFrame->fun->atom ? ATOM_TO_STRING(pStackFrame->fun->atom) : nullptr;
    string funNameStr;
    {
        if (nullptr != funName)
        {
            funNameStr = DeflateJSString(pContext, funName);
        }
        else
        {
            funNameStr = js_anonymous_str;
        }
    }

    string Method = string(JS_GetClass(pStackFrame->thisp)->name) + '.' + funNameStr;

    // Check if this method is never logged.
    if (Log::IsNever(Method))
    {
        return 0;   // The 'after' hook will not be called if 0 is returned.
    }

    const INT32 Pri =
        (g_FieldDiagMode != FieldDiagMode::FDM_NONE) ? Tee::PriNormal : Tee::PriAlways;
    if (IsBefore)
    {
        // This code is exelwted just BEFORE the JS or native function exelwtes.

        MASSERT(s_StartTimes.find(pStackFrame) == s_StartTimes.end());
        s_StartTimes[pStackFrame] = Xp::QueryPerformanceCounter();
        ErrCounter::EnterStackFrame(NULL, pStackFrame, true);

        if (Log::IsAlways(Method))
        {
            int TestNumber = -1;
            bool gotTestNumber = GetTestNumber(pContext, pStackFrame, &TestNumber);

            // ErrorMap may be set externally using ErrorMap.Test
            if (gotTestNumber)
                ErrorMap::SetTest(TestNumber);

            string testName = GetTestName(pContext, pStackFrame, Method);
            string line;
            if (Log::PrintEnterCode())
            {
                // Create an error code using a non-zero RC to get
                // phase info, then set the error number to 0.
                ErrorMap Error(1);
                string EnterCode = Error.ToDecimalStr();
                EnterCode[EnterCode.size() - 1] = '0';

                line += Utility::StrPrintf("Enter %s %s",
                    EnterCode.c_str(), testName.c_str());
            }
            else
            {
                line += Utility::StrPrintf("Enter %s", testName.c_str());
            }

            if (gotTestNumber)
            {
                line += Utility::StrPrintf(" (test %d)", TestNumber);
            }

            const bool isTestObj = ModsTest_Object.IsPrototypeOf(pContext, pStackFrame->thisp);

            jsval value;
            string virtualTestId;
            if (isTestObj &&
                OK == GetPropertyJsval(pStackFrame->thisp, "VirtualTestId", &value) &&
                OK == FromJsval(value, &virtualTestId) && virtualTestId != "")
            {
                line += Utility::StrPrintf(" [virtual test id %s]", virtualTestId.c_str());
            }

            string testDescription;
            if (isTestObj &&
                OK == GetPropertyJsval(pStackFrame->thisp, "TestDescription", &value) &&
                OK == FromJsval(value, &testDescription) && testDescription != "")
            {
                line += Utility::StrPrintf(" {%s}", testDescription.c_str());
            }

            INT32 parentNumber = -1;
            if (isTestObj &&
                RC::OK == GetPropertyJsval(pStackFrame->thisp, "ParentNumber", &value))
            {
                if (JSVAL_IS_NUMBER(value))
                {
                    if (FromJsval(value, &parentNumber) != RC::OK)
                    {
                        parentNumber = -1;
                    }
                }
            }

            if (Log::RecordTime())
            {
                // The string returned from ctime() is always 26 characters
                // and has this format: Sun Jan 01 12:34:56 1993\n\0
                time_t Now = time(0);
                line += Utility::StrPrintf(" %s", ctime(&Now));
            }
            else
            {
                line += '\n';
            }
            Printf(Pri, "%s", line.c_str());

#if defined(INCLUDE_GPU)
            if (Platform::IsVirtFunMode())
            {
                VmiopElwManager* pMgr = VmiopElwManager::Instance();
                UINT32 gfid = pMgr->GetGfidAttachedToProcess();
                VmiopElw* pVmiopElw = pMgr->GetVmiopElw(gfid);

                if (pVmiopElw != nullptr)
                {
                    pVmiopElw->CallPluginLogTestStart(gfid, TestNumber);
                }
            }
#endif

            Mle::Print(Mle::Entry::test_info)
                .test_name(testName)
                .parent_id(parentNumber == -1 ? -1 : parentNumber + 1)
                .virtual_name(virtualTestId)
                .virtual_desc(testDescription);
        }

        // Sync log file to disk at start of test that is log on error or always.
        Tee::FlushToDisk();
    }
    else
    {
        // This code is exelwted just AFTER the JS or native function exelwtes.
        UINT64 StartTime = 0;
        FLOAT64 TimeDiff = 0;
        if (s_StartTimes.find(pStackFrame) != s_StartTimes.end())
        {
            StartTime = s_StartTimes[pStackFrame];
            TimeDiff = static_cast<FLOAT64>(Xp::QueryPerformanceCounter() - StartTime)
                       / Xp::QueryPerformanceFrequency();
            s_StartTimes.erase(pStackFrame);
        }

        // Get the methods return code unless a critical event oclwrred.
        RC rc = OK;    // RC's constructor overrides the rc if a critical event oclwrred.
        if (rc == OK)
        {
            if (JSVAL_IS_NUMBER(pStackFrame->rval))
            {
                int32 Code;
                if (JS_ValueToECMAInt32(pContext, pStackFrame->rval, &Code))
                    rc = Code;
            }
        }

        // Check for counter errors, and override the methods's return
        // code if there was a counter error & the method was OK.
        //
        RC tmpRc = ErrCounter::ExitStackFrame(NULL, pStackFrame, NULL);
        if (rc == OK)
        {
            rc = tmpRc;
        }

        if (!(*IsOk))
        {
            // Syntax error or exception.
            rc.Clear();
            rc = GetRcException(pContext);
            if (rc == OK)
                rc = RC::SCRIPT_FAILED_TO_EXELWTE;
        }

        ErrorMap Error(rc);

        const bool isAlways = Log::IsAlways(Method);
        // The method is logged if it returns !OK or it is tagged as log always.
        if (((rc != OK) && (rc != RC::USER_ABORTED_SCRIPT)) || isAlways)
        {
            INT32 firstRc = rc;
            int errorTestNumber = -1;
            if (GetErrorTestNumber(pContext, pStackFrame, &errorTestNumber))
            {
                firstRc = rc % RC::TEST_BASE + errorTestNumber * RC::TEST_BASE;
            }
            Log::SetFirstError(ErrorMap(firstRc));

            string testName = GetTestName(pContext, pStackFrame, Method);
            string line;
            line += Utility::StrPrintf("%s %s : %s", isAlways ? "Exit" : "Error",
                   Error.ToDecimalStr().c_str(), testName.c_str());

            int TestNumber = -1;
            if (GetTestNumber(pContext, pStackFrame, &TestNumber))
            {
                line += Utility::StrPrintf(" (test %d)", TestNumber);
            }

            const bool isTestObj = ModsTest_Object.IsPrototypeOf(pContext, pStackFrame->thisp);

            jsval value;
            string str;
            if (isTestObj &&
                OK == GetPropertyJsval(pStackFrame->thisp, "VirtualTestId", &value) &&
                OK == FromJsval(value, &str) && str != "")
            {
                line += Utility::StrPrintf(" [virtual test id %s]", str.c_str());
            }

            if (isTestObj &&
                OK == GetPropertyJsval(pStackFrame->thisp, "TestDescription", &value) &&
                OK == FromJsval(value, &str) && str != "")
            {
                line += Utility::StrPrintf(" {%s}", str.c_str());
            }

            line += Utility::StrPrintf(" %s", Error.Message());

            if (Log::RecordTime())
            {
                line += Utility::StrPrintf(" [%.3f seconds]", TimeDiff);
            }

            line += "\n";

            // Make mods.log files easier to search.
            // It's easier to grep for "Error" than for "(Error)|(Exit [^0])".
            if ((rc != OK) && isAlways)
                line += "Error!\n";

            Printf(Pri, "%s", line.c_str());

#if defined(INCLUDE_GPU)
            if (Platform::IsVirtFunMode())
            {
                VmiopElwManager* pMgr = VmiopElwManager::Instance();
                UINT32 gfid = pMgr->GetGfidAttachedToProcess();
                VmiopElw* pVmiopElw = pMgr->GetVmiopElw(gfid);

                if (pVmiopElw != nullptr)
                {
                    pVmiopElw->CallPluginLogTestEnd(gfid, TestNumber,
                                                    Error.Code());
                }
            }
#endif
            // Only print Exit lines. Error lines don't mark the end of tests.
            if (isAlways)
            {
                ErrorMap::ScopedTestNum setTestNum(TestNumber);
                Mle::Print(Mle::Entry::test_info).test_end().rc(static_cast<UINT32>(Error.Code()));
            }
            // Sync log file to disk at end of test that is logged.
            Tee::FlushToDisk();
        }
    }

    return this;
}

//------------------------------------------------------------------------------
// Mozilla's JavaScript wrapper.

// Create the JavaScript global class.
JS_CLASS(Global);

// Internal JS methods that should never be logged.
static void InitJsMethods(vector<string> * pJsMethods)
{
    static const char* const methods[] =
    {
       ENCJSENT("Array") "." ENCJSENT("concat"),
       ENCJSENT("Array") "." ENCJSENT("indexOf"),
       ENCJSENT("Array") "." ENCJSENT("join"),
       ENCJSENT("Array") "." ENCJSENT("pop"),
       ENCJSENT("Array") "." ENCJSENT("push"),
       ENCJSENT("Array") "." ENCJSENT("reverse"),
       ENCJSENT("Array") "." ENCJSENT("shift"),
       ENCJSENT("Array") "." ENCJSENT("slice"),
       ENCJSENT("Array") "." ENCJSENT("splice"),
       ENCJSENT("Array") "." ENCJSENT("sort"),
       ENCJSENT("Array") "." ENCJSENT("toSource"),
       ENCJSENT("Array") "." ENCJSENT("toString"),
       ENCJSENT("Array") "." ENCJSENT("unshift"),
       ENCJSENT("Array") "." ENCJSENT("valueOf"),

       ENCJSENT("Boolean") "." ENCJSENT("toSource"),
       ENCJSENT("Boolean") "." ENCJSENT("toString"),
       ENCJSENT("Boolean") "." ENCJSENT("valueOf"),

       ENCJSENT("Date") "." ENCJSENT("getDate"),
       ENCJSENT("Date") "." ENCJSENT("getDay"),
       ENCJSENT("Date") "." ENCJSENT("getFullYear"),
       ENCJSENT("Date") "." ENCJSENT("getHours"),
       ENCJSENT("Date") "." ENCJSENT("getMilliseconds"),
       ENCJSENT("Date") "." ENCJSENT("getMinutes"),
       ENCJSENT("Date") "." ENCJSENT("getMonth"),
       ENCJSENT("Date") "." ENCJSENT("getSeconds"),
       ENCJSENT("Date") "." ENCJSENT("getTime"),
       ENCJSENT("Date") "." ENCJSENT("getTimezoneOffset"),
       ENCJSENT("Date") "." ENCJSENT("getUTCDate"),
       ENCJSENT("Date") "." ENCJSENT("getUTCDay"),
       ENCJSENT("Date") "." ENCJSENT("getUTCFullYear"),
       ENCJSENT("Date") "." ENCJSENT("getUTCHours"),
       ENCJSENT("Date") "." ENCJSENT("getUTCMilliseconds"),
       ENCJSENT("Date") "." ENCJSENT("getUTCMinutes"),
       ENCJSENT("Date") "." ENCJSENT("getUTCMonth"),
       ENCJSENT("Date") "." ENCJSENT("getUTCSeconds"),
       ENCJSENT("Date") "." ENCJSENT("getYear"),
       ENCJSENT("Date") "." ENCJSENT("parse"),
       ENCJSENT("Date") "." ENCJSENT("setDate"),
       ENCJSENT("Date") "." ENCJSENT("setFullYear"),
       ENCJSENT("Date") "." ENCJSENT("setHours"),
       ENCJSENT("Date") "." ENCJSENT("setMilliseconds"),
       ENCJSENT("Date") "." ENCJSENT("setMinutes"),
       ENCJSENT("Date") "." ENCJSENT("setMonth"),
       ENCJSENT("Date") "." ENCJSENT("setSeconds"),
       ENCJSENT("Date") "." ENCJSENT("setTime"),
       ENCJSENT("Date") "." ENCJSENT("setUTCDate"),
       ENCJSENT("Date") "." ENCJSENT("setUTCFullYear"),
       ENCJSENT("Date") "." ENCJSENT("setUTCHours"),
       ENCJSENT("Date") "." ENCJSENT("setUTCMilliseconds"),
       ENCJSENT("Date") "." ENCJSENT("setUTCMinutes"),
       ENCJSENT("Date") "." ENCJSENT("setUTCMonth"),
       ENCJSENT("Date") "." ENCJSENT("setUTCSeconds"),
       ENCJSENT("Date") "." ENCJSENT("setYear"),
       ENCJSENT("Date") "." ENCJSENT("toGMTString"),
       ENCJSENT("Date") "." ENCJSENT("toLocalString"),
       ENCJSENT("Date") "." ENCJSENT("toSource"),
       ENCJSENT("Date") "." ENCJSENT("toString"),
       ENCJSENT("Date") "." ENCJSENT("toUTCString"),
       ENCJSENT("Date") "." ENCJSENT("UTC"),
       ENCJSENT("Date") "." ENCJSENT("valueOf"),

       ENCJSENT("File") "." ENCJSENT("open"),
       ENCJSENT("File") "." ENCJSENT("close"),
       ENCJSENT("File") "." ENCJSENT("flush"),
       ENCJSENT("File") "." ENCJSENT("skip"),
       ENCJSENT("File") "." ENCJSENT("read"),
       ENCJSENT("File") "." ENCJSENT("readln"),
       ENCJSENT("File") "." ENCJSENT("readAll"),
       ENCJSENT("File") "." ENCJSENT("write"),
       ENCJSENT("File") "." ENCJSENT("writeln"),
       ENCJSENT("File") "." ENCJSENT("writeAll"),
       ENCJSENT("File") "." ENCJSENT("remove"),
       ENCJSENT("File") "." ENCJSENT("copyTo"),
       ENCJSENT("File") "." ENCJSENT("renameTo"),
       ENCJSENT("File") "." ENCJSENT("list"),
       ENCJSENT("File") "." ENCJSENT("mkdir"),
       ENCJSENT("File") "." ENCJSENT("toString"),

       ENCJSENT("Function") "." ENCJSENT("apply"),
       ENCJSENT("Function") "." ENCJSENT("call"),
       ENCJSENT("Function") "." ENCJSENT("toSource"),
       ENCJSENT("Function") "." ENCJSENT("toString"),
       ENCJSENT("Function") "." ENCJSENT("valueOf"),

       ENCJSENT("Generator") "." ENCJSENT("next"),

       ENCJSENT("Math") "." ENCJSENT("abs"),
       ENCJSENT("Math") "." ENCJSENT("acos"),
       ENCJSENT("Math") "." ENCJSENT("asin"),
       ENCJSENT("Math") "." ENCJSENT("atan"),
       ENCJSENT("Math") "." ENCJSENT("atan2"),
       ENCJSENT("Math") "." ENCJSENT("ceil"),
       ENCJSENT("Math") "." ENCJSENT("cos"),
       ENCJSENT("Math") "." ENCJSENT("exp"),
       ENCJSENT("Math") "." ENCJSENT("floor"),
       ENCJSENT("Math") "." ENCJSENT("log"),
       ENCJSENT("Math") "." ENCJSENT("max"),
       ENCJSENT("Math") "." ENCJSENT("min"),
       ENCJSENT("Math") "." ENCJSENT("pow"),
       ENCJSENT("Math") "." ENCJSENT("random"),
       ENCJSENT("Math") "." ENCJSENT("round"),
       ENCJSENT("Math") "." ENCJSENT("sin"),
       ENCJSENT("Math") "." ENCJSENT("sqrt"),
       ENCJSENT("Math") "." ENCJSENT("tan"),

       ENCJSENT("Number") "." ENCJSENT("toSource"),
       ENCJSENT("Number") "." ENCJSENT("toString"),
       ENCJSENT("Number") "." ENCJSENT("valueOf"),

       ENCJSENT("Object") "." ENCJSENT("anonymous"),
       ENCJSENT("Object") "." ENCJSENT("eval"),
       ENCJSENT("Object") "." ENCJSENT("toSource"),
       ENCJSENT("Object") "." ENCJSENT("toString"),
       ENCJSENT("Object") "." ENCJSENT("unwatch"),
       ENCJSENT("Object") "." ENCJSENT("valueOf"),
       ENCJSENT("Object") "." ENCJSENT("watch"),

       ENCJSENT("RegExp") "." ENCJSENT("compile"),
       ENCJSENT("RegExp") "." ENCJSENT("exec"),
       ENCJSENT("RegExp") "." ENCJSENT("test"),
       ENCJSENT("RegExp") "." ENCJSENT("toSource"),
       ENCJSENT("RegExp") "." ENCJSENT("toString"),
       ENCJSENT("RegExp") "." ENCJSENT("valueOf"),

       ENCJSENT("String") "." ENCJSENT("anchor"),
       ENCJSENT("String") "." ENCJSENT("big"),
       ENCJSENT("String") "." ENCJSENT("blink"),
       ENCJSENT("String") "." ENCJSENT("bold"),
       ENCJSENT("String") "." ENCJSENT("charAt"),
       ENCJSENT("String") "." ENCJSENT("charCodeAt"),
       ENCJSENT("String") "." ENCJSENT("concat"),
       ENCJSENT("String") "." ENCJSENT("fixed"),
       ENCJSENT("String") "." ENCJSENT("fontcolor"),
       ENCJSENT("String") "." ENCJSENT("fontsize"),
       ENCJSENT("String") "." ENCJSENT("fromCharCode"),
       ENCJSENT("String") "." ENCJSENT("indexOf"),
       ENCJSENT("String") "." ENCJSENT("italics"),
       ENCJSENT("String") "." ENCJSENT("lastIndexOf"),
       ENCJSENT("String") "." ENCJSENT("link"),
       ENCJSENT("String") "." ENCJSENT("match"),
       ENCJSENT("String") "." ENCJSENT("replace"),
       ENCJSENT("String") "." ENCJSENT("search"),
       ENCJSENT("String") "." ENCJSENT("slice"),
       ENCJSENT("String") "." ENCJSENT("small"),
       ENCJSENT("String") "." ENCJSENT("split"),
       ENCJSENT("String") "." ENCJSENT("strike"),
       ENCJSENT("String") "." ENCJSENT("sub"),
       ENCJSENT("String") "." ENCJSENT("substr"),
       ENCJSENT("String") "." ENCJSENT("substring"),
       ENCJSENT("String") "." ENCJSENT("sup"),
       ENCJSENT("String") "." ENCJSENT("toLowerCase"),
       ENCJSENT("String") "." ENCJSENT("toSource"),
       ENCJSENT("String") "." ENCJSENT("toString"),
       ENCJSENT("String") "." ENCJSENT("toUpperCase"),
       ENCJSENT("String") "." ENCJSENT("valueOf"),

       ENCJSENT("Global") "." ENCJSENT("anonymous"),
       ENCJSENT("Global") "." ENCJSENT("escape"),
       ENCJSENT("Global") "." ENCJSENT("eval"),
       ENCJSENT("Global") "." ENCJSENT("isFinite"),
       ENCJSENT("Global") "." ENCJSENT("isNaN"),
       ENCJSENT("Global") "." ENCJSENT("Number"),
       ENCJSENT("Global") "." ENCJSENT("parseFloat"),
       ENCJSENT("Global") "." ENCJSENT("parseInt"),
       ENCJSENT("Global") "." ENCJSENT("String"),
       ENCJSENT("Global") "." ENCJSENT("unescape"),

       ENCJSENT("Global") "." ENCJSENT("begin"),
       ENCJSENT("Global") "." ENCJSENT("main"),
       ENCJSENT("Global") "." ENCJSENT("end")
    };
    pJsMethods->assign(&methods[0], &methods[NUMELEMS(methods)]);
}

// Use GetInitRC() to get the constructor's return code.
JavaScript::JavaScript
(
    size_t RuntimeMaxBytes,
    size_t ContextStackSize
) :
    m_RuntimeMaxBytes(RuntimeMaxBytes),
    m_ContextStackSize(ContextStackSize),
    m_pRuntime(0),
    m_pGlobalObject(0),
    m_IsCtrlCHooked(false),
    m_OriginalCtrlCHandler(0),
    m_ImportDepth(0),
    m_ImportPath(),
    m_bDebuggerStarted(false),
    m_RunScriptJsdIndex(0),
    m_bStrict(false)
{
    MASSERT(0 <m_RuntimeMaxBytes);
    MASSERT(0 <m_ContextStackSize);

    m_ZombieMutex = Tasker::AllocMutex("ZombieLock", Tasker::mtxUnchecked);
    MASSERT(m_ZombieMutex);

    // Create the JavaScript runtime.
    m_pRuntime = JS_NewRuntime((uint32)m_RuntimeMaxBytes);
    if (0 == m_pRuntime)
    {
        m_InitRC = RC::COULD_NOT_CREATE_JAVASCRIPT_ENGINE;
        return;
    }

    // Get the JavaScript context (note that this will create the context
    // on the current thread as a context cannot yet exist)
    JSContext *pLwrContext;
    m_InitRC = GetContext(&pLwrContext);
    if (m_InitRC != OK)
    {
        return;
    }

    JSVersion jsVer = JS_GetVersion(pLwrContext);
    if (jsVer == JSVERSION_DEFAULT)
    {
        Printf(Tee::PriLow, "MODS using default Javascript version\n");
    }
    else
    {
        Printf(Tee::PriLow, "MODS using Javascript v%2.1f\n", (float)jsVer/100);
    }

#if defined(INCLUDE_BYTECODE)
    m_objectIdMapImpl = make_unique<ObjectIdsMapper>();
    JS_SetObjectHook(m_pRuntime,
                     ObjectIdsMapper::ObjectIdsMapperCallback,
                     m_objectIdMapImpl.get());
#endif

    // Create the global object.
    m_pGlobalObject = JS_NewObject(pLwrContext, &GlobalClass, 0, 0);
    if (0 == m_pGlobalObject)
    {
        m_InitRC = RC::COULD_NOT_CREATE_JS_OBJECT;
        return;
    }

#if defined(INCLUDE_BYTECODE)
    (*m_objectIdMapImpl)[1] = m_pGlobalObject;
#endif

    // Initialize the JavaScript standard classes.
    if (!JS_InitStandardClasses(pLwrContext, m_pGlobalObject))
    {
        m_InitRC = RC::COULD_NOT_INITIALIZE_JS_STANDARD_CLASSES;
        return;
    }

    // Create the global properties.
    RC rc;
    SPropertyVec::iterator ipProperty;
    SPropertyVec & GlobalSProperties = GetGlobalSProperties();
    for
    (
        ipProperty = GlobalSProperties.begin();
        ipProperty != GlobalSProperties.end();
        ++ipProperty
    )
    {
        if (OK != (rc = (*ipProperty)->DefineProperty(pLwrContext, m_pGlobalObject)))
        {
            m_InitRC = rc;
            return;
        }
    }

    // Create the global methods.
    SMethodVec::iterator ipMethod;
    SMethodVec & GlobalSMethods = GetGlobalSMethods();
    for
    (
        ipMethod = GlobalSMethods.begin();
        ipMethod != GlobalSMethods.end();
        ++ipMethod
    )
    {
        if (OK != (rc = (*ipMethod)->DefineMethod(pLwrContext, m_pGlobalObject)))
        {
            m_InitRC = rc;
            return;
        }
    }

    // Add non-global properties and methods to thier sobjects.
    SPropertyVec & NonGlobalSProperties = GetNonGlobalSProperties();
    for
    (
        ipProperty = NonGlobalSProperties.begin();
        ipProperty != NonGlobalSProperties.end();
        ++ipProperty
    )
    {
        (*ipProperty)->AddToSObject();
    }
    SMethodVec & NonGlobalSMethods = GetNonGlobalSMethods();
    for
    (
        ipMethod = NonGlobalSMethods.begin();
        ipMethod != NonGlobalSMethods.end();
        ++ipMethod
    )
    {
        (*ipMethod)->AddToSObject();
    }

    // Create objects.
    SObjectVec & SObjects = GetSObjects();
    for
    (
        SObjectVec::iterator ipSObject = SObjects.begin();
        ipSObject != SObjects.end();
        ++ipSObject
    )
    {
        if (OK != (rc = (*ipSObject)->DefineObject(pLwrContext, m_pGlobalObject)))
        {
            m_InitRC = rc;
            return;
        }
    }

    // Setup the logger.
    vector<string> JsMethods;
    InitJsMethods(&JsMethods);
    Log::Initialize(JsMethods);

    // Set the call hook function if we are logging return codes.
    if (CommandLine::LogReturnCodes())
    {
        m_CheckJSCallsHook.InitHook(m_pRuntime, this, &JavaScript::CheckJSCalls);
    }

    JS_SetBranchCallback(pLwrContext, BranchCallBack);

    if (CommandLine::DebugJavascript())
    {
        if (JS_FALSE == JS_StartModsDebugger(m_pRuntime, pLwrContext, m_pGlobalObject))
        {
            m_InitRC = RC::SOFTWARE_ERROR;
            return;
        }
        m_bDebuggerStarted = true;
        Printf(Tee::PriNormal, "MODS Javascript debugging enabled (context %p, object %p)\n",
               pLwrContext, m_pGlobalObject);
    }

    m_InitRC = OK;
}

JavaScript::~JavaScript()
{
    Log::Teardown();

    // Restore the original Ctrl-C handler.
    UnhookCtrlC();

    // Shut down all call hooks
    AllJSHookChains::ShutDownAllChains();

    // Destroy the JavaScript engine.
    Tasker::DestroyJsContexts();

    if (m_bDebuggerStarted)
    {
        JS_StopModsDebugger();
    }

    JS_DestroyRuntime(m_pRuntime);
    JS_ShutDown();

    Tasker::FreeMutex(m_ZombieMutex);
    m_ZombieMutex = nullptr;
}

void JavaScript::HookCtrlC()
{
    // Handle Ctrl-C (SIGINT) interrupts.
    if (!m_IsCtrlCHooked && (Platform::GetSimulationMode() == Platform::Hardware))
    {
        m_IsCtrlCHooked = true;
        m_OriginalCtrlCHandler = signal(SIGINT, CtrlCHandler);
    }
}

void JavaScript::UnhookCtrlC()
{
    if (m_IsCtrlCHooked)
    {
        signal(SIGINT, m_OriginalCtrlCHandler);
    }
}

// Get the return code from the constructor.
// If the contructor created all the JavaScript objects and initialized
// the JavaScript engine correctly, then RC == OK.
RC JavaScript::GetInitRC() const
{
    return m_InitRC;
}

//------------------------------------------------------------------------------
// ToJsval implementations
//------------------------------------------------------------------------------
RC JavaScript::ToJsval
(
    bool    Boolean,
    jsval * pValue
)
{
    MASSERT(pValue != 0);

    *pValue = BOOLEAN_TO_JSVAL(Boolean ? JS_TRUE : JS_FALSE);

    return OK;
}

RC JavaScript::ToJsval
(
    double d,
    jsval * pjsv
)
{
    Tasker::AttachThread attach;

    MASSERT(pjsv);
    JSContext *pContext;
    RC rc = GetContext(&pContext);

    if (OK == rc)
    {
        if (!JS_NewNumberValue(pContext, d, pjsv))
        {
            rc = RC::CANNOT_COLWERT_FLOAT_TO_JSVAL;
        }
    }
    return rc;
}

RC JavaScript::ToJsval
(
    const string & String,
    jsval * pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pValue != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    JSString * pJSString = JS_NewStringCopyN(pContext, String.c_str(), String.size());

    if (0 == pJSString)
        return RC::CANNOT_COLWERT_STRING_TO_JSVAL;

    *pValue = STRING_TO_JSVAL(pJSString);

    return OK;
}

RC JavaScript::ToJsval
(
    const char* str,
    jsval* pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pValue);
    MASSERT(str);

    RC rc;
    JSContext* pContext;
    CHECK_RC(GetContext(&pContext));

    JSString* pJSString = JS_NewStringCopyZ(pContext, str);

    if (!pJSString)
        return RC::CANNOT_COLWERT_STRING_TO_JSVAL;

    *pValue = STRING_TO_JSVAL(pJSString);

    return RC::OK;
}

RC JavaScript::ToJsval
(
    JsArray * pArray,
    jsval   * pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pArray != 0);
    MASSERT(pValue != 0);

    JSObject * pArrayObject;
    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    if (pArray->size())
    {
        pArrayObject = JS_NewArrayObject(pContext, (jsint) pArray->size(),
            &((*pArray)[0]));
    }
    else
    {
        pArrayObject = JS_NewArrayObject(pContext, 0, NULL);
    }

    if (0 == pArrayObject)
        return RC::CANNOT_COLWERT_ARRAY_TO_JSVAL;

    *pValue = OBJECT_TO_JSVAL(pArrayObject);

    return OK;
}

RC JavaScript::ToJsval
(
    JSObject * pObj,
    jsval * pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pValue != 0);

    *pValue = OBJECT_TO_JSVAL(pObj);

    return OK;
}

template<typename T>
RC JavaScript::ToJsval(const vector<T>& Vector, jsval* pValue)
{
    Tasker::AttachThread attach;

    RC rc = OK;
    jsval Elem;
    JsArray ElemArray;

    MASSERT(pValue);
    ElemArray.reserve(Vector.size());

    for (UINT32 i = 0; i < Vector.size(); ++i)
    {
        CHECK_RC(ToJsval(Vector[i], &Elem));
        ElemArray.push_back(Elem);
    }

    return ToJsval(&ElemArray, pValue);
}
template RC JavaScript::ToJsval<bool     >(const vector<bool     >&v, jsval* p);
template RC JavaScript::ToJsval<INT08    >(const vector<INT08    >&v, jsval* p);
template RC JavaScript::ToJsval<UINT08   >(const vector<UINT08   >&v, jsval* p);
template RC JavaScript::ToJsval<INT16    >(const vector<INT16    >&v, jsval* p);
template RC JavaScript::ToJsval<UINT16   >(const vector<UINT16   >&v, jsval* p);
template RC JavaScript::ToJsval<INT32    >(const vector<INT32    >&v, jsval* p);
template RC JavaScript::ToJsval<UINT32   >(const vector<UINT32   >&v, jsval* p);
// Skip: vector<INT64> is the same type as JsArray.
template RC JavaScript::ToJsval<UINT64   >(const vector<UINT64   >&v, jsval* p);
template RC JavaScript::ToJsval<FLOAT32  >(const vector<FLOAT32  >&v, jsval* p);
template RC JavaScript::ToJsval<double   >(const vector<double   >&v, jsval* p);
template RC JavaScript::ToJsval<string   >(const vector<string   >&v, jsval* p);
template RC JavaScript::ToJsval<JsArray* >(const vector<JsArray* >&v, jsval* p);
template RC JavaScript::ToJsval<JSObject*>(const vector<JSObject*>&v, jsval* p);

//------------------------------------------------------------------------------
// FromJsval implementations
//------------------------------------------------------------------------------
RC JavaScript::FromJsval
(
    jsval   Value,
    INT32 * pInteger
)
{
    Tasker::AttachThread attach;

    MASSERT(pInteger != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    if (!JS_ValueToECMAInt32(pContext, Value, reinterpret_cast<int32*>(pInteger)))
        return RC::CANNOT_COLWERT_JSVAL_TO_INTEGER;

    return OK;
}

RC JavaScript::FromJsval
(
    jsval    Value,
    UINT32 * pUinteger
)
{
    Tasker::AttachThread attach;

    MASSERT(pUinteger != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    if (!JS_ValueToECMAUint32(pContext, Value, reinterpret_cast<uint32*>(pUinteger)))
        return RC::CANNOT_COLWERT_JSVAL_TO_INTEGER;

    return OK;
}

RC JavaScript::FromJsval
(
    jsval    Value,
    UINT64 * pUinteger
)
{
    Tasker::AttachThread attach;

    MASSERT(pUinteger != 0);

    RC rc;
    if (JSVAL_IS_STRING(Value))
    {
        JSString *jsStr;
        CHECK_RC(FromJsval(Value, &jsStr));
        string s = JS_GetStringBytes(jsStr);
        errno = 0;
        *pUinteger = Utility::Strtoull(s.c_str(), nullptr, 0);
        if ((static_cast<UINT64>((std::numeric_limits<UINT64>::max)()) == *pUinteger) && errno)
            return RC::BAD_PARAMETER;
        return rc;
    }

    double d;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_ValueToNumber(pContext, Value, &d))
        return RC::CANNOT_COLWERT_JSVAL_TO_FLOAT;

    // JavaScript stores numbers in float64 values.
    //
    // As a speed optimization, it can compress some values to 32-bits, if
    // they are booleans or small signed integers (30-bit 2's complement).
    // But this is hidden, JS behaves *as if* the number was always float64.
    //
    // The bitwise operators in JavaScript read two numbers, colwert each to
    // integers, do the math, then colwert them back with sign extension.
    //
    // For example:
    //   var a = 0x80000000; // same as var a = 2147483648.0;
    //   Print(a);           // 2147483648
    //   Print(a|0);         //-2147483648 from the int32->float64 sign extension
    //
    // Because of this "feature" of JavaScript, we must be very careful when
    // colwerting from a negative float64 to an unsigned integer.

    if (d > 0)
    {
        // If the double is positive, a simple colwersion will do.
        *pUinteger = UINT64(d);
    }
    else
    {
        // Fall back to the standard UINT32 colwersion.
        uint32 u32val;
        if (!JS_ValueToECMAUint32(pContext, Value, &u32val))
            return RC::CANNOT_COLWERT_JSVAL_TO_INTEGER;

        *pUinteger = UINT64(u32val);
    }

    return OK;
}

RC JavaScript::FromJsval
(
    jsval   Value,
    bool  * pBoolean
)
{
    Tasker::AttachThread attach;

    MASSERT(pBoolean != 0);

    JSBool Bool;
    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_ValueToBoolean(pContext, Value, &Bool))
        return RC::CANNOT_COLWERT_JSVAL_TO_BOOLEAN;

    *pBoolean = (Bool == JS_TRUE);

    return OK;
}

RC JavaScript::FromJsval
(
    jsval    Value,
    double * pDouble
)
{
    Tasker::AttachThread attach;

    MASSERT(pDouble != 0);
    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    if (!JS_ValueToNumber(pContext, Value, pDouble))
        return RC::CANNOT_COLWERT_JSVAL_TO_FLOAT;

    return OK;
}

RC JavaScript::FromJsval
(
    jsval    Value,
    string * pString
)
{
    Tasker::AttachThread attach;

    MASSERT(pString != 0);
    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    JSString * pJSString = JS_ValueToString(pContext, Value);
    if (!pJSString)
        return RC::CANNOT_COLWERT_JSVAL_TO_STRING;
    JSContext *ctx = nullptr;
    CHECK_RC(GetContext(&ctx));
    *pString = DeflateJSString(ctx, pJSString);

    *pString = JS_GetStringBytes(pJSString);
    return OK;
}

RC JavaScript::FromJsval
(
    jsval      value,
    JSString **str
)
{
    Tasker::AttachThread attach;

    MASSERT(str != 0);
    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    *str = JS_ValueToString(pContext, value);
    if (!*str)
       return RC::CANNOT_COLWERT_JSVAL_TO_STRING;

    return OK;
}

RC JavaScript::FromJsval
(
    jsval     Value,
    JsArray * pArray
)
{
    Tasker::AttachThread attach;

    MASSERT(pArray != 0);
    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    // Colwert jsval to an array object.
    JSObject * pArrayObject = 0;
    if
    (
            !JS_ValueToObject(pContext, Value, &pArrayObject)
        || (0 == pArrayObject)
    )
    {
        return RC::CANNOT_COLWERT_JSVAL_TO_ARRAY;
    }

    if (!JS_IsArrayObject(pContext, pArrayObject))
        return RC::CANNOT_COLWERT_JSVAL_TO_ARRAY;

    UINT32 NumElements = 0;
    if (!JS_GetArrayLength(pContext, pArrayObject,
            reinterpret_cast<uint32*>(&NumElements)))
        return RC::CANNOT_COLWERT_JSVAL_TO_ARRAY;

    JsArray Vals(NumElements);

    for (UINT32 i = 0; i < NumElements; ++i)
    {
        if (!JS_GetElement(pContext, pArrayObject, i, &Vals[i]))
            return RC::CANNOT_COLWERT_JSVAL_TO_ARRAY;
    }

    *pArray = Vals;

    return OK;
}

RC JavaScript::FromJsval
(
    jsval       Value,
    JSObject ** ppObject
)
{
    Tasker::AttachThread attach;

    MASSERT(ppObject != 0);

    *ppObject = 0;

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    // Colwert jsval to an object.
    if
    (
            !JS_ValueToObject(pContext, Value, ppObject)
        || (0 == *ppObject)
    )
    {
        return RC::CANNOT_COLWERT_JSVAL_TO_OJBECT;
    }

    return OK;
}

RC JavaScript::FromJsval
(
    jsval         Value,
    JSFunction ** pFunction
)
{
    Tasker::AttachThread attach;

    MASSERT(pFunction != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    // Colwert jsval to a function.
    JSFunction * Function = JS_ValueToFunction(pContext, Value);
    if (!Function)
        return RC::CANNOT_COLWERT_JSVAL_TO_FUNCTION;

    *pFunction = Function;
    return OK;
}

template<typename T>
RC JavaScript::FromJsval(jsval Value, vector<T>* pVector)
{
    Tasker::AttachThread attach;

    RC rc = OK;
    T Elem;
    JsArray ElemArray;

    MASSERT(pVector);
    pVector->clear();
    CHECK_RC(FromJsval(Value, &ElemArray));
    pVector->reserve(ElemArray.size());

    for (UINT32 i = 0; i < ElemArray.size(); ++i)
    {
        CHECK_RC(FromJsval(ElemArray[i], &Elem));
        pVector->push_back(Elem);
    }

    return rc;
}
template RC JavaScript::FromJsval<bool     >(jsval jsv, vector<bool     >*pv);
template RC JavaScript::FromJsval<double   >(jsval jsv, vector<double   >*pv);
template RC JavaScript::FromJsval<string   >(jsval jsv, vector<string   >*pv);
template RC JavaScript::FromJsval<JSObject*>(jsval jsv, vector<JSObject*>*pv);
template RC JavaScript::FromJsval<INT08    >(jsval jsv, vector<INT08    >*pv);
template RC JavaScript::FromJsval<UINT08   >(jsval jsv, vector<UINT08   >*pv);
template RC JavaScript::FromJsval<INT16    >(jsval jsv, vector<INT16    >*pv);
template RC JavaScript::FromJsval<UINT16   >(jsval jsv, vector<UINT16   >*pv);
template RC JavaScript::FromJsval<INT32    >(jsval jsv, vector<INT32    >*pv);
template RC JavaScript::FromJsval<UINT32   >(jsval jsv, vector<UINT32   >*pv);
// Skip: vector<INT64> is same type as JsArray.
template RC JavaScript::FromJsval<UINT64   >(jsval jsv, vector<UINT64   >*pv);
template RC JavaScript::FromJsval<FLOAT32  >(jsval jsv, vector<FLOAT32  >*pv);

namespace
{  // anonymous namespace
template<typename T>
RC FromJsvalU(JavaScript * pJ, jsval jsv, T* pv)
{
    Tasker::AttachThread attach;

    // For types UINT08, UINT16:
    // extract a UINT32 and return error if value is out of range.

    UINT32 u;
    RC rc = pJ->FromJsval(jsv, &u);
    if (OK == rc)
    {
        // Unfortunately, gcc 2.95 does not support std::numeric_limits.
        const UINT32 maxT = (1 << (8*sizeof(T))) - 1;

        if (u > maxT)
            rc = RC::CANNOT_COLWERT_JSVAL_TO_INTEGER;
        else
            *pv = static_cast<T>(u);
    }
    return rc;
}
template<typename T>
RC FromJsvalI(JavaScript * pJ, jsval jsv, T* pv)
{
    Tasker::AttachThread attach;

    // For types INT08, INT16:
    // extract an INT32 and return error if value is out of range.
    INT32 i;
    RC rc = pJ->FromJsval(jsv, &i);
    if (OK == rc)
    {
        // Unfortunately, gcc 2.95 does not support std::numeric_limits.
        const INT32 maxT = (1 << (8*sizeof(T) -1)) - 1;
        const INT32 minT = -maxT - 1;

        if ((i > maxT) || (i < minT))
            rc = RC::CANNOT_COLWERT_JSVAL_TO_INTEGER;
        else
            *pv = static_cast<T>(i);
    }
    return rc;
}
template<typename T>
RC FromJsvalD(JavaScript * pJ, jsval jsv, T* pv)
{
    Tasker::AttachThread attach;

    // For INT64 and FLOAT32:
    // extract a double and downcast ignoring range/precision.
    double d;
    RC rc = pJ->FromJsval(jsv, &d);
    if (OK == rc)
        *pv = static_cast<T>(d);
    return rc;
}
} // close anonymous namespace

RC JavaScript::FromJsval(jsval v, UINT08 * p) { return FromJsvalU(this, v, p); }
RC JavaScript::FromJsval(jsval v, UINT16 * p) { return FromJsvalU(this, v, p); }
RC JavaScript::FromJsval(jsval v, INT08 * p)  { return FromJsvalI(this, v, p); }
RC JavaScript::FromJsval(jsval v, INT16 * p)  { return FromJsvalI(this, v, p); }
RC JavaScript::FromJsval(jsval v, FLOAT32 * p) { return FromJsvalD(this, v, p); }

RC JavaScript::FromJsval
(
    jsval v,
    INT64 * pInt
)
{
    Tasker::AttachThread attach;

    if (JSVAL_IS_STRING(v))
    {
        RC rc;
        JSString *jsStr;
        CHECK_RC(FromJsval(v, &jsStr));
        string s = JS_GetStringBytes(jsStr);
        errno = 0;
        UINT64 tempVal = Utility::Strtoull(s.c_str(), nullptr, 0);
        if ((static_cast<UINT64>((std::numeric_limits<UINT64>::max)()) == tempVal) && errno)
            return RC::BAD_PARAMETER;
        *pInt = static_cast<INT64>(tempVal);
        return rc;
    }

    return FromJsvalD(this, v, pInt);
}

size_t JavaScript::UnpackArgs
(
    const jsval * pArgs
    ,size_t nArgs
    ,const char * Format
    ,...
)
{
    va_list restOfArgs;
    va_start(restOfArgs, Format);
    const size_t result = UnpackVA(pArgs, nArgs, Format, &restOfArgs);
    va_end(restOfArgs);
    return result;
}

size_t JavaScript::UnpackJsArray
(
    const JsArray & jsa
    ,const char * Format
    ,...
)
{
    if (0 == jsa.size())
        return 0;    // Unsafe to take &jsa[0] if size is 0.

    va_list restOfArgs;
    va_start(restOfArgs, Format);
    const size_t result = UnpackVA(&jsa[0], jsa.size(), Format, &restOfArgs);
    va_end(restOfArgs);
    return result;
}

size_t JavaScript::UnpackJsval
(
    jsval src
    ,const char * Format
    ,...
)
{
    JsArray jsa;
    if (OK != FromJsval(src, &jsa))
        return 0;
    if (0 == jsa.size())
        return 0;    // Unsafe to take &jsa[0] if size is 0.

    va_list restOfArgs;
    va_start(restOfArgs, Format);
    const size_t result = UnpackVA(&jsa[0], jsa.size(), Format, &restOfArgs);
    va_end(restOfArgs);
    return result;
}

namespace
{ // anonymous namespace
template <typename T>
RC UnpackHelper(va_list *pv, jsval jsv, JavaScript * pJ)
{
    T * pT = va_arg(*pv, T*);
    return pJ->FromJsval(jsv, pT);
}
RC UnpackOne(char c, va_list *pv, jsval j, JavaScript * pJ)
{
    switch (c)
    {
        case '?': return UnpackHelper< bool >     (pv, j, pJ);
        case 'b': return UnpackHelper< INT08 >    (pv, j, pJ);
        case 'B': return UnpackHelper< UINT08 >   (pv, j, pJ);
        case 'h': return UnpackHelper< INT16 >    (pv, j, pJ);
        case 'H': return UnpackHelper< UINT16 >   (pv, j, pJ);
        case 'i': return UnpackHelper< INT32 >    (pv, j, pJ);
        case 'I': return UnpackHelper< UINT32 >   (pv, j, pJ);
        case 'l': return UnpackHelper< INT64 >    (pv, j, pJ);
        case 'L': return UnpackHelper< UINT64 >   (pv, j, pJ);
        case 'f': return UnpackHelper< FLOAT32 >  (pv, j, pJ);
        case 'd': return UnpackHelper< FLOAT64 >  (pv, j, pJ);
        case 's': return UnpackHelper< string >   (pv, j, pJ);
        case 'a': return UnpackHelper< JsArray >  (pv, j, pJ);
        case 'o': return UnpackHelper< JSObject* >(pv, j, pJ);
        default:
            return RC::BAD_PARAMETER;
    }
}
} // close anonymous namespace

size_t JavaScript::UnpackVA
(
    const jsval * pJsvals
    ,size_t       nJsvals
    ,const char * Format
    ,va_list    * pRestOfArgs
)
{
    MASSERT(Format);
    RC rc;
    size_t i = 0;
    const size_t maxi = min(strlen(Format), nJsvals);
    for (/**/; i < maxi; i++)
    {
        rc = UnpackOne(Format[i], pRestOfArgs, pJsvals[i], this);
        if (OK != rc)
        {
            Printf(Tee::PriError, "Failed to unpack: index %u, format %c\n",
                   (unsigned)i, Format[i]);
            return i;
        }
    }

    return i;
}

RC JavaScript::UnpackFields
(
    JSObject * pObj
    ,const char * Format
    ,...
)
{
    va_list restOfArgs;
    va_start(restOfArgs, Format);
    RC rc = UnpackFieldsVA(false, pObj, Format, &restOfArgs);
    va_end(restOfArgs);
    return rc;
}

RC JavaScript::UnpackFieldsOptional
(
    JSObject * pObj
    ,const char * Format
    ,...
)
{
    va_list restOfArgs;
    va_start(restOfArgs, Format);
    RC rc = UnpackFieldsVA(true, pObj, Format, &restOfArgs);
    va_end(restOfArgs);
    return rc;
}

RC JavaScript::UnpackFieldsVA
(
    bool          allOptional
    ,JSObject *   pObj
    ,const char * Format
    ,va_list    * pRestOfArgs
)
{
    MASSERT(Format);

    const char * fieldName = "";
    RC rc;
    size_t i = 0;
    const size_t maxi = strlen(Format);
    bool * skippedField = nullptr;

    for (/**/; i < maxi; i++)
    {
        // A '~' prefix marks an optional field.
        bool optionalField = allOptional;
        if (Format[i] == '~')
        {
            optionalField = true;
            i++;
            if (i >= maxi)
            {
                Printf(Tee::PriError, "Unmatched ~ in UnpackFields\n");
                return RC::BAD_PARAMETER;
            }
        }

        // Get field name, get property from JS object.
        fieldName = va_arg(*pRestOfArgs, const char *);
        jsval jsv;
        RC rc = GetPropertyJsval(pObj, fieldName, &jsv);
        if (OK != rc)
        {
            if (optionalField)
            {
                // Skip over the destination argument.
                // Assumes all pointers are the same size, use bool*.
                skippedField = va_arg(*pRestOfArgs, bool*);
                continue;
            }

            Printf(Tee::PriError, "Failed to extract property: field \"%s\", format %c\n",
                   fieldName, Format[i]);
            break;
        }

        // Colwert the jsval to C++ type, store in pointer.
        rc = UnpackOne(Format[i], pRestOfArgs, jsv, this);
        if (OK != rc)
        {
            if (optionalField)
                continue;

            Printf(Tee::PriError, "Failed to colwert jsvalue: field \"%s\", format %c\n",
                   fieldName, Format[i]);
            break;
        }
    }
    // Need to use the result of va_arg somehow to avoid
    // compiler warnings for "value computed is not used".
    if (skippedField != nullptr)
    {
        Printf(Tee::PriDebug, "UnpackFields skipped a field\n");
    }

    return rc;
}

namespace
{ // anonymous namespace
template <typename T>
RC PackHelper(va_list *pv, jsval *pjv, JavaScript * pJ)
{
    T t = va_arg(*pv, T);
    return pJ->ToJsval(t, pjv);
}
RC PackOne(char c, va_list *pv, jsval *pjv, JavaScript * pJ)
{
    switch (c)
    {
        case '?': // bool promotes to int in ...
        case 'b': // char promotes to int in ...
        case 'B': // uchar promotes to int in ...
        case 'h': // short promotes to int in ...
        case 'H': // ushort promotes to int in ...
        {
            return PackHelper< int >      (pv, pjv, pJ);
        }
        case 'i': return PackHelper< INT32 >    (pv, pjv, pJ);
        case 'I': return PackHelper< UINT32 >   (pv, pjv, pJ);
        case 'l': return PackHelper< INT64 >    (pv, pjv, pJ);
        case 'L': return PackHelper< UINT64 >   (pv, pjv, pJ);

        case 'f': // float promotes to double in ...
        case 'd':
        {
            return PackHelper< FLOAT64 >  (pv, pjv, pJ);
        }
        case 's':
        {
            // Can't pass a string object through ..., must use const char*
            const char * s = va_arg(*pv, const char*);
            return pJ->ToJsval(s, pjv);
        }
        case 'a': return PackHelper< JsArray* > (pv, pjv, pJ);
        case 'o': return PackHelper< JSObject* >(pv, pjv, pJ);
        default:
            return RC::BAD_PARAMETER;
    }
}
} // close anonymous namespace

RC JavaScript::PackJsArray
(
    JsArray * pAry,
    const char * Format,
    ...
)
{
    MASSERT(pAry);
    MASSERT(Format);
    va_list restOfArgs;
    va_start(restOfArgs, Format);

    size_t i = 0;
    const size_t maxi = strlen(Format);
    RC rc;

    pAry->reserve(pAry->size() + maxi);

    for (/**/; i < maxi; i++)
    {
        jsval jsv;
        RC rc = PackOne(Format[i], &restOfArgs, &jsv, this);
        if (OK != rc)
        {
            Printf(Tee::PriError,
                   "Failed to colwert value to jsval, index %u format %c\n",
                   static_cast<unsigned>(i), Format[i]);
            break;
        }
        pAry->push_back(jsv);
    }
    va_end(restOfArgs);
    return rc;
}

RC JavaScript::PackFields
(
    JSObject * pObj,
    const char * Format,
    ...
)
{
    MASSERT(pObj);
    MASSERT(Format);
    va_list restOfArgs;
    va_start(restOfArgs, Format);

    RC rc;
    const char * msg = "";
    const char * fieldName = "";
    size_t i = 0;
    const size_t maxi = strlen(Format);

    for (/**/; i < maxi; i++)
    {
        // Get field name, colwert value to jsval.
        fieldName = va_arg(restOfArgs, const char *);
        jsval jsv;
        rc = PackOne(Format[i], &restOfArgs, &jsv, this);
        if (OK != rc)
        {
            msg = "colwert value to jsval";
            break;
        }

        // Create the JS property on the parent object.
        rc = SetPropertyJsval(pObj, fieldName, jsv);
        if (OK != rc)
        {
            msg = "store property";
            break;
        }
    }
    va_end(restOfArgs);
    if (rc)
    {
        Printf(Tee::PriError, "Failed to %s, field \"%s\", format %c\n",
               msg, fieldName, Format[i]);
    }
    return rc;
}

// Get an SObject's SProperty value.
RC JavaScript::GetPropertyJsval
(
    const SObject   & Object,
    const SProperty & Property,
    jsval           * pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pValue != 0);

    JSObject   * JsObject     = Object.GetJSObject();
    const char * PropertyName = Property.GetName();

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    JSBool foundProp = JS_FALSE;
    if (JS_HasProperty(pContext, JsObject, PropertyName, &foundProp) &&
        (foundProp == JS_FALSE))
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    if (!JS_GetProperty(pContext, JsObject, PropertyName, pValue))
        return RC::ILWALID_OBJECT_PROPERTY;

    // If the property is not defined in the object's scope,
    // or in its prototype links, *pValue is set to JSVAL_VOID.
    if (JSVAL_VOID == *pValue)
        return RC::ILWALID_OBJECT_PROPERTY;

    return OK;
}

RC JavaScript::SetPropertyJsval
(
    const SObject   & Object,
    const SProperty & Property,
    jsval             Value
)
{
    Tasker::AttachThread attach;

    JSObject   * JsObject     = Object.GetJSObject();
    const char * PropertyName = Property.GetName();

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_SetProperty(pContext, JsObject, PropertyName, &Value))
        return RC::ILWALID_OBJECT_PROPERTY;

    return OK;
}

// Get a JSObject property.
RC JavaScript::GetPropertyJsval
(
    JSObject   * pObject,
    const char * Property,
    jsval      * pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pObject  != 0);
    MASSERT(Property != 0);
    MASSERT(pValue   != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    JSBool foundProp = JS_FALSE;
    if (JS_HasProperty(pContext, pObject, Property, &foundProp) &&
        (foundProp == JS_FALSE))
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    if (!JS_GetProperty(pContext, pObject, Property, pValue))
        return RC::ILWALID_OBJECT_PROPERTY;

    // If the property is not defined in the object's scope,
    // or in its prototype links, *pValue is set to JSVAL_VOID.
    if (JSVAL_VOID == *pValue)
        return RC::ILWALID_OBJECT_PROPERTY;

    return OK;
}

RC JavaScript::GetPropertyJsval(JSObject *pObject, JSString *Property, jsval *pValue)
{
    Tasker::AttachThread attach;

    MASSERT(pObject != 0);
    MASSERT(Property != 0);
    MASSERT(pValue != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    const jschar *pCharProp = JSSTRING_CHARS(Property);
    size_t charPropLen = JSSTRING_LENGTH(Property);

    JSBool foundProp = JS_FALSE;
    if (JS_HasUCProperty(pContext, pObject, pCharProp, charPropLen, &foundProp) &&
        (foundProp == JS_FALSE))
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    if (!JS_GetUCProperty(pContext, pObject, pCharProp, charPropLen, pValue))
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    // If the property is not defined in the object's scope,
    // or in its prototype links, *pValue is set to JSVAL_VOID.
    if (JSVAL_VOID == *pValue)
        return RC::ILWALID_OBJECT_PROPERTY;

    return OK;
}

// Set a JSObject property.
RC JavaScript::SetPropertyJsval
(
    JSObject   * pObject,
    const char * Property,
    jsval        Value
)
{
    Tasker::AttachThread attach;

    MASSERT(pObject  != 0);
    MASSERT(Property != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_SetProperty(pContext, pObject, Property, &Value))
        return RC::ILWALID_OBJECT_PROPERTY;

    return OK;
}

RC JavaScript::SetPropertyJsval(JSObject *pObject, JSString *Property, jsval Value)
{
    Tasker::AttachThread attach;

    MASSERT(pObject  != 0);
    MASSERT(Property != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_SetUCProperty(pContext, pObject,
                          JSSTRING_CHARS(Property), JSSTRING_LENGTH(Property), &Value))
        return RC::ILWALID_OBJECT_PROPERTY;

    return OK;
}

// Set Array's element at index.
RC JavaScript::SetElementJsval
(
    JSObject * pArrayObject,
    INT32      Index,
    jsval      Value
)
{
    Tasker::AttachThread attach;

    MASSERT(pArrayObject != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_SetElement(pContext, pArrayObject, Index, &Value))
        return RC::CANNOT_SET_ELEMENT;

    return OK;
}

RC JavaScript::GetElementJsval
(
    JSObject * pArrayObject,
    INT32      Index,
    jsval      *pValue
)
{
    Tasker::AttachThread attach;

    MASSERT(pArrayObject != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (!JS_GetElement(pContext, pArrayObject, Index, pValue))
        return RC::CANNOT_GET_ELEMENT;

    return OK;
}

RC JavaScript::SetElement(JSObject *o, INT32 i, jsval j)
{
    Tasker::AttachThread attach;

    // Legacy code expects "no-colwersion" behavior for type jsval.
    // This means INT64 colwersions won't happen, surprise!
    // We should switch no-colwersion cases to call SetElementJsval directly.
    return SetElementJsval(o, i, j);
}

// Create an empty JSObject property.
JS_CLASS(Generic);

RC JavaScript::DefineProperty
(
    JSObject   * pObject,
    const char * PropertyName,
    JSObject  ** ppNewObject
)
{
    Tasker::AttachThread attach;

    MASSERT(pObject      != 0);
    MASSERT(PropertyName != 0);
    MASSERT(ppNewObject  != 0);

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    *ppNewObject = JS_DefineObject(pContext, pObject, PropertyName,
        &GenericClass, 0, JSPROP_ENUMERATE);

    if (!*ppNewObject)
    {
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    return OK;
}

// Get all the object properties.
RC JavaScript::GetProperties
(
    JSObject       * pObject,
    vector<string> * pProperties
)
{
    Tasker::AttachThread attach;

    MASSERT(pObject     != 0);
    MASSERT(pProperties != 0);

    RC rc;

    pProperties->clear();

    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    // Enumerate the object to get all the property ids.
    JSIdArray * pIds = JS_Enumerate(pContext, pObject);
    if (0 == pIds)
        return RC::CANNOT_ENUMERATE_OBJECT;

    DEFER
    {
        JS_DestroyIdArray(pContext, pIds);
    };

    // Store the properties.
    for (INT32 i = 0; i < pIds->length; ++i)
    {
        jsval Val;
        if (!JS_IdToValue(pContext, pIds->vector[i], &Val))
        {
            return RC::CANNOT_GET_ELEMENT;
        }

        string Property;
        rc = FromJsval(Val, &Property);
        if (rc != OK)
        {
            return rc;
        }

        pProperties->push_back(Property);
    }

    return rc;
}

RC JavaScript::GetProperties(JSObject *pObject, vector<JSString *> *pProperties)
{
    Tasker::AttachThread attach;

    MASSERT(pObject     != 0);
    MASSERT(pProperties != 0);

    RC rc;

    pProperties->clear();

    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    // Enumerate the object to get all the property ids.
    JSIdArray * pIds = JS_Enumerate(pContext, pObject);
    if (0 == pIds)
        return RC::CANNOT_ENUMERATE_OBJECT;

    DEFER
    {
        JS_DestroyIdArray(pContext, pIds);
    };

    // Store the properties.
    for (INT32 i = 0; i < pIds->length; ++i)
    {
        jsid id = pIds->vector[i];
        jsval Val;
        if (!JS_IdToValue(pContext, id, &Val))
        {
            return RC::CANNOT_GET_ELEMENT;
        }

        JSString *propName;
        rc = FromJsval(Val, &propName);
        if (rc != OK)
        {
            return rc;
        }
        pProperties->push_back(propName);
    }

    return rc;
}

void JavaScript::DeferredDelete(unique_ptr<GenericDeleterBase> pObject)
{
    Tasker::MutexHolder lock(m_ZombieMutex);

    m_ZombieObjects.push_back(move(pObject));
}

void JavaScript::ClearZombies()
{
    Tasker::MutexHolder lock(m_ZombieMutex);

    m_ZombieObjects.clear();
}

// Parse and run the script passed in the string 'Script'.
// The script's return value is returned in 'pReturlwalue'.
RC JavaScript::RunScript
(
    string  Script,
    jsval * pReturlwalue /* = 0 */
)
{
    ClearCriticalEvent();

    RC rc;
    JSContext *pContext;
    string     scriptFileName;

    scriptFileName = Utility::StrPrintf("rs%05d.js", m_RunScriptJsdIndex++);

    CHECK_RC(WriteDebugFile(scriptFileName, Script));

    CHECK_RC(GetContext(&pContext));
    JSScript * pScript = JS_CompileScript(pContext, m_pGlobalObject,
        &Script[0], Script.size(), scriptFileName.c_str(), 1);

    if (0 == pScript)
    {
        return RC::COULD_NOT_COMPILE_FILE;
    }

    DEFER
    {
        JS_DestroyScript(pContext, pScript);
    };

    // Execute the compiled script.
    jsval Returlwalue;
    if (!JS_ExelwteScript(pContext, m_pGlobalObject, pScript, &Returlwalue))
    {
        return RC::SCRIPT_FAILED_TO_EXELWTE;
    }
    if (pReturlwalue)
    {
        *pReturlwalue = Returlwalue;

        // WARNING: Returlwalue is not reachable via the JS garbage collector!
        // JS_GC() will destroy it unless it is a bool or small integer.
        // Apparently pReturlwalue is null except for one call from ModsMain
        // which is expecting an RC stored here.
        MASSERT(!JSVAL_IS_GCTHING(Returlwalue));
    }
    JS_GC(pContext);

    // Delete objects deferred from finalize functions
    ClearZombies();

    return OK;
}

// Preprocess, compile, and execute a script stored in an external file.
RC JavaScript::Import
(
    string     FileName,
    JSObject * pReturlwals, /* =0 */
    bool       mustExist    /* =true */
)
{
    PROFILE_ZONE(GENERIC)

    MASSERT(FileName != "");

    // If this is the top level import file, store it's path.
    if (0 == m_ImportDepth)
    {
        string::size_type Pos = FileName.rfind('/');
        if (Pos != string::npos)
        {
            m_ImportPath = FileName.substr(0, Pos + 1);
        }
        else
        {
            m_ImportPath = "";
        }
    }

    RC rc = OK;

    string unredactedName = Utility::RestoreString(FileName);
    if (FileName != unredactedName)
    {
        // Check for unredacted name first so user can drop-in unredacted, unencrypted files.
        if (Xp::DoesFileExist(unredactedName))
        {
            FileName = unredactedName;
        }
        else
        {
            vector<string> paths;
            paths.push_back(m_ImportPath);
            paths.push_back(CommandLine::ProgramPath());
            paths.push_back(CommandLine::ScriptFilePath());
            string path = Utility::FindFile(unredactedName, paths);
            if (path != "")
            {
                FileName = path + unredactedName;
            }
        }
    }

    // Check if the file exists.
    if (!Xp::DoesFileExist(FileName))
    {
        // Search for the file.
        vector<string> Paths;
        Paths.push_back(m_ImportPath);
        Paths.push_back(CommandLine::ProgramPath());
        Paths.push_back(CommandLine::ScriptFilePath());
        string Path = Utility::FindFile(FileName, Paths);
        if ("" == Path)
        {
            const INT32 pri = mustExist ? Tee::PriNormal : Tee::PriLow;
            // if we can't find the file, search for a ".jse"
            // encrypted script, ".dbe" encrypted boards db file, or
            // ".spe" encrypted spec file.
            const string::size_type lastDot = FileName.rfind('.');
            const string suffix =
                (lastDot == string::npos) ? "" :
                Utility::ToLowerCase(FileName.substr(lastDot));
            if (suffix == ".js" || suffix == ".db" || suffix == ".spc")
            {
                const string encryptedName = (suffix.length() > 3 ?
                                              FileName.substr(0, lastDot + 3) :
                                              FileName) + "e";
                rc = Import(encryptedName, pReturlwals, mustExist);
                if (rc == RC::FILE_DOES_NOT_EXIST)
                    Printf(pri, "%s does not exist.\n", FileName.c_str());
                return rc;
            }
            else
            {
                Printf(pri, "%s does not exist.\n", FileName.c_str());
                return mustExist ? RC::FILE_DOES_NOT_EXIST : OK;
            }
        }
        FileName = Path + FileName;
    }

    INT32 i;
    for (i = 0; i < m_ImportDepth; ++i)
        Printf(Tee::PriLow, "---");

    Printf(Tee::PriLow, ">%s\n", FileName.c_str());

    ++m_ImportDepth;

    // Preprocess the file.
    vector<char> PreprocessedBuffer;

    JSContext *pContext;
    Preprocessor::StrictMode jsStrictMode;
    CHECK_RC(GetContext(&pContext));
    Printf(Tee::PriLow, "Preprocessing %s.\n", FileName.c_str());
    if (OK != (rc = Preprocessor::Preprocess(FileName.c_str(),
                                             &PreprocessedBuffer,
                                             0, 0, false,
                                             false,
                                             true,
                                             &jsStrictMode)))
    {
        Printf(Tee::PriError, "Failed while preprocessing %s.\n",
            FileName.c_str());
        --m_ImportDepth;
        return rc;
    }

    // Redact imported script contents so that they will work with existing redaction
    string preprocessedString(PreprocessedBuffer.begin(), PreprocessedBuffer.end());
    preprocessedString = Utility::RedactString(preprocessedString);
    PreprocessedBuffer.assign(preprocessedString.begin(), preprocessedString.end());

    if (jsStrictMode != Preprocessor::StrictModeUnspecified)
    {
       UINT32 jsOpts = JS_GetOptions(pContext);
       switch (jsStrictMode)
       {
           default:
           case Preprocessor::StrictModeOff:
               jsOpts &= ~JSOPTION_STRICT;
               break;
           case Preprocessor::StrictModeOn:
               jsOpts |= JSOPTION_STRICT;
               break;
           case Preprocessor::StrictModeEngr:
               if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
                   jsOpts |= JSOPTION_STRICT;
               else
                   jsOpts &= ~JSOPTION_STRICT;
               break;
       }
       // Once strict is on, it is always on
       if (jsOpts & JSOPTION_STRICT)
           m_bStrict = true;
       JS_SetOptions(pContext, jsOpts);
    }

    if (OK != (rc = WriteDebugFile(FileName, PreprocessedBuffer)))
    {
        --m_ImportDepth;
        return rc;
    }

    // Compile the file.
    Printf(Tee::PriLow, "Compiling %s.\n", FileName.c_str());
    JSScript * pScript = JS_CompileScript(pContext, m_pGlobalObject,
        &PreprocessedBuffer[0], PreprocessedBuffer.size(), FileName.c_str(), 1);
    Printf(Tee::PriLow, "Compiled %s, pScript = 0x%p.\n", FileName.c_str(), pScript);

    if (0 == pScript)
    {
        --m_ImportDepth;
        return RC::COULD_NOT_COMPILE_FILE;
    }

    DEFER
    {
        JS_DestroyScript(pContext, pScript);
    };

    // Execute the compiled script.
    Printf(Tee::PriLow, "Exelwting %s.\n", FileName.c_str());
    jsval Returlwalue;
    if (!JS_ExelwteScript(pContext, m_pGlobalObject, pScript, &Returlwalue))
    {
        --m_ImportDepth;
        return RC::SCRIPT_FAILED_TO_EXELWTE;
    }

    --m_ImportDepth;

    Printf(Tee::PriLow, "<");
    for (i = 0; i < m_ImportDepth; ++i)
        Printf(Tee::PriLow, "---");
    Printf(Tee::PriLow, "%s\n", FileName.c_str());

    if (pReturlwals)
    {
        CHECK_RC(SetElement(pReturlwals, 0, Returlwalue));
    }
    JS_GC(pContext);

    // Delete objects deferred from finalize functions
    ClearZombies();

    return OK;
}

// Preprocess, compile, and execute the string passed.
RC JavaScript::Eval
(
    string str,
    JSObject * pReturlwals  /* =0 */
)
{
    MASSERT(str != "");

    RC rc = OK;

    // Preprocess the string.
    vector<char> PreprocessedBuffer;

    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    Printf(Tee::PriLow, "Preprocessing \"%s\".\n", str.c_str());
    if (OK != (rc = Preprocessor::Preprocess(str.c_str(),
                                             &PreprocessedBuffer,
                                             0, 0, false,
                                             false,
                                             false,
                                             nullptr)))
    {
        Printf(Tee::PriError, "Failed while preprocessing \"%s\".\n",
            str.c_str());
        return rc;
    }

    CHECK_RC(WriteDebugFile("Eval.js", PreprocessedBuffer));

    // Compile the string.
    Printf(Tee::PriLow, "Compiling \"%s\".\n", &PreprocessedBuffer[0]);
    JSScript * pScript = JS_CompileScript(pContext, m_pGlobalObject,
        &PreprocessedBuffer[0], PreprocessedBuffer.size(), "Eval.js", 1);

    if (0 == pScript)
    {
        return RC::COULD_NOT_COMPILE_FILE;
    }

    DEFER
    {
        JS_DestroyScript(pContext, pScript);
    };

    // Execute the compiled script.
    Printf(Tee::PriLow, "Exelwting \"%s\".\n", str.c_str());
    jsval Returlwalue;
    if (!JS_ExelwteScript(pContext, m_pGlobalObject, pScript, &Returlwalue))
    {
        return RC::SCRIPT_FAILED_TO_EXELWTE;
    }
    if (pReturlwals)
    {
        CHECK_RC(SetElement(pReturlwals, 0, Returlwalue));
    }
    MaybeCollectGarbage(pContext);
    return OK;
}

// Execute a script method.
RC JavaScript::CallMethod
(
    JSObject      * pObject,
    string          Method,
    const JsArray & Arguments,   /* = JsArray() */
    jsval         * pReturlwalue /* = 0 */
)
{
    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    ClearCriticalEvent();

    JSFunction * pFunction = 0;
    if (OK != GetProperty(pObject, Method.c_str(), &pFunction))
    {
        Printf(Tee::PriDebug, "%s method is not defined.\n", Method.c_str());
        return RC::UNDEFINED_JS_METHOD;
    }

    jsval Returlwalue;

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (Arguments.size() == 0)
    {
        if (!JS_CallFunctionName(pContext, pObject, Method.c_str(), 0, 0,
                 &Returlwalue))
            return RC::SCRIPT_FAILED_TO_EXELWTE;
    }
    else
    {
        if (!JS_CallFunctionName(pContext, pObject, Method.c_str(),
                 (uintN) Arguments.size(), const_cast<jsval *>(&Arguments[0]),
                 &Returlwalue))
            return RC::SCRIPT_FAILED_TO_EXELWTE;
    }

    if (pReturlwalue)
        *pReturlwalue = Returlwalue;

    MaybeCollectGarbage(pContext);
    return OK;
}

RC JavaScript::CallMethod
(
    JSObject      * pObject,
    JSFunction *    Method,
    const JsArray & Arguments,    /* = JsArray() */
    jsval         * pReturlwalue  /* = 0 */
)
{
    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    ClearCriticalEvent();

    jsval Returlwalue;

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    if (Arguments.size() == 0)
    {
        if (!JS_CallFunction(pContext, pObject, Method, 0, 0, &Returlwalue))
            return RC::SCRIPT_FAILED_TO_EXELWTE;
    }
    else
    {
        if (!JS_CallFunction(pContext, pObject, Method, (uintN) Arguments.size(),
              const_cast<jsval*>(&Arguments[0]), &Returlwalue))
            return RC::SCRIPT_FAILED_TO_EXELWTE;
    }

    if (pReturlwalue)
        *pReturlwalue = Returlwalue;

    MaybeCollectGarbage(pContext);
    return OK;
}

RC JavaScript::CallMethod
(
    JSFunction    * Method,
    const JsArray & Arguments,    /* = JsArray() */
    jsval         * pReturlwalue /* = 0 */
)
{
    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    RC rc;
    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));
    return (CallMethod(pContext, Method, Arguments, pReturlwalue));
}

RC JavaScript::CallMethod
(
    JSContext     * pContext,
    JSFunction    * Method,
    const JsArray & Arguments,   /* = JsArray() */
    jsval         * pReturlwalue /* = 0 */
)
{
    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    ClearCriticalEvent();

    JSObject * pObject = JS_GetFunctionObject(Method);

    jsval Returlwalue;
    if (Arguments.size() == 0)
    {
        if (!JS_CallFunction(pContext, pObject, Method, 0, 0, &Returlwalue))
            return RC::SCRIPT_FAILED_TO_EXELWTE;
    }
    else
    {
        if (!JS_CallFunction(pContext, pObject, Method, (uintN) Arguments.size(),
              const_cast<jsval*>(&Arguments[0]), &Returlwalue))
            return RC::SCRIPT_FAILED_TO_EXELWTE;
    }

    if (pReturlwalue)
        *pReturlwalue = Returlwalue;

    MaybeCollectGarbage(pContext);
    return OK;
}

void JavaScript::MaybeCollectGarbage(JSContext * pContext)
{
    Tasker::AttachThread attach;

    Xp::OperatingSystem OS = Xp::GetOperatingSystem();
    if ((Xp::OS_WINDOWS != OS) && (Xp::OS_WINSIM != OS))
    {
        JS_MaybeGC(pContext);
    }
    else
    {
        JS_GC(pContext);
    }

    // Delete objects deferred from finalize functions
    ClearZombies();
}

void JavaScript::CollectGarbage(JSContext * pContext)
{
    Tasker::AttachThread attach;

    JS_GC(pContext);

    // Delete objects deferred from finalize functions
    ClearZombies();
}

// Run a compressed bound javascript file.
#if defined(BOUND_JS)

#include "boundjs.h"

//! \brief In a bound (restricted) exelwtable, run the bound javascript file.
//!
//! Decrypts, compresses and runs the bound-in javacsript file.  Decrypts
//! differently than non-bound JS files.
//! \sa JavaScript::DecryptScript
RC JavaScript::RunBoundScript()
{
    RC rc;

    vector<UINT08> decryptedBuffer;

    // Do not leave it decrypted in memory
    DEFER
    {
        if (!decryptedBuffer.empty())
        {
            memset(&decryptedBuffer[0], 0, decryptedBuffer.size());
        }
    };

    // Decrypt and decompress the bound script.
    rc = Utility::InterpretModsUtilErrorCode(
        Decryptor::DecryptDataArray(BoundScript, BoundScriptSize, &decryptedBuffer));
    if (rc != RC::OK)
        return RC::COULD_NOT_COMPILE_FILE;

    CHECK_RC(WriteDebugFile("bound.js", reinterpret_cast<vector<char>&>(decryptedBuffer)));

    JSContext *pContext;
    CHECK_RC(GetContext(&pContext));

    JSScript* pScript = nullptr;

#if defined(INCLUDE_BYTECODE)
    // Load the script from bytecode
    typedef BitsIO::InBitStream<vector<UINT08>::const_iterator> InStream;
    typedef JSScriptLoader<vector<UINT08>::const_iterator, JavaScript> Loader;
    InStream is(decryptedBuffer.begin(),
                decryptedBuffer.end());
    Loader ia(is, pContext, this);
    ia.Load(&pScript);
#else
    // Compile the script
    pScript = JS_CompileScript(pContext,
                               m_pGlobalObject,
                               reinterpret_cast<const char*>(&decryptedBuffer[0]),
                               decryptedBuffer.size(),
                               BoundScriptName,
                               1);
#endif

    if (0 == pScript)
        return RC::COULD_NOT_COMPILE_FILE;

    DEFER
    {
        JS_DestroyScript(pContext, pScript);
    };

    memset(&decryptedBuffer[0], 0, decryptedBuffer.size());
    {
        vector<UINT08> empty;
        decryptedBuffer.swap(empty);
    }

    // Execute the compiled script.
    jsval Returlwalue;
    if (!JS_ExelwteScript(pContext, m_pGlobalObject, pScript, &Returlwalue))
        return RC::SCRIPT_FAILED_TO_EXELWTE;

    // Collect garbage.
    JS_GC(pContext);

    // Delete objects deferred from finalize functions
    ClearZombies();

    return OK;
}

#else
RC JavaScript::RunBoundScript()
{
    return OK;
}
#endif   // defined(BOUND_JS)

// Get global object.
JSObject * JavaScript::GetGlobalObject() const
{
    return m_pGlobalObject;
}

// Get the context for the current thread, create a context if one
// does not yet exist.
RC JavaScript::GetContext(JSContext **ppContext)
{
    // The JS engine is not preemptive-threading safe.
    // JS should never be run from a detached thread.
    Tasker::AttachThread attach;

    RC rc;
    *ppContext = Tasker::GetLwrrentJsContext();
    if (*ppContext == NULL)
    {
        CHECK_RC(CreateContext(m_ContextStackSize, ppContext));
        Tasker::SetLwrrentJsContext(*ppContext);
    }

    if (JS_GetGlobalObject(*ppContext) == nullptr)
    {
        JS_SetGlobalObject(*ppContext, m_pGlobalObject);
    }

    return rc;
}

// Create a JS context
RC JavaScript::CreateContext
(
    const size_t StackSize,
    JSContext **ppNewContext
)
{
    // Create the JavaScript context.
    *ppNewContext = JS_NewContext(m_pRuntime, StackSize, nullptr, nullptr, nullptr, nullptr);
    if (0 == *ppNewContext)
    {
        return RC::COULD_NOT_CREATE_JAVASCRIPT_ENGINE;
    }

    // Set and validate the version on the new context
    JS_SetVersion(*ppNewContext, JSVERSION_1_7);
    const auto jsVer = JS_GetVersion(*ppNewContext);
    if (jsVer != JSVERSION_1_7)
    {
        JS_DestroyContext(*ppNewContext);
        *ppNewContext = NULL;
        return RC::COULD_NOT_CREATE_JAVASCRIPT_ENGINE;
    }

    // Initialize the options on the new context
    UINT32 jsOpts = JS_GetOptions(*ppNewContext);
    jsOpts |= JSOPTION_ATLINE;
    if (m_bStrict)
        jsOpts |= JSOPTION_STRICT;
    JS_SetOptions(*ppNewContext, jsOpts);

    // Set the error reporter.
    JS_SetErrorReporter(*ppNewContext, ErrorReporter);

    return OK;
}

// Gets the runtime.
JSRuntime * JavaScript::Runtime() const
{
    return m_pRuntime;
}

template<typename T>
RC JavaScript::FromJsArray(const JsArray &Array, vector<T> *pVector, Tee::Priority msgPri)
{
    T value;
    RC rc;

    MASSERT(pVector != 0);
    pVector->clear();
    pVector->reserve(Array.size());

    for (UINT32 ValueIdx = 0; ValueIdx < Array.size(); ValueIdx++)
    {
        rc = FromJsval(Array[ValueIdx], &value);
        if (rc != OK)
        {
            pVector->clear();
            Printf(msgPri, "Can't parse element %d of the JsArray.\n", ValueIdx);
            return rc;
        }
        pVector->push_back(value);
    }
    return OK;
}
// We are limiting the template instantiation to just the signatures that are lwrrently
// being used to keep code size down. If you need a different verion then add the prototype
// here.
template RC JavaScript::FromJsArray<UINT32>(const JsArray &Array, vector<UINT32> *pVector, Tee::Priority msgPri);
template RC JavaScript::FromJsArray<FLOAT32>(const JsArray &Array, vector<FLOAT32> *pVector, Tee::Priority msgPri);
template RC JavaScript::FromJsArray<string>(const JsArray &Array, vector<string> *pVector, Tee::Priority msgPri);


template<typename T>
RC JavaScript::ToJsArray(const vector<T> &Vector, JsArray *pArray, Tee::Priority msgPri)
{
    jsval value;
    RC rc;

    MASSERT(pArray != 0);
    pArray->clear();
    pArray->reserve(Vector.size());

    for (UINT32 ValueIdx = 0; ValueIdx < Vector.size(); ValueIdx++)
    {
        rc = ToJsval(Vector[ValueIdx], &value);
        if (rc != OK)
        {
            pArray->clear();
            Printf(msgPri, "Can't colwert element %d to JS object.\n", ValueIdx);
            return rc;
        }
        pArray->push_back(value);
    }
    return OK;
}

// We are limiting the template instantiation to just the signatures that are lwrrently
// being used to keep code size down. If you need a different verion then add the prototype
// here.
template RC JavaScript::ToJsArray<UINT32>(const vector<UINT32> &Vector, JsArray *pArray, Tee::Priority msgPri);
template RC JavaScript::ToJsArray<FLOAT32>(const vector<FLOAT32> &Vector, JsArray *pArray, Tee::Priority msgPri);
template RC JavaScript::ToJsArray<string>(const vector<string> &Vector, JsArray *pArray, Tee::Priority msgPri);

RC JavaScript::WriteDebugFile
(
    string              FileName,
    const vector<char> &FileBuffer
)
{
    RC rc;

#ifdef DEBUG
    if (CommandLine::DebugJavascript())
    {
        FileHolder fh;

        if (FileName.rfind(".js") == (FileName.size() - 3))
        {
            FileName += "d";
        }
        else
        {
            FileName += ".jsd";
        }

        rc = fh.Open(FileName, "w");
        if (OK != rc)
        {
            Printf(Tee::PriError, "Failed to open debug file %s\n", FileName.c_str());
            return rc;
        }
        if (0 == fwrite(&FileBuffer[0], FileBuffer.size(), 1, fh.GetFile()))
        {
            Printf(Tee::PriError, "Failed to write debug file %s\n", FileName.c_str());
            return RC::FILE_WRITE_ERROR;
        }
        Printf(Tee::PriNormal, "Wrote %s\n", FileName.c_str());
    }
#endif

    return rc;
}

RC JavaScript::WriteDebugFile
(
    string        FileName,
    const string &FileData
)
{
    RC rc;

#ifdef DEBUG
    if (CommandLine::DebugJavascript())
    {
        FileHolder fh;
        if (FileName.rfind(".js") == (FileName.size() - 3))
        {
            FileName += "d";
        }
        else
        {
            FileName += ".jsd";
        }

        rc = fh.Open(FileName, "w");
        if (OK != rc)
        {
            Printf(Tee::PriError, "Failed to open debug file %s\n", FileName.c_str());
            return rc;
        }
        if (0 == fwrite(FileData.c_str(), FileData.size(), 1, fh.GetFile()))
        {
            Printf(Tee::PriError, "Failed to write debug file %s\n", FileName.c_str());
            return RC::FILE_WRITE_ERROR;
        }
        Printf(Tee::PriNormal, "Wrote %s\n", FileName.c_str());
    }
#endif

    return rc;
}

// Throw a JsException object that contains a rc and custom message to print
void JavaScript::Throw(JSContext *cx, RC rc, string msg)
{
    JavaScriptPtr pJs;
    array<jsval, 2> args;

    pJs->ToJsval(static_cast<INT32>(rc), &args[0]);
    pJs->ToJsval(msg, &args[1]);

    JSObject *pJsException = JS_ConstructObjectWithArguments
    (
        cx, &JsExceptionClass, nullptr, nullptr, static_cast<uintN>(args.size()), &args[0]
    );

    if (nullptr != pJsException)
    {
        JS_SetPendingException(cx, OBJECT_TO_JSVAL(pJsException));
    }
    else
    {
        JS_SetPendingException(cx, INT_TO_JSVAL(static_cast<INT32>(rc)));
    }
}

string JavaScript::GetStackTrace()
{
    string stackTrace;
    Tasker::ForEachAttachedThread([&stackTrace](Tasker::ThreadID id)
    {
        auto *cx = Tasker::GetJsContext(id);
        if (nullptr != cx)
        {
            using Utility::StrPrintf;
            stackTrace += StrPrintf(
                "JavaScript stack trace for thread \"%s\"%s:\n",
                Tasker::ThreadName(id),
                Tasker::LwrrentThread() == id ? " (current)" : ""
            );
            JSStackFrame *stackWalk;
            for (stackWalk = cx->fp; stackWalk; stackWalk = stackWalk->down)
            {
                if (stackWalk->fun && stackWalk->fun->atom)
                {
                    JSString *jsFunName = ATOM_TO_STRING(stackWalk->fun->atom);
                    stackTrace += StrPrintf("  %s\n", DeflateJSString(cx, jsFunName).c_str());
                }
            }
        }
    });
    return stackTrace;
}

RC JavaScript::GetRcException(JSContext *pContext)
{
    jsval ex;
    int32 Code;

    if (!JS_IsExceptionPending(pContext))
        return OK;

    RC rc;
    if (JS_GetPendingException(pContext, &ex))
    {
        if (JSVAL_IS_NUMBER(ex))
        {
            if (JS_ValueToECMAInt32(pContext, ex, &Code) &&
                (Code > 0) && (Code < RC::TEST_BASE))
            {
                // The exception data is plausible as an RC.
                rc = Code;
            }
        }
        if (JSVAL_IS_OBJECT(ex))
        {
            JSObject *pObj = JSVAL_TO_OBJECT(ex);
            if (pObj && JS_InstanceOf(pContext, pObj, &JsExceptionClass, nullptr))
            {
                INT32 rcInt;
                if (GetProperty(pObj, "rc", &rcInt) == OK)
                {
                    rc = static_cast<RC>(rcInt);
                }
            }
        }
    }

    return rc;
}

void JavaScript::ReportAndClearException(JSContext *pContext)
{
    if (!JS_IsExceptionPending(pContext))
        return;
    JS_ReportPendingException(pContext);
    JS_ClearPendingException(pContext);
}

#if defined(INCLUDE_BYTECODE)
JSObject*& JavaScript::operator[](JSObjectId id)
{
    return (*m_objectIdMapImpl)[id];
}

JSObject* JavaScript::GetObjectById(JSObjectId id)
{
    return m_objectIdMapImpl->GetObjectById(id);
}
#endif

//------------------------------------------------------------------------------
// JavaScript functions.
//
C_(Global_Import);
static STest Global_Import
(
    "Import",
    C_Global_Import,
    1,
    "Preprocess, compile, and execute JavaScript code from a file."
);

C_(Global_Import)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string     FileName;
    JSObject * pReturlwals = nullptr;
    bool       mustExist   = true;

    if ((NumArguments < 1) ||
        (NumArguments > 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        ((NumArguments > 1) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))) ||
        ((NumArguments > 2) &&
         (OK != pJavaScript->FromJsval(pArguments[2], &mustExist))))
    {
        JS_ReportError(pContext,
                       "Usage: Import(\"file\" [, opt_eval_result [, opt_bool_must_exist]])");
        return JS_FALSE;
    }

    RC rc = pJavaScript->Import(FileName.c_str(), pReturlwals, mustExist);
    RETURN_RC(rc);
}

C_(Global_Eval);
static STest Global_Eval
(
    "Eval",
    C_Global_Eval,
    1,
    "Preprocess, compile, and execute JavaScript code from a string."
);

C_(Global_Eval)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Str;
    JSObject * pReturlwals = nullptr;
    if ((NumArguments < 1) ||
        (NumArguments > 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Str)) ||
        ((NumArguments == 2) &&
         (OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))))
    {
        JS_ReportError(pContext, "Usage: Eval(\"str\", [opt_eval_result])");
        return JS_FALSE;
    }

    RC rc = pJavaScript->Eval(Str.c_str(), pReturlwals);
    RETURN_RC(rc);
}

C_(Global_MakeBoundScript);
static STest Global_MakeBoundScript
(
    "MakeBoundScript",
    C_Global_MakeBoundScript,
    1,
    "Compress a .js file (and all its #includes) to the boundjs.h file."
);

C_(Global_MakeBoundScript)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    Printf(Tee::PriError,
           "MakeBoundScript is deprecated.  Script binding is now automated by "
           "setting environment variable BOUND_JS to the file you wish to bind.");

    RETURN_RC(RC::SCRIPT_FAILED_TO_EXELWTE);
}

C_(Global_CollectGarbage);
static STest Global_CollectGarbage
(
    "CollectGarbage",
    C_Global_CollectGarbage,
    0,
    "Run the JS engine's garbage collector."
);

C_(Global_CollectGarbage)
{
    JavaScriptPtr pJavaScript;
    pJavaScript->CollectGarbage(pContext);

    RETURN_RC(OK);
}

P_(Global_Get_JavaScriptVersion);
static SProperty Global_JavaScriptVersion
(
    "JavaScriptVersion",
    0,
    0,
    Global_Get_JavaScriptVersion,
    0,
    JSPROP_READONLY,
    "Returns JS version as string (i.e.\"1.7\")"
);
P_(Global_Get_JavaScriptVersion)
{
    JSVersion ver = JS_GetVersion(pContext);
    string sVer   = JS_VersionToString(ver);

    if (OK != JavaScriptPtr()->ToJsval(sVer, pValue))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

namespace
{ // anonymous namespace
C_( Global_FileToString );
SMethod Global_FileToString
(
    "FileToString",
    C_Global_FileToString,
    1,
    "Return a JS string with contents of a text file."
);

RC FileToString(string fileName, string* pBuf)
{
    RC rc;
    FileHolder fh;
    CHECK_RC(fh.Open(fileName, "rb"));

    long fileSize;
    CHECK_RC(Utility::FileSize(fh.GetFile(), &fileSize));
    pBuf->resize(fileSize);

    long readBytes = static_cast<long>(
        fread(&((*pBuf)[0]), 1, fileSize, fh.GetFile()));
    if (fileSize != readBytes)
        return RC::FILE_READ_ERROR;

    return OK;
}

C_( Global_FileToString )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    string fileName;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &fileName)))
    {
        JS_ReportError(pContext, "Usage: FileToString(\"fileName\")");
        return JS_FALSE;
    }

    string buf;
    RC rc;
    C_CHECK_RC_THROW(FileToString(fileName, &buf), "FileToString failed");
    if (pReturlwalue)
        C_CHECK_RC_THROW(pJs->ToJsval(buf, pReturlwalue), "FailToString failed");

    return JS_TRUE;
}
} // anonymous namespace

void JavaScript::PrintStackTrace(INT32 pri)
{
    Printf(pri, "JavaScript stack trace:\n");

    JSContext* pJSContext;
    GetContext(&pJSContext);

    int depth = 0;
    JSStackFrame* fp = nullptr;

    while (JS_FrameIterator(pJSContext, &fp))
    {
        const auto fun = JS_GetFrameFunction(pJSContext, fp);

        // Skip inner scopes in functions
        if (fun)
        {
            const auto script = JS_GetFrameScript(pJSContext, fp);
            const auto pc     = JS_GetFramePC(pJSContext, fp);

            const auto     funName  = fun->atom ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
                                      : "?";
            const unsigned lineNo   = JS_PCToLineNumber(pJSContext, script, pc);
            const auto     filename = JS_IsNativeFrame(pJSContext, fp) ? "<native>"
                                      : JS_GetScriptFilename(pJSContext, script);

            Printf(pri, " #%d: %s() in %s:%u\n", depth++, funName, filename, lineNo);
        }
    }
}

namespace
{
    C_(Global_PrintStackTrace);
    SMethod Global_PrintStackTrace
    (
        "PrintStackTrace",
        C_Global_PrintStackTrace,
        0,
        "Prints a JavaScript stack trace."
    );

    C_(Global_PrintStackTrace)
    {
        STEST_HEADER(0, 1, "Usage: PrintStackTrace([priority])");
        STEST_OPTIONAL_ARG(0, INT32, pri, Tee::PriNormal);

        JavaScriptPtr pJs;
        JavaScriptPtr()->PrintStackTrace(pri);

        RETURN_RC(OK);
    }
}
