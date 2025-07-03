/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2018, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file
//! \brief Link VmiopMdiagElwMgr to the JavaScript engine
//!
//! Group JS functionality in a separate file to keep vmiopmdiagelwmgr.cpp cleaner

#include "core/include/jscript.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/advschd/policymn.h"
#include "core/include/script.h"
#include "core/include/cmdline.h"
#include "mdiag/lwgpu.h"

JS_CLASS(VmiopMdiagElwMgr);

static SObject VmiopMdiagElwMgr_Object
(
    "VmiopMdiagElwMgr",
    VmiopMdiagElwMgrClass,
    &VmiopElwManager_Object,
    0,
    "VmiopMdiagElwMgr (tests sriov)"
);

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    VmiopMdiagElwMgr,
    StartAllVfTests,
    0,
    "Start vmiopVmiopElw objects to run"
)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    JSObject *jsArgDb;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsArgDb)))
    {
        JS_ReportError(pContext, "Usage: VmiopMdiagElwMgr.StartAllVfTests(ArgDb)");
        return JS_FALSE;
    }

    ArgDatabase * pArgDb = JS_GET_PRIVATE(ArgDatabase, pContext, jsArgDb, "ArgDatabase");
    if (pArgDb == NULL)
    {
        JS_ReportError(pContext, "Usage: VmiopMdiagElwMgr.StartAllVfTests(ArgDb)");
        return JS_FALSE;
    }

    VmiopMdiagElwManager * pVmiopMgr = VmiopMdiagElwManager::Instance();
    RC rc = pVmiopMgr->SetupAllVfTests(pArgDb);
    if (rc != OK)
    {
        return JS_FALSE;
    }

    PolicyManager * pPolicyManager = PolicyManager::Instance();
    if (!pPolicyManager->GetStartVfTestInPolicyManager())
    {
        RETURN_RC(pVmiopMgr->RulwfTests());
    }

    return JS_FALSE;
}

//--------------------------------------------------------------------
JS_STEST_LWSTOM
(
    VmiopMdiagElwMgr,
    SetupGlobalResources,
    0,
    "Setup global Resources"
)
{
    STEST_HEADER(1, 1, "Usage: VmiopMdiagElwMgr.SetupGlobalResources(ArgDb)");
    STEST_ARG(0, JSObject*, pJsArgDb);
    RC rc;

    ArgDatabase* pArgDb =
        JS_GET_PRIVATE(ArgDatabase, pContext, pJsArgDb, "ArgDatabase");
    if (pArgDb == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (LWGpuResource::ScanSystem(pArgDb) < 0)
    {
        return JS_FALSE;
    }
    C_CHECK_RC(LWGpuResource::InitSmcResource());

    RETURN_RC(OK);
}

//--------------------------------------------------------------------
JS_STEST_LWSTOM
(
    VmiopMdiagElwMgr,
    CleanupGlobalResources,
    0,
    "Cleanup global Resources"
)
{
    STEST_HEADER(0, 0, "Usage: VmiopMdiagElwMgr.CleanupGlobalResources()");
    RC rc;

    LWGpuResource::FreeResources();

    RETURN_RC(OK);
}
