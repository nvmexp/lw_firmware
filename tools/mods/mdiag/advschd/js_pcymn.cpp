/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file
//! \brief Link PolicyManager to the JavaScript engine
//!
//! Group JS functionality in a separate file to keep policymn.cpp cleaner

#include "core/include/jscript.h"
#include "pmgilder.h"
#include "pmparser.h"
#include "core/include/script.h"

JS_CLASS(PolicyManager);

static SObject PolicyManager_Object
(
    "PolicyManager",
    PolicyManagerClass,
    0,
    0,
    "PolicyManager (tests advanced scheduling)"
);

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    Parse,
    1,
    "Parse the specified policy file to set advanced scheduling policy"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: PolicyManager.Parse(fileName)");
        return JS_FALSE;
    }
    string arg;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &arg))
    {
        JS_ReportError(pContext, "Cannot colwert argument FromJsval");
        return JS_FALSE;
    }
    RETURN_RC(PolicyManager::Instance()->GetParser()->Parse(arg));
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    SetSelfTestMode,
    0,
    "Triggers a self test of the PolicyManager infrastructure instead "
    "of running the tests"
)
{
    RETURN_RC(OK); // TODO
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    SetGilding,
    1,
    "Tell the PolicyManager what type of gilding to do:\n"
    "    INCLUDE (default), EQUAL, or NONE"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);
    RC rc;

    JavaScriptPtr pJavaScript;
    string arg;
    const char *usage
        = "Usage: PolicyManager.SetGilding(\"REQUIRED\"|\"INLWLDE\"|\"EQUAL\"|\"NONE\")";
    PmGilder *pGilder = PolicyManager::Instance()->GetGilder();

    if (NumArguments != 1)
    {
        rc = RC::BAD_PARAMETER;
    }
    else if (pJavaScript->FromJsval(pArguments[0], &arg) != OK)
    {
        JS_ReportError(pContext, "Cannot colwert argument FromJsval");
        return JS_FALSE;
    }
    else if (arg == "REQUIRED")
    {
        pGilder->SetMode(PmGilder::REQUIRED_MODE);
    }
    else if (arg == "INCLUDE")
    {
        pGilder->SetMode(PmGilder::INCLUDE_MODE);
    }
    else if (arg == "EQUAL")
    {
        pGilder->SetMode(PmGilder::EQUAL_MODE);
    }
    else if (arg == "NONE")
    {
        pGilder->SetMode(PmGilder::NONE_MODE);
    }
    else
    {
        rc = RC::BAD_PARAMETER;
    }

    if (rc != OK)
        JS_ReportWarning(pContext, usage);
    RETURN_RC(rc);
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    SetGildFile,
    1,
    "Give PolicyManager the results of a previous run to gild against"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    string filename;

    if (NumArguments != 1)
    {
        JS_ReportWarning(pContext, "Usage: PolicyManager.SetGildFile(filename)");
        RETURN_RC(RC::BAD_PARAMETER);
    }
    else if (pJavaScript->FromJsval(pArguments[0], &filename) != OK)
    {
        JS_ReportError(pContext, "Cannot colwert argument FromJsval");
        return JS_FALSE;
    }

    PolicyManager::Instance()->GetGilder()->SetInputFile(filename);
    RETURN_RC(OK);
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    EnableGildDump,
    1,
    "Tell PolicyManager to dump a gildfile after testing"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments > 0)
    {
        JS_ReportWarning(pContext, "Usage: PolicyManager.EnableGildDump()");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    PolicyManager::Instance()->GetGilder()->SetOutputFile("gild.log");
    RETURN_RC(OK);
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    SetGildEventsMin,
    1,
    "Tell PolicyManager to FAIL if a minumum number of events do not occur"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 eventsMin;

    if (NumArguments != 1)
    {
        JS_ReportWarning(pContext, "Usage: PolicyManager.SetGildEventsMin(eventsMin)");
        RETURN_RC(RC::BAD_PARAMETER);
    }
    else if (pJavaScript->FromJsval(pArguments[0], &eventsMin) != OK)
    {
        JS_ReportError(pContext, "Cannot colwert argument FromJsval");
        return JS_FALSE;
    }

    PolicyManager::Instance()->GetGilder()->SetEventsMin(eventsMin);
    RETURN_RC(OK);
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    PolicyManager,
    SetRandomSeed,
    1,
    "Set the random seed that PolicyManager will use (overrides pcy file options)"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 seed;

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: PolicyManager.SetSeed(seed)");
        RETURN_RC(RC::BAD_PARAMETER);
    }
    else if (pJavaScript->FromJsval(pArguments[0], &seed) != OK)
    {
        JS_ReportError(pContext, "Cannot colwert argument FromJsval");
        return JS_FALSE;
    }

    PolicyManager::Instance()->SetRandomSeed(seed);
    RETURN_RC(OK);
}

CLASS_PROP_READWRITE_LWSTOM(PolicyManager, StrictMode,
                        "PolicyManager strict mode (boolean, default = false)");
P_(PolicyManager_Get_StrictMode)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool bStrictMode = PolicyManager::Instance()->GetStrictMode();
    if (OK != JavaScriptPtr()->ToJsval(bStrictMode, pValue))
    {
       JS_ReportError(pContext, "Failed to get PolicyManager.StrictMode");
       *pValue = JSVAL_NULL;
       return JS_FALSE;
    }

    return JS_TRUE;
}
P_(PolicyManager_Set_StrictMode)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool bStrictMode;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &bStrictMode))
    {
       JS_ReportError(pContext, "Failed to set PolicyManager.StrictMode");
       return JS_FALSE;
    }

    PolicyManager::Instance()->SetStrictMode(bStrictMode);
    return JS_TRUE;
}
