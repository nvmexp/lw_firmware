/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/pwrsub.h"
#include "gpu/perf/thermsub.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/mle_protobuf.h"
#include "gpu/perf/perfutil.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegrasocdevice.h"
#include <limits>

JS_CLASS(PowerDummy);

namespace {  // anonymous namespace

struct CodeToStr
{
    UINT32 Code;
    const char * Str;
};

const char * CodeToStrLookup
(
    UINT32 code,
    const CodeToStr * table,
    const CodeToStr * tableEnd
)
{
    for (const CodeToStr * p = table; p < tableEnd; p++)
    {
        if (p->Code == code)
            return p->Str;
    }
    return "unknown";
}

#define CODE_LOOKUP(c, table) CodeToStrLookup(c, table, table + NUMELEMS(table))

const CodeToStr PMGR_PWR_DEVICE_TYPE_ToStr[] =
{
     { LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_DISABLED,            ENCJSENT("DISABLED") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA10HW,              ENCJSENT("BA10HW") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA11HW,              ENCJSENT("BA11HW") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW,              ENCJSENT("BA12HW") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW,              ENCJSENT("BA13HW") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW,              ENCJSENT("BA14HW") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW,              ENCJSENT("BA15HW") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219,              ENCJSENT("INA219") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221,             ENCJSENT("INA3221") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U,            ENCJSENT("NCT3933U") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC1X,            ENCJSENT("GPUADC1X") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10,            ENCJSENT("GPUADC10") }
    ,{ LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11,            ENCJSENT("GPUADC11") }
};
const CodeToStr PMGR_PWR_CHANNEL_TYPE_ToStr[] =
{
     { LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_DEFAULT,               ENCJSENT("DEFAULT") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION,             ENCJSENT("SUMMATION") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_ESTIMATION,            ENCJSENT("ESTIMATION") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR,                ENCJSENT("SENSOR") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_PSTATE_ESTIMATION_LUT, ENCJSENT("PSTATE_ESTIMATION_LUT") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED, ENCJSENT("SENSOR_CLIENT_ALIGNED") }
};
const CodeToStr PMGR_PWR_RAIL_ToStr[] =
{
     { LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_UNKNOWN,            ENCJSENT("UNKNOWN") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_LWVDD,       ENCJSENT("OUTPUT_LWVDD") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_FBVDD,       ENCJSENT("OUTPUT_FBVDD") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_FBVDDQ,      ENCJSENT("OUTPUT_FBVDDQ") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_FBVDD_Q,     ENCJSENT("OUTPUT_FBVDD_Q") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_PEXVDD,      ENCJSENT("OUTPUT_PEXVDD") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_A3V3,        ENCJSENT("OUTPUT_A3V3") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_3V3LW,       ENCJSENT("OUTPUT_3V3LW") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_TOTAL_GPU,   ENCJSENT("OUTPUT_TOTAL_GPU") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_FBVDDQ_GPU,  ENCJSENT("OUTPUT_FBVDDQ_GPU") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_FBVDDQ_MEM,  ENCJSENT("OUTPUT_FBVDDQ_MEM") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_OUTPUT_SRAM,        ENCJSENT("OUTPUT_SRAM") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_PEX12V1,      ENCJSENT("INPUT_PEX12V1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD2, ENCJSENT("INPUT_TOTAL_BOARD2") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_HIGH_VOLT0,   ENCJSENT("INPUT_HIGH_VOLT0") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_HIGH_VOLT1,   ENCJSENT("INPUT_HIGH_VOLT1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_LWVDD1,       ENCJSENT("INPUT_LWVDD1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_LWVDD2,       ENCJSENT("INPUT_LWVDD2") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN2, ENCJSENT("INPUT_EXT12V_8PIN2") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN3, ENCJSENT("INPUT_EXT12V_8PIN3") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN4, ENCJSENT("INPUT_EXT12V_8PIN4") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN5, ENCJSENT("INPUT_EXT12V_8PIN5") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_MISC0,        ENCJSENT("INPUT_MISC0") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_MISC1,        ENCJSENT("INPUT_MISC1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_MISC2,        ENCJSENT("INPUT_MISC2") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_MISC3,        ENCJSENT("INPUT_MISC3") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_USBC0,        ENCJSENT("INPUT_USBC0") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_USBC1,        ENCJSENT("INPUT_USBC1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_FAN0,         ENCJSENT("INPUT_FAN0") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_FAN1,         ENCJSENT("INPUT_FAN1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_SRAM,         ENCJSENT("INPUT_SRAM") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_PWR_SRC_PP,   ENCJSENT("INPUT_PWR_SRC_PP") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_3V3_PP,       ENCJSENT("INPUT_3V3_PP") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_3V3_MAIN,     ENCJSENT("INPUT_3V3_MAIN") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_3V3_AON,      ENCJSENT("INPUT_3V3_AON") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD,  ENCJSENT("INPUT_TOTAL_BOARD") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_LWVDD,        ENCJSENT("INPUT_LWVDD") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_FBVDD,        ENCJSENT("INPUT_FBVDD") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_FBVDDQ,       ENCJSENT("INPUT_FBVDDQ") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_FBVDD_Q,      ENCJSENT("INPUT_FBVDD_Q") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN0, ENCJSENT("INPUT_EXT12V_8PIN0") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN1, ENCJSENT("INPUT_EXT12V_8PIN1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_6PIN0, ENCJSENT("INPUT_EXT12V_6PIN0") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_6PIN1, ENCJSENT("INPUT_EXT12V_6PIN1") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_PEX3V3,       ENCJSENT("INPUT_PEX3V3") }
    ,{ LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_PEX12V,       ENCJSENT("INPUT_PEX12V") }
};
const CodeToStr POWER_CAP_POLICY_INDEX_ToStr[] =
{
    { Power::RTP,      ENCJSENT("RTP") }
    ,{ Power::TGP,     ENCJSENT("TGP") }
    ,{ Power::DROOPY,  ENCJSENT("DROOPY") }
};
const CodeToStr POWER_POLICY_TYPE_ToStr[] =
{
    { LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU,           ENCJSENT("TOTAL_GPU") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD,           ENCJSENT("WORKLOAD") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF,       ENCJSENT("BANG_BANG_VF") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT,         ENCJSENT("PROP_LIMIT") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_HW_THRESHOLD,       ENCJSENT("HW_THRESHOLD") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF,           ENCJSENT("MARCH_VF") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BALANCE,            ENCJSENT("BALANCE") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_GEMINI,             ENCJSENT("GEMINI") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL, ENCJSENT("WORKLOAD_MULTIRAIL") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH,              ENCJSENT("MARCH") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_DOMGRP,             ENCJSENT("DOMGRP") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_LIMIT,              ENCJSENT("LIMIT") }
    ,{ LW2080_CTRL_PMGR_PWR_POLICY_TYPE_UNKNOWN,            ENCJSENT("UNKNOWN") }
};

} // anonymous namespace

Power::Power(GpuSubdevice* pSubdevice) :
    m_pSubdevice(pSubdevice),
    m_LwstPowerLeakageChecker(pSubdevice),
    m_PowerRangeChecker(pSubdevice),
    m_PowerSensorInfoRetrieved(false),
    m_PowerMonitorIsEnabled(false),
    m_PowerChannelMask(0),
    m_PowerSensorMask(0),
    m_PowerMonitorSampleCount(0),
    m_PowerMonitorSamplePeriodMs(0),
    m_TotalPowerMask(0),
    m_WeightedPolicyRelationshipMask(0),
    m_BalanceablePolicyRelationshipMask(0)
{

    MASSERT(pSubdevice);
    m_Callbacks.resize(NUM_POWER_CALLBACKS);
    memset(&m_CachedPowerPolicyInfo, 0, sizeof(m_CachedPowerPolicyInfo));
    memset(&m_CachedPwrStatus, 0, sizeof(m_CachedPwrStatus));
}

Power::~Power()
{
    if (m_TegraSensorsStarted)
    {
        CheetAh::SocPtr()->EndPowerMeas();
        m_TegraSensorsStarted = false;
    }
}

RC Power::SetUseCallbacks(PowerCallback CallbackType, string FuncName)
{
    JavaScriptPtr pJS;
    JSFunction   *pJsFunc  = 0;
    JSObject     *pGlobObj = pJS->GetGlobalObject();
    if (OK != pJS->GetProperty(pGlobObj, FuncName.c_str(), &pJsFunc))
    {
       Printf(Tee::PriError, "%s not defined.\n", FuncName.c_str());
       return RC::UNDEFINED_JS_METHOD;
    }

    if (CallbackType != SET_POWER_RAIL_VOLTAGE &&
        CallbackType != CHECK_POWER_RAIL_LEAKAGE)
    {
        Printf(Tee::PriNormal, "unknown callback type!\n");
        return RC::BAD_PARAMETER;
    }

    // connect the callback functions
    m_Callbacks[CallbackType].Connect(pGlobObj, pJsFunc);
    return OK;
}

RC Power::CheckPowerPolicyMask(UINT32* policyMask)
{
    RC rc;

    if (*policyMask == 0 || *policyMask == (std::numeric_limits<UINT32>::max)())
    {
        *policyMask = m_CachedPowerPolicyInfo.policyMask;
    }
    if (!IS_MASK_SUBSET(*policyMask, m_CachedPowerPolicyInfo.policyMask))
    {
        Printf(Tee::PriError, "Invalid power policy mask 0x%x\n", *policyMask);
        return RC::BAD_PARAMETER;
    }

    return rc;
}

RC Power::GetPolicyLimitUnitStr(LwU8 limitUnit, string* unitSuffix)
{
    switch (limitUnit)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
            *unitSuffix = "mW";
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
            *unitSuffix = "mA";
            break;
        default:
            Printf(Tee::PriError, "Unknown policy limit unit type %d\n",
                   limitUnit);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC Power::PrintPoliciesInfo(UINT32 policyMask)
{
    RC rc;
    UINT32 origPolicyMask = policyMask;

    CHECK_RC(GetPowerSensorInfo());
    CHECK_RC(CheckPowerPolicyMask(&policyMask));

    // If the user specified a wildcard mask, print all of the useful policy
    // submasks. There can be more than one instance of each policy for
    // all policy types except for the domain-group policy which governs the
    // current pstate and gpc2clk frequency
    if (origPolicyMask == 0 ||
        origPolicyMask == (std::numeric_limits<UINT32>::max)())
    {
        Printf(Tee::PriNormal, "All policies mask: 0x%x\n",
               m_CachedPowerPolicyInfo.policyMask);
        Printf(Tee::PriNormal, "Relationship policy mask: 0x%x\n",
               m_CachedPowerPolicyInfo.policyRelMask);
        Printf(Tee::PriNormal, "Balanceable policy mask: 0x%x\n",
               m_CachedPowerPolicyInfo.balancePolicyMask);
        Printf(Tee::PriNormal, "Violation policy mask: 0x%x\n",
               m_CachedPowerPolicyInfo.pwrViolMask);
        Printf(Tee::PriNormal, "Domain-Group policy mask: 0x%x\n",
               m_CachedPowerPolicyInfo.domGrpPolicyMask);
    }

    for (UINT32 polIdx = 0; polIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES;
         polIdx++)
    {
        if (0 == ((1 << polIdx) & policyMask))
        {
            continue;
        }

        const LW2080_CTRL_PMGR_PWR_POLICY_2X_INFO& policyData =
            m_CachedPowerPolicyInfo.policies[polIdx].spec.v2x;
        Printf(Tee::PriNormal, "Policy %d (mask 0x%04x) - %s\n",
               polIdx, BIT(polIdx),
               PowerPolicyTypeToStr(policyData.type));
        Printf(Tee::PriNormal, "  Input Channel %d (mask 0x%04x)\n",
               policyData.chIdx, BIT(policyData.chIdx));

        string unitSuffix;
        CHECK_RC(GetPolicyLimitUnitStr(policyData.limitUnit, &unitSuffix));

        Printf(Tee::PriNormal, "  limitMin=%u%s ",
               policyData.limitMin, unitSuffix.c_str());
        Printf(Tee::PriNormal, "limitRated=%u%s ",
               policyData.limitRated, unitSuffix.c_str());
        Printf(Tee::PriNormal, "limitMax=%u%s ",
               policyData.limitMax, unitSuffix.c_str());
        if (policyData.limitBatt != (std::numeric_limits<LwU32>::max)())
        {
            Printf(Tee::PriNormal, "limitBatt=%u%s",
                   policyData.limitBatt, unitSuffix.c_str());
        }
        Printf(Tee::PriNormal, "\n");
    }

    return rc;
}

// Dump the dynamic state of all power policies in policyMask
RC Power::PrintPoliciesStatus(UINT32 policyMask)
{
    RC rc;

    CHECK_RC(GetPowerSensorInfo());
    CHECK_RC(CheckPowerPolicyMask(&policyMask));

    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS StatusParam;
    memset(&StatusParam, 0, sizeof(LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS));
    CHECK_RC(PrepPmgrPowerPolicyStatusParam(policyMask, &StatusParam));

    const Tee::Priority& pri = Tee::PriNormal;
    for (UINT32 polIdx = 0; polIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES;
         polIdx++)
    {
        /*!
         * StatusParam.policies - Array of PWR_POLICY entries.
         * Has valid indexes corresponding to the bits set in @ref policyMask.
         */
        if (0 == ((1 << polIdx) & policyMask))
        {
            continue;
        }
        string unitSuffix;
        CHECK_RC(GetPolicyLimitUnitStr(
            m_CachedPowerPolicyInfo.policies[polIdx].spec.v2x.limitUnit,
            &unitSuffix));
        LW2080_CTRL_PMGR_PWR_POLICY_STATUS *pPolicy;
        pPolicy = &(StatusParam.policies[polIdx]);
        Printf(pri, "Policy %d mask (0x%04x) :\n", polIdx, BIT(polIdx));
        Printf(pri, "  LwrrLimit=%d%s ", pPolicy->limitLwrr, unitSuffix.c_str());
        Printf(pri, "LwrrValue=%d%s ", pPolicy->valueLwrr, unitSuffix.c_str());
        Printf(pri, "Delta=%d%s\n", pPolicy->limitDelta, unitSuffix.c_str());

        UINT32 numInputs = pPolicy->limitArbPmuLwrr.numInputs;
        for (UINT32 inpIdx = 0; inpIdx < numInputs; inpIdx++)
        {
            Printf(pri, "  Limit[%d]: policyIdx=%u limitValue=%u%s\n",
                    inpIdx, pPolicy->limitArbPmuLwrr.inputs[inpIdx].pwrPolicyIdx,
                    pPolicy->limitArbPmuLwrr.inputs[inpIdx].limitValue,
                    unitSuffix.c_str());
        }
        switch (pPolicy->type)
        {
            // _DOMGRP class.
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            {
                Printf(pri, "  Domain-Group Limits: ");
                Printf(pri, "PState=%d gpc2clk=%dkHz\n",
                       pPolicy->data.domGrp.domGrpLimits.values[0],
                       pPolicy->data.domGrp.domGrpLimits.values[1]);
                break;
            }
            default:
                break;
        }
        // Handle more type specific data in detail
        switch (pPolicy->type)
        {
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
            {
                Printf(pri, "\nType = BANG_BANG_VF algorithm\n");
                Printf(pri, "  Last action: %d\n", pPolicy->data.bangBangVf.action);
                Printf(pri, "  Input VF point: pstateIdx = %u, VfIdx = %u\n",
                    pPolicy->data.bangBangVf.input.pstateIdx,
                    pPolicy->data.bangBangVf.input.vfIdx);
                Printf(pri, "  Output VF point: pstateIdx = %u, VfIdx = %u\n",
                    pPolicy->data.bangBangVf.output.pstateIdx,
                    pPolicy->data.bangBangVf.output.vfIdx);
                break;
            }
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
            {
                LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_MARCH *pMarchRmStatus = NULL;

                Printf(pri, "\nType = MARCH_VF \n");
                pMarchRmStatus = &pPolicy->data.marchVF.march;

                Printf(pri, "  Last action = %d, Current uncap limit = %u\n",
                        pMarchRmStatus->action, pMarchRmStatus->uncapLimit);
                break;
            }

            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            {
                Printf(pri, "\nType = WORKLOAD\n");
                Printf(pri, "Work input\n");
                Printf(pri, "  leakage power(mW) = %u, Freq(MHz) = %u, Volt^2(mV^2) = %u\n",
                    pPolicy->data.workload.work.pwrLeakagemW,
                    pPolicy->data.workload.work.freqMHz,
                    pPolicy->data.workload.work.voltmV2);

                Printf(pri, "Freq input\n");
                Printf(pri, "  active capacitance(w) = %u, leakage power(mW) = %u,"
                       " Freq(MHz) = %u, Volt^2(mV^2) = %u, VFIndex = %u\n",
                pPolicy->data.workload.freq.workloadmWperMHzmV2,
                pPolicy->data.workload.freq.pwrLeakagemW,
                pPolicy->data.workload.freq.freqMaxMHz,
                pPolicy->data.workload.freq.voltmV2,
                pPolicy->data.workload.freq.vfEntryIdx);
                Printf(pri, "  Workload mWperMHzmV^2 %u\n",
                        pPolicy->data.workload.workloadmWperMHzmV2);
                break;
            }
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            {
                Printf(pri, "  Type = WORKLOAD_MULTIRAIL\n");
                Printf(pri, "  Callwlated gpcclk = %uMHz\n",
                       pPolicy->data.workloadMulRail.workIface.freq.freqMaxMHz);
                Printf(pri, "  Current Target gpcclk = %uMHz\n",
                       pPolicy->data.workloadMulRail.workIface.work.freqMHz);
                Printf(pri, "  mscgResidency=%.1f%% pgResidency=%.1f%%\n",
                       Utility::ColwertFXPToFloat(
                           pPolicy->data.workloadMulRail.workIface.work.mscgResidency,
                           20, 12),
                       Utility::ColwertFXPToFloat(
                           pPolicy->data.workloadMulRail.workIface.work.pgResidency,
                           20, 12));
                Volt3x* pVolt3x = m_pSubdevice->GetVolt3x();
                MASSERT(pVolt3x);
                UINT32 railMask = pVolt3x->GetVoltRailMask();
                INT32 railIdx;
                while ((railIdx = Utility::BitScanForward(railMask)) != -1)
                {
                    railMask ^= BIT(railIdx);
                    Printf(pri, "  %s:\n",
                           PerfUtil::SplitVoltDomainToStr(
                               static_cast<Gpu::SplitVoltageDomain>(railIdx)));
                    Printf(pri, "    Workload Callwlation:\n");
                    Printf(pri, "      Volt=%umV\n",
                           pPolicy->data.workloadMulRail.workIface.work.rail[railIdx].voltmV);
                    Printf(pri, "      Power=%umW\n",
                           pPolicy->data.workloadMulRail.workIface.work.rail[railIdx].observedVal);
                    Printf(pri, "      Leakage=%umW\n",
                           pPolicy->data.workloadMulRail.workIface.work.rail[railIdx].leakagemX);
                    Printf(pri, "    Frequency Callwlation:\n");
                    Printf(pri, "      Workload=%f\n",
                           Utility::ColwertFXPToFloat(
                               pPolicy->data.workloadMulRail.workIface.freq.rail[railIdx].workload, 20, 12));
                    Printf(pri, "      Workload (unfiltered)=%f\n",
                           Utility::ColwertFXPToFloat(
                               pPolicy->data.workloadMulRail.workIface.workload[railIdx], 20, 12));
                    Printf(pri, "      Leakage=%umW\n",
                           pPolicy->data.workloadMulRail.workIface.freq.rail[railIdx].leakagemX);
                    Printf(pri, "      Volt=%umV\n",
                           pPolicy->data.workloadMulRail.workIface.freq.rail[railIdx].voltmV);
                    Printf(pri, "      VoltFloor=%uuV\n",
                           pPolicy->data.workloadMulRail.workIface.freq.rail[railIdx].voltFlooruV);
                    Printf(pri, "      EstPower=%umW\n",
                           pPolicy->data.workloadMulRail.workIface.freq.rail[railIdx].estimatedVal);
                }

                break;
            }
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
                break;
            default:
            {
                Printf(Tee::PriWarn, "Unknown power policy type 0x%x\n", pPolicy->type);
                break;
            }
        }
    }
    return rc;
}

RC Power::PrintViolationsInfo(UINT32 policyMask)
{
    RC rc;

    CHECK_RC(GetPowerSensorInfo());
    CHECK_RC(CheckPowerPolicyMask(&policyMask));
    if (policyMask == m_CachedPowerPolicyInfo.policyMask)
        policyMask = m_CachedPowerPolicyInfo.pwrViolMask;

    for (UINT32 polIdx = 0; polIdx < LW2080_CTRL_PMGR_PWR_VIOLATION_MAX; polIdx++)
    {
        if (0 == ((1 << polIdx) & policyMask))
        {
            continue;
        }
        LW2080_CTRL_PMGR_PWR_VIOLATION_INFO& viol =
            m_CachedPowerPolicyInfo.violations[polIdx];
        Printf(Tee::PriNormal, "Violation Policy %d mask (0x%04x) info:\n",
               polIdx, BIT(polIdx));
        switch (viol.thrmIdx.thrmIdxType)
        {
            case LW2080_CTRL_PMGR_PWR_VIOLATION_THERM_INDEX_TYPE_EVENT:
            {
                Printf(Tee::PriNormal, "  Thermal event idx: %u\n",
                       viol.thrmIdx.index.thrmEvent);
                break;
            }
            case LW2080_CTRL_PMGR_PWR_VIOLATION_THERM_INDEX_TYPE_MONITOR:
            {
                Printf(Tee::PriNormal, "  Thermal monitor idx: %u\n",
                       viol.thrmIdx.index.thrmMon);
                break;
            }
            default:
                Printf(Tee::PriError, "  Unknown thermal index type\n");
                return RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriNormal, "  Sample multiplier: %u\n", viol.sampleMult);
        switch (viol.type)
        {
            case LW2080_CTRL_PMGR_PWR_VIOLATION_TYPE_PROPGAIN:
            {
                Printf(Tee::PriNormal, "  Proportional gain data:\n");
                Printf(Tee::PriNormal, "    Relationship policy idx first: %u\n",
                       viol.data.propGain.policyRelIdxFirst);
                Printf(Tee::PriNormal, "    Relationship policy idx last: %u\n",
                       viol.data.propGain.policyRelIdxLast);
                break;
            }
            default:
                Printf(Tee::PriError, "Unknown violation policy type\n");
                return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

// Dump the dynamic state of all power policies in policyMask
RC Power::PrintViolationsStatus(UINT32 policyMask)
{
    RC rc;

    CHECK_RC(GetPowerSensorInfo());
    CHECK_RC(CheckPowerPolicyMask(&policyMask));
    if (policyMask == m_CachedPowerPolicyInfo.policyMask)
        policyMask = m_CachedPowerPolicyInfo.pwrViolMask;

    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS StatusParam;
    memset(&StatusParam, 0 ,sizeof(LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS));
    CHECK_RC(PrepPmgrPowerPolicyStatusParam(policyMask, &StatusParam));

    for (UINT32 polIdx = 0; polIdx < LW2080_CTRL_PMGR_PWR_VIOLATION_MAX; polIdx++)
    {
        if (0 == ((1 << polIdx) & policyMask))
        {
            continue;
        }

        Printf(Tee::PriNormal, "Violation Policy %d mask (0x%04x) status:\n",
               polIdx, BIT(polIdx));
        switch (StatusParam.violStatus.violations[polIdx].type)
        {
            case LW2080_CTRL_PMGR_PWR_VIOLATION_TYPE_PROPGAIN:
            {
                Printf(Tee::PriNormal, "  Proportional gain data:\n");
                Printf(Tee::PriNormal, "    Violation rate: %f%% (%u 4_12FXP)\n",
                       Utility::ColwertFXPToFloat(
                           StatusParam.violStatus.violations[polIdx].violLwrrent, 4, 12) * 100,
                           StatusParam.violStatus.violations[polIdx].violLwrrent);
                Printf(Tee::PriNormal, "    Power limit adj: %d\n",
                       StatusParam.violStatus.violations[polIdx].data.propGain.pwrLimitAdj);
                break;
            }
            default:
                Printf(Tee::PriError, "Unknown violation policy type\n");
                return RC::SOFTWARE_ERROR;
        }
    }
    return rc;
}

RC Power::SetPowerSupplyType(UINT32 PowerSupply)
{
    RC rc = OK;

    if (!m_pSubdevice->IsEmulation() && (Platform::GetSimulationMode() != Platform::Fmodel))
    {
        LW2080_CTRL_PERF_SET_POWERSTATE_PARAMS Param = {{0}};

        switch (PowerSupply)
        {
            case LW2080_CTRL_PERF_POWER_SOURCE_BATTERY:
            case LW2080_CTRL_PERF_POWER_SOURCE_AC:
                Param.powerStateInfo.powerState = PowerSupply;
                break;
            default:
                Printf(Tee::PriError, "Unknown power source\n");
                return RC::BAD_PARAMETER;
        }

        LwRmPtr pLwRm;
        LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
        MASSERT(pRmHandle);
        rc = pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PERF_SET_POWERSTATE,
                            &Param,
                            sizeof(Param));
        if (RC::LWRM_NOT_SUPPORTED == rc)
        {
            rc.Clear();
        }
        CHECK_RC(rc);
    }

    return rc;
}

// Custom power rail voltage controller can be different for each
// board SKU, so let the user define how to set the voltage
RC Power::SetLwstPowerRailVoltage(UINT32 Voltage)
{
    if (m_Callbacks[SET_POWER_RAIL_VOLTAGE].GetCount() == 0)
    {
        Printf(Tee::PriNormal,
               "SET_POWER_RAIL_VOLTAGE callback not hooked up!\n");
        return OK;
    }
    RC rc;
    CallbackArguments args;
    args.Push(m_pSubdevice->GetGpuInst());
    args.Push(Voltage);
    CHECK_RC(m_Callbacks[SET_POWER_RAIL_VOLTAGE].Fire(Callbacks::STOP_ON_ERROR,
                                                      args));
    return OK;
}

RC Power::CheckLwstomPowerRailLeakage()
{
    if (m_Callbacks[CHECK_POWER_RAIL_LEAKAGE].GetCount() == 0)
    {
        Printf(Tee::PriNormal,
               "CHECK_POWER_RAIL_LEAKAGE callback not hooked up!\n");
        return OK;
    }
    RC rc;
    CallbackArguments args;
    args.Push(m_pSubdevice->GetGpuInst());
    args.Push(m_LwstPowerLeakageChecker.GetMin());
    args.Push(m_LwstPowerLeakageChecker.GetMax());
    CHECK_RC(m_Callbacks[CHECK_POWER_RAIL_LEAKAGE].Fire(
                    Callbacks::STOP_ON_ERROR, args));
    return rc;
}

RC Power::RegisterLwstPowerRailLeakageCheck(UINT32 Min, UINT32 Max)
{
    RC rc;
    Printf(Tee::PriLow, "registering min= %d, Max = %d\n", Min, Max);
    CHECK_RC(m_LwstPowerLeakageChecker.InitializeCounter(Min, Max));
    return OK;
}

UINT32 Power::GetNumPowerSensors()
{
    GetPowerSensorInfo();
    return static_cast<UINT32>(m_PowerSensors.size());
}

UINT32 Power::GetPowerChannelMask()
{
    GetPowerSensorInfo();
    return m_PowerChannelMask;
}

UINT32 Power::GetPowerSensorMask()
{
    GetPowerSensorInfo();
    return m_PowerSensorMask;
}

UINT32 Power::GetPowerSensorType(UINT32 SensorMask)
{
    GetPowerSensorInfo();

    if (m_PowerSensors.find(SensorMask) == m_PowerSensors.end())
    {
        return LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_DISABLED;
    }

    return m_PowerSensors[SensorMask].DevType;
}

UINT32 Power::GetPowerRail(UINT32 channelMask)
{
    GetPowerSensorInfo();

    if (m_PowerSensors.find(channelMask) == m_PowerSensors.end())
    {
        return LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_DISABLED;
    }

    return m_PowerSensors[channelMask].PowerRail;
}

// Static string colwersion methods
const char * Power::PowerRailToStr(UINT32 Rail)
{
    return CODE_LOOKUP(Rail, PMGR_PWR_RAIL_ToStr);
}
const char * Power::PowerSensorTypeToStr(UINT32 DevType)
{
    return CODE_LOOKUP(DevType, PMGR_PWR_DEVICE_TYPE_ToStr);
}
const char * Power::PowerChannelTypeToStr(UINT32 ChannelType)
{
    return CODE_LOOKUP(ChannelType, PMGR_PWR_CHANNEL_TYPE_ToStr);
}
const char * Power::PowerCapPolicyIndexToStr(UINT32 policyIndex)
{
    return CODE_LOOKUP(policyIndex, POWER_CAP_POLICY_INDEX_ToStr);
}
const char * Power::PowerPolicyTypeToStr(UINT32 policyType)
{
    return CODE_LOOKUP(policyType, POWER_POLICY_TYPE_ToStr);
}

RC Power::GetPowerSensorReading(UINT32 SensorMask, PowerSensorReading *pReadings)
{
    RC rc;
    MASSERT(pReadings);
    CHECK_RC(GetPowerSensorInfo());

    if ((1 != Utility::CountBits(SensorMask)) ||
        (m_PowerSensors.find(SensorMask) == m_PowerSensors.end()))
    {
        // Mask 0 means "total" rather than an individual sensor.
        // But this is a legacy interface that doesn't support this.
        // Does anyone actually use this function anymore?
        Printf(Tee::PriNormal, "unknown power sensor mask\n");
        return RC::BAD_PARAMETER;
    }

    UINT32 SensorId = Utility::BitScanForward(SensorMask);

    LW2080_CTRL_PMGR_PWR_DEVICES_GET_STATUS_PARAMS Params = {};

    Params.super.objMask.super.pData[0] = SensorMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_DEVICES_GET_STATUS,
                                           &Params, sizeof(Params)));

    const UINT32 sensorType = GetPowerSensorType(SensorMask);
    if (sensorType == LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION || !sensorType)
    {
        pReadings->LwrrentMa = 0;
        pReadings->PowerMw   = 0;
        pReadings->VoltageMv = 0;
        for (LwU8 provIdx = 0; provIdx < Params.devices[SensorId].numProviders; provIdx++)
        {
            pReadings->LwrrentMa += Params.devices[SensorId].providers[provIdx].tuple.lwrrmA;
            pReadings->PowerMw   += Params.devices[SensorId].providers[provIdx].tuple.pwrmW;
            pReadings->VoltageMv += Params.devices[SensorId].providers[provIdx].tuple.voltuV / 1000;
        }
    }
    else
    {
        // Hard coding to provider 0 to mimic the existing behavior. This should be
        // moved to a provider index based sensor reading.
        pReadings->LwrrentMa = Params.devices[SensorId].providers[0].tuple.lwrrmA;
        pReadings->PowerMw   = Params.devices[SensorId].providers[0].tuple.pwrmW;
        pReadings->VoltageMv = Params.devices[SensorId].providers[0].tuple.voltuV / 1000;
    }

    Tasker::MutexHolder lock(m_Mutex);

    if (!m_MaxPowerReadings.count(SensorMask) ||
        (pReadings->PowerMw > m_MaxPowerReadings[SensorMask]))
    {
        m_MaxPowerReadings[SensorMask] = pReadings->PowerMw;
    }
    CHECK_RC(m_PowerRangeChecker.Update(SensorMask, pReadings->PowerMw));
    return rc;
}

UINT32 Power::GetWeightedPolicyRelationshipMask()
{
    GetPowerSensorInfo();
    return m_WeightedPolicyRelationshipMask;
}

UINT32 Power::GetBalanceablePolicyRelationshipMask()
{
    GetPowerSensorInfo();
    return m_BalanceablePolicyRelationshipMask;
}

RC Power::InitRunningAvg(UINT32 sensorMask, UINT32 runningAvgSize)
{
    if ((0 != sensorMask) &&
        (1 != Utility::CountBits(sensorMask)))
    {
        Printf(Tee::PriNormal, "invalid sensor mask: 0x%x\n",
               sensorMask);
        return RC::BAD_PARAMETER;
    }
    m_RunningAvgSizes[sensorMask] = runningAvgSize;
    return OK;
}

RC Power::GetPowerSensorSample(LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS * pSamp)
{
    RC rc;
    MASSERT(pSamp);
    CHECK_RC(GetPowerSensorInfo());

    memset(pSamp, 0, sizeof(*pSamp));

    if (m_pSubdevice->IsSOC())
        return GetTegraPowerSensorSample(pSamp);

    // Start with Cached values from LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_STATUS
    // ChannelStatus struct has a last time and last tuple value which is used on
    // all other subsequent reads
    *pSamp = m_CachedPwrStatus;
    // Update the Channel mask
    pSamp->super.objMask.super.pData[0] = m_PowerChannelMask;

    if (!m_PowerMonitorIsEnabled)
    {
        Tasker::MutexHolder lock(m_Mutex);

        // Older boards may support raw power-sensor readings but not
        // the newer power-monitor interface.
        // Fake it here to hide this from the rest of mods.

        for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; i++)
        {
            const UINT32 m = (1<<i);
            if (0 == (m & pSamp->super.objMask.super.pData[0]))
                continue;

            PowerSensorReading r = {0};
            CHECK_RC(GetPowerSensorReading(m, &r));
            pSamp->channelStatus[i].tuple.pwrmW = r.PowerMw;

            // In the event that we don't have a total power channel or
            // set of channels, we'll do a manual sum of all the channels,
            // otherwise use the total power channel(s)
            if (m_TotalPowerMask == 0 ||
               (m & m_TotalPowerMask) != 0)
            {
                pSamp->totalGpuPowermW += r.PowerMw;
            }
        }

        // Device mask 0 means "total".  Individual sensor masks are
        // updated in GetPowerSensorReadings.
        if (!m_MaxPowerReadings.count(0) ||
            (m_MaxPowerReadings[0] < pSamp->totalGpuPowermW))
        {
            m_MaxPowerReadings[0] = pSamp->totalGpuPowermW;
        }
        CHECK_RC(m_PowerRangeChecker.Update(0, pSamp->totalGpuPowermW));

        // Update Pwr Status cache
        m_CachedPwrStatus = *pSamp;
        return rc;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                 m_pSubdevice,
                 LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_STATUS,
                 pSamp, sizeof(*pSamp)));

    Tasker::MutexHolder lock(m_Mutex);

    UINT32 sum = 0;
    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; i++)
    {
        const UINT32 m = (1<<i);
        if (0 == (m & m_PowerChannelMask))
            continue;

        const UINT32 sampleMw = pSamp->channelStatus[i].tuple.pwrmW;

        // Replace pwr sample with running average if "-pwr_range_running_avg"
        // is in use. Requested in Bug 200209532
        if (m_RunningAvgSizes[m])
        {
            m_RunningSums[m] += sampleMw;
            m_SamplesToAvg[m].push(sampleMw);
            if (m_SamplesToAvg[m].size() > m_RunningAvgSizes[m])
            {
                m_RunningSums[m] -= m_SamplesToAvg[m].front();
                m_SamplesToAvg[m].pop();
            }
            pSamp->channelStatus[i].tuple.pwrmW = UNSIGNED_CAST(LwU32,
                m_RunningSums[m] / m_SamplesToAvg[m].size());
        }
        const UINT32 avgMw = pSamp->channelStatus[i].tuple.pwrmW;

        if ((0 == m_MaxPowerReadings.count(m)) ||
            (m_MaxPowerReadings[m] < sampleMw))
        {
            m_MaxPowerReadings[m] = sampleMw;
        }

        CHECK_RC(m_PowerRangeChecker.Update(m, avgMw));

        // In the event that we don't have a total power channel or
        // set of channels, we'll do a manual sum of all the channels,
        // otherwise use the total power channel(s)
        if (!m_TotalPowerMask || (m & m_TotalPowerMask))
        {
            sum += avgMw;
        }
    }
    if (0 == pSamp->totalGpuPowermW)
    {
        // Ugh, on older systems RM doesn't provide sum.
        // Saves having to do this work in JavaScript.
        pSamp->totalGpuPowermW = sum;
    }
    // Device mask 0 means "total".
    if ((0 == m_MaxPowerReadings.count(0)) ||
        (m_MaxPowerReadings[0] < pSamp->totalGpuPowermW))
    {
        m_MaxPowerReadings[0] = pSamp->totalGpuPowermW;
    }
    CHECK_RC(m_PowerRangeChecker.Update(0, pSamp->totalGpuPowermW));

    // Update Pwr Status cache
    m_CachedPwrStatus = *pSamp;
    return rc;
}

RC Power::GetTegraPowerSensorSample(LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS* pSamp)
{
    RC rc;

    vector<UINT32> data;
    CHECK_RC(CheetAh::SocPtr()->GetPowerMeas(&data));
    MASSERT(!data.empty());

    if (!data.empty())
    {
        pSamp->channelStatus[0].tuple.pwrmW = data[0];

        pSamp->totalGpuPowermW = data[0];
    }

    Tasker::MutexHolder lock(m_Mutex);

    if (!m_MaxPowerReadings.count(0) ||
        (m_MaxPowerReadings[0] < pSamp->totalGpuPowermW))
    {
        m_MaxPowerReadings[0] = pSamp->totalGpuPowermW;
    }
    CHECK_RC(m_PowerRangeChecker.Update(0, pSamp->totalGpuPowermW));

    return rc;
}

RC Power::GetMaxPowerSensorReading(UINT32 SensorMask, UINT32 *pPowerMw)
{
    MASSERT(pPowerMw);
    *pPowerMw = 0;

    if (Utility::CountBits(SensorMask) > 1)
    {
        Printf(Tee::PriError,
               "GetMaxPowerSensorReading : Only 1 sensor (or 0 for sum) may be specified\n");
        return RC::BAD_PARAMETER;
    }

    Tasker::MutexHolder lock(m_Mutex);

    if (!m_MaxPowerReadings.count(SensorMask))
    {
        Printf(Tee::PriError,
               "GetMaxPowerSensorReading : No power reading found for "
               "sensor 0x%08x\n",
               SensorMask);
        return RC::BAD_PARAMETER;
    }

    *pPowerMw = m_MaxPowerReadings[SensorMask];
    return OK;
}

void Power::ResetMaxPowerSensorReading(UINT32 SensorMask)
{
    Tasker::MutexHolder lock(m_Mutex);
    if (SensorMask == 0)
    {
        m_MaxPowerReadings.clear();
    }
    else if (m_MaxPowerReadings.count(SensorMask))
    {
        m_MaxPowerReadings.erase(SensorMask);
    }
}

RC Power::InitPowerRangeChecker(const vector<PowerRangeChecker::PowerRange> &Ranges)
{
    return m_PowerRangeChecker.InitializeCounter(Ranges);
}

RC Power::PrintChannelsInfo(UINT32 channelMask) const
{
    RC rc;

    LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS devParams = {0};
    LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS monParams = {0};

    // Get power devices data from the Power Sensors Table in the VBIOS
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_DEVICES_GET_INFO,
                                           &devParams, sizeof(devParams)));

    // Get power channels data from Power Topology Table in the VBIOS
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_INFO,
                                           &monParams, sizeof(monParams)));

    if (channelMask == 0 || channelMask == (std::numeric_limits<UINT32>::max)())
        channelMask = monParams.channelMask;

    if (!IS_MASK_SUBSET(channelMask, monParams.channelMask))
    {
        Printf(Tee::PriError, "Invalid power channel mask 0x%x\n", channelMask);
        return RC::BAD_PARAMETER;
    }

    ct_assert(LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS <= 32);
    for (UINT32 channelIdx = 0;
         channelIdx < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; channelIdx++)
    {
        if (!(BIT(channelIdx) & channelMask))
            continue;

        LW2080_CTRL_PMGR_PWR_CHANNEL_INFO& ch = monParams.channels[channelIdx];
        const LwU8& pwrRail = ch.pwrRail;
        const LwU8& channelType = ch.type;

        Printf(Tee::PriNormal, "Channel %02d (mask 0x%04x) %s type=%s\n",
               channelIdx, BIT(channelIdx),
               PowerRailToStr(pwrRail),
               PowerChannelTypeToStr(channelType));

        switch (channelType)
        {
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED:
            {
                Printf(Tee::PriNormal, "  ");
                const LwU8& pwrDevIdx =
                    ch.data.sensor.pwrDevIdx;
                const LwU8& pwrDevType =
                    devParams.devices[pwrDevIdx].type;
                Printf(Tee::PriNormal, "sensorType=%s sensorIdx=%d sensorInputIdx=%d\n",
                       PowerSensorTypeToStr(pwrDevType),
                       pwrDevIdx,
                       ch.data.sensor.pwrDevProvIdx);

                break;
            }
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION:
            {
                Printf(Tee::PriNormal, "  ");
                for (LwU8 relIdx = ch.data.sum.relIdxFirst;
                     relIdx <= ch.data.sum.relIdxLast;
                     relIdx++)
                {
                    LwU8& childChannelIdx = monParams.chRels[relIdx].chIdx;
                    LwS32& weight = monParams.chRels[relIdx].data.weight.weight;
                    Printf(Tee::PriNormal, "%s*%.3f",
                           PowerRailToStr(monParams.channels[childChannelIdx].pwrRail),
                           Utility::ColwertFXPToFloat(weight, 20, 12));
                    if (relIdx != ch.data.sum.relIdxLast)
                        Printf(Tee::PriNormal, " + ");
                }
                Printf(Tee::PriNormal, "\n");
                break;
            }
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_PSTATE_ESTIMATION_LUT:
            {
                Printf(Tee::PriNormal, "  ");
                Printf(Tee::PriNormal, "volt=%duV ", ch.voltFixeduV);
                for (UINT32 ii = 0;
                     ii < LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_MAX_ENTRIES;
                     ii++)
                {
                    Printf(Tee::PriNormal, "P%d offset=%dmW",
                           ch.data.pstateEstLUT.lutEntry[ii].pstate,
                           ch.data.pstateEstLUT.lutEntry[ii].powerOffset);
                    if (ii != LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_MAX_ENTRIES - 1)
                        Printf(Tee::PriNormal, " ");
                }
                Printf(Tee::PriNormal, "\n");
                break;
            }
            default:
                break;
        }
    }

    return rc;
}

RC Power::GetLwrrPowerCapLimFreqkHzToJs
(
    JSObject *pJsObj
)
{
    RC rc = OK;
    JavaScriptPtr pJs;
    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(1);
    statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
    statuses[0].output.bEnabled = FALSE;
    if (!(m_pSubdevice->IsEmOrSim()))
    {
        rc = PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice);
        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            rc.Clear();
        }
        CHECK_RC(rc);
    }
    UINT64 freqkHz = statuses[0].output.bEnabled ?
                     statuses[0].output.value :
                     0;
    CHECK_RC(pJs->SetElement(pJsObj, 0, freqkHz));
    return rc;
}

RC Power::GetPowerCapRange(Power::PowerCapIdx i,
                           UINT32 * pMin,
                           UINT32 * pRated,
                           UINT32 * pMax)
{
    MASSERT(pMin && pRated && pMax);
    RC rc;
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS infoParam = {0};
    CHECK_RC(PrepPmgrPowerPolicyInfoParam(&infoParam));
    CHECK_RC(ColwertPowerPolicyIndex(&i));

    *pMin = infoParam.policies[i].spec.v2x.limitMin;
    *pRated = infoParam.policies[i].spec.v2x.limitRated;
    *pMax = infoParam.policies[i].spec.v2x.limitMax;
    return rc;
}

RC Power::GetPowerCap(Power::PowerCapIdx i, UINT32 *pLimit)
{
    MASSERT(pLimit);

    RC rc;
    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS param = {0};
    CHECK_RC(ColwertPowerPolicyIndex(&i));
    CHECK_RC(PrepPmgrPowerPolicyControlParam(&param, (1<<i), 0));

    // Report the power limit setting.
    *pLimit = param.policies[i].limitLwrr;
    return rc;
}

RC Power::SetPowerCap(Power::PowerCapIdx i, UINT32 newLimit)
{
    RC rc;
    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS param = {0};
    CHECK_RC(ColwertPowerPolicyIndex(&i));
    CHECK_RC(PrepPmgrPowerPolicyControlParam(&param, (1<<i), 0));

    // We will use the clock-ratio settings as-is (retrieved by Prep... above),
    // with our new power limit setting.
    param.policies[i].limitLwrr = newLimit;

    // RM will store, and send the new limit on the PMU.
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_POLICY_SET_CONTROL,
        &param, sizeof(param)));

    return rc;
}

RC Power::PrintPowerCapPolicyTable(Tee::Priority pri)
{
    RC rc;

    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS infoParam = {0};
    CHECK_RC(PrepPmgrPowerPolicyInfoParam(&infoParam));

    Printf(pri, "LW2080_CTRL_PMGR_PWR_POLICY_INFO_INFOPARAM:\n");
    Printf(pri, " policyMask\t0x%x\n", infoParam.policyMask);
    Printf(pri, " domGrpPolicyMask\t0x%x\n", infoParam.domGrpPolicyMask);
    Printf(pri, " limitPolicyMask\t0x%x\n", infoParam.limitPolicyMask);
    Printf(pri, " tgpPolicyIdx\t%u\n", infoParam.tgpPolicyIdx);
    Printf(pri, " rtpPolicyIdx\t%u\n", infoParam.rtpPolicyIdx);

    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; i++)
    {
        if (0 == (infoParam.policyMask & (1 << i)))
            continue;

        LW2080_CTRL_PMGR_PWR_POLICY_2X_INFO & p2x = infoParam.policies[i].spec.v2x;
        Printf(pri, " policies[%u].type\t%u\n", i, p2x.type);
        Printf(pri, " policies[%u].chIdx\t%u\n", i, p2x.chIdx);
        Printf(pri, " policies[%u].limitUnit\t%d (%s)\n",
               i,
               p2x.limitUnit,
               (p2x.limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
               ? "mW" : "mA" );
        Printf(pri, " policies[%u].limitMin\t%u\n", i, p2x.limitMin);
        Printf(pri, " policies[%u].limitRated\t%u\n", i, p2x.limitRated);
        Printf(pri, " policies[%u].limitMax\t%u\n", i, p2x.limitMax);
        switch(p2x.type)
        {
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_DOMGRP:
                Printf(pri, " policies[%u].data.domGrp.b3DBoostVpstateFloor\t%d\n",
                       i, p2x.data.domGrp.b3DBoostVpstateFloor);
                break;
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
                Printf(pri, " policies[%u].data.bangBangVf.domGrp.b3DBoostVpstateFloor\t%d\n",
                       i, p2x.data.bangBangVf.domGrp.b3DBoostVpstateFloor);
                Printf(pri, " policies[%u].data.bangBangVf.uncapLimitRatio\t%d\n",
                       i, p2x.data.bangBangVf.uncapLimitRatio);
                break;
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
                Printf(pri, " policies[%u].data.propLimit.policyRelIdxFirst\t%u\n",
                       i, p2x.data.propLimit.policyRelIdxFirst);
                Printf(pri, " policies[%u].data.propLimit.policyRelIdxLast\t%u\n",
                       i, p2x.data.propLimit.policyRelIdxLast);
                break;
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
                Printf(pri, " policies[%u].data.totalGpu.tgpIface.fbPolicyRelIdx\t%u\n",
                       i, p2x.data.totalGpu.tgpIface.fbPolicyRelIdx);
                Printf(pri, " policies[%u].data.totalGpu.tgpIface.corePolicyRelIdx\t%u\n",
                       i, p2x.data.totalGpu.tgpIface.corePolicyRelIdx);
                Printf(pri, " policies[%u].data.totalGpu.staticValue\t%u\n",
                       i, p2x.data.totalGpu.staticValue);
                break;
            case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
                Printf(pri, " policies[%u].data.workload.domGrp.b3DBoostVpstateFloor\t%d\n",
                       i, p2x.data.workload.domGrp.b3DBoostVpstateFloor);
                Printf(pri, " policies[%u].data.workload.leakageIdx\t%u\n",
                       i, p2x.data.workload.leakageIdx);
                Printf(pri, " policies[%u].data.workload.medianFilterSize\t%u\n",
                       i, p2x.data.workload.medianFilterSize);
                break;
        }
    }

    Printf(pri, " policyRelMask\t0x%x\n", infoParam.policyRelMask);

    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; i++)
    {
        if (0 == (infoParam.policyRelMask & (1 << i)))
            continue;

        LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_INFO & r = infoParam.policyRels[i];
        Printf(pri, " policyRels[%u].type\t%u\n", i, r.type);
        Printf(pri, " policyRels[%u].policyIdx\t%u\n", i, r.policyIdx);
        Printf(pri, " policyRels[%u].data.weight.weight\t%u\n", i, r.data.weight.weight);
    }

    if (0 == infoParam.policyMask)
    {
        // This is a non-PowerCapping/SmartPower vbios.
        return rc;
    }

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS ctrlparams = {0};
    CHECK_RC(PrepPmgrPowerPolicyControlParam(&ctrlparams, infoParam.policyMask, 0));

    Printf(pri, "\nLW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS:\n");
    Printf(pri, " policyMask\t0x%x\n", ctrlparams.policyMask);

    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; i++)
    {
        if (0 == (ctrlparams.policyMask & (1 << i)))
            continue;
        LW2080_CTRL_PMGR_PWR_POLICY_CONTROL & c = ctrlparams.policies[i];
        Printf(pri, " policies[%u].type\t%u\n", i, c.type);
        Printf(pri, " policies[%u].limitLwrr\t%u\n", i, c.limitLwrr);
        Printf(pri, " policies[%u].data.workload.clkUpScale\t0x%x\n",
               i, c.data.workload.clkUpScale);
        Printf(pri, " policies[%u].data.workload.clkDownScale\t0x%x\n",
               i, c.data.workload.clkDownScale);
    }
    return rc;
}

bool Power::IsPowerBalancingSupported()
{
    StickyRC rc;
    rc = GetPowerSensorInfo();

    if (rc != OK)
    {
        Printf(Tee::PriNormal,
               "Unable to determine if power balancing is supported (rc = %d)\n",
               rc.Get());
        return false;
    }

    return m_CachedPowerPolicyInfo.balancePolicyMask != 0;
}

RC Power::GetBalanceablePowerPolicyRelations(vector<UINT32> *pPolicyRelIndices)
{
    MASSERT(pPolicyRelIndices);
    RC rc;

    CHECK_RC(GetPowerSensorInfo());

    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICY_RELATIONSHIPS; ++i)
    {
        if (m_BalanceablePolicyRelationshipMask & (1U << i))
        {
            pPolicyRelIndices->push_back(i);
        }
    }

    return OK;
}

RC Power::GetPowerBalancePwm(UINT32 policyRelationshipIndex, FLOAT64 *pct)
{
    RC rc;
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS statusParams;
    memset(&statusParams, 0 ,sizeof(LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS));
    CHECK_RC(CheckForValidPolicyRelationshipIndex(policyRelationshipIndex));
    statusParams.policyRelStatus.super.objMask.super.pData[0] = 1U << policyRelationshipIndex;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             m_pSubdevice,
             LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_STATUS,
             &statusParams, sizeof(statusParams)));

    UINT32 fxpPct = statusParams.policyRelStatus.policyRels[policyRelationshipIndex].data
                                                                                    .balance
                                                                                    .pwmPctLwrr;

    // RM thinks that a percentage is 0-1.0, so 50% is 0.5 -> 0x8000 in UFXP16.16
    *pct = 100. * Utility::ColwertFXPToFloat(fxpPct, 16, 16);
    return OK;
}

RC Power::SetPowerBalancePwm(UINT32 policyRelationshipIndex, FLOAT64 pct)
{
    RC rc;
    Printf(Tee::PriDebug,
           "SetPowerBalancePwm: pct = %f, policyRelationshipIndex = %u\n",
           pct, policyRelationshipIndex);

    if (!(0.0 <= pct && pct <= 100.0))
    {
        Printf(Tee::PriError, "SetPowerBalancePwm: PWM percent must be 0-100\n");
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS ctrlParams = {0};
    CHECK_RC(PrepPowerBalanceCtrlParams(&ctrlParams, policyRelationshipIndex));

    UINT32 pctFxp = Utility::ColwertFloatToFXP(pct / 100., 16, 16);

    ctrlParams.policyRels[policyRelationshipIndex].data.balance.bPwmSim = LW_TRUE;
    ctrlParams.policyRels[policyRelationshipIndex].data.balance.pwmPctSim = pctFxp;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             m_pSubdevice,
             LW2080_CTRL_CMD_PMGR_PWR_POLICY_SET_CONTROL,
             &ctrlParams, sizeof(ctrlParams)));

    return rc;
}

RC Power::ResetPowerBalance(UINT32 policyRelationshipIndex)
{
    RC rc;

    Printf(Tee::PriDebug,
           "Resetting policyRelationshipIndex = %u\n",
           policyRelationshipIndex);

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS ctrlParams = {0};
    CHECK_RC(PrepPowerBalanceCtrlParams(&ctrlParams, policyRelationshipIndex));

    ctrlParams.policyRels[policyRelationshipIndex].data.balance.bPwmSim = LW_FALSE;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_POLICY_SET_CONTROL,
        &ctrlParams, sizeof(ctrlParams)));

    return rc;
}

RC Power::GetPowerChannelsFromPolicyRelationshipIndex(UINT32 policyRelationshipIndex,
                                                     UINT32 *primaryChannel,
                                                     UINT32 *secondaryChannel,
                                                     UINT32 *phaseEstimateChannel)
{
    MASSERT(primaryChannel);
    MASSERT(secondaryChannel);
    MASSERT(phaseEstimateChannel);

    RC rc;
    CHECK_RC(CheckForValidPolicyRelationshipIndex(policyRelationshipIndex));

    // Get policies associated with this relationship
    UINT32 primaryRailPowerPolicy = m_CachedPowerPolicyInfo
                                    .policyRels[policyRelationshipIndex]
                                    .policyIdx;
    UINT32 secondaryRailPowerPolicy = m_CachedPowerPolicyInfo
                                      .policyRels[policyRelationshipIndex]
                                      .data.balance.secPolicyIdx;

    // Get channels associated with each policy
    *primaryChannel = m_CachedPowerPolicyInfo
                      .policies[primaryRailPowerPolicy]
                      .spec.v2x.chIdx;
    *secondaryChannel = m_CachedPowerPolicyInfo
                        .policies[secondaryRailPowerPolicy]
                        .spec.v2x.chIdx;

    // Invalid if set to LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID
    *phaseEstimateChannel = m_CachedPowerPolicyInfo
                            .policyRels[policyRelationshipIndex]
                            .data.balance.phaseEstimateChIdx;
    return OK;
}

RC Power::GetPowerEquationInfoToJs(JSContext *pCtx, JSObject *pJsObj)
{
    RC rc;
    LW2080_CTRL_PMGR_PWR_EQUATION_GET_INFO_PARAMS Param = {{0}, 0};
    CHECK_RC(PrepPmgrPowerEquationInfo(&Param));

    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(pJsObj, "hwIddqVersion",
                              static_cast<UINT32>(Param.iddq.versionHw)));
    CHECK_RC(pJs->SetProperty(pJsObj, "iddqVersion",
                              static_cast<UINT32>(Param.iddq.version)));
    CHECK_RC(pJs->SetProperty(pJsObj, "iddqmA",
                              static_cast<UINT32>(Param.iddq.valuemA)));
    CHECK_RC(pJs->SetProperty(pJsObj, "equationMask",
                              static_cast<UINT32>(Param.equationMask)));

    JSObject *pPowerEquEntries = JS_NewArrayObject(pCtx, 0, NULL);
    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_EQUATION_MAX_EQUATIONS; i++)
    {
        JSObject *pPowerEqu = JS_NewObject(pCtx, &PowerDummyClass, NULL, NULL);
        if (Param.equations[i].type == 1)
        {
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k0",
                                      static_cast<UINT32>(Param.equations[i].data.dtcs11.k0)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k1",
                                      static_cast<UINT32>(Param.equations[i].data.dtcs11.k1)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k2",
                                      static_cast<UINT32>(Param.equations[i].data.dtcs11.k2)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k3",
                                      static_cast<INT32>(Param.equations[i].data.dtcs11.k3)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "fsEff",
                                      static_cast<UINT16>(Param.equations[i].data.leakage.fsEff)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "pgEff",
                                      static_cast<UINT16>(Param.equations[i].data.leakage.pgEff)));
        }
        else if (Param.equations[i].type == 2)
        {
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k0",
                                      static_cast<UINT32>(Param.equations[i].data.ba1xScale.refVoltageuV)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k1",
                                      static_cast<UINT32>(Param.equations[i].data.ba1xScale.ba2mW)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k2",
                                      static_cast<UINT32>(Param.equations[i].data.ba1xScale.gpcClkMHz)));
            CHECK_RC(pJs->SetProperty(pPowerEqu, "k3",
                                      static_cast<UINT32>(Param.equations[i].data.ba1xScale.utilsClkMHz)));
        }
        else
            continue;

        CHECK_RC(pJs->SetProperty(pPowerEqu, "type",
                                  static_cast<UINT08>(Param.equations[i].type)));
        jsval PowerEquJsval;
        CHECK_RC(pJs->ToJsval(pPowerEqu, &PowerEquJsval));
        CHECK_RC(pJs->SetElement(pPowerEquEntries, i, PowerEquJsval));
    }
    jsval PowerEquArrayJsval;
    CHECK_RC(pJs->ToJsval(pPowerEquEntries, &PowerEquArrayJsval));
    CHECK_RC(pJs->SetPropertyJsval(pJsObj, "PowerEquEntries", PowerEquArrayJsval));
    return rc;
}

RC Power::GetPowerCapPollingPeriod(UINT16* periodms)
{
    MASSERT(periodms);
    *periodms = 0;

    RC rc;

    CHECK_RC(GetPowerSensorInfo());
    *periodms = m_CachedPowerPolicyInfo.baseSamplePeriod;

    return rc;
}

RC Power::GetClkUpScaleFactor(LwUFXP4_12* pClkUpScale)
{
    RC rc;

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS policyCtrl = {};
    CHECK_RC(GetWorkloadMultiRailPolicies(&policyCtrl));

    for (UINT32 policyIdx = 0; policyIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; policyIdx++)
    {
        if (!(policyCtrl.policyMask & (1 << policyIdx)))
        {
            continue;
        }

        // Assumes all workload policies will have the same clkUpScale factor, which
        // is true for GP10x/GV100
        *pClkUpScale = policyCtrl.policies[policyIdx].data.workloadMulRail.clkUpScale;
        return rc;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC Power::SetClkUpScaleFactor(LwUFXP4_12 clkUpScale)
{
    // Don't allow clkUpScale > 1.0 or clkUpScale == 0.0
    if (clkUpScale > 0x1000 || clkUpScale == 0)
    {
        return RC::BAD_PARAMETER;
    }

    RC rc;

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS policyCtrl = {};
    CHECK_RC(GetWorkloadMultiRailPolicies(&policyCtrl));

    for (UINT32 policyIdx = 0; policyIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; policyIdx++)
    {
        if (!(policyCtrl.policyMask & (1 << policyIdx)))
        {
            continue;
        }

        policyCtrl.policies[policyIdx].data.workloadMulRail.clkUpScale = clkUpScale;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_POLICY_SET_CONTROL,
        &policyCtrl, sizeof(policyCtrl)));

    return rc;
}

RC Power::GetWorkloadMultiRailPolicies(LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS* pParams)
{
    RC rc;

    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS infoParam = {};
    CHECK_RC(PrepPmgrPowerPolicyInfoParam(&infoParam));

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS policyCtrl = {};
    CHECK_RC(PrepPmgrPowerPolicyControlParam(&policyCtrl, infoParam.policyMask, 0));

    UINT32 mask = 0;

    for (UINT32 policyIdx = 0; policyIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; policyIdx++)
    {
        if (policyCtrl.policies[policyIdx].type ==
            LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL)
        {
            mask |= 1 << policyIdx;
        }
    }

    CHECK_RC(PrepPmgrPowerPolicyControlParam(pParams, mask, 0));

    return rc;
}

RC Power::IsClkUpScaleSupported(bool* pSupported)
{
    RC rc;

    *pSupported = false;

    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS policyCtrl = {};
    CHECK_RC(GetWorkloadMultiRailPolicies(&policyCtrl));

    *pSupported = (policyCtrl.policyMask != 0);

    return rc;
}

UINT32 Power::FindChannelMask(UINT32 rail)
{
    UINT32 channelMask = GetPowerChannelMask();

    while (channelMask)
    {
        const INT32 channelBit = Utility::BitScanForward(channelMask);
        if (channelBit == -1)
            break;
        const UINT32 channelSubMask = 1 << channelBit;
        channelMask ^= channelSubMask;
        UINT32 powerRail = GetPowerRail(channelSubMask);
        if (powerRail == rail)
        {
            return channelSubMask;
        }
    }

    return 0;
}

INT32 Power::FindChannelIdx(UINT32 rail)
{
    UINT32 channelMask = GetPowerChannelMask();

    for (UINT32 channelIdx = 0; channelIdx < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; channelIdx++)
    {
        const INT32 channelBit = Utility::BitScanForward(channelMask);
        if (channelBit == -1)
            break;
        const UINT32 channelSubMask = 1 << channelBit;
        channelMask ^= channelSubMask;
        UINT32 powerRail = GetPowerRail(channelSubMask);
        if (powerRail == rail)
        {
            return channelIdx;
        }
    }

    return -1;
}

RC Power::GetPowerSensorInfo()
{
    if (m_PowerSensorInfoRetrieved)
        return OK;

    if (m_pSubdevice->IsSOC())
        return GetTegraPowerSensorInfo();

    StickyRC rc;
    LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS devParams = {0};
    LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS monParams = {0};

    // Get power devices data from the Power Sensors Table in the VBIOS
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_DEVICES_GET_INFO,
                                           &devParams, sizeof(devParams)));

    // Get power channels data from Power Topology Table in the VBIOS
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_INFO,
                                           &monParams, sizeof(monParams)));

    // Get power policy information from the Power Policy Tables in the VBIOS
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_INFO,
                                           &m_CachedPowerPolicyInfo,
                                           sizeof(m_CachedPowerPolicyInfo)));

    m_PowerMonitorIsEnabled = monParams.bSupported ? true : false;

    // RM has moved to a higher-level abstraction (channels/rails) rather
    // than devices.
    // We need to redefine mods' power-monitoring APIs to be per-rail.
    //
    // Until we do this, we must force-fit the mods concept of power-devices
    // into the RM concept of power-rails/channels, which doesn't 100% work
    // across all possible boards.

    if (monParams.bSupported)
    {
        m_PowerMonitorSampleCount = monParams.sampleCount;
        m_PowerMonitorSamplePeriodMs = monParams.samplingPeriodms;
        if (monParams.totalGpuChannelIdx !=
            LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID)
        {
            m_TotalPowerMask = 1 << monParams.totalGpuChannelIdx;
        }
        else
        {
            m_TotalPowerMask = monParams.totalGpuPowerChannelMask;
        }

        rc = ConstructPowerSensors(devParams, monParams);
    }

    if (!m_PowerSensorMask)
    {
        Printf(Tee::PriLow, "Legacy Power Channels not supported.\n");
    }
    else
    {
        Printf(Tee::PriLow, "Will use the RM PWR_MONITOR API.\n");
    }

    rc = ConstructPowerPolicyMasks();

    GetDroopyInfo(devParams, monParams);

    DumpPowerTables((OK == rc) ? Tee::PriLow : Tee::PriError,
                    devParams, monParams);

    m_PowerSensorInfoRetrieved = true;

    return rc;
}

RC Power::GetTegraPowerSensorInfo()
{
    RC rc;

    if (!m_TegraSensorsStarted)
    {
        CHECK_RC(CheetAh::SocPtr()->StartPowerMeas());
        m_TegraSensorsStarted = true;
    }

    vector<string> railNames;
    CHECK_RC(CheetAh::SocPtr()->GetPowerMeasRailNames(&railNames));
    MASSERT(!railNames.empty());
    MASSERT(railNames[0].size() >= 3 && memcmp(railNames[0].data(), "GPU", 3) == 0); // GPU always first on CheetAh

    m_PowerMonitorIsEnabled = true;

    m_TotalPowerMask = 1;

    m_PowerMonitorSampleCount = 1;
    m_PowerMonitorSamplePeriodMs = 10;

    m_PowerChannelMask = m_TotalPowerMask;
    m_PowerSensorMask = m_TotalPowerMask;

    PowerSensor sensor = { };
    sensor.ChannelType = LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR;
    sensor.PowerRail = LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_UNKNOWN;
    sensor.DevType = LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221;
    m_PowerSensors[1] = sensor;

    m_PowerSensorInfoRetrieved = true;

    return rc;
}

RC Power::ConstructPowerSensors
(
    const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS& devParams,
    const LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS& monParams
)
{
    // Find power-channels that correspond to a single sensor-device.
    // Add each such channel to the mods m_PowerSensors map.
    for (UINT32 cIdx = 0; cIdx < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS;
         cIdx++)
    {
        const int cMask = 1<<cIdx;
        const LW2080_CTRL_PMGR_PWR_CHANNEL_INFO & c =
            monParams.channels[cIdx];
        if ((0 == (monParams.channelMask & cMask)) || (0 == c.pwrRail))
            continue;

        const int SKIP_CHAN = -1;
        const int NO_DEV_CHAN = -2;

        int devIdx = SKIP_CHAN;

        switch (c.type)
        {
            default:
                Printf(Tee::PriError, "Unexpected power channel type 0x%x\n", c.type);
                return RC::SOFTWARE_ERROR;

            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_DEFAULT:
                // Ignore this channel.
                break;

            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_ESTIMATION:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_PSTATE_ESTIMATION_LUT:
                // This is a valid power tables 2.0 rail/channel
                // But it is not its own device/sensor
                devIdx = NO_DEV_CHAN;
                break;

            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED:
            {
                // See bug 1921730 - Droopy power sensor does not provide any
                // telemetry data
                if (devParams.devices[c.data.sensor.pwrDevIdx].type ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U)
                {
                    devIdx = SKIP_CHAN;
                }
                else
                {
                    devIdx = c.data.sensor.pwrDevIdx;
                }
                break;
            }
        }
        if (devIdx != SKIP_CHAN)
        {
            PowerSensor sensor = {0};
            sensor.ChannelType = c.type;
            sensor.PowerRail = c.pwrRail;
            m_PowerChannelMask |= cMask;
            if (devIdx != NO_DEV_CHAN)
            {
                MASSERT(devParams.devMask & (1<<devIdx));
                sensor.DevType  = devParams.devices[devIdx].type;
                m_PowerSensorMask |= cMask;
            }
            m_PowerSensors[cMask] = sensor;
        }
    }

    return OK;
}

RC Power::ConstructPowerPolicyMasks()
{
    for (UINT32 i = 0;
         i < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICY_RELATIONSHIPS;
         ++i)
    {
        UINT32 mask = 1U << i;
        if (m_CachedPowerPolicyInfo.policyRelMask & mask)
        {
            switch (m_CachedPowerPolicyInfo.policyRels[i].type)
            {
                case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_WEIGHT:
                    m_WeightedPolicyRelationshipMask |= mask;
                    break;
                case LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_TYPE_BALANCE:
                    m_BalanceablePolicyRelationshipMask |= mask;
                    break;
                default:
                    Printf(Tee::PriError, "Unexpected power policy info type\n");
                    return RC::SOFTWARE_ERROR;
            }
        }
    }

    // WAR an RM bug where they do not always set "tgpPolicyIdx"
    if (m_CachedPowerPolicyInfo.tgpPolicyIdx != LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID)
    {
        return RC::OK;
    }

    for (UINT32 ii = 0;
         ii < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES;
         ii++)
    {
        switch (m_CachedPowerPolicyInfo.policies[ii].version)
        {
            case LW2080_CTRL_PMGR_PWR_POLICY_TABLE_VERSION_2X:
                if (m_CachedPowerPolicyInfo.policies[ii].spec.v2x.type ==
                    LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU)
                {
                    m_CachedPowerPolicyInfo.tgpPolicyIdx = ii;
                }
                break;
            case LW2080_CTRL_PMGR_PWR_POLICY_TABLE_VERSION_3X:
                if (m_CachedPowerPolicyInfo.policies[ii].spec.v3x.super.type ==
                    LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU)
                {
                    m_CachedPowerPolicyInfo.tgpPolicyIdx = ii;
                }
                break;
            default:
                // TODO: why is RM returning 0 for the "version"??
                break;
        }
    }

    return OK;
}

void Power::GetDroopyInfo
(
    const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS& devParams,
    const LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS& monParams
)
{
    // Find the first HW threshold policy that uses a NCT3933U sensor.
    // Assumes that there will only be one droopy policy.
    UINT32 polIdx = 0;
    for (; polIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES; ++polIdx)
    {
        if (!(m_CachedPowerPolicyInfo.policyMask & (1 << polIdx)))
            continue;

        if (m_CachedPowerPolicyInfo.policies[polIdx].spec.v2x.type ==
            LW2080_CTRL_PMGR_PWR_POLICY_TYPE_HW_THRESHOLD)
        {
            const auto& chIdx = m_CachedPowerPolicyInfo.policies[polIdx].spec.v2x.chIdx;
            const auto& devIdx = monParams.channels[chIdx].data.sensor.pwrDevIdx;
            if (devParams.devices[devIdx].type ==
                LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U)
            {
                m_CachedDroopyPolicyIdx = polIdx;
                break;
            }
        }
    }

    if (polIdx == LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES)
    {
        Printf(Tee::PriDebug, "Droopy power policy not supported in VBIOS\n");
    }
}

void Power::DumpPowerTables(
    Tee::Priority pri,
    const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS & devParams,
    const LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS & monParams
)
{
    Printf(pri, "LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS =\n");
    Printf(pri, " .devMask = 0x%x\n", devParams.devMask);
    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICES_MAX_DEVICES; i++)
    {
        if (devParams.devMask & (1<<i))
            Printf(pri, " .devices[%u].type=0x%x, .powerRail=0x%x\n",
                   i,
                   devParams.devices[i].type,
                   devParams.devices[i].powerRail);
    }

    Printf(pri, "LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS =\n");
    Printf(pri, " .bSupported = %d\n", monParams.bSupported);
    Printf(pri, " .channelMask = 0x%x\n", monParams.channelMask);
    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; i++)
    {
        if (monParams.channelMask & (1<<i))
        {
            Printf(pri, " .channels[%u].type=0x%x .pwrRail=0x%x ",
                   i,
                   monParams.channels[i].type,
                   monParams.channels[i].pwrRail);
            switch (monParams.channels[i].type)
            {
                case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
                    Printf(pri, ".data.sensor.pwrDevIdx=%d\n",
                           monParams.channels[i].data.sensor.pwrDevIdx);
                    break;

                default:
                    // Mods doesn't care about the others.
                    Printf(pri, "\n");
                    break;
            }
        }
    }

    Printf(pri, " .chRelMask = 0x%x\n", monParams.chRelMask);
    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHRELATIONSHIPS; ++i)
    {
        if (monParams.chRelMask & (1 << i))
        {
            const LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_INFO & rel =
                monParams.chRels[i];
            Printf(pri, " .chRels[%d].type=0x%x .chIdx=0x%x ", i, rel.type,
                   rel.chIdx);
            switch(rel.type)
            {
                case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_WEIGHT:
                    Printf(pri, " .data.weight.weight = %x (%.1f)\n",
                           rel.data.weight.weight,
                           Utility::ColwertFXPToFloat(rel.data.weight.weight,
                                                      20, 12));
                    break;
                case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCED_PHASE_EST:
                    Printf(pri, "\n");
                    Printf(pri, "  .data.balancedPhaseEst.numTotalPhases = %d\n",
                                rel.data.balancedPhaseEst.numTotalPhases);
                    Printf(pri, "  .data.balancedPhaseEst.numStaticPhases = %d\n",
                                rel.data.balancedPhaseEst.numStaticPhases);
                    Printf(pri, "  .data.balancedPhaseEst"
                                ".balancedPhasePolicyRelIdxFirst = %d\n",
                                rel.data.balancedPhaseEst
                                .balancedPhasePolicyRelIdxFirst);
                    Printf(pri, "  .data.balancedPhaseEst."
                                "balancedPhasePolicyRelIdxLast = %d\n",
                                rel.data.balancedPhaseEst
                                .balancedPhasePolicyRelIdxLast);
                    break;
                case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCING_PWM_WEIGHT:
                    Printf(pri, "\n");
                    Printf(pri, "  .data.balancingPwmWeight.balancingRelIdx = %d\n",
                                rel.data.balancingPwmWeight.balancingRelIdx);
                    Printf(pri, "  .data.balancingPwmWeight.bPrimary = %d\n",
                                rel.data.balancingPwmWeight.bPrimary);
                    break;
                case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_LOSS_EST:
                    Printf(pri, "\n");
                    Printf(pri, "  .data.regulatorLossEst.regulatorType = %d\n",
                                rel.data.regulatorLossEst.regulatorType);
                    Printf(pri, "  .data.regulatorLossEst.coefficient0 = %d\n",
                                rel.data.regulatorLossEst.coefficient0);
                    Printf(pri, "  .data.regulatorLossEst.coefficient1 = %d\n",
                                rel.data.regulatorLossEst.coefficient1);
                    Printf(pri, "  .data.regulatorLossEst.coefficient2 = %d\n",
                                rel.data.regulatorLossEst.coefficient2);
                    Printf(pri, "  .data.regulatorLossEst.coefficient3 = %d\n",
                                rel.data.regulatorLossEst.coefficient3);
                    Printf(pri, "  .data.regulatorLossEst.coefficient4 = %d\n",
                                rel.data.regulatorLossEst.coefficient4);
                    Printf(pri, "  .data.regulatorLossEst.coefficient5 = %d\n",
                                rel.data.regulatorLossEst.coefficient5);
                    break;
                default:
                    Printf(pri, " unknown type!\n");
                    break;
            }
        }
    }
    Printf(pri, "\n");
}

RC Power::PrepPmgrPowerPolicyInfoParam
(
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS * pInfoParam
)
{
    MASSERT(pInfoParam);
    memset(pInfoParam, 0, sizeof(*pInfoParam));
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_INFO,
        pInfoParam, sizeof(*pInfoParam));
}

// Prepare Power Policy Dynamic Status structure
RC Power::PrepPmgrPowerPolicyStatusParam
(
     UINT32 policyMask,
     LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS * pStatusParam
)
{
    MASSERT(pStatusParam);
    memset(pStatusParam, 0, sizeof(*pStatusParam));
    pStatusParam->super.objMask.super.pData[0] = policyMask;
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_STATUS,
        pStatusParam, sizeof(*pStatusParam));
}

// Prepare Perf Power Model Static status structure
RC Power::PrepPerfPowerModelInfoParam
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODELS_INFO * pPowerModelInfoParam
)
{
    MASSERT(pPowerModelInfoParam);
    memset(pPowerModelInfoParam, 0, sizeof(*pPowerModelInfoParam));
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PERF_PERF_CF_PWR_MODELS_GET_INFO,
        pPowerModelInfoParam, sizeof(*pPowerModelInfoParam));
}

// Inject Perf Power Model Scaling Parameters
RC Power::SetPerfPowerModelScaleParam
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_PARAMS * pPowerModelScaleParam
)
{
    MASSERT(pPowerModelScaleParam);
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PERF_PERF_CF_PWR_MODEL_SCALE,
        pPowerModelScaleParam, sizeof(*pPowerModelScaleParam));
}

RC Power::ColwertPowerPolicyIndex
(
    PowerCapIdx * pIdx  // In,Out
)
{
    MASSERT(pIdx);

    RC rc;

    CHECK_RC(GetPowerSensorInfo());

    PowerCapIdx idx = *pIdx;

    // Colwert the index from symbolic (rtp/tgp) to literal, if necessary.
    if (idx == RTP)
    {
        idx = static_cast<PowerCapIdx>(m_CachedPowerPolicyInfo.rtpPolicyIdx);
    }
    else if (idx == TGP)
    {
        idx = static_cast<PowerCapIdx>(m_CachedPowerPolicyInfo.tgpPolicyIdx);
    }
    else if (idx == DROOPY)
    {
        idx = static_cast<PowerCapIdx>(m_CachedDroopyPolicyIdx);
    }
    else if (idx >= LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES)
    {
        Printf(Tee::PriError, "PowerCap policy index %d is invalid.\n", idx);
        Printf(Tee::PriNormal,
               "Index must be 0 to %d or one of %d (RTP), %d (TGP), or %d (DROOPY)\n",
               LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES - 1, RTP, TGP, DROOPY);
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (idx >= LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES ||
        !(m_CachedPowerPolicyInfo.policyMask & (1 << idx)))
    {
        Printf(Tee::PriWarn, "%s power policy is not supported in the VBIOS on GPU %u\n",
               PowerCapPolicyIndexToStr(*pIdx), m_pSubdevice->GetGpuInst());
        return RC::UNSUPPORTED_FUNCTION;
    }
    *pIdx = idx;
    return rc;
}

//! Common code for Get/SetPowerCap, Set/GetPowerBalance.
RC Power::PrepPmgrPowerPolicyControlParam
(
    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS * pParam,
    UINT32 policyMask,
    UINT32 policyRelMask
)
{
    MASSERT(pParam);

    RC rc;
    memset(pParam, 0, sizeof(*pParam));
    pParam->policyMask = policyMask;
    pParam->policyRelMask = policyRelMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_CONTROL,
        pParam, sizeof(*pParam)));

    return rc;
}

//! code for getting PWR_EQUATION info using RMCTRL
RC Power::PrepPmgrPowerEquationInfo
(
    LW2080_CTRL_PMGR_PWR_EQUATION_GET_INFO_PARAMS *pParam
)
{
    RC rc;
    memset(pParam, 0, sizeof(*pParam));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_PMGR_PWR_EQUATION_GET_INFO,
        pParam, sizeof(*pParam)));

    return rc;
}

RC Power::CheckForValidPolicyRelationshipIndex(UINT32 policyRelationshipIndex)
{
    RC rc;

    CHECK_RC(GetPowerSensorInfo());
    UINT32 policyRelMask = 1U << policyRelationshipIndex;
    if (!(m_BalanceablePolicyRelationshipMask & policyRelMask))
    {
        Printf(Tee::PriError,
               "CheckForValidPolicyRelationshipIndex: "
               "Invalid policyRelationshipIndex (%d)\n", policyRelationshipIndex);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC Power::PrepPowerBalanceCtrlParams
(
    LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS *pParam,
    UINT32 policyRelationshipIndex
)
{
    RC rc;
    CHECK_RC(CheckForValidPolicyRelationshipIndex(policyRelationshipIndex));
    CHECK_RC(PrepPmgrPowerPolicyControlParam(pParam, 0, 1U << policyRelationshipIndex));
    return rc;
}

RC Power::GetPowerSensorCfgVals
(
    const UINT08 pwrSensorType,
    PwrSensorCfg *pPwrSensorCfg
)
{
    RC rc;

    LwRmPtr pLwRm;
    LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS devParams = {};

    // Get all valid sensors from Power Sensors Table
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PMGR_PWR_DEVICES_GET_INFO,
                                           &devParams, sizeof(devParams)));

    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICES_MAX_DEVICES; i++)
    {
        // Parse entries marked as VALID
        if (devParams.devMask & (1<<i))
        {
            if (devParams.devices[i].type == pwrSensorType)
            {
                Printf(Tee::PriDebug, "Found sensor type =%02x with I2C Device index =%02x"
                                      "I2C Device Configuration =%04x\n",
                                      pwrSensorType,
                                      devParams.devices[i].data.ina3221.i2cDevIdx,
                                      devParams.devices[i].data.ina3221.configuration);
                pPwrSensorCfg->push_back({devParams.devices[i].data.ina3221.i2cDevIdx,
                                          devParams.devices[i].data.ina3221.configuration});
            }
        }
    }

    if (pPwrSensorCfg->size() == 0)
    {
        Printf(Tee::PriError, "This board is not configured with INA3221\n");
        return RC::I2C_DEVICE_NOT_FOUND;
    }

    return rc;
}

void Power::DumpTgp1xEstimatedMetrics
(
    Tee::Priority pri,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_COMBINED_1X *pwrTgp1XEstimates
)
{
    Printf(pri, "\n****** GPU %u Start - Dumping TGP1X Scaled estimates ******\n",
                                                            m_pSubdevice->GetGpuInst());
    Printf(pri, "PowerHintW = %.3f\n", pwrTgp1XEstimates->estimatedVal / 1000.0f);

    // TODO - Implement this also for rails other than LWVDD
    Printf(pri, "effectiveFreqMHz = %d\n", pwrTgp1XEstimates->singles[0].effectiveFreqMHz);
    Printf(pri, "estimatedVal = %d\n", pwrTgp1XEstimates->singles[0].estimatedVal);
    Printf(pri, "estimatedValFloor = %d\n", pwrTgp1XEstimates->singles[0].estimatedValFloor);
    Printf(pri, "freqFloorMHz = %d\n", pwrTgp1XEstimates->singles[0].freqFloorMHz);
    Printf(pri, "freqMHz = %d\n", pwrTgp1XEstimates->singles[0].freqMHz);
    Printf(pri, "leakagemX = %d\n", pwrTgp1XEstimates->singles[0].leakagemX);
    Printf(pri, "voltmV = %d\n", pwrTgp1XEstimates->singles[0].voltmV);
    Printf(pri, "workload = %d\n", pwrTgp1XEstimates->singles[0].workload);

    Printf(pri, "****** End - Dumping TGP1X Scaled estimates ******\n");
}

RC Power::GetPowerScalingTgp1XEstimates
(
    FLOAT32 temp,
    UINT32 gpcClkMhz,
    UINT32 workLoad
)
{
    RC rc;

    // Check if VBIOS supports TGP1X power model
    LW2080_CTRL_PERF_PERF_CF_PWR_MODELS_INFO  perfPwrModelInfoParams  = {};
    CHECK_RC(PrepPerfPowerModelInfoParam(&perfPwrModelInfoParams));

    UINT32 i;
    bool bFound = false;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &perfPwrModelInfoParams.super.objMask.super)
    {
        if (perfPwrModelInfoParams.pwrModels[i].super.type
                        == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X)
        {
            bFound = true;
            break;
        }
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    if (!bFound)
    {
        Printf(Tee::PriError, "TGP1X Power Model not supported on this GPU\n");
        return RC::SOFTWARE_ERROR;
    }

    // Set Simulated temperature
    Thermal * pTherm = m_pSubdevice->GetThermal();
    CHECK_RC(pTherm->SetChipTempViaChannel(ThermalSensors::GPU_AVG, temp));

    // Give sufficient time for VF lwrve to catch up with temperature updates
    Perf* pPerf = m_pSubdevice->GetPerf();
    UINT32 vfePollingMs;
    CHECK_RC(pPerf->GetVfePollingPeriodMs(&vfePollingMs));
    Tasker::Sleep(vfePollingMs);

    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS pwrPolicyInfoParams = {};
    CHECK_RC(PrepPmgrPowerPolicyInfoParam(&pwrPolicyInfoParams));

    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS pwrPolicyStatusParams = {};
    CHECK_RC(PrepPmgrPowerPolicyStatusParam(pwrPolicyInfoParams.policyMask, &pwrPolicyStatusParams));

    // TODO - Add a print function to dump Observed and Estimated Metrics from CWCC1X

    // Set Clock frequency and workload input for scaling
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_PARAMS pwrModelScaleParams = {};
    pwrModelScaleParams.pwrModelIdx = i;
    pwrModelScaleParams.numEstimatedMetrics = 1;
    pwrModelScaleParams.inputs[0].freqkHz[0] = gpcClkMhz * 1000ULL;
    pwrModelScaleParams.bounds.type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X;
    pwrModelScaleParams.observedMetrics.type= LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X;
    pwrModelScaleParams.observedMetrics.data.tgp1x.super.type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X;
    pwrModelScaleParams.observedMetrics.data.tgp1x.workloadParameters.workload[0] = workLoad;
    //Set mask for GpcClk
    LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&pwrModelScaleParams.inputs[0].domainsMask.super, 0);

    // Set Scaling Params
    CHECK_RC(SetPerfPowerModelScaleParam(&pwrModelScaleParams));

    DumpTgp1xEstimatedMetrics(Tee::PriNormal,
                           &pwrModelScaleParams.estimatedMetrics[0].data.tgp1x.workloadCombined1x);

    // Check the actual temperature being set. RM will ignore unrealistic temperature
    
    // Print Power Hint and input config to MLE    
    ErrorMap::ScopedDevInst setDevInst(static_cast<INT32>(m_pSubdevice->GetGpuInst()));
    Mle::Print(Mle::Entry::power_hint)
         .temperature(static_cast<INT32>(temp))
         .gpcclk(gpcClkMhz)
         .workload(workLoad)
         .pwrestimate(pwrModelScaleParams.estimatedMetrics[0].data.tgp1x.workloadCombined1x.estimatedVal / 1000.0f);

    return rc;
}

bool Power::HasAdcPwrSensor()
{
    LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS devParams = {};

    // Get all valid sensors from Power Sensors Table
    if (RC::OK != LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                                LW2080_CTRL_CMD_PMGR_PWR_DEVICES_GET_INFO,
                                                &devParams, sizeof(devParams)))
    {
        return false;
    }

    for (UINT32 ii = 0; ii < LW2080_CTRL_PMGR_PWR_DEVICES_MAX_DEVICES; ii++)
    {
        // Parse entries marked as VALID
        if ((devParams.devMask & (1 << ii)) && IsAdcPwrDevice(devParams.devices[ii].type))
        {
            return true;
        }
    }

    return false;
}

bool Power::IsAdcPwrDevice(UINT32 devType)
{
    switch (devType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC1X:
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            return true;
        default:
            return false;
    }
}

RC LwstPowerRailLeakageChecker::InitializeCounter(UINT32 Min,
                                                  UINT32 Max)
{
    m_Min = Min;
    m_Max = Max;
    RC rc;
    // Todo.. come up with a better name and error code
    CHECK_RC(ErrCounter::Initialize("FailedPowerRailLeak",
                                    1, 0, nullptr,
                                    MODS_TEST, POWER_RAIL_LEAK_PRI));
    return rc;
}

RC LwstPowerRailLeakageChecker::ReadErrorCount(UINT64 * pCount)
{
    Power *pPower = (GpuErrCounter::GetGpuSubdevice())->GetPower();
    if (OK != pPower->CheckLwstomPowerRailLeakage())
    {
        Printf(Tee::PriNormal, "Leakage outside threshold detected\n");
        m_Count++;
    }
    pCount[0] = m_Count;
    return OK;
}

/* virtual */ RC LwstPowerRailLeakageChecker::OnError(const ErrorData *pData)
{
    return RC::EXCEEDED_EXPECTED_THRESHOLD;
}

RC PowerRangeChecker::InitializeCounter(const vector<PowerRange> & RangePerMask)
{
    for (UINT32 i = 0; i < RangePerMask.size(); i++)
    {
        if ((0 != RangePerMask[i].SensorMask) &&
            (1 != Utility::CountBits(RangePerMask[i].SensorMask)))
        {
            Printf(Tee::PriNormal, "invalid sensor mask: 0x%x\n",
                   RangePerMask[i].SensorMask);
            return RC::BAD_PARAMETER;
        }
        if (RangePerMask[i].MinMw > RangePerMask[i].MaxMw)
        {
            Printf(Tee::PriNormal, "Min cannot be > Max\n");
            return RC::BAD_PARAMETER;
        }
        m_SensorMaskToIdx[RangePerMask[i].SensorMask] = (UINT32) m_Count.size();
        m_Count.push_back(0);
        m_MinMw.push_back(RangePerMask[i].MinMw);
        m_MaxMw.push_back(RangePerMask[i].MaxMw);
    }
    RC rc;
    CHECK_RC(ErrCounter::Initialize("PowerReadChecker",
                                    static_cast<UINT32>(m_Count.size()), 0,
                                    nullptr, MODS_TEST, POWER_READING_PRI));
    m_Active = true;
    return rc;
}

RC PowerRangeChecker::ReadErrorCount(UINT64 * pCount)
{
    for (UINT32 i = 0; i < m_Count.size(); i++)
    {
        pCount[i] = m_Count[i];
    }
    return OK;
}

RC PowerRangeChecker::OnError(const ErrorData *pData)
{
    map<UINT32, UINT32>::const_iterator SensorIter = m_SensorMaskToIdx.begin();
    for (; SensorIter != m_SensorMaskToIdx.end(); SensorIter++)
    {
        UINT32 SensorMask = SensorIter->first;
        UINT32 SensorIdx  = SensorIter->second;
        UINT64 OverThresholdCount = pData->pNewErrors[SensorIdx];
        if (OverThresholdCount > 0)
        {
            Power *pPower = (GpuErrCounter::GetGpuSubdevice())->GetPower();
            Printf(Tee::PriNormal, "%s : Sensor 0x%x ",
                   GpuErrCounter::GetGpuSubdevice()->GpuIdentStr().c_str(),
                   SensorMask);
            const UINT32 Type = SensorMask ? pPower->GetPowerSensorType(SensorMask)
                : LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION;
            const UINT32 Rail = SensorMask ? pPower->GetPowerRail(SensorMask)
                : LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD;
            const char * RailStr = pPower->PowerRailToStr(Rail);
            Printf(Tee::PriNormal, "Type: 0x%x, Rail=%s ",
                   Type, RailStr);
            Printf(Tee::PriNormal, "\n\tover the range count: %lld\n",
                   OverThresholdCount);
        }
    }
    return RC::EXCEEDED_EXPECTED_THRESHOLD;
}

RC PowerRangeChecker::Update(UINT32 SensorMask, UINT32 PowerMw)
{
    // user didn't register the sensor to be checked
    if (!m_Active ||
        (m_SensorMaskToIdx.find(SensorMask) == m_SensorMaskToIdx.end()))
    {
        return OK;
    }
    UINT32 Idx = m_SensorMaskToIdx[SensorMask];
    if ((PowerMw < m_MinMw[Idx]) || (PowerMw > m_MaxMw[Idx]))
    {
        m_Count[Idx]++;
    }
    return OK;
}
