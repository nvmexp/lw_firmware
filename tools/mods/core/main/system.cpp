/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

#include "core/include/xp.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include <cstdlib>
#include <array>

C_( Global_System );
static SMethod Global_System
(
   "System",
   C_Global_System,
   1,
   "Run a system command, or pass \"\" to shell out."
);

C_( Global_System )
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   string Command;
   if
   (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Command))
   )
   {
      JS_ReportError(pContext, "Usage: System( \"command\" )");
      return JS_FALSE;
   }

    INT32 Result = system(Command.c_str());
    if (-1 == Result)
    {
        JS_ReportError( pContext, "Could not run System(\"%s\").",
            Command.c_str());
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

   if (OK != pJavaScript->ToJsval(Result, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in System()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

C_(Global_Popen);
static SMethod Global_Popen
(
    "Popen",
    C_Global_Popen,
    2,
    "Run a popen command and capture the output."
);

C_(Global_Popen)
{
    STEST_HEADER(2, 2, "Usage: Popen(command, [outputString])");
    STEST_ARG(0, string, command);
    STEST_ARG(1, JSObject*, pOutput);

    string output;
    FILE* pf = Xp::Popen(command.c_str(), "r");
    if (!pf)
    {
        JS_ReportError(pContext, "Could not start Popen(\"%s\", \"r\")", command.c_str());
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    array<char, 128> buffer;
    while (fgets(buffer.data(), 128, pf) != 0)
    {
        output += buffer.data();
    }

    INT32 returnCode = Xp::Pclose(pf);

    RC rc;
    C_CHECK_RC(pJavaScript->SetElement(pOutput, 0, output));
    C_CHECK_RC(pJavaScript->ToJsval(returnCode, pReturlwalue));

    return JS_TRUE;
}
