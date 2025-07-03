 /*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/log.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/errormap.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include <atomic>
#include <set>
#include <vector>
#include <stdlib.h>

static set<string> s_MustBeNeverMethods;
static set<string> s_AlwaysMethods;
static set<string> s_NeverMethods;
static ErrorMap    s_FirstErrorMap;
static INT32       s_FirstErrorCode;
static bool        s_Next            = true;
static bool        s_PrintEnterCode  = false;
static bool        s_RecordeTime     = false;
static UINT64      s_LastAssertFailure = 0;
static INT32       s_LastAssertCode = OK;
static void*       s_LogMutex = NULL;

// By default exit after 5 breakpoints
static UINT32      s_ExitOnBreakpointCount = 5;

// for cirlwlar buffer of error history:
static CirBuffer<Log::ErrorEntry> s_ErrorHistory(16);

RC Log::Initialize
(
   const vector<string> & JsMethods
)
{
   s_LogMutex = Tasker::AllocMutex("Log::s_LogMutex", Tasker::mtxUnchecked);
   MASSERT(s_LogMutex);
   Tasker::MutexHolder mh(s_LogMutex);

   s_MustBeNeverMethods.clear();
   s_AlwaysMethods.clear();
   s_NeverMethods.clear();

   // Never log the JS methods.
   for
   (
      vector<string>::const_iterator iJsMethod = JsMethods.begin();
      iJsMethod != JsMethods.end();
      ++iJsMethod
   )
   {
      s_MustBeNeverMethods.insert(*iJsMethod);
      s_NeverMethods.insert(*iJsMethod);
   }

   // Never log SMethod::Function's.
   const SMethodVec & GlobalSMethods = GetGlobalSMethods();
   for
   (
      SMethodVec::const_iterator ipGlobalSMethod = GlobalSMethods.begin();
      ipGlobalSMethod != GlobalSMethods.end();
      ++ipGlobalSMethod
   )
   {
      if ((*ipGlobalSMethod)->GetMethodType() == SMethod::Function)
      {
         string Method = string((*ipGlobalSMethod)->GetObjectName()) + '.' + string((*ipGlobalSMethod)->GetName());
         s_MustBeNeverMethods.insert(Method);
         s_NeverMethods.insert(Method);
      }
   }

   const SObjectVec & SObjects = GetSObjects();
   for
   (
      SObjectVec::const_iterator ipSObject = SObjects.begin();
      ipSObject != SObjects.end();
      ++ipSObject
   )
   {
      const SMethodVec & ObjectSMethods = (*ipSObject)->GetInheritedMethods();
      for
      (
         SMethodVec::const_iterator ipObjectSMethod = ObjectSMethods.begin();
         ipObjectSMethod != ObjectSMethods.end();
         ++ipObjectSMethod
      )
      if ((*ipObjectSMethod)->GetMethodType() == SMethod::Function)
      {
         string Method = string((*ipSObject)->GetName()) + '.'
                        + string((*ipObjectSMethod)->GetName());
         s_MustBeNeverMethods.insert(Method);
         s_NeverMethods.insert(Method);
      }
   }

   return OK;
}

RC Log::Teardown()
{
    Tasker::FreeMutex(s_LogMutex);
    return OK;
}

RC Log::Always
(
   string Method
)
{
   Tasker::MutexHolder mh(s_LogMutex);
   MASSERT(Method != "");

   // If the method does not contain a '.' then it must be a Global method.
   if (Method.find('.') == string::npos)
   {
      Method.insert(0, ENCJSENT("Global") ".");
   }

   // Check if you are allowed to log this method.
   if (s_MustBeNeverMethods.find(Method) != s_MustBeNeverMethods.end())
   {
      Printf(Tee::PriHigh, "%s cannot be logged.\n", Method.c_str());
      return RC::CANNOT_LOG_METHOD;
   }

   s_AlwaysMethods.insert(Method);
   s_NeverMethods.erase(Method);

   return OK;
}

RC Log::Never
(
   string Method
)
{
   Tasker::MutexHolder mh(s_LogMutex);
   MASSERT(Method != "");

   // If the method does not contain a '.' then it must be a Global method.
   if (Method.find('.') == string::npos)
   {
      Method.insert(0, ENCJSENT("Global") ".");
   }

   // Print a warning if this is an internal JS method.
   if (s_MustBeNeverMethods.find(Method) != s_MustBeNeverMethods.end())
   {
      Printf(Tee::PriHigh, "%s is an internal JavaScript method.\n", Method.c_str());
      return OK;
   }

   s_AlwaysMethods.erase(Method);
   s_NeverMethods.insert(Method);

   return OK;
}

RC Log::OnError
(
   string Method
)
{
   Tasker::MutexHolder mh(s_LogMutex);
   MASSERT(Method != "");

   // If the method does not contain a '.' then it must be a Global method.
   if (Method.find('.') == string::npos)
   {
      Method.insert(0, ENCJSENT("Global") ".");
   }

   // Check if you are allowed to log this method.
   if (s_MustBeNeverMethods.find(Method) != s_MustBeNeverMethods.end())
   {
      Printf(Tee::PriHigh, "%s cannot be logged.\n", Method.c_str());
      return RC::CANNOT_LOG_METHOD;
   }

   s_AlwaysMethods.erase(Method);
   s_NeverMethods.erase(Method);

   return OK;
}

bool Log::IsAlways
(
   string Method
)
{
   Tasker::MutexHolder mh(s_LogMutex);
   MASSERT(Method != "");

   // If the method does not contain a '.' then it must be a Global method.
   if (Method.find('.') == string::npos)
   {
      Method.insert(0, ENCJSENT("Global") ".");
   }

   return (s_AlwaysMethods.find(Method) != s_AlwaysMethods.end());
}

bool Log::IsNever
(
   string Method
)
{
   Tasker::MutexHolder mh(s_LogMutex);
   MASSERT(Method != "");

   // If the method does not contain a '.' then it must be a Global method.
   if (Method.find('.') == string::npos)
   {
      Method.insert(0, ENCJSENT("Global") ".");
   }

   return (s_NeverMethods.find(Method) != s_NeverMethods.end());
}

namespace Platform
{
    extern atomic<bool> g_ModsExitOnBreakPoint;
}

void Log::AddAssertFailure(RC reasonRc)
{
    bool exitImmediate = false;
    Tasker::MutexHolder mh(s_LogMutex);
    ++s_LastAssertFailure;
    ErrorMap Error(reasonRc);
    SetFirstError(Error);
    s_LastAssertCode = Error.Code();

    // For fatal errors, exit immediately to avoid infinite loops/timeouts
    if (reasonRc == RC::LWRM_FATAL_ERROR)
    {
        Printf(Tee::PriAlways,
              "LWRM has encountered an unrecoverable error. Exiting MODS.\n");
        exitImmediate = true;
    }
    else if ((s_ExitOnBreakpointCount != 0) &&
        (s_ExitOnBreakpointCount == s_LastAssertFailure))
    {
        Printf(Tee::PriAlways,
               "MODS is exiting with failure after assert #%lld. Breakpoint limit exceeded.\n",
               s_LastAssertFailure);
        exitImmediate = true;
    }

    if (exitImmediate)
    {
        Log::SetNext(false);
        JavaScriptPtr()->CallMethod(JavaScriptPtr()->GetGlobalObject(), 
                                    "PrintErrorWrapperPostTeardown");
        Tee::FlushToDisk();
        Platform::g_ModsExitOnBreakPoint = true; // Prevent further, potentially infinite breakpoints
        Utility::ExitMods(reasonRc, Utility::ExitQuickAndDirty);
    }
}

INT32 Log::GetLastAssertCode()
{
    return s_LastAssertCode;
}

UINT64 Log::GetLastAssertCount()
{
    return s_LastAssertFailure;
}

UINT32 Log::ExitOnBreakpointCount()
{
   return s_ExitOnBreakpointCount;
}

void Log::SetExitOnBreakpointCount(UINT32 BreakpointCount)
{
    Tasker::MutexHolder mh(s_LogMutex);
    s_ExitOnBreakpointCount = BreakpointCount;
}

INT32 Log::FirstError()
{
    return s_FirstErrorCode;
}

string Log::FirstErrorStr()
{
    const bool haveMutex = Tasker::IsInitialized();
    if (haveMutex)
    {
        Tasker::AcquireMutex(s_LogMutex);
    }
    const string error = s_FirstErrorMap.ToDecimalStr();
    if (haveMutex)
    {
        Tasker::ReleaseMutex(s_LogMutex);
    }
    return error;
}

UINT32 Log::FirstPhase()
{
   Tasker::MutexHolder mh(s_LogMutex);
   return s_FirstErrorMap.FailedPhase();
}

void Log::SetFirstError(const ErrorMap& error)
{
    Tasker::MutexHolder mh(s_LogMutex);

    const INT32 code = error.Code();

    // why call this outside if (OK==s_FirstError): because we want to log all
    // errors more than just on FirstError.
    InsertErrorToHistory(code);

    if (s_FirstErrorMap.Code() == RC::OK)
    {
        s_FirstErrorMap  = error;
        s_FirstErrorCode = code;

        // Print JS stack trace for the first error recorded
        if (code != RC::OK)
        {
            const bool onLWNet = (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK);
            const INT32 pri = onLWNet ? Tee::PriNormal : Tee::PriLow;
            Printf(pri, "First error recorded: %d %s\n",
                   code, s_FirstErrorMap.Message());
            JavaScriptPtr()->PrintStackTrace(pri);
        }
    }
}

void Log::ResetFirstError()
{
   Tasker::MutexHolder mh(s_LogMutex);
   s_FirstErrorMap.Clear();
   s_FirstErrorCode = RC::OK;
}

INT32 Log::FirstFailedTest()
{
   return s_FirstErrorMap.FailedTest();
}

bool Log::Next()
{
   return s_Next;
}

void Log::SetNext
(
   bool State
)
{
   s_Next = State;
}

bool Log::RecordTime()
{
   return s_RecordeTime;
}

void Log::SetRecordTime
(
   bool State
)
{
   s_RecordeTime = State;
}

bool Log::PrintEnterCode()
{
   return s_PrintEnterCode;
}

void Log::SetPrintEnterCode
(
   bool State
)
{
   s_PrintEnterCode = State;
}
//------------------------------------------------------------------------------
// Todo: setup a policy of where we should monitor for errors:
// Places that come to mind:
// 1) In JS functions - handled if we call this in SetFirstError
// 2) ModsTest's Setup/Run/Cleanup
// 3) ErrorLogger/Unexpected interrupts: handled in SetFirstError
//
// What happens if there are duplicates - ie. unexpected interrupts in a JS MODS
// test, so we get one error for interrupt and another one for failed MODS test:
// This is okay because it's just extra information.
void Log::InsertErrorToHistory(INT32 ErrorCode)
{
    // no point adding error if the result is OK
    if (OK == ErrorCode)
    {
        return;
    }

    Tasker::MutexHolder mh(s_LogMutex);
    ErrorEntry NewEntry;
    NewEntry.RcVal       = ErrorCode;
    NewEntry.TimeOfError = Xp::QueryPerformanceCounter();

    s_ErrorHistory.push_back(NewEntry);
    Printf(Tee::PriDebug, "InsertErrorToHistory: pushback %d @%lld\n",
           ErrorCode, NewEntry.TimeOfError);
}

// return a copy of the history (copy is probably safer considering multi-thread
CirBuffer<Log::ErrorEntry> Log::GetErrorHistory()
{
    Tasker::MutexHolder mh(s_LogMutex);
    return s_ErrorHistory;
}

void Log::PurgeErrorHistory()
{
    Tasker::MutexHolder mh(s_LogMutex);
    s_ErrorHistory.clear();
}
//------------------------------------------------------------------------------
// JavaScript Log class and methods.
//
JS_CLASS(Log);
static SObject Log_Object
(
   "Log",
   LogClass,
   0,
   0,
   "Always log methods or only on error."
);

P_(Log_Get_FirstError);
static SProperty Log_FirstError
(
   Log_Object,
   "FirstError",
   0,
   0,
   Log_Get_FirstError,
   0,
   0,
   "First error code."
);

P_(Log_Get_FirstErrorStr);
static SProperty Log_FirstErrorStr
(
   Log_Object,
   "FirstErrorStr",
   0,
   0,
   Log_Get_FirstErrorStr,
   0,
   0,
   "First error in string."
);

P_(Log_Get_FirstPhase);
static SProperty Log_FirstPhase
(
   Log_Object,
   "FirstPhase",
   0,
   0,
   Log_Get_FirstPhase,
   0,
   0,
   "First error phase."
);

P_(Log_Get_FirstFailedTest);
static SProperty Log_FirstFailedTest
(
   Log_Object,
   "FirstFailedTest",
   0,
   0,
   Log_Get_FirstFailedTest,
   0,
   0,
   "First test to fail."
);

P_(Log_Get_Next);
P_(Log_Set_Next);
static SProperty Log_Next
(
   Log_Object,
   "Next",
   0,
   "",
   Log_Get_Next,
   Log_Set_Next,
   0,
   "Log next method."
);

P_(Log_Get_PrintEnterCode);
P_(Log_Set_PrintEnterCode);
static SProperty Log_PrintEnterCode
(
   Log_Object,
   "PrintEnterCode",
   0,
   0,
   Log_Get_PrintEnterCode,
   Log_Set_PrintEnterCode,
   0,
   "Display the error code on test entry."
);

P_(Log_Get_RecordTime);
P_(Log_Set_RecordTime);
static SProperty Log_RecordTime
(
   Log_Object,
   "RecordTime",
   0,
   0,
   Log_Get_RecordTime,
   Log_Set_RecordTime,
   0,
   "Record the duration of the method."
);

P_(Log_Get_ExitOnBreakpointCount);
P_(Log_Set_ExitOnBreakpointCount);
static SProperty Log_ExitOnBreakpointCount
(
   Log_Object,
   "ExitOnBreakpointCount",
   0,
   0,
   Log_Get_ExitOnBreakpointCount,
   Log_Set_ExitOnBreakpointCount,
   0,
   "Force MODS to exit when the breakpoint count reaches the threshold."
);

C_(Log_Always);
static STest Log_Always
(
   Log_Object,
   "Always",
   C_Log_Always,
   1,
   "Alway log specified method."
);

C_(Log_Never);
static STest Log_Never
(
   Log_Object,
   "Never",
   C_Log_Never,
   1,
   "Never log specified method."
);

C_(Log_OnError);
static STest Log_OnError
(
   Log_Object,
   "OnError",
   C_Log_OnError,
   1,
   "Log specified method only when an error oclwrs."
);

C_(Log_ResetFirstError);
static STest Log_ResetFirstError
(
   Log_Object,
   "ResetFirstError",
   C_Log_ResetFirstError,
   0,
   "Reset the first error."
);

P_(Log_Get_FirstError)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::FirstError(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.FirstError.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Log_Get_FirstErrorStr)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::FirstErrorStr(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.FirstErrorStr.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

P_(Log_Get_FirstPhase)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::FirstPhase(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.FirstPhase.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

P_(Log_Get_FirstFailedTest)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::FirstFailedTest(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.FirstFailedTest.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Log_Get_Next)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::Next(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.Next.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Log_Set_Next)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 State = true;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
   {
      JS_ReportError(pContext, "Failed to set Log.Last.");
      return JS_FALSE;
   }

   Log::SetNext(State != JS_FALSE);

   return JS_TRUE;
}

P_(Log_Get_PrintEnterCode)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::PrintEnterCode(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.PrintEnterCode.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Log_Set_PrintEnterCode)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 State;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
   {
      JS_ReportError(pContext, "Failed to set Log.PrintEnterCode.");
      return JS_FALSE;
   }

   Log::SetPrintEnterCode(State != JS_FALSE);

   return JS_TRUE;
}

P_(Log_Get_RecordTime)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::RecordTime(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.RecordTime.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Log_Set_RecordTime)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 State;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
   {
      JS_ReportError(pContext, "Failed to set Log.RecordTime.");
      return JS_FALSE;
   }

   Log::SetRecordTime(State != JS_FALSE);

   return JS_TRUE;
}

P_(Log_Get_ExitOnBreakpointCount)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Log::ExitOnBreakpointCount(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Log.ExitOnBreakpointCount.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Log_Set_ExitOnBreakpointCount)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 BreakpointCount;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &BreakpointCount))
   {
      JS_ReportError(pContext, "Failed to set Log.ExitOnBreakpointCount.");
      return JS_FALSE;
   }

   Log::SetExitOnBreakpointCount(BreakpointCount);

   return JS_TRUE;
}

C_(Log_Always)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the argument.
   string Method;
   if
   (
         (NumArguments != 1)
      || (OK != JavaScriptPtr()->FromJsval(pArguments[0], &Method))
   )
   {
      JS_ReportError(pContext, "Usage: Log.Always(\"class.method\")");
      return JS_FALSE;
   }

   RETURN_RC(Log::Always(Method));
}

C_(Log_Never)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the argument.
   string method;
   if
   (
         (NumArguments != 1)
      || (OK != JavaScriptPtr()->FromJsval(pArguments[0], &method))
   )
   {
      JS_ReportError(pContext, "Usage: Log.Never(\"class.method\")");
      return JS_FALSE;
   }

   RETURN_RC(Log::Never(method));
}

// STest
C_(Log_OnError)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the argument.
   string Method;
   if
   (
         (NumArguments != 1)
      || (OK != JavaScriptPtr()->FromJsval(pArguments[0], &Method))
   )
   {
      JS_ReportError(pContext, "Usage: Log.OnError(\"class.method\")");
      return JS_FALSE;
   }

   RETURN_RC(Log::OnError(Method));
}

// STest
C_(Log_ResetFirstError)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // This is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Log.ResetFirstError()");
      return JS_FALSE;
   }

   Log::ResetFirstError();

   RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(Log, PurgeErrorHistory, 0, "purge the error history buffer")
{
    Log::PurgeErrorHistory();
    return JS_TRUE;
}
