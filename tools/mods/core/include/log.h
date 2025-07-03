/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Log method interface.

#ifndef INCLUDED_LOG_H
#define INCLUDED_LOG_H

#ifndef INCLUDED_MODS_H
#include "types.h"
#endif
#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_CIRLWLARBUFFER_H
#include "cirlwlarbuffer.h"
#endif
#ifndef INCLUDED_ERRORMAP_H
#include "errormap.h"
#endif

namespace Log
{
   // Initialize the logger and setup the methods that should be logged.
   // Must be called after SMethods are added to SObjects.
   RC Initialize(const vector<string> & JsMethods);
   RC Teardown();

   // Log the method either always, never, or on error (default).
   RC Always(string Method);
   RC Never(string Method);
   RC OnError(string Method);

   // Is the method always or never logged?
   bool IsAlways(string Method);
   bool IsNever(string Method);

   // Get, set, and reset the first error code and phase
   // SetFirst() will only set the first error code if its lwrrently OK.
   // To reset the first error to OK, use ResetFirst().
   INT32 FirstError();
   void SetFirstError(const ErrorMap &Error);
   void ResetFirstError();

   UINT32 FirstPhase();

   // Get the first failing test number.
   INT32 FirstFailedTest();

   // Get the first error string (contains Phase, test number, error number)
   string FirstErrorStr();

   // Get and set if next method should be logged.
   bool Next();
   void SetNext(bool State);

   // Get and set if we need to record how long the test took to execute.
   bool RecordTime();
   void SetRecordTime(bool State);

   // Get and set if the 12-digit error code should be displayed on test entry.
   bool PrintEnterCode();
   void SetPrintEnterCode(bool State);

   INT32  GetLastAssertCode();
   UINT64 GetLastAssertCount();
   void   AddAssertFailure(RC reasonRc);

   // Get and set the breakpoint count to exit MODS on.
   UINT32 ExitOnBreakpointCount();
   void SetExitOnBreakpointCount(UINT32 BreakpointCount);

   struct ErrorEntry
   {
       INT32  RcVal;
       UINT64 TimeOfError;
   };

   void InsertErrorToHistory(INT32 ErrorCode);
   void PurgeErrorHistory();
   CirBuffer<Log::ErrorEntry> GetErrorHistory();
}

#endif // !INCLUDED_LOG_H
