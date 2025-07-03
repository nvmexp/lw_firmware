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

// Mods headers
#include "gpu/perf/voltsub.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "core/include/registry.h"
#include "lwRmReg.h"
#include <map>

Volt3x::Volt3x(GpuSubdevice *pSubdevice) :
    m_pSubdevice(pSubdevice)
   ,m_IsInitialized(false)
   ,m_VoltRailMask(0)
   ,m_VoltDeviceMask(0)
   ,m_VoltPolicyMask(0)
   ,m_SplitRailConstraintAuto(false)
   ,m_pSplitRailConstraintManager(nullptr)
   ,m_HasActiveVoltageLimits(false)
   ,m_HasActiveVoltageLimitOffsets(false)
{
}

Volt3x::~Volt3x()
{
    LazyDeleteSRConstMgr();
}

RC Volt3x::Initialize()
{
    RC rc;

    // Skip Volt initialization based on PMU bootstrap mode
    UINT32 pmuBootstrapMode;
    if (OK == Registry::Read("ResourceManager", "RmPmuBootstrapMode", &pmuBootstrapMode) &&
        LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_RM != pmuBootstrapMode)
    {
        return OK;
    }

    if (m_IsInitialized)
    {
        return OK;
    }
    Printf(Tee::PriDebug, "Beginning mods Volt3x initialization\n");

    UINT32 i;

    // Get and print all of the rail info
    rc = LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_INFO
       ,&m_CachedRailsInfo
       ,sizeof(m_CachedRailsInfo));
    if (rc != OK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_VoltRailMask = m_CachedRailsInfo.super.objMask.super.pData[0];

    Printf(Tee::PriDebug,
           "Global voltage rail info:\n"
           "rail mask = 0x%x\n"
           "voltDomainHAL = %d\n",
           m_VoltRailMask,
           m_CachedRailsInfo.voltDomainHAL);

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRailsInfo.super.objMask.super)
        PrintSingleVoltRailInfo(m_CachedRailsInfo.rails[i]);
        UINT32 type = m_CachedRailsInfo.rails[i].super.type;
        Gpu::SplitVoltageDomain dom = RmVoltDomToGpuSplitVoltageDomain(type);
        m_VoltRailInfos.push_back({ dom, i });
        m_AvailableDomains.insert(dom);
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    // Update Volt Limits
    CHECK_RC(ConstructVoltLimits());

    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS status;
    status.super = m_CachedRailsInfo.super;
    CHECK_RC(GetVoltRailStatusViaMask(m_VoltRailMask, &status));
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &status.super.objMask.super)
        UINT32 type = status.rails[i].super.type;
        Gpu::SplitVoltageDomain dom = RmVoltDomToGpuSplitVoltageDomain(type);
        m_OrigVoltLimitsuV[dom][RELIABILITY_LIMIT] = status.rails[i].relLimituV;
        m_OrigVoltLimitsuV[dom][ALT_RELIABILITY_LIMIT] = status.rails[i].altRelLimituV;
        m_OrigVoltLimitsuV[dom][OVERVOLTAGE_LIMIT] = status.rails[i].ovLimituV;
        m_OrigVoltLimitsuV[dom][VMIN_LIMIT] = status.rails[i].vminLimituV;
        m_OrigVoltLimitsuV[dom][VCRIT_LOW_LIMIT] = status.rails[i].ramAssist.vCritLowuV;
        m_OrigVoltLimitsuV[dom][VCRIT_HIGH_LIMIT] = status.rails[i].ramAssist.vCritHighuV;
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    // Get and print all of device info
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_DEVICES_GET_INFO
       ,&m_CachedDevicesInfo
       ,sizeof(m_CachedDevicesInfo)));

    m_VoltDeviceMask = m_CachedDevicesInfo.super.objMask.super.pData[0];

    Printf(Tee::PriDebug,
           "Global voltage device info\n"
           "device mask = 0x%x\n",
           m_VoltDeviceMask);
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedDevicesInfo.super.objMask.super)
        PrintSingleVoltDeviceInfo(m_CachedDevicesInfo.devices[i]);
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    // Get and print all of the policy info
    LW2080_CTRL_VOLT_VOLT_POLICIES_INFO_PARAMS policyParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_GET_INFO
       ,&policyParams
       ,sizeof(policyParams)));

    LW2080_CTRL_VOLT_VOLT_POLICIES_STATUS_PARAMS policyStatusParams = {};
    policyStatusParams.super = policyParams.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_GET_STATUS
       ,&policyStatusParams
       ,sizeof(policyStatusParams)));

    m_VoltPolicyMask = policyParams.super.objMask.super.pData[0];

    Printf(Tee::PriDebug,
           "Global voltage policy info\n"
           "policy mask = 0x%x\n",
           m_VoltPolicyMask);
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &policyParams.super.objMask.super)
        PrintSingleVoltPolicyInfo(policyParams.policies[i],
                                  policyStatusParams.policies[i]);
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_CONTROL
       ,&m_CachedRailsCtrl
       ,sizeof(m_CachedRailsCtrl)));

    m_IsInitialized = true;
    Printf(Tee::PriDebug, "Mods Volt3x initialized\n");
    return OK;
}

RC Volt3x::Cleanup()
{
    RC rc;

    if (!m_IsInitialized)
        return OK;

    if (m_HasActiveVoltageLimits)
        CHECK_RC(ClearAllSetVoltages());

    if (m_HasActiveVoltageLimitOffsets)
        CHECK_RC(ClearRailLimitOffsets());

    if (m_pSplitRailConstraintManager)
        CHECK_RC(m_pSplitRailConstraintManager->Cleanup());

    return rc;
}

RC Volt3x::ConstructVoltLimits()
{
    MASSERT(m_VoltLimits.size() == 0);

    RC rc;

    LW2080_CTRL_PERF_LIMIT_SET_STATUS newLimit = {0};
    newLimit.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;
    newLimit.input.data.volt.info.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
    newLimit.input.data.volt.clkDomain = 0;

    for (auto voltDom : m_AvailableDomains)
    {
        CHECK_RC(VoltageDomainToLimitId(voltDom, &newLimit.limitId));
        newLimit.input.data.volt.info.voltageDomain =
            PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(voltDom);
        m_VoltLimits.push_back(newLimit);
    }

    if (m_VoltLimits.size() == 0)
    {
        Printf(Tee::PriError, "Unable to construct Volt Limits\n");
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    return RC::OK;
}

bool Volt3x::HasDomain(Gpu::SplitVoltageDomain dom) const
{
    return m_AvailableDomains.find(dom) != m_AvailableDomains.end();
}

bool Volt3x::IsPascalSplitRailSupported() const
{
    return (m_CachedRailsInfo.voltDomainHAL ==
                LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GP10X_SPLIT_RAIL);
}

RC Volt3x::PrintVoltLimits() const
{
    RC rc;

    Printf(Tee::PriAlways, "Setting voltage limits:\n");

    bool atLeastOneLimitActive = false;
    for (UINT32 ii = 0; ii < m_VoltLimits.size(); ii++)
    {
        if (m_VoltLimits[ii].input.type != LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED)
            atLeastOneLimitActive = true;
    }
    if (!atLeastOneLimitActive)
    {
        Printf(Tee::PriAlways, " All MODS voltage limits disabled\n");
        return rc;
    }

    vector<LW2080_CTRL_PERF_LIMIT_INFO> infos(LW2080_CTRL_PERF_MAX_LIMITS);
    for (UINT32 i = 0; i < infos.size(); i++)
        infos[i].limitId = i;
    CHECK_RC(PerfUtil::GetRmPerfLimitsInfo(infos, m_pSubdevice));

    for (UINT32 ii = 0; ii < m_VoltLimits.size(); ii++)
    {
        MASSERT(m_VoltLimits[ii].limitId < infos.size());
        PerfUtil::PrintRmPerfLimit(Tee::PriNormal, infos[m_VoltLimits[ii].limitId],
                                   m_VoltLimits[ii], m_pSubdevice);
    }

    return rc;
}

RC Volt3x::GetLwrrentVoltagesuV
(
    map<Gpu::SplitVoltageDomain, UINT32>& voltages
)
{
    RC rc;

    UINT32 ii;
    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS status;
    CHECK_RC(GetVoltRailStatusViaMask(m_VoltRailMask, &status));
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &status.super.objMask.super)
        Gpu::SplitVoltageDomain dom = RailIdxToGpuSplitVoltageDomain(ii);
        voltages[dom] = status.rails[ii].lwrrVoltDefaultuV;
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Volt3x::GetSensedVoltagesuV
(
    map<Gpu::SplitVoltageDomain, UINT32>& voltages
)
{
    RC rc;

    UINT32 ii;
    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS status;
    CHECK_RC(GetVoltRailStatusViaMask(m_VoltRailMask, &status));
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &status.super.objMask.super)
        Gpu::SplitVoltageDomain dom = RailIdxToGpuSplitVoltageDomain(ii);
        voltages[dom] = status.rails[ii].lwrrVoltSenseduV;
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Volt3x::GetVoltRailStatusViaMask
(
    UINT32 voltRailMask,
    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS *pVoltRailParams
)
{
    MASSERT(voltRailMask);

    if (!IS_MASK_SUBSET(voltRailMask, GetVoltRailMask()))
    {
        Gpu::SplitVoltageDomain voltDom =
            RmVoltDomToGpuSplitVoltageDomain(voltRailMask);
        Printf(Tee::PriError,
               "voltage domain %s is not present on this board\n",
               PerfUtil::SplitVoltDomainToStr(voltDom));
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }
    MASSERT(pVoltRailParams);

    RC rc;
    memset(pVoltRailParams, 0, sizeof(*pVoltRailParams));
    pVoltRailParams->super.objMask.super.pData[0] = voltRailMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_STATUS
       ,pVoltRailParams
       ,sizeof(*pVoltRailParams)));

    return rc;
}

RC Volt3x::GetVoltRailStatus
(
    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS *pVoltRailParams
)
{
    return GetVoltRailStatusViaMask(GetVoltRailMask(), pVoltRailParams);
}

RC Volt3x::GetVoltRailStatusViaId
(
    Gpu::SplitVoltageDomain voltDom,
    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS *pRailStatus
)
{
    RC rc;
    MASSERT(pRailStatus);

    // Make sure the voltage domain exists
    if (!HasDomain(voltDom))
    {
        Printf(Tee::PriError, "Cannot fetch voltage on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(voltDom));
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    UINT32 railMask = (1 << GpuSplitVoltageDomainToRailIdx(voltDom));
    LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS railParams = {};
    CHECK_RC(GetVoltRailStatusViaMask(railMask, &railParams));

    // POD copy here
    INT32 railIdx;
    if ((railIdx = Utility::BitScanForward(railMask)) != -1)
    {
        *pRailStatus = railParams.rails[railIdx];
    }
    else
    {
        Printf(Tee::PriLow, "rail Index is < 0 which makes the rail Status unavailable \n");
        *pRailStatus = {};
    }
    return rc;
}

RC Volt3x::GetVoltRailRegulatorStepSizeuV
(
    Gpu::SplitVoltageDomain dom,
    LwU32* pStepSizeuV
) const
{
    MASSERT(pStepSizeuV);
    *pStepSizeuV = 0;

    RC rc;

    // Make sure the voltage domain exists
    if (!HasDomain(dom))
    {
        Printf(Tee::PriError, "Cannot fetch voltage on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(dom));
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    // Colwert to the RM voltage domain
    UINT32 voltDomBit = PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(dom);

    // Loop over each voltage device and return the step-size for the device
    // with the corresponding voltage domain
    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &m_CachedDevicesInfo.super.objMask.super)
        if (m_CachedDevicesInfo.devices[ii].voltDomain == voltDomBit)
        {
            *pStepSizeuV = m_CachedDevicesInfo.devices[ii].voltStepuV;
            return rc;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    Printf(Tee::PriError,
           "Cannot find voltage regulator for %s\n",
           PerfUtil::SplitVoltDomainToStr(dom));

    return RC::ILWALID_VOLTAGE_DOMAIN;
}

RC Volt3x::SetRailLimitOffsetuV
(
    Gpu::SplitVoltageDomain voltDom,
    VoltageLimit voltLimit,
    INT32 offsetuV
)
{
    RC rc;

    // Make sure the voltage domain we are offsetting a limit for exists
    if (!HasDomain(voltDom))
    {
        Printf(Tee::PriError, "Cannot offset voltage limit on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(voltDom));
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    // Make sure we have a valid index into rails[railIdx].voltDeltauV[]
    if (voltLimit >= NUM_VOLTAGE_LIMITS || voltLimit < 0)
    {
        Printf(Tee::PriError, "Bad voltage limit %d passed to %s\n",
               static_cast<INT32>(voltLimit), __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    // Make sure the original voltage limit is not 0. If it is 0, its
    // corresponding VFE has been disabled in the Voltage Rail Table in
    // the VBIOS, and RM cannot offset the limit since it doesn't exist.
    if (!m_OrigVoltLimitsuV.find(voltDom)->second.find(voltLimit)->second)
    {
        Printf(Tee::PriError,
               "%s voltage limit's VFE is disabled in VBIOS\n",
               PerfUtil::VoltageLimitToStr(voltLimit));
        return RC::BAD_PARAMETER;
    }

    LW2080_CTRL_VOLT_VOLT_RAILS_CONTROL_PARAMS railsControlParams = {};
    const UINT32 railIdx = GpuSplitVoltageDomainToRailIdx(voltDom);
    MASSERT(railIdx != ~0U);
    PerfUtil::BoardObjGrpMaskBitSet(&railsControlParams.super.objMask.super, railIdx);

    // Get the previously set limit offsets on this rail so that
    // we don't overwrite them
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    // Set the new limit offset
    railsControlParams.rails[railIdx].voltDeltauV[voltLimit] = offsetuV;
    m_CachedRailsCtrl.rails[railIdx].voltDeltauV[voltLimit] = offsetuV;

    // And send it to RM
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_SET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    m_HasActiveVoltageLimitOffsets = true;

    return rc;
}

RC Volt3x::SetRailLimitsOffsetuV
(
    Gpu::SplitVoltageDomain voltDom,
    const map<VoltageLimit, INT32> &voltOffsetuV
)
{
    RC rc;

    // Make sure the voltage domain we are offsetting a limit for exists
    if (!HasDomain(voltDom))
    {
        Printf(Tee::PriError, "Cannot offset voltage limit on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(voltDom));
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }


    LW2080_CTRL_VOLT_VOLT_RAILS_CONTROL_PARAMS railsControlParams = {};
    const UINT32 railIdx = GpuSplitVoltageDomainToRailIdx(voltDom);
    MASSERT(railIdx != ~0U);
    PerfUtil::BoardObjGrpMaskBitSet(&railsControlParams.super.objMask.super, railIdx);

    // Get the previously set limit offsets on this rail so that
    // we don't overwrite them
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    for (auto const& domOffset : voltOffsetuV)
    {
        VoltageLimit voltLimit = domOffset.first;
        INT32 offsetuV = domOffset.second;

        // Make sure we have a valid index into rails[railIdx].voltDeltauV[]
        if (voltLimit >= NUM_VOLTAGE_LIMITS || voltLimit < 0)
        {
            Printf(Tee::PriError, "Bad voltage limit %d passed to %s\n",
                static_cast<INT32>(voltLimit), __FUNCTION__);
            return RC::BAD_PARAMETER;
        }

        // Make sure the original voltage limit is not 0. If it is 0, its
        // corresponding VFE has been disabled in the Voltage Rail Table in
        // the VBIOS, and RM cannot offset the limit since it doesn't exist.
        if (!m_OrigVoltLimitsuV.find(voltDom)->second.find(voltLimit)->second)
        {
            Printf(Tee::PriError,
                "%s voltage limit's VFE is disabled in VBIOS\n",
                PerfUtil::VoltageLimitToStr(voltLimit));
            return RC::BAD_PARAMETER;
        }

        // Set the new limit offset
        railsControlParams.rails[railIdx].voltDeltauV[voltLimit] = offsetuV;
        m_CachedRailsCtrl.rails[railIdx].voltDeltauV[voltLimit] = offsetuV;

    }

    // And send it to RM
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_SET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    m_HasActiveVoltageLimitOffsets = true;

    return rc;
}

RC Volt3x::GetRailLimitOffsetsuV
(
    map<Gpu::SplitVoltageDomain, vector<INT32> >* limitsuV
) const
{
    RC rc;

    // Get all offsets to all limits for any existing rails
    LW2080_CTRL_VOLT_VOLT_RAILS_CONTROL_PARAMS railsControlParams;
    railsControlParams.super.objMask.super.pData[0] = m_VoltRailMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    limitsuV->clear();
    for (set<Gpu::SplitVoltageDomain>::const_iterator domItr = m_AvailableDomains.begin();
         domItr != m_AvailableDomains.end(); ++domItr)
    {
        const UINT32 railIdx = GpuSplitVoltageDomainToRailIdx(*domItr);
        MASSERT(railIdx != ~0U);

        // Copy voltDeltauV[] from 0 to NUM_VOLTAGE_LIMITS - 1
        (*limitsuV)[*domItr].insert((*limitsuV)[*domItr].end(),
            &railsControlParams.rails[railIdx].voltDeltauV[0],
            &railsControlParams.rails[railIdx].voltDeltauV[Volt3x::NUM_VOLTAGE_LIMITS]);
    }

    return rc;
}

RC Volt3x::ClearRailLimitOffsets()
{
    RC rc;
    LW2080_CTRL_VOLT_VOLT_RAILS_CONTROL_PARAMS railsControlParams;
    railsControlParams.super.objMask.super.pData[0] = m_VoltRailMask;

    // Needed to populate the boardobj data for each rail in
    // railControlParams.rails[]
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    // Set every offset for each rail to 0 uV
    UINT32 railMask = m_VoltRailMask;
    INT32 railIdx;
    while ((railIdx = Utility::BitScanForward(railMask)) != -1)
    {
        // Set each entry in voltDeltauV to 0
        memset(&railsControlParams.rails[railIdx].voltDeltauV[0], 0,
               sizeof(railsControlParams.rails[railIdx].voltDeltauV));
        railMask ^= BIT(railIdx);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_RAILS_SET_CONTROL
       ,&railsControlParams
       ,sizeof(railsControlParams)));

    m_HasActiveVoltageLimitOffsets = false;

    return rc;
}

RC Volt3x::GetCachedRailLimitOffsetsuV
(
    const Gpu::SplitVoltageDomain voltDom,
    const VoltageLimit voltLimit,
    INT32 *pOffsetuV
) const
{
    MASSERT(pOffsetuV);
    *pOffsetuV = 0;

    if (!HasDomain(voltDom))
    {
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    const UINT32 railIdx = GpuSplitVoltageDomainToRailIdx(voltDom);
    MASSERT(railIdx != ~0U);
    *pOffsetuV = m_CachedRailsCtrl.rails[railIdx].voltDeltauV[voltLimit];

    return RC::OK;
}

RC Volt3x::GetVoltPoliciesStatus
(
    map<UINT32, LW2080_CTRL_VOLT_VOLT_POLICY_STATUS>& statuses
) const
{
    RC rc;

    LW2080_CTRL_VOLT_VOLT_POLICIES_STATUS_PARAMS policyStatusParams = {};
    policyStatusParams.super.objMask.super.pData[0] = m_VoltPolicyMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_GET_STATUS
       ,&policyStatusParams
       ,sizeof(policyStatusParams)));

    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &policyStatusParams.super.objMask.super)
        statuses[ii] = policyStatusParams.policies[ii];
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Volt3x::GetVoltageMv(Gpu::SplitVoltageDomain voltDom, UINT32 *pVoltMv)
{
    RC rc;
    MASSERT(pVoltMv);
    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = {};
    CHECK_RC(GetVoltRailStatusViaId(voltDom, &railStatus));

    *pVoltMv = railStatus.lwrrVoltDefaultuV / 1000;
    return rc;
}

RC Volt3x::GetCoreSensedVoltageUv(Gpu::SplitVoltageDomain voltDom, UINT32 *pVoltUv)
{
    RC rc;
    MASSERT(pVoltUv);
    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = {};
    CHECK_RC(GetVoltRailStatusViaId(voltDom, &railStatus));

    *pVoltUv = railStatus.lwrrVoltSenseduV;
    return rc;    
}

RC Volt3x::SetVoltagesuV(const vector<VoltageSetting>& voltages)
{
    MASSERT(!voltages.empty());

    // If the RM perf arbiter is disabled, we need to use the change sequencer
    // RMCTRL APIs to set the voltages instead
    if (!m_pSubdevice->GetPerf()->GetArbiterEnabled())
    {
        return InjectVoltagesuV(voltages);
    }

    RC rc;

    for (vector<VoltageSetting>::const_iterator voltItr = voltages.begin();
         voltItr != voltages.end();
         ++voltItr)
    {
        // Make sure the domain whose voltage we are setting actually exists
        if (!HasDomain(voltItr->voltDomain))
        {
            Printf(Tee::PriError, "Cannot offset voltage limit on %s rail\n",
                   PerfUtil::SplitVoltDomainToStr(voltItr->voltDomain));
            return RC::ILWALID_VOLTAGE_DOMAIN;
        }

        // Retrieve the correct limit input for this voltage domain
        LW2080_CTRL_PERF_LIMIT_INPUT* pInputLimit;
        CHECK_RC(GetVoltLimitByVoltDomain(voltItr->voltDomain, &pInputLimit));

        // Apply the values/type for the limit
        pInputLimit->type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE; // enable the limit
        pInputLimit->data.volt.info.data.logical.logicalVoltageuV = voltItr->voltuV;
        pInputLimit->data.volt.info.lwrrTargetVoltageuV = voltItr->voltuV;
    }

    CHECK_RC(SetVoltageLimits());
    m_HasActiveVoltageLimits = true;

    return rc;
}

RC Volt3x::InjectVoltagesuV(const vector<VoltageSetting>& voltages)
{
    MASSERT(!voltages.empty());

    Perf::InjectedPerfPoint injPP;
    injPP.voltages.reserve(voltages.size());
    for (const auto& voltSetting : voltages)
    {
        Perf::SplitVoltageSetting splitVoltSetting;
        splitVoltSetting.domain = voltSetting.voltDomain;
        splitVoltSetting.voltMv = voltSetting.voltuV / 1000;
        injPP.voltages.push_back(splitVoltSetting);
    }
    return m_pSubdevice->GetPerf()->InjectPerfPoint(injPP);
}

RC Volt3x::ClearSetVoltage(Gpu::SplitVoltageDomain voltDom)
{
    RC rc;

    // Make sure the voltage domain exists
    if (!HasDomain(voltDom))
    {
        Printf(Tee::PriError, "Cannot clear voltage on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(voltDom));
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    // Retrieve the correct limit input for this voltage domain
    LW2080_CTRL_PERF_LIMIT_INPUT* pInputLimit;
    CHECK_RC(GetVoltLimitByVoltDomain(voltDom, &pInputLimit));
    pInputLimit->type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;

    return SetVoltageLimits();
}

RC Volt3x::ClearAllSetVoltages()
{
    RC rc;

    for (vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS>::iterator voltLimitItr = m_VoltLimits.begin();
         voltLimitItr != m_VoltLimits.end();
         ++voltLimitItr)
    {
        voltLimitItr->input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;
    }

    CHECK_RC(SetVoltageLimits());
    m_HasActiveVoltageLimits = false;

    return rc;
}

RC Volt3x::GetVoltageViaStatus
(
    JSObject *pJsObj,
    const vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS>& statuses
)
{
    MASSERT(statuses.size() == 2);

    RC rc;

    // Volt40 - Limit IDs for SRAM has been repurposed for MSVDD
    // SRAM now corresponds to MSVDD on supporting Voltage Domain HAL
    // LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS returns an array with indexes
    // mapped to railmask. Pass index 0/1 to make it compatiable with MSVDD/SRAM
    UINT64 logicVoltage = 0;
    UINT64 sramVoltage  = 0;
    JavaScriptPtr pJs;

    // Rail Index 0
    switch (statuses[0].input.type)
    {
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE:
            logicVoltage = statuses[0].input.data.volt.info.lwrrTargetVoltageuV;
            break;
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE_3X:
            logicVoltage = statuses[0].input.data.volt3x.lwrrTargetVoltageuV;
            break;
        default:
            logicVoltage = 0;
            break;
    }

    // Rail Index 1
    switch (statuses[1].input.type)
    {
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE:
            sramVoltage = statuses[1].input.data.volt.info.lwrrTargetVoltageuV;
            break;
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE_3X:
            sramVoltage = statuses[1].input.data.volt3x.lwrrTargetVoltageuV;
            break;
        default:
            sramVoltage  = 0;
            break;
    }

    CHECK_RC(pJs->SetElement(pJsObj, Gpu::VOLTAGE_LOGIC, logicVoltage));
    if (HasDomain(Gpu::VOLTAGE_MSVDD))
    {
        CHECK_RC(pJs->SetElement(pJsObj, Gpu::VOLTAGE_MSVDD, sramVoltage));
    }
    else if (HasDomain(Gpu::VOLTAGE_SRAM))
    {
        CHECK_RC(pJs->SetElement(pJsObj, Gpu::VOLTAGE_SRAM, sramVoltage));
    }

    return rc;
}

RC Volt3x::GetLwrrOverVoltLimitsuVToJs
(
    JSObject *pJsObj
)
{
    RC rc;

    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
    statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_OVERVOLTAGE_LOGIC;
    statuses[1].limitId = IsPascalSplitRailSupported() ?
                            LW2080_CTRL_PERF_LIMIT_OVERVOLTAGE_SRAM:
                            LW2080_CTRL_PERF_LIMIT_OVERVOLTAGE_MSVDD;
    CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice));
    CHECK_RC(GetVoltageViaStatus(pJsObj, statuses));

    return rc;
}

RC Volt3x::GetLwrrVoltRelLimitsuVToJs
(
    JSObject *pJsObj
)
{
    RC rc;

    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
    statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_RELIABILITY_LOGIC;
    statuses[1].limitId = IsPascalSplitRailSupported() ?
                            LW2080_CTRL_PERF_LIMIT_RELIABILITY_SRAM:
                            LW2080_CTRL_PERF_LIMIT_RELIABILITY_MSVDD;
    CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice));
    CHECK_RC(GetVoltageViaStatus(pJsObj, statuses));

    return rc;
}

RC Volt3x::GetLwrrAltVoltRelLimitsuVToJs
(
    JSObject *pJsObj
)
{
    RC rc;

    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
    statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_RELIABILITY_ALT_LOGIC;
    statuses[1].limitId = IsPascalSplitRailSupported() ?
                            LW2080_CTRL_PERF_LIMIT_RELIABILITY_ALT_SRAM:
                            LW2080_CTRL_PERF_LIMIT_RELIABILITY_ALT_MSVDD;
    CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice));
    CHECK_RC(GetVoltageViaStatus(pJsObj, statuses));

    return rc;
}

RC Volt3x::GetLwrrVminLimitsuVToJs
(
    JSObject *pJsObj
)
{
    RC rc;

    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
    statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_VMIN_LOGIC;
    statuses[1].limitId = IsPascalSplitRailSupported() ?
                            LW2080_CTRL_PERF_LIMIT_VMIN_SRAM:
                            LW2080_CTRL_PERF_LIMIT_VMIN_MSVDD;
    CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice));
    CHECK_RC(GetVoltageViaStatus(pJsObj, statuses));

    return rc;
}

RC Volt3x::GetRamAssistInfos
(
    map<Gpu::SplitVoltageDomain, Volt3x::RamAssistType> *pRamAssistTypes
)
{
    RC rc;
    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRailsInfo.super.objMask.super)
        UINT32 type = m_CachedRailsInfo.rails[i].super.type;
        Gpu::SplitVoltageDomain dom = RmVoltDomToGpuSplitVoltageDomain(type);
        (*pRamAssistTypes)[dom] = static_cast<Volt3x::RamAssistType>
            (
                m_CachedRailsInfo.rails[i].ramAssist.type
            );
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

bool Volt3x::IsRamAssistSupported()
{
    UINT32 i;
    bool ramType = false;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRailsInfo.super.objMask.super)
        UINT32 type = m_CachedRailsInfo.rails[i].ramAssist.type;
        if (static_cast<Volt3x::RamAssistType>(type) != RamAssistType::RAM_ASSIST_TYPE_DISABLED)
        {
            ramType = true;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return ramType;
}

void Volt3x::PrintSingleVoltRailInfo
(
    LW2080_CTRL_VOLT_VOLT_RAIL_INFO &railInfo
)
{
    Printf(Tee::PriDebug,
           "Voltage rail info: type = 0x%x\n"
           " bootVoltageuV = %d\n"
           " relLimitVfeEquIdx = %d\n"
           " altRelLimitVfeEquIdx = %d\n"
           " ovLimitVfeEquIdx = %d\n"
           " voltDevIdxDefault = %d\n"
           " voltDevMask = 0x%x\n",
           railInfo.super.type,
           railInfo.bootVoltageuV,
           railInfo.relLimitVfeEquIdx,
           railInfo.altRelLimitVfeEquIdx,
           railInfo.ovLimitVfeEquIdx,
           railInfo.voltDevIdxDefault,
           railInfo.voltDevMask.super.pData[0]
          );
}

void Volt3x::PrintSingleVoltDeviceInfo
(
    LW2080_CTRL_VOLT_VOLT_DEVICE_INFO &devInfo
)
{
    constexpr auto pri = Tee::PriDebug;
    Printf(pri,
           "Voltage device info: type = 0x%x\n"
           " voltDomain = 0x%x\n"
           " i2cDevIdx = %d\n"
           " operationType = 0x%x\n"
           " voltageMinuV = %d\n"
           " voltageMaxuV = %d\n"
           " voltStepuV = %d\n",
           devInfo.super.type,
           devInfo.voltDomain,
           devInfo.i2cDevIdx,
           devInfo.operationType,
           devInfo.voltageMinuV,
           devInfo.voltageMaxuV,
           devInfo.voltStepuV);

    switch (devInfo.super.type)
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_VID:
        {
            LW2080_CTRL_VOLT_VOLT_DEVICE_INFO_DATA_VID &vidInfo = devInfo.data.vid;
            Printf(pri,
                   "  VID volt device\n"
                   "  vselMask = 0x%x\n"
                   "  vidMin = %d\n"
                   "  vidMax = %d\n"
                   "  voltageBaseuV = %d\n"
                   "  voltageOffsetScaleuV = %d\n",
                   vidInfo.vselMask,
                   vidInfo.vidMin,
                   vidInfo.vidMax,
                   vidInfo.voltageBaseuV,
                   vidInfo.voltageOffsetScaleuV);
            Printf(pri,
                   "  vidToVsel | gpioPin | vselFuncTbl\n");
            for (UINT32 i = 0; i < LW2080_CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES; ++i)
            {
                Printf(pri,
                       "   %3d   |  %3d    | %3d\n",
                       vidInfo.vidToVselMapping[i],
                       vidInfo.gpioPin[i],
                       vidInfo.vselFunctionTable[i]);
            }
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
        {
            LW2080_CTRL_VOLT_VOLT_DEVICE_INFO_DATA_PWM &pwmInfo = devInfo.data.pwm;
            Printf(pri,
                   "  PWM volt device\n"
                   "  voltageBaseuV = %d\n"
                   "  voltageOffsetScaleuV = %d\n"
                   "  source = %d\n"
                   "  rawPeriod = %d\n",
                   pwmInfo.voltageBaseuV,
                   pwmInfo.voltageOffsetScaleuV,
                   pwmInfo.source,
                   pwmInfo.rawPeriod);
            break;
        }
        default:
            Printf(Tee::PriError,
                   "Invalid VOLT_DEVICE type 0x%x\n",
                   devInfo.super.type);
            MASSERT(0);
            break;
    }
}

void Volt3x::PrintSingleVoltPolicyInfo
(
    LW2080_CTRL_VOLT_VOLT_POLICY_INFO &policyInfo,
    LW2080_CTRL_VOLT_VOLT_POLICY_STATUS &policyStatus
)
{
    constexpr auto pri = Tee::PriDebug;
    Printf(pri,
           "Voltage policy info: type = 0x%x\n",
           policyInfo.super.type);
    switch (policyInfo.super.type)
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            Printf(pri, "Single rail policy\n");
            Printf(pri, " railIdx = %d\n", policyInfo.data.singleRail.railIdx);
            Printf(pri, " lwrrVoltuV = %d\n", policyStatus.data.singleRail.lwrrVoltuV);
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            Printf(pri, "Single rail multi-step policy\n");
            Printf(pri, " railIdx = %u\n", policyInfo.data.singleRailMS.super.railIdx);
            Printf(pri, " rampUpStepSizeuV = %u\n",
                   policyInfo.data.singleRailMS.rampUpStepSizeuV);
            Printf(pri, " rampDownStepSizeuV = %u\n",
                   policyInfo.data.singleRailMS.rampDownStepSizeuV);
            Printf(pri, " lwrrVoltuV = %d\n", policyStatus.data.singleRailMS.super.lwrrVoltuV);
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL:
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            LW2080_CTRL_VOLT_VOLT_POLICY_INFO_DATA_SPLIT_RAIL &splitInfo = policyInfo.data.splitRail;
            LW2080_CTRL_VOLT_VOLT_POLICY_STATUS_DATA_SPLIT_RAIL &splitStatus = policyStatus.data.splitRail;
            Printf(pri,
                   "Split rail policy\n"
                   " railIdxMaster = %d\n"
                   " railIdxSlave = %d\n"
                   " deltaMilwfeEquIdx = %u\n"
                   " deltaMaxVfeEquIdx = %u\n"
                   " origDeltaMinUv = %d\n"
                   " origDeltaMaxUv = %d\n"
                   " deltaMinUv = %d\n"
                   " deltaMaxUv = %d\n",
                   splitInfo.railIdxMaster,
                   splitInfo.railIdxSlave,
                   splitInfo.deltaMilwfeEquIdx,
                   splitInfo.deltaMaxVfeEquIdx,
                   splitStatus.origDeltaMinuV,
                   splitStatus.origDeltaMaxuV,
                   splitStatus.deltaMinuV,
                   splitStatus.deltaMaxuV);
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            // TODO - Dump Policy Info. We need to see is that is useful
            // For Multi Rail it has voltRailMask
            // LW2080_CTRL_VOLT_VOLT_POLICY_INFO_DATA_MULTI_RAIL &multiInfo = policyInfo.data.multiRail;

            LW2080_CTRL_VOLT_VOLT_POLICY_STATUS_DATA_MULTI_RAIL &multiStatus = policyStatus.data.multiRail;
            for (UINT08 idx = 0; idx < multiStatus.numRails; idx++)
            {
                Printf(pri,
                   "Multi Rail Policy Status ** idx = %d ***\n"
                   " railIdx = %d\n"
                   " Current Voltage = %d\n",
                   idx,
                   multiStatus.railItems[idx].railIdx,
                   multiStatus.railItems[idx].lwrrVoltuV);
            }
            break;
        }
        default:
            Printf(Tee::PriError,
                   "Invalid VOLT_POLICY type 0x%x\n",
                   policyInfo.super.type);
            MASSERT(0);
            break;
    }
}

RC Volt3x::SetInterVoltSettleTimeUs(UINT16 timeUs)
{
    RC rc;
    LW2080_CTRL_VOLT_VOLT_POLICIES_CONTROL_PARAMS voltControl = {};
    voltControl.super.objMask.super.pData[0] = m_VoltPolicyMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_GET_CONTROL
       ,&voltControl
       ,sizeof(voltControl)));

    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &voltControl.super.objMask.super)
        // InterVoltSettleTime is the amount of time RM waits between changing
        // the voltage on each rail. This behavior is only applicable to the
        // multi-step voltage policy.
        if (voltControl.policies[i].super.type ==
            LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP)
        {
            voltControl.policies[i].data.splitRailMS.interSwitchDelayus = timeUs;
        }
        else if (voltControl.policies[i].super.type ==
            LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP)
        {
            voltControl.policies[i].data.singleRailMS.interSwitchDelayus = timeUs;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_SET_CONTROL
       ,&voltControl
       ,sizeof(voltControl)));

    return rc;
}

RC Volt3x::SetSplitRailConstraintAuto(bool enable)
{
    RC rc;

    if (!IsPascalSplitRailSupported())
    {
        Printf(Tee::PriError,
               "Split voltage rails are not supported on GPU %d subdev %d\n",
               m_pSubdevice->GetGpuInst(),
               m_pSubdevice->GetSubdeviceInst());
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (enable)
        CHECK_RC(LazyInstantiateSRConstMgr());

    m_SplitRailConstraintAuto = enable;

    return rc;
}

RC Volt3x::SetSplitRailConstraintOffsetuV(bool bMax, INT32 OffsetInuV)
{
    RC rc;

    if (!IsPascalSplitRailSupported())
    {
        Printf(Tee::PriError,
               "Split voltage rails are not supported on GPU %d subdev %d\n",
               m_pSubdevice->GetGpuInst(),
               m_pSubdevice->GetSubdeviceInst());
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(LazyInstantiateSRConstMgr());
    if (bMax)
    {
        CHECK_RC(m_pSplitRailConstraintManager->SetConstraintOffsetsuV(
                    false, true, 0, OffsetInuV));
    }
    else
    {
        CHECK_RC(m_pSplitRailConstraintManager->SetConstraintOffsetsuV(
                    true, false, OffsetInuV, 0));
    }

    return rc;
}

RC Volt3x::GetSplitRailConstraintuV(INT32* min, INT32* max)
{
    RC rc;
    if (!IsPascalSplitRailSupported())
    {
        Printf(Tee::PriError,
               "Split voltage rails are not supported on GPU %d subdev %d\n",
               m_pSubdevice->GetGpuInst(),
               m_pSubdevice->GetSubdeviceInst());
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(LazyInstantiateSRConstMgr());
    CHECK_RC(m_pSplitRailConstraintManager->GetConstraintuV(min, max));
    return rc;
}

RC Volt3x::VoltageDomainToLimitId(Gpu::SplitVoltageDomain voltDom, UINT32 *pLimitId)
{
    MASSERT(pLimitId);
    RC rc;
    switch (voltDom)
    {
        case Gpu::VOLTAGE_LOGIC:
        {
            *pLimitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOGIC;
            break;
        }
        case Gpu::VOLTAGE_SRAM:
        {
            *pLimitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_SRAM;
            break;
        }
        case Gpu::VOLTAGE_MSVDD:
        {
            *pLimitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_MSVDD;
            break;
        }
        default:
            return RC::BAD_PARAMETER;
    }
    return rc;
}

RC Volt3x::GetVoltLimitByVoltDomain
(
    Gpu::SplitVoltageDomain voltDom,
    LW2080_CTRL_PERF_LIMIT_INPUT** limit
)
{
    for (UINT32 ii = 0; ii < m_VoltLimits.size(); ii++)
    {
        if (m_VoltLimits[ii].input.data.volt.info.voltageDomain ==
                PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(voltDom))
        {
            *limit = &(m_VoltLimits[ii].input);
            return RC::OK;
        }
    }

    Printf(Tee::PriError, "voltage limit not found for domain = %d\n", voltDom);
    return RC::BAD_PARAMETER;
}

RC Volt3x::SetVoltageLimits()
{
    RC rc;

    LW2080_CTRL_PERF_LIMITS_SET_STATUS_V2_PARAMS limitParams = {0};
    limitParams.numLimits = UNSIGNED_CAST(LwU32, m_VoltLimits.size());

    bool atLeastOneLimitActive = false;
    for (UINT32 limitIdx = 0; limitIdx < m_VoltLimits.size(); limitIdx++)
    {
        MASSERT(limitIdx < LW2080_CTRL_PERF_MAX_LIMITS);

        limitParams.limitsList[limitIdx].limitId = m_VoltLimits[limitIdx].limitId;
        limitParams.limitsList[limitIdx].input = m_VoltLimits[limitIdx].input;

        if (m_VoltLimits[limitIdx].input.type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED)
        {
            continue;
        }
        else
        {
            atLeastOneLimitActive = true;
        }

        UINT32 targetVoltageUv = m_VoltLimits[limitIdx].input.data.volt.info.data.logical.logicalVoltageuV;
        Gpu::SplitVoltageDomain dom =
            RmVoltDomToGpuSplitVoltageDomain(
                m_VoltLimits[limitIdx].input.data.volt.info.voltageDomain);

        const string domStr = PerfUtil::SplitVoltDomainToStr(dom);
        if (!targetVoltageUv)
        {
            Printf(Tee::PriError, "Requested 0mV on %s rail\n", domStr.c_str());
            return RC::VOLTAGE_OUT_OF_RANGE;
        }

        // Scream if we are going beyond the original OV limit
        if (targetVoltageUv > m_OrigVoltLimitsuV[dom][OVERVOLTAGE_LIMIT] &&
            m_PrintVoltSafetyWarnings)
        {
            Printf(Tee::PriWarn, Tee::GetTeeModuleCoreCode(), Tee::SPS_FAIL,
                   "%umv IS ABOVE THE ORIGINAL OVER-VOLTAGE LIMIT ON THE %s RAIL (%umv)\n",
                   targetVoltageUv/1000,
                   Utility::ToUpperCase(domStr).c_str(),
                   m_OrigVoltLimitsuV[dom][OVERVOLTAGE_LIMIT]/1000);
        }
        if (targetVoltageUv > m_OrigVoltLimitsuV[dom][RELIABILITY_LIMIT] &&
            m_PrintVoltSafetyWarnings)
        {
            Printf(Tee::PriWarn, Tee::GetTeeModuleCoreCode(), Tee::SPS_WARNING,
                   "%umv is above the original reliability limit on the %s rail (%umv)\n",
                   targetVoltageUv/1000,
                   domStr.c_str(),
                   m_OrigVoltLimitsuV[dom][RELIABILITY_LIMIT]/1000);
        }
    }

    if (PerfUtil::s_LimitVerbosity != PerfUtil::NO_LIMITS)
    {
        // During shutdown Perf will get destroyed before Volt3x so
        // we cannot call PrintPerfLimits()
        if (m_pSubdevice->GetPerf())
        {
            CHECK_RC(m_pSubdevice->GetPerf()->PrintPerfLimits());
        }
        else
        {
            PrintVoltLimits();
        }
    }

    if (m_SplitRailConstraintAuto)
    {
        for (UINT32 ii = 0; ii < static_cast<UINT32>(Gpu::SplitVoltageDomain_NUM); ii++)
        {
            LW2080_CTRL_PERF_LIMIT_INPUT* pInputLimit;
            Gpu::SplitVoltageDomain dom = static_cast<Gpu::SplitVoltageDomain>(ii);
            if (!HasDomain(dom))
            {
                continue;
            }
            CHECK_RC(GetVoltLimitByVoltDomain(dom, &pInputLimit));
            if (pInputLimit->type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED)
            {
                m_pSplitRailConstraintManager->ResetVoltLimit(dom);
            }
            else
            {
                m_pSplitRailConstraintManager->ApplyVoltLimituV(dom,
                    pInputLimit->data.volt.info.lwrrTargetVoltageuV);
            }
        }
        CHECK_RC(m_pSplitRailConstraintManager->UpdateConstraint());
    }

    // Only warn users if they are setting a limit, not clearing them all
    if (atLeastOneLimitActive && m_PrintVoltSafetyWarnings)
    {
        UINT32 val = 0;
        if ((Registry::Read("ResourceManager", "RmVFPointCheckIgnore", &val) != OK) || !val)
        {
            Printf(Tee::PriWarn, Tee::GetTeeModuleCoreCode(), Tee::SPS_WARNING,
                   "***************************************************************\n"
                   "* WARNING                                             WARNING *\n"
                   "*                                                             *\n"
                   "* Setting voltage limits without setting RmVFPointCheckIgnore *\n"
                   "* rmkey may fail VF lwrve validation! If you know what you    *\n"
                   "* are doing and are willing to operate at what RM thinks is   *\n"
                   "* an unsafe VF point, add \"-rmkey RmVFPointCheckIgnore 1\"     *\n"
                   "* to your command line arguments to avoid this warning.       *\n"
                   "*                                                             *\n"
                   "* If you see any failure after this warning message, do NOT   *\n"
                   "* file a MODS or RM bug. You were attempting to operate at an *\n"
                   "* unsafe VF point and MODS/RM cannot guarantee stability.     *\n"
                   "*                                                             *\n"
                   "* WARNING                                             WARNING *\n"
                   "***************************************************************\n");

            // Make sure the user sees the above warning
            Tasker::Sleep(5000);
        }
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS_V2
       ,&limitParams
       ,sizeof(limitParams)));

    return rc;
}

RC Volt3x::LazyInstantiateSRConstMgr()
{
    if (!m_pSplitRailConstraintManager)
    {
        m_pSplitRailConstraintManager = new SplitRailConstraintManager(m_pSubdevice);
    }
    return m_pSplitRailConstraintManager->Initialize();
}

void Volt3x::LazyDeleteSRConstMgr()
{
    if (m_pSplitRailConstraintManager)
    {
        delete m_pSplitRailConstraintManager;
        m_pSplitRailConstraintManager = nullptr;
    }
}

UINT32 Volt3x::GpuSplitVoltageDomainToRailIdx(Gpu::SplitVoltageDomain dom) const
{
    for (const auto& voltRailInfo : m_VoltRailInfos)
    {
        if (voltRailInfo.voltDom == dom)
        {
            return voltRailInfo.railIdx;
        }
    }
    MASSERT(!"Invalid voltage domain");
    return ~0U;
}

Gpu::SplitVoltageDomain Volt3x::RailIdxToGpuSplitVoltageDomain(UINT32 railIdx) const
{
    for (const auto& voltRailInfo : m_VoltRailInfos)
    {
        if (voltRailInfo.railIdx == railIdx)
        {
            return voltRailInfo.voltDom;
        }
    }
    MASSERT(!"Invalid voltage rail index");
    return Gpu::ILWALID_SplitVoltageDomain;
}

RC Volt3x::GetRailLimitVfeEquIdx
(
    const Gpu::SplitVoltageDomain dom,
    const VoltageLimit lim,
    UINT32* pVfeEquIdx
) const
{
    MASSERT(pVfeEquIdx);
    *pVfeEquIdx = ~UINT32(0);

    if (!HasDomain(dom))
    {
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    const UINT32 railIdx = GpuSplitVoltageDomainToRailIdx(dom);
    MASSERT(railIdx != ~0U);

    switch (lim)
    {
        case RELIABILITY_LIMIT:
            *pVfeEquIdx = m_CachedRailsInfo.rails[railIdx].relLimitVfeEquIdx;
            break;
        case ALT_RELIABILITY_LIMIT:
            *pVfeEquIdx = m_CachedRailsInfo.rails[railIdx].altRelLimitVfeEquIdx;
            break;
        case OVERVOLTAGE_LIMIT:
            *pVfeEquIdx = m_CachedRailsInfo.rails[railIdx].ovLimitVfeEquIdx;
            break;
        case VMIN_LIMIT:
            *pVfeEquIdx = m_CachedRailsInfo.rails[railIdx].vminLimitVfeEquIdx;
            break;
        default:
            MASSERT(!"unknown voltage limit");
            return RC::BAD_PARAMETER;
    }

    return OK;
}

Gpu::SplitVoltageDomain Volt3x::RmVoltDomToGpuSplitVoltageDomain
(
    UINT08 RmVoltDomain
)
{

   switch (m_CachedRailsInfo.voltDomainHAL)
   {
       case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GP10X_SINGLE_RAIL:
       {
           switch (RmVoltDomain)
           {
               case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
                   return Gpu::VOLTAGE_LOGIC;
           }
           break;
       }
       case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GP10X_SPLIT_RAIL:
       {
           switch (RmVoltDomain)
           {
               case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
                   return Gpu::VOLTAGE_LOGIC;
               case LW2080_CTRL_VOLT_VOLT_DOMAIN_SRAM:
                   return Gpu::VOLTAGE_SRAM;
            }
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GA10X_MULTI_RAIL:
        {
           switch (RmVoltDomain)
           {
               case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
                   return Gpu::VOLTAGE_LOGIC;
               case LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD:
                   return Gpu::VOLTAGE_MSVDD;
            }
            break;
        }
    }
    return Gpu::ILWALID_SplitVoltageDomain;
}

// SplitRailConstraintManager --------------------------------------------------

SplitRailConstraintManager::SplitRailConstraintManager(GpuSubdevice* pGpuSub) :
    m_pGpuSub(pGpuSub)
{
    lwrrVoltagesuV[Gpu::VOLTAGE_LOGIC] = 0;
    lwrrVoltagesuV[Gpu::VOLTAGE_SRAM] = 0;
    newVoltagesuV[Gpu::VOLTAGE_LOGIC] = 0;
    newVoltagesuV[Gpu::VOLTAGE_SRAM] = 0;
}

RC SplitRailConstraintManager::Initialize()
{
    RC rc;

    CHECK_RC(GetConstraintuV(&origConstraints.minuV, &origConstraints.maxuV));

    return rc;
}

RC SplitRailConstraintManager::Cleanup()
{
    // Reset the split-rail constraint min/max to the original values
    return SetConstraintOffsetsuV(true, true, 0, 0);
}

RC SplitRailConstraintManager::GetConstraintuV(INT32* pMinuV, INT32* pMaxuV)
{
    RC rc;

    LW2080_CTRL_VOLT_VOLT_POLICIES_STATUS_PARAMS voltStatus = {};
    voltStatus.super.objMask.super.pData[0] = m_pGpuSub->GetVolt3x()->GetVoltPolicyMask();
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pGpuSub
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_GET_STATUS
       ,&voltStatus
       ,sizeof(voltStatus)));

    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &voltStatus.super.objMask.super)
        if (voltStatus.policies[ii].super.type ==
                LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP ||
            voltStatus.policies[ii].super.type ==
                LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP)
        {
            *pMinuV = voltStatus.policies[ii].data.splitRail.deltaMinuV;
            *pMaxuV = voltStatus.policies[ii].data.splitRail.deltaMaxuV;
            return OK;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC SplitRailConstraintManager::SetConstraintOffsetsuV
(
    bool bMin,
    bool bMax,
    INT32 MinOffsetuV,
    INT32 MaxOffsetuV
)
{
    RC rc;

    UINT32 policyMask = m_pGpuSub->GetVolt3x()->GetVoltPolicyMask();

    LW2080_CTRL_VOLT_VOLT_POLICIES_CONTROL_PARAMS voltControl = {};
    voltControl.super.objMask.super.pData[0] = policyMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pGpuSub
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_GET_CONTROL
       ,&voltControl
       ,sizeof(voltControl)));

    policyMask = 0;

    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &voltControl.super.objMask.super)
        if (voltControl.policies[ii].super.type ==
                LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP ||
            voltControl.policies[ii].super.type ==
                LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP)
        {
            if (bMin)
            {
                voltControl.policies[ii].data.splitRail.offsetDeltaMinuV = MinOffsetuV;
            }
            if (bMax)
            {
                voltControl.policies[ii].data.splitRail.offsetDeltaMaxuV = MaxOffsetuV;
            }
            policyMask |= BIT(ii);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    voltControl.super.objMask.super.pData[0] = policyMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pGpuSub
       ,LW2080_CTRL_CMD_VOLT_VOLT_POLICIES_SET_CONTROL
       ,&voltControl
       ,sizeof(voltControl)));

    return rc;
}

RC SplitRailConstraintManager::UpdateConstraint()
{
    RC rc;

    if (UsingTwoVoltageLimits())
    {
        return UpdateConstraintFromVoltLimits();
    }

    ConstraintPair newOffsetsCP = railOffsetCP + railClkDomOffsetCP + vfpOffsetCP;
    CHECK_RC(ActuallyUpdateConstraint(lwrrOffsetsCP, newOffsetsCP));

    return rc;
}

// This function assumes there are limits being set on BOTH
// voltage rails. Any offsets will not get applied when using
// MODS_RULES to force voltages
RC SplitRailConstraintManager::UpdateConstraintFromVoltLimits()
{
    RC rc;

    ConstraintPair newOffsetsCP;
    if (newVoltagesuV[Gpu::VOLTAGE_SRAM] < newVoltagesuV[Gpu::VOLTAGE_LOGIC])
    {
        newOffsetsCP.minuV = newVoltagesuV[Gpu::VOLTAGE_SRAM] - newVoltagesuV[Gpu::VOLTAGE_LOGIC];
    }
    else
    {
        newOffsetsCP.maxuV = newVoltagesuV[Gpu::VOLTAGE_SRAM] - newVoltagesuV[Gpu::VOLTAGE_LOGIC];
    }

    CHECK_RC(ActuallyUpdateConstraint(lwrrOffsetsCP, newOffsetsCP));
    lwrrVoltagesuV = newVoltagesuV;
    lwrrOffsetsCP = newOffsetsCP;

    return rc;
}

RC SplitRailConstraintManager::ActuallyUpdateConstraint
(
    ConstraintPair& lwrrOffsetsCP,
    ConstraintPair& newOffsetsCP
)
{
    RC rc;

    LwU32 stepSizeuV = 0;
    LwU32 maxStepSizeuV = 0;
    set<Gpu::SplitVoltageDomain> voltDoms = m_pGpuSub->GetVolt3x()->GetAvailableDomains();
    for (set<Gpu::SplitVoltageDomain>::const_iterator voltDomItr = voltDoms.begin();
         voltDomItr != voltDoms.end();
         ++voltDomItr)
    {
        CHECK_RC(m_pGpuSub->GetVolt3x()->GetVoltRailRegulatorStepSizeuV(
                *voltDomItr, &stepSizeuV));
        if (stepSizeuV > maxStepSizeuV)
            maxStepSizeuV = stepSizeuV;
    }

    // For safety, decrease the constraint min by one regulator
    // step size as a WAR to an RM arbiter rounding issue
    // (see http://lwbugs/1664327/35)
    ConstraintPair regulatorOffsetCP;
    if (newOffsetsCP.minuV != lwrrOffsetsCP.minuV)
        regulatorOffsetCP.minuV = -static_cast<INT32>(maxStepSizeuV);
    if (newOffsetsCP.maxuV != lwrrOffsetsCP.maxuV)
        regulatorOffsetCP.maxuV = static_cast<INT32>(maxStepSizeuV);

    newOffsetsCP += regulatorOffsetCP;

    // Make sure we are not increasing the min nor
    // decreasing the max
    if (lwrrOffsetsCP.minuV < newOffsetsCP.minuV)
        newOffsetsCP.minuV = lwrrOffsetsCP.minuV;
    if (lwrrOffsetsCP.maxuV > newOffsetsCP.maxuV)
        newOffsetsCP.maxuV = lwrrOffsetsCP.maxuV;

    CHECK_RC(SetConstraintOffsetsuV(true, true,
                                    newOffsetsCP.minuV,
                                    newOffsetsCP.maxuV));

    lwrrOffsetsCP = newOffsetsCP;

    return rc;
}

RC SplitRailConstraintManager::ApplyVoltLimituV
(
    Gpu::SplitVoltageDomain dom,
    INT32 voltageuV
)
{
    RC rc;
    INT32 minuV = 0;
    INT32 maxuV = 0;
    CHECK_RC(GetOffsetEffectuV(dom, voltageuV, &minuV, &maxuV));
    voltLimitsCP += ConstraintPair(minuV, maxuV);
    newVoltagesuV[dom] = voltageuV;
    return rc;
}

void SplitRailConstraintManager::ResetVoltLimit(Gpu::SplitVoltageDomain dom)
{
    lwrrVoltagesuV[dom] = 0;
    newVoltagesuV[dom] = 0;
}

RC SplitRailConstraintManager::ApplyRailOffsetuV
(
    Gpu::SplitVoltageDomain dom,
    INT32 offsetuV
)
{
    RC rc;
    INT32 minuV = 0;
    INT32 maxuV = 0;
    CHECK_RC(GetOffsetEffectuV(dom, offsetuV, &minuV, &maxuV));
    railOffsetCP += ConstraintPair(minuV, maxuV);
    return rc;
}

RC SplitRailConstraintManager::ApplyClkDomailwoltOffsetuV
(
    Gpu::SplitVoltageDomain dom,
    INT32 offsetuV
)
{
    RC rc;
    INT32 minuV = 0;
    INT32 maxuV = 0;
    CHECK_RC(GetOffsetEffectuV(dom, offsetuV, &minuV, &maxuV));
    railClkDomOffsetCP += ConstraintPair(minuV, maxuV);
    return rc;
}

void SplitRailConstraintManager::ApplyVfPointOffsetuV(INT32 offsetuV)
{
    if (abs(offsetuV) > vfpOffsetCP.maxuV)
        vfpOffsetCP.maxuV = offsetuV;
    if (offsetuV < vfpOffsetCP.minuV)
        vfpOffsetCP.minuV = offsetuV;
}

bool SplitRailConstraintManager::UsingTwoVoltageLimits()
{
    return newVoltagesuV[Gpu::VOLTAGE_LOGIC] && newVoltagesuV[Gpu::VOLTAGE_SRAM];
}

RC SplitRailConstraintManager::GetOffsetEffectuV
(
    Gpu::SplitVoltageDomain dom,
    INT32 offsetuV,
    INT32* pMinuV,
    INT32* pMaxuV
)
{
    if (dom == Gpu::VOLTAGE_LOGIC)
    {
        if (offsetuV > 0)
            *pMinuV -= offsetuV;
        else
            *pMaxuV -= offsetuV;
    }
    else if (dom == Gpu::VOLTAGE_SRAM)
    {
        if (offsetuV > 0)
            *pMaxuV += offsetuV;
        else
            *pMinuV += offsetuV;
    }
    else
    {
        Printf(Tee::PriError, "Unknown voltage domain %u\n", static_cast<UINT32>(dom));
        return RC::BAD_PARAMETER;
    }

    return OK;
}

SplitRailConstraintManager::ConstraintPair SplitRailConstraintManager::ConstraintPair::operator+
(
    const ConstraintPair& other
)
{
    ConstraintPair result;

    if (this->minuV < 0 && other.minuV < 0)
        result.minuV = this->minuV + other.minuV;
    else
        result.minuV = min(this->minuV, other.minuV);

    if (this->maxuV > 0 && other.maxuV > 0)
        result.maxuV = this->maxuV + other.maxuV;
    else
        result.maxuV = max(this->maxuV, other.maxuV);

    return result;
}

SplitRailConstraintManager::ConstraintPair SplitRailConstraintManager::ConstraintPair::operator+=
(
    const ConstraintPair& other
)
{
    return *this = *this + other;
}
