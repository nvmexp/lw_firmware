/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \brief Link VmiopElwManager to the JavaScript engine

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/argdb.h"
#include "gpu/vmiop/vmiopelwmgr.h"

JS_CLASS(VmiopElwManager);

SObject VmiopElwManager_Object
(
    "VmiopElwManager",
    VmiopElwManagerClass,
    0,
    0,
    "VmiopElwManager (tests SRIOV)"
);

//--------------------------------------------------------------------
// ConfigFile property
//
CLASS_PROP_READWRITE_LWSTOM
(
    VmiopElwManager,
    ConfigFile,
    "VF exelwable script file list"
);

P_(VmiopElwManager_Get_ConfigFile)
{
    JavaScriptPtr pJs;
    const string& configFile = VmiopElwManager::Instance()->GetConfigFile();
    RC rc = pJs->ToJsval(configFile, pValue);
    if (rc != OK)
    {
        pJs->Throw(pContext, rc,
                   "Error colwerting result of VmiopElwManager.ConfigFile");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(VmiopElwManager_Set_ConfigFile)
{
    JavaScriptPtr pJs;
    string configFile;
    RC rc = pJs->FromJsval(*pValue, &configFile);
    if (rc != OK)
    {
        pJs->Throw(pContext, rc,
                   "Error colwerting input of VmiopElwManager.ConfigFile");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    VmiopElwManager::Instance()->SetConfigFile(configFile);
    return JS_TRUE;
}

//--------------------------------------------------------------------
// VfCount property
//
CLASS_PROP_READWRITE_LWSTOM
(
    VmiopElwManager,
    VfCount,
    "VF exelwable script file list"
);

P_(VmiopElwManager_Get_VfCount)
{
    JavaScriptPtr pJs;
    const UINT32 vfCount = VmiopElwManager::Instance()->GetVfCount();
    RC rc = pJs->ToJsval(vfCount, pValue);
    if (rc != OK)
    {
        pJs->Throw(pContext, rc,
                   "Error colwerting result of VmiopElwManager.VfCount");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(VmiopElwManager_Set_VfCount)
{
    JavaScriptPtr pJs;
    UINT32 vfCount;
    RC rc = pJs->FromJsval(*pValue, &vfCount);
    if (rc != OK)
    {
        pJs->Throw(pContext, rc,
                   "Error colwerting input of VmiopElwManager.VfCount");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    VmiopElwManager::Instance()->SetVfCount(vfCount);
    return JS_TRUE;
}

//--------------------------------------------------------------------
// GfidAttachedToProcess property
//
P_(VmiopElwManager_Get_GfidAttachedToProcess)
{
    JavaScriptPtr pJs;
    const UINT32 gfid = VmiopElwManager::Instance()->GetGfidAttachedToProcess();
    RC rc = pJs->ToJsval(gfid, pValue);
    if (rc != OK)
    {
        pJs->Throw(pContext, rc,
            "Error colwerting result of VmiopElwManager.GfidAttachedToProcess");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

static SProperty VmiopElwManager_GfidAttachedToProcess
(
    VmiopElwManager_Object,
    "GfidAttachedToProcess",
    0,
    0,
    VmiopElwManager_Get_GfidAttachedToProcess,
    0,
    JSPROP_READONLY,
    "GFID attached to current process"
);

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    VmiopElwManager,
    StartAllVfTests,
    0,
    "Start vmiopVmiopElw objects to run"
)
{
    STEST_HEADER(1, 1, "Usage: VmiopElwManager.StartAllVfTests(ArgDb)");
    STEST_ARG(0, JSObject*, pJsArgDb);
    VmiopElwManager* pVmiopManager = VmiopElwManager::Instance();
    RC rc;

    ArgDatabase* pArgDb =
        JS_GET_PRIVATE(ArgDatabase, pContext, pJsArgDb, "ArgDatabase");
    if (pArgDb == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    C_CHECK_RC(pVmiopManager->SetupAllVfTests(pArgDb));
    C_CHECK_RC(pVmiopManager->RulwfTests());
    RETURN_RC(OK);
}

//--------------------------------------------------------------------

JS_STEST_LWSTOM
(
    VmiopElwManager,
    WaitAllVfTestsDone,
    0,
    "Wait for all vf tests to finish"
)
{
    STEST_HEADER(0, 0, "Usage: VmiopElwManager.WaitAllVfTestsDone()");
    VmiopElwManager* pVmiopManager = VmiopElwManager::Instance();
    RC rc;

    C_CHECK_RC(pVmiopManager->WaitAllPluginThreadsDone());
    RETURN_RC(OK);
}
