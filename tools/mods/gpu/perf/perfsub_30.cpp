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

#include "gpu/perf/perfsub_30.h"

#include "gpu/include/boarddef.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "core/include/utility.h"
#include "gpu/perf/voltsub.h"
#include "gpu/perf/avfssub.h"
#include "gpu/perf/perfutil.h"
#include "core/include/fuseutil.h"
#include "core/include/fuseparser.h"
#include "gpu/fuse/fuse.h"

using namespace PerfUtil;

// Because JS_NewObject with a NULL class is unsafe in JS 1.4.
JS_CLASS(PerfDummy);

// List of limits used across all the pstates that will be automatically
// disabled by default when switching to a new pstate:
static const UINT32 PerfLimitsIDs[] =
{
    // CLIENT_LOW limits are useful for display tests to guarantee that IMP
    // requirements are not violated because their priority is below IMP
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_INTERSECT,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PSTATE_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PSTATE_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DRAM_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DRAM_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_GPC_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_GPC_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DISP_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DISP_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PCIE_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PCIE_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_1_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_1_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_2_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_2_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_3_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_3_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_4_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_4_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_5_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_5_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_6_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_6_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_7_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_7_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_8_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_8_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_9_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_9_MAX,

    // CLIENT limits are the default behavior for MODS in PStates 3.0
    // and are higher priority than IMP but lower than power-capping
    LW2080_CTRL_PERF_LIMIT_INTERSECT,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PSTATE_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PSTATE_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DRAM_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DRAM_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_GPC_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_GPC_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DISP_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DISP_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PCIE_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PCIE_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_0_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_0_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_1_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_1_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_2_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_2_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_3_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_3_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_4_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_4_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_5_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_5_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_6_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_6_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_7_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_7_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_8_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_8_MAX,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_9_MIN,
    LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_9_MAX,

    // MODS_RULES limits have the highest priority and take precedence
    // over both power-capping and over-voltage/reliability limits
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_INTERSECT,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PSTATE_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PSTATE_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DRAM_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DRAM_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_GPC_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_GPC_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DISP_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DISP_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PCIE_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PCIE_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_0_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_0_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_1_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_1_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_2_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_2_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_3_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_3_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_4_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_4_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_5_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_5_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_6_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_6_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_7_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_7_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_8_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_8_MAX,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_9_MIN,
    LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_9_MAX,
};

// The number of available MAX _or_ MIN limits:
static constexpr UINT32 NUM_LOOSE_FREQ_LOCK_LIMITS = 10;

Perf30::Perf30(GpuSubdevice *pSubdevice, bool owned) :
    Perf(pSubdevice, owned)
, m_OverrideOVOC(true)
, m_HasPowerCapping(false)
, m_ShouldHavePowerCapping(true)
{
    memset(&m_RailVoltTuningOffsetsuV, 0, sizeof(m_RailVoltTuningOffsetsuV));
    memset(&m_ClkDomainsControl, 0, sizeof(m_ClkDomainsControl));

    LW2080_CTRL_PERF_LIMIT_SET_STATUS newLimit = {0};
    newLimit.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;
    for (UINT32 limitIdx = 0; limitIdx < NUMELEMS(PerfLimitsIDs); limitIdx++)
    {
        newLimit.limitId = PerfLimitsIDs[limitIdx];
        m_PerfLimits.push_back(newLimit);
    }
}

namespace
{
    const char* const vfSwitchTypeStr[] = { "MODS", "RM_LIMITS", "CHANGE_SEQ", "invalid" };
}

Perf30::~Perf30()
{
    Printf(m_VfSwitchTimePriority, "VF switch statistics:\n");
    Printf(m_VfSwitchTimePriority, "Total MODS-initiated VF switches = %u\n",
           m_NumVfSwitches);

    for (UINT32 vfSwitchType = VF_SWITCH_MODS; vfSwitchType < VF_SWITCH_NUM_TYPES; ++vfSwitchType)
    {
        MASSERT(vfSwitchType < VF_SWITCH_NUM_TYPES);
        Printf(m_VfSwitchTimePriority,
            "%s VF switch time range (us) = [%llu, %llu], avg = %llu\n",
            vfSwitchTypeStr[vfSwitchType],
            m_MilwfSwitchTimeUs[vfSwitchType],
            m_MaxVfSwitchTimeUs[vfSwitchType],
            m_AvgVfSwitchTimeUs[vfSwitchType]);
    }
}

RC Perf30::Cleanup()
{
    StickyRC rc;

    // Clear any perf limits we have sent to RM
    if (m_RestoreClocksOnExit && m_ActivePStateLockType != NotLocked)
    {
        rc = ClearPerfLimits();
    }

    // Make sure all NAFLL clocks are in their default regime before exit
    for (const auto &noiseDomItr : m_NoiseAwareDomains)
    {
        if (noiseDomItr.second != DEFAULT_REGIME)
        {
            rc = SetRegime(noiseDomItr.first, DEFAULT_REGIME);
        }
    }

    auto copy = set<Gpu::ClkDomain>(m_DisabledClkDomains);
    for (auto dom : copy)
    {
        rc = EnableArbitration(dom);
    }

    // Remove any dramclk programming entry offsets that MODS may have applied
    if (m_RestoreClocksOnExit)
    {
        rc = ClearDramclkProgEntryOffsets();
    }

    // We don't need to call Perf::Cleanup() because it only calls
    // ClearForcedPState() which is obsolete/unsupported in Perf30.
    return rc;
}

RC Perf30::GetVfSwitchTimePriority(UINT32* priority)
{
    *priority = m_VfSwitchTimePriority;
    return OK;
}

RC Perf30::SetVfSwitchTimePriority(UINT32 priority)
{
    m_VfSwitchTimePriority = static_cast<Tee::Priority>(priority);
    return OK;
}

RC Perf30::GetCoreVoltageMv(UINT32 *pVoltage)
{
    MASSERT(pVoltage);
    RC rc;
    Volt3x *pVolt3x = m_pSubdevice->GetVolt3x();
    if (!pVolt3x->IsInitialized())
    {
        *pVoltage = 0;
        Printf(Tee::PriNormal,
               "Voltage readings cannot be made because Volt3x is not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    // Arbitrarily choose logic rail to mean "core voltage"
    CHECK_RC(pVolt3x->GetVoltageMv(Gpu::VOLTAGE_LOGIC, pVoltage));
    return OK;
}

RC Perf30::SetCoreVoltageMv(UINT32 Voltage, UINT32 PStateNum/*= ILWALID_PSTATE*/)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::SetVoltage(Gpu::VoltageDomain Domain, UINT32 VoltMv, UINT32 PState)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::PrintAllPStateInfo()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::GetClockAtPStateNoCache(UINT32 PStateNum,
                                   Gpu::ClkDomain Domain,
                                   UINT64 *pFreqHz)
{
    MASSERT(pFreqHz);
    RC rc;
    LW2080_CTRL_PERF_PSTATES_STATUS pstatesStatus = {};
    pstatesStatus.super = m_CachedRmInfo.pstates.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_GET_STATUS
       ,&pstatesStatus
       ,sizeof(pstatesStatus)));

    UINT32 rmPStateIdx;
    CHECK_RC(GetRmPStateIndex(PStateNum, &rmPStateIdx));
    FreqCtrlType clkType = ClkDomainType(Domain);
    UINT32 clkIndex = GetClkDomainClkObjIndex(Domain);
    const LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY_STATUS &clkEntryStatus = 
        (m_PStateVersion >= PStateVersion::PSTATE_35) ?
            pstatesStatus.pstates[rmPStateIdx].data.v35.clkEntries[clkIndex].super :
            pstatesStatus.pstates[rmPStateIdx].data.v30.clkEntries[clkIndex];

    UINT32 factor = (Domain == Gpu::ClkPexGen) ? 1 : 1000;
    switch (clkType)
    {
        case FIXED:
        case DECOUPLED:
        case RATIO:
        {
            *pFreqHz = clkEntryStatus.nom.freqkHz * factor;
            break;
        }
        default:
        {
            Printf(Tee::PriNormal, "Invalid clock type %d\n", clkType);
            return RC::ILWALID_ARGUMENT;
        }
    }

    // Check if clock domain isn't supported by RM, colwert in that case
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(Domain))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(Domain);
    }
    *pFreqHz = static_cast<UINT64>(*pFreqHz * freqRatio);
    return rc;
}

RC Perf30::GetPexSpeedAtPState(UINT32 PStateNum, UINT32 *pLinkSpeed)
{
    RC rc;

    UINT32 rmPStateIdx;
    CHECK_RC(GetRmPStateIndex(PStateNum, &rmPStateIdx));
    FreqCtrlType clkGenType = ClkDomainType(Gpu::ClkPexGen);
    UINT32 clkIdx = GetClkDomainClkObjIndex(Gpu::ClkPexGen);

    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY_INFO &clkEntryInfo =
        m_CachedRmInfo.pstates.pstates[rmPStateIdx].data.v3x.clkEntries[clkIdx];

    UINT32 genNumber = 0;
    switch (clkGenType)
    {
        // For the pstates 3.0 VBIOS, gen speed is not stored as a
        // frequency, it's stored as a gen number
        case FIXED:
        case DECOUPLED:
        case RATIO:
        {
            genNumber = clkEntryInfo.nom.origFreqkHz;
            break;
        }
        default:
        {
            Printf(Tee::PriError, "GetPexSpeedAtPState: Invalid clock type\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    switch (genNumber)
    {
        case 5:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed32000MBPS);
            break;
        case 4:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed16000MBPS);
            break;
        case 3:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed8000MBPS);
            break;
        case 2:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed5000MBPS);
            break;
        case 1:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed2500MBPS);
            break;
        default:
            *pLinkSpeed = static_cast<UINT32>(Pci::SpeedUnknown);
    }
    return rc;
}

RC Perf30::GetLinkWidthAtPState(UINT32 PStateNum, UINT32 *pLinkWidth)
{
    RC rc;
    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS lpwrParams = { 0 };
    lpwrParams.listSize = 1;
    lpwrParams.list[0].feature.id = LW2080_CTRL_LPWR_FEATURE_ID_PEX;
    lpwrParams.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_PEX_LINK_WIDTH;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        , LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET
        , &lpwrParams
        , sizeof(lpwrParams)));

    *pLinkWidth = lpwrParams.list[0].param.val;
    return OK;
}

// Unsupported in this subclass
RC Perf30::GetVdtToJs(JSContext *pCtx, JSObject *pJsObj)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::CheckSplitRailDomainExists(Gpu::SplitVoltageDomain voltDom) const
{
    RC rc;

    Volt3x* pVolt3x = m_pSubdevice->GetVolt3x();
    if (!pVolt3x || !pVolt3x->IsInitialized())
    {
        Printf(Tee::PriError,
               "Volt3x is not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    set<Gpu::SplitVoltageDomain> voltDoms = pVolt3x->GetAvailableDomains();
    if (voltDoms.find(voltDom) == voltDoms.end())
    {
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    return rc;
}

RC Perf30::SetPStateOffsetMv(UINT32 PStateNum, INT32 OffsetMv)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::SetRailVoltTuningOffsetuV(Gpu::SplitVoltageDomain Rail, INT32 OffsetuV)
{
    if (Rail >= Gpu::SplitVoltageDomain_NUM)
    {
        Printf(Tee::PriNormal, "Error: unexpected rail index.\n");
        return RC::SOFTWARE_ERROR;
    }

    RC tempRC;
    if ((tempRC = Perf30::CheckSplitRailDomainExists(Rail)) != OK)
    {
        Printf(Tee::PriError, "Cannot offset voltage on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(Rail));
        return tempRC;
    }

    m_RailVoltTuningOffsetsuV[Rail] = OffsetuV;
    return SetVoltTuningParams(writeIfPStateLocked);
}

RC Perf30::SetRailClkDomailwoltTuningOffsetuV
(
    Gpu::SplitVoltageDomain Rail,
    Gpu::ClkDomain ClkDomain,
    INT32 OffsetuV
)
{
    if (PerfUtil::Is2xClockDomain(ClkDomain))
        PerfUtil::Warn2xClock(ClkDomain);

    if (Rail >= Gpu::SplitVoltageDomain_NUM)
    {
        Printf(Tee::PriNormal, "Error: unexpected rail index.\n");
        return RC::SOFTWARE_ERROR;
    }

    RC tempRC;
    if ((tempRC = Perf30::CheckSplitRailDomainExists(Rail)) != OK)
    {
        Printf(Tee::PriError, "Cannot offset voltage on %s rail\n",
               PerfUtil::SplitVoltDomainToStr(Rail));
        return tempRC;
    }

    if (ClkDomain >= Gpu::ClkDomain_NUM)
    {
        Printf(Tee::PriNormal, "Error: unexpected clock domain index.\n");
        return RC::SOFTWARE_ERROR;
    }
    m_RailClkDomailwoltTuningOffsetsuV[Rail]
        [m_ClockDomains.find(ClkDomain)->second.objectIndex] = OffsetuV;
    return SetVoltTuningParams(writeIfPStateLocked);
}

RC Perf30::ValidateClkDomain(Gpu::ClkDomain clkDom)
{
    if (clkDom >= Gpu::ClkDomain_NUM)
    {
        Printf(Tee::PriError, "Unexpected clock domain index\n");
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    if (m_ClockDomains.find(clkDom) == m_ClockDomains.end())
    {
        Printf(Tee::PriNormal, "SetFreqClkDomainOffsetkHz error: unable to find %s.\n",
            PerfUtil::ClkDomainToStr(clkDom));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    return OK;
}

RC Perf30::SetFreqClkDomainOffsetkHz(Gpu::ClkDomain ClkDomain, INT32 FrequencyInkHz)
{
    RC rc;

    CHECK_RC(ValidateClkDomain(ClkDomain));
    m_FreqClkDomainOffsetkHz[ClkDomain] = FrequencyInkHz;
    CHECK_RC(SetVoltTuningParams(writeIfChanged));

    return rc;
}

RC Perf30::SetFreqClkDomainOffsetPercent(Gpu::ClkDomain ClkDomain, FLOAT32 percent)
{
    RC rc;

    CHECK_RC(ValidateClkDomain(ClkDomain));
    m_FreqClkPercentOffsets[ClkDomain] = percent;
    CHECK_RC(SetVoltTuningParams(writeIfChanged));

    return rc;
}

RC Perf30::GetClkProgEntriesMHz
(
    UINT32 pstateNum,
    Gpu::ClkDomain clkDom,
    const ProgEntryOffset applyOffset,
    map<LwU8, clkProgEntry>* pEntriesMHz
)
{
    RC rc;

    // Make sure it's a RM clock domain
    if (!IsRmClockDomain(clkDom))
    {
        clkDom = PerfUtil::ColwertToOppositeClockDomain(clkDom);
    }

    if (ClkDomainType(clkDom) != DECOUPLED)
    {
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkDom);
    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &m_CachedRmInfo.clkDomains.super.objMask.super)
        if (m_CachedRmInfo.clkDomains.domains[ii].domain == rmDomain)
        {
            const LW2080_CTRL_CLK_CLK_DOMAIN_INFO_3X_PROG& infoEntry =
                m_CachedRmInfo.clkDomains.domains[ii].data.v3xProg;

            // Get the PState frequency range for clkDom
            ClkRange range = { 0 };
            if (pstateNum != ILWALID_PSTATE)
            {
                CHECK_RC(GetClockRange(pstateNum, clkDom, &range));
            }

            // Retrieve the clock programming entry VF deltas
            LW2080_CTRL_CLK_CLK_PROGS_STATUS clkProgsStatus = {};
            for (UINT32 jj = infoEntry.clkProgIdxFirst;
                 jj <= infoEntry.clkProgIdxLast; jj++)
            {
                BoardObjGrpMaskBitSet(&clkProgsStatus.super.objMask.super, jj);
            }
            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                m_pSubdevice
               ,LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_STATUS
               ,&clkProgsStatus
               ,sizeof(clkProgsStatus)));

            // Clock programming entries come in two flavors:
            //  1. Entries returned by GET_INFO
            //  2. Entries returned by GET_STATUS
            //
            // The frequency returned by GET_INFO is the POR value for the
            // clock programming entry set by VBIOS. RM always adds the factory
            // OC from the "Overclocking Table" in VBIOS if bOverrideOCOV is
            // enabled for the point, but this is stored separately in
            // factoryDelta.
            //
            // GET_STATUS will return the point's current frequency with ALL
            // offsets that have been applied (including factory OC and whatever
            // MODS does). This is what we expect will match with the current
            // pstate clock range. So we use the values from GET_STATUS to find
            // which points we want to return, and we either return the "base"
            // frequency (includes factory OC) or the GET_STATUS frequency
            // depending on "applyOffset".
            for (UINT32 jj = infoEntry.clkProgIdxFirst;
                 jj <= infoEntry.clkProgIdxLast; jj++)
            {
                const auto& clkProgInfo = m_CachedRmInfo.clkProgs.progs[jj];
                const auto& clkProgStatus = clkProgsStatus.progs[jj];

                UINT32 baseEntryMHz = 0;
                UINT32 dynamicEntryMHz = 0;
                bool overclockingEnabled = false;

                MASSERT(clkProgInfo.type == clkProgStatus.type);
                switch (clkProgStatus.type)
                {
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_1X_PRIMARY:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_1X_PRIMARY_RATIO:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_1X_PRIMARY_TABLE:
                        baseEntryMHz = clkProgInfo.data.v1x.freqMaxMHz;
                        dynamicEntryMHz = clkProgStatus.data.v1x.freqMaxMHz;
                        overclockingEnabled =
                            (clkProgInfo.data.v1xMaster.bOCOVEnabled == LW_TRUE);
                        break;
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
                        baseEntryMHz = clkProgInfo.data.v30.super.freqMaxMHz;
                        dynamicEntryMHz = clkProgStatus.data.v30.offsettedFreqMaxMHz;
                        overclockingEnabled =
                            (clkProgInfo.data.v30Master.master.bOCOVEnabled == LW_TRUE);
                        break;
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
                        baseEntryMHz = clkProgInfo.data.v35.super.freqMaxMHz;
                        dynamicEntryMHz = clkProgStatus.data.v35.offsettedFreqMaxMHz;
                        overclockingEnabled =
                            (clkProgInfo.data.v35Master.master.bOCOVEnabled == LW_TRUE);
                        break;
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_30:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_35:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_3X:
                    case LW2080_CTRL_CLK_CLK_PROG_TYPE_1X:
                    default:
                        MASSERT(!"Unknown or invalid clock programming entry type");
                        return RC::SOFTWARE_ERROR;
                }
                MASSERT(baseEntryMHz);
                MASSERT(dynamicEntryMHz);

                // If MODS has set the global bOVOCEnabled flag, overclocking
                // is always enabled for every clock programming entry.
                overclockingEnabled = overclockingEnabled || m_OverrideOVOC;

                // Apply the factory overclock from the Overclocking Table in VBIOS
                if (overclockingEnabled)
                {
                    baseEntryMHz = static_cast<UINT32>(
                        static_cast<INT32>(baseEntryMHz) +
                        infoEntry.factoryDelta.data.staticOffset.deltakHz / 1000);
                }

                bool outOfRange = dynamicEntryMHz + 1 < range.MinKHz / 1000 ||
                                  dynamicEntryMHz - 1 > range.MaxKHz / 1000;
                if (pstateNum != ILWALID_PSTATE && outOfRange)
                {
                    continue;
                }

                (*pEntriesMHz)[jj].entryFreqMHz =
                    (applyOffset == ProgEntryOffset::ENABLED) ? dynamicEntryMHz : baseEntryMHz;
                (*pEntriesMHz)[jj].freqDeltaMinMHz = infoEntry.freqDeltaMinMHz;
                (*pEntriesMHz)[jj].freqDeltaMaxMHz = infoEntry.freqDeltaMaxMHz;
                (*pEntriesMHz)[jj].bOCOVEnabled = overclockingEnabled;
            }

            if (pEntriesMHz->empty())
            {
                Printf(Tee::PriError,
                    "Cannot find pstate %u clock prog entries for %s\n",
                    pstateNum,
                    PerfUtil::ClkDomainToStr(clkDom));
                return RC::SOFTWARE_ERROR;
            }
            return rc;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    Printf(Tee::PriError, "Could not find any programming entries for %s\n",
        PerfUtil::ClkDomainToStr(clkDom));
    return RC::ILWALID_CLOCK_DOMAIN;
}

namespace
{
    // Returns a clock programming entry index and the offset that must be
    // applied to achieve targetFreqkHz
    void GetClosestProgEntryOffsetkHz
    (
        const map<LwU8, Perf::clkProgEntry>& progEntries,
        UINT64 targetFreqkHz,
        std::pair<LwU8, INT32>* pClosestEntry
    )
    {
        MASSERT(pClosestEntry);
        *pClosestEntry = std::pair<LwU8, INT32>((std::numeric_limits<LwU8>::max)(),
                                                (std::numeric_limits<INT32>::max)());
        for (const auto& progsItr : progEntries)
        {
            const INT32 offsetReqkHz = static_cast<INT32>(targetFreqkHz) -
                static_cast<INT32>(progsItr.second.entryFreqMHz * 1000);
            if (abs(offsetReqkHz) < abs(pClosestEntry->second))
            {
                *pClosestEntry = std::pair<LwU8, INT32>(progsItr.first, offsetReqkHz);
            }
        }
    }
}

RC Perf30::ConfigureDramclkSettings(UINT32 pstateNum, const ClkSetting& dramclkSetting)
{
    RC rc;

    m_NewDramclkVFPointIdx = ProgEntrySetting::ILWALID_IDX;
    m_NewDramclkVFPointOffsetkHz = 0;

    // Cannot adjust clock programming entries if dramclk is fixed
    if (ClkDomainType(Gpu::ClkM) == FIXED)
    {
        return m_pSubdevice->SetClock(Gpu::ClkM, dramclkSetting.FreqHz);
    }

    // If there is only one dramclk frequency in this pstate and
    // we are trying to lock to it, then we don't need to set any
    // clock programming entry offsets.
    const UINT32 dramFreqkHz = static_cast<UINT32>(dramclkSetting.FreqHz / 1000);
    ClkRange lwrDramRange;
    CHECK_RC(GetClockRange(pstateNum, Gpu::ClkM, &lwrDramRange));
    if (lwrDramRange.MinKHz == lwrDramRange.MaxKHz &&
        dramFreqkHz >= lwrDramRange.MinKHz - 1 &&
        dramFreqkHz <= lwrDramRange.MaxKHz + 1)
    {
        return rc;
    }

    // Get all the clock programming entries for this pstate. These are
    // the entries that are eligible to be offset
    map<LwU8, Perf::clkProgEntry> pstateProgEntries;
    CHECK_RC(GetClkProgEntriesMHz(pstateNum, dramclkSetting.Domain, ProgEntryOffset::DISABLED,
                                  &pstateProgEntries));

    std::pair<LwU8, INT32> closestPStateProgEntryOffsetkHz;
    GetClosestProgEntryOffsetkHz(pstateProgEntries, dramclkSetting.FreqHz / 1000,
                                 &closestPStateProgEntryOffsetkHz);

    // If a programming entry is an exact match, exit early or else
    // we might fail the check below if -disable_override_ovoc is used
    if (!closestPStateProgEntryOffsetkHz.second)
    {
        m_NewDramclkVFPointIdx = closestPStateProgEntryOffsetkHz.first;
        return rc;
    }

    if (!m_OverrideOVOC)
    {
        // Remove all clock programming entries that will be unable to
        // satisfy dramclkSetting requirements. This includes points
        // where overclocking is disabled as well as points where the
        // allowed overclocking range will not be able to meet the target.
        const UINT32 dramFreqMHz = static_cast<UINT32>(dramclkSetting.FreqHz / 1000000);
        for (auto entryItr = pstateProgEntries.begin(); entryItr != pstateProgEntries.end();)
        {
            if (!entryItr->second.bOCOVEnabled)
            {
                entryItr = pstateProgEntries.erase(entryItr);
                continue;
            }

            INT16 signedMinMHz =
                static_cast<INT16>(entryItr->second.entryFreqMHz) +
                entryItr->second.freqDeltaMinMHz;
            if (signedMinMHz < 0)
                signedMinMHz = 0;

            const UINT32 freqMinMHz = static_cast<UINT32>(signedMinMHz);
            const UINT32 freqMaxMHz = static_cast<UINT32>(
                static_cast<INT16>(entryItr->second.entryFreqMHz) +
                entryItr->second.freqDeltaMaxMHz);
            if (dramFreqMHz < freqMinMHz || dramFreqMHz > freqMaxMHz)
            {
                entryItr = pstateProgEntries.erase(entryItr);
                continue;
            }
            ++entryItr;
        }

        if (pstateProgEntries.empty())
        {
            Printf(Tee::PriError,
                "Cannot find a dramclk programming entry in P%u that can set %uMHz\n",
                pstateNum, dramFreqMHz);
            return RC::CANNOT_APPLY_VF_OFFSET;
        }
        GetClosestProgEntryOffsetkHz(pstateProgEntries, dramclkSetting.FreqHz / 1000,
                                     &closestPStateProgEntryOffsetkHz);
    }

    // Get all the clock programming entries for this domain. These will
    // be used to sanity check user input.
    map<LwU8, Perf::clkProgEntry> allProgEntries;
    CHECK_RC(GetClkProgEntriesMHz(ILWALID_PSTATE, dramclkSetting.Domain, ProgEntryOffset::DISABLED,
                                  &allProgEntries));
    std::pair<LwU8, INT32> closestEntryOffsetkHz;
    GetClosestProgEntryOffsetkHz(allProgEntries, dramclkSetting.FreqHz / 1000,
                                 &closestEntryOffsetkHz);

    if (abs(closestEntryOffsetkHz.second) <
        abs(closestPStateProgEntryOffsetkHz.second))
    {
        Printf(Tee::PriWarn,
               "Closest programming entry to %llu MHz on %s domain is not in P%u\n",
               dramclkSetting.FreqHz/1000000,
               PerfUtil::ClkDomainToStr(dramclkSetting.Domain), pstateNum);
    }

    m_NewDramclkVFPointIdx = closestPStateProgEntryOffsetkHz.first;
    m_NewDramclkVFPointOffsetkHz = closestPStateProgEntryOffsetkHz.second;

    return rc;
}

RC Perf30::ClearDramclkProgEntryOffsets()
{
    RC rc;

    for (const auto& progItr : m_AdjustedDramclkProgEntries)
    {
        if (progItr.offsetkHz)
        {
            // Reset this clock programming entry by offsetting to 0kHz
            CHECK_RC(SetClkProgrammingTableOffsetkHz(progItr.index, 0));
        }
    }
    m_AdjustedDramclkProgEntries.clear();

    return rc;
}

RC Perf30::SetClkProgrammingTableOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz)
{
    RC rc;

    LW2080_CTRL_CLK_CLK_PROGS_CONTROL clkProgsControl;
    memset(&clkProgsControl, 0, sizeof(clkProgsControl));
    BoardObjGrpMaskBitSet(&clkProgsControl.super.objMask.super, TableIdx);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        ,LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_CONTROL
        ,&clkProgsControl
        ,sizeof(clkProgsControl)));

    PLW2080_CTRL_CLK_FREQ_DELTA pClkProgsCtrlFreqDelta =
        &clkProgsControl.progs[TableIdx].data.v1xMaster.deltas.freqDelta;

    if ((LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pClkProgsCtrlFreqDelta) ==
            LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC) &&
        (LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pClkProgsCtrlFreqDelta) ==
            FrequencyInkHz))
    {
       return OK;
    }

    Printf(Tee::PriLow, "Offsetting clock programming entry %u by %dkHz\n",
        TableIdx, FrequencyInkHz);

    pClkProgsCtrlFreqDelta->type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC;
    pClkProgsCtrlFreqDelta->data.staticOffset.deltakHz = FrequencyInkHz;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        ,LW2080_CTRL_CMD_CLK_CLK_PROGS_SET_CONTROL
        ,&clkProgsControl
        ,sizeof(clkProgsControl)));

    m_CacheStatus = CACHE_DIRTY;

    return rc;
}

RC Perf30::GetClkProgrammingTableSlaveRatio
(
    UINT32  tableIdx,
    UINT32  slaveEntryIdx,
    UINT08 *pRatio
)
{
    MASSERT(pRatio);

    RC rc;
    if (tableIdx >= LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
    {
        Printf(Tee::PriNormal,
               "Invalid clock programming table index: %d\n", tableIdx);
        return RC::BAD_PARAMETER;
    }

    if (slaveEntryIdx >= LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES)
    {
        Printf(Tee::PriNormal,
               "Invalid slave entry index: %d\n", slaveEntryIdx);
        return RC::BAD_PARAMETER;
    }
    LW2080_CTRL_CLK_CLK_PROGS_CONTROL clkProgsControl = {};
    BoardObjGrpMaskBitSet(&clkProgsControl.super.objMask.super, tableIdx);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_CONTROL
       ,&clkProgsControl
       ,sizeof(clkProgsControl)));

    if (LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO ==
            clkProgsControl.progs[tableIdx].super.type)
    {
        *pRatio =clkProgsControl.progs[tableIdx].data.v30MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio; //$
    }
    else if (LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO ==
            clkProgsControl.progs[tableIdx].super.type)
    {
        *pRatio = clkProgsControl.progs[tableIdx].data.v35MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio; //$
    }
    else
    {
        *pRatio = 0;
        Printf(Tee::PriNormal,
               "You are attempting to retrieve the ratio of a clock programming "
               "entry which is not of type MASTER_RATIO.\n");
        return RC::BAD_PARAMETER;
    }
    return rc;
}

RC Perf30::SetClkProgrammingTableSlaveRatio
(
    UINT32 tableIdx,
    UINT32 slaveEntryIdx,
    UINT08 ratio
)
{
    RC rc;
    if (tableIdx >= LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
    {
        Printf(Tee::PriNormal,
               "Invalid clock programming table index: %d\n", tableIdx);
        return RC::BAD_PARAMETER;
    }

    if (slaveEntryIdx >= LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES)
    {
        Printf(Tee::PriNormal,
               "Invalid slave entry index: %d\n", slaveEntryIdx);
        return RC::BAD_PARAMETER;
    }

    LW2080_CTRL_CLK_CLK_PROGS_CONTROL clkProgsControl = {};
    BoardObjGrpMaskBitSet(&clkProgsControl.super.objMask.super, tableIdx);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_CONTROL
       ,&clkProgsControl
       ,sizeof(clkProgsControl)));

    if (LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO ==
            clkProgsControl.progs[tableIdx].super.type)
    {
        if (ratio == clkProgsControl.progs[tableIdx]
                            .data.v30MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio)
        {
            return OK;
        }

        clkProgsControl.progs[tableIdx]
            .data.v30MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio = ratio;
    }
    else if (LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO ==
            clkProgsControl.progs[tableIdx].super.type)
    {
        if (ratio == clkProgsControl.progs[tableIdx]
                            .data.v35MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio)
        {
            return OK;
        }

        clkProgsControl.progs[tableIdx]
            .data.v35MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio = ratio;
    }
    else
    {
        Printf(Tee::PriNormal,
               "You are attempting to modify the ratio of a clock programming "
               "entry which is not of type MASTER_RATIO.\n");
        return RC::BAD_PARAMETER;
    }

    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_PROGS_SET_CONTROL
       ,&clkProgsControl
       ,sizeof(clkProgsControl));
}

RC Perf30::SetVFPointsOffset
(
    VFPointType Type,
    UINT32 TableIdxStart,
    UINT32 TableIdxEnd,
    INT32 Value,
    UINT32 lwrveIdx
)
{
    RC rc;

    LW2080_CTRL_CLK_CLK_VF_POINTS_CONTROL clkVFPointsControl;
    memset(&clkVFPointsControl, 0, sizeof(clkVFPointsControl));
    for (UINT32 idx = TableIdxStart; idx <= TableIdxEnd; idx++)
    {
        BoardObjGrpMaskBitSet(&clkVFPointsControl.super.objMask.super, idx);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        ,LW2080_CTRL_CMD_CLK_CLK_VF_POINTS_GET_CONTROL
        ,&clkVFPointsControl
        ,sizeof(clkVFPointsControl)));

    for (UINT32 idx = TableIdxStart; idx <= TableIdxEnd; idx++)
    {
        if (GetVFPointType(clkVFPointsControl.vfPoints[idx].type) != Type)
        {
            Printf(Tee::PriNormal,
                "SetVFPointsOffset error: VF point index %d is not of expected type\n",
                idx);
            return RC::BAD_PARAMETER;
        }
    }

    switch (clkVFPointsControl.vfPoints[TableIdxStart].type)
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        {
            bool changeDetected = false;
            for (UINT32 idx = TableIdxStart; idx <= TableIdxEnd; idx++)
            {
                PLW2080_CTRL_CLK_FREQ_DELTA pClkVFPointsCtrlFreqDelta =
                    &clkVFPointsControl.vfPoints[idx].data.v30Volt.freqDelta;

                if ((LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pClkVFPointsCtrlFreqDelta) ==
                        LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC) &&
                    (LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pClkVFPointsCtrlFreqDelta) !=
                        Value))
                {
                    changeDetected = true;
                    pClkVFPointsCtrlFreqDelta->data.staticOffset.deltakHz = Value;
                }
            }
            if (!changeDetected)
                return OK;
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        {
            bool changeDetected = false;
            for (UINT32 idx = TableIdxStart; idx <= TableIdxEnd; idx++)
            {
                if (clkVFPointsControl.vfPoints[idx].data.v30Freq.voltDeltauV != Value)
                {
                    changeDetected = true;
                    clkVFPointsControl.vfPoints[idx].data.v30Freq.voltDeltauV = Value;
                }
            }
            if (!changeDetected)
                return OK;

            if (m_pSubdevice->GetVolt3x()->GetSplitRailConstraintAuto())
            {
                SplitRailConstraintManager* pSRConstMgr =
                    m_pSubdevice->GetVolt3x()->GetSplitRailConstraintManager();
                pSRConstMgr->ApplyVfPointOffsetuV(Value);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            bool changeDetected = false;
            for (UINT32 idx = TableIdxStart; idx <= TableIdxEnd; idx++)
            {
                PLW2080_CTRL_CLK_FREQ_DELTA pClkVFPointsCtrlFreqDelta =
                    &clkVFPointsControl.vfPoints[idx].data.v35Volt.freqDelta;

                if ((LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pClkVFPointsCtrlFreqDelta) ==
                        LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC) &&
                    (LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pClkVFPointsCtrlFreqDelta) !=
                        Value))
                {
                    changeDetected = true;
                    pClkVFPointsCtrlFreqDelta->data.staticOffset.deltakHz = Value;
                }
            }
            if (!changeDetected)
                return OK;
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            bool changeDetected = false;
            for (UINT32 idx = TableIdxStart; idx <= TableIdxEnd; idx++)
            {
                if (clkVFPointsControl.vfPoints[idx].data.v35Freq.voltDeltauV != Value)
                {
                    changeDetected = true;
                    clkVFPointsControl.vfPoints[idx].data.v35Freq.voltDeltauV = Value;
                }
            }
            if (!changeDetected)
                return OK;

            if (m_pSubdevice->GetVolt3x()->GetSplitRailConstraintAuto())
            {
                SplitRailConstraintManager* pSRConstMgr =
                    m_pSubdevice->GetVolt3x()->GetSplitRailConstraintManager();
                pSRConstMgr->ApplyVfPointOffsetuV(Value);
            }
            break;
        }
        default:
            Printf(Tee::PriNormal, "SetVFPointsOffset error: unrecognized type\n");
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        ,LW2080_CTRL_CMD_CLK_CLK_VF_POINTS_SET_CONTROL
        ,&clkVFPointsControl
        ,sizeof(clkVFPointsControl)));

    m_CacheStatus = CACHE_DIRTY;

    return rc;
}

RC Perf30::SetVFPointsOffsetkHz
(
    UINT32 TableIdxStart,
    UINT32 TableIdxEnd,
    INT32 FrequencyInkHz,
    UINT32 lwrveIdx
)
{
    return SetVFPointsOffset(VFPointType::Volt,
        TableIdxStart, TableIdxEnd, FrequencyInkHz);
}

RC Perf30::SetVFPointsOffsetuV
(
    UINT32 TableIdxStart,
    UINT32 TableIdxEnd,
    INT32 OffsetInuV
)
{
    return SetVFPointsOffset(VFPointType::Freq,
        TableIdxStart, TableIdxEnd, OffsetInuV);
}

// Unsupported in this subclass
RC Perf30::SetVdtTuningOffsetMv(INT32 OffsetMv)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::GetVoltageAtPStateNoCache
(
    UINT32 PStateNum,
    Gpu::VoltageDomain Domain,
    UINT32 *pVoltageUv
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::GetLwrrentPerfValuesNoCache(PStateInfos *pPstatesInfo)
{
    Printf(Tee::PriLow, "Perf30::GetLwrrentPerfValeusNoCache\n");
    RC rc;
    LW2080_CTRL_PERF_PSTATES_STATUS pstatesStatus = {};
    pstatesStatus.super = m_CachedRmInfo.pstates.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_GET_STATUS
       ,&pstatesStatus
       ,sizeof(pstatesStatus)));

    PStateInfos &pstatesInfo = *pPstatesInfo;

    UINT32 pstateIndex = 0;
    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &pstatesStatus.super.objMask.super)
        const LW2080_CTRL_PERF_PSTATE_STATUS &pstate = pstatesStatus.pstates[i];
        for (const auto& clkDomItr : m_ClockDomains)
        {
            UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkDomItr.first);
            // Make sure this clock domain is supported by VBIOS/RM
            if (!IS_MASK_SUBSET(rmDomain, m_CachedRmInfo.clkDomains.vbiosDomains))
            {
                continue;
            }
            const LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY_STATUS &clkEntryStatus = 
                (m_PStateVersion >= PStateVersion::PSTATE_35) ?
                    pstate.data.v35.clkEntries[clkDomItr.second.objectIndex].super :
                    pstate.data.v30.clkEntries[clkDomItr.second.objectIndex];
            pstatesInfo[pstateIndex].ClkFlags[rmDomain] = 0;
            ClkRange &domRange = pstatesInfo[pstateIndex].ClkVals[rmDomain];
            domRange.LwrrKHz = clkEntryStatus.nom.freqkHz;
            domRange.MinKHz  = clkEntryStatus.min.freqkHz;
            domRange.MaxKHz  = clkEntryStatus.max.freqkHz;
            Printf(Tee::PriLow, "rmDomain = %d, MinKHz = %d, MaxKHz = %d \n",
                    rmDomain, domRange.MinKHz, domRange.MaxKHz);
        }
        pstateIndex++;
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Perf30::SetPStateTableInternal
(
    UINT32 pstateNum,
    const vector<LW2080_CTRL_PERF_CLK_DOM_INFO> & clks,
    const vector<LW2080_CTRL_PERF_VOLT_DOM_INFO>& voltages
)
{
    Tasker::MutexHolder mh(GetMutex());
    RC rc;

    // We need to refresh the perf values cache so that we can do appropriate
    // clock range checking. This call accomplishes this
    CHECK_RC(GetPStatesInfo());

    UINT32 modsPStateIndex;
    CHECK_RC(GetPStateIdx(pstateNum, &modsPStateIndex));
    PStateInfo &pstate = m_CachedPStates[modsPStateIndex];

    // Perform input validation - make sure that clocks are in range and that
    // they are not fixed/unknown
    for (auto& clkItr : clks)
    {
        UINT32 rmDomain = clkItr.domain;
        Gpu::ClkDomain dom = PerfUtil::ClkCtrl2080BitToGpuClkDomain(rmDomain);

        // Verfiy that clock is in the pstate range
        if (!(pstate.ClkVals[rmDomain].MinKHz <= clkItr.freq &&
              clkItr.freq <= pstate.ClkVals[rmDomain].MaxKHz))
        {
            Printf(Tee::PriNormal,
                   "Frequency is out of range for clock %s (0x%x). Is %d kHz, "
                   "needs to be within [%d - %d] kHz\n",
                   PerfUtil::ClkDomainToStr(dom),
                   rmDomain,
                   clkItr.freq,
                   pstate.ClkVals[rmDomain].MinKHz,
                   pstate.ClkVals[rmDomain].MaxKHz);
            return RC::INCORRECT_FREQUENCY;
        }
        FreqCtrlType clkType = ClkDomainType(dom);
        if (clkType == FIXED || clkType == UNKNOWN_TYPE)
        {
            Printf(Tee::PriNormal,
                "Clock %s (0x%x) is of of an invalid type. Its nominal "
                "frequency cannot be changed.\n",
                PerfUtil::ClkDomainToStr(dom),
                rmDomain);
            return RC::BAD_PARAMETER;
        }
    }
    if (voltages.size())
    {
        Printf(Tee::PriNormal, "You cannot set voltage via pstates in pstates 3.0\n");
        return RC::BAD_PARAMETER;
    }

    if (pstateNum == ILWALID_PSTATE)
    {
        CHECK_RC(GetLwrrentPState(&pstateNum));
    }
    UINT32 rmPStateIndex;
    CHECK_RC(GetRmPStateIndex(pstateNum, &rmPStateIndex));

    LW2080_CTRL_PERF_PSTATES_CONTROL pstatesControl = {};
    pstatesControl.super = m_CachedRmInfo.pstates.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_GET_CONTROL
       ,&pstatesControl
       ,sizeof(pstatesControl)));

    // Assign information from clks to pstatesControl structure
    Printf(Tee::PriLow, "SetPStatetableInternal: pstate %d\n", pstateNum);
    for (auto& clkItr : clks)
    {
        UINT32 rmDomain = clkItr.domain;
        Gpu::ClkDomain dom = PerfUtil::ClkCtrl2080BitToGpuClkDomain(rmDomain);
        UINT32 rmClockIndex = GetClkDomainClkObjIndex(dom);

        Printf(Tee::PriLow,
               "SetPStatetableInternal: clk domain %s (0x%x)  freq %d kHz\n",
               PerfUtil::ClkDomainToStr(dom),
               clkItr.domain,
               clkItr.freq);

        LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY_CONTROL &clkEntryControl = pstatesControl
                                                         .pstates[rmPStateIndex]
                                                         .data.v3x
                                                         .clkEntries[rmClockIndex];
        FreqCtrlType clkType = ClkDomainType(dom);
        // Set nominal frequency, don't let a fixed frequency get changed
        switch (clkType)
        {
            case FIXED:
            {
                // User MUST NOT change a PSTATE's base frequency for a fixed
                // clock domain.
                MASSERT(clkEntryControl.nom.baseFreqkHz == clkItr.freq);
                break;
            }
            case DECOUPLED:
            case RATIO:
            {
                clkEntryControl.nom.baseFreqkHz = clkItr.freq;
                break;
            }
            default:
                return RC::ILWALID_ARGUMENT;
        }
        if (dom == Gpu::ClkGpc2 || dom == Gpu::ClkGpc)
        {
            m_ClockChangeCounter.AddExpectedChange(clkItr.freq);
        }
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_SET_CONTROL
       ,&pstatesControl
       ,sizeof(pstatesControl)));

    m_CacheStatus = CACHE_DIRTY;
    return rc;
}

RC Perf30::InjectPerfPoint(const InjectedPerfPoint &point)
{
    if (point.clks.empty() && point.voltages.empty())
        return OK;

    RC rc;

    Printf(Tee::PriLow, "Injecting PerfPoint:\n");
    point.Print(Tee::PriLow);

    LW2080_CTRL_PERF_CHANGE_SEQ_CONTROL chgSeqCtrl = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_CHANGE_SEQ_GET_CONTROL
       ,&chgSeqCtrl
       ,sizeof(chgSeqCtrl)));

    chgSeqCtrl.bChangeRequested = LW_TRUE;
    chgSeqCtrl.changeInput.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE;

    for (const auto& clkSetting : point.clks)
    {
        Gpu::ClkDomain clkDom = clkSetting.Domain;
        const UINT32 clkFreqkHz = static_cast<UINT32>(clkSetting.FreqHz / 1000);

        FLOAT32 freqRatio = 1.0f;
        if (!IsRmClockDomain(clkDom))
        {
            freqRatio = PerfUtil::GetModsToRmFreqRatio(clkDom);
            clkDom = PerfUtil::ColwertToOppositeClockDomain(clkDom);
        }

        const auto clkItr = m_ClockDomains.find(clkDom);
        if (clkItr == m_ClockDomains.end())
        {
            return RC::ILWALID_CLOCK_DOMAIN;
        }
        const UINT32 clkObjIdx = clkItr->second.objectIndex;
        chgSeqCtrl.changeInput.clk[clkObjIdx].clkFreqkHz =
            static_cast<LwU32>(clkFreqkHz * freqRatio);
        chgSeqCtrl.changeInput.clkDomainsMask.super.pData[0] |= BIT(clkObjIdx);
    }

    Volt3x* pVolt3x = m_pSubdevice->GetVolt3x();
    if (!pVolt3x || !pVolt3x->IsInitialized())
    {
        Printf(Tee::PriError, "%s: Volt3x is not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // PP-TODO: remove this hack once RM correctly populates this field in GET_CONTROL
    for (UINT32 railIdx = 0; railIdx < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; railIdx++)
    {
        chgSeqCtrl.changeInput.volt[railIdx].voltageMinNoiseUnawareuV = 625000;
    }

    for (const auto& voltEntry : point.voltages)
    {
        const UINT32 voltRailIdx = pVolt3x->GpuSplitVoltageDomainToRailIdx(voltEntry.domain);
        MASSERT(voltRailIdx < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS);
        auto& newVoltSetting = chgSeqCtrl.changeInput.volt[voltRailIdx];
        newVoltSetting.voltageuV = voltEntry.voltMv * 1000;
        newVoltSetting.voltageMinNoiseUnawareuV = voltEntry.voltMv * 1000;
        chgSeqCtrl.changeInput.voltRailsMask.super.pData[0] |= BIT(voltRailIdx);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_CHANGE_SEQ_SET_CONTROL
       ,&chgSeqCtrl
       ,sizeof(chgSeqCtrl)));

    return rc;
}

RC Perf30::GetRmPStateIndex(UINT32 pstateNum, UINT32 *rmPStateIndex) const
{
    MASSERT(rmPStateIndex);
    map<UINT32, UINT32>::const_iterator it = m_PStateNumToRmPStateIndex.find(pstateNum);
    if (it == m_PStateNumToRmPStateIndex.end())
    {
        Printf(Tee::PriNormal, "Could not find pstate %d\n", pstateNum);
        return RC::ILWALID_ARGUMENT;
    }
    *rmPStateIndex = it->second;
    return OK;
}

RC Perf30::GetRmPStateLevel(UINT32 pstateNum, UINT32 *rmPStateLevel) const
{
    MASSERT(rmPStateLevel);
    map<UINT32, UINT32>::const_iterator it = m_PStateNumToRmPStateLevel.find(pstateNum);
    if (it == m_PStateNumToRmPStateLevel.end())
    {
        Printf(Tee::PriNormal, "Could not find pstate %d\n", pstateNum);
        return RC::ILWALID_ARGUMENT;
    }
    *rmPStateLevel = it->second;
    return OK;
}

RC Perf30::GetClkDomainsControl()
{
    if (!m_InitDone)
        return RC::SOFTWARE_ERROR;

    memset(&m_ClkDomainsControl, 0, sizeof(m_ClkDomainsControl));
    m_ClkDomainsControl.super = m_CachedRmInfo.clkDomains.super;

    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_CONTROL
       ,&m_ClkDomainsControl
       ,sizeof(m_ClkDomainsControl));
}

RC Perf30::SetClkDomainsControl()
{
    if (!m_InitDone)
        return RC::SOFTWARE_ERROR;

    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_DOMAINS_SET_CONTROL
       ,&m_ClkDomainsControl
       ,sizeof(m_ClkDomainsControl));
}

RC Perf30::SetVoltTuningParams(VoltTuningWriteMode m)
{
    RC rc;

    Volt3x *pVolt3x = m_pSubdevice->GetVolt3x();
    SplitRailConstraintManager* pSRConstMgr = nullptr;
    if (pVolt3x->GetSplitRailConstraintAuto())
        pSRConstMgr = pVolt3x->GetSplitRailConstraintManager();

    if ((pVolt3x == nullptr) || !pVolt3x->IsInitialized())
    {
        Printf(Tee::PriNormal,
            "SetVoltTuningParams failure due to Volt3x not being available\n");
        return RC::SOFTWARE_ERROR;
    }

    INT32 offsetmV = 0;
    for (UINT32 i = 0; i < NUMELEMS(m_VoltTuningOffsetsMv); i++)
    {
        offsetmV += m_VoltTuningOffsetsMv[i];
    }

    CHECK_RC(GetClkDomainsControl());

    UINT32 voltRailMask = pVolt3x->GetVoltRailMask();
    bool changeDetected = false;

    while (voltRailMask)
    {
        UINT32 railIdx = Utility::BitScanForward(voltRailMask);
        voltRailMask ^= 1<<railIdx;

        if (railIdx >= Gpu::SplitVoltageDomain_NUM)
        {
            MASSERT(!"Unexpected rail idx");
            return RC::SOFTWARE_ERROR;
        }

        const INT32 offsetuV = offsetmV * 1000 + m_RailVoltTuningOffsetsuV[railIdx];

        if (m_ClkDomainsControl.deltas.voltDeltauV[railIdx] != offsetuV)
        {
            if (pSRConstMgr)
            {
                CHECK_RC(pSRConstMgr->ApplyRailOffsetuV(
                            static_cast<Gpu::SplitVoltageDomain>(railIdx),
                            offsetuV));
            }

            m_ClkDomainsControl.deltas.voltDeltauV[railIdx] = offsetuV;
            changeDetected = true;
        }
    }

    UINT32 clkIdx;
    for (const auto& clkDom : m_ClockDomains)
    {
        clkIdx = GetClkDomainClkObjIndex(clkDom.first);
        INT32 offsetkHz = 0;
        FLOAT32 freqRatio = 1.0f;
        if (m_FreqClkDomainOffsetkHz.find(clkDom.first) != m_FreqClkDomainOffsetkHz.end())
        {
            if (!IsRmClockDomain(clkDom.first))
            {
                freqRatio = PerfUtil::GetModsToRmFreqRatio(clkDom.first);
            }
            offsetkHz = static_cast<INT32>(m_FreqClkDomainOffsetkHz[clkDom.first] * freqRatio);
        }
        else
        {
            continue;
        }
        INT32 deltaMinFreqKHz = m_ClockDomains[clkDom.first].freqDeltaRangeMHz.minFreqMHz * 1000;
        INT32 deltaMaxFreqKHz = m_ClockDomains[clkDom.first].freqDeltaRangeMHz.maxFreqMHz * 1000;
        if (!m_OverrideOVOC &&
            (deltaMinFreqKHz > offsetkHz || deltaMaxFreqKHz < offsetkHz) )
        {
            Printf(Tee::PriError,
                    "SetVoltTuningParams failure: Desired frequency offset out of range.\n");
            Printf(Tee::PriNormal,
                    "Desired offset = %d KHz, Frequency Delta Range =(%d KHz, %d KHz)\n",
                    static_cast<INT32>(offsetkHz / freqRatio),
                    static_cast<INT32>(deltaMinFreqKHz / freqRatio),
                    static_cast<INT32>(deltaMaxFreqKHz / freqRatio));
            return RC::CANNOT_APPLY_VF_OFFSET;
        }

        auto& deltaCtrl = m_ClkDomainsControl.domains[clkIdx].data.v3xProg.deltas.freqDelta;

        if (deltaCtrl.type != LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC ||
            deltaCtrl.data.staticOffset.deltakHz != offsetkHz)
        {
            deltaCtrl.type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC;
            deltaCtrl.data.staticOffset.deltakHz = offsetkHz;
            changeDetected = true;
        }
    }

    for (const auto& percentOffset : m_FreqClkPercentOffsets)
    {
        clkIdx = GetClkDomainClkObjIndex(percentOffset.first);

        // RM accepts an offset range of [-1.0, 1.0]
        LwSFXP4_12 sfxpOffset = Utility::ColwertFloatToFXP(percentOffset.second / 100, 4, 12);

        auto& deltaCtrl = m_ClkDomainsControl.domains[clkIdx].data.v3xProg.deltas.freqDelta;

        if (deltaCtrl.type != LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_PERCENT ||
            deltaCtrl.data.percentOffset.deltaPercent != sfxpOffset)
        {
            deltaCtrl.type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_PERCENT;
            deltaCtrl.data.percentOffset.deltaPercent = sfxpOffset;
            changeDetected = true;
        }
    }

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx, &m_CachedRmInfo.clkDomains.super.objMask.super)
        voltRailMask = pVolt3x->GetVoltRailMask();
        while (voltRailMask)
        {
            UINT32 railIdx = Utility::BitScanForward(voltRailMask);
            voltRailMask ^= 1<<railIdx;

            INT32 offsetuV = 0;
            if (m_RailClkDomailwoltTuningOffsetsuV[railIdx].find(clkIdx) !=
                m_RailClkDomailwoltTuningOffsetsuV[railIdx].end())
            {
                offsetuV = m_RailClkDomailwoltTuningOffsetsuV[railIdx][clkIdx];
            }
            if (m_ClkDomainsControl.domains[clkIdx].data.v3xProg.deltas.voltDeltauV[railIdx] != offsetuV)
            {
                if (pSRConstMgr)
                {
                    CHECK_RC(pSRConstMgr->ApplyClkDomailwoltOffsetuV(
                                static_cast<Gpu::SplitVoltageDomain>(railIdx),
                                offsetuV));
                }

                m_ClkDomainsControl.domains[clkIdx].data.v3xProg.deltas.voltDeltauV[railIdx] = offsetuV;
                changeDetected = true;
            }
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    if (((writeIfChanged == m) && !changeDetected) ||
        ((writeIfPStateLocked == m) && (GetPStateLockType() == NotLocked)))
    {
        return OK;
    }

    if (pSRConstMgr)
    {
        CHECK_RC(pSRConstMgr->UpdateConstraint());
    }

    m_ClkDomainsControl.bOverrideOVOC = m_OverrideOVOC ? LW_TRUE : LW_FALSE;

    CHECK_RC(SetClkDomainsControl());

    m_CacheStatus = CACHE_DIRTY;

    return rc;
}

RC Perf30::SetOverrideOVOC(bool override)
{
    m_OverrideOVOC = override;
    return OK;
}

RC Perf30::ApplyFreqTuneOffset(INT32 freqOffsetKhz, UINT32 pstateNum)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::ParseVfTable()
{
    RC rc;

    Printf(Tee::PriLow,
           "Clock programming mask = 0x%x\n"
           "slave entry count = %d\n"
           "vfEntryCount = %d\n",
           m_CachedRmInfo.clkProgs.super.objMask.super.pData[0],
           m_CachedRmInfo.clkProgs.slaveEntryCount,
           m_CachedRmInfo.clkProgs.vfEntryCount);

    // Defer printing basic info about vf points for now
    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRmInfo.clkDomains.super.objMask.super)
        LW2080_CTRL_CLK_CLK_DOMAIN_INFO &domInfo = m_CachedRmInfo.clkDomains.domains[i];
        Gpu::ClkDomain domain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(domInfo.domain);
        if (!IS_MASK_SUBSET(domInfo.domain, m_CachedRmInfo.clkDomains.programmableDomains))
        {
            continue;
        }
        if ((domInfo.super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY) ||
            (domInfo.super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY))
        {
            CHECK_RC(GetClkDomailwfEntryTables(domain,
                                               domInfo.data.v3xProg.clkProgIdxFirst,
                                               domInfo.data.v3xProg.clkProgIdxLast,
                                               m_CachedRmInfo.clkProgs,
                                               m_CachedRmInfo.vfPoints));
        }
        m_ClockDomains[domain].freqDeltaRangeMHz.minFreqMHz =
            domInfo.data.v3xProg.freqDeltaMinMHz;
        m_ClockDomains[domain].freqDeltaRangeMHz.maxFreqMHz =
            domInfo.data.v3xProg.freqDeltaMaxMHz;
        if (PerfUtil::Is1xClockDomain(domain) || PerfUtil::Is2xClockDomain(domain))
        {
            domain = PerfUtil::ColwertToOppositeClockDomain(domain);
            m_ClockDomains[domain].freqDeltaRangeMHz.minFreqMHz =
                domInfo.data.v3xProg.freqDeltaMinMHz;
            m_ClockDomains[domain].freqDeltaRangeMHz.maxFreqMHz =
                domInfo.data.v3xProg.freqDeltaMaxMHz;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    return rc;
}

RC Perf30::GetVirtualPerfPoints(PerfPoints *points)
{
    RC rc;
    CHECK_RC(GetVPStatesInfo());
    std::copy(m_VPStates.begin(),
              m_VPStates.end(),
              std::inserter(*points, points->begin()));
    return rc;
}

LwU32 Perf30::GetVPStateId(GpcPerfMode m) const
{
    switch (m)
    {
        case GpcPerf_TDP:
            return LW2080_CTRL_PERF_VPSTATES_IDX_RATEDTDP;
            break;

        case GpcPerf_TURBO:
            return LW2080_CTRL_PERF_VPSTATES_IDX_TURBOBOOST;
            break;

        default:
            return LW2080_CTRL_PERF_VPSTATES_IDX_NUM_INDEXES;
    }
}

bool Perf30::HasVirtualPState(GpcPerfMode m) const
{
    for (const auto& vpstateItr : m_VPStates)
    {
        if (vpstateItr.SpecialPt == m)
            return true;
    }

    return false;
}

RC Perf30::GetVPStatesInfo()
{
    RC rc;

    // If there are no PStates, don't try to find VPStates
    if (m_CachedPStates.empty())
        return rc;

    m_VPStates.clear();

    for (UINT32 ii = 0; ii < NUM_GpcPerfMode; ii++)
    {
        const GpcPerfMode gpcPerfMode = static_cast<GpcPerfMode>(ii);

        UINT32 vpstateID = GetVPStateId(gpcPerfMode);
        if (vpstateID == LW2080_CTRL_PERF_VPSTATES_IDX_NUM_INDEXES)
            continue;

        UINT32 vpstateIdx = m_CachedRmInfo.vpstates.vpstateIdx[vpstateID];

        if (vpstateIdx == LW2080_CTRL_PERF_VPSTATE_INDEX_ILWALID)
        {
            Printf(Tee::PriDebug, "VBIOS does not specify vpstate %s\n",
                   SpecialPtToString(gpcPerfMode));
            continue;
        }

        PerfPoint vpstate;
        vpstate.SpecialPt = gpcPerfMode;
        UINT32 pstateIdx = m_CachedRmInfo.vpstates.vpstates[vpstateIdx].data.v3x.pstateIdx;
        if (pstateIdx >= m_CachedPStates.size())
        {
            Printf(Tee::PriError,
                   "RM returned invalid pstate index %d for %s PerfPoint\n",
                   pstateIdx, SpecialPtToString(vpstate.SpecialPt));
            return RC::LWRM_ILWALID_INDEX;
        }
        vpstate.PStateNum = m_CachedPStates[pstateIdx].PStateNum;

        ClkSetting clk;
        for (UINT32 jj = 0; jj < NUMELEMS(m_CachedRmInfo.vpstates.vpstates[vpstateIdx].data.v3x.clocks); jj++)
        {
            clk.Domain = GetDomainFromClockProgrammingOrder(jj);

            // If this domain was marked as a skip entry in VBIOS or if VBIOS
            // specified a non-programmable domain, skip it now so we don't
            // try to program it later
            if (clk.Domain == Gpu::ClkUnkUnsp || ClkDomainType(clk.Domain) == FIXED)
                continue;

            clk.FreqHz = static_cast<UINT64>(
                m_CachedRmInfo.vpstates.vpstates[vpstateIdx].data.v3x.clocks[jj].targetFreqMHz) * 1000000ULL;
            if (clk.FreqHz)
            {
                // Always store vpstate frequencies as 1x
                if (PerfUtil::Is2xClockDomain(clk.Domain))
                {
                    clk.Domain = PerfUtil::ColwertTo1xClockDomain(clk.Domain);
                    clk.FreqHz /= 2ULL;
                }

                // Apply any active frequency offsets to this vpstate
                Gpu::ClkDomain opDom = PerfUtil::ColwertToOppositeClockDomain(clk.Domain);
                for (auto clkDom : { clk.Domain, opDom })
                {
                    const auto& fixedOffsetEntrykHz = m_FreqClkDomainOffsetkHz.find(clkDom);
                    if (fixedOffsetEntrykHz != m_FreqClkDomainOffsetkHz.end())
                    {
                        if (PerfUtil::Is2xClockDomain(fixedOffsetEntrykHz->first))
                        {
                            clk.FreqHz += 1000 * fixedOffsetEntrykHz->second / 2;
                        }
                        else
                        {
                            clk.FreqHz += 1000 * fixedOffsetEntrykHz->second;
                        }
                    }

                    const auto& percentOffset = m_FreqClkPercentOffsets.find(clkDom);
                    if (percentOffset != m_FreqClkPercentOffsets.end())
                    {
                        clk.FreqHz = static_cast<UINT64>(
                            (1.0f + percentOffset->second / 100.0f) * clk.FreqHz);
                    }
                }
                vpstate.Clks[clk.Domain] = clk;
            }
        }
        m_VPStates.insert(vpstate);
    }

    return rc;
}

RC Perf30::ClockOfVPState
(
    int idx,
    bool isSymbolicIdx,
    UINT64 * pRtnGpcHz,
    UINT32 * pRtnPState
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::GetTDPGpcHz(UINT64* tdpGpcHz)
{
    // For each VPState
    for (const auto& vpstateItr : m_VPStates)
    {
        if (vpstateItr.SpecialPt == GpcPerf_TDP)
        {
            if (vpstateItr.Clks.find(Gpu::ClkGpc2) != vpstateItr.Clks.end())
            {
                *tdpGpcHz = vpstateItr.Clks.find(Gpu::ClkGpc2)->second.FreqHz / 2;
                return OK;
            }
            else if (vpstateItr.Clks.find(Gpu::ClkGpc) != vpstateItr.Clks.end())
            {
                *tdpGpcHz = vpstateItr.Clks.find(Gpu::ClkGpc)->second.FreqHz;
                return OK;
            }
        }
    }

    // VBIOS does not specify TDP VPState
    return RC::UNSUPPORTED_FUNCTION;
}

Gpu::ClkDomain Perf30::GetDomainFromClockProgrammingOrder(UINT32 idx) const
{
    for (const auto& clkDomItr : m_ClockDomains)
    {
        if (clkDomItr.second.objectIndex == idx && clkDomItr.second.IsRmClkDomain)
            return clkDomItr.first;
    }

    return Gpu::ClkUnkUnsp;
}

#define GET_RM_INFO(RMCTRL, paramName)      \
    CHECK_RC(LwRmPtr()->ControlBySubdevice( \
        m_pSubdevice                        \
       ,RMCTRL                              \
       ,&m_CachedRmInfo.paramName           \
       ,sizeof(m_CachedRmInfo.paramName)));

RC Perf30::GetCachedRmInfo()
{
    RC rc;

    Printf(Tee::PriLow, "Getting cached RM perf info\n");

    GET_RM_INFO(LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO, clkDomains);
    GET_RM_INFO(LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_INFO, clkProgs);
    GET_RM_INFO(LW2080_CTRL_CMD_CLK_CLK_VF_POINTS_GET_INFO, vfPoints);
    GET_RM_INFO(LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO, pstates);
    GET_RM_INFO(LW2080_CTRL_CMD_PERF_VPSTATES_GET_INFO, vpstates);

    // Possible that the arbiter is explicitly disabled and will return
    // LW_ERR_NOT_SUPPORTED. Will use the change sequencer directly.
    rc = LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_GET_INFO
       ,&m_CachedRmInfo.perfLimits
       ,sizeof(m_CachedRmInfo.perfLimits));
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        rc.Clear();
    }
    CHECK_RC(rc);

    GET_RM_INFO(LW2080_CTRL_CMD_PERF_VFE_VARS_GET_INFO, vfeVars);
    GET_RM_INFO(LW2080_CTRL_CMD_PERF_VFE_EQUS_GET_INFO, vfeEqus);
    GET_RM_INFO(LW2080_CTRL_CMD_CLK_CLIENT_CLK_DOMAINS_GET_INFO, clientClkDoms);

    return rc;
}

// The the RMCTRL calls need to be called in a specific order in order to
// satisfy all of the data dependencies between the different control calls.
// Order of RMCTRLs
// 1. LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO
//    Retrieves which clock domains exist on the chip. Also gives us the
//    ordering (ie index to clock domain mappings) for clock entries
//    in other control calls (like LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO).
// 2. LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO
//    Retrieves the pstates present on the chip. Also returns the clock ranges
//    for each clock domain in the pstate.
// 3. LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_INFO
//    Assists in giving information about the frequency points that master
//    clocks can set to. Depends on information from
//    LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO to understand which entries in
//    the table correspond to which clock domains.
// 4. LW2080_CTRL_CMD_CLK_CLK_VF_POINTS_GET_INFO - Retrieves the specific points
//    that master clocks can set to. Depends on the information from
//    LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_INFO.
// 5. We can only populate m_VfEntryTbl after all the rest because in order to
//    do this we need the pstate clock ranges *and* the VF points.
// 6. We get the VPState data from RM AFTER we get the clock programming order
//    from step 1, otherwise we don't know how to interpret the data.
RC Perf30::Init()
{
    if (m_InitDone)
    {
        return OK;
    }
    Printf(Tee::PriLow, "Beginning mods Perf30 Init\n");
    JavaScriptPtr pJs;
    RC rc;

    CHECK_RC(GetCachedRmInfo());

    m_ClkDomainsMask = m_CachedRmInfo.clkDomains.programmableDomains;

    Printf(Tee::PriLow, "Supported clock domains = 0x%x\n", m_CachedRmInfo.clkDomains.vbiosDomains);

    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRmInfo.clkDomains.super.objMask.super)
        // super.objMask.super is *not* a mask of LW2080_CTRL_CLK_DOMAIN_*
        // It is a mask of objects in LW2080_CTRL_CLK_CLK_DOMAINS_INFO::domains
        LW2080_CTRL_CLK_CLK_DOMAIN_INFO &domInfo = m_CachedRmInfo.clkDomains.domains[i];

        ClkDomainInfo newClockDomain;
        Gpu::ClkDomain domain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(domInfo.domain);
        Printf(Tee::PriLow,
               "Clock domain 0x%x (%s)\n",
               domInfo.domain, PerfUtil::ClkDomainToStr(domain));

        newClockDomain.Domain = domain;
        newClockDomain.partitionMask = domInfo.partMask;
        newClockDomain.objectIndex = i;
        Printf(Tee::PriLow, " objectIndex = %d\n", newClockDomain.objectIndex);
        Printf(Tee::PriLow, " Type = ");
        switch (domInfo.super.type)
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED:
            {
                Printf(Tee::PriLow, "fixed\n");
                newClockDomain.Type = FIXED;
                break;
            }
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
            {
                Printf(Tee::PriLow, "master\n");
                newClockDomain.Type = DECOUPLED;
                break;
            }
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
            {
                Printf(Tee::PriLow, "slave\n");
                newClockDomain.Type = RATIO;
                Gpu::ClkDomain masterDom = PerfUtil::ClkCtrl2080BitToGpuClkDomain(
                    m_CachedRmInfo.clkDomains.domains[domInfo.data.v30Slave.slave.masterIdx].domain);
                newClockDomain.RatioDomain = masterDom;

                // Figure out the ratio and store it
                UINT32 masterDomObjIdx = m_ClockDomains[masterDom].objectIndex;
                const LW2080_CTRL_CLK_CLK_DOMAIN_INFO &masterDomInfo =
                    m_CachedRmInfo.clkDomains.domains[masterDomObjIdx];
                const UINT32 &clkProgIdx = masterDomInfo.data.v3xProg.clkProgIdxFirst;
                for (UINT32 entryIdx = 0;
                    entryIdx < LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES;
                    entryIdx++)
                {
                    const LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY &slaveEntry =
                        m_CachedRmInfo.clkProgs.progs[clkProgIdx].data.v30MasterRatio.ratio.slaveEntries[entryIdx];
                    if (slaveEntry.clkDomIdx == i)
                    {
                        newClockDomain.Ratio = slaveEntry.ratio;
                        break;
                    }
                }

                Printf(Tee::PriLow,
                       " master is %s\n",
                       PerfUtil::ClkDomainToStr(newClockDomain.RatioDomain));
                break;
            }
            default:
            {
                Printf(Tee::PriError,
                       "Invalid clock domain type: %d\n", domInfo.super.type);
                return RC::SOFTWARE_ERROR;
            }
        }

        // Store the noise aware capable domains
        switch (domInfo.super.type)
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
                if (domInfo.data.v3x.bNoiseAwareCapable)
                {
                    m_NoiseAwareDomains[domain] = DEFAULT_REGIME;
                }
                break;
            default:
                break;
        }

        if (BIT(i) & m_CachedRmInfo.clkDomains.clkMonDomainsMask.super.pData[0])
        {
            Printf(Tee::PriLow, "RM supports clock monitoring for domain = %s \n",
                   PerfUtil::ClkDomainToStr(domain));
            newClockDomain.SupportsClkMon = true;
        }

        newClockDomain.IsRmClkDomain = true;
        m_ClockDomains[domain] = newClockDomain;

        // For any 1x/2x Clock domains, create a corresponding 2x/1x clock domain
        if (PerfUtil::Is1xClockDomain(domain) || PerfUtil::Is2xClockDomain(domain))
        {
            newClockDomain.Domain = PerfUtil::ColwertToOppositeClockDomain(domain);
            // This is not the original RM domain
            newClockDomain.IsRmClkDomain = false;
            newClockDomain.SupportsClkMon = false;
            m_ClockDomains[newClockDomain.Domain] = newClockDomain;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    switch (m_CachedRmInfo.pstates.version)
    {
        case LW2080_CTRL_PERF_PSTATE_VERSION_35:
            m_PStateVersion = PStateVersion::PSTATE_35;
            break;
        case LW2080_CTRL_PERF_PSTATE_VERSION_30:
            m_PStateVersion = PStateVersion::PSTATE_30;
            break;
        case LW2080_CTRL_PERF_PSTATE_VERSION_2X:
            m_PStateVersion = PStateVersion::PSTATE_20;
            break;
        default:
            m_PStateVersion = PStateVersion::PSTATE_UNSUPPORTED;
            break;
    }

    UINT32 allPStatesMask = 0;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRmInfo.pstates.super.objMask.super)
        LW2080_CTRL_PERF_PSTATE_INFO &pstate = m_CachedRmInfo.pstates.pstates[i];
        PStateInfo newPState;
        newPState.PStateBit = pstate.pstateID;
        allPStatesMask |= newPState.PStateBit;
        newPState.PStateNum = Utility::BitScanForward(newPState.PStateBit);
        m_PStateNumToRmPStateIndex[newPState.PStateNum] = i;
        m_PStateNumToRmPStateLevel[newPState.PStateNum] = pstate.level;
        // Unused in pstates 3.0, but set to 0 for safety
        newPState.FreqOffsetKHz = 0;
        m_CachedPStates.push_back(newPState);
        // Populate pstate clock ranges in GetLwrrentPerfValuesNoCache
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    Printf(Tee::PriLow, "PStates Mask = 0x%x\n", allPStatesMask);

    // P-State range info can be retrieved from LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO
    // as well, however no offsets should be applied at this point, so use this API
    // here to save code
    CHECK_RC(GetLwrrentPerfValuesNoCache(&m_CachedPStates));

    // Initializing with no clock domains at intersect unless the
    // user requests so via -XXXclk intersect, (OR) XX.intersect
    for (auto& pstateItr : m_CachedPStates)
    {
        for (auto& clkItr : pstateItr.ClkVals)
        {
            pstateItr.IsClkIntersect[clkItr.first] = false;
        }
    }

    // We have to get VF points last because we don't know what the clock ranges
    // for each pstate are until after GetLwrrentPerfValuesNoCache
    CHECK_RC(ParseVfTable());

    // Get all VPStates from RM (MODS mostly cares about TDP)
    CHECK_RC(GetVPStatesInfo());

    // Query whether the RM perf arbiter is enabled. The arbiter can be
    // explicitly disabled for automotive builds or via a flag. We are assuming
    // that this will remain constant for the duration of the run (i.e. no calls
    // to PERF_LIMITS_SET_CONTROL to change this)
    LW2080_CTRL_PERF_PERF_LIMITS_CONTROL ctrlParam = {};
    rc = LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_GET_CONTROL
       ,&ctrlParam
       ,sizeof(ctrlParam));
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        m_ArbiterEnabled = false;
        rc.Clear();
    }
    else
    {
        m_ArbiterEnabled = (ctrlParam.bArbitrateAndApplyLock == LW_FALSE);
    }
    CHECK_RC(rc);

    if (!m_ArbiterEnabled)
    {
        Printf(Tee::PriLow, "RM perf arbiter is disabled\n");
        m_ShouldHavePowerCapping = false; // If the arbiter is disabled, so is power-capping
    }

    pJs->GetProperty(pJs->GetGlobalObject(), "g_RestoreClocksOnExit",
                     &m_RestoreClocksOnExit);

    bool powerCappingCmdLine = true;
    if (pJs->GetProperty(pJs->GetGlobalObject(), "g_AllowRmPowerCapping", &powerCappingCmdLine) ==
        RC::OK)
    {
        // If the user specified "-pwr_cap 0", RM power-capping should be disabled
        if (!powerCappingCmdLine)
        {
            m_ShouldHavePowerCapping = false;
        }
    }

    // EMU/SIM platforms don't support power policies, nor do they have specs based on a board
    if (!m_pSubdevice->IsEmOrSim() && m_ShouldHavePowerCapping)
    {
        // Check BoardsDb info to see if power capping is intentionally disabled
        string boardName;
        UINT32 index;
        if (OK == m_pSubdevice->GetBoardInfoFromBoardDB(&boardName, &index, false) &&
            boardName != "unknown board")
        {
            const BoardDB& boards = BoardDB::Get();
            const BoardDB::BoardEntry* pDef =
                    boards.GetBoardEntry(m_pSubdevice->DeviceId(), boardName, index);
            JavaScriptPtr pJs;
            bool allowPowerCapping;
            if (pDef != nullptr &&
                pDef->m_JSData != nullptr &&
                OK == pJs->GetProperty(pDef->m_JSData, "Pwrcap", &allowPowerCapping))
            {
                m_ShouldHavePowerCapping = allowPowerCapping;
            }
        }
    }
    else
    {
        // Power policies are only supported on real silicon
        m_ShouldHavePowerCapping = false;
    }

    if (m_ShouldHavePowerCapping)
    {
        vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(1);
        statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
        RC pollRc;
        RC limitsRc;

        // Query to see if the power-capping perf limit is enabled.
        // Poll in case the PMU PMGR task has not finished initializing.
        pollRc = Tasker::Poll(1000.0, [&]()->bool
        {
            limitsRc = PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice);
            if (limitsRc != RC::OK)
            {
                return true;
            }
            m_HasPowerCapping = (statuses[0].output.bEnabled && statuses[0].output.value);
            if (m_HasPowerCapping)
            {
                return true;
            }
            return false;
        });
        CHECK_RC(limitsRc);
        if (pollRc == RC::TIMEOUT_ERROR)
        {
            // If we timed out, then power-capping is disabled.
            // This is not a fatal error. So clear the RC.
            pollRc.Clear();
        }
        CHECK_RC(pollRc); // Catch if the user pressed Ctrl-c
    }

    if (!m_HasPowerCapping && m_ShouldHavePowerCapping)
    {
        Printf(Tee::PriWarn, "RM power-capping is disabled\n");
    }

    Printf(Tee::PriLow, "Perf30 Init done\n");
    m_InitDone = true;
    return rc;
}

RC Perf30::GenerateStandardPerfPoints
(
    UINT64 fastestSafeGpcHz,
    const PStateInfos &pstateInfo,
    PerfPoints *pPoints
)
{
    MASSERT(pPoints);
    RC rc;

    // Pre-generate list of decoupled domains
    set<Gpu::ClkDomain> decoupledDomains;
    for (const auto& dom : m_ClockDomains)
    {
        if (m_pSubdevice->HasDomain(dom.first) && dom.second.Type == DECOUPLED)
        {
            decoupledDomains.insert(dom.first);
        }
    }

    for (UINT32 i = 0; i < pstateInfo.size(); ++i)
    {
        // PerfPoints across a *single* pstate go in here
        vector<PerfPoint> pstatePerfPoints;

        const UINT32 pstate = pstateInfo[i].PStateNum;
        vector<GpcPerfMode> perfPointsToGenerate = { GpcPerf_MAX };

        auto HasMultipleFrequencies = [&](Gpu::ClkDomain dom)->bool
        {
            if (!IsRmClockDomain(dom))
            {
                return false;
            }
            const UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
            auto& orgClkInfo = m_OrgPStates[i].ClkVals;
            if (orgClkInfo.find(rmDomain) == orgClkInfo.end())
            {
                Printf(Tee::PriError,
                    "PState %d does not have an entry for %s\n",
                    pstate, PerfUtil::ClkDomainToStr(dom));
                rc = RC::SOFTWARE_ERROR;
            }
            return orgClkInfo[rmDomain].MaxKHz != orgClkInfo[rmDomain].MinKHz;
        };

        if (any_of(decoupledDomains.begin(), decoupledDomains.end(),
                   HasMultipleFrequencies))
        {
            auto additionalPerfModes =
                { GpcPerf_MID, GpcPerf_MIN, GpcPerf_NOM, GpcPerf_INTERSECT };
            perfPointsToGenerate.insert(perfPointsToGenerate.end(),
                additionalPerfModes.begin(), additionalPerfModes.end());
        }
        CHECK_RC(rc);

        auto GetFreqHzFromPerfMode = [&](GpcPerfMode mode, const ClkRange& range)->UINT64
        {
            switch (mode)
            {
                case GpcPerf_MAX:
                    return range.MaxKHz * 1000ULL;
                case GpcPerf_MID:
                {
                    UINT64 midFreqkHz = range.MinKHz + (range.MaxKHz - range.MinKHz) / 2;
                    return midFreqkHz * 1000ULL;
                }
                case GpcPerf_MIN:
                case GpcPerf_INTERSECT:
                    return range.MinKHz * 1000ULL;
                case GpcPerf_NOM:
                    return range.LwrrKHz * 1000ULL;
                default:
                    return 0;
            }
        };

        // NOTE: gpcInfo uses rm clockdomain!!!
        Gpu::ClkDomain gpcDomain = IsRmClockDomain(Gpu::ClkGpc) ? Gpu::ClkGpc : Gpu::ClkGpc2;
        const UINT32 rmGpcDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(gpcDomain);

        // gpcclk/gpc2clk not found?
        if (rmGpcDomain == 0)
            return RC::SOFTWARE_ERROR;

        const auto& gpcIt = pstateInfo[i].ClkVals.find(rmGpcDomain);
        const ClkRange &gpcInfo = gpcIt->second;
        
        for (auto mode : perfPointsToGenerate)
        {
            PerfPoint p(pstate, mode);
            const UINT64 gpcclkFreqHz = GetFreqHzFromPerfMode(mode, gpcInfo) /
                ((gpcDomain == Gpu::ClkGpc2) ? 2ULL : 1ULL);
            if (gpcclkFreqHz && mode != GpcPerf_INTERSECT)
            {
                p.Clks[Gpu::ClkGpc] = ClkSetting(Gpu::ClkGpc, gpcclkFreqHz, FORCE_IGNORE_LIMIT);
            }

            // Add dramclk to Clks to allow PerfHack to offset it
            if (decoupledDomains.find(Gpu::ClkM) != decoupledDomains.end())
            {
                const UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkM);
                auto& orgClkInfo = m_OrgPStates[i].ClkVals;
                if (orgClkInfo.find(rmDomain) != orgClkInfo.end())
                {
                    UINT64 freqHz = GetFreqHzFromPerfMode(mode, orgClkInfo[rmDomain]);
                    p.Clks[Gpu::ClkM] = ClkSetting(Gpu::ClkM, freqHz);
                }
            }

            if (mode == GpcPerf_INTERSECT)
            {
                IntersectSetting is;
                is.Type = IntersectPState;
                m_pSubdevice->GetVolt3x()->Initialize();
                for (const auto rail : m_pSubdevice->GetVolt3x()->GetAvailableDomains())
                {
                    is.VoltageDomain = rail;
                    p.IntersectSettings.clear();
                    p.IntersectSettings.insert(is);
                    pstatePerfPoints.push_back(p);
                }
            }
            else
            {
                if (mode == GpcPerf_MAX)
                {
                    //GpcClk is not added as part of the Clks when standard perf points
                    //are generated, since we automatically poll from RM for min, max and mid points
                    //If gpcClk were there are part of Clks field then ConstructClksPerfLimits,
                    //will send a pair of GPC_MIN/MAX limits to RM if it sees an entry in
                    //Clks[Gpu::ClkGpc]. If the entry is sent to RM, the clock frequency will always
                    //be fixed to the freq that was sent. Removing the GpcClk breaks
                    //PerfPoint::SetUnsafeIfFasterThan(), hence adding below statement
                    p.UnsafeDefault = true;
                }
                pstatePerfPoints.push_back(p);
            }
        }

        // Flag new perf points that are unsafe (faster than 0.tdp)
        for (auto& perfPoint : pstatePerfPoints)
        {
            if (perfPoint.SpecialPt != GpcPerf_INTERSECT)
            {
                perfPoint.SetUnsafeIfFasterThan(fastestSafeGpcHz);
            }
        }

        pPoints->insert(pstatePerfPoints.begin(), pstatePerfPoints.end());
    }
    return rc;
}

RC Perf30::GetRatiodClocks
(
    UINT32 PStateNum,
    UINT64 FreqHz,
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo
)
{
    // In Perf20, this function inserts the ratiod clocks as well, however the
    // RMCTRL to do this is unsupported in pstates 3.0.
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::SetRegime(Gpu::ClkDomain dom, RegimeSetting regime)
{
    RC rc;

    // Make sure it's a valid RM clock domain
    if (!IsRmClockDomain(dom))
    {
        dom = PerfUtil::ColwertToOppositeClockDomain(dom);
    }

    if (m_NoiseAwareDomains[dom] != regime)
    {
        CHECK_RC(m_pSubdevice->GetAvfs()->SetRegime(dom, regime));
        m_NoiseAwareDomains[dom] = regime;
    }

    return rc;
}

RC Perf30::GetRegime(Gpu::ClkDomain dom, RegimeSetting* regime)
{
    Avfs* pAvfs = m_pSubdevice->GetAvfs();

    switch (dom)
    {
        case Gpu::ClkGpc2:
        case Gpu::ClkGpc:
            return pAvfs->GetNafllDeviceRegimeViaId(Avfs::NAFLL_GPC0, regime);
        case Gpu::ClkXbar2:
        case Gpu::ClkXbar:
            return pAvfs->GetNafllDeviceRegimeViaId(Avfs::NAFLL_XBAR, regime);
        case Gpu::ClkSys2:
        case Gpu::ClkSys:
            return pAvfs->GetNafllDeviceRegimeViaId(Avfs::NAFLL_SYS, regime);
        case Gpu::ClkLtc2:
        case Gpu::ClkLtc:
            return pAvfs->GetNafllDeviceRegimeViaId(Avfs::NAFLL_LTC, regime);
        case Gpu::ClkLwd:
            return pAvfs->GetNafllDeviceRegimeViaId(Avfs::NAFLL_LWD, regime);
        case Gpu::ClkHost:
            return pAvfs->GetNafllDeviceRegimeViaId(Avfs::NAFLL_HOST, regime);
        default:
            Printf(Tee::PriError, "Cannot get regime for %s clock domain\n",
                   PerfUtil::ClkDomainToStr(dom));
            return RC::ILWALID_CLOCK_DOMAIN;
    }
}

RC Perf30::ForceNafllRegimes(RegimeSetting regime)
{
    Avfs::RegimeOverrides overrides;
    for (auto& dom : m_NoiseAwareDomains)
    {
        overrides.push_back({ dom.first, regime });
        dom.second = regime;
    }

    return m_pSubdevice->GetAvfs()->SetRegime(overrides);
}

// If you set the FORCE_FFR_SET flags, set the frequency to 0, and set the
// _BYPASS_CHANGE_SEQ flag, this tells RM to make no clock or voltage changes,
// but causes the clock to go into the specified NAFLL regime
RC Perf30::SetRegimesForClocks(const ClkMap& clks)
{
    RC rc;

    for (const auto& clkItr : clks)
    {
        Gpu::ClkDomain dom    = clkItr.second.Domain;
        RegimeSetting  regime = clkItr.second.Regime;

        // Skip setting regime if it's invalid
        if (regime >= RegimeSetting_NUM)
            continue;

        // Make sure it's a valid RM clock domain
        if (!IsRmClockDomain(dom))
        {
            dom = PerfUtil::ColwertToOppositeClockDomain(dom);
        }

        if (m_NoiseAwareDomains[dom] != regime)
        {
            CHECK_RC(m_pSubdevice->GetAvfs()->SetRegime(dom, regime));
            m_NoiseAwareDomains[dom] = regime;
        }
    }

    return rc;
}

RC Perf30::InnerSetPerfPoint(const PerfPoint& Setting)
{
    RC rc;

    const UINT64 startTime = Xp::QueryPerformanceCounter();

    CHECK_RC(ValidatePerfPoint(Setting));
    CHECK_RC(SetPStateLock(Setting, m_ForcedPStateLockType));

    // TODO: Remove me by Volta
    CHECK_RC(SetPerfPointPcieSettings(Setting));

    // TODO: We don't absolutely need these in PStates 3.0 since
    //       m_LwrPerfPoint has all of this data. These members are required
    //       for legacy Perf APIs to function correctly.
    m_LwrPerfPointGpcPerfMode = Setting.SpecialPt;
    m_LwrPerfPointPStateNum = Setting.PStateNum;
    m_LwrIntersectSettings = Setting.IntersectSettings;

    UINT64 endTime = Xp::QueryPerformanceCounter();
    UINT64 elapsedTimeUs =
        (endTime - startTime) * 1000000 / Xp::QueryPerformanceFrequency();
    UpdateVfSwitchStats(VF_SWITCH_MODS, elapsedTimeUs);
    m_NumVfSwitches++;

    return rc;
}

RC Perf30::InnerSetPStateLock
(
    const PerfPoint &Setting,
    PStateLockType Lock //! unused
)
{
    RC rc;

    // Bug 1746824
    // In order to avoid the split rail constraint violation, we defer sending
    // voltage rail offsets to the RM until the first perfpoint is set. So, we
    // check for the very first transition where pstate gets locked and send
    // the pending voltage tuning offsets(if any) to the RM in a single shot.
    if (GetPStateLockType() == NotLocked)
        CHECK_RC(SetVoltTuningParams(writeUnconditionally));

    if (Setting.Regime != RegimeSetting_NUM)
    {
        CHECK_RC(ForceNafllRegimes(Setting.Regime));
    }

    // Set any forced regimes for NAFLL clock domains in this PerfPoint
    CHECK_RC(SetRegimesForClocks(Setting.Clks));

    if (!Setting.IntersectSettings.empty())
    {
        CHECK_RC(SetIntersectPt(Setting));
    }
    else
    {
        if (m_ArbiterEnabled)
        {
            CHECK_RC(SetPStateLockViaLimits(Setting));
            if (GetPStateLockType() == NotLocked && m_UnlockChangeSequencer)
            {
                CHECK_RC(UnlockChangeSequencer());
            }
        }
        else
        {
            CHECK_RC(SetClocksViaInjection(Setting, ClockInjectionType::ALL_CLOCKS));
        }
    }

    m_ActivePStateLockType = m_ForcedPStateLockType;
    m_LwrPerfPoint = Setting;

    // Ilwalidate the PStateNum cache only for intersect points since we must
    // query RM to figure out which PState it chose, otherwise just use the
    // PStateNum field from the current PerfPoint
    if (m_LwrPerfPoint.IntersectSettings.empty())
    {
        m_LwrPerfPointPStateNum = m_LwrPerfPoint.PStateNum;
        m_CachedPStateNum = m_LwrPerfPoint.PStateNum;
        m_PStateNumCache = CACHE_CLEAN;
    }
    else
    {
        m_PStateNumCache = CACHE_DIRTY;
        m_LwrPerfPointPStateNum = ILWALID_PSTATE;
    }

    return rc;
}

RC Perf30::GetLwrrentPerfPoint(PerfPoint *pPoint)
{
    MASSERT(pPoint);
    RC rc;
    UINT32 numPState = 0;
    CHECK_RC(GetNumPStates(&numPState));
    if (numPState == 0)
    {
        // no-op
        Printf(Tee::PriError,
               "PStates must be enabled to use GetPerfPoint\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    *pPoint = m_LwrPerfPoint;
    return rc;
}

namespace
{
struct ClockLimit
{
    UINT32 minLimitId;
    UINT32 maxLimitId;
};

struct LimitInfo
{
    Gpu::ClkDomain clkDomain;
    ClockLimit limit;
};

// Must be in same order as strictFreqLockLimits
LimitInfo clientLowStrictFreqLockLimits[] =
{
    {Gpu::ClkM, {LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DRAM_MIN,
                 LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DRAM_MAX}},
    {Gpu::ClkGpc, {LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_GPC_MIN,
                   LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_GPC_MAX}},
    {Gpu::ClkDisp, {LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DISP_MIN,
                    LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_DISP_MAX}},
    {Gpu::ClkPexGen, {LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PCIE_MIN,
                      LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PCIE_MAX}}
};

// Must be in same order as modsRulesStrictFreqLockLimits
LimitInfo strictFreqLockLimits[] =
{
    {Gpu::ClkM, {LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DRAM_MIN,
                 LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DRAM_MAX}},
    {Gpu::ClkGpc, {LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_GPC_MIN,
                   LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_GPC_MAX}},
    {Gpu::ClkDisp, {LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DISP_MIN,
                    LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_DISP_MAX}},
    {Gpu::ClkPexGen, {LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PCIE_MIN,
                      LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PCIE_MAX}}
};

LimitInfo modsRulesStrictFreqLockLimits[] =
{
    {Gpu::ClkM, {LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DRAM_MIN,
                 LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DRAM_MAX}},
    {Gpu::ClkGpc, {LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_GPC_MIN,
                   LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_GPC_MAX}},
    {Gpu::ClkDisp, {LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DISP_MIN,
                    LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DISP_MAX}},
    {Gpu::ClkPexGen, {LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PCIE_MIN,
                      LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PCIE_MAX}}
};

void ConstructFreqLimit
(
    Gpu::ClkDomain dom,
    UINT32 clkFreqkHz,
    LW2080_CTRL_PERF_LIMIT_INPUT *pInputLimit
)
{
    pInputLimit->type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;
    pInputLimit->flags = 0;
    pInputLimit->data.freq.freqKHz = static_cast<LwU32>(clkFreqkHz);
    pInputLimit->data.freq.domain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
}

// RM provides dedicated perf limits for min, mid, nom and max PerfPoints.
bool RequiresSpecialPStateLimit(Perf::GpcPerfMode perfMode)
{
    return (perfMode == Perf::GpcPerf_MIN) ||
           (perfMode == Perf::GpcPerf_MID) ||
           (perfMode == Perf::GpcPerf_NOM) ||
           (perfMode == Perf::GpcPerf_MAX);
}

} // namespace

RC Perf30::LookupPStateLimits
(
    LW2080_CTRL_PERF_LIMIT_INPUT **pMinLimit,
    LW2080_CTRL_PERF_LIMIT_INPUT **pMaxLimit
)
{
    RC rc;
    switch (m_ForcedPStateLockType)
    {
        case SoftLock:
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PSTATE_MIN, pMinLimit));
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_STRICT_PSTATE_MAX, pMaxLimit));
            break;
        case StrictLock:
        case DefaultLock:
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PSTATE_MIN, pMinLimit));
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_CLIENT_STRICT_PSTATE_MAX, pMaxLimit));
            break;
        case HardLock:
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PSTATE_MIN, pMinLimit));
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PSTATE_MAX, pMaxLimit));
            break;
        default:
            Printf(Tee::PriError, "%s: Invalid PState lock type\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC Perf30::ConstructPStateLimit
(
    const PerfPoint &Setting,
    LW2080_CTRL_PERF_LIMIT_INPUT *pInputLimit
) const
{
    RC rc;
    pInputLimit->type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
    pInputLimit->flags = 0;
    pInputLimit->data.pstate.pstateId = PStateNumTo2080Bit(Setting.PStateNum);
    switch (Setting.SpecialPt)
    {
        case GpcPerf_MIN:
            pInputLimit->data.pstate.point = LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MIN;
            break;
        case GpcPerf_MID:
            pInputLimit->data.pstate.point = LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MID;
            break;
        case GpcPerf_NOM:
            pInputLimit->data.pstate.point = LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_NOM;
            break;
        case GpcPerf_MAX:
            pInputLimit->data.pstate.point = LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MAX;
            break;
        default:
            Printf(Tee::PriError, "%s: Invalid perf mode\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC Perf30::ConstructPStatePerfLimits(const PerfPoint& Setting)
{
    if (!RequiresSpecialPStateLimit(Setting.SpecialPt))
    {
        Printf(Tee::PriError,
            "%s: Not supported for perfpoints except min/mid/nom/max\n",
            __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    LW2080_CTRL_PERF_LIMIT_INPUT *pMinLim, *pMaxLim;
    CHECK_RC(LookupPStateLimits(&pMinLim, &pMaxLim));

    LW2080_CTRL_PERF_LIMIT_INPUT perfLimit = { 0 };
    CHECK_RC(ConstructPStateLimit(Setting, &perfLimit));

    *pMinLim = perfLimit;
    *pMaxLim = perfLimit;

    return rc;
}

RC Perf30::ValidatePerfPoint(const PerfPoint& Setting)
{
    RC rc;

    if (m_ShouldHavePowerCapping &&
        Setting.UnsafeDefault && !m_HasPowerCapping && m_ForcedPStateLockType != HardLock)
    {
        Printf(Tee::PriError,
            "Cannot set gpcclk above 0.tdp without -pstate_hard\n");
        return RC::CANNOT_SET_GRAPHICS_CLOCK;
    }

    if (!m_ArbiterEnabled)
    {
        // We cannot set anything other than an explicit point if arbitration
        // is disabled.
        if (Setting.SpecialPt != GpcPerf_EXPLICIT)
        {
            return RC::BAD_PARAMETER;
        }

        // Remaining validation depends on the RM perf arbiter being enabled.
        return rc;
    }

    bool gpcclkFound = false;
    if (Setting.Clks.find(Gpu::ClkGpc) != Setting.Clks.end() ||
        Setting.Clks.find(Gpu::ClkGpc2) != Setting.Clks.end())
    {
        gpcclkFound = true;
    }

    bool requiresGpcClk = (Setting.SpecialPt == GpcPerf_EXPLICIT) ||
                          (Setting.SpecialPt == GpcPerf_TDP) ||
                          (Setting.SpecialPt == GpcPerf_TURBO);

    if (!gpcclkFound && requiresGpcClk)
    {
        Printf(Tee::PriError,
               "PerfPoint must specify gpcclk\n");
        return RC::BAD_PARAMETER;
    }

    // PStateNum will default to 0 for some intersect types, but the PStateNum
    // itself holds no significance (i.e. we do not set a perf limit for the
    // pstate). On some VBIOSes, there might not be a PState 0, so we should
    // skip checking the pstate number in this case or else we will fail.
    bool requiresValidPStateNum = true;
    if (!Setting.IntersectSettings.empty())
    {
        requiresValidPStateNum = false;
        for (const auto& intersectItr : Setting.IntersectSettings)
        {
            if (intersectItr.Type == Perf::IntersectPState)
                requiresValidPStateNum = true;
        }
    }

    if (requiresValidPStateNum)
    {
        bool pstateExists;
        CHECK_RC(DoesPStateExist(Setting.PStateNum, &pstateExists));
        if (!pstateExists)
        {
            Printf(Tee::PriError, "PState %d is unavailable\n",
                   Setting.PStateNum);
            return RC::BAD_PARAMETER;
        }
    }

    if (Setting.LinkWidth)
        Printf(Tee::PriWarn, "Setting PCIE link width via Perf is deprecated\n");

    // Don't verify Clks if we have a non-VF intersect PerfPoint
    bool requiresValidClks = true;
    if (!Setting.IntersectSettings.empty())
    {
        requiresValidClks = false;
        for (const auto& setting : Setting.IntersectSettings)
        {
            if (setting.Type == IntersectFrequency ||
                setting.Type == IntersectVoltFrequency)
            {
                requiresValidClks = true;
            }
        }
    }
    if (HasVirtualPState(Setting.SpecialPt))
    {
        // We will override this PerfPoint from GetMatchingStdPerfPoint()
        requiresValidClks = false;
    }

    if (RequiresSpecialPStateLimit(Setting.SpecialPt) || !requiresValidClks)
        return rc;

    // Verify that gpcclk falls within the pstate range
    Gpu::ClkDomain GpcDomain = Gpu::ClkGpc;
    if (!IsRmClockDomain(Gpu::ClkGpc))
    {
        GpcDomain = Gpu::ClkGpc2;
    }
    UINT64 settingGpcPerfHz = 0;
    (void) Setting.GetFreqHz(GpcDomain, &settingGpcPerfHz);

    if (settingGpcPerfHz &&
        (m_NoiseAwareDomains.find(GpcDomain) != m_NoiseAwareDomains.end()))
    {
        UINT32 GpcPerfkHz = static_cast<UINT32>(settingGpcPerfHz / 1000);
        if (!IsClockInRange(Setting.PStateNum,
            PerfUtil::GpuClkDomainToCtrl2080Bit(GpcDomain),
            GpcPerfkHz))
        {
            return RC::BAD_PARAMETER;
        }
    }

    // Verify that all clock domains fall within the PState range
    for (const auto& clkItr : Setting.Clks)
    {
        Gpu::ClkDomain dom = clkItr.first;

        // Memory clock is a special case because we will actually adjust the
        // pstate range to allow it.
        if (dom == Gpu::ClkM)
        {
            continue;
        }

        const UINT32 freqkHz = static_cast<UINT32>(clkItr.second.FreqHz / 1000);

        // Make sure it's a RM clock domain
        FLOAT32 freqRatio = 1.0f;
        if (!IsRmClockDomain(dom))
        {
            freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
            dom = PerfUtil::ColwertToOppositeClockDomain(dom);
        }
        const INT32 finalFreqkHz = static_cast<INT32>(freqkHz * freqRatio);

        if (ClkDomainType(dom) == FIXED)
        {
            Printf(Tee::PriError,
                   "Cannot set perf limits for fixed clock domain %s\n",
                   PerfUtil::ClkDomainToStr(dom));
            return RC::ILWALID_CLOCK_DOMAIN;
        }

        if (!IsClockInRange(Setting.PStateNum, PerfUtil::GpuClkDomainToCtrl2080Bit(dom),
                            finalFreqkHz))
        {
            Printf(Tee::PriError, "Requested %s (%dkHz) is outside of P%u range\n",
                PerfUtil::ClkDomainToStr(dom), finalFreqkHz, Setting.PStateNum);
            return RC::BAD_PARAMETER;
        }
    }

    return rc;
}

RC Perf30::ConstructClksPerfLimits(const PerfPoint& Setting, const ClkMap& Clks)
{
    RC rc;

    LW2080_CTRL_PERF_LIMIT_INPUT *pMinLim = nullptr;
    LW2080_CTRL_PERF_LIMIT_INPUT *pMaxLim = nullptr;

    for (const auto& clkItr : Clks)
    {
        if (clkItr.second.Flags & FORCE_IGNORE_LIMIT)
        {
            continue;
        }
        // TODO: Add support for arbitrary PLL frequencies for all domains once
        //       RM fixes the bug where we cannot offset programming entries
        //       with active strict perf limits for the domain we are offsetting
        if (clkItr.first == Gpu::ClkM)
        {
            CHECK_RC(ConfigureDramclkSettings(Setting.PStateNum, clkItr.second));

            // Only send strict limits for dramclk if multiple points are available
            ClkRange dramclkRange;
            CHECK_RC(GetClockRange(Setting.PStateNum, Gpu::ClkM, &dramclkRange));
            if (dramclkRange.MinKHz == dramclkRange.MaxKHz)
                continue;
        }

        Gpu::ClkDomain dom = clkItr.first;
        FreqCtrlType clkType = ClkDomainType(dom);
        UINT32 freqkHz = static_cast<UINT32>(clkItr.second.FreqHz / 1000);

        // Make sure it's a RM clock domain
        FLOAT32 freqRatio = 1.0f;
        if (!IsRmClockDomain(dom))
        {
            freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
            dom = PerfUtil::ColwertToOppositeClockDomain(dom);
        }

        if (clkType == RATIO || (clkItr.second.Flags & FORCE_LOOSE_LIMIT))
        {
            CHECK_RC(LookupPerfLooseLimits(&pMinLim, &pMaxLim));
        }
        else if (clkType == DECOUPLED)
        {
            CHECK_RC(LookupPerfStrictLimits(clkItr.first, &pMinLim, &pMaxLim));
        }
        else if (m_PStateVersion < PStateVersion::PSTATE_35)
        {
            Printf(Tee::PriError, "Cannot program fixed clock domain %s\n",
                PerfUtil::ClkDomainToStr(dom));
            return RC::ILWALID_CLOCK_DOMAIN;
        }
        else
        {
            // Skip this entry if it is a fixed clock and pstates 3.5 is supported.
            // We will handle this later with SetClocksViaInjection().
            continue;
        }

        if (!(pMinLim && pMaxLim))
        {
            return RC::SOFTWARE_ERROR;
        }

        LW2080_CTRL_PERF_LIMIT_INPUT inputLimit = {};
        ConstructFreqLimit(dom, static_cast<UINT32>(freqkHz * freqRatio), &inputLimit);

        *pMinLim = inputLimit;
        *pMaxLim = inputLimit;
    }

    return rc;
}

RC Perf30::SetPStateLockViaLimits(const PerfPoint &Setting)
{
    RC rc;

    // Clear m_PerfLimits, but don't update RM's perf limits yet
    DisableAllPerfLimits();

    // Construct perf limits for MIN, MID, NOM and MAX PerfPoints
    if (RequiresSpecialPStateLimit(Setting.SpecialPt))
    {
        CHECK_RC(ConstructPStatePerfLimits(Setting));
    }
    else
    {
        // If unspecified, CS wants master clock domains pushed to the minimum
        // frequencies in the PState ranges. See bug 1664223
        PerfPoint minPP(Setting.PStateNum, GpcPerf_MIN);
        CHECK_RC(ConstructPStatePerfLimits(minPP));
    }

    ClkMap Clks = Setting.Clks;
    if (Setting.LinkSpeed)
    {
        Clks[Gpu::ClkPexGen] = ClkSetting(Gpu::ClkPexGen,
                               PerfUtil::PciGenToCtrl2080Bit(static_cast<Pci::PcieLinkSpeed>(Setting.LinkSpeed)));
    }

    // Construct perf limits for Setting.Clks
    if (HasVirtualPState(Setting.SpecialPt))
    {
        const PerfPoint stdPerfPoint = GetMatchingStdPerfPoint(Setting);
        if (stdPerfPoint.SpecialPt == NUM_GpcPerfMode)
            return RC::SOFTWARE_ERROR;
        CHECK_RC(ConstructClksPerfLimits(stdPerfPoint, Clks));
    }
    else
    {
        CHECK_RC(ConstructClksPerfLimits(Setting, Clks));
    }

    CHECK_RC(SendPerfPointToRM());

    CHECK_RC(SetClocksViaInjection(Setting, ClockInjectionType::FIXED_CLOCKS));

    return rc;
}

RC Perf30::ResetLimitsIfPStateLockChanged(PStateLockType newLockType)
{
    RC rc;

    if (newLockType != m_ActivePStateLockType &&
        m_ActivePStateLockType != NotLocked)
    {
        PerfPoint lwrrPP;
        CHECK_RC(GetLwrrentPerfPoint(&lwrrPP));
        CHECK_RC(SetPerfPoint(lwrrPP));
    }

    return rc;
}

RC Perf30::SetForcedPStateLockType(PStateLockType type)
{
    RC rc;

    m_ForcedPStateLockType = type;
    CHECK_RC(ResetLimitsIfPStateLockChanged(type));

    return rc;
}

// Regardless of whether we are at CLIENT_LOW or CLIENT_STRICT, we
// should always jump to/from MODS_RULES when requested to do so
RC Perf30::SetUseHardPStateLocks(bool useHard)
{
    RC rc;
    m_ForcedPStateLockType = useHard ? HardLock : DefaultLock;
    CHECK_RC(ResetLimitsIfPStateLockChanged(m_ForcedPStateLockType));
    return rc;
}

RC Perf30::UseIMPCompatibleLimits()
{
    RC rc;

    if (m_ActivePStateLockType != HardLock &&
        m_ActivePStateLockType != StrictLock)
    {
        m_ForcedPStateLockType = SoftLock;
        CHECK_RC(SetForcedPStateLockType(m_ForcedPStateLockType));
    }
    else
    {
        Printf(Tee::PriDebug,
               "Cannot set CLIENT_LOW perf limits due to PState %s\n",
               m_ActivePStateLockType == HardLock ? "HardLock" : "StrictLock");
    }

    return rc;
}

RC Perf30::RestoreDefaultLimits()
{
    RC rc;

    if (m_ActivePStateLockType != HardLock &&
        m_ActivePStateLockType != StrictLock)
    {
        m_ForcedPStateLockType = DefaultLock;
        CHECK_RC(SetForcedPStateLockType(m_ForcedPStateLockType));
    }
    else if (m_ActivePStateLockType == HardLock)
    {
        Printf(Tee::PriWarn,
               "Cannot set default perf limit priorities due to PState HardLock\n");
    }

    return rc;
}

RC Perf30::ClearForcedPState()
{
    return ClearPerfLimits();
}

RC Perf30::ClearForcePStateEx(UINT32 flags)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::ClearPerfLimits()
{
    RC rc;
    Printf(Tee::PriLow, "ClearPerfLimits: Clearing perf limits\n");

    if (m_UsePStateCallbacks)
    {
        CallbackArguments args;
        args.Push(m_pSubdevice->GetGpuInst());
        CHECK_RC(m_Callbacks[PRE_CLEANUP].Fire(Callbacks::STOP_ON_ERROR));
    }

    DisableAllPerfLimits();
    CHECK_RC(SendPerfLimitsToRM());

    // If all perf limits have been cleared, then we are not locked
    m_ActivePStateLockType = NotLocked;

    // Reset m_LwrPerfPoint since we have cleared all perf limits
    m_LwrPerfPoint = PerfPoint();

    return rc;
}

RC Perf30::SendPerfLimitsToRM()
{
    RC rc;
    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS limitParams = {0};
    limitParams.numLimits = static_cast<LwU32>(m_PerfLimits.size());
    limitParams.pLimits = LW_PTR_TO_LwP64(&m_PerfLimits[0]);

    if (m_ActivePStateLockType != m_ForcedPStateLockType)
    {
        Printf(Tee::PriDebug, "Switching to ");
        switch (m_ForcedPStateLockType)
        {
            case HardLock:
                Printf(Tee::PriDebug, "MODS_RULES");
                break;
            case StrictLock:
            case DefaultLock:
                Printf(Tee::PriDebug, "CLIENT_STRICT");
                break;
            case SoftLock:
                Printf(Tee::PriDebug, "CLIENT_LOW");
                break;
            default:
                Printf(Tee::PriError, "\nunknown limit type)\n");
                return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriDebug, " perf limit priority\n");
    }

    // Set the flag to track the perf changes requested by MODS
    m_ClockChangeCounter.SetIsClockChangeExpected(true);

    const UINT64 startTime = Xp::QueryPerformanceCounter();

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice, LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
        &limitParams, sizeof(limitParams)));

    UINT64 endTime = Xp::QueryPerformanceCounter();
    UINT64 elapsedTimeUs =
        (endTime - startTime) * 1000000 / Xp::QueryPerformanceFrequency();
    UpdateVfSwitchStats(VF_SWITCH_RM_LIMITS, elapsedTimeUs);

    if (s_LimitVerbosity != NO_LIMITS)
    {
        CHECK_RC(PrintPerfLimits());
    }

    CHECK_RC(HandleRmChgSeq());

    return rc;
}

// In order to lock clocks/voltages for a PerfPoint, MODS sends perf
// limits to the RM. When we lock to a non-POR memory clock frequency,
// we have to also adjust a memory clock programming entry offset. Sometimes
// we have to send perf limits before altering the dramclk programming entry
// and vice-versa. This function determines in what order we need to call
// those functions to avoid "glitching" up to the wrong dramclk frequency.
RC Perf30::SendPerfPointToRM()
{
    RC rc;

    DEFER
    {
        m_LwrDramclkVFPointIdx = m_NewDramclkVFPointIdx;
    };

    INT32 oldOffsetkHz = 0;
    for (const auto& progEntry : m_AdjustedDramclkProgEntries)
    {
        if (progEntry.index == m_NewDramclkVFPointIdx)
        {
            oldOffsetkHz = progEntry.offsetkHz;
            break;
        }
    }

    if (m_NewDramclkVFPointOffsetkHz == oldOffsetkHz)
    {
        return SendPerfLimitsToRM();
    }

    MASSERT(m_NewDramclkVFPointIdx != ProgEntrySetting::ILWALID_IDX);
    if (m_NewDramclkVFPointIdx == m_LwrDramclkVFPointIdx &&
        m_NewDramclkVFPointOffsetkHz < oldOffsetkHz)
    {
        CHECK_RC(SendPerfLimitsToRM());
        CHECK_RC(SetClkProgrammingTableOffsetkHz(
            m_NewDramclkVFPointIdx,
            m_NewDramclkVFPointOffsetkHz));
    }
    else
    {
        CHECK_RC(SetClkProgrammingTableOffsetkHz(
            m_NewDramclkVFPointIdx,
            m_NewDramclkVFPointOffsetkHz));
        CHECK_RC(SendPerfLimitsToRM());
    }

    // Keep track of any programming entries we have adjusted, even if
    // we reset their offset to 0.
    for (auto& progEntry : m_AdjustedDramclkProgEntries)
    {
        if (progEntry.index == m_NewDramclkVFPointIdx)
        {
            progEntry.offsetkHz = m_NewDramclkVFPointOffsetkHz;
            return rc;
        }
    }
    m_AdjustedDramclkProgEntries.emplace_back(
        m_NewDramclkVFPointIdx, m_NewDramclkVFPointOffsetkHz);

    return rc;
}

RC Perf30::PrintPerfLimits() const
{
    RC rc;

    Printf(Tee::PriNormal, "New perf limits:\n");

    LW2080_CTRL_PERF_PERF_LIMITS_STATUS limitsStatus = {};
    CHECK_RC(GetPerfLimitsStatus(&limitsStatus));

    UINT32 limitIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(limitIdx, &m_CachedRmInfo.perfLimits.super.objMask.super)
        const auto& limitInfo = m_CachedRmInfo.perfLimits.limits[limitIdx];
        const auto& limitStatus = limitsStatus.limits[limitIdx];
        bool bEnabled = false;

        if (limitInfo.type == LW2080_CTRL_PERF_PERF_LIMIT_TYPE_2X)
        {
            bEnabled = (limitStatus.data.v2x.input.type !=
                LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED);
        }
        else // PMU perf limits implementation
        {
            bEnabled = LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_ACTIVE(
                &limitStatus.data.pmu.clientInput);
        }

        if (!bEnabled && (s_LimitVerbosity & ONLY_ENABLED_LIMITS))
            continue;

        PrintPerfLimit(limitIdx, limitInfo, limitStatus);

        if (limitInfo.type == LW2080_CTRL_PERF_PERF_LIMIT_TYPE_35)
        {
            PrintPerfLimitStatus_v35(limitInfo, limitStatus);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    PrintArbiterOutput(limitsStatus);

    if (m_CachedRmInfo.perfLimits.version != LW2080_CTRL_PERF_PERF_LIMITS_VERSION_2X)
    {
        PrintArbiterOutput_PMU(limitsStatus);
    }
    if (m_CachedRmInfo.perfLimits.version == LW2080_CTRL_PERF_PERF_LIMITS_VERSION_35)
    {
        PrintArbiterOutput_v35(limitsStatus);
    }

    return rc;
}

void Perf30::PrintPerfLimit
(
    UINT32 limIdx,
    const LW2080_CTRL_PERF_PERF_LIMIT_INFO& limitInfo,
    const LW2080_CTRL_PERF_PERF_LIMIT_STATUS& limitStatus
) const
{
    Printf(Tee::PriNormal, " %s priority=%d flags=%s%s propagationRegime=%u\n",
        limitInfo.szName, limIdx,
        FLD_TEST_DRF(2080, _CTRL_PERF_LIMIT_INFO_FLAGS, _MIN, _TRUE, limitInfo.flags) ?
            "min" : "",
        FLD_TEST_DRF(2080, _CTRL_PERF_LIMIT_INFO_FLAGS, _MAX, _TRUE, limitInfo.flags) ?
            "max" : "",
        limitInfo.propagationRegime);

    const auto& limitData = limitStatus.clientInput.data;

    string limitStr = "  type=";
    switch (limitStatus.clientInput.type)
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_DISABLED:
            limitStr += "DISABLED\n";
            break;
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_PSTATE_IDX:
            limitStr += Utility::StrPrintf("pstate pstateIdx=%d",
                limitData.pstate.pstateIdx);
            switch (limitData.pstate.point)
            {
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_NOM:
                    limitStr += " point=nom";
                    break;
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MIN:
                    limitStr += " point=min";
                    break;
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MAX:
                    limitStr += " point=max";
                    break;
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MID:
                    limitStr += " point=mid";
                    break;
                default:
                    break;
            }
            limitStr += "\n";
            break;
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_FREQUENCY:
        {
            const Gpu::ClkDomain dom = GetDomainFromClockProgrammingOrder(
                limitData.freq.clkDomainIdx);
            limitStr += Utility::StrPrintf("freq %s freqKHz=%u\n",
                PerfUtil::ClkDomainToStr(dom), limitData.freq.freqKHz);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VPSTATE_IDX:
            limitStr += Utility::StrPrintf("vpstate vpstateIdx=%u\n",
                limitData.vpstate.vpstateIdx);
            break;
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VOLTAGE:
            limitStr += Utility::StrPrintf("voltage railIdx=%u lwrrVoltuV=%u deltauV=%d\n",
                limitData.voltage.voltRailIdx,
                limitData.voltage.lwrrVoltageuV,
                limitData.voltage.deltauV);
            for (LwU8 limIdx = 0; limIdx < limitData.voltage.numElements; limIdx++)
            {
                const auto& voltElem = limitData.voltage.elements[limIdx];
                limitStr += "   subType=";
                switch (voltElem.type)
                {
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_LOGICAL:
                        limitStr += Utility::StrPrintf("logical voltageuV=%u",
                            voltElem.data.logical.voltageuV);
                        break;
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VFE:
                        limitStr += Utility::StrPrintf("vfe vfeEquIdx=%u vfeMonIdx=%u",
                            voltElem.data.vfe.vfeEquIdx, voltElem.data.vfe.vfeEquMonIdx);
                        break;
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_PSTATE:
                        limitStr += Utility::StrPrintf("pstate pstateIdx=%u freqType=%u",
                            voltElem.data.pstate.pstateIdx, voltElem.data.pstate.freqType);
                        break;
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VPSTATE:
                        limitStr += Utility::StrPrintf("vpstate vpstateIdx=%u",
                            voltElem.data.vpstateIdx.vpstateIdx);
                        break;
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_FREQUENCY:
                    {
                        const Gpu::ClkDomain dom = GetDomainFromClockProgrammingOrder(
                            voltElem.data.frequency.clkDomainIdx);
                        limitStr += Utility::StrPrintf("freq %s freqKHz=%u",
                            PerfUtil::ClkDomainToStr(dom), voltElem.data.frequency.freqKHz);
                        break;
                    }
                    default:
                        break;
                }
                limitStr += Utility::StrPrintf(" lwrrVoltuV=%u deltauV=%d\n",
                    voltElem.lwrrVoltageuV, voltElem.deltauV);
            }
            break;
        default:
            MASSERT(!"Unknown perf limit clientInput type");
            break;
    }
    Printf(Tee::PriNormal, "%s", limitStr.c_str());
}

void Perf30::PrintPerfLimitStatus_v35
(
    const LW2080_CTRL_PERF_PERF_LIMIT_INFO& limitInfo,
    const LW2080_CTRL_PERF_PERF_LIMIT_STATUS& limitStatus
) const
{
    Printf(Tee::PriNormal,
        "  v35 info: strictPropMask=0x%08x clkVminNoiseUnaware=0x%08x bPStateContrained=%s\n",
        limitInfo.data.v35.clkDomainStrictPropagationMask.super.pData[0],
        limitInfo.data.v35.clkDomainsMaskForceVminNoiseUnaware.super.pData[0],
        limitInfo.data.v35.bStrictPropagationPstateConstrained ? "true" : "false");

    const auto& arbInput = limitStatus.data.v35.arbInput;
    Printf(Tee::PriNormal, "  v35 arbInput:\n");
    UINT32 voltIdx;

    for (UINT32 tupleIdx = 0; tupleIdx < 2; tupleIdx++)
    {
        if (tupleIdx == LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN &&
            !FLD_TEST_DRF(2080, _CTRL_PERF_LIMIT_INFO_FLAGS, _MIN, _TRUE, limitInfo.flags))
        {
            continue;
        }
        if (tupleIdx == LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX &&
            !FLD_TEST_DRF(2080, _CTRL_PERF_LIMIT_INFO_FLAGS, _MAX, _TRUE, limitInfo.flags))
        {
            continue;
        }

        const auto& tuple = arbInput.tuples[tupleIdx];
        Printf(Tee::PriNormal, "   %s Tuple:\n", tupleIdx ? "MAX" : "MIN");
        Printf(Tee::PriNormal, "    pstateIdx=%u\n", tuple.pstateIdx);

        UINT32 clkIdx;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx, &arbInput.clkDomainsMask.super)
            Printf(Tee::PriNormal, "    %s freqKHz=%u\n",
                PerfUtil::ClkDomainToStr(GetDomainFromClockProgrammingOrder(clkIdx)),
                tuple.clkDomains[clkIdx]);
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(voltIdx, &arbInput.voltRailsMask.super)
            Printf(Tee::PriNormal, "    railIdx=%u voltuV=%u\n",
                voltIdx, tuple.voltRails[voltIdx]);
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    }

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(voltIdx, &arbInput.voltRailsMask.super)
        Printf(Tee::PriNormal, "   voltageMaxLooseuV=%u railIdx=%u\n",
            arbInput.voltageMaxLooseuV[voltIdx].value, voltIdx);
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
}

void Perf30::PrintArbiterOutput
(
    const LW2080_CTRL_PERF_PERF_LIMITS_STATUS& limitsStatus
) const
{
    Printf(Tee::PriNormal, "ARBITER OUTPUT:\n");
    Printf(Tee::PriNormal, " applySeqId=%u\n", limitsStatus.applySeqId);
    Printf(Tee::PriNormal, " arbSeqId=%u\n", limitsStatus.arbSeqId);

    Printf(Tee::PriNormal, " arbOutputApply:\n");
    Printf(Tee::PriNormal, "  cacheData:\n");
    Printf(Tee::PriNormal, "   bDirty=%s\n",
        limitsStatus.arbOutputApply.tuple.cacheData.bDirty ? "true" : "false");
    Printf(Tee::PriNormal, "   arbSeqId=%u\n",
        limitsStatus.arbOutputApply.tuple.cacheData.arbSeqId);
    Printf(Tee::PriNormal, "   limitMaskExclude[0]=0x%08x\n",
        limitsStatus.arbOutputApply.tuple.cacheData.limitMaskExclude.super.pData[0]);
    for (UINT32 ii = 0; ii < 7; ii++)
    {
        Printf(Tee::PriNormal, "   limitMaskExclude[%u]=0x%08x\n",
            ii + 1,
            limitsStatus.arbOutputApply.tuple.cacheData.limitMaskExclude.pDataE255[ii]);
    }

    const auto& pstateIdx = limitsStatus.arbOutputApply.tuple.pstateIdx;
    Printf(Tee::PriNormal, "  pstateIdx=%u limitIdx=%u %s\n",
        pstateIdx.value,
        pstateIdx.limitIdx,
        m_CachedRmInfo.perfLimits.limits[pstateIdx.limitIdx].szName);

    UINT32 clkIdx, voltIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx,
        &limitsStatus.arbOutputApply.clkDomainsMask.super)

        const auto& freqkHz = limitsStatus.arbOutputApply.tuple.clkDomains[clkIdx].freqkHz;
        Printf(Tee::PriNormal, "  %s freqKHz=%u limitIdx=%u %s\n",
            PerfUtil::ClkDomainToStr(GetDomainFromClockProgrammingOrder(clkIdx)),
            freqkHz.value,
            freqkHz.limitIdx,
            m_CachedRmInfo.perfLimits.limits[freqkHz.limitIdx].szName);

    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(voltIdx,
        &limitsStatus.arbOutputApply.voltRailsMask.super)

        const auto& voltageuV = limitsStatus.arbOutputApply.tuple.voltRails[voltIdx].voltageuV;
        Printf(Tee::PriNormal, "  voltRailIdx=%u voltuV=%u limitIdx=%u %s\n",
            voltIdx,
            voltageuV.value,
            voltageuV.limitIdx,
            m_CachedRmInfo.perfLimits.limits[voltageuV.limitIdx].szName);

        const auto& vmin = limitsStatus.arbOutputApply.tuple.
            voltRails[voltIdx].voltageNoiseUnawareMinuV;
        Printf(Tee::PriNormal, "  voltRailIdx=%u voltNoiseUnawareVminuV=%u limitIdx=%u %s\n",
            voltIdx,
            vmin.value,
            vmin.limitIdx,
            m_CachedRmInfo.perfLimits.limits[vmin.limitIdx].szName);

    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
}

void Perf30::PrintArbiterOutput_PMU
(
    const LW2080_CTRL_PERF_PERF_LIMITS_STATUS& limitsStatus
) const
{
    Printf(Tee::PriNormal, "arbOutputDefault:\n");

    UINT32 clkIdx;
    for (UINT32 tupleIdx = 0;
         tupleIdx < LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS;
         tupleIdx++)
    {
        const auto& tuple = limitsStatus.data.pmu.arbOutputDefault.tuples[tupleIdx];
        Printf(Tee::PriNormal, " %s Tuple:\n", tupleIdx ? "MAX" : "MIN");
        Printf(Tee::PriNormal, "  pstateIdx=%u\n", tuple.pstateIdx);
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx,
            &limitsStatus.data.pmu.arbOutputDefault.clkDomainsMask.super)

            Printf(Tee::PriNormal, "  %s=%u kHz\n",
                PerfUtil::ClkDomainToStr(GetDomainFromClockProgrammingOrder(clkIdx)),
                tuple.clkDomains[clkIdx]);

        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    }

    // TODO: Decode limitsStatus.data.pmu.changeSeqChange
}

void Perf30::PrintArbiterOutput_v35
(
    const LW2080_CTRL_PERF_PERF_LIMITS_STATUS& limitsStatus
) const
{
    Printf(Tee::PriNormal, "arbOutputApply:\n");

    Volt3x *pVolt3x = m_pSubdevice->GetVolt3x();
    UINT32 clkIdx;

    for (UINT32 tupleIdx = 0;
         tupleIdx < LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS;
         tupleIdx++)
    {
        const auto& tuple = limitsStatus.data.v35.arbOutput35Apply.tuples[tupleIdx];
        Printf(Tee::PriNormal, " %s Tuple:\n", tupleIdx ? "MAX" : "MIN");
        Printf(Tee::PriNormal, "  pstateIdx=%u limitIdx=%u\n",
            tuple.pstateIdx.value, tuple.pstateIdx.limitIdx);
        Printf(Tee::PriNormal, "  Noise unaware frequencies:\n");
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx, &m_CachedRmInfo.clkDomains.super.objMask.super)
            Printf(Tee::PriNormal, "   %s=%ukHz limitIdx=%u\n",
                PerfUtil::ClkDomainToStr(GetDomainFromClockProgrammingOrder(clkIdx)),
                tuple.clkDomains[clkIdx].freqNoiseUnawareMinkHz.value,
                tuple.clkDomains[clkIdx].freqNoiseUnawareMinkHz.limitIdx);
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        Printf(Tee::PriNormal, "  Voltage Min Loose:\n");
        UINT32 voltRailMask = pVolt3x->GetVoltRailMask();
        while (voltRailMask)
        {
            UINT32 railIdx = Utility::BitScanForward(voltRailMask);
            voltRailMask ^= 1<<railIdx;

            Printf(Tee::PriNormal, "   rail%u=%uuV limitIdx=%u\n",
                railIdx,
                tuple.voltRails[railIdx].voltageMinLooseuV.value,
                tuple.voltRails[railIdx].voltageMinLooseuV.limitIdx);
        }
    }
}

RC Perf30::SetIntersectPt(const PerfPoint &setting)
{
    RC rc;

    // Clear m_PerfLimits, but don't update RM's limits
    DisableAllPerfLimits();

    // Verify that this PerfPoint has valid IntersectSettings
    CHECK_RC(CheckIntersectSettings(setting.IntersectSettings));

    // This is the perf limit that sets an intersect
    LW2080_CTRL_PERF_LIMIT_INPUT intersectLimit = {0};

    // Create a VOLTAGE_3X perf limit based on the PerfPoint's IntersectSettings
    CHECK_RC(ConstructIntersectLimits(setting, &intersectLimit));

    // Create any upper bound limits, if applicable
    vector<LW2080_CTRL_PERF_LIMIT_INPUT> upperBoundLimits;
    CHECK_RC(ConstructBoundLimits(setting, &upperBoundLimits));

    // VFE intersect limits work correctly only on ps3.5+
    if (m_PStateVersion >= PStateVersion::PSTATE_35)
    {
        CHECK_RC(AppendVminLimit(&intersectLimit));
    }

    // Add intersectLimit/upperBoundLimits to m_PerfLimits
    CHECK_RC(AssignIntersectAndBoundLimits(intersectLimit, upperBoundLimits));

    // Apply any clock frequency overrides in Clks
    ClkMap Clks = setting.Clks;
    if (setting.LinkSpeed)
    {
        Clks[Gpu::ClkPexGen] = ClkSetting(Gpu::ClkPexGen,
                               PerfUtil::PciGenToCtrl2080Bit(static_cast<Pci::PcieLinkSpeed>(setting.LinkSpeed)));
    }
    CHECK_RC(ConstructClksPerfLimits(setting, Clks));

    // Send perf limits and required programming entry offsets to RM
    return SendPerfPointToRM();
}

RC Perf30::ConstructIntersectLimits
(
    const PerfPoint& perfPoint,
    LW2080_CTRL_PERF_LIMIT_INPUT* pIntersectLimit
)
{
    RC rc;

    pIntersectLimit->type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE_3X;
    pIntersectLimit->data.volt3x.voltDomain =
        PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(
            perfPoint.IntersectSettings.begin()->VoltageDomain);
    pIntersectLimit->data.volt3x.numElements = 0;

    for (const auto& is : perfPoint.IntersectSettings)
    {
        CHECK_RC(InnerConstructIntersectLimit(
            is, perfPoint.PStateNum, pIntersectLimit));
    }

    return rc;
}

RC Perf30::InnerConstructIntersectLimit
(
    const IntersectSetting& is,
    UINT32 pstateNum,
    LW2080_CTRL_PERF_LIMIT_INPUT* pIntersectLimit
)
{
    RC rc;

    LW2080_CTRL_PERF_VOLT_DOM_INFO& intersectInfo =
        pIntersectLimit->data.volt3x.info[m_IntersectLimitIdx++];
    pIntersectLimit->data.volt3x.numElements++;

    switch (is.Type)
    {
        case IntersectLogical:
        {
            intersectInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
            intersectInfo.data.logical.logicalVoltageuV = is.LogicalVoltageuV;
            break;
        }

        case IntersectVDT:
        {
            intersectInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT;
            intersectInfo.data.vdt.vdtIndex = is.VDTIndex;
            break;
        }

        case IntersectVFE:
        {
            intersectInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VFE;
            intersectInfo.data.vfe.vfeEquIndex = is.VFEEquIndex;
            break;
        }

        case IntersectPState:
        {
            UINT32 rmPStateLvl;
            CHECK_RC(GetRmPStateLevel(pstateNum, &rmPStateLvl));

            // These parameters should ask RM to take pstate's PStateNum lowest frequencies
            // (note _PSTATE_FREQ_TYPE_MIN below) and callwlate needed voltages for all of
            // them in context of the selected rail (setting.IntersectVoltageDomain). Then take
            // the max value and use it to select voltage on the rail. The actual frequencies
            // for all the clock domains are then derived from that voltage.

            intersectInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_PSTATE;

            // the function that is called with this Info parameter assumes that index == level,
            // so we need to pass the pstate level in instead of its direct index.
            intersectInfo.data.pstate.pstateIndex = rmPStateLvl;
            intersectInfo.data.pstate.freqType =
                LW2080_CTRL_PERF_VOLT_DOM_INFO_PSTATE_FREQ_TYPE_MIN;
            break;
        }

        case IntersectVPState:
        {
            intersectInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VPSTATE;
            intersectInfo.data.vpstate.vpstateIndex = is.VPStateIndex;
            break;
        }

        case IntersectVoltFrequency:
        {
            Gpu::ClkDomain dom = is.Frequency.ClockDomain;
            UINT32 freqkHz = is.Frequency.ValueKHz;

            // Make sure it's a valid RM clock domain
            FLOAT32 freqRatio = 1.0f;
            if (!IsRmClockDomain(dom))
            {
                freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
                dom = PerfUtil::ColwertToOppositeClockDomain(dom);
            }

            intersectInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_FREQUENCY;
            intersectInfo.data.freq.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
            intersectInfo.data.freq.freqKHz = static_cast<LwU32>(freqkHz * freqRatio);
            break;
        }

        case IntersectFrequency:
        {
            Gpu::ClkDomain dom = is.Frequency.ClockDomain;
            UINT32 freqkHz = is.Frequency.ValueKHz;

            // Make sure it's a valid RM clock domain
            FLOAT32 freqRatio = 1.0f;
            if (!IsRmClockDomain(dom))
            {
                freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
                dom = PerfUtil::ColwertToOppositeClockDomain(dom);
            }

            intersectInfo.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;
            intersectInfo.data.freq.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
            intersectInfo.data.freq.freqKHz = static_cast<LwU32>(freqkHz * freqRatio);
            break;
        }

        default:
            Printf(Tee::PriError, "Unrecognized intersect type\n");
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

// These are additional perf limits that keep RM within certain bounds:
//  * For IntersectPState, this is a loose limit that keeps us locked
//      to the input PState
//  * For IntersectFrequency/IntersectVoltFrequency, this is a strict or
//      loose limit that keeps us locked to the input frequency
//  * For IntersectVPState, this is a loose limit that keeps us to the
//      input VPState
RC Perf30::ConstructBoundLimits
(
    const PerfPoint& perfPoint,
    vector<LW2080_CTRL_PERF_LIMIT_INPUT>* pBoundLimits
) const
{
    MASSERT(pBoundLimits);
    MASSERT(perfPoint.IntersectSettings.size() <= LW2080_CTRL_PERF_LIMIT_VOLTAGE_DATA_ELEMENTS_MAX);
    RC rc;

    for (const auto& is : perfPoint.IntersectSettings)
    {
        LW2080_CTRL_PERF_LIMIT_INPUT upperBoundLimit = {0};
        upperBoundLimit.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;

        switch (is.Type)
        {
            case IntersectPState:
            {
                upperBoundLimit.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
                upperBoundLimit.data.pstate.pstateId = PStateNumTo2080Bit(perfPoint.PStateNum);
                upperBoundLimit.data.pstate.point =
                    LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_LAST;
                break;
            }
            case IntersectVPState:
            {
                upperBoundLimit.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VPSTATE;
                upperBoundLimit.data.vpstate.vpstate = is.VPStateIndex;
                upperBoundLimit.data.vpstate.vpstateIdx = is.VPStateIndex;
                break;
            }
            case IntersectVoltFrequency:
            case IntersectFrequency:
            {
                Gpu::ClkDomain dom = is.Frequency.ClockDomain;
                UINT32 freqkHz = is.Frequency.ValueKHz;

                // Make sure it's a valid RM clock domain
                FLOAT32 freqRatio = 1.0f;
                if (!IsRmClockDomain(dom))
                {
                    freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
                    dom = PerfUtil::ColwertToOppositeClockDomain(dom);
                }

                upperBoundLimit.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;
                upperBoundLimit.data.freq.domain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
                upperBoundLimit.data.freq.freqKHz = static_cast<LwU32>(freqkHz * freqRatio);
                break;
            }
            default:
                break;
        }

        pBoundLimits->push_back(upperBoundLimit);
    }

    return rc;
}

RC Perf30::AssignIntersectAndBoundLimits
(
    const LW2080_CTRL_PERF_LIMIT_INPUT& intersectLimit,
    const vector<LW2080_CTRL_PERF_LIMIT_INPUT>& boundLimits
)
{
    RC rc;

    LW2080_CTRL_PERF_LIMIT_INPUT *pPStateIntLimit;
    CHECK_RC(LookupPerfIntersectLimit(&pPStateIntLimit));
    *pPStateIntLimit = intersectLimit;

    for (const auto& upperBoundLimit : boundLimits)
    {
        if (upperBoundLimit.type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE)
        {
            LW2080_CTRL_PERF_LIMIT_INPUT *pPStateLooseLimitMin;
            LW2080_CTRL_PERF_LIMIT_INPUT *pPStateLooseLimitMax;
            CHECK_RC(LookupPerfLooseLimits(&pPStateLooseLimitMin, &pPStateLooseLimitMax));
            *pPStateLooseLimitMax = upperBoundLimit;
        }
        else if (upperBoundLimit.type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ)
        {
            LW2080_CTRL_PERF_LIMIT_INPUT *pFreqLimitMin;
            LW2080_CTRL_PERF_LIMIT_INPUT *pFreqLimitMax;

            // Figure out the clock domain based on the bound limit
            Gpu::ClkDomain clkDom;
            clkDom = PerfUtil::ClkCtrl2080BitToGpuClkDomain(upperBoundLimit.data.freq.domain);

            if (clkDom == Gpu::ClkUnkUnsp)
            {
                Printf(Tee::PriError, "Cannot figure out clock domain for bounding perf limit\n");
                return RC::SOFTWARE_ERROR;
            }

            if (PerfUtil::Is2xClockDomain(clkDom))
                clkDom = PerfUtil::ColwertTo1xClockDomain(clkDom);

            FreqCtrlType clkType = ClkDomainType(clkDom);
            if (clkType == DECOUPLED)
            {
                CHECK_RC(LookupPerfStrictLimits(clkDom,
                                                &pFreqLimitMin, &pFreqLimitMax));
            }
            else if (clkType == RATIO)
            {
                CHECK_RC(LookupPerfLooseLimits(&pFreqLimitMin, &pFreqLimitMax));
            }
            else // (clkType == FIXED)
            {
                Printf(Tee::PriError,
                       "Cannot intersect to %s clock domain\n",
                       PerfUtil::ClkDomainToStr(clkDom));
                return RC::ILWALID_CLOCK_DOMAIN;
            }

            *pFreqLimitMax = upperBoundLimit;
        }
        else if (upperBoundLimit.type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VPSTATE)
        {
            LW2080_CTRL_PERF_LIMIT_INPUT *pVPStateLimitMin;
            LW2080_CTRL_PERF_LIMIT_INPUT *pVPStateLimitMax;
            CHECK_RC(LookupPerfLooseLimits(&pVPStateLimitMin, &pVPStateLimitMax));
            *pVPStateLimitMax = upperBoundLimit;
        }
    }

    return rc;
}

RC Perf30::CheckIntersectSettings(const set<IntersectSetting>& intersectSettings)
{
    RC rc;

    // The RM Volt3x perf limit type only allows a certain number of elements
    if (intersectSettings.size() > LW2080_CTRL_PERF_LIMIT_VOLTAGE_DATA_ELEMENTS_MAX)
    {
        Printf(Tee::PriError,
               "PerfPoint cannot specify more than %u intersect settings\n",
               LW2080_CTRL_PERF_LIMIT_VOLTAGE_DATA_ELEMENTS_MAX);
    }

    if (intersectSettings.empty())
    {
        Printf(Tee::PriError,
               "PerfPoint must have at least valid IntersectSetting\n");
        return RC::BAD_PARAMETER;
    }

    Volt3x* pVolt3x = m_pSubdevice->GetVolt3x();
    Gpu::SplitVoltageDomain voltDom = intersectSettings.begin()->VoltageDomain;
    for (const auto& is : intersectSettings)
    {
        if (!pVolt3x || !pVolt3x->IsInitialized() || !pVolt3x->HasDomain(is.VoltageDomain))
        {
            Printf(Tee::PriError,
                   "Cannot intersect to %s voltage domain\n",
                   PerfUtil::SplitVoltDomainToStr(is.VoltageDomain));
            return RC::ILWALID_VOLTAGE_DOMAIN;
        }
        if (is.VoltageDomain != voltDom)
        {
            Printf(Tee::PriError, "Cannot intersect to multiple voltage domains at once\n");
            return RC::ILWALID_VOLTAGE_DOMAIN;
        }
    }

    return rc;
}

RC Perf30::AppendVminLimit(LW2080_CTRL_PERF_LIMIT_INPUT* pIntersectLimit)
{
    MASSERT(pIntersectLimit);

    RC rc;

    IntersectSetting vminIS;
    vminIS.Type = IntersectVFE;
    vminIS.VoltageDomain = m_pSubdevice->GetVolt3x()->RmVoltDomToGpuSplitVoltageDomain(
        pIntersectLimit->data.volt3x.voltDomain);
    CHECK_RC(m_pSubdevice->GetVolt3x()->GetRailLimitVfeEquIdx(
        vminIS.VoltageDomain, Volt3x::VMIN_LIMIT, &vminIS.VFEEquIndex));

    // Make sure the VFE index is valid. A VBIOS is not strictly required to
    // have a rail Vmin limit, especially for internal SKUs and bringup ROMs.
    if (vminIS.VFEEquIndex != 0xFF)
    {
        CHECK_RC(InnerConstructIntersectLimit(vminIS, ILWALID_PSTATE, pIntersectLimit));
    }

    CHECK_RC(m_pSubdevice->GetVolt3x()->GetCachedRailLimitOffsetsuV(
        vminIS.VoltageDomain,
        Volt3x::VMIN_LIMIT,
        &pIntersectLimit->data.volt3x.info[m_IntersectLimitIdx - 1].voltageDeltauV.value));

    return rc;
}

RC Perf30::ClearVoltTuningOffsets()
{
    StickyRC rc;

    memset(&m_RailVoltTuningOffsetsuV, 0, sizeof(m_RailVoltTuningOffsetsuV));
    for (auto& rail : m_RailClkDomailwoltTuningOffsetsuV)
    {
        for (auto& clkDomOffset : rail)
        {
            clkDomOffset.second = 0;
        }
    }
    for (auto& offset : m_FreqClkDomainOffsetkHz)
    {
        offset.second = 0;
    }
    for (auto& offset : m_FreqClkPercentOffsets)
    {
        offset.second = 0;
    }

    rc = Perf::ClearVoltTuningOffsets();
    rc = SetVoltTuningParams(writeIfChanged);

    for (auto& rail : m_RailClkDomailwoltTuningOffsetsuV)
    {
        rail.clear();
    }
    m_FreqClkDomainOffsetkHz.clear();
    m_FreqClkPercentOffsets.clear();

    rc = ClearDramclkProgEntryOffsets();

    return rc;
}

RC Perf30::GetClkDomailwfEntryTables
(
    Gpu::ClkDomain dom,
    UINT32 clkProgIdxFirst,
    UINT32 clkProgIdxLast,
    const LW2080_CTRL_CLK_CLK_PROGS_INFO &clkProgTable,
    const LW2080_CTRL_CLK_CLK_VF_POINTS_INFO &vfPointsInfo
)
{
    RC rc;
    const Tee::Priority pri = (m_DumpVfTable) ? Tee::PriNormal : Tee::PriSecret;
    Printf(pri, "VF points for domain: %s (0x%x)\n",
           PerfUtil::ClkDomainToStr(dom),
           PerfUtil::GpuClkDomainToCtrl2080Bit(dom));

    // Get all of the vf points for the clock domain
    VfPoints domainPoints;
    CHECK_RC(GetClkDomailwfPoints(
        pri, dom, clkProgIdxFirst, clkProgIdxLast, clkProgTable, vfPointsInfo, &domainPoints));

    // Can't use GetAvailablePStates here belwase Init isn't finished at this point
    for (const auto& cachePStateItr : m_CachedPStates)
    {
        // One VfEntryTbl per pstate per domain
        VfEntryTbl newTbl;
        newTbl.Domain = dom;
        newTbl.PStateMask = PStateNumTo2080Bit(cachePStateItr.PStateNum);
        // For each pstate, figure out which points go into that pstate
        CHECK_RC(FindVfPointsInPState(
            dom, cachePStateItr.PStateNum, domainPoints, &newTbl.Points));
        m_VfEntryTbls.push_back(newTbl);
    }

    for (const auto& domPtsItr : domainPoints)
    {
        m_ClkDomRmVfIndices[dom].push_back(domPtsItr.VfIdx);
    }

    return rc;
}

RC Perf30::FindVfPointsInPState
(
    Gpu::ClkDomain dom,
    UINT32 pstateNum,
    const VfPoints &allPoints,
    VfPoints *pPStatePoints
)
{
    MASSERT(pPStatePoints);
    RC rc;
    UINT32 index;
    CHECK_RC(GetPStateIdx(pstateNum, &index));

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(dom))
    {
        freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
        dom = PerfUtil::ColwertToOppositeClockDomain(dom);
    }

    UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
    if (m_CachedPStates[index].ClkVals.find(rmDomain) ==
        m_CachedPStates[index].ClkVals.end())
    {
        return RC::BAD_PARAMETER;
    }

    ClkRange &range = m_CachedPStates[index].ClkVals[rmDomain];
    for (const auto& allPtsItr : allPoints)
    {
        if (range.MinKHz * freqRatio <= allPtsItr.ClkFreqKHz &&
            allPtsItr.ClkFreqKHz <= range.MaxKHz * freqRatio)
        {
            pPStatePoints->push_back(allPtsItr);
        }
    }
    return rc;
}

RC Perf30::GetClkDomailwfPoints
(
    Tee::Priority pri,
    Gpu::ClkDomain dom,
    UINT32 clkProgIdxFirst,
    UINT32 clkProgIdxLast,
    const LW2080_CTRL_CLK_CLK_PROGS_INFO &clkProgTable,
    const LW2080_CTRL_CLK_CLK_VF_POINTS_INFO &vfPointsInfo,
    VfPoints *pClkVfPoints
)
{
    MASSERT(pClkVfPoints);

    RC rc;

    LW2080_CTRL_CLK_CLK_VF_POINTS_STATUS vfPointsStatus = {};
    vfPointsStatus.super = vfPointsInfo.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_VF_POINTS_GET_STATUS
       ,&vfPointsStatus
       ,sizeof(vfPointsStatus)));

    for (UINT32 i = clkProgIdxFirst; i <= clkProgIdxLast; ++i)
    {
        const LW2080_CTRL_CLK_CLK_PROG_INFO &clkProg = clkProgTable.progs[i];

        Printf(pri,
            " CLK_PROG entry type = 0x%x, index = %d, source = 0x%x, VfPoints:\n",
            clkProg.super.type, i, clkProg.data.v1x.source);

        // Generate VfPoints based off of LW2080_CTRL_CLK_CLK_VF_POINT_INFO
        for (UINT32 j = 0; j < clkProgTable.vfEntryCount; ++j)
        {
            Printf(pri, "  vfEntry idx = %d\n", j);

            const LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY &masterVfEntry =
                clkProg.data.v30Master.master.vfEntries[j];

            if (masterVfEntry.vfPointIdxFirst == LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID)
            {
                Printf(pri, "   Invalid vf point idx, skipping\n");
                return OK;
            }

            for (UINT32 i = masterVfEntry.vfPointIdxFirst; i <= masterVfEntry.vfPointIdxLast; ++i)
            {
                const LW2080_CTRL_CLK_CLK_VF_POINT_INFO   &vfPointInfo   = vfPointsInfo.vfPoints[i];
                const LW2080_CTRL_CLK_CLK_VF_POINT_STATUS &vfPointStatus = vfPointsStatus.vfPoints[i];

                VfPoint point = {};
                point.VfIdx = i;
                point.VfeIdx = vfPointInfo.vfeEquIdx;
                switch (vfPointInfo.super.type)
                {
                    case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
                    case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
                    {
                        const UINT32 freqMHz =
                            (vfPointInfo.super.type == LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ) ?
                                vfPointInfo.data.v30Freq.freqMHz :
                                vfPointInfo.data.v35Freq.freqMHz;
                        point.ClkFreqKHz = freqMHz * ((dom == Gpu::ClkPexGen) ? 1 : 1000);

                        const UINT32 voltuV = vfPointStatus.voltageuV;

                        Printf(pri,
                            "   ClkFreqKHz = %d, VoltuV = %d, VFE idx = %d, VF idx = %d\n",
                            point.ClkFreqKHz, voltuV, point.VfeIdx, point.VfIdx);
                        break;
                    }
                    case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
                    case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
                    {
                        const UINT32 freqMHz = vfPointStatus.freqMHz;
                        point.ClkFreqKHz = freqMHz * ((dom == Gpu::ClkPexGen) ? 1 : 1000);

                        const UINT32 voltuV =
                            (vfPointInfo.super.type == LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT) ?
                                vfPointInfo.data.v30Volt.sourceVoltageuV :
                                vfPointInfo.data.v35Volt.sourceVoltageuV;

                        Printf(pri,
                            "   ClkFreqKHz = %d, SourceVoltageuV = %d, "
                            "VFE idx = %d, VF idx = %d\n",
                            point.ClkFreqKHz, voltuV, point.VfeIdx, point.VfIdx);
                        break;
                    }
                    default:
                        Printf(Tee::PriError, "   Unknown VF point type (0x%x)\n",
                            vfPointInfo.super.type);
                        return RC::SOFTWARE_ERROR;
                }
                pClkVfPoints->push_back(point);
            }
        }
    }
    return rc;
}

RC Perf30::GetFrequencieskHz
(
    Gpu::ClkDomain dom,
    vector<UINT32> *freqskHz
) const
{
    MASSERT(freqskHz);

    RC rc;
    map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator domIt = m_ClockDomains.find(dom);
    if (domIt == m_ClockDomains.end())
    {
        Printf(Tee::PriError,
            "%s: Invalid clock domain %s\n",
            __FUNCTION__, PerfUtil::ClkDomainToStr(dom));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    LW2080_CTRL_CLK_CLK_DOMAIN_FREQS_ENUM freqs = {};
    freqs.clkDomainIdx = domIt->second.objectIndex;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                m_pSubdevice
                ,LW2080_CTRL_CMD_CLK_CLK_DOMAIN_FREQS_ENUM
                ,&freqs
                ,sizeof(freqs)));

    // Check if it's a valid RM clock domain, handle the colwersion otherwise
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(dom))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(dom);
    }
    if (dom != Gpu::ClkPexGen)
    {
        freqRatio *= 1000.0f;
    }

    for (UINT32 ii = 0; ii < freqs.numFreqs; ii++)
    {
        freqskHz->push_back(Utility::RoundCast<UINT32>(freqs.freqsMHz[ii] * freqRatio));
    }

    MASSERT(!freqskHz->empty());

    return rc;
}

RC Perf30::GetVfPointPerfPoints
(
    vector<Perf::PerfPoint> * pPoints,
    const vector<Gpu::ClkDomain> & domainsArg
)
{
    MASSERT(pPoints);
    pPoints->clear();

    // Only allow a single domain
    if (domainsArg.size() > 1)
    {
        Printf(Tee::PriError, "Cannot handle multiple clock domains\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    Gpu::ClkDomain domain = domainsArg[0];
    bool isGpcClkDomain = false;
    if (domain == Gpu::ClkGpc2 || domain == Gpu::ClkGpc)
        isGpcClkDomain = true;

    for (const auto& ppsItr : m_OrgPStates)
    {
        const UINT32 pstateNum = ppsItr.PStateNum;

        // Required to generate min perfpoint for non gpc domains
        UINT64 minGpcFreqkHz = 0;
        if (!isGpcClkDomain)
        {
            ClkRange gpcClkRange;
            CHECK_RC(GetClockRange(pstateNum, Gpu::ClkGpc, &gpcClkRange));
            minGpcFreqkHz = gpcClkRange.MinKHz;
        }

        ClkRange clkRange;
        CHECK_RC(GetClockRange(pstateNum, domain, &clkRange));

        vector<UINT32> freqskHz;
        CHECK_RC(GetFrequencieskHz(domain, &freqskHz));

        vector<PerfPoint> points;
        for (const auto& freqItr : freqskHz)
        {
            if (freqItr < clkRange.MinKHz || freqItr > clkRange.MaxKHz)
            {
                continue;
            }

            if (isGpcClkDomain)
            {
                UINT32 gpcClkHz = PerfUtil::Is1xClockDomain(domain) ?
                    (freqItr * 1000ULL) : (freqItr * 1000ULL / 2ULL);
                PerfPoint pp(pstateNum, GpcPerf_EXPLICIT, gpcClkHz);

                // Label the gpc perf point
                pp.SpecialPt = LabelGpcPerfPoint(pp);
                points.push_back(pp);
            }
            else
            {
                // For all other domains use pstateNum.min
                PerfPoint pp(pstateNum, GpcPerf_MIN,
                             minGpcFreqkHz * 1000ULL);
                const ClkSetting clks(
                        domain,
                        freqItr * 1000ULL,
                        0,
                        false);
                pp.Clks[domain] = clks;
                points.push_back(pp);
            }
        }
        // Copy the new PerfPoints from this pstate to the user's vector
        pPoints->insert(pPoints->end(), points.begin(), points.end());
    }
    return OK;
}

RC Perf30::GetVfPointsStatusForDomain
(
    Gpu::ClkDomain dom,
    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> *pLwrrentVfPoints
)
{
    RC rc;

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(dom))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(dom);
        dom = PerfUtil::ColwertToOppositeClockDomain(dom);
    }

    map<Gpu::ClkDomain, vector<UINT32> >::const_iterator domIt = m_ClkDomRmVfIndices.find(dom);

    if (m_ClkDomRmVfIndices.end() == domIt)
    {
        Printf(Tee::PriError,
               "%s: Invalid domain %s\n",
               __FUNCTION__, PerfUtil::ClkDomainToStr(dom));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    const vector<UINT32> &vfIndices = domIt->second;

    LW2080_CTRL_CLK_CLK_VF_POINTS_STATUS vfPointsStatus = {};
    vfPointsStatus.super = m_CachedRmInfo.vfPoints.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_VF_POINTS_GET_STATUS
       ,&vfPointsStatus
       ,sizeof(vfPointsStatus)));

    for (const auto& vfIndItr : vfIndices)
    {
        (*pLwrrentVfPoints)[vfIndItr] = vfPointsStatus.vfPoints[vfIndItr];

        // Colwert back if required
        (*pLwrrentVfPoints)[vfIndItr].freqMHz =
            static_cast<LwU16>((*pLwrrentVfPoints)[vfIndItr].freqMHz * freqRatio);
    }
    return rc;
}

// These error code digits adhere to the format specified in
// bug 1712759.
RC Perf30::GetPStateErrorCodeDigits(UINT32 *pThreeDigits)
{
    MASSERT(pThreeDigits);
    RC rc;

    PerfPoint lwrrPP;
    CHECK_RC(GetLwrrentPerfPoint(&lwrrPP));

    // locationStr or IntersectSettings
    PerfPointType inflPtDigit = PerfPointType::Unknown;
    // which rail used for intersect
    IntersectRail intersectRailDigit = IntersectRail::Unknown;
    // clock domain used for intersect
    ClkDomainForIntersect intersectClkDomainDigit = ClkDomainForIntersect::Unknown;

    // If we're at an intersect point
    if (!lwrrPP.IntersectSettings.empty())
    {
        // At intersect but unknown clock domain
        intersectClkDomainDigit = ClkDomainForIntersect::NotApplicable;
        const IntersectSetting& is = *(lwrrPP.IntersectSettings.begin());

        if (lwrrPP.IntersectSettings.size() > 1)
        {
            inflPtDigit = PerfPointType::MultipleIntersect; // multiple intersect parameters
        }
        else
        {
            switch (is.Type)
            {
                case IntersectLogical:
                    inflPtDigit = PerfPointType::IntersectVolt;
                    break;
                case IntersectVoltFrequency:
                {
                    inflPtDigit = PerfPointType::IntersectVoltFreq;
                    switch (is.Frequency.ClockDomain)
                    {
                        case Gpu::ClkGpc2:
                        case Gpu::ClkGpc:
                            intersectClkDomainDigit = ClkDomainForIntersect::Gpc2Clk;
                            break;
                        case Gpu::ClkDisp:
                            intersectClkDomainDigit = ClkDomainForIntersect::DispClk;
                            break;
                        case Gpu::ClkM:
                            intersectClkDomainDigit = ClkDomainForIntersect::DramClk;
                            break;
                        default:
                            break; // unknown master clock domain
                    }
                    break;
                }
                case IntersectPState:
                    inflPtDigit = PerfPointType::IntersectPState;
                    break;
                case IntersectVPState:
                    inflPtDigit = PerfPointType::IntersectVPState;
                    break;
                default:
                    inflPtDigit = PerfPointType::MultipleIntersect; // unknown intersect type
                    break;
            }
        }
        switch (is.VoltageDomain)
        {
            case Gpu::VOLTAGE_LOGIC:
                intersectRailDigit = IntersectRail::Logic;
                break;
            case Gpu::VOLTAGE_SRAM:
                intersectRailDigit = IntersectRail::Sram;
                break;
            default:
                Printf(Tee::PriError, "Invalid voltage domain for current PerfPoint IntersectSettings\n");
                return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        switch (lwrrPP.SpecialPt)
        {
            case GpcPerf_EXPLICIT:
                inflPtDigit = PerfPointType::Explicit;
                break;
            case GpcPerf_MIN:
                inflPtDigit = PerfPointType::Min;
                break;
            case GpcPerf_MAX:
                inflPtDigit = PerfPointType::Max;
                break;
            case GpcPerf_TDP:
                inflPtDigit = PerfPointType::Tdp;
                break;
            case GpcPerf_TURBO:
                inflPtDigit = PerfPointType::Turbo;
                break;
            default:
                break;
        }
    }

    *pThreeDigits = (static_cast<UINT32>(inflPtDigit) * 100)
                    + (static_cast<UINT32>(intersectRailDigit) * 10)
                    + static_cast<UINT32>(intersectClkDomainDigit);
    return rc;
}

RC Perf30::GetVfPointsToJs
(
    Gpu::ClkDomain domain,
    JSContext *pCtx,
    JSObject *pJsObj
)
{
    RC rc;

    vector<UINT32> freqskHz;
    CHECK_RC(GetFrequencieskHz(domain, &freqskHz));

    JavaScriptPtr pJs;
    for (UINT32 ii = 0; ii < freqskHz.size(); ii++)
    {
        CHECK_RC(pJs->SetElement(pJsObj, ii, freqskHz[ii]));
    }

    return rc;
}

RC Perf30::PrintVfeIndepVars()
{
    RC rc;

    Printf(Tee::PriNormal, "**** VFE Table Independent Variables ****\n");
    LW2080_CTRL_PERF_VFE_VARS_CONTROL vfeVarsControl = {};

    vfeVarsControl.super = m_CachedRmInfo.vfeVars.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_VARS_GET_CONTROL
       ,&vfeVarsControl
       ,sizeof(vfeVarsControl)));

    UINT32 i;

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRmInfo.vfeVars.super.objMask.super)
        bool single = false; // is not a derived product or sum
        bool hasMinMax = false; // does this variable type have a floating point min/max

        Printf(Tee::PriNormal, "Entry %u\n", i);
        switch (m_CachedRmInfo.vfeVars.vars[i].super.type)
        {
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
                Printf(Tee::PriNormal, " Type: Frequency\n");
                single = true;
                hasMinMax = true;
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
                Printf(Tee::PriNormal, " Type: Voltage\n");
                single = true;
                hasMinMax = true;
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
                Printf(Tee::PriNormal, " Type: Sensed Temperature\n");
                Printf(Tee::PriNormal, " Thermal Channel Index: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.sensedTemp.thermChannelIndex);
                Printf(Tee::PriNormal, " Positive Hysterisis: %0.2f\n",
                       vfeVarsControl.vars[i].data.sensedTemp.tempHysteresisPositive / 256.0);
                Printf(Tee::PriNormal, " Negative Hysterisis: %0.2f\n",
                       vfeVarsControl.vars[i].data.sensedTemp.tempHysteresisNegative / 256.0);
                single = true;
                hasMinMax = true;
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
            {
                Printf(Tee::PriNormal, " Type: Sensed Fuse\n");
                if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
                {
                    break;
                }
                if (m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.bSigned)
                {
                    Printf(Tee::PriNormal, " Value: %d\n",
                           m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.data.signedValue);
                }
                else
                {
                    Printf(Tee::PriNormal, " Value: 0x%08x (%u)\n",
                           m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.data.unsignedValue,
                           m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.data.unsignedValue);
                }
                Printf(Tee::PriNormal, " Default Value: 0x%08x (%d)\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValDefault.data.unsignedValue,
                       m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValDefault.data.unsignedValue);
                const auto vfieldId =
                    m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.vFieldId;
                Printf(Tee::PriNormal, " VFIELD ID: %s (0x%02x)\n",
                       PerfUtil::VFieldIdToStr(vfieldId), vfieldId);
                Printf(Tee::PriNormal, " VFIELD ID Version: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.vFieldIdVer);
                Printf(Tee::PriNormal, " Expected VFIELD ID Version: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.verInfo.verExpected);
                single = true;
                hasMinMax = true;
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT:
                Printf(Tee::PriNormal, " Type: Derived Product\n");
                Printf(Tee::PriNormal, " Variable Index 0: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.derivedProd.varIdx0);
                Printf(Tee::PriNormal, " Variable Index 1: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.derivedProd.varIdx1);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM:
                Printf(Tee::PriNormal, " Type: Derived Sum\n");
                Printf(Tee::PriNormal, " Variable Index 0: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.derivedSum.varIdx0);
                Printf(Tee::PriNormal, " Variable Index 1: %u\n",
                       m_CachedRmInfo.vfeVars.vars[i].data.derivedSum.varIdx1);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED:
                Printf(Tee::PriNormal, " Type: Single Globally Specified\n");
                Printf(Tee::PriNormal, " Unique ID: %u\n",
                    m_CachedRmInfo.vfeVars.vars[i].data.globallySpecified.uniqueId);
                Printf(Tee::PriNormal, " Number of fractional bits in default values: %u\n",
                    m_CachedRmInfo.vfeVars.vars[i].data.globallySpecified.numFracBits);
                Printf(Tee::PriNormal, " Default Value: %u \n",
                    m_CachedRmInfo.vfeVars.vars[i].data.globallySpecified.valDefault);
                Printf(Tee::PriNormal,
                    " Client Specified Value of globally specified single variable: %u\n",
                    vfeVarsControl.vars[i].data.globallySpecified.valOverride);
                single = true;
                hasMinMax = true;
                break;
            default:
                Printf(Tee::PriError, "unknown VFE independent variable\n");
                return RC::LWRM_ILWALID_INDEX;
        }
        if (single)
        {
            if (vfeVarsControl.vars[i].data.single.overrideType !=
                LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_NONE)
            {
                Printf(Tee::PriNormal, " Override Type: ");
                switch (vfeVarsControl.vars[i].data.single.overrideType)
                {
                    case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_VALUE:
                        Printf(Tee::PriNormal, "VALUE\n");
                        break;
                    case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_OFFSET:
                        Printf(Tee::PriNormal, "OFFSET\n");
                        break;
                    case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_SCALE:
                        Printf(Tee::PriNormal, "SCALE\n");
                        break;
                    default:
                        Printf(Tee::PriNormal, "\n");
                        Printf(Tee::PriError, "invalid VFE variable override type");
                        return RC::LWRM_ILWALID_INDEX;
                }

                Printf(Tee::PriNormal, " Override Value: %0.2f (0x%08x)\n",
                       *reinterpret_cast<FLOAT32*>(
                           &vfeVarsControl.vars[i].data.single.overrideValue),
                       vfeVarsControl.vars[i].data.single.overrideValue);
            }
        }
        if (hasMinMax)
        {
            Printf(Tee::PriNormal, " Minimum: %4.3e (0x%08x)\n",
                   *reinterpret_cast<FLOAT32*>(&vfeVarsControl.vars[i].outRangeMin),
                   vfeVarsControl.vars[i].outRangeMin);
            Printf(Tee::PriNormal, " Maximum: %4.3e (0x%08x)\n",
                   *reinterpret_cast<FLOAT32*>(&vfeVarsControl.vars[i].outRangeMax),
                   vfeVarsControl.vars[i].outRangeMax);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    return rc;
}

RC Perf30::GetVfeIndepVarsToJs
(
    JSContext *pCtx,
    JSObject *pJsOutArray
)
{
    RC rc;
    LW2080_CTRL_PERF_VFE_VARS_CONTROL vfeVarsControl = {};

    vfeVarsControl.super = m_CachedRmInfo.vfeVars.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        , LW2080_CTRL_CMD_PERF_VFE_VARS_GET_CONTROL
        , &vfeVarsControl
        , sizeof(vfeVarsControl)));

    UINT32 i;
    JavaScriptPtr pJs;

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRmInfo.vfeVars.super.objMask.super)
    {
        bool single = false;
        bool hasMinMax = false;
        JSObject *pVfeInfo = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);
        switch (m_CachedRmInfo.vfeVars.vars[i].super.type)
        {
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_FREQ));
            single = true;
            hasMinMax = true;
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_VOLT));
            single = true;
            hasMinMax = true;
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_TEMP));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "ThermalChannelIndex",
                m_CachedRmInfo.vfeVars.vars[i].data.sensedTemp.thermChannelIndex));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "PositiveHysterisis",
                vfeVarsControl.vars[i].data.sensedTemp.tempHysteresisPositive / 256.0));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "NegativeHysterisis",
                vfeVarsControl.vars[i].data.sensedTemp.tempHysteresisNegative / 256.0));
            single = true;
            hasMinMax = true;
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
        {
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_FUSE));
            if (m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.bSigned)
            {
                CHECK_RC(pJs->SetProperty(pVfeInfo, "Value",
                    m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.data.signedValue));
            }
            else
            {
                CHECK_RC(pJs->SetProperty(pVfeInfo, "Value",
                    m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValue.data.unsignedValue));
            }
            CHECK_RC(pJs->SetProperty(pVfeInfo, "DefaultValue",
                m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.fuseValDefault.data.unsignedValue));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VFieldId",
                m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.vFieldId));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VFieldIdVer",
                m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.vFieldIdVer));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VFieldIdVerExp",
                m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.super.verInfo.verExpected));
            single = true;
            hasMinMax = true;
            break;
        }
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_PRODUCT));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VarIdx0",
                m_CachedRmInfo.vfeVars.vars[i].data.derivedProd.varIdx0));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VarIdx1",
                m_CachedRmInfo.vfeVars.vars[i].data.derivedProd.varIdx1));
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_SUM));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VarIdx0",
                m_CachedRmInfo.vfeVars.vars[i].data.derivedSum.varIdx0));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "VarIdx1",
                m_CachedRmInfo.vfeVars.vars[i].data.derivedSum.varIdx1));
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_GLOBAL));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "UniqueId",
                m_CachedRmInfo.vfeVars.vars[i].data.globallySpecified.uniqueId));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "NumFracBits",
                m_CachedRmInfo.vfeVars.vars[i].data.globallySpecified.numFracBits));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "ValDefault",
                m_CachedRmInfo.vfeVars.vars[i].data.globallySpecified.valDefault));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "ValOverride",
                *reinterpret_cast<FLOAT32*>(
                &vfeVarsControl.vars[i].data.globallySpecified.valOverride)));
            single = true;
            hasMinMax = true;
            break;
        default:
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Type", CLASS_ILWALID));
        }
        if (single)
        {
            if (vfeVarsControl.vars[i].data.single.overrideType !=
                LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_NONE)
            {
                switch (vfeVarsControl.vars[i].data.single.overrideType)
                {
                case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_VALUE:
                    CHECK_RC(pJs->SetProperty(pVfeInfo, "OverrideType", OVERRIDE_VALUE));
                    break;
                case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_OFFSET:
                    CHECK_RC(pJs->SetProperty(pVfeInfo, "OverrideType", OVERRIDE_OFFSET));
                    break;
                case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_SCALE:
                    CHECK_RC(pJs->SetProperty(pVfeInfo, "OverrideType", OVERRIDE_SCALE));
                    break;
                default:
                    CHECK_RC(pJs->SetProperty(pVfeInfo, "OverrideType", OVERRIDE_NONE));
                }

                CHECK_RC(pJs->SetProperty(pVfeInfo, "OverrideValue",
                    *reinterpret_cast<FLOAT32*>(
                        &vfeVarsControl.vars[i].data.single.overrideValue)));
            }
        }
        if (hasMinMax)
        {
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Min",
                *reinterpret_cast<FLOAT32*>(&vfeVarsControl.vars[i].outRangeMin)));
            CHECK_RC(pJs->SetProperty(pVfeInfo, "Max",
                *reinterpret_cast<FLOAT32*>(&vfeVarsControl.vars[i].outRangeMax)));
        }

        jsval VfeInfoJsval;
        CHECK_RC(pJs->ToJsval(pVfeInfo, &VfeInfoJsval));
        CHECK_RC(pJs->SetElement(pJsOutArray, i, VfeInfoJsval));
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Perf30::OverrideVfeVar
(
    VfeOverrideType overrideType
   ,VfeVarClass varClass
   ,INT32 index // index into the VFE vars table
   ,FLOAT32 val // can be an IEEE-754 float
)
{
    RC rc;

    LW2080_CTRL_PERF_VFE_VARS_CONTROL vfeVarsControl = {};

    vfeVarsControl.super = m_CachedRmInfo.vfeVars.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_VARS_GET_CONTROL
       ,&vfeVarsControl
       ,sizeof(vfeVarsControl)));

    // If index < 0, override all variables with class == varClass
    // It is possible there are no variables of type varClass in the VFE table.
    // Not considering that case to be an error. Instead just warn the user.
    if (index < 0)
    {
        bool hasOverriden = false;
        UINT32 i;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &vfeVarsControl.super.objMask.super)
            if (vfeVarsControl.vars[i].super.type == varClass)
            {
                vfeVarsControl.vars[i].data.single.overrideType = overrideType;
                vfeVarsControl.vars[i].data.single.overrideValue = *reinterpret_cast<LwU32*>(&val);
                hasOverriden = true;
                BoardObjGrpMaskBitSet(&vfeVarsControl.super.objMask.super, i);
                Printf(Tee::PriDebug, "Overrode VFE variable at index %u\n", i);
            }
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        if (!hasOverriden)
        {
            Printf(Tee::PriWarn, "no VFE variables overriden\n");
        }
        else
        {
            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                m_pSubdevice
               ,LW2080_CTRL_CMD_PERF_VFE_VARS_SET_CONTROL
               ,&vfeVarsControl
               ,sizeof(vfeVarsControl)));
        }
    }
    else
    {
        // It's an error if the user tries to override a non-existent variable
        if (!(BIT(index) & vfeVarsControl.super.objMask.super.pData[0]))
        {
            Printf(Tee::PriError,
                   "cannot override VFE variable at index %d which doesn't exist\n",
                   index);
            return RC::LWRM_ILWALID_INDEX;
        }

        // It's also an error if the types mismatch
        if (vfeVarsControl.vars[index].super.type != varClass)
        {
            Printf(Tee::PriError,
                   "cannot override VFE variable %d due to type mismatch\n",
                   index);
            return RC::BAD_PARAMETER;
        }

        vfeVarsControl.vars[index].data.single.overrideType = overrideType;
        vfeVarsControl.vars[index].data.single.overrideValue = *reinterpret_cast<LwU32*>(&val);

        BoardObjGrpMaskBitSet(&vfeVarsControl.super.objMask.super, index);
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdevice
           ,LW2080_CTRL_CMD_PERF_VFE_VARS_SET_CONTROL
           ,&vfeVarsControl
           ,sizeof(vfeVarsControl)));
        Printf(Tee::PriDebug, "Overrode VFE variable at index %d\n", index);
    }
    return rc;
}

RC Perf30::OverrideVfeVarRange
(
    VfeVarClass varClass
   ,INT32 index // index into the VFE vars table
   ,pair<FLOAT32, FLOAT32> range // can be a range of IEEE-754 float type
)
{
    RC rc;

    LW2080_CTRL_PERF_VFE_VARS_CONTROL vfeVarsControl = {};

    vfeVarsControl.super = m_CachedRmInfo.vfeVars.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_VARS_GET_CONTROL
       ,&vfeVarsControl
       ,sizeof(vfeVarsControl)));

    // If index < 0, override all variables with class == varClass
    if (index < 0)
    {
        bool hasOverriden = false;
        UINT32 i;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &vfeVarsControl.super.objMask.super)
            if (vfeVarsControl.vars[i].super.type == varClass)
            {
                vfeVarsControl.vars[i].outRangeMin = *reinterpret_cast<LwU32*>(&range.first);
                vfeVarsControl.vars[i].outRangeMax = *reinterpret_cast<LwU32*>(&range.second);
                hasOverriden = true;
                BoardObjGrpMaskBitSet(&vfeVarsControl.super.objMask.super, i);
                Printf(Tee::PriDebug, "Overrode VFE variable range at index %u\n", i);
            }
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        if (!hasOverriden)
        {
            Printf(Tee::PriWarn, "no VFE variables overriden\n");
        }
        else
        {
            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                m_pSubdevice
               ,LW2080_CTRL_CMD_PERF_VFE_VARS_SET_CONTROL
               ,&vfeVarsControl
               ,sizeof(vfeVarsControl)));
        }
    }
    else
    {
        // It's an error if the user tries to override a non-existent variable
        if (!(BIT(index) & vfeVarsControl.super.objMask.super.pData[0]))
        {
            Printf(Tee::PriError,
                   "cannot override VFE variable range at index %d which doesn't exist\n",
                   index);
            return RC::LWRM_ILWALID_INDEX;
        }

        // It's also an error if the types mismatch
        if (vfeVarsControl.vars[index].super.type != varClass)
        {
            Printf(Tee::PriError,
                   "cannot override VFE variable range %d due to type mismatch\n",
                   index);
            return RC::BAD_PARAMETER;
        }

        vfeVarsControl.vars[index].outRangeMin = *reinterpret_cast<LwU32*>(&range.first);
        vfeVarsControl.vars[index].outRangeMax = *reinterpret_cast<LwU32*>(&range.second);

        BoardObjGrpMaskBitSet(&vfeVarsControl.super.objMask.super, index);
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdevice
           ,LW2080_CTRL_CMD_PERF_VFE_VARS_SET_CONTROL
           ,&vfeVarsControl
           ,sizeof(vfeVarsControl)));
        Printf(Tee::PriDebug, "Overrode VFE variable range at index %d\n", index);
    }
    return rc;
}

RC Perf30::PrintVfeEqus()
{
    RC rc;

    Printf(Tee::PriNormal, "**** VFE Equations ****\n");
    LW2080_CTRL_PERF_VFE_EQUS_CONTROL vfeEquControl = {};

    vfeEquControl.super = m_CachedRmInfo.vfeEqus.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_EQUS_GET_CONTROL
       ,&vfeEquControl
       ,sizeof(vfeEquControl)));

    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(i, &m_CachedRmInfo.vfeEqus.super.objMask.super)
        bool hasIndepVar = false;
        bool hasMinMax = false;

        Printf(Tee::PriNormal, "Entry %u\n", i);
        switch (m_CachedRmInfo.vfeEqus.equs[i].super.type)
        {
            case LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE:
                Printf(Tee::PriNormal, " Type: COMPARE\n");
                Printf(Tee::PriNormal, " Comparator: ");
                switch (vfeEquControl.equs[i].data.compare.funcId)
                {
                    case LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL:
                        Printf(Tee::PriNormal, "==\n");
                        break;
                    case LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ:
                        Printf(Tee::PriNormal, ">=\n");
                        break;
                    case LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER:
                        Printf(Tee::PriNormal, ">\n");
                        break;
                    default:
                        Printf(Tee::PriError, "unknown VFE comparator\n");
                        return RC::LWRM_ILWALID_DATA;
                }
                Printf(Tee::PriNormal, " Criteria: %4.3e (0x%08x)\n",
                       *reinterpret_cast<FLOAT32*>(&vfeEquControl.equs[i].data.compare.criteria),
                       vfeEquControl.equs[i].data.compare.criteria);
                Printf(Tee::PriNormal, " Equ Idx True: %u\n",
                       m_CachedRmInfo.vfeEqus.equs[i].data.compare.equIdxTrue);
                Printf(Tee::PriNormal, " Equ Idx False: %u\n",
                       m_CachedRmInfo.vfeEqus.equs[i].data.compare.equIdxFalse);
                hasIndepVar = true;
                break;
            case LW2080_CTRL_PERF_VFE_EQU_TYPE_MINMAX:
                Printf(Tee::PriNormal, " Type: %s\n",
                       vfeEquControl.equs[i].data.minmax.bMax ? "MAX" : "MIN");
                Printf(Tee::PriNormal, " Equ Idx 0: %u\n",
                       m_CachedRmInfo.vfeEqus.equs[i].data.minmax.equIdx0);
                Printf(Tee::PriNormal, " Equ Idx 1: %u\n",
                       m_CachedRmInfo.vfeEqus.equs[i].data.minmax.equIdx1);
                break;
            case LW2080_CTRL_PERF_VFE_EQU_TYPE_QUADRATIC:
                Printf(Tee::PriNormal, " Type: QUADRATIC\n");
                Printf(Tee::PriNormal, " C2: %4.3e (0x%08x)\n",
                       *reinterpret_cast<FLOAT32*>(&vfeEquControl.equs[i].data.quadratic.coeffs[2]),
                       vfeEquControl.equs[i].data.quadratic.coeffs[2]);
                Printf(Tee::PriNormal, " C1: %4.3e (0x%08x)\n",
                       *reinterpret_cast<FLOAT32*>(&vfeEquControl.equs[i].data.quadratic.coeffs[1]),
                       vfeEquControl.equs[i].data.quadratic.coeffs[1]);
                Printf(Tee::PriNormal, " C0: %4.3e (0x%08x)\n",
                       *reinterpret_cast<FLOAT32*>(&vfeEquControl.equs[i].data.quadratic.coeffs[0]),
                       vfeEquControl.equs[i].data.quadratic.coeffs[0]);
                hasIndepVar = true;
                hasMinMax = true;
                break;
            default:
                Printf(Tee::PriError, "unknown VFE equation type\n");
                return RC::LWRM_ILWALID_INDEX;
        }

        if (hasIndepVar)
        {
            Printf(Tee::PriNormal, " Indep Var Idx: %u\n", m_CachedRmInfo.vfeEqus.equs[i].varIdx);
        }
        if (m_CachedRmInfo.vfeEqus.equs[i].equIdxNext != LW2080_CTRL_PERF_VFE_EQU_INDEX_ILWALID)
        {
            Printf(Tee::PriNormal, " Next Equ Idx: %u\n", m_CachedRmInfo.vfeEqus.equs[i].equIdxNext);
        }

        if (hasMinMax)
        {
            Printf(Tee::PriNormal, " Output Min: %4.3e (0x%08x)\n",
                   *reinterpret_cast<FLOAT32*>(&vfeEquControl.equs[i].outRangeMin),
                   vfeEquControl.equs[i].outRangeMin);
            Printf(Tee::PriNormal, " Output Max: %4.3e (0x%08x)\n",
                   *reinterpret_cast<FLOAT32*>(&vfeEquControl.equs[i].outRangeMax),
                   vfeEquControl.equs[i].outRangeMax);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Perf30::OverrideVfeEqu
(
    UINT32 idx,
    const LW2080_CTRL_PERF_VFE_EQU_CONTROL& vfeEqu
)
{
    MASSERT(idx < 256);

    RC rc;

    LW2080_CTRL_PERF_VFE_EQUS_CONTROL vfeEquControl = {};
    BoardObjGrpMaskBitSet(&vfeEquControl.super.objMask.super, idx);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_EQUS_GET_CONTROL
       ,&vfeEquControl
       ,sizeof(vfeEquControl)));

    if (vfeEquControl.equs[idx].super.type != vfeEqu.super.type)
    {
        Printf(Tee::PriError, "VFE equation type mismatch\n");
        return RC::BAD_PARAMETER;
    }

    vfeEquControl.equs[idx].data = vfeEqu.data;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_EQUS_SET_CONTROL
       ,&vfeEquControl
       ,sizeof(vfeEquControl)));

    return rc;
}

RC Perf30::OverrideVfeEquOutputRange
(
    UINT32 idx
   ,bool bMax
   ,FLOAT32 val
)
{
    MASSERT(idx < 256);

    RC rc;

    LW2080_CTRL_PERF_VFE_EQUS_CONTROL vfeEquControl = {};
    BoardObjGrpMaskBitSet(&vfeEquControl.super.objMask.super, idx);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_EQUS_GET_CONTROL
       ,&vfeEquControl
       ,sizeof(vfeEquControl)));

    if (bMax)
    {
        vfeEquControl.equs[idx].outRangeMax = *reinterpret_cast<UINT32*>(&val);
    }
    else
    {
        vfeEquControl.equs[idx].outRangeMin = *reinterpret_cast<UINT32*>(&val);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_EQUS_SET_CONTROL
       ,&vfeEquControl
       ,sizeof(vfeEquControl)));

    return rc;
}

void Perf30::DisableAllPerfLimits()
{
    MASSERT(m_PerfLimits.size() == NUMELEMS(PerfLimitsIDs));
    for (UINT32 limitIdx = 0; limitIdx < NUMELEMS(PerfLimitsIDs); limitIdx++)
    {
        m_PerfLimits[limitIdx].input.type =
            LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;
    }

    // All loose/intersect limits have been cleared
    m_LooseLimitIdx = 0;
    m_IntersectLimitIdx = 0;
}

RC Perf30::GetPerfLimitByID(UINT32 limitID, LW2080_CTRL_PERF_LIMIT_INPUT **limit)
{
    for (UINT32 limitIdx = 0; limitIdx < NUMELEMS(PerfLimitsIDs); limitIdx++)
    {
        if (limitID == PerfLimitsIDs[limitIdx])
        {
            *limit = &(m_PerfLimits[limitIdx].input);
            return OK;
        }
    }

    Printf(Tee::PriError, "Error: PState limit not found for id = %d\n", limitID);
    return RC::SOFTWARE_ERROR;
}

RC Perf30::LookupPerfStrictLimits
(
    Gpu::ClkDomain dom,
    LW2080_CTRL_PERF_LIMIT_INPUT **pMinLimit,
    LW2080_CTRL_PERF_LIMIT_INPUT **pMaxLimit
)
{
    RC rc;

    // Our map of domains to perf limit IDs uses 1x clocks!
    if (PerfUtil::Is2xClockDomain(dom))
        dom = PerfUtil::ColwertTo1xClockDomain(dom);

    for (UINT32 i = 0; i < NUMELEMS(strictFreqLockLimits); ++i)
    {
        if (strictFreqLockLimits[i].clkDomain == dom)
        {
            switch (m_ForcedPStateLockType)
            {
                case SoftLock:
                {
                    CHECK_RC(GetPerfLimitByID(clientLowStrictFreqLockLimits[i].limit.minLimitId, pMinLimit));
                    CHECK_RC(GetPerfLimitByID(clientLowStrictFreqLockLimits[i].limit.maxLimitId, pMaxLimit));
                    break;
                }
                case StrictLock:
                case DefaultLock:
                {
                    CHECK_RC(GetPerfLimitByID(strictFreqLockLimits[i].limit.minLimitId, pMinLimit));
                    CHECK_RC(GetPerfLimitByID(strictFreqLockLimits[i].limit.maxLimitId, pMaxLimit));
                    break;
                }
                case HardLock:
                {
                    CHECK_RC(GetPerfLimitByID(modsRulesStrictFreqLockLimits[i].limit.minLimitId, pMinLimit));
                    CHECK_RC(GetPerfLimitByID(modsRulesStrictFreqLockLimits[i].limit.maxLimitId, pMaxLimit));
                    break;
                }
                default:
                {
                    Printf(Tee::PriError, "Invalid PState lock type\n");
                    return RC::SOFTWARE_ERROR;
                }
            }
            return OK;
        }
    }

    Printf(Tee::PriError, "No strict perf limits for %s domain\n",
           PerfUtil::ClkDomainToStr(dom));

    return RC::ILWALID_CLOCK_DOMAIN;
}

namespace
{
    struct LimitPair
    {
        UINT32 minLimitId;
        UINT32 maxLimitId;
    };
    using LimitIdx = UINT32;
    using LimitTable = map<LimitIdx, LimitPair>;

    // Limits 0-2 are contiguous, and so are limits 3-9, but there is a gap in
    // limit IDs between limits 2 and 3. So we must maintain a mapping from
    // limit indices to limit IDs for each limit priority level.
    static const LimitTable clientLowLooseLimits =
    {
        { 0, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MAX } },
        { 1, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_1_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_1_MAX } },
        { 2, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_2_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_2_MAX } },
        { 3, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_3_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_3_MAX } },
        { 4, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_4_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_4_MAX } },
        { 5, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_5_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_5_MAX } },
        { 6, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_6_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_6_MAX } },
        { 7, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_7_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_7_MAX } },
        { 8, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_8_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_8_MAX } },
        { 9, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_9_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_9_MAX } }
    };

    static const LimitTable clientLooseLimits =
    {
        { 0, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_0_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_0_MAX } },
        { 1, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_1_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_1_MAX } },
        { 2, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_2_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_2_MAX } },
        { 3, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_3_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_3_MAX } },
        { 4, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_4_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_4_MAX } },
        { 5, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_5_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_5_MAX } },
        { 6, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_6_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_6_MAX } },
        { 7, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_7_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_7_MAX } },
        { 8, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_8_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_8_MAX } },
        { 9, { LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_9_MIN,
               LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_9_MAX } }
    };

    static const LimitTable modsRulesLooseLimits =
    {
        { 0, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_0_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_0_MAX } },
        { 1, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_1_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_1_MAX } },
        { 2, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_2_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_2_MAX } },
        { 3, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_3_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_3_MAX } },
        { 4, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_4_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_4_MAX } },
        { 5, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_5_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_5_MAX } },
        { 6, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_6_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_6_MAX } },
        { 7, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_7_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_7_MAX } },
        { 8, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_8_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_8_MAX } },
        { 9, { LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_9_MIN,
               LW2080_CTRL_PERF_LIMIT_MODS_RULES_LOOSE_9_MAX } }
    };
}

RC Perf30::LookupPerfLooseLimits
(
    LW2080_CTRL_PERF_LIMIT_INPUT **pMinLimit,
    LW2080_CTRL_PERF_LIMIT_INPUT **pMaxLimit
)
{
    if (m_LooseLimitIdx >= NUM_LOOSE_FREQ_LOCK_LIMITS)
    {
        Printf(Tee::PriError, "Exceeded loose limit capacity(%d)\n",
            NUM_LOOSE_FREQ_LOCK_LIMITS);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;

    UINT32 minLimitId;
    UINT32 maxLimitId;

    // Pick the appropriate loose limit LUT based on the PStateLockType
    const LimitTable* pLimitTable = nullptr;
    switch (m_ForcedPStateLockType)
    {
        case SoftLock:
        {
            pLimitTable = &clientLowLooseLimits;
            break;
        }
        case StrictLock:
        case DefaultLock:
        {
            pLimitTable = &clientLooseLimits;
            break;
        }
        case HardLock:
        {
            pLimitTable = &modsRulesLooseLimits;
            break;
        }
        default:
        {
            Printf(Tee::PriError, "Invalid PState lock type\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    MASSERT(pLimitTable);

    const auto& limitItr = pLimitTable->find(m_LooseLimitIdx);
    if (limitItr == pLimitTable->end())
    {
        return RC::SOFTWARE_ERROR;
    }
    minLimitId = limitItr->second.minLimitId;
    maxLimitId = limitItr->second.maxLimitId;

    CHECK_RC(GetPerfLimitByID(minLimitId, pMinLimit));
    CHECK_RC(GetPerfLimitByID(maxLimitId, pMaxLimit));

    m_LooseLimitIdx++;

    return rc;
}

RC Perf30::LookupPerfIntersectLimit(LW2080_CTRL_PERF_LIMIT_INPUT **pIntersectLimit)
{
    RC rc;

    switch (m_ForcedPStateLockType)
    {
        case SoftLock:
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_INTERSECT, pIntersectLimit));
            break;
        case StrictLock:
        case DefaultLock:
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_INTERSECT, pIntersectLimit));
            break;
        case HardLock:
            CHECK_RC(GetPerfLimitByID(LW2080_CTRL_PERF_LIMIT_MODS_RULES_INTERSECT, pIntersectLimit));
            break;
        default:
        {
            Printf(Tee::PriError, "Invalid PState lock type\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

RC Perf30::VoltFreqLookup
(
    Gpu::ClkDomain clkDom,
    Gpu::SplitVoltageDomain voltDom,
    VFLookupType lookupType,
    UINT32 vfInput,
    UINT32 *vfOutput
)
{
    MASSERT(vfOutput);
    MASSERT(lookupType == VoltToFreq || lookupType == FreqToVolt);

    RC rc;
    if (voltDom >= Gpu::SplitVoltageDomain_NUM)
    {
        Printf(Tee::PriError,
            "%s: Invalid voltage domain\n", __FUNCTION__);
        return RC::ILWALID_VOLTAGE_DOMAIN;
    }

    // Make sure it's a RM clock domain
    Gpu::ClkDomain rmClkDom = clkDom;
    FLOAT32 freqFactorModsToRm = 1.0f;
    FLOAT32 freqFactorRmToMods = 1.0f;
    if (!IsRmClockDomain(rmClkDom))
    {
        freqFactorModsToRm = PerfUtil::GetModsToRmFreqRatio(rmClkDom);
        freqFactorRmToMods = PerfUtil::GetRmToModsFreqRatio(rmClkDom);
        rmClkDom = PerfUtil::ColwertToOppositeClockDomain(rmClkDom);
    }

    UINT32 clkDomIdx = GetClkDomainClkObjIndex(rmClkDom);
    if (clkDomIdx == (std::numeric_limits<UINT32>::max)())
    {
        Printf(Tee::PriError,
            "%s: Invalid clock domain %s\n",
            __FUNCTION__, PerfUtil::ClkDomainToStr(clkDom));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    LW2080_CTRL_CLK_CLK_DOMAIN_RPC clkDomainRpc = { 0 };
    clkDomainRpc.classType = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_PROG;
    clkDomainRpc.clkDomainIdx = clkDomIdx;
    switch (lookupType)
    {
    case VoltToFreq:
        clkDomainRpc.infoData.v3xRpcInfo.type =
            LW2080_CTRL_CLK_DOMAIN_3X_PROG_RPC_ID_VOLT_TO_FREQ;
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.voltToFreqInfo.voltDomain =
            PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(voltDom);
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.voltToFreqInfo.voltageType =
            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR;
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.voltToFreqInfo.voltageuV =
            vfInput;
        break;
    case FreqToVolt:
        clkDomainRpc.infoData.v3xRpcInfo.type =
            LW2080_CTRL_CLK_DOMAIN_3X_PROG_RPC_ID_FREQ_TO_VOLT;
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqToVoltInfo.voltDomain =
            PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(voltDom);
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqToVoltInfo.voltageType =
            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR;
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqToVoltInfo.clkFreqMHz =
            static_cast<LwU16>(vfInput * freqFactorModsToRm);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        , LW2080_CTRL_CMD_CLK_CLK_DOMAIN_RPC
        , &clkDomainRpc
        , sizeof(clkDomainRpc)));

    switch (lookupType)
    {
    case VoltToFreq:
        *vfOutput =
            static_cast<UINT32>
            (clkDomainRpc.infoData.v3xRpcInfo.rpcData.voltToFreqInfo.clkFreqMHz * freqFactorRmToMods);
        break;
    case FreqToVolt:
        *vfOutput =
            clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqToVoltInfo.voltageuV;
        break;
    }
    return rc;
}

RC Perf30::EnableArbitration(Gpu::ClkDomain dom)
{
    return SetArbitrationStatus(dom, true);
}

RC Perf30::DisableArbitration(Gpu::ClkDomain dom)
{
    return SetArbitrationStatus(dom, false);
}

RC Perf30::SetArbitrationStatus(Gpu::ClkDomain dom, bool enable)
{
    RC rc;

    if (!m_ArbiterEnabled)
    {
        Printf(Tee::PriError, "RM perf arbiter disabled\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pSubdevice->HasDomain(dom) || ClkDomainType(dom) == FIXED)
    {
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    if (enable)
    {
        if (m_DisabledClkDomains.find(dom) == m_DisabledClkDomains.end())
        {
            Printf(Tee::PriError,
                   "Perf arbitration for %s is already enabled\n",
                   PerfUtil::ClkDomainToStr(dom));
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        if (m_DisabledClkDomains.find(dom) != m_DisabledClkDomains.end())
        {
            Printf(Tee::PriError,
                   "Perf arbitration for %s is already disabled\n",
                   PerfUtil::ClkDomainToStr(dom));
            return RC::SOFTWARE_ERROR;
        }
    }

    LW2080_CTRL_PERF_CHANGE_SEQ_CONTROL_PARAM perfCtrlParam = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_CHANGE_SEQ_GET_CONTROL
       ,&perfCtrlParam
       ,sizeof(perfCtrlParam)));

    // Find the clock domain boardobj index associated with the GpuClkDomain
    UINT32 clkIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx, &m_CachedRmInfo.clkDomains.super.objMask.super)
    {
        if (m_CachedRmInfo.clkDomains.domains[clkIdx].domain == PerfUtil::GpuClkDomainToCtrl2080Bit(dom))
        {
            break;
        }
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    if (enable)
    {
        PerfUtil::BoardObjGrpMaskBitClear(&perfCtrlParam.clkDomainsExclusionMask.objMask.super, clkIdx);
    }
    else
    {
        PerfUtil::BoardObjGrpMaskBitSet(&perfCtrlParam.clkDomainsExclusionMask.objMask.super, clkIdx);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_CHANGE_SEQ_SET_CONTROL
       ,&perfCtrlParam
       ,sizeof(perfCtrlParam)));

    if (enable)
    {
        auto domItr = m_DisabledClkDomains.find(dom);
        if (domItr != m_DisabledClkDomains.end())
        {
            m_DisabledClkDomains.erase(domItr);
        }
    }
    else
    {
        m_DisabledClkDomains.insert(dom);
    }

    return rc;
}

bool Perf30::IsArbitrated(Gpu::ClkDomain dom) const
{
    if (!m_ArbiterEnabled)
    {
        return false;
    }
    if (!m_pSubdevice->HasDomain(dom) || ClkDomainType(dom) == FIXED)
    {
        return false;
    }
    if (m_DisabledClkDomains.find(dom) != m_DisabledClkDomains.end())
    {
        return false;
    }
    return true;
}

RC Perf30::DisablePerfLimit(UINT32 limitId) const
{
    if (!m_ArbiterEnabled)
    {
        return OK;
    }

    RC rc;

    Printf(Tee::PriLow, "Disabling perf limit %d\n", limitId);

    LW2080_CTRL_PERF_LIMIT_SET_STATUS limit = {0};
    limit.limitId = limitId;
    limit.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;

    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS limitParams = {};
    limitParams.numLimits = 1;
    limitParams.pLimits = LW_PTR_TO_LwP64(&limit);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice, LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
        &limitParams, sizeof(limitParams)));

    return rc;
}

RC Perf30::GetVPStateIdx(UINT32 vpstateID, UINT32 *pIdx) const
{
    MASSERT(pIdx);
    *pIdx = LW2080_CTRL_PERF_VPSTATE_INDEX_ILWALID;
    if (vpstateID >= LW2080_CTRL_PERF_VPSTATES_IDX_NUM_INDEXES)
        return RC::BAD_PARAMETER;

    *pIdx = m_CachedRmInfo.vpstates.vpstateIdx[vpstateID];

    return OK;
}

void Perf30::UpdateVfSwitchStats(UINT32 vfSwitchType, UINT64 timeUs)
{
    MASSERT(vfSwitchType < VF_SWITCH_NUM_TYPES);

    UINT64 totalSwitchTimeUs =
        m_AvgVfSwitchTimeUs[vfSwitchType] * m_NumVfSwitches + timeUs;
    m_AvgVfSwitchTimeUs[vfSwitchType] = totalSwitchTimeUs / (m_NumVfSwitches + 1);

    if (timeUs < m_MilwfSwitchTimeUs[vfSwitchType] ||
        m_MilwfSwitchTimeUs[vfSwitchType] == 0)
    {
        m_MilwfSwitchTimeUs[vfSwitchType] = timeUs;
    }
    if (timeUs > m_MaxVfSwitchTimeUs[vfSwitchType])
    {
        m_MaxVfSwitchTimeUs[vfSwitchType] = timeUs;
    }
}

RC Perf30::SetClocksViaInjection(const PerfPoint& Setting, ClockInjectionType injType)
{
    InjectedPerfPoint injPP;
    injPP.clks.reserve(Setting.Clks.size());

    for (const auto& clkEntry : Setting.Clks)
    {
        if (injType == ClockInjectionType::ALL_CLOCKS)
        {
            injPP.clks.push_back(clkEntry.second);
        }
        else if (injType == ClockInjectionType::FIXED_CLOCKS)
        {
            if (ClkDomainType(clkEntry.second.Domain) == FIXED)
            {
                if (m_PStateVersion >= PStateVersion::PSTATE_35)
                {
                    injPP.clks.push_back(clkEntry.second);
                }
                else
                {
                    Printf(Tee::PriError, "Cannot program fixed clock domain %s\n",
                           PerfUtil::ClkDomainToStr(clkEntry.second.Domain));
                    return RC::ILWALID_CLOCK_DOMAIN;
                }
            }
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    if (injPP.clks.empty())
    {
        return OK;
    }

    return InjectPerfPoint(injPP);
}

RC Perf30::HandleRmChgSeq()
{
    const bool hasSwitchTimeLimit =
        (GetAllowedPerfSwitchTimeNs(VfSwitchVal::Pmu) != (std::numeric_limits<UINT32>::max)());

    if (!hasSwitchTimeLimit && m_VfSwitchTimePriority == Tee::PriNone)
    {
        return OK;
    }

    RC rc;

    LW2080_CTRL_PERF_CHANGE_SEQ_STATUS seqStatus = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_PERF_CHANGE_SEQ_GET_STATUS,
            &seqStatus,
            sizeof(seqStatus)));

    UINT64 timeTakenNs;
    switch (seqStatus.version)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_2X:
            LwU64_ALIGN32_UNPACK(&timeTakenNs,
                                 &seqStatus.data.v2x.scriptLast.profiling.totalTimens);
            break;
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_PMU:
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_31:
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35:
            LwU64_ALIGN32_UNPACK(&timeTakenNs,
                                 &seqStatus.data.pmu.scriptLast.hdr.profiling.totalTimens);
            break;
        default:
            MASSERT(!"Unknown change sequencer version");
            return RC::SOFTWARE_ERROR;
    }

    UpdateVfSwitchStats(VF_SWITCH_CHANGE_SEQ, timeTakenNs / 1000);

    if (hasSwitchTimeLimit && timeTakenNs > GetAllowedPerfSwitchTimeNs(VfSwitchVal::Pmu))
    {
        Printf(Tee::PriError,
            "RM perf change sequencer took %lluns (limit %uns)\n",
            timeTakenNs, GetAllowedPerfSwitchTimeNs(VfSwitchVal::Pmu));
        return RC::VF_SWITCH_TOO_SLOW;
    }

    return rc;
}

RC Perf30::SetArbiterState(LockState state)
{
    RC rc;

    LW2080_CTRL_PERF_PERF_LIMITS_CONTROL ctrlParam = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_GET_CONTROL
       ,&ctrlParam
       ,sizeof(ctrlParam)));

    ctrlParam.bArbitrateAndApplyLock = (state == LockState::LOCKED ? LW_TRUE : LW_FALSE);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_SET_CONTROL
       ,&ctrlParam
       ,sizeof(ctrlParam)));

    m_ArbiterEnabled = (state == LockState::UNLOCKED ? true : false);

    return rc;
}

RC Perf30::LockArbiter()
{
    return SetArbiterState(LockState::LOCKED);
}

RC Perf30::UnlockArbiter()
{
    return SetArbiterState(LockState::UNLOCKED);
}

RC Perf30::SetChangeSequencerState(LockState state)
{
    RC rc;

    LW2080_CTRL_PERF_CHANGE_SEQ_CONTROL seqCtrl = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_PERF_CHANGE_SEQ_GET_CONTROL,
            &seqCtrl,
            sizeof(seqCtrl)));

    seqCtrl.bLock = (state == LockState::LOCKED ? LW_TRUE : LW_FALSE);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_PERF_CHANGE_SEQ_SET_CONTROL,
            &seqCtrl,
            sizeof(seqCtrl)));

    return rc;
}

RC Perf30::LockChangeSequencer()
{
    return SetChangeSequencerState(LockState::LOCKED);
}

RC Perf30::UnlockChangeSequencer()
{
    return SetChangeSequencerState(LockState::UNLOCKED);
}

bool Perf30::IsChangeSequencerLocked() const
{
    LW2080_CTRL_PERF_CHANGE_SEQ_CONTROL seqCtrl = {};
    if (OK != LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                            LW2080_CTRL_CMD_PERF_CHANGE_SEQ_GET_CONTROL,
                                            &seqCtrl,
                                            sizeof(seqCtrl)))
    {
        return false;
    }

    return seqCtrl.bLock == LW_TRUE;
}

RC Perf30::GetPerfLimitsStatus(LW2080_CTRL_PERF_PERF_LIMITS_STATUS* pStatus) const
{
    MASSERT(pStatus);
    pStatus->super = m_CachedRmInfo.perfLimits.super;
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_GET_STATUS
       ,pStatus
       ,sizeof(*pStatus));
}

RC Perf30::GetActivePerfLimitsToJs(JSContext *pCtx, JSObject* pJsObj)
{
    MASSERT(pCtx);
    MASSERT(pJsObj);
    RC rc;

    LW2080_CTRL_PERF_PERF_LIMITS_STATUS limitsStatus = {};
    CHECK_RC(GetPerfLimitsStatus(&limitsStatus));

    JavaScriptPtr pJs;

    const auto& pstateLimIdx = limitsStatus.arbOutputApply.tuple.pstateIdx.limitIdx;
    CHECK_RC(pJs->SetProperty(pJsObj, "PState",
        string(m_CachedRmInfo.perfLimits.limits[pstateLimIdx].szName)));

    UINT32 clkIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(clkIdx, &limitsStatus.arbOutputApply.clkDomainsMask.super)
        const string clkDomStr = PerfUtil::ClkDomainToStr(GetDomainFromClockProgrammingOrder(clkIdx));
        const auto& clkLimIdx = limitsStatus.arbOutputApply.tuple.clkDomains[clkIdx].freqkHz.limitIdx;
        CHECK_RC(pJs->SetProperty(pJsObj, clkDomStr.c_str(),
            string(m_CachedRmInfo.perfLimits.limits[clkLimIdx].szName)));
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    // TODO: fix JIRA MODSAMPERE-140 so we can get perf limits for voltage rails

    return rc;
}

RC Perf30::SetFmonTestPoint(UINT32 gpcclk_MHz, UINT32 margin_MHz, UINT32 margin_uV)
{
    RC rc;

    LwU16 targetGpcclk_MHz = static_cast<LwU16>(gpcclk_MHz + margin_MHz);
    LwU16 quantizedTargetGpcclk_MHz;
    CHECK_RC(QuantizeFrequency(Gpu::ClkGpc, targetGpcclk_MHz, false, &quantizedTargetGpcclk_MHz));

    UINT32 volt_uV;
    CHECK_RC(VoltFreqLookup(Gpu::ClkGpc, Gpu::VOLTAGE_LOGIC, Perf::FreqToVolt,
        quantizedTargetGpcclk_MHz, &volt_uV));
    UINT32 finalVolt_uV = volt_uV + margin_uV;

    InjectedPerfPoint injPP;
    injPP.clks.emplace_back(Gpu::ClkGpc, quantizedTargetGpcclk_MHz * 1000000ULL);
    injPP.voltages.emplace_back(Gpu::VOLTAGE_LOGIC, finalVolt_uV / 1000U);

    UINT32 clkDomIdx = GetClkDomainClkObjIndex(Gpu::ClkGpc);
    const auto& progInfo = m_CachedRmInfo.clkDomains.domains[clkDomIdx].data.v3xProg;
    if (progInfo.clkProgIdxFirst != progInfo.clkProgIdxLast)
    {
        // We cannot handle when gpcclk uses mutiple clock programming entries
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 ii = 0; ii < LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES; ii++)
    {
        LwU8 clkDomIdx, ratio;
        ClkSetting clkSetting;
        const auto& progEntry = m_CachedRmInfo.clkProgs.progs[progInfo.clkProgIdxFirst];
        if (progEntry.type == LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO)
        {
            ratio = progEntry.data.v30MasterRatio.ratio.slaveEntries[ii].ratio;
            clkDomIdx = progEntry.data.v30MasterRatio.ratio.slaveEntries[ii].clkDomIdx;
        }
        else if (progEntry.type == LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO)
        {
            ratio = progEntry.data.v35MasterRatio.ratio.slaveEntries[ii].ratio;
            clkDomIdx = progEntry.data.v35MasterRatio.ratio.slaveEntries[ii].clkDomIdx;
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
        if (ratio == 0xFF || clkDomIdx == 0xFF)
            continue;

        bool bFoundClkDom = false;
        for (const auto& clk : m_ClockDomains)
        {
            if (clk.second.objectIndex == clkDomIdx && clk.second.IsRmClkDomain)
            {
                clkSetting.Domain = clk.first;
                LwU16 slaveFreq_MHz = static_cast<LwU16>(
                    (quantizedTargetGpcclk_MHz / 100.0f) * ratio);
                LwU16 quantizedSlaveFreq_MHz;
                CHECK_RC(QuantizeFrequency(
                    clkSetting.Domain, slaveFreq_MHz, true, &quantizedSlaveFreq_MHz));
                clkSetting.FreqHz = quantizedSlaveFreq_MHz * 1000000ULL;
                bFoundClkDom = true;
                break;
            }
        }
        if (!bFoundClkDom)
        {
            return RC::SOFTWARE_ERROR;
        }
        injPP.clks.push_back(clkSetting);
    }

    CHECK_RC(InjectPerfPoint(injPP));

    return rc;
}

RC Perf30::QuantizeFrequency
(
    Gpu::ClkDomain clkDom,
    LwU16 inputFreq_MHz,
    bool bFloor,
    LwU16* quantizedOutputFreq_MHz
)
{
    MASSERT(quantizedOutputFreq_MHz);
    *quantizedOutputFreq_MHz = 0;

    RC rc;

    UINT32 clkDomIdx = GetClkDomainClkObjIndex(clkDom);
    if (clkDomIdx == (std::numeric_limits<UINT32>::max)())
    {
        Printf(Tee::PriError,
            "%s: Invalid clock domain %s\n",
            __FUNCTION__, PerfUtil::ClkDomainToStr(clkDom));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    LW2080_CTRL_CLK_CLK_DOMAIN_RPC clkDomainRpc = { 0 };
    clkDomainRpc.classType = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_PROG;
    clkDomainRpc.clkDomainIdx = clkDomIdx;
    clkDomainRpc.infoData.v3xRpcInfo.type = LW2080_CTRL_CLK_DOMAIN_3X_PROG_RPC_ID_FREQ_QUANTIZE;
    clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqQuantizeInfo.clkFreqMHz = inputFreq_MHz;
    clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqQuantizeInfo.bReqFreqDeltaAdj = LW_FALSE;
    clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqQuantizeInfo.bFloor =
        bFloor ? LW_TRUE : LW_FALSE;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
        , LW2080_CTRL_CMD_CLK_CLK_DOMAIN_RPC
        , &clkDomainRpc
        , sizeof(clkDomainRpc)));

    *quantizedOutputFreq_MHz =
        clkDomainRpc.infoData.v3xRpcInfo.rpcData.freqQuantizeInfo.clkFreqMHz;

    return rc;
}

RC Perf30::UnlockClockDomains(const vector<Gpu::ClkDomain>& clkDoms)
{
    RC rc;

    LW2080_CTRL_PERF_PSTATES_CONTROL pstatesControl = {};
    pstatesControl.super = m_CachedRmInfo.pstates.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_GET_CONTROL
       ,&pstatesControl
       ,sizeof(pstatesControl)));

    vector<UINT32> pstates;
    CHECK_RC(GetAvailablePStates(&pstates));

    for (const auto& clkDom : clkDoms)
    {
        const UINT32 rmClockIndex = GetClkDomainClkObjIndex(clkDom);
        const FreqCtrlType clkType = ClkDomainType(clkDom);

        if (clkType == FIXED)
        {
            Printf(Tee::PriError,
                "%s is a fixed clock domain and cannot be unlocked\n",
                PerfUtil::ClkDomainToStr(clkDom));
            return RC::ILWALID_CLOCK_DOMAIN;
        }

        // Find the lowest/highest frequency possible across all pstates
        ClkRange widestRange = { 0 };
        widestRange.MinKHz = (std::numeric_limits<decltype(widestRange.MinKHz)>::max)();
        for (const auto& pstate : pstates)
        {
            UINT32 rmPStateIndex;
            CHECK_RC(GetRmPStateIndex(pstate, &rmPStateIndex));
            auto& clkEntry =
                pstatesControl.pstates[rmPStateIndex].data.v3x.clkEntries[rmClockIndex];
            if (clkEntry.min.baseFreqkHz < widestRange.MinKHz)
            {
                widestRange.MinKHz = clkEntry.min.baseFreqkHz;
            }
            if (clkEntry.max.baseFreqkHz > widestRange.MaxKHz)
            {
                widestRange.MaxKHz = clkEntry.max.baseFreqkHz;
            }
        }
        MASSERT(widestRange.MaxKHz >= widestRange.MinKHz);

        // Set each pstate's clock range to the maximum possible (e.g. [P8.min, P0.max])
        for (const auto& pstate : pstates)
        {
            UINT32 rmPStateIndex;
            CHECK_RC(GetRmPStateIndex(pstate, &rmPStateIndex));
            LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY_CONTROL& clkEntry =
                pstatesControl.pstates[rmPStateIndex].data.v3x.clkEntries[rmClockIndex];

            Printf(Tee::PriLow,
                "Widening P%u %s range from [%u, %u] to [%u, %u] kHz\n",
                pstate, PerfUtil::ClkDomainToStr(clkDom),
                clkEntry.min.baseFreqkHz, clkEntry.max.baseFreqkHz,
                widestRange.MinKHz, widestRange.MaxKHz);

            clkEntry.min.baseFreqkHz = widestRange.MinKHz;
            clkEntry.max.baseFreqkHz = widestRange.MaxKHz;
        }
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_SET_CONTROL
       ,&pstatesControl
       ,sizeof(pstatesControl)));

    m_CacheStatus = CACHE_DIRTY;

    return rc;
}

RC Perf30::GetClkMonFaults(const vector<Gpu::ClkDomain> &clkDomainsToMonitor,
                           ClkMonStatuses* pStatuses)
{   
    MASSERT(pStatuses);
    const size_t clkMons = clkDomainsToMonitor.size();
    if (clkMons == 0)
    {
        Printf(Tee::PriLow, "No Clock Monitors available\n");
        return OK;
    }
    if (clkMons > LW2080_CTRL_CLK_ARCH_MAX_DOMAINS)
    {
        return RC::BAD_PARAMETER;
    }

    RC rc;

    LW2080_CTRL_CLK_DOMAINS_CLK_MON_STATUS_PARAMS statusParam = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_DOMAINS_CLK_MON_GET_STATUS
       ,&statusParam
       ,sizeof(statusParam)));

    map<Gpu::ClkDomain, UINT32> rmFaultMasks;
    if (statusParam.bGlobalStatus)
    {
        for (UINT32 i = 0; i < statusParam.clkMonListSize; i++)
        {
            Gpu::ClkDomain clkDom = 
                PerfUtil::ClkCtrl2080BitToGpuClkDomain(statusParam.clkMonList[i].clkApiDomain);
            rmFaultMasks[clkDom] = statusParam.clkMonList[i].clkDomainFaultMask;
        }
    }
    /*
    Example 1: clkDomainsToMonitor = [GPC, SYS]
    // Say GPC has two partitions then pStatuses will
    pStatuses = [{GPC, 0, faultMask}, {GPC, 1, faultMask}, {SYS, 0, faultMask}]
    clkMonIdx = 0
    statusIdx = 0
    pstatuses for all the GPC partitions will be grouped together 
    according to the way we have pushed into the pStatuses vector in clkmon.cpp. We also ensure that because of the 
    MASSERT present inside per GPC loop
    numGpcs = 2
    i = 0 (Partition 0) {Say Partition 0 is FloorSwept}
    statusIdx 0 => GPC 0 => pStatuses[0].faultMask = 0
    statusIdx = 1
    i = 1 (Partition 1)
    statusIdx 1 => GPC 1 => pStatuses[1].faultMask = GetPerGpcClkMonFaults(1)
    statusIdx = 2
    clkMonIdx = 1
    pStatuses[2].faultMask = statusParam.clkMonList[1].clkDomainFaultMask
    *************************************************************************************************************
    Example 2: clkDomainsToMonitor = [GPC, DRAM, LWD]
    // Say GPC has two partitions then pStatuses will
    pStatuses = [{GPC, 0, faultMask}, {GPC, 1, faultMask}, {DRAM, 0, faultMask}, {DRAM, 1, faultMask}, {LWD, 0, faultMask}]
    clkMonIdx = 0, statusIdx = 0, pstatuses for all the GPC partitions will be grouped together 
    according to the way we have pushed into the pStatuses vector in clkmon.cpp. We ensure that because of the 
    MASSERT present inside per GPC loop
    numGpcs = 2
    i = 0 (Partition 0) {Say Partition 0 is FloorSwept}
    statusIdx 0 => GPC 0 => pStatuses[0].faultMask = 0
    statusIdx = 1
    i = 1 (Partition 1)
    statusIdx 1 => GPC 1 => pStatuses[1].faultMask = GetPerGpcClkMonFaults(1)
    statusIdx = 2
    clkMonIdx = 1
    numFbps = 2
    i = 0
    pStatuses[2].faultMask = GetPerfFbpClkMonFaults(0)
    statusIdx = 3
    i = 1
    pstatuses[3].faultMask = GetPerfFbpClkMonFaults(1)
    statusIdx = 4
    clkMonIdx = 2
    pstatuses[4].faultMask = statusParam.clkMonList[2].faultMask
    */
    // sizeOfStatusesVector should be the sum of the number of partitions for each clock monitor, 
    // if there are no partitions for that clock then just add the clock
    size_t sizeOfStatusesVector = clkDomainsToMonitor.size();
    const UINT32 numGpcs = m_pSubdevice->GetMaxGpcCount();
    const UINT32 numFbs = m_pSubdevice->GetMaxFbpCount();
    if (std::count(clkDomainsToMonitor.begin(), clkDomainsToMonitor.end(), Gpu::ClkGpc))
    {
        // Reduce by 1 because clkDomainsMonitor would contain ClkGpc
        sizeOfStatusesVector += (numGpcs - 1);
    }
    if (std::count(clkDomainsToMonitor.begin(), clkDomainsToMonitor.end(), Gpu::ClkM))
    {
        // Reduce by 1 because clkDomainsMonitor would contain ClkM
        sizeOfStatusesVector += (numFbs - 1);
    }
    if (pStatuses->size() != sizeOfStatusesVector)
    {
        Printf(Tee::PriError, "Clock Domain Statuses has incorrect values\n");
        return RC::SOFTWARE_ERROR;
    }
    size_t statusIdx = 0;
    size_t clkMonIdx = statusIdx;
    for (; clkMonIdx < clkMons; clkMonIdx++)
    {
        // bGlobalStatus can be used to check if there is any faults in any of the 
        // supported RM clock domains
        auto iter = m_ClockDomains.find(clkDomainsToMonitor[clkMonIdx]);
        bool isRmSupportedClkMonDomain = iter != m_ClockDomains.end() &&
                                         iter->second.SupportsClkMon;
        if (isRmSupportedClkMonDomain && !statusParam.bGlobalStatus)
        {
            // Need to increment statusIdx because the clock domains of Clock Domain Monitor Vector
            // and Clock Domain Status Vector should match
            if (clkDomainsToMonitor[clkMonIdx] == Gpu::ClkGpc)
            {
                statusIdx += numGpcs;
            }
            else if (clkDomainsToMonitor[clkMonIdx] == Gpu::ClkM)
            {
                statusIdx += numFbs;
            }
            else
            {
                statusIdx = statusIdx + 1;
            }
            continue;
        }
        MASSERT((*pStatuses)[statusIdx].clkDom == clkDomainsToMonitor[clkMonIdx]);
        if ((*pStatuses)[statusIdx].clkDom == Gpu::ClkGpc)
        {
            bool broadCastFault = false;
            CHECK_RC(m_pSubdevice->GetGPCClkFaultStatus(&broadCastFault));
            if (broadCastFault)
            {
                const UINT32 gpcs = m_pSubdevice->GetFsImpl()->GpcMask();
                // Loop through all per GPC values for GPC clock
                for (UINT32 i = 0; i < numGpcs; i++)
                {
                    auto& status = (*pStatuses)[statusIdx];
                    MASSERT(status.clkDom == Gpu::ClkGpc);
                    MASSERT(status.index == i);
                    // Check if GPC is valid
                    if (!(gpcs & (1 << i)))
                    {
                        status.faultMask = 0;
                        Printf(Tee::PriLow, "Skipping per GPC index = %d\n", i);
                    }
                    else
                    {
                        CHECK_RC(m_pSubdevice->GetPerGpcClkMonFaults(i,
                                &(status.faultMask)));
                    }
                    statusIdx++;
                }
            }
            else
            {
                statusIdx = statusIdx + numGpcs;
            }
        }
        else if ((*pStatuses)[statusIdx].clkDom == Gpu::ClkM)
        {
            bool broadCastFault = false;
            CHECK_RC(m_pSubdevice->GetDRAMClkFaultStatus(&broadCastFault));
            if (broadCastFault)
            {
                const UINT32 fbps = m_pSubdevice->GetFsImpl()->FbMask();
                for (UINT32 i = 0; i < numFbs; i++)
                {
                    auto& status = (*pStatuses)[statusIdx];
                    MASSERT(status.clkDom == Gpu::ClkM);
                    MASSERT(status.index == i);
                    // Check if FB is valid
                    if (!(fbps & (1 << i)))
                    {
                        status.faultMask = 0;
                        Printf(Tee::PriLow, "Skipping per FB index = %d\n", i);
                    }
                    else
                    {
                        CHECK_RC(m_pSubdevice->GetPerFbClkMonFaults(i,
                                &(status.faultMask)));
                    }
                    statusIdx++;
                }
            }
            else
            {
                statusIdx = statusIdx + numFbs;
            }
        }
        else if (isRmSupportedClkMonDomain)
        {
            auto& status = (*pStatuses)[statusIdx];
            status.faultMask = 0;
            const auto& rmFault = rmFaultMasks.find(status.clkDom);
            if (rmFault != rmFaultMasks.end())
            {
                status.faultMask = rmFaultMasks[status.clkDom];
            }
            statusIdx++;
        }
        else
        {
            auto& status = (*pStatuses)[statusIdx];
            CHECK_RC(m_pSubdevice->GetNonHalClkMonFaultMask(status.clkDom,
                     &(status.faultMask)));
            statusIdx++;
        }
    }

    return rc;
}

RC Perf30::ClearClkMonFaults(const vector<Gpu::ClkDomain>& clkDoms)
{
    if (clkDoms.empty())
    {
        return OK;
    }

    RC rc;

    CHECK_RC(GetClkDomainsControl());

    for (const auto& clkDom : clkDoms)
    {
        UINT32 clkIdx = GetClkDomainClkObjIndex(clkDom);
        auto iter = m_ClockDomains.find(clkDom);
        bool isRmSupportedClkMonDomain = iter != m_ClockDomains.end() && 
                                         iter->second.SupportsClkMon;
        if (clkIdx == (std::numeric_limits<UINT32>::max)() || 
            !isRmSupportedClkMonDomain)
        {
            //For non-HAL clocks, RM doesn't support clearFaults
            continue;
        }
        switch (m_ClkDomainsControl.domains[clkIdx].type)
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PROG:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
                break;
            default:
                Printf(Tee::PriError, "Cannot clear FMON faults for %s\n",
                    PerfUtil::ClkDomainToStr(clkDom));
                return RC::ILWALID_CLOCK_DOMAIN;
        }
        m_ClkDomainsControl.domains[clkIdx].data.v35Prog.clkMon.flags =
            FLD_SET_DRF(2080, _CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON_FLAGS, _FAULT, _CLEAR,
                m_ClkDomainsControl.domains[clkIdx].data.v35Prog.clkMon.flags);
    }

    rc = SetClkDomainsControl();
    if (rc != OK)
    {
        Printf(Tee::PriError, "Cannot clear FMON faults\n");
        if (rc == RC::LWRM_INSUFFICIENT_PERMISSIONS &&
            Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
        {
            Printf(Tee::PriWarn, "Get a \"FMON Unlock\" HULK license!\n");
        }
    }

    return rc;
}

RC Perf30::SetClkMonThresholds(const vector<ClkMonOverride>& overrides)
{
    RC rc;

    CHECK_RC(GetClkDomainsControl());

    for (const auto& ov : overrides)
    {
        const UINT32 clkIdx = GetClkDomainClkObjIndex(ov.clkDom);
        // A valid RM clock domain, need not support clock monitoring
        auto iter = m_ClockDomains.find(ov.clkDom);
        bool isRmSupportedClkMonDomain = iter != m_ClockDomains.end() && 
                                         iter->second.SupportsClkMon;
        if (clkIdx == std::numeric_limits<UINT32>::max() || 
            !isRmSupportedClkMonDomain)
        {
            // For clocks not supported by RM, we lwrrently do not set the thresholds
            continue;
        }

        switch (m_ClkDomainsControl.domains[clkIdx].type)
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PROG:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
                break;
            default:
                Printf(Tee::PriError, "Cannot override FMON threshold for %s\n",
                    PerfUtil::ClkDomainToStr(ov.clkDom));
                return RC::ILWALID_CLOCK_DOMAIN;
        }

        const UINT32 fxpVal = static_cast<UINT32>(Utility::ColwertFloatToFXP(ov.val, 20, 12));
        auto& clkMon = m_ClkDomainsControl.domains[clkIdx].data.v35Prog.clkMon;
        auto& rmVal = ov.thresh == ClkMonThreshold::LOW ?
            clkMon.lowThresholdOverride : clkMon.highThresholdOverride;
        rmVal = fxpVal;
    }

    CHECK_RC(SetClkDomainsControl());

    return rc;
}

RC Perf30::ClearClkMonThresholds(const vector<Gpu::ClkDomain>& clkDoms)
{
    RC rc;

    CHECK_RC(GetClkDomainsControl());

    for (const auto clkDom : clkDoms)
    {
        const UINT32 clkIdx = GetClkDomainClkObjIndex(clkDom);
        auto iter = m_ClockDomains.find(clkDom);
        bool isRmSupportedClkMonDomain = iter != m_ClockDomains.end() && 
                                         iter->second.SupportsClkMon;
        // A valid RM clock domain, need not support clock monitoring
        if (clkIdx == std::numeric_limits<UINT32>::max() ||
           !(isRmSupportedClkMonDomain))
        {
            // For clocks not supported by RM, we lwrrently do not set the thresholds
            continue;
        }
        m_ClkDomainsControl.domains[clkIdx].data.v35Prog.clkMon.lowThresholdOverride =
            LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON_THRESHOLD_OVERRIDE_IGNORE;
        m_ClkDomainsControl.domains[clkIdx].data.v35Prog.clkMon.highThresholdOverride =
            LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON_THRESHOLD_OVERRIDE_IGNORE;

    }

    CHECK_RC(SetClkDomainsControl());

    return rc;
}
RC Perf30::SetUseMinSramBin(const string &skuName)
{
    RC rc;

    if (skuName == "")
    {
        Printf(Tee::PriError, "SKU name is Invalid\n");
        return RC::ILWALID_ARGUMENT;
    }

    // Check if we are allowed to access POR
    if (!m_pSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_POR_BIN_CHECK))
    {
        Printf(Tee::PriError, "This GPU doesn't support POR Check!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        Printf(Tee::PriError, "POR Check not allowed\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Check for POR Bin
    string fuseFileName = m_pSubdevice->GetFuse()->GetFuseFilename();
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFileName));
    const FuseUtil::FuseDefMap* pFuseDefMap;
    const FuseUtil::SkuList* pSkuList;
    const FuseUtil::MiscInfo* pMiscInfo;
    const FuseDataSet* pFuseDataSet;
    CHECK_RC(pParser->ParseFile(fuseFileName,
        &pFuseDefMap, &pSkuList, &pMiscInfo, &pFuseDataSet));

    FuseUtil::SkuList::const_iterator skuIter = pSkuList->find(skuName);
    if (skuIter == pSkuList->end())
    {
        // This can only happen if the sku given is bad
        Printf(Tee::PriError, "Cannot find SKU %s\n", skuName.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // If there are multiple POR bins available, then use the one
    // with the highest minSram value. This will make MODS test at
    // the highest available Vmin for the SKU, which is what
    // SSG/SLT requested.
    INT32 minSram = std::numeric_limits<INT32>::min();
    const auto porBins = skuIter->second.porBinsMap.find(FuseUtil::PORBIN_GPU);
    if (porBins != skuIter->second.porBinsMap.end())
    {
        for (const auto& bin : porBins->second)
        {
            Printf(Tee::PriDebug, "Found MIN_SRAM_BIN = %d\n", minSram);
            minSram = max(minSram, bin.minSram);
        }
    }

    if (minSram == std::numeric_limits<INT32>::min())
    {
        Printf(Tee::PriError, "SKU %s does not have any POR bins\n", skuName.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    UINT32 i;

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_CachedRmInfo.vfeVars.super.objMask.super)

        if (m_CachedRmInfo.vfeVars.vars[i].data.sensedFuse.vFieldId ==
            LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SRAM_VMIN)
            {
                Printf(Tee::PriDebug, "Overriding Vmin at index %d\n", i);
                CHECK_RC(OverrideVfeVar(Perf::OVERRIDE_VALUE, Perf::CLASS_FUSE, i,
                    static_cast<FLOAT32>(minSram)));
            }

    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return RC::OK;
}

RC Perf30::ScaleSpeedo(const FLOAT32 factor)
{
    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &m_CachedRmInfo.vfeVars.super.objMask.super)
        if (m_CachedRmInfo.vfeVars.vars[ii].data.sensedFuse.vFieldId ==
            LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SPEEDO)
        {
            Printf(Tee::PriLow,
                "Overriding speedo at VFE variable index %u with scale factor %f\n",
                ii, factor);
            return OverrideVfeVar(Perf::OVERRIDE_SCALE, Perf::CLASS_FUSE, ii, factor);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    Printf(Tee::PriError,
        "Cannot override speedo because there is no such VFE variable (check VBIOS)\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf30::GetOverclockRange(const Gpu::ClkDomain clkDom, OverclockRange* pOcRange)
{
    MASSERT(pOcRange);
    RC rc;

    const auto& clkInfo = m_ClockDomains.find(clkDom);
    if (clkInfo == m_ClockDomains.end())
    {
        return RC::ILWALID_CLOCK_DOMAIN;
    }
    const UINT32 rmClkIdx = clkInfo->second.objectIndex;

    const auto& prog = m_CachedRmInfo.clientClkDoms.domains[rmClkIdx].data.prog;
    pOcRange->MinOffsetkHz = prog.freqOffsetMinkHz;
    pOcRange->MaxOffsetkHz = prog.freqOffsetMaxkHz;

    return rc;
}

RC Perf30::ForceClock
(
    Gpu::ClkDomain clkDomain,
    UINT64 targetkHz,
    UINT32 flags /* unused */
)
{
    // For Pascal/Volta, call legacy LW2080_CTRL_CMD_CLK_SET_INFO
    if (m_PStateVersion < PStateVersion::PSTATE_35)
    {
        return Perf::ForceClock(clkDomain, targetkHz, flags);
    }

    // LW2080_CTRL_CMD_CLK_SET_INFOS_PMU only works for hub/disp/dram
    if (clkDomain != Gpu::ClkHub &&
        clkDomain != Gpu::ClkDisp &&
        clkDomain != Gpu::ClkM)
    {
        InjectedPerfPoint injPP = {};
        ClkSetting clkSetting(clkDomain, targetkHz * 1000ULL);
        injPP.clks.push_back(clkSetting);
        return InjectPerfPoint(injPP);
    }

    RC rc;

    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(clkDomain))
    {
        freqRatio = PerfUtil::GetModsToRmFreqRatio(clkDomain);
        clkDomain = PerfUtil::ColwertToOppositeClockDomain(clkDomain);
    }

    LW2080_CTRL_CLK_SET_INFOS_PMU_PARAMS params = {};
    params.domainList.numDomains = 1;
    params.domainList.clkDomains[0].clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkDomain);
    params.domainList.clkDomains[0].clkFreqKHz = static_cast<LwU32>(targetkHz * freqRatio);
    params.domainList.clkDomains[0].regimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    params.domainList.clkDomains[0].source = LW2080_CTRL_CLK_PROG_1X_SOURCE_DEFAULT;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_SET_INFOS_PMU
       ,&params
       ,sizeof(params)));

    return rc;
}

RC Perf30::SetVfePollingPeriodMs(UINT32 periodMs)
{
    RC rc;

    LW2080_CTRL_PERF_VFE_VARS_CONTROL vfeVarsControl = {};
    vfeVarsControl.super = m_CachedRmInfo.vfeVars.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_VARS_GET_CONTROL
       ,&vfeVarsControl
       ,sizeof(vfeVarsControl)));

    vfeVarsControl.pollingPeriodms = static_cast<LwU8>(periodMs);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_VARS_SET_CONTROL
       ,&vfeVarsControl
       ,sizeof(vfeVarsControl)));

    return rc;
}

RC Perf30::GetVfePollingPeriodMs(UINT32* periodMs)
{
    RC rc;

    LW2080_CTRL_PERF_VFE_VARS_CONTROL vfeVarsControl = {};
    vfeVarsControl.super = m_CachedRmInfo.vfeVars.super;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_VFE_VARS_GET_CONTROL
       ,&vfeVarsControl
       ,sizeof(vfeVarsControl)));

    *periodMs = vfeVarsControl.pollingPeriodms;

    return rc;
}

RC Perf30::OverrideKappa(const FLOAT32 factor)
{
    RC rc;
    UINT32 indexValid;
    bool matchKappaValid = false;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(indexValid, &m_CachedRmInfo.vfeVars.super.objMask.super)
        if (m_CachedRmInfo.vfeVars.vars[indexValid].data.sensedFuse.vFieldId ==
            LW2080_CTRL_BIOS_VFIELD_ID_KAPPA_VALID)
        {
            matchKappaValid = true;
            Printf(Tee::PriLow,
                "Setting kappa valid bit to 1 using VFE variable %u\n", indexValid);
            CHECK_RC(OverrideVfeVar(Perf::OVERRIDE_VALUE, Perf::CLASS_FUSE, indexValid, 1));
            break;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
 
    UINT32 indexKappa;
    bool matchKappa = false;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(indexKappa, &m_CachedRmInfo.vfeVars.super.objMask.super)
        if (m_CachedRmInfo.vfeVars.vars[indexKappa].data.sensedFuse.vFieldId ==
            LW2080_CTRL_BIOS_VFIELD_ID_KAPPA)
        {
            matchKappa = true;
            Printf(Tee::PriLow,
                "Overriding kappa by %.1f using VFE variable %u\n",
                factor * 2, indexKappa);
            CHECK_RC(OverrideVfeVar(Perf::OVERRIDE_VALUE, Perf::CLASS_FUSE, indexKappa, factor * 2));
            break;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
 
    UINT32 indexBoardBin;
    bool matchBoardBin = false;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(indexBoardBin, &m_CachedRmInfo.vfeVars.super.objMask.super)
        if (m_CachedRmInfo.vfeVars.vars[indexBoardBin].data.sensedFuse.vFieldId ==
            LW2080_CTRL_BIOS_VFIELD_ID_STRAP_BOARD_BINNING)
        {
            matchBoardBin = true;
            Printf(Tee::PriLow,
                "Overriding Board Bin by %.1f using VFE variable %u\n",
                factor * 2, indexBoardBin);
            CHECK_RC(OverrideVfeVar(Perf::OVERRIDE_VALUE, Perf::CLASS_FUSE, indexBoardBin, factor * 2));
            break;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
 
    // On initial V0 fusing, use Board Bin variable to replace Kappa VFE variable
    // Either Kappa or Board Bin variable would exist in VBIOS, they can't both exist
    // See http://lwbugs/2952055 MODS - Request Command Line argument to override Kappa
    if ((matchBoardBin || matchKappa) && matchKappaValid)
    {
        return rc;
    }
    if (!matchKappa && !matchBoardBin)
    {
        Printf(Tee::PriError,
            "Cannot override kappa because there is no Kappa / Board Bin VFE variable (check VBIOS)\n");
    }
    if (!matchKappaValid)
    {
        Printf(Tee::PriError,
            "Cannot override kappa_valid because there is no such VFE variable (check VBIOS)\n");
    }
 
    return RC::UNSUPPORTED_FUNCTION;
}

