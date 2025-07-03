
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Headers from outside mods
#include "ctrl/ctrl2080.h"
#include "jsapi.h"

#include "gpu/perf/js_volt.h"

#include "gpu/perf/perfutil.h"
#include "gpu/perf/voltsub.h"
#include "core/include/jscript.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/utility.h"

JsVolt3x::JsVolt3x() :
     m_pVolt3x(nullptr)
    ,m_pJsVolt3xObject(nullptr)
{
}

static void C_JsVolt3x_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsVolt3x *pJsVolt3x;
    pJsVolt3x = (JsVolt3x *)JS_GetPrivate(cx, obj);
    delete pJsVolt3x;
}

static JSClass JsVolt3x_class =
{
    "JsVolt3x",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsVolt3x_finalize
};

static SObject JsVolt3x_Object
(
    "JsVolt3x",
    JsVolt3x_class,
    0,
    0,
    "Volt3x JS object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! Volt3x object
RC JsVolt3x::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Volt3x object
    if (m_pJsVolt3xObject)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsVolt3x.\n");
        return OK;
    }

    m_pJsVolt3xObject = JS_DefineObject(cx,
                                        obj, // GpuSubdevice object
                                        "Volt3x", // Property name
                                        &JsVolt3x_class,
                                        JsVolt3x_Object.GetJSObject(),
                                        JSPROP_READONLY);

    if (!m_pJsVolt3xObject)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsVolt3x instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsVolt3xObject, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsVolt3x.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsVolt3xObject, "Help", &C_Global_Help, 1, 0);

    return OK;
}

Volt3x *JsVolt3x::GetVolt3x()
{
    MASSERT(m_pVolt3x);
    return m_pVolt3x;
}

void JsVolt3x::SetVolt3x(Volt3x *pVolt3x)
{
    m_pVolt3x = pVolt3x;
}

RC JsVolt3x::Initialize()
{
    return GetVolt3x()->Initialize();
}

JS_STEST_BIND_NO_ARGS(JsVolt3x, Initialize, "Initialize the mods Volt3x class");

RC JsVolt3x::VoltRailStatusToJsObject
(
    const LW2080_CTRL_VOLT_VOLT_RAIL_STATUS &railStatus,
    JSObject *pJsStatus
)
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->PackFields(
        pJsStatus, "BIIIIIIIIIIIIIB",
        "type", railStatus.super.type,
        "lwrrVoltDefaultuV", railStatus.lwrrVoltDefaultuV,
        "relLimituV", railStatus.relLimituV,
        "altRelLimituV", railStatus.altRelLimituV,
        "ovLimituV", railStatus.ovLimituV,
        "maxLimituV", railStatus.maxLimituV,
        "vminLimituV", railStatus.vminLimituV,
        "voltMarginLimituV", railStatus.voltMarginLimituV,
        "voltMinNoiseUnawareuV", railStatus.voltMinNoiseUnawareuV,
        "lwrrVoltSenseduV", railStatus.lwrrVoltSenseduV,
        "ramAssistVCritLowuV", railStatus.ramAssist.vCritLowuV,
        "ramAssistVCritHighuV", railStatus.ramAssist.vCritHighuV,
        "ramAssistEngagedMask", railStatus.ramAssist.engagedMask,
        "ramAssistenabled", railStatus.ramAssist.bEnabled,
        "railAction", railStatus.railAction);
}

JS_CLASS(RmVoltPolicyStatus);

RC JsVolt3x::VoltPolicyStatusToJsObject
(
    UINT32 policyIdx,
    const LW2080_CTRL_VOLT_VOLT_POLICY_STATUS& status,
    JSObject* pJsStatus
)
{
    RC rc;
    JavaScriptPtr pJavaScript;

    switch (status.type)
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            return pJavaScript->PackFields(
                pJsStatus, "IIiiii?",
                "policyIndex", policyIdx,
                "policyType", status.type,
                "origDeltaMinuV", status.data.splitRailMS.super.origDeltaMinuV,
                "origDeltaMaxuV", status.data.splitRailMS.super.origDeltaMaxuV,
                "deltaMinuV", status.data.splitRailMS.super.deltaMinuV,
                "deltaMaxuV", status.data.splitRailMS.super.deltaMaxuV,
                "violation", status.data.splitRailMS.super.bViolation);
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            return pJavaScript->PackFields(
                pJsStatus, "IIiiiiii?",
                "policyIndex", policyIdx,
                "policyType", status.type,
                "masterRail", status.data.splitRailSS.super.lwrrVoltMasteruV,
                "slaveRail", status.data.splitRailSS.super.lwrrVoltSlaveuV,
                "origDeltaMinuV", status.data.splitRailSS.super.origDeltaMinuV,
                "origDeltaMaxuV", status.data.splitRailSS.super.origDeltaMaxuV,
                "deltaMinuV", status.data.splitRailSS.super.deltaMinuV,
                "deltaMaxuV", status.data.splitRailSS.super.deltaMaxuV,
                "violation", status.data.splitRailSS.super.bViolation);
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            return pJavaScript->PackFields(
                pJsStatus, "III",
                "policyIndex", policyIdx,
                "policyType", status.type,
                "lwrrVoltuV", status.data.singleRail.lwrrVoltuV);
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            return pJavaScript->PackFields(
                pJsStatus, "III",
                "policyIndex", policyIdx,
                "policyType", status.type,
                "lwrrVoltuV", status.data.singleRailMS.super.lwrrVoltuV);
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            JSObject* pJsRailItems = 0;
            for (UINT32 ii = 0; ii < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; ii++)
            {
                JSObject* pJsRailItem = 0;
                CHECK_RC(pJavaScript->SetProperty(pJsRailItem, "railIdx",
                                                  status.data.multiRail.railItems[ii].railIdx));
                CHECK_RC(pJavaScript->SetProperty(pJsRailItem, "lwrrVoltuV",
                                                  status.data.multiRail.railItems[ii].lwrrVoltuV));
                CHECK_RC(pJavaScript->SetElement(pJsRailItems, (INT32)ii, pJsRailItem));
            }

            CHECK_RC(pJavaScript->SetProperty(pJsStatus, "policyIndex", policyIdx));
            CHECK_RC(pJavaScript->SetProperty(pJsStatus, "policyType", status.type));
            CHECK_RC(pJavaScript->SetProperty(pJsStatus, "numRails",
                                              status.data.multiRail.numRails));
            CHECK_RC(pJavaScript->SetProperty(pJsStatus, "railItems", pJsRailItems));
            break;
        }
        default:
            Printf(Tee::PriError, "Invalid voltage policy type (%u)\n", status.type);
            return RC::BAD_PARAMETER;
    }
    return rc;
}

JS_STEST_LWSTOM(JsVolt3x,
                GetVoltPoliciesStatus,
                1,
                "Get the status of all RM voltage policies")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: GpuSub.Volt3x.GetVoltPoliciesStatus([statuses])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject*, outPoliciesStatus);
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();

    map<UINT32, LW2080_CTRL_VOLT_VOLT_POLICY_STATUS> statuses;
    C_CHECK_RC(pVolt3x->GetVoltPoliciesStatus(statuses));

    UINT32 ii = 0;
    for (map<UINT32, LW2080_CTRL_VOLT_VOLT_POLICY_STATUS>::const_iterator
         policyItr = statuses.begin();
         policyItr != statuses.end(); ++policyItr)
    {
        JSObject* pJsObjVoltStatus = JS_NewObject(pContext, &RmVoltPolicyStatusClass,
                                                  nullptr, nullptr);
        C_CHECK_RC(JsVolt3x::VoltPolicyStatusToJsObject(policyItr->first,
                    policyItr->second, pJsObjVoltStatus));
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pJsObjVoltStatus, &tmp));
        C_CHECK_RC(pJavaScript->SetElement(outPoliciesStatus, ii++, tmp));
    }
    RETURN_RC(rc);
}

JS_CLASS(VoltRailStatus);

JS_STEST_LWSTOM(JsVolt3x,
                GetVoltRailStatus,
                1,
                "Retrieve the status information from all voltage rails on the chip")
{
    RC rc;
    UINT32 i = 0;
    STEST_HEADER(1, 1, "Usage: Volt3x.GetVoltRailStatus([outAllRailStatuses])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject *, outAllRailStatuses);
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS railStatusParams = {};
    C_CHECK_RC(pVolt3x->GetVoltRailStatus(&railStatusParams));
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &railStatusParams.super.objMask.super)
        LW2080_CTRL_VOLT_VOLT_RAIL_STATUS &railStatus = railStatusParams.rails[i];
        JSObject *pStatus = JS_NewObject(pContext, &VoltRailStatusClass, nullptr, nullptr);
        C_CHECK_RC(JsVolt3x::VoltRailStatusToJsObject(railStatus, pStatus));
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pStatus, &tmp));
        C_CHECK_RC(pJavaScript->SetElement(outAllRailStatuses, (INT32)i, tmp));
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsVolt3x,
                GetVoltRailStatusViaId,
                2,
                "Retrieve volt rail status for the given voltage domain")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: Volt3x.GetVoltRailStatusViaId(Gpu.VOLTAGE_*, {voltStatus})");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, UINT32, voltId);
    STEST_ARG(1, JSObject *, outVoltRailStatus);
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = {};
    Gpu::SplitVoltageDomain voltDom = pVolt3x->RailIdxToGpuSplitVoltageDomain(voltId);
    C_CHECK_RC(pVolt3x->GetVoltRailStatusViaId(voltDom, &railStatus));
    RETURN_RC(JsVolt3x::VoltRailStatusToJsObject(railStatus, outVoltRailStatus));
}

JS_STEST_LWSTOM(JsVolt3x,
                GetVoltageMv,
                2,
                "Retrieve the voltage reading on the specified rail")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: Volt3x.GetVoltageMv(RailIdx, [outVoltage_mV]");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, UINT32, voltId);
    STEST_ARG(1, JSObject *, outVoltMv);
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    UINT32 voltMv;
    Gpu::SplitVoltageDomain voltDom = pVolt3x->RailIdxToGpuSplitVoltageDomain(voltId);
    C_CHECK_RC(pVolt3x->GetVoltageMv(voltDom, &voltMv));
    RETURN_RC(pJavaScript->SetElement(outVoltMv, 0, voltMv));
}

RC JsVolt3x::SetVoltageMv(UINT32 railIdx, UINT32 voltMv)
{
    vector<Volt3x::VoltageSetting> voltages;
    Gpu::SplitVoltageDomain voltDom = GetVolt3x()->RailIdxToGpuSplitVoltageDomain(railIdx);
    voltages.push_back(Volt3x::VoltageSetting(voltDom, voltMv * 1000));
    return GetVolt3x()->SetVoltagesuV(voltages);
}

JS_STEST_BIND_TWO_ARGS(JsVolt3x, SetVoltageMv, voltDom, UINT32, voltMv, UINT32,
                       "Set the voltage (in mV) on the specified voltage rail");

JS_STEST_LWSTOM(JsVolt3x,
                SetVoltagesMv,
                1,
                "Set the voltages (in mV) on one or more voltage rails")
{
    RC rc;
    STEST_HEADER(1, 1,
                 "Usage: Volt3x.SetVoltagesMv(voltages)\n"
                 " voltages - an array of objects of the form\n"
                 "            { railIdx: value, voltMv: value }");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JsArray, jsVoltagesArray);

    vector<Volt3x::VoltageSetting> voltageSettings;
    for (UINT32 ii = 0; ii < jsVoltagesArray.size(); ii++)
    {
        JSObject* jsVoltSetting;
        C_CHECK_RC(pJavaScript->FromJsval(jsVoltagesArray[ii], &jsVoltSetting));
        Volt3x::VoltageSetting voltageSetting;
        Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
        UINT32 railIdx;
        FLOAT32 voltMv;
        C_CHECK_RC(pJavaScript->UnpackFields(jsVoltSetting, "If",
                    "railIdx", &railIdx,
                    "voltMv", &voltMv));
        voltageSetting.voltuV = static_cast<UINT32>(voltMv * 1000);
        voltageSetting.voltDomain = pVolt3x->RailIdxToGpuSplitVoltageDomain(railIdx);
        voltageSettings.push_back(voltageSetting);
    }

    RETURN_RC(pJsVolt3x->GetVolt3x()->SetVoltagesuV(voltageSettings));
}

JS_STEST_LWSTOM(JsVolt3x,
    GetLwrrOverVoltLimitsuV,
    1,
    "Get current over-volt limits.")
{
    MASSERT((pContext != 0) || (pArguments != 0));

    STEST_HEADER(1, 1,
        "Usage: Volt3x.GetLwrrOverVoltLimitsuV([overVoltLimits])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject*, pReturnObj);

    RC rc = OK;
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    C_CHECK_RC(pVolt3x->GetLwrrOverVoltLimitsuVToJs(pReturnObj));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsVolt3x,
    GetLwrrVoltRelLimitsuV,
    1,
    "Get current voltage reliability limits.")
{
    MASSERT((pContext != 0) || (pArguments != 0));

    STEST_HEADER(1, 1,
        "Usage: subdev.Perf.GetLwrrVoltRelLimitsuV([voltRelLimits])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject*, pReturnObj);

    RC rc = OK;
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    C_CHECK_RC(pVolt3x->GetLwrrVoltRelLimitsuVToJs(pReturnObj));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsVolt3x,
    GetLwrrAltVoltRelLimitsuV,
    1,
    "Get current Alternate voltage reliability limits.")
{
    MASSERT((pContext != 0) || (pArguments != 0));

    STEST_HEADER(1, 1,
        "Usage: subdev.Perf.GetLwrrAltVoltRelLimitsuV([altVoltRelLimits])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject*, pReturnObj);

    RC rc = OK;
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    C_CHECK_RC(pVolt3x->GetLwrrAltVoltRelLimitsuVToJs(pReturnObj));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsVolt3x,
    GetLwrrVminLimitsuV,
    1,
    "Get current Vmin limits.")
{
    MASSERT((pContext != 0) || (pArguments != 0));

    STEST_HEADER(1, 1,
        "Usage: Volt3x.GetLwrrVminLimitsuV([vminLimits])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject*, pReturnObj);

    RC rc = OK;
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    C_CHECK_RC(pVolt3x->GetLwrrVminLimitsuVToJs(pReturnObj));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsVolt3x,
                GetVoltRailRegulatorStepSizeuV,
                2,
                "Get a rail's voltage regulator step-size in uV")
{
    STEST_HEADER(2, 2,
        "Usage: Volt3x.GetVoltRailRegulatorStepSizeuV(Gpu.VOLTAGE_*, outStepSizeuV)");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, INT32, railIdx);
    STEST_ARG(1, JSObject*, pReturnObj);

    RC rc;
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    LwU32 stepSizeuV = 0;
    Gpu::SplitVoltageDomain voltDom = pVolt3x->RailIdxToGpuSplitVoltageDomain(railIdx);
    C_CHECK_RC(pVolt3x->GetVoltRailRegulatorStepSizeuV(
        voltDom,
        &stepSizeuV));
    RETURN_RC(pJavaScript->SetElement(pReturnObj, 0, stepSizeuV));
}

// Return the VFE Index for the static RAM Assist or SW Dynamic RAM Assist without LUT 
JS_STEST_LWSTOM(JsVolt3x,
    GetRamAssistPerRailType,
    1,
    "Get current voltage reliability limits.")
{
    MASSERT((pContext != 0) || (pArguments != 0));

    STEST_HEADER(1, 1,
        "Usage: Volt3x.GetRamAssistInfo([RamAssistJsObj])");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, JSObject*, pJsramAssistInfos);

    RC rc;
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    map<Gpu::SplitVoltageDomain, Volt3x::RamAssistType> ramAssistTypes;
    C_CHECK_RC(pVolt3x->GetRamAssistInfos(&ramAssistTypes));

    for (auto ramType:ramAssistTypes)
    {
        CHECK_RC(pJavaScript->SetProperty(pJsramAssistInfos, PerfUtil::SplitVoltDomainToStr(ramType.first),
            PerfUtil::RamAssistTypeToStr(ramType.second)));
    }

    RETURN_RC(rc);
}

RC JsVolt3x::ClearSetVoltage(UINT32 voltDom)
{
    return GetVolt3x()->ClearSetVoltage(static_cast<Gpu::SplitVoltageDomain>(voltDom));
}
JS_STEST_BIND_ONE_ARG(JsVolt3x, ClearSetVoltage, voltDom, UINT32,
                      "Release limit on voltage for the specified rail. "
                      "Effectively undoes SetVoltageMv.");

RC JsVolt3x::ClearVoltages()
{
    return GetVolt3x()->ClearAllSetVoltages();
}
JS_STEST_BIND_NO_ARGS(JsVolt3x, ClearVoltages, "Clear all voltages set by MODS at once");

RC JsVolt3x::SetRamAssistOffsetuV
(
    UINT32 railIdx,
    INT32 vCritLowoffsetuV,
    INT32 vCritHighoffsetuV
)
{
    Gpu::SplitVoltageDomain voltDomTemp = GetVolt3x()->RailIdxToGpuSplitVoltageDomain(railIdx);
    map<Volt3x::VoltageLimit, INT32> voltOffsetuV;
    if (vCritLowoffsetuV != 0)
        voltOffsetuV[Volt3x::VoltageLimit::VCRIT_LOW_LIMIT] = vCritLowoffsetuV;
    if (vCritHighoffsetuV != 0)
        voltOffsetuV[Volt3x::VoltageLimit::VCRIT_HIGH_LIMIT] = vCritHighoffsetuV;
    return GetVolt3x()->SetRailLimitsOffsetuV(
        voltDomTemp,
        voltOffsetuV);
}
JS_STEST_BIND_THREE_ARGS(JsVolt3x, SetRamAssistOffsetuV,
                         railIdx, UINT32,
                         vCritLowoffsetuV, INT32,
                         vCritHighoffsetuV, INT32,
                         "Offset RAM Assist limits on a voltage rail");

RC JsVolt3x::SetRailLimitOffsetuV
(
    UINT32 voltDom,
    UINT32 voltLimit,
    INT32 offsetuV
)
{
    Gpu::SplitVoltageDomain voltDomTemp = GetVolt3x()->RailIdxToGpuSplitVoltageDomain(voltDom);
    return GetVolt3x()->SetRailLimitOffsetuV(
        voltDomTemp,
        static_cast<Volt3x::VoltageLimit>(voltLimit),
        offsetuV);
}
JS_STEST_BIND_THREE_ARGS(JsVolt3x, SetRailLimitOffsetuV,
                         voltDom, UINT32,
                         voltLimit, UINT32,
                         offsetuV, INT32,
                         "Offset a limit on a voltage rail");

JS_STEST_LWSTOM(JsVolt3x,
                GetRailLimitOffsetuV,
                3,
                "Get the offset to a limit on a voltage rail")
{
    STEST_HEADER(3, 3, "Usage: Volt3x.GetRailLimitOffsetuV(rail, limit, offsetuV)");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, UINT32, voltRail);
    STEST_ARG(1, UINT32, voltLimit);
    STEST_ARG(2, JSObject*, pRetVal);

    if (voltLimit >= Volt3x::NUM_VOLTAGE_LIMITS)
        RETURN_RC(RC::BAD_PARAMETER);

    RC rc;
    Volt3x* pVolt3x = pJsVolt3x->GetVolt3x();
    map<Gpu::SplitVoltageDomain, vector<INT32> > limitsuV;
    C_CHECK_RC(pVolt3x->GetRailLimitOffsetsuV(&limitsuV));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0,
        limitsuV[static_cast<Gpu::SplitVoltageDomain>(voltRail)][voltLimit]));
}

RC JsVolt3x::ClearRailLimitOffsets()
{
    return GetVolt3x()->ClearRailLimitOffsets();
}
JS_STEST_BIND_NO_ARGS(JsVolt3x, ClearRailLimitOffsets,
                      "Clear all voltage limit offsets");

RC JsVolt3x::SetInterVoltSettleTimeUs(UINT16 timeUs)
{
    return GetVolt3x()->SetInterVoltSettleTimeUs(timeUs);
}
JS_STEST_BIND_ONE_ARG(JsVolt3x, SetInterVoltSettleTimeUs, timeUs, UINT16,
                      "Set the intermediate voltage switch settle time "
                      "for split-rail voltage switch sequence");

JS_STEST_LWSTOM(JsVolt3x,
                SetSplitRailConstraintOffsetuV,
                2,
                "Set an offset to the split rail constraint min/max")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: Volt3x.SetSplitRailConstraintOffsetuV(minMaxStr, offset)");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, string, minMaxStr);
    STEST_ARG(1, INT32, offsetuV);
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    C_CHECK_RC(pVolt3x->SetSplitRailConstraintOffsetuV(minMaxStr == "max", offsetuV));
    RETURN_RC(rc);
}

bool JsVolt3x::GetSplitRailConstraintAuto()
{
    return GetVolt3x()->GetSplitRailConstraintAuto();
}

RC JsVolt3x::SetSplitRailConstraintAuto(bool autoEnable)
{
    return GetVolt3x()->SetSplitRailConstraintAuto(autoEnable);
}

CLASS_PROP_READWRITE_FULL(JsVolt3x, SplitRailConstraintAuto, bool,
                          "Whether to handle split-rail voltage constraints automatically",
                          0, false);

UINT32 JsVolt3x::GetVoltRailMask()
{
    return GetVolt3x()->GetVoltRailMask();
}

CLASS_PROP_READONLY(JsVolt3x, VoltRailMask, UINT32,
                    "Bitmask of available voltage rails.");

JS_STEST_LWSTOM(JsVolt3x, GetRailLimitVfeEquIdx, 3,
                "returns the VFE equation index for a voltage rail limit")
{
    STEST_HEADER(3, 3, "Usage: GpuSub.Volt3x.GetRailLimitVfeEquIdx(rail, lim, equ)");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, UINT32, rail);
    STEST_ARG(1, UINT32, limit);
    STEST_ARG(2, JSObject*, pRetVal);
    Volt3x *pVolt3x = pJsVolt3x->GetVolt3x();
    RC rc;
    UINT32 vfeEquIdx;
    C_CHECK_RC(pVolt3x->GetRailLimitVfeEquIdx(
        static_cast<Gpu::SplitVoltageDomain>(rail),
        static_cast<Volt3x::VoltageLimit>(limit),
        &vfeEquIdx));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, vfeEquIdx));
}

bool JsVolt3x::GetIsPascalSplitRailSupported()
{
    return GetVolt3x()->IsPascalSplitRailSupported();
}

CLASS_PROP_READONLY_FULL(JsVolt3x, IsPascalSplitRailSupported, bool,
                         "Whether split rail is supported.", 0, false);

bool JsVolt3x::GetIsRamAssistSupported()
{
    return GetVolt3x()->IsRamAssistSupported();
}

CLASS_PROP_READONLY_FULL(JsVolt3x, IsRamAssistSupported, bool,
                         "Whether RAM Assist is supported.", 0, false);

JS_SMETHOD_LWSTOM(JsVolt3x, DomainToRailIdx, 1,
    "Colwerts a Gpu.VOLTAGE enum value to the corresponding VBIOS rail index")
{
    STEST_HEADER(1, 1,
                 "Usage: railIdx = subdev.Volt3x.DomainToRailIdx(voltDom)");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, UINT32, voltDomNum);
    Gpu::SplitVoltageDomain voltDom = static_cast<Gpu::SplitVoltageDomain>(voltDomNum);
    const UINT32 railIdx = pJsVolt3x->GetVolt3x()->GpuSplitVoltageDomainToRailIdx(voltDom);
    if (railIdx == ~0U)
    {
        Printf(Tee::PriError, "%s is not supported\n",
            PerfUtil::SplitVoltDomainToStr(voltDom));
        return JS_FALSE;
    }
    if (pJavaScript->ToJsval(railIdx, pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsVolt3x, RailIdxToDomain, 1,
    "Colwerts a VBIOS rail index to the corresponding Gpu.VOLTAGE enum value")
{
    STEST_HEADER(1, 1,
                 "Usage: voltDom = subdev.Volt3x.RailIdxToDomain(railIdx)");
    STEST_PRIVATE(JsVolt3x, pJsVolt3x, "JsVolt3x");
    STEST_ARG(0, UINT32, railIdx);
    const Gpu::SplitVoltageDomain voltDom =
        pJsVolt3x->GetVolt3x()->RailIdxToGpuSplitVoltageDomain(railIdx);
    if (voltDom == Gpu::ILWALID_SplitVoltageDomain)
    {
        Printf(Tee::PriError, "Voltage rail %u is not supported\n", railIdx);
        return JS_FALSE;
    }
    if (pJavaScript->ToJsval(static_cast<UINT32>(voltDom), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_CLASS(VoltConst);

static SObject VoltConst_Object
(
    "VoltConst",
    VoltConstClass,
    0,
    0,
    "VoltConst JS object"
);

PROP_CONST(VoltConst, SPLIT_RAIL_MULTI_STEP_POLICY,
           LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP);
PROP_CONST(VoltConst, SPLIT_RAIL_SINGLE_STEP_POLICY,
           LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP);
PROP_CONST(VoltConst, SINGLE_RAIL_POLICY,
           LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL);
PROP_CONST(VoltConst, SINGLE_RAIL_MULTI_STEP_POLICY,
           LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP);
PROP_CONST(VoltConst, MULTI_RAIL, LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL);
PROP_CONST(VoltConst, RELIABILITY_LIMIT, Volt3x::RELIABILITY_LIMIT);
PROP_CONST(VoltConst, ALT_RELIABILITY_LIMIT, Volt3x::ALT_RELIABILITY_LIMIT);
PROP_CONST(VoltConst, OVERVOLTAGE_LIMIT, Volt3x::OVERVOLTAGE_LIMIT);
PROP_CONST(VoltConst, VMIN_LIMIT, Volt3x::VMIN_LIMIT);
PROP_CONST(VoltConst, NUM_VOLTAGE_LIMITS, Volt3x::NUM_VOLTAGE_LIMITS);
PROP_CONST(VoltConst, VCRIT_LOW_LIMIT, Volt3x::VCRIT_LOW_LIMIT);
PROP_CONST(VoltConst, VCRIT_HIGH_LIMIT, Volt3x::VCRIT_HIGH_LIMIT);