/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008, 2013, 2015, 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/js_pmu.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/xp.h"
#include "core/include/rc.h"
#include "gpu/perf/pmusub.h"
#include "ctrl/ctrl2080.h"

JsPmu::JsPmu()
    : m_pPmu(NULL), m_pJsPmuObj(NULL)
{
}

//-----------------------------------------------------------------------------
static void C_JsPmu_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPmu * pJsPmu;
    //! Delete the C++
    pJsPmu = (JsPmu *)JS_GetPrivate(cx, obj);
    delete pJsPmu;
};

//-----------------------------------------------------------------------------
static JSClass JsPmu_class =
{
    "JsPmu",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsPmu_finalize
};

//-----------------------------------------------------------------------------
static SObject JsPmu_Object
(
    "JsPmu",
    JsPmu_class,
    0,
    0,
    "Pmu JS Object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! PMU object
RC JsPmu::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsPmuObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsPmu.\n");
        return OK;
    }

    m_pJsPmuObj = JS_DefineObject(cx,
                                  obj, // GpuSubdevice object
                                  "Pmu", // Property name
                                  &JsPmu_class,
                                  JsPmu_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsPmuObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsPerf instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPmuObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPmu.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPmuObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the private JS data - C++ PMU object.
//!
PMU * JsPmu::GetPmuObj()
{
    MASSERT(m_pPmu);
    return m_pPmu;
}

//------------------------------------------------------------------------------
//! \brief Set the associated PMU object.
//!
//! This is called by JS GpuSubdevice Initialize
void JsPmu::SetPmuObj(PMU * pPmu)
{
    m_pPmu = pPmu;
}

//-----------------------------------------------------------------------------
//! \brief PmuConst javascript interface class
//!
//! Create a JS class to declare PMU constants on
//!
JS_CLASS(PmuConst);

//-----------------------------------------------------------------------------
//! \brief ParamConst javascript interface object
//!
//! ParamConst javascript interface
//!
static SObject PmuConst_Object
(
    "PmuConst",
    PmuConstClass,
    0,
    0,
    "PmuConst JS Object"
);
PROP_CONST(PmuConst, ELPG_GRAPHICS_ENGINE, PMU::ELPG_GRAPHICS_ENGINE);
PROP_CONST(PmuConst, ELPG_VIDEO_ENGINE, PMU::ELPG_VIDEO_ENGINE);
PROP_CONST(PmuConst, ELPG_VIC_ENGINE, PMU::ELPG_VIC_ENGINE);
PROP_CONST(PmuConst, ELPG_DI_ENGINE, PMU::ELPG_DI_ENGINE);
PROP_CONST(PmuConst, ELPG_MS_ENGINE, PMU::ELPG_MS_ENGINE);
PROP_CONST(PmuConst, ELPG_ALL_ENGINES, PMU::ELPG_ALL_ENGINES);

PROP_CONST(PmuConst, PMU_ELPG_ENABLED,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_PMU_ELPG_ENABLED);
PROP_CONST(PmuConst, IDLE_THRESHOLD,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD);
PROP_CONST(PmuConst, POST_POWERUP_THRESHOLD,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD);
PROP_CONST(PmuConst, DELAY_INTERVAL,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_DELAY_INTERVAL);
PROP_CONST(PmuConst, PWRRAIL_IDLE_THRESHOLD,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_IDLE_THRESHOLD);
PROP_CONST(PmuConst, PWRRAIL_PREDICTIVE_EN,
           DRF_DEF(2080_CTRL_MC, _POWERGATING_PARAMETER,
                   _PWRRAIL_IDLE_THRESHOLD_PREDICTIVE, _EN));
PROP_CONST(PmuConst, PWRRAIL_PREDICTIVE_DIS,
           DRF_DEF(2080_CTRL_MC, _POWERGATING_PARAMETER,
                   _PWRRAIL_IDLE_THRESHOLD_PREDICTIVE, _DIS));
PROP_CONST(PmuConst, INITIALIZED,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_INITIALIZED);
PROP_CONST(PmuConst, GATINGCOUNT,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT);
PROP_CONST(PmuConst, DENYCOUNT,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_DENYCOUNT);
PROP_CONST(PmuConst, NUM_POWERGATEABLE_ENGINES,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES);
PROP_CONST(PmuConst, PSORDER,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_PSORDER);
PROP_CONST(PmuConst, ZONELUT,
           LW2080_CTRL_MC_POWERGATING_PARAMETER_ZONELUT);

// LPWR features supported by RM
PROP_CONST(PmuConst, MSCG, static_cast<UINT32>(PMU::LpwrFeature::MSCG));
PROP_CONST(PmuConst, TPCPG, static_cast<UINT32>(PMU::LpwrFeature::TPCPG));

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPmu,
                  GetLwrrentElpgMask,
                  0,
                  "Get lwrrently active Elpg mask")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJavaScript;
    JsPmu *pJsPmu;
    UINT32 EngineMask;

    if (NumArguments)
    {
        JS_ReportError(pContext, "Usage: var Maskvalue ="
                " subdev.Pmu.GetLwrrentElpgMask()");
        return JS_FALSE;
    }

    if (NULL != (pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")))
    {
        if (OK !=
            ((pJsPmu->GetPmuObj())->GetPowerGateEnableEngineMask(&EngineMask)))
        {
            JS_ReportError(pContext, "MODS failed to get current ElpgMask");
            return JS_FALSE;
        }
    }
    else
    {
        JS_ReportError(pContext, "JSError: Undefined object JsPMU");
        return JS_FALSE;
    }
    if (OK == (pJavaScript->ToJsval(EngineMask, pReturlwalue)))
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPmu,
                SetElpgMask,
                1,
                "Set newer Elpgmask value")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJavaScript;
    UINT32 ElpgMask = 0;

    const char usage[] =
        "Usage: var status = subdev.Pmu.SetElpgMask(ElpgMaskValue)";

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ElpgMask)))

    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPmu *pJsPmu;
    if (NULL != (pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")))
    {
        RC rc;
        C_CHECK_RC(pJsPmu->GetPmuObj()->SetPowerGateEnableEngineMask(ElpgMask));
        RETURN_RC(rc);
    }
    else
    {
        JS_ReportError(pContext, "JSError: Undefined object JsPMU");
        return JS_FALSE;
    }
}

JS_STEST_LWSTOM(JsPmu,
                SetPowerGatingParameters,
                1,
                "Set power gating parameters")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] =
        "Usage: subdev.Pmu.SetPowerGatingParameters("
        "[[Parameter, Index, Value],...], Flags)";

    if((NumArguments > 2) || (NumArguments < 1))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    JsArray       jsOuterArray;
    JsArray       jsInnerArray;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;
    UINT32        param, paramExt, paramVal;
    LW2080_CTRL_POWERGATING_PARAMETER newParam = {0};

    if (OK != pJavaScript->FromJsval(pArguments[0], &jsOuterArray))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Optional Argument;
    UINT32 PgFlags = 0;
    if (NumArguments > 1)
    {
        pJavaScript->FromJsval(pArguments[1], &PgFlags);
    }

    MASSERT(jsOuterArray.size());
    for (UINT32 paramIndex = 0; paramIndex < jsOuterArray.size(); paramIndex++)
    {
        if ((OK != pJavaScript->FromJsval(jsOuterArray[paramIndex],
                                          &jsInnerArray)) ||
            (jsInnerArray.size() != 3) ||
            (OK != pJavaScript->FromJsval(jsInnerArray[0], &param)) ||
            (OK != pJavaScript->FromJsval(jsInnerArray[1], &paramExt)) ||
            (OK != pJavaScript->FromJsval(jsInnerArray[2], &paramVal)))

        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        newParam.parameter = param;
        newParam.parameterExtended = paramExt;
        newParam.parameterValue = paramVal;
        pgParams.push_back(newParam);
    }

    JsPmu *pJsPmu;
    if ((pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")) != 0)
    {
        RC rc;
        PMU* pPmu = pJsPmu->GetPmuObj();
        C_CHECK_RC(pPmu->SetPowerGatingParameters(&pgParams, PgFlags));
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPmu,
                  GetPowerGatingParameter,
                  2,
                  "Get a power gating parameter")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32        param, paramExt;
    LW2080_CTRL_POWERGATING_PARAMETER getParam = {0};
    const char usage[] =
        "Usage: subdev.Pmu.GetPowerGatingParameter(Parameter, Index)";

    if ((NumArguments != 2)  ||
        (OK != pJavaScript->FromJsval(pArguments[0], &param)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &paramExt)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;

    getParam.parameter = param;
    getParam.parameterExtended = paramExt;
    pgParams.push_back(getParam);

    JsPmu *pJsPmu;
    if ((pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")) != 0)
    {
        PMU* pPmu = pJsPmu->GetPmuObj();
        if (OK != pPmu->GetPowerGatingParameters(&pgParams))
        {
            JS_ReportError(pContext,
                       "GetPowerGatingParameters : Failed to get parameter\n");
            return JS_FALSE;
        }
        if (pJavaScript->ToJsval((UINT32)pgParams[0].parameterValue,
                                 pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPmu,
                PrintPowerGatingParameters,
                2,
                "Print power gating parameters")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 engine;
    bool   bShowThresholds;

    const char usage[] =
        "Usage: subdev.Pmu.PrintPowerGatingParams(engine, bShowThresholds)";

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &engine)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &bShowThresholds)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPmu *pJsPmu;
    if ((pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")) != 0)
    {
        PMU* pPmu = pJsPmu->GetPmuObj();
        pPmu->PrintPowerGatingParams(Tee::PriHigh,
                                     (PMU::PgTypes)engine,
                                     bShowThresholds);
        RETURN_RC(OK);
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPmu,
                  IsPmuUcodeReady,
                  0,
                  "Is current state of PMU Ucode Ready")
{
    MASSERT(pContext     !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;

    UINT32 UCodeState;
    JsPmu *pJsPmu;
    if ((pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")) != 0)
    {
        PMU* pPmu = pJsPmu->GetPmuObj();
        if ( OK != pPmu->GetUcodeState(&UCodeState))
        {
            JS_ReportError(pContext,
                    "IsPmuUcodeReady : Failed to get UCODE state\n");
            return JS_FALSE;
        }
        else
        {
            bool IsReady = (UCodeState ==  LW85B6_CTRL_PMU_UCODE_STATE_READY);
            if (pJavaScript->ToJsval(IsReady, pReturlwalue) == OK)
               return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPmu,
                PowerGateOn,
                2,
                "Simulate a Power Gate On from PMU")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] =
        "Usage: subdev.Pmu.PowerGateOn(Engine)";

    JavaScriptPtr pJavaScript;
    UINT32 Engine;
    UINT32 TimeoutMs;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Engine)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &TimeoutMs)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPmu *pJsPmu;
    if ((pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pObject, "JsPmu")) != 0)
    {
        RC rc;
        PMU* pPmu = pJsPmu->GetPmuObj();
        C_CHECK_RC(pPmu->PowerGateOn((PMU::PgTypes)Engine, TimeoutMs));
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPmu,
                  PStateSupportsLpwrFeature,
                  2,
                  "Returns true if a LPWR feature is supported at a PState")
{
    const char* usageStr =
        "let bSupported = Pmu.PStateSupportsLpwrFeature(0, PmuConst.MSCG);\n";

    STEST_HEADER(2, 2, usageStr);
    STEST_PRIVATE(JsPmu, pJsPmu, "JsPmu");
    STEST_ARG(0, UINT32, pstateNum);
    STEST_ARG(1, UINT32, lFeat);

    PMU* pPmu = pJsPmu->GetPmuObj();
    PMU::LpwrFeature feature = static_cast<PMU::LpwrFeature>(lFeat);
    bool bSupported;

    if (OK != pPmu->PStateSupportsLpwrFeature(pstateNum, feature, &bSupported))
    {
        bSupported = false;
    }
    if (OK == pJavaScript->ToJsval(bSupported, pReturlwalue))
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPmu,
                GetMscgCount,
                1,
                "Returns the MSCG ungating count")
{
    STEST_HEADER(1, 1, "Usage: rc = GpuSub.Pmu.GetMscgCount(retCnt);\n");
    STEST_PRIVATE(JsPmu, pJsPmu, "JsPmu");
    STEST_ARG(0, JSObject*, pRetCount);
    RC rc;
    PMU* pPmu = pJsPmu->GetPmuObj();
    UINT32 count;
    C_CHECK_RC(pPmu->GetMscgEntryCount(&count));
    RETURN_RC(pJavaScript->SetElement(pRetCount, 0, count));
}

//-----------------------------------------------------------------------------
//!\brief Elpg owner JSSupport

static JSBool C_ElpgOwner_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN    argc,
    jsval   *argv,
    jsval   *rval
)
{
    if (argc != 0)
    {
        JS_ReportError( cx, "Usage: new ElpgOwner()");
        return JS_FALSE;
    }

    //Create the private data
    ElpgOwner *pElpgOwner = new ElpgOwner();
    MASSERT(pElpgOwner);

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    // Finally tie tie the ElpgOwner to the new Object
    return JS_SetPrivate(cx, obj, pElpgOwner);
}
//-----------------------------------------------------------------------------
static void C_ElpgOwner_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx);
    MASSERT(obj);
    ElpgOwner *pClass = (ElpgOwner *)JS_GetPrivate(cx, obj);
    if (pClass)
        delete pClass;
}

//-----------------------------------------------------------------------------
static JSClass ElpgOwner_class =
{
    "ElpgOwner",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_ElpgOwner_finalize
};

//-----------------------------------------------------------------------------
static SObject ElpgOwner_Object
(
    "ElpgOwner",
    ElpgOwner_class,
    0,
    0,
    "Elpgowner object",
    C_ElpgOwner_constructor
);

//-----------------------------------------------------------------------------
C_(ElpgOwner_ClaimElpg)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JSObject *pJsPmuObj = NULL;
    JavaScriptPtr pJs;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &pJsPmuObj)))
    {
        JS_ReportError(pContext, "Usage: var status = "
                "ElpgOwner.ClaimElpg(JsPMUobj)");
        return JS_FALSE;
    }
    ElpgOwner *pElpgOwner = (ElpgOwner *) JS_GetPrivate(pContext, pObject);
    if (!pElpgOwner)
    {
        JS_ReportError(pContext, "Bad ElpgOwner object");
        return JS_FALSE;
    }

    JsPmu *pJsPmu = JS_GET_PRIVATE(JsPmu, pContext, pJsPmuObj, "JsPmu");
    if (!pJsPmu)
    {
        JS_ReportError(pContext, "Bad JS_PMU object");
        return JS_FALSE;
    }

    PMU *pPmu = pJsPmu->GetPmuObj();
    if (!pPmu)
    {
        JS_ReportError(pContext, "Bad PMU object");
        return JS_FALSE;
    }
    RC rc;
    C_CHECK_RC(pElpgOwner->ClaimElpg(pPmu));
    RETURN_RC(rc);
}

static STest ElpgOwner_ClaimElpg
(
    ElpgOwner_Object,
    "ClaimElpg",
    C_ElpgOwner_ClaimElpg,
    1,
    "Make caller _mods_thread the owner of Elpg controls"
);

//-----------------------------------------------------------------------------
C_(ElpgOwner_ReleaseElpg)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: var status = ElpgOwner.ReleaseElpg()");
        return JS_FALSE;
    }
    ElpgOwner *pElpgOwner = (ElpgOwner *) JS_GetPrivate(pContext, pObject);
    if (!pElpgOwner)
    {
        JS_ReportError(pContext, "Bad ElpgOwner object");
        return JS_FALSE;
    }
    RC rc;
    C_CHECK_RC(pElpgOwner->ReleaseElpg());
    RETURN_RC(rc);
}

static STest ElpgOwner_ReleaseElpg
(
    ElpgOwner_Object,
    "ReleaseElpg",
    C_ElpgOwner_ReleaseElpg,
    1,
    "Make caller _mods_thread release Elpg resource"
);
