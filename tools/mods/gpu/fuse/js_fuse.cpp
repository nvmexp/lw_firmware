/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_fuse.h"
#include "gpu/fuse/fuse.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/xp.h"
#include "core/include/version.h"
#include "gpu/include/gpusbdev.h"
#include "core/tests/testconf.h"
#include "gpu/fuse/fuseutils.h"
#include "core/include/utility.h"
#include "core/include/deprecat.h"
#include "gpu/fuse/fusetypes.h"
#include "core/include/fuseutil.h"

JsFuse::JsFuse()
    : m_pFuse(NULL), m_pJsFuseObj(NULL)
{
}

//-----------------------------------------------------------------------------
static void C_JsFuse_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsFuse * pJsFuse;
    //! Delete the C++
    pJsFuse = (JsFuse *)JS_GetPrivate(cx, obj);
    delete pJsFuse;
};

//-----------------------------------------------------------------------------
static JSClass JsFuse_class =
{
    "JsFuse",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsFuse_finalize
};

//-----------------------------------------------------------------------------
static SObject JsFuse_Object
(
    "JsFuse",
    JsFuse_class,
    0,
    0,
    "Fuse JS Object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! Fuse object
RC JsFuse::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Fuse object
    if (m_pJsFuseObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsFuse.\n");
        return OK;
    }

    m_pJsFuseObj = JS_DefineObject(cx,
                                   obj,     // device object
                                   "Fuse",  // Property name
                                   &JsFuse_class,
                                   JsFuse_Object.GetJSObject(),
                                   JSPROP_READONLY);

    if (!m_pJsFuseObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsFuse instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsFuseObj, this) != JS_TRUE)
    {
        Printf(Tee::PriError,
               "Unable to set private value of JsFuse.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsFuseObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the private JS data - C++ Fuse object.
//!
Fuse* JsFuse::GetFuseObj()
{
    MASSERT(m_pFuse);
    return m_pFuse;
}

//------------------------------------------------------------------------------
//! \brief Set the associated Fuse object.
//!
//! This is called by JS GpuSubdevice/LwSwitchDevice Initialize
void JsFuse::SetFuseObj(Fuse* pFuse)
{
    m_pFuse = pFuse;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DoesSkuMatch,
                3,
                "DoesSkuMatch")
{
    STEST_HEADER(2, 3, "Usage: device.Fuse.DoesSkuMatch(Sku, Strictness, checkFs)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, skuName);
    STEST_ARG(1, INT32, strictness);
    STEST_OPTIONAL_ARG(2, bool, skipFs, false);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    FuseFilter filter = ToleranceToFuseFilter(static_cast<OldFuse::Tolerance>(strictness));
    if (skipFs)
    {
        filter |= FuseFilter::SKIP_FS_FUSES;
    }

    bool skuMatches = false;
    C_CHECK_RC(pFuse->DoesSkuMatch(skuName, &skuMatches, filter));
    if (skuMatches)
    {
        RETURN_RC(RC::OK);
    }
    RETURN_RC(RC::INCORRECT_FEATURE_SET);
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                FindSkuMatch,
                2,
                "FindSkuMatch")
{
    const char usage[] = "Usage: device.Fuse.FindSkuMatch(ChipSkus, Fuse.ALL_MATCH)\n";

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    JSObject     *pRetArray;
    UINT32        Strictness = 0;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &pRetArray)) ||
        (OK != pJS->FromJsval(pArguments[1], &Strictness)))
    {
        JS_ReportError(pContext,usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        vector<string>SkusFound;
        C_CHECK_RC(pFuse->FindMatchingSkus(&SkusFound,
                          ToleranceToFuseFilter(static_cast<OldFuse::Tolerance>(Strictness))));
        for (UINT32 i = 0; i< SkusFound.size(); i++)
        {
            C_CHECK_RC(pJS->SetElement(pRetArray, i, SkusFound[i]));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                GetFusesInfo,
                3,
                "GetFusesInfo")
{
    STEST_HEADER(3, 3, "Usage: device.Fuse.GetFusesInfo(optFusesOnly)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, bool, optFusesOnly);
    STEST_ARG(1, JSObject*, pFuseNames);
    STEST_ARG(2, JSObject*, pFuseVals);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    JavaScriptPtr pJS;
    map<string, UINT32> fuseNameValPairs;

    CHECK_RC(pFuse->GetFusesInfo(optFusesOnly, fuseNameValPairs));
    UINT32 idx = 0;
    for (const auto& fuseValPair : fuseNameValPairs)
    {
        CHECK_RC(pJS->SetElement(pFuseNames, idx, fuseValPair.first.c_str()));
        CHECK_RC(pJS->SetElement(pFuseVals, idx, fuseValPair.second));
        idx++;
    }

    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                PrintFuseValues,
                4,
                "PrintFuseValues")
{
    static Deprecation depr("Fuse.PrintFuseValues", "8/30/2018",
                            "PrintFuseValues is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;
    return JS_TRUE;
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                PrintMismatchingFuses,
                1,
                "PrintMismatchingFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    const char usage[] = "Usage: device.Fuse.PrintMismatchingFuses(ChipSku)";

    JavaScriptPtr pJS;
    string        chipSku;

    if ((NumArguments != 1) ||
        OK != pJS->FromJsval(pArguments[0], &chipSku))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    FuseFilter filter = FuseFilter::ALL_FUSES;
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        filter |= FuseFilter::LWSTOMER_FUSES_ONLY;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        C_CHECK_RC(pFuse->PrintMismatchingFuses(chipSku, filter));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                PrintFuses,
                1,
                "PrintFuses")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.PrintFuses(optFusesOnly)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, bool, optFusesOnly);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    RETURN_RC(pFuse->PrintFuses(optFusesOnly));
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                PrintRawFuses,
                0,
                "PrintRawFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    JavaScriptPtr pJS;

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        C_CHECK_RC(pFuse->PrintRawFuses());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                CheckAteFuses,
                1,
                "CheckAteFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    string        ChipSku;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &ChipSku)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.CheckAteFuses(ChipSku)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        C_CHECK_RC(pFuse->CheckAteFuses(ChipSku));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                GetLwrrentFuseRegs,
                2,
                "GetLwrrentFuseRegs")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    const char usage[] = "Usage: > let FuseRegs = new Array\n"
                         ">device.Fuse.GetLwrrentFuseRegs(FuseRegs, true )\n";

    JavaScriptPtr pJS;
    JSObject     *pRetArray;
    string        ChipSku;
    if (((NumArguments != 1) && (NumArguments != 2))||
        (OK != pJS->FromJsval(pArguments[0], &pRetArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    bool UseSimulatedFuseReg = false;
    if ((NumArguments == 2)&&
        (OK != pJS->FromJsval(pArguments[1], &UseSimulatedFuseReg)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        vector<UINT32>FuseRegValues;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        if (UseSimulatedFuseReg)
            C_CHECK_RC(pFuse->GetSimulatedFuses(&FuseRegValues));
        else
            C_CHECK_RC(pFuse->GetCachedFuseReg(&FuseRegValues));

        for (UINT32 i = 0; i< FuseRegValues.size(); i++)
        {
            C_CHECK_RC(pJS->SetElement(pRetArray, i, FuseRegValues[i]));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                GetDesiredFuses,
                2,
                "GetDesiredFuses")
{
    static Deprecation depr("Fuse.GetDesiredFuses", "4/19/2018",
                            "GetDesiredFuses is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;
    return JS_TRUE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                ReadInRecords,
                0,
                "ReadInRecords")
{
    static Deprecation depr("Fuse.ReadInRecords", "4/10/2018",
                            "ReadInRecords is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;
    return JS_TRUE;
}
//------------------------------------------------------------------------------
P_( JsFuse_Get_FuselessRecordCount );
static SProperty FuselessRecordCount
(
    JsFuse_Object,
    "FuselessRecordCount",
    0,
    0,
    JsFuse_Get_FuselessRecordCount,
    0,
    0,
    "Get number of fuseless records"
);
P_( JsFuse_Get_FuselessRecordCount )
{
    static Deprecation depr("Fuse.FuselessRecordCount", "4/10/2018",
                            "FuselessRecordCount is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pValue != 0);

    JavaScriptPtr pJavaScript;
    if (pJavaScript->ToJsval(0, pValue) == 0)
        return JS_TRUE;
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetSimulatedFuses,
                1,
                "pass in an array of fuse registers - eg. output from GetDesiredFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: >let FuseRegs = new Array\n"
                         ">device.Fuse.GetLwrrentFuseRegs(FuseRegs)\n"
                         ">FuseRegs[23] = 0xF // ilwalidate a record\n"
                         ">device.Fuse.SetInitCondition(FuseRegs)\n";

    JavaScriptPtr pJS;
    JsArray       JsRegArray;
    if ((NumArguments != 1) || (OK != pJS->FromJsval(pArguments[0], &JsRegArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        UINT32 NumFuseWord = pFuse->GetFuseSpec()->numOfFuseReg;

        vector<UINT32>SimFuses(NumFuseWord, 0);
        for (UINT32 i = 0; i < JsRegArray.size(); i++)
        {
            if (i == NumFuseWord)
                break;

            if (OK != pJS->FromJsval(JsRegArray[i], &SimFuses[i]))
            {
                RETURN_RC(RC::CANNOT_COLWERT_JSVAL_TO_ARRAY);
            }
        }

        pFuse->SetSimulatedFuses(SimFuses);
        pFuse->MarkFuseReadDirty();
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                ClearSimulatedFuses,
                0,
                "stop using simulated fuse")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        pFuse->ClearSimulatedFuses();
        RETURN_RC(OK);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                PrintRecords,
                0,
                "PrintRecords")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // We don't need to explicitly print fuseless fuse records anymore
    static Deprecation depr("Fuse.PrintRecords", "4/19/2018",
                            "PrintRecords is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        pFuse->PrintRecords();
        RETURN_RC(OK);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetOverrides,
                1,
                "SetOverrides")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: >let Override = [[\"opt_gpc_disable\", 0xc, flag]\n"
                         ">device.Fuse.SetOverrides(Override)\n";

    JavaScriptPtr pJS;
    JsArray       JsOverrides;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &JsOverrides)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        map<string, FuseOverride> OverrideList;
        for (UINT32 i = 0; i < JsOverrides.size(); i++)
        {
            JsArray OverrideEntry;
            C_CHECK_RC(pJS->FromJsval(JsOverrides[i], &OverrideEntry));
            string FuseName;
            if ((OverrideEntry.size() < 2) || (OverrideEntry.size() > 3))
            {
                Printf(Tee::PriError, "bad override array format\n");
                RETURN_RC(RC::BAD_PARAMETER);
            }
            C_CHECK_RC(pJS->FromJsval(OverrideEntry[0], &FuseName));
            FuseName = Utility::ToUpperCase(FuseName);
            if (OverrideList.find(FuseName) != OverrideList.end())
            {
                Printf(Tee::PriError, "Fuse name already entered\n");
                RETURN_RC(RC::BAD_PARAMETER);
            }

            FuseOverride Override = { 0 };
            C_CHECK_RC(pJS->FromJsval(OverrideEntry[1], &Override.value));
            if (OverrideEntry.size() == 3)
            {
                UINT32 useSkuDef;
                C_CHECK_RC(pJS->FromJsval(OverrideEntry[2], &useSkuDef));
                Override.useSkuDef = useSkuDef != 0;
            }
            OverrideList[FuseName] = Override;
        }
        Fuse* pFuse = pJsFuse->GetFuseObj();
        pFuse->SetOverrides(OverrideList);
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SkipFuses,
                1,
                "SkipFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: >let skipList = [ \"opt_priv_sec_en\" ]\n"
                         ">device.Fuse.SkipFuses(skipList)\n";

    JavaScriptPtr pJs;
    JsArray       JsSkipList;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &JsSkipList)))
    {
        pJs->Throw(pContext, RC::ILWALID_ARGUMENT, usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        for (UINT32 i = 0; i < JsSkipList.size(); i++)
        {
            string skipEntry;
            C_CHECK_RC(pJs->FromJsval(JsSkipList[i], &skipEntry));
            pFuse->AppendIgnoreList(skipEntry);
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                RegenerateRecords,
                1,
                "RegenerateRecords")
{
    static Deprecation depr("Fuse.RegenerateRecords", "4/10/2018",
                            "RegenerateRecords is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    return JS_TRUE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                BlowFuses,
                1,
                "BlowFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: >let fuses = [[\"opt_gpc_disable\", 0xc]]\n"
                         ">device.Fuse.BlowFuses(fuses)\n";

    JavaScriptPtr pJS;
    JsArray       JsFuseVals;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &JsFuseVals)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        map<string, UINT32> fuseVals;
        for (UINT32 i = 0; i < JsFuseVals.size(); i++)
        {
            JsArray fuseEntry;
            C_CHECK_RC(pJS->FromJsval(JsFuseVals[i], &fuseEntry));
            string FuseName;
            if (fuseEntry.size() != 2)
            {
                Printf(Tee::PriError, "bad fuse format\n");
                RETURN_RC(RC::BAD_PARAMETER);
            }
            C_CHECK_RC(pJS->FromJsval(fuseEntry[0], &FuseName));
            FuseName = Utility::ToUpperCase(FuseName);

            UINT32 value;
            C_CHECK_RC(pJS->FromJsval(fuseEntry[1], &value));

            fuseVals[FuseName] = value;
        }
        Fuse* pFuse = pJsFuse->GetFuseObj();
        C_CHECK_RC(pFuse->BlowFuses(fuseVals));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DumpRecordsToReg,
                1,
                "DumpRecordsToReg")
{
    static Deprecation depr("Fuse.DumpRecordsToReg", "4/19/2018",
                            "DumpRecordsToReg is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    return JS_TRUE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                CheckFuse,
                2,
                "CheckFuse")
{
    Printf(Tee::PriError, "CheckFuse is no longer supported, "
        "please check fuses via SKU checks\n");
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                GetFuseValues,
                2,
                "GetFuseValues")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    JsArray       JsFuseNames;
    JSObject     *pRetArray;

    const char usage[] = "Usage: >let Names = [[\"opt_gpc_disable\"]\n"
                         ">let RetVals = new Array;"
                         ">device.Fuse.GetFuseValues(Names, RetVals)\n";

    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &JsFuseNames)) ||
        (OK != pJS->FromJsval(pArguments[1], &pRetArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        for (UINT32 i = 0; i < JsFuseNames.size(); i++)
        {
            string FuseName;
            C_CHECK_RC(pJS->FromJsval(JsFuseNames[i], &FuseName));
            UINT32 FuseVal;
            C_CHECK_RC(pFuse->GetFuseValue(FuseName, FuseFilter::ALL_FUSES, &FuseVal));
            C_CHECK_RC(pJS->SetElement(pRetArray, i, FuseVal));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
//! Brief: Function to blow fuse rows
//
JS_STEST_LWSTOM(JsFuse,
                BlowFuseRows,
                1,
                "BlowFuseRows")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.BlowFuseRows(FuseRows)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, JsArray, jsRegArray);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    RC rc;
    JavaScriptPtr pJS;
    vector<UINT32> rowsToBlow;
    for (UINT32 i = 0; i < jsRegArray.size(); i++)
    {
        UINT32 fuseReg;
        if (pJS->FromJsval(jsRegArray[i], &fuseReg) != RC::OK)
        {
            RETURN_RC(RC::CANNOT_COLWERT_JSVAL_TO_ARRAY);
        }
        rowsToBlow.push_back(fuseReg);
    }

    Fuse* pFuse = pJsFuse->GetFuseObj();
    Printf(Tee::PriNormal, "Blowing fuses with user defined (fuseRow, value) pairs...\n");
    // Keeping FuseMacroType as 'Fuse' by default
    // TODO: Can we take FuseMacroType as a user input?
    FuseMacroType macroType = FuseMacroType::Fuse;
    RETURN_RC(pFuse->BlowFuseRows(rowsToBlow, macroType));
}

//------------------------------------------------------------------------------
//! Brief: This will mainly be used in bringup, where user can blow arbitrary
//  fuses.
JS_STEST_LWSTOM(JsFuse,
                BlowArray,
                4,
                "BlowArray")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    // This might come back for debug usage, but for now it's deprecated...
    static Deprecation depr("Fuse.BlowArray", "4/19/2018",
                            "BlowArray is no longer supported. Please use FuseSku\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJS;
    JsArray       JsRegArray;
    string        Method;
    UINT32        DurationNs;
    UINT32        LwVddMv;
    if ((NumArguments != 4) ||
        (OK != pJS->FromJsval(pArguments[0], &JsRegArray)) ||
        (OK != pJS->FromJsval(pArguments[1], &Method)) ||
        (OK != pJS->FromJsval(pArguments[2], &DurationNs)) ||
        (OK != pJS->FromJsval(pArguments[3], &LwVddMv)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.BlowArray(Array, Method, Duration, Lwvdd)");
        return JS_FALSE;
    }

    vector<UINT32>RegsToBlow;
    for (UINT32 i = 0; i < JsRegArray.size(); i++)
    {
        UINT32 FuseReg;
        if (OK != pJS->FromJsval(JsRegArray[i], &FuseReg))
        {
            RETURN_RC(RC::CANNOT_COLWERT_JSVAL_TO_ARRAY);
        }
        RegsToBlow.push_back(FuseReg);
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        Printf(Tee::PriNormal, "Blowing fuses using user array...\n");
        RETURN_RC(pFuse->BlowArray(RegsToBlow, Method, DurationNs, LwVddMv));
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                FuseSku,
                1,
                "FuseSku")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    JavaScriptPtr pJS;
    string        sku;
    if (NumArguments != 1 ||
        OK != pJS->FromJsval(pArguments[0], &sku))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.FuseSku(SkuName)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        Printf(Tee::PriNormal, "Blowing fuses using SKU %s...\n", sku.c_str());
        RETURN_RC(pFuse->FuseSku(sku));
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                BlowSku,
                4,
                "BlowSku")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    static Deprecation depr("Fuse.BlowSku", "4/19/2018",
                            "BlowSku is no longer supported. Please use FuseSku\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJS;
    string        ChipSku;
    string        Method;
    UINT32        DurationNs;
    UINT32        LwVddMv;
    if ((NumArguments != 4) ||
        (OK != pJS->FromJsval(pArguments[0], &ChipSku)) ||
        (OK != pJS->FromJsval(pArguments[1], &Method)) ||
        (OK != pJS->FromJsval(pArguments[2], &DurationNs)) ||
        (OK != pJS->FromJsval(pArguments[3], &LwVddMv)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.BlowSku(ChipSku, Method, Duration, Lwvdd)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        C_CHECK_RC(pFuse->CheckAteFuses(ChipSku));
        RETURN_RC(pFuse->FuseSku(ChipSku));
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                EnableSwFusing,
                3,
                "EnableSwFusing")
{
    static Deprecation depr("Fuse.EnableSwFusing", "4/10/2018",
                            "SW Fusing cannot be enabled it can only be disabled\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;
    return JS_TRUE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetSwSku,
                1,
                "SetSwSku")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    static Deprecation depr("Fuse.SetSwSku", "4/10/2018",
                            "SetSwSku is no longer supported, "
                            "please use SwFuseSku.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJS;
    string        SkuName;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &SkuName)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.SetSwSku(SkuName)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        RETURN_RC(pFuse->SwFuseSku(SkuName));
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SwFuseSku,
                1,
                "SwFuseSku")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    JavaScriptPtr pJS;
    string        SkuName;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &SkuName)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.SwFuseSku(SkuName)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        RETURN_RC(pFuse->SwFuseSku(SkuName));
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                CheckSwSku,
                2,
                "Validate SW fuse")
{
    STEST_HEADER(1, 2, "Usage: device.Fuse.CheckSwFuse(skuName, skipFs)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, skuName);
    STEST_OPTIONAL_ARG(1, bool, skipFs, false);
 
    FuseFilter filter = FuseFilter::NONE;
    if (skipFs)
    {
        filter = FuseFilter::SKIP_FS_FUSES;
    }

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    RETURN_RC(pFuse->CheckSwSku(skuName, filter));
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetSwFuseByName,
                2,
                "SetSwFuseByName")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    static Deprecation depr("Fuse.SetSwFuseByName", "4/10/2018",
                            "SetSwFuseByName is no longer supported, "
                            "please use SetSwFuses.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJS;
    string        FuseName;
    UINT32        FuseVal;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &FuseName)) ||
        (OK != pJS->FromJsval(pArguments[1], &FuseVal)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.SetSwFuseByName(FuseName, FuseVal)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        Fuse* pFuse = pJsFuse->GetFuseObj();
        map<string, UINT32> fuseToSet = {{ FuseName, FuseVal }};
        RETURN_RC(pFuse->SetSwFuses(fuseToSet));
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetSwFuses,
                1,
                "SetSwFuses")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    JsArray jsFuseVals;

    if (NumArguments != 1 || OK != pJS->FromJsval(pArguments[0], &jsFuseVals))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.SetSwFuses(fuseNameValPairs)");
        return JS_FALSE;
    }

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        RC rc;
        map<string, UINT32> swFuseList;
        for (UINT32 i = 0; i < jsFuseVals.size(); i++)
        {
            JsArray swFuseEntry;
            C_CHECK_RC(pJS->FromJsval(jsFuseVals[i], &swFuseEntry));
            string fuseName;
            UINT32 fuseVal;
            if (swFuseEntry.size() != 2)
            {
                Printf(Tee::PriError, "Bad sw fuse array format\n");
                RETURN_RC(RC::BAD_PARAMETER);
            }
            C_CHECK_RC(pJS->FromJsval(swFuseEntry[0], &fuseName));
            fuseName = Utility::ToUpperCase(fuseName);
            if (swFuseList.count(fuseName) != 0)
            {
                Printf(Tee::PriError, "Fuse name %s already entered\n", fuseName.c_str());
                RETURN_RC(RC::BAD_PARAMETER);
            }
            C_CHECK_RC(pJS->FromJsval(swFuseEntry[1], &fuseVal));

            swFuseList[fuseName] = fuseVal;
        }
        Fuse* pFuse = pJsFuse->GetFuseObj();
        RETURN_RC(pFuse->SetSwFuses(swFuseList));
    }
    return JS_FALSE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                UseRir,
                1,
                "UseRir")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.UseRir(fuseName)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, fuseName);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    RETURN_RC(pFuse->RequestRirOnFuse(fuseName));
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DisableRir,
                1,
                "DisableRir")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.DisableRir(fuseName)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, fuseName);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    RETURN_RC(pFuse->RequestDisableRirOnFuse(fuseName));
}
//------------------------------------------------------------------------------
P_( JsFuse_Get_IsSwFusingEnabled );
static SProperty IsSwFusingEnabled
(
    JsFuse_Object,
    "IsSwFusingEnabled",
    0,
    0,
    JsFuse_Get_IsSwFusingEnabled,
    0,
    0,
    "See if we can proceeed with SW fusing"
);
P_( JsFuse_Get_IsSwFusingEnabled )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("Fuse.IsSwFusingEnabled", "4/10/2018",
                            "IsSwFusingEnabled is deprecated. Please check IsSwFusingAllowed\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        bool SwFuseEnabled = pFuse->IsSwFusingAllowed();
        if (pJavaScript->ToJsval(SwFuseEnabled, pValue) == 0)
            return JS_TRUE;
    }
    return JS_FALSE;
}

P_( JsFuse_UseMarginRead );
static SProperty UseMarginRead
(
    JsFuse_Object,
    "UseMarginRead",
    0,
    0,
    JsFuse_UseMarginRead,
    JsFuse_UseMarginRead,
    0,
    "whether to read fuses using Margin Reads"
);
P_( JsFuse_UseMarginRead )
{
    static Deprecation depr("Fuse.UseMarginRead", "4/10/2018",
                            "UseMarginRead is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;
    return JS_TRUE;
}

P_( JsFuse_Get_WaitForIdle );
P_( JsFuse_Set_WaitForIdle );
static SProperty WaitForIdle
(
    JsFuse_Object,
    "WaitForIdle",
    0,
    true,
    JsFuse_Get_WaitForIdle,
    JsFuse_Set_WaitForIdle,
    0,
    "whether to wait for the fuse controller to become idle"
);
P_( JsFuse_Get_WaitForIdle )
{
    Printf(Tee::PriError, "Reading WaitForIdle is no longer supported\n");
    return JS_FALSE;
}
P_( JsFuse_Set_WaitForIdle )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("Fuse.WaitForIdle", "4/10/2018",
                            "WaitForIdle is deprecated. Please use DisableWaitForIdle\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        bool ToEnable;
        if (OK != pJavaScript->FromJsval(*pValue, &ToEnable))
        {
            JS_ReportError(pContext, "cannot colwert bool for WaitForIdle");
            return JS_FALSE;
        }
        if (!ToEnable)
        {
            pFuse->DisableWaitForIdle();
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsFuse,
                DisableWaitForIdle,
                1,
                "DisableWaitForIdle")
{
    MASSERT(pContext != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        pJsFuse->GetFuseObj()->DisableWaitForIdle();
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsFuse,
                UseTestValues,
                1,
                "UseTestValues")
{
    MASSERT(pContext != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        pJsFuse->GetFuseObj()->UseTestValues();
        return JS_TRUE;
    }
    return JS_FALSE;
}

P_( JsFuse_Get_UseUndoFuse );
P_( JsFuse_Set_UseUndoFuse );
static SProperty UseUndoFuse
(
    JsFuse_Object,
    "UseUndoFuse",
    0,
    0,
    JsFuse_Get_UseUndoFuse,
    JsFuse_Set_UseUndoFuse,
    0,
    "Allow or not allow usage of undo fuses"
);
P_( JsFuse_Get_UseUndoFuse )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        bool UseUndoFuse = pFuse->GetUseUndoFuse();
        if (pJavaScript->ToJsval(UseUndoFuse, pValue) == 0)
            return JS_TRUE;
    }
    return JS_FALSE;
}
P_( JsFuse_Set_UseUndoFuse )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        bool ToSet;
        if (OK != pJavaScript->FromJsval(*pValue, &ToSet))
        {
            JS_ReportError(pContext, "cannot colwert bool for UndoFuse");
            return JS_FALSE;
        }
        pFuse->SetUseUndoFuse(ToSet);
        return JS_TRUE;
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
P_( JsFuse_ReblowAttempts );
static SProperty ReblowAttempts
(
    JsFuse_Object,
    "ReblowAttempts",
    0,
    0,
    JsFuse_ReblowAttempts,
    JsFuse_ReblowAttempts,
    0,
    "number of times to attempt to issue a fuse blow attempt - take care of flaky fuse writes"
);
P_( JsFuse_ReblowAttempts )
{
    Printf(Tee::PriError, "ReblowAttempts is no longer supported\n");
    return JS_FALSE;
}

//------------------------------------------------------------------------------
P_( JsFuse_Get_XmlFilename );
static SProperty JsFuse_XmlFilename
(
    JsFuse_Object,
    "XmlFilename",
    0,
    0,
    JsFuse_Get_XmlFilename,
    0,
    JSPROP_READONLY,
    "The XML filename for this device"
);
P_( JsFuse_Get_XmlFilename )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("Fuse.XmlFilename", "6/26/2018",
                            "XmlFilename is deprecated. Use Fuse.FuseFilename instead\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        if (pJavaScript->ToJsval(pFuse->GetFuseFilename(), pValue) == 0)
            return JS_TRUE;
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
P_(JsFuse_Get_FuseFilename);
static SProperty JsFuse_FuseFileName
(
    JsFuse_Object,
    "FuseFilename",
    0,
    0,
    JsFuse_Get_FuseFilename,
    0,
    JSPROP_READONLY,
    "The fuse-file name for this device"
);
P_(JsFuse_Get_FuseFilename)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        if (pJavaScript->ToJsval(pFuse->GetFuseFilename(), pValue) == 0)
            return JS_TRUE;
    }
    return JS_FALSE;
}

P_( JsFuse_SetPrintPriority );
static SProperty JsFuse_PrintPriority
(
    JsFuse_Object,
    "PrintPriority",
    0,
    0,
    0,
    JsFuse_SetPrintPriority,
    0,
    "Set priority for printing in the fuse objects"
);
P_( JsFuse_SetPrintPriority )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;
    UINT32 Pri;
    if ((OK != pJavaScript->FromJsval(*pValue, &Pri)) ||
        (Pri > Tee::PriAlways))
    {
        JS_ReportError(pContext, "cannot colwert priority for SetPrintPriority");
        return JS_FALSE;
    }
    FuseUtils::SetVerbosePriority(static_cast<Tee::Priority>(Pri));
    return JS_TRUE;
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                EnableSelwreFuseblow,
                1,
                "EnableSelwreFuseblow")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    static Deprecation depr("Fuse.EnableSelwreFuseBlow", "4/19/2018",
                            "EnableSelwreFuseBlow is handled automatically "
                            "when necessary, thus the function is deprecated\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;
    return JS_TRUE;
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
        GetFuseKeyToInstall,
        1,
        "GetFuseKeyToInstall")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    JSObject     *pFuseKey;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pFuseKey)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.GetFuseKeyToInstall(fuseKey)");
        return JS_FALSE;
    }

    RC rc;
    string key;
    C_CHECK_RC(FuseUtils::ReadL2FuseKeyFromFile(&key));
    C_CHECK_RC(pJS->SetElement(pFuseKey, 0, key));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                EncryptFuseFile,
                3,
                "EncryptFuseFile")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string fileName;
    string keyName;
    bool isFuseKey;
    if ((NumArguments != 3) ||
        (OK != pJS->FromJsval(pArguments[0], &fileName)) ||
        (OK != pJS->FromJsval(pArguments[1], &keyName)) ||
        (OK != pJS->FromJsval(pArguments[2], &isFuseKey)))
    {
        JS_ReportError(pContext,
            "Usage: device.Fuse.EncryptFuseFile"
            "(fileToEncryt, KeyToEncrypt, isFuseKey)");
        return JS_FALSE;
    }

    RETURN_RC(FuseUtils::EncryptFuseFile(fileName, keyName, isFuseKey));
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetInstalledFuseKey,
                1,
                "SetInstalledFuseKey")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    static Deprecation depr("Fuse.SetInstalledFuseKey", "5/8/2018",
                            "SetInstalledFuseKey is deprecated, please use Fuse.L2FuseKey\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string fuseKey;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &fuseKey)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.SetInstalledFuseKey(fuseKey)");
        return JS_FALSE;
    }

    FuseUtils::SetL2FuseKey(fuseKey);
    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_(JsFuse_Set_L2FuseKey);
static SProperty L2FuseKey
(
    JsFuse_Object,
    "L2FuseKey",
    0,
    0,
    0,
    JsFuse_Set_L2FuseKey,
    0,
    "Returns true if fuse priv security is enabled."
);
P_(JsFuse_Set_L2FuseKey)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return JS_FALSE;

    JavaScriptPtr pJs;
    string fuseKey;
    if (OK != pJs->FromJsval(*pValue, &fuseKey))
    {
        JS_ReportError(pContext, "Usage: device.Fuse.L2FuseKey = fuseKey");
        return JS_FALSE;
    }

    FuseUtils::SetL2FuseKey(fuseKey);
    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_( JsFuse_Get_IsFusePrivSelwrityEnabled );
static SProperty IsFusePrivSelwrityEnabled
(
    JsFuse_Object,
    "IsFusePrivSelwrityEnabled",
    0,
    0,
    JsFuse_Get_IsFusePrivSelwrityEnabled,
    0,
    0,
    "Returns true if fuse priv security is enabled."
);
P_( JsFuse_Get_IsFusePrivSelwrityEnabled )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        bool FusePrivSecEnabled = pFuse->IsFusePrivSelwrityEnabled();
        if (pJavaScript->ToJsval(FusePrivSecEnabled, pValue) == 0)
            return JS_TRUE;
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
P_( JsFuse_Get_IsSwFusingAllowed );
static SProperty IsSwFusingAllowed
(
    JsFuse_Object,
    "IsSwFusingAllowed",
    0,
    0,
    JsFuse_Get_IsSwFusingAllowed,
    0,
    0,
    "See if it is possible to enable SW fusing"
);
P_( JsFuse_Get_IsSwFusingAllowed )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsFuse *pJsFuse;
    if ((pJsFuse = JS_GET_PRIVATE(JsFuse, pContext, pObject, "JsFuse")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Fuse* pFuse = pJsFuse->GetFuseObj();
        bool SwFuseEnabled = pFuse->IsSwFusingAllowed();
        if (pJavaScript->ToJsval(SwFuseEnabled, pValue) == 0)
            return JS_TRUE;
    }
    return JS_FALSE;
}

//------------------------------------------------------------------------------
P_( JsFuse_Get_IsFuseSelwritySupported );
static SProperty IsFuseSelwritySupported
(
    JsFuse_Object,
    "IsFuseSelwritySupported",
    0,
    0,
    JsFuse_Get_IsFuseSelwritySupported,
    0,
    0,
    "Returns true if fuse security is supported."
);
P_( JsFuse_Get_IsFuseSelwritySupported )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("Fuse.IsFuseSelwritySupported", "4/10/2018",
                            "IsFuseSelwritySupported is always true.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJavaScript;
    if (pJavaScript->ToJsval(true, pValue) == 0)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SwReset,
                0,
                "Trigger a SW Reset when fusing")
{
    static Deprecation depr("Fuse.SwReset", "4/30/2018",
                            "SwReset from JS is no longer supported.\n");

    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    STEST_HEADER(0, 0, "Usage: device.Fuse.SwReset()");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");

    Fuse* pFuse = pJsFuse->GetFuseObj();

    RETURN_RC(pFuse->SwReset());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DecodeIff,
                0,
                "Decode and print the IFF records")
{
    STEST_HEADER(0, 0, "Usage: device.Fuse.DecodeIff()");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");

    Fuse* pFuse = pJsFuse->GetFuseObj();

    RETURN_RC(pFuse->DecodeIff());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DecodeSkuIff,
                1,
                "Decode and print the IFF records")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.DecodeSkuIff(skuName)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");

    JavaScriptPtr pJS;
    string skuName;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &skuName)))
    {
        JS_ReportError(pContext,
                       "Usage: device.Fuse.DecodeSkuIff(skuName)");
        return JS_FALSE;
    }

    Fuse* pFuse = pJsFuse->GetFuseObj();

    RETURN_RC(pFuse->DecodeIff(skuName));
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                GetSkuReworkInfo,
                3,
                "GetSkuReworkInfo")
{
    STEST_HEADER(3, 3, "Usage: device.Fuse.GetSkuReworkInfo(srcSku, destSku, fileName)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, srcSku);
    STEST_ARG(1, string, destSku);
    STEST_ARG(2, string, fileName);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    RETURN_RC(pFuse->GetSkuReworkInfo(srcSku, destSku, fileName));
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetFsFuseList,
                1,
                "SetFsFuseList")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.GetFsFuseList(fsFuseList)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, JsArray, JsFsFuseList);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();

    for (UINT32 i = 0; i < JsFsFuseList.size(); i++)
    {
        string fsFuse;
        C_CHECK_RC(pJavaScript->FromJsval(JsFsFuseList[i], &fsFuse));
        pFuse->AppendFsFuseList(fsFuse);
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetCoreVoltage,
                1,
                "SetCoreVoltage")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetCoreVoltage(vddMv)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, UINT32, vddMv);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();

    RETURN_RC(pFuse->SetCoreVoltage(vddMv));
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetFuseBlowTime,
                1,
                "SetFuseBlowTime")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetFuseBlowTime(blowTimeNs)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, UINT32, blowTimeNs);

    Fuse* pFuse = pJsFuse->GetFuseObj();
    pFuse->SetFuseBlowTime(blowTimeNs);

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetNumValidRir,
                1,
                "SetNumValidRir")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetNumValidRir(numValidRecords)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, UINT32, numValidRecords);

    Fuse* pFuse = pJsFuse->GetFuseObj();
    pFuse->SetNumValidRir(numValidRecords);

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetOverridePoisonRir,
                1,
                "SetOverridePoisonRir")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetOverridePoisonRir(overrideVal)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, UINT32, overrideVal);

    Fuse* pFuse = pJsFuse->GetFuseObj();
    pFuse->SetOverridePoisonRir(overrideVal);

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetIgnoreRawOptMismatch,
                1,
                "SetIgnoreRawOptMismatch")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetIgnoreRawOptMismatch(bIgnore)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, bool, ignore);

    Fuse* pFuse = pJsFuse->GetFuseObj();
    pFuse->SetIgnoreRawOptMismatch(ignore);

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
P_(JsFuse_Set_ShowFusingPrompt);
static SProperty JsFuse_ShowFusingPrompt
(
    JsFuse_Object,
    "ShowFusingPrompt",
    0,
    0,
    0,
    JsFuse_Set_ShowFusingPrompt,
    0,
    "Show prompt for HW fusing"
);

P_(JsFuse_Set_ShowFusingPrompt)
{
    MASSERT(pContext != 0);
    MASSERT(pValue != 0);

    JavaScriptPtr pJs;
    bool showFusingPrompt;
    RC rc = pJs->FromJsval(*pValue, &showFusingPrompt);
    if (rc != RC::OK)
    {
        pJs->Throw(pContext, rc,
                   Utility::StrPrintf("Usage: Fuse.ShowFusingPrompt = showFusingPrompt"));
        return JS_FALSE;
    }

    FuseUtils::SetShowFusingPrompt(showFusingPrompt);
    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_(JsFuse_Set_UseDummyFuses);
static SProperty JsFuse_UseDummyFuses
(
    JsFuse_Object,
    "UseDummyFuses",
    0,
    0,
    0,
    JsFuse_Set_UseDummyFuses,
    0,
    "Use SW array instead of the physical fuse macro"
);

P_(JsFuse_Set_UseDummyFuses)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    bool useDummyFuses;
    if (OK != pJs->FromJsval(*pValue, &useDummyFuses))
    {
        JS_ReportError(pContext, "Usage: Fuse.UseDummyFuses = <useDummyFuses>");
        return JS_FALSE;
    }

    FuseUtils::SetUseDummyFuses(useDummyFuses);
    return JS_TRUE;
}

//-------------------------------------------------------------------------------
P_(JsFuse_Set_FuseFile);
static SProperty JsFuse_FuseFile
(
    JsFuse_Object,
    "FuseFile",
    0,
    0,
    0,
    JsFuse_Set_FuseFile,
    0,
    "Use the given fuse file instead of the default"
);

P_(JsFuse_Set_FuseFile)
{
    MASSERT(pContext != 0);
    MASSERT(pValue != 0);

    JavaScriptPtr pJs;
    string fuseFile;
    RC rc = pJs->FromJsval(*pValue, &fuseFile);
    if (rc != RC::OK)
    {
        pJs->Throw(pContext, rc, 
                   Utility::StrPrintf("Usage: Fuse.FuseFile = fuseFileName"));
        return JS_FALSE;
    }

    FuseUtils::SetFuseFile(fuseFile);
    return JS_TRUE;
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                GetFsInfo,
                3,
                "GetFsInfo")
{
    STEST_HEADER(3, 3, "Usage: FuseUtil.GetFsInfo(FileName, ChipSku, RetObj)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, FileName);
    STEST_ARG(1, string, ChipSku);
    STEST_ARG(2, JSObject*, pRetObj);

    RC rc;
    map<string, FuseUtil::DownbinConfig> fsSettings;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    C_CHECK_RC(pFuse->GetFsInfo(FileName, ChipSku, &fsSettings));

    for (auto& unitConfigPair : fsSettings)
    {
        const string& unit = unitConfigPair.first;
        const FuseUtil::DownbinConfig& unitConfig = unitConfigPair.second;
        JSObject* pObj;
        C_CHECK_RC(pJavaScript->DefineProperty(pRetObj, unit.c_str(), &pObj));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "isBurnCount", unitConfig.isBurnCount));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "disableCount", unitConfig.disableCount));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "matchValues", unitConfig.matchValues));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "mustDisableList", unitConfig.mustDisableList));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "mustEnableList", unitConfig.mustEnableList));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "enableCountPerGroup",
                                            unitConfig.enableCountPerGroup));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "maxBurnPerGroup", unitConfig.maxBurnPerGroup));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "minEnablePerGroup",
                                            unitConfig.minEnablePerGroup));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "groupIlwalidIfBurned",
                                            unitConfig.groupIlwalidIfBurned));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "hasReconfig", unitConfig.hasReconfig));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "reconfigCount", unitConfig.reconfigCount));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "repairMinCount", unitConfig.repairMinCount));
        C_CHECK_RC(pJavaScript->SetProperty(pObj, "repairMaxCount", unitConfig.repairMaxCount));
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                SetNumSpareFFRows,
                1,
                "SetNumSpareFFRows")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetNumSpareFFRows(numFFRows)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, UINT32, numFFRows);

    Fuse* pFuse = pJsFuse->GetFuseObj();
    pFuse->SetNumSpareFFRows(numFFRows);

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
RC JsFuse::SetReworkBinaryFilename(const string& filename)
{
    MASSERT(m_pFuse);
    return m_pFuse->SetReworkBinaryFilename(filename);
}
JS_STEST_BIND_ONE_ARG(JsFuse, SetReworkBinaryFilename,
                      filename, string,
                      "Sets the fuse rework binary filename");

//------------------------------------------------------------------------------
P_(JsFuse_Set_IgnoreAte);
static SProperty JsFuse_IgnoreAte
(
    JsFuse_Object,
    "IgnoreAte",
    0,
    0,
    0,
    JsFuse_Set_IgnoreAte,
    0,
    "ignore the ate fuses while matching with the sku"
);

P_(JsFuse_Set_IgnoreAte)
{ 
    MASSERT(pContext != 0);
    MASSERT(pValue != 0);

    JavaScriptPtr pJs;
    bool bIgnoreAte;
    RC rc = pJs->FromJsval(*pValue, &bIgnoreAte);
    if (rc != RC::OK)
    {
        pJs->Throw(pContext, rc,
                   Utility::StrPrintf("Usage: Fuse.IgnoreAte = <boolean>"));
        return JS_FALSE;
    }

    FuseUtils::SetIgnoreAte(bIgnoreAte);
    return JS_TRUE;
}

JS_CLASS(DownbinFlag);
static SObject DownbinFlag_Object
(
    "DownbinFlag",
    DownbinFlagClass,
    0,
    0,
    "DownbinFlag JS Object"
);
using DownbinFlag = Downbin::DownbinFlag;
PROP_CONST(DownbinFlag, SKIP_FS_IMPORT, static_cast<UINT32>(DownbinFlag::SKIP_FS_IMPORT));
PROP_CONST(DownbinFlag, SKIP_GPU_IMPORT, static_cast<UINT32>(DownbinFlag::SKIP_GPU_IMPORT));
PROP_CONST(DownbinFlag, SKIP_DEFECTIVE_FUSE, static_cast<UINT32>(DownbinFlag::SKIP_DEFECTIVE_FUSE));
PROP_CONST(DownbinFlag, SKIP_DOWN_BIN, static_cast<UINT32>(DownbinFlag::SKIP_DOWN_BIN));
PROP_CONST(DownbinFlag, IGNORE_SKU_FS, static_cast<UINT32>(DownbinFlag::IGNORE_SKU_FS));
PROP_CONST(DownbinFlag, USE_BOARD_FS, static_cast<UINT32>(DownbinFlag::USE_BOARD_FS));

JS_STEST_LWSTOM(JsFuse, SetDownbinInfo, 1, "Set DownBin related variables")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.SetDownbinInfo(downbinInfo)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, JSObject*, pDownbinInfo);

    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    Downbin::DownbinInfo downbinInfo = {};
    UINT32 downbinFlags = 0;
    UINT32 fsMode = 0;
    JavaScriptPtr pJs;
    JsArray JsFuseOverridesArray;
    map<string, FuseOverride> fuseOverrideList;
    CHECK_RC(pJs->GetProperty(pDownbinInfo, "downbinFlags", &downbinFlags));
    CHECK_RC(pJs->GetProperty(pDownbinInfo, "fslibMode", &fsMode));
    CHECK_RC(pJs->GetProperty(pDownbinInfo, "fuseOverrideArray", &JsFuseOverridesArray));

    downbinInfo.downbinFlags = static_cast<Downbin::DownbinFlag>(downbinFlags);
    downbinInfo.fsMode = static_cast<FsLib::FsMode>(fsMode);
    for (UINT32 i = 0; i < JsFuseOverridesArray.size(); i++)
    {
        JsArray JsfuseOverrideEntry;
        C_CHECK_RC(pJs->FromJsval(JsFuseOverridesArray[i], &JsfuseOverrideEntry));
        string fuseName;
        // JsfuseOverrideEntry should be [<fuseName>, <value>, <useSkuDef>(optional flag)]
        if ((JsfuseOverrideEntry.size() < 2) || (JsfuseOverrideEntry.size() > 3))
        {
            Printf(Tee::PriError, "bad override array format\n");
            RETURN_RC(RC::BAD_PARAMETER);
        }
        C_CHECK_RC(pJs->FromJsval(JsfuseOverrideEntry[0], &fuseName));
        fuseName = Utility::ToUpperCase(fuseName);
        if (fuseOverrideList.find(fuseName) != fuseOverrideList.end())
        {
            Printf(Tee::PriError, "Fuse name %s already entered\n", fuseName.c_str());
            RETURN_RC(RC::BAD_PARAMETER);
        }
        FuseOverride fuseOverride = {0};
        C_CHECK_RC(pJs->FromJsval(JsfuseOverrideEntry[1], &fuseOverride.value));
        if (JsfuseOverrideEntry.size() == 3)
        {
            UINT32 useSkuDef;
            C_CHECK_RC(pJs->FromJsval(JsfuseOverrideEntry[2], &useSkuDef));
            fuseOverride.useSkuDef = useSkuDef != 0;
        }
        fuseOverrideList[fuseName] = fuseOverride;
    }
    downbinInfo.fuseOverrideList = fuseOverrideList;

    RETURN_RC(pFuse->SetDownbinInfo(downbinInfo));
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DumpFsInfo,
                1,
                "DumpFsInfo")
{
    STEST_HEADER(1, 1, "Usage: device.Fuse.DumpFsInfo(chipSku)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, string, chipSku);
    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    C_CHECK_RC(pFuse->DumpFsInfo(chipSku));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                FuseOnlyFs,
                1,
                "FuseOnlyFs")
{
    STEST_HEADER(2, 2, "Usage: device.Fuse.FuseOnlyFs(isSwFusing, fsFusingOpt)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    STEST_ARG(0, bool, isSwFusing);
    STEST_ARG(1, string, fsFusingOpt);
    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    C_CHECK_RC(pFuse->FuseOnlyFs(isSwFusing, fsFusingOpt));
    RETURN_RC(rc);
}
//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFuse,
                DumpFsScriptInfo,
                0,
                "DumpFsScriptInfo")
{
    STEST_HEADER(0, 0, "Usage: device.Fuse.DumpFsInfo(chipSku)");
    STEST_PRIVATE(JsFuse, pJsFuse, "JsFuse");
    RC rc;
    Fuse* pFuse = pJsFuse->GetFuseObj();
    C_CHECK_RC(pFuse->DumpFsScriptInfo());
    RETURN_RC(rc);
}
