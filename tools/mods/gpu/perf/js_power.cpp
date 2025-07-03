/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/js_power.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/rc.h"
#include "gpu/perf/pwrsub.h"
#include "core/include/utility.h"

JsPower::JsPower()
    : m_pPower(nullptr), m_pJsPowerObj(nullptr)
{
}

static void C_JsPower_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPower * pJsPower;
    //! Delete the C++
    pJsPower = (JsPower *)JS_GetPrivate(cx, obj);
    delete pJsPower;
};

static JSClass JsPower_class =
{
    "JsPower",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsPower_finalize
};

static SObject JsPower_Object
(
    "JsPower",
    JsPower_class,
    0,
    0,
    "Power JS Object"
);

//! \brief Create a JS Object representation of the current associated
//! Power object
RC JsPower::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Power object
    if (m_pJsPowerObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsPower.\n");
        return OK;
    }

    m_pJsPowerObj = JS_DefineObject(cx,
                                    obj, // GpuSubdevice object
                                    "Power", // Property name
                                    &JsPower_class,
                                    JsPower_Object.GetJSObject(),
                                    JSPROP_READONLY);

    if (!m_pJsPowerObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsPower instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPowerObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPower.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPowerObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//! \brief Return the private JS data - C++ Power object.
//!
Power * JsPower::GetPowerObj()
{
    MASSERT(m_pPower);
    return m_pPower;
}

//! \brief Set the associated Power object.
//!
//! This is called by JS GpuSubdevice Initialize
void JsPower::SetPowerObj(Power * pPower)
{
    m_pPower = pPower;
}

#define JSPOWER_GETSETPROP(rettype, funcname,helpmsg)                                         \
P_( JsPower_Get##funcname );                                                                  \
P_( JsPower_Set##funcname );                                                                  \
static SProperty Get##funcname                                                               \
(                                                                                            \
    JsPower_Object,                                                                           \
    #funcname,                                                                               \
    0,                                                                                       \
    0,                                                                                       \
    JsPower_Get##funcname,                                                                    \
    JsPower_Set##funcname,                                                                    \
    0,                                                                                       \
    helpmsg                                                                                  \
);                                                                                           \
P_( JsPower_Set##funcname )                                                                   \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsPower *pJsPower;                                                                         \
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)                \
    {                                                                                        \
        RC rc;                                                                               \
        Power* pPower = pJsPower->GetPowerObj();                                                 \
        rettype RetVal;                                                                      \
        if (OK != pJavaScript->FromJsval(*pValue, &RetVal))                                  \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Set"#funcname"");                    \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        char msg[256];                                                                       \
        if ((rc = pPower->Set##funcname(RetVal))!= OK)                                        \
        {                                                                                    \
            sprintf(msg, "Error %d reading calling Set"#funcname"", (UINT32)rc);             \
            JS_ReportError(pContext, msg);                                                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        return JS_TRUE;                                                                      \
    }                                                                                        \
    return JS_FALSE;                                                                         \
}                                                                                            \
P_( JsPower_Get##funcname )                                                                   \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsPower *pJsPower;                                                                         \
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)                \
    {                                                                                        \
        RC rc;                                                                               \
        Power* pPower = pJsPower->GetPowerObj();                                                 \
        rettype RetVal;                                                                      \
        char msg[256];                                                                       \
        rc = pPower->Get##funcname(&RetVal);                                                  \
        if (RC::LWRM_NOT_SUPPORTED == rc)                                                    \
        {                                                                                    \
            RetVal = 0;                                                                      \
            rc.Clear();                                                                      \
        }                                                                                    \
        else if (rc != OK)                                                                   \
        {                                                                                    \
            sprintf(msg, "Error %d reading calling Get"#funcname"", (UINT32)rc);             \
            JS_ReportError(pContext, msg);                                                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        if (OK != pJavaScript->ToJsval(RetVal, pValue))                                      \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Get" #funcname"");                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        return JS_TRUE;                                                                      \
    }                                                                                        \
    return JS_FALSE;                                                                         \
}

P_(JsPower_Get_NumPowerSensors);
static SProperty NumPowerSensors
(
    JsPower_Object,
    "NumPowerSensors",
    0,
    0,
    JsPower_Get_NumPowerSensors,
    0,
    JSPROP_READONLY,
    "acquire number of power sensors on the board"
);
P_( JsPower_Get_NumPowerSensors)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Power* pPower = pJsPower->GetPowerObj();
        if (pJavaScript->ToJsval(pPower->GetNumPowerSensors(), pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_(JsPower_Get_PowerSensorMask);
static SProperty PowerSensorMask
(
    JsPower_Object,
    "PowerSensorMask",
    0,
    0,
    JsPower_Get_PowerSensorMask,
    0,
    JSPROP_READONLY,
    "acquire device masks for the power sensors"
);
P_( JsPower_Get_PowerSensorMask)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Power* pPower = pJsPower->GetPowerObj();
        if (pJavaScript->ToJsval(pPower->GetPowerSensorMask(), pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_(JsPower_Get_PollingPeriod);
static SProperty PollingPeriod
(
    JsPower_Object,
    "PollingPeriod",
    0,
    0,
    JsPower_Get_PollingPeriod,
    0,
    JSPROP_READONLY,
    "return the polling period for power capping evaluation"
);
P_(JsPower_Get_PollingPeriod)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Power* pPower = pJsPower->GetPowerObj();
        UINT16 periodms;
        if (pPower->GetPowerCapPollingPeriod(&periodms) != OK)
        {
            return JS_FALSE;
        }
        if (pJavaScript->ToJsval(periodms, pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

bool JsPower::GetIsClkUpScaleSupported()
{
    bool isSupported;
    if (m_pPower->IsClkUpScaleSupported(&isSupported) != OK)
    {
        return false;
    }
    return isSupported;
}
CLASS_PROP_READONLY_FULL(JsPower,
                         IsClkUpScaleSupported,
                         bool,
                         "query whether clkUpScale factor is supported by RM",
                         JSPROP_READONLY,
                         0);

JS_STEST_LWSTOM(JsPower, GetClkUpScaleFactor, 1,
                "Returns the clkUpScale factor used by RM power capping")
{
    STEST_HEADER(1, 1,
        "Usage: GpuSub.Power.GetClkUpScaleFactor(retVal);");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, JSObject*, pReturnArray);

    RC rc;
    Power *pPower = pJsPower->GetPowerObj();
    LwUFXP4_12 clkUpScaleFXP;
    C_CHECK_RC(pPower->GetClkUpScaleFactor(&clkUpScaleFXP));
    FLOAT64 clkUpScale = Utility::ColwertFXPToFloat(clkUpScaleFXP, 4, 12);

    jsval jsClkUpScale;
    C_CHECK_RC(pJavaScript->ToJsval(clkUpScale, &jsClkUpScale));
    RETURN_RC(pJavaScript->SetElement(pReturnArray, 0, jsClkUpScale));
}

JS_STEST_LWSTOM(JsPower, SetClkUpScaleFactor, 1,
                "Set the clkUpScale factor used by RM power-capping")
{
    STEST_HEADER(1, 1,
        "Usage: GpuSub.Power.SetClkUpScaleFactor(clkUpScale);");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, FLOAT64, clkUpScale);

    if (clkUpScale <= 0.0 || clkUpScale > 1.0)
    {
        RETURN_RC(RC::BAD_PARAMETER);
    }

    Power *pPower = pJsPower->GetPowerObj();

    LwUFXP4_12 clkUpScaleFXP = Utility::ColwertFloatToFXP(clkUpScale, 4, 12);
    RETURN_RC(pPower->SetClkUpScaleFactor(clkUpScaleFXP));
}

JS_STEST_LWSTOM(JsPower, GetPowerCapRange, 2,
                "Get min,rated,max values of a power-cap policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policyIdx = 0;
    JSObject * pReturnObj = 0;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &policyIdx, &pReturnObj))
    {
        JS_ReportError(pContext,
                       "Usage: subdev.Power.GetPowerCapRange(policyIdx, {rtn})\n");
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        UINT32 min, rated, max;
        Power* pPower = pJsPower->GetPowerObj();
        C_CHECK_RC(pPower->GetPowerCapRange((Power::PowerCapIdx)policyIdx,
                                      &min, &rated, &max));
        C_CHECK_RC(pJavaScript->PackFields(pReturnObj, "III",
                                           "Min", min,
                                           "Rated", rated,
                                           "Max", max));
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower, GetPowerCap, 2, "Get power limit of a policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policyIdx = 0;
    JSObject * pReturnCap = 0;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &policyIdx, &pReturnCap))
    {
        JS_ReportError(pContext, "Usage: subdev.Power.GetPowerCap(policyIdx, [rtn])\n");
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        UINT32 CapMw;
        Power* pPower = pJsPower->GetPowerObj();
        C_CHECK_RC(pPower->GetPowerCap((Power::PowerCapIdx)policyIdx, &CapMw));
        RETURN_RC(pJavaScript->SetElement(pReturnCap, 0, CapMw));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower, SetPowerCap, 1, "Set power limit of a policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policyIdx = 0;
    UINT32 limitMw = 0;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "II",
                                     &policyIdx, &limitMw))
    {
        JS_ReportError(pContext, "Usage: subdev.Power.SetPowerCap(policyIdx, LimitMw)\n");
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        Power* pPower = pJsPower->GetPowerObj();
        RETURN_RC(pPower->SetPowerCap((Power::PowerCapIdx)policyIdx, limitMw));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                PrintPowerInfo,
                1,
                "Show power-sensor and -monitor setup."
)
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    Printf(Tee::PriWarn,
           "PrintPowerInfo() is deprecated. Use PrintChannelsInfo() instead\n");

    JavaScriptPtr pJavaScript;
    UINT32 pri = Tee::PriNormal;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pri)))
    {
        JS_ReportError(pContext, "syntax: Power.PrintPowerInfo(Out.PriNormal)");
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        Power* pPower = pJsPower->GetPowerObj();
        MASSERT(pPower);
        RETURN_RC(pPower->PrintChannelsInfo(0U));
    }
    return JS_FALSE;
}

RC JsPower::PrintChannelsInfo(UINT32 mask)
{
    return m_pPower->PrintChannelsInfo(mask);
}
JS_STEST_BIND_ONE_ARG(JsPower, PrintChannelsInfo,
                      mask, UINT32,
                      "Print information about available channels")

JS_STEST_LWSTOM(JsPower,
                PrintPowerCapPolicyTable,
                1,
                "Show power-capping policy tables.")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 pri = Tee::PriNormal;

    if ((NumArguments > 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pri)))
    {
        JS_ReportError(pContext, "syntax: Power.PrintPowerCapPolicyTable(Out.PriNormal)");
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        Power* pPower = pJsPower->GetPowerObj();
        RETURN_RC(pPower->PrintPowerCapPolicyTable((Tee::Priority)pri));
    }
    return JS_FALSE;
}

// Clock value(s) returned are 2X clocks.
JS_STEST_LWSTOM(JsPower,
    GetLwrrPowerCapLimFreqkHz,
    1,
    "Get current power capping limited frequency."
)
{
    MASSERT((pContext != 0) || (pArguments != 0));

    STEST_HEADER(1, 1, "Usage: Power.GetLwrrPowerCapLimFreqkHz([powerCapLimits])");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, JSObject*, pReturnArray);

    RC rc = OK;
    Power *pPower = pJsPower->GetPowerObj();
    C_CHECK_RC(pPower->GetLwrrPowerCapLimFreqkHzToJs(pReturnArray));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPower, GetMaxPolicyCount, 1,
                "Return the value of LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES")
{
    STEST_HEADER(1, 1, "Usage: Power.GetMaxPolicyCount(outPolicyCount)");
    STEST_ARG(0, JSObject *, outputObj);
    RETURN_RC(pJavaScript->SetProperty(outputObj, "maxPowerPolicyCount",
                                       LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES));
}

JS_STEST_LWSTOM(JsPower, IsPowerBalancingSupported, 1,
                "Return true if PWM based power rail balancing is supported "
                "on this setup")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: Power.IsPowerBalancingSupported([outIsSupported])");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, JSObject *, jsArrIsSupported);
    Power* pPower = pJsPower->GetPowerObj();
    jsval jsIsSupported;
    C_CHECK_RC(pJavaScript->ToJsval(pPower->IsPowerBalancingSupported(),
                                    &jsIsSupported));
    RETURN_RC(pJavaScript->SetElement(jsArrIsSupported, 0, jsIsSupported));
}

JS_STEST_LWSTOM(JsPower, GetBalanceablePowerPolicyRelations, 1,
                "Retrieve the list of indices of valid PWR_POLICY_RELATIONSHIP"
                " entries")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: "
                       "Power.GetBalanceablePowerPolicyRelations([outRelations])");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, JSObject *, jsArrayObj);
    vector<UINT32> indices;
    Power* pPower = pJsPower->GetPowerObj();
    C_CHECK_RC(pPower->GetBalanceablePowerPolicyRelations(&indices));
    jsval temp;
    for (UINT32 i = 0; i < indices.size(); ++i)
    {
        C_CHECK_RC(pJavaScript->ToJsval(indices[i], &temp));
        C_CHECK_RC(pJavaScript->SetElement(jsArrayObj, i, temp));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPower, GetPowerBalancePwm, 2,
                "Get the current power balancing state on specified policy "
                "relationship index")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: Power.GetPowerBalancePwm(policyRelIdx, [outPct])");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, UINT32, policyRelIndex);
    STEST_ARG(1, JSObject *, pctJsArr);
    Power* pPower = pJsPower->GetPowerObj();
    FLOAT64 pct;
    jsval jsPct;
    C_CHECK_RC(pPower->GetPowerBalancePwm(policyRelIndex, &pct));
    C_CHECK_RC(pJavaScript->ToJsval(pct, &jsPct));
    RETURN_RC(pJavaScript->SetElement(pctJsArr, 0, jsPct));
}

JS_STEST_LWSTOM(JsPower, SetPowerBalancePwm, 2, "Set the power balance PWM state "
                                               "on specified policy relationship "
                                               "index")
{
    // pwmPct should be 0 - 100 NOT 0 - 1.0
    STEST_HEADER(2, 2, "Usage: Power.SetPowerBalancePwm(policyRelIdx, pwmPct)");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, UINT32, policyRelIndex);
    STEST_ARG(1, FLOAT64, pwmPct);
    Power* pPower = pJsPower->GetPowerObj();
    RETURN_RC(pPower->SetPowerBalancePwm(policyRelIndex, pwmPct));
}

JS_STEST_LWSTOM(JsPower, GetPowerScalingTgp1XEstimates, 3, "Get GPU power hint info"
                                                           "based on specified gpcClk"
                                                           "temperature and workloadItem")
{
    STEST_HEADER(3, 3, "Usage: Power.GetPowerScalingTgp1XEstimates(temperature, gpcClk, workloadItem)");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, FLOAT32, temp);
    STEST_ARG(1, UINT32, gpcClk);
    STEST_ARG(2, UINT32, workloadItem);
    Power* pPower = pJsPower->GetPowerObj();
    RETURN_RC(pPower->GetPowerScalingTgp1XEstimates(temp, gpcClk, workloadItem));
}

JS_STEST_LWSTOM(JsPower, ResetPowerBalance, 1, "Return control over PWM balancing "
                                              "of specified policy relation index "
                                              "to RM")
{
    STEST_HEADER(1, 1, "Usage: Power.ResetPowerBalance(policyRelIdx)");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, UINT32, policyRelIndex);
    Power* pPower = pJsPower->GetPowerObj();
    RETURN_RC(pPower->ResetPowerBalance(policyRelIndex));
}

JS_STEST_LWSTOM(JsPower,
                GetPowerChannelsFromPolicyRelationshipIndex, 2,
                "Given a power policy relationsip, get the two power channels "
                "that are balanced using that relationship")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: "
                 "Power.GetPowerChannelsFromPolicyRelationshipIndex(policyRelIdx, "
                                                                  "[outChannels])");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, UINT32, policyRelIndex);
    STEST_ARG(1, JSObject *, outChannels);
    Power* pPower = pJsPower->GetPowerObj();
    UINT32 primaryChannel;
    UINT32 secondaryChannel;
    UINT32 phaseEstimateChannel;
    C_CHECK_RC(pPower->GetPowerChannelsFromPolicyRelationshipIndex(policyRelIndex,
                                                                  &primaryChannel,
                                                                  &secondaryChannel,
                                                                  &phaseEstimateChannel));
    jsval jsPrimary;
    jsval jsSecondary;
    jsval jsPhase;
    C_CHECK_RC(pJavaScript->ToJsval(primaryChannel, &jsPrimary));
    C_CHECK_RC(pJavaScript->ToJsval(secondaryChannel, &jsSecondary));
    C_CHECK_RC(pJavaScript->SetElement(outChannels, 0, jsPrimary));
    C_CHECK_RC(pJavaScript->SetElement(outChannels, 1, jsSecondary));
    if (phaseEstimateChannel != LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID)
    {
        C_CHECK_RC(pJavaScript->ToJsval(phaseEstimateChannel, &jsPhase));
        C_CHECK_RC(pJavaScript->SetElement(outChannels, 2, jsPhase));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPower, PrintPowerPolicyDynamicStatus, 1,
                        "Print current status of power policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policymask = 0;

    if ((NumArguments != 1) ||
            (OK != pJavaScript->FromJsval(pArguments[0], &policymask)))
    {
        JS_ReportError(pContext,
                "Usage: subdev.Power.PrintPowerPolicyStatus(policyMask)\n");
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        C_CHECK_RC(pPower->PrintPoliciesStatus(policymask));
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

RC JsPower::PrintPoliciesStatus(UINT32 mask)
{
    return m_pPower->PrintPoliciesStatus(mask);
}
JS_STEST_BIND_ONE_ARG(JsPower, PrintPoliciesStatus,
                      mask, UINT32,
                      "Print dynamic power policy(s) information")

RC JsPower::PrintViolationsInfo(UINT32 mask)
{
    return m_pPower->PrintViolationsInfo(mask);
}
JS_STEST_BIND_ONE_ARG(JsPower, PrintViolationsInfo,
                      mask, UINT32,
                      "Print power violation policy(s) static information")

RC JsPower::PrintViolationsStatus(UINT32 mask)
{
    return m_pPower->PrintViolationsStatus(mask);
}
JS_STEST_BIND_ONE_ARG(JsPower, PrintViolationsStatus,
                      mask, UINT32,
                      "Print power violation policy(s) dynamic information")

RC JsPower::PrintPoliciesInfo(UINT32 mask)
{
    return m_pPower->PrintPoliciesInfo(mask);
}
JS_STEST_BIND_ONE_ARG(JsPower, PrintPoliciesInfo,
                      mask, UINT32,
                      "Print static power policy information")

JS_SMETHOD_LWSTOM(JsPower,
                  GetPowerSensorType,
                  1,
                  "Get the type of power sensor")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJs;
    UINT32 SensorMask;
    const char usage[] = "Usage: subdev.Power.GetPowerSensorType(SensorMask)\n";

    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &SensorMask)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        Power* pPower = pJsPower->GetPowerObj();
        if (pJs->ToJsval(pPower->GetPowerSensorType(SensorMask),
                         pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPower,
                  GetPowerRail,
                  1,
                  "Get the power rail of power channel")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJs;
    UINT32 ChannelMask;
    const char usage[] = "Usage: subdev.Power.GetPowerRail(ChannelMask)\n";

    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &ChannelMask)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        Power* pPower = pJsPower->GetPowerObj();
        UINT32 Rail = pPower->GetPowerRail(ChannelMask);
        if (pJs->ToJsval(pPower->PowerRailToStr(Rail), pReturlwalue) == RC::OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

// Deprecated interface
JS_SMETHOD_LWSTOM(JsPower,
                  GetPowerSensorPowerRail,
                  1,
                  "Get the power rail of power sensor")
{
    return C_JsPower_GetPowerRail(pContext, pObject, NumArguments,
                               pArguments, pReturlwalue);
}

JS_STEST_LWSTOM(JsPower,
                SetCallbacks,
                2,
                "Set function names of the callback functions")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Power.SetCallbacks(CallbackType, CallbackName)";

    JavaScriptPtr pJavaScript;
    UINT32 CallbackType;
    string CallbackName;
    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Is",
                                     &CallbackType, &CallbackName))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        C_CHECK_RC(pPower->SetUseCallbacks((Power::PowerCallback)CallbackType,
                                          CallbackName));
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

PROP_CONST(JsPower, BatteryPower,    LW2080_CTRL_PERF_POWER_SOURCE_BATTERY);
PROP_CONST(JsPower, ACPower,         LW2080_CTRL_PERF_POWER_SOURCE_AC);

JS_STEST_LWSTOM(JsPower,
                SetPowerSource,
                1,
                "Attempt to set the power source")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: subdev.Power.SetPowerSource(PowerSource)";

    if(NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 PowerSource;

    if (OK != pJavaScript->FromJsval(pArguments[0], &PowerSource))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        rc = pPower->SetPowerSupplyType(PowerSource);
        if (RC::LWRM_NOT_SUPPORTED == rc)
        {
            rc.Clear();
        }
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                SetLwstPowerRailVoltage,
                1,
                "Attempt to set power rail voltage using user defined function")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: subdev.Power.SetLwstPowerRailVoltage(Value)";

    JavaScriptPtr pJavaScript;
    UINT32 Voltage;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Voltage)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        C_CHECK_RC(pPower->SetLwstPowerRailVoltage(Voltage));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                RegisterLwstPowerRailLeakageCheck,
                2,
                "RegisterLwstPowerRailLeakageCheck")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Power.RegisterLwstPowerRailLeakageCheck()";

    JavaScriptPtr pJavaScript;
    UINT32 Min;
    UINT32 Max;
    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "II",
                                     &Min, &Max))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        C_CHECK_RC(pPower->RegisterLwstPowerRailLeakageCheck(Min, Max));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                GetPowerSensorReading,
                2,
                "GetPowerSensorReading")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Power.GetPowerSensorReading(SensorMask, OutArray)";

    JavaScriptPtr pJS;
    JSObject * pRetArray;
    UINT32 SensorMask;

    if (2 != pJS->UnpackArgs(pArguments, NumArguments, "Io",
                             &SensorMask, &pRetArray))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        Power::PowerSensorReading Reading;
        C_CHECK_RC(pPower->GetPowerSensorReading(SensorMask, &Reading));
        C_CHECK_RC(pJS->SetElement(pRetArray, Power::VOLTAGE_IDX, Reading.VoltageMv));
        C_CHECK_RC(pJS->SetElement(pRetArray, Power::POWER_IDX, Reading.PowerMw));
        C_CHECK_RC(pJS->SetElement(pRetArray, Power::LWRRENT_IDX, Reading.LwrrentMa));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

// Because JS_NewObject with a NULL class* is unsafe in JS 1.4.
JS_CLASS(PowerSensorSample);

JS_STEST_LWSTOM(JsPower,
                GetPowerSensorSample,
                1,
                "Read all power sensors & total gpu power.")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Power.GetPowerSensorSample(RtnObj)";

    JavaScriptPtr pJS;
    JSObject * pRetObj;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pRetObj)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS Samp;
        memset(&Samp, 0, sizeof(LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS));
        C_CHECK_RC(pPower->GetPowerSensorSample(&Samp));
        C_CHECK_RC(pJS->SetProperty(pRetObj, "totalGpuPowermW",
                                    static_cast<UINT32>(Samp.totalGpuPowermW)));

        JsArray chans;
        UINT32 SensorMask = Samp.super.objMask.super.pData[0];
        for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; i++)
        {
            UINT32 m = (1<<i);
            if (m > SensorMask)
                break;
            if (0 == (SensorMask & m))
            {
                chans.push_back(JSVAL_NULL);  // placeholder to avoid gaps in array
                continue;
            }

            JSObject * pChan = JS_NewObject(pContext, &PowerSensorSampleClass,
                                            NULL, NULL);
            C_CHECK_RC(pJS->PackFields(pChan, "IIIII",
                                       "pwrAvgmW", Samp.channelStatus[i].tuplePolled.pwrmW,
                                       "pwrMinmW", Samp.channelStatus[i].tuplePolled.pwrmW,
                                       "pwrMaxmW", Samp.channelStatus[i].tuplePolled.pwrmW,
                                       "voltuV",   Samp.channelStatus[i].tuplePolled.voltuV,
                                       "lwrrmA",   Samp.channelStatus[i].tuplePolled.lwrrmA));
            jsval tmp;
            C_CHECK_RC(pJS->ToJsval(pChan, &tmp));
            chans.push_back(tmp);
        }
        jsval tmp;
        C_CHECK_RC(pJS->ToJsval(&chans, &tmp));
        C_CHECK_RC(pJS->SetPropertyJsval(pRetObj, "channels", tmp));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                GetMaxPowerSensorReading,
                2,
                "Get the maximum power reading on the specified sensor")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] =
        "Usage: subdev.Power.GetMaxPowerSensorReading(SensorMask, RtnObj)";

    JavaScriptPtr pJS;
    UINT32        sensorMask;
    JSObject *    pRetPowerMw;

    if (2 != pJS->UnpackArgs(pArguments, NumArguments, "Io",
                             &sensorMask, &pRetPowerMw))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        UINT32 pwrMw = 0;

        C_CHECK_RC(pPower->GetMaxPowerSensorReading(sensorMask, &pwrMw));
        RETURN_RC(pJS->SetElement(pRetPowerMw, 0, pwrMw));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                ResetMaxPowerSensorReading,
                1,
                "Reset the maximum power reading on the specified sensor (0 = reset all)")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] =
        "Usage: subdev.Power.ResetMaxPowerSensorReading(SensorMask)";

    JavaScriptPtr pJS;
    UINT32        sensorMask;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &sensorMask)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        Power* pPower = pJsPower->GetPowerObj();
        pPower->ResetMaxPowerSensorReading(sensorMask);
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                InitPowerRanges,
                1,
                "InitPowerRanges")
{
    MASSERT(pContext && pArguments);
    JavaScriptPtr pJS;

    STEST_HEADER(1, 1, "Usage: subdev.Power.InitPowerRanges(SensorRangeArray)");
    STEST_ARG(0, JsArray, JsSensorArray);

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        vector<PowerRangeChecker::PowerRange> SensorRanges;
        for (UINT32 i = 0; i < JsSensorArray.size(); i++)
        {
            JSObject *OneEntry;
            C_CHECK_RC(pJS->FromJsval(JsSensorArray[i], &OneEntry));
            PowerRangeChecker::PowerRange NewRange;
            C_CHECK_RC(pJS->UnpackFields(OneEntry, "III",
                                         "SensorMask", &NewRange.SensorMask,
                                         "MinMw", &NewRange.MinMw,
                                         "MaxMw", &NewRange.MaxMw));
            SensorRanges.push_back(NewRange);
        }
        RETURN_RC(pPower->InitPowerRangeChecker(SensorRanges));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                InitRunningAvgs,
                1,
                "InitRunningAvgs")
{
    MASSERT(pContext && pArguments);
    JavaScriptPtr pJS;

    STEST_HEADER(1, 1, "Usage: subdev.Power.InitRunningAvgs(SensorArray)");
    STEST_ARG(0, JsArray, JsSensorArray);

    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) != 0)
    {
        RC rc;
        Power* pPower = pJsPower->GetPowerObj();
        for (UINT32 i = 0; i < JsSensorArray.size(); i++)
        {
            JSObject *OneEntry;
            UINT32    sensorMask;
            UINT32    runningAvgSize;
            C_CHECK_RC(pJS->FromJsval(JsSensorArray[i], &OneEntry));
            C_CHECK_RC(pJS->UnpackFields(OneEntry, "II",
                                         "SensorMask", &sensorMask,
                                         "RunningAvg", &runningAvgSize));
            C_CHECK_RC(pPower->InitRunningAvg(sensorMask, runningAvgSize));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPower,
                GetPowerEquationTable,
                1,
                "Get Power Equation info")
{
    MASSERT((pContext !=0) || (pArguments !=0));
    const char usage[] = "Usage: \n"
                     "var subdev = new GpuSubdevice(0,0);\n"
                     "var obj = new Object;\n"
                     "rc = subdev.Power.GetPowerEquationTable(obj)\n"
                     "--- Properties in each object:----\n"
                     "hwIddqVersion: fuse value read from GPU\n"
                     "iddqVersion: IDDQ version from the vbios\n"
                     "iddqmA: IDDQ value to use for callwlating leakage power values\n"
                     "equationMask: Mask of PWR_EQUATION entries specified on this GPU\n"
                     "PwrEquEntries: Table of PWR_EQUATION objects\n"
                     "   type\n"
                     "   k0 - for LEAKAGE_DTCS11\n"
                     "   k1 - for LEAKAGE_DTCS11\n"
                     "   k2 - for LEAKAGE_DTCS11\n"
                     "   k3 - for LEAKAGE_DTCS11\n"
                     "   fsEff - for LEAKAGE_DTCS11\n"
                     "   pgEff - for LEAKAGE_DTCS11\n"
                     "   refVoltageuV - for BA1X_SCALE\n"
                     "   ba2mW - for BA1X_SCALE\n"
                     "   gpcClkMHz - for BA1X_SCALE\n"
                     "   utilsClkMHz - for BA1X_SCALE\n"
                     "   expBASlope - for BA00_FIT\n"
                     "   expBAIntercept - for BA00_FIT\n"
                     "   expDynPwrSlope - for BA00_FIT\n"
                     "   expDynPwrIntercept - for BA00_FIT\n";

    JavaScriptPtr pJs;
    JSObject * pReturnObj = 0;
    if((NumArguments != 1) || (OK != pJs->FromJsval(pArguments[0], &pReturnObj)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsPower *pJsPower;
    if ((pJsPower = JS_GET_PRIVATE(JsPower, pContext, pObject, "JsPower")) == 0)
        return JS_FALSE;

    Power* pPower = pJsPower->GetPowerObj();
    C_CHECK_RC(pPower->GetPowerEquationInfoToJs(pContext, pReturnObj));
    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPower,
                  IsAdcPwrDevice,
                  1,
                  "Returns true if the device type corresponds to ISENSE ADC")
{
    STEST_HEADER(1, 1, "Usage: isAdc = GpuSub.Power.IsAdcPwrDevice(devType)");
    STEST_PRIVATE(JsPower, pJsPower, "JsPower");
    STEST_ARG(0, UINT32, devType);
    Power* pPower = pJsPower->GetPowerObj();
    RC rc = pJavaScript->ToJsval(pPower->IsAdcPwrDevice(devType), pReturlwalue);
    if (rc != RC::OK)
    {
        pJavaScript->Throw(pContext, rc, "Error in Power.IsAdcPwrDevice");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//! Create a JS class to declare Power constants on
JS_CLASS(PowerConst);

//! \brief ParamConst javascript interface object
//!
//! ParamConst javascript interface
//!
static SObject PowerConst_Object
(
    "PowerConst",
    PowerConstClass,
    0,
    0,
    "PowerConst JS Object"
);

PROP_CONST(PowerConst, SET_POWER_RAIL_VOLTAGE, Power::SET_POWER_RAIL_VOLTAGE);
PROP_CONST(PowerConst, CHECK_POWER_RAIL_LEAKAGE, Power::CHECK_POWER_RAIL_LEAKAGE);
PROP_CONST(PowerConst, VOLTAGE_IDX, Power::VOLTAGE_IDX);
PROP_CONST(PowerConst, POWER_IDX, Power::POWER_IDX);
PROP_CONST(PowerConst, LWRRENT_IDX, Power::LWRRENT_IDX);
PROP_CONST(PowerConst, RTP, Power::RTP);
PROP_CONST(PowerConst, TGP, Power::TGP);
PROP_CONST(PowerConst, DROOPY, Power::DROOPY);

PROP_CONST(PowerConst, TYPE_GPUADC,  LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10);
PROP_CONST(PowerConst, TYPE_GPUADC1X,  LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC1X);
PROP_CONST(PowerConst, TYPE_GPUADC10,  LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10);
PROP_CONST(PowerConst, TYPE_GPUADC11,  LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11);
PROP_CONST(PowerConst, TYPE_INA3221, LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221);
