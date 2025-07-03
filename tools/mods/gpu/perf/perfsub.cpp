/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>  // for find

#include "core/include/display.h"
#include "core/include/errcount.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "jsstr.h"
#include "math.h"
#include "Lwcm.h"
#include "lwcmrsvd.h"

// Because JS_NewObject with a NULL class is unsafe in JS 1.4.
JS_CLASS(PerfDummy);

//-----------------------------------------------------------------------------
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

const CodeToStr SlowdownReasons_ToStr[] =
{
    { DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_THERMAL_SHUTDOWN),
      "thermal gpu slowdown" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_THERMAL_PROTECTION),
      "thermal POR limit" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_AVERAGE_POWER),
      "average power limit" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_PEAK_POWER),
      "peak power limit" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_INSUFFICIENT_POWER),
      "insufficient power" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_AC_BATT),
      "AC to battery event" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_API_TRIGGERED),
      "API request" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_POWER_BRAKE),
      "power brake event" }
    ,{DRF_SHIFTMASK(LW2080_CTRL_PERF_DECREASE_REASON_UNKNOWN),
      "unknown" }
};
} // anonymous namespace

const char * const Perf::DEFAULT_INFLECTION_PT = ENCJSENT("DEFAULT_INFLECTION_PT");

bool Perf::IntersectSetting::operator<(const IntersectSetting &is) const
{
    if (Type != is.Type)
        return (Type < is.Type);

    switch (Type)
    {
        case IntersectLogical:
            if (LogicalVoltageuV != is.LogicalVoltageuV)
                return (LogicalVoltageuV < is.LogicalVoltageuV);
            break;
        case IntersectVDT:
            if (VDTIndex != is.VDTIndex)
                return (VDTIndex < is.VDTIndex);
            break;
        case IntersectVFE:
            if (VFEEquIndex != is.VFEEquIndex)
                return (VFEEquIndex < is.VFEEquIndex);
            break;
        case IntersectPState:
            break;
        case IntersectVPState:
            if (VPStateIndex != is.VPStateIndex)
                return (VPStateIndex < is.VPStateIndex);
            break;
        case IntersectVoltFrequency:
        case IntersectFrequency:
            if (Frequency.ClockDomain != is.Frequency.ClockDomain)
                return (Frequency.ClockDomain < is.Frequency.ClockDomain);
            return (Frequency.ValueKHz < is.Frequency.ValueKHz);
        default:
            Printf(Tee::PriError, "Unimplemented intersect type = %d\n", Type);
            MASSERT(!"Unimplemented intersect type!");
            return false;
    }

    return (VoltageDomain < is.VoltageDomain);
}
bool Perf::IntersectSetting::operator==(const IntersectSetting &is) const
{
    if (Type != is.Type)
    {
        return false;
    }

    switch (Type)
    {
        case IntersectLogical:
            if (LogicalVoltageuV != is.LogicalVoltageuV)
                return false;
            break;
        case IntersectVDT:
            if (VDTIndex != is.VDTIndex)
                return false;
            break;
        case IntersectVFE:
            if (VFEEquIndex != is.VFEEquIndex)
                return false;
            break;
        case IntersectPState:
            break;
        case IntersectVPState:
            if (VPStateIndex != is.VPStateIndex)
                return false;
            break;
        case IntersectVoltFrequency:
        case IntersectFrequency:
            if (Frequency.ClockDomain != is.Frequency.ClockDomain)
                return false;
            return Frequency.ValueKHz == is.Frequency.ValueKHz;
        default:
            Printf(Tee::PriError, "Unimplemented intersect type = %d\n", Type);
            MASSERT(!"Unimplemented intersect type!");
            return false;
    }

    return (VoltageDomain == is.VoltageDomain);
}

bool Perf::ClkSetting::operator==(const ClkSetting& rhs) const
{
    if (this->Domain != rhs.Domain)
        return false;

    if (this->FreqHz != rhs.FreqHz)
        return false;

    if (this->Flags != rhs.Flags)
        return false;

    if (this->IsIntersect != rhs.IsIntersect)
        return false;

    if (this->Regime != rhs.Regime)
        return false;

    return true;
}

void Perf::ClkSetting::Print(Tee::Priority pri) const
{
    Printf(pri, "%s=%lluHz\n", PerfUtil::ClkDomainToStr(this->Domain), this->FreqHz);
}

void Perf::SplitVoltageSetting::Print(Tee::Priority pri) const
{
    Printf(pri, "%s=%umV\n", PerfUtil::SplitVoltDomainToStr(this->domain), this->voltMv);
}

void Perf::InjectedPerfPoint::Print(Tee::Priority pri) const
{
    for (const auto& clk : this->clks)
    {
        clk.Print(pri);
    }
    for (const auto& volt : this->voltages)
    {
        volt.Print(pri);
    }
}

Gpu::SplitVoltageDomain Perf::PerfPoint::IntersectVoltageDomain() const
{
    for (set<IntersectSetting>::const_iterator isIter = IntersectSettings.begin();
            isIter != IntersectSettings.end();
            isIter++)
    {
        switch (isIter->Type)
        {
            case IntersectLogical:
            case IntersectVDT:
            case IntersectVFE:
            case IntersectPState:
            case IntersectVPState:
            case IntersectVoltFrequency:
                return isIter->VoltageDomain;
            case IntersectFrequency:
                break;
        }
    }
    return Gpu::VOLTAGE_LOGIC;
}
bool Perf::PerfPoint::operator< (const PerfPoint & that) const
{
    if (this->PStateNum != that.PStateNum)
        return (this->PStateNum < that.PStateNum);

    if (this->SpecialPt != that.SpecialPt)
        return (this->SpecialPt < that.SpecialPt);

    if (this->SpecialPt == GpcPerf_INTERSECT)
    {
        return (this->IntersectSettings < that.IntersectSettings);
    }

    return false;
}
bool Perf::PerfPoint::operator== (const PerfPoint & rhs) const
{
    if (this->PStateNum != rhs.PStateNum)
        return false;

    if (this->SpecialPt != rhs.SpecialPt)
        return false;

    if (this->SpecialPt == GpcPerf_INTERSECT)
    {
        return (this->IntersectSettings == rhs.IntersectSettings);
    }

    if (this->Clks != rhs.Clks)
        return false;

    if (this->LinkWidth != rhs.LinkWidth)
        return false;

    if (this->LinkSpeed != rhs.LinkSpeed)
        return false;

    return true;
}

string Perf::PerfPoint::name(bool showRail) const
{
    const char *specialPtName = Perf::SpecialPtToString(SpecialPt);
    string specialPtStr = specialPtName;
    string str = Utility::StrPrintf("%u.%s",
                                    PStateNum,
                                    specialPtStr.c_str());
    if (showRail && (SpecialPt == GpcPerf_INTERSECT))
    {
        str += ".";
        str += PerfUtil::SplitVoltDomainToStr(IntersectVoltageDomain());
    }

    return str;
}

Perf::Perf(GpuSubdevice* pSubdevice, bool owned)
:m_pSubdevice(pSubdevice),
 m_InitDone(false),
 m_PStatesInfoRetrieved(false),
 m_DumpVfTable(false),
 m_CachedPStateNum(ILWALID_PSTATE),
 m_ClkDomainsMask(0),
 m_VoltageDomainsMask(0),
 m_VPStatesMask(0),
 m_VPStatesNum(0),
 m_ActivePStateLockType(NotLocked),
 m_CacheStatus(CACHE_DIRTY),
 m_PStateNumCache(CACHE_DIRTY),
 m_UsePStateCallbacks(false),
 m_ForcedPStateLockType(DefaultLock),
 m_LwrPerfPointPStateNum(ILWALID_PSTATE),
 m_LwrPerfPointGpcPerfMode(GpcPerf_EXPLICIT),
 m_ClockChangeCounter(pSubdevice),
 m_VfSwitchTimePriority(Tee::PriNone),
 m_ArbiterEnabled(false),
 m_UnlockChangeSequencer(false),
 m_pMutex(0),
 m_PStateChangeCounter(pSubdevice),
 m_Owned(owned),
 m_ForceUnlockBeforeLock(false),
 m_ClearPerfLevelsForUi(false),
 m_DispVminCapActive(false),
 m_RequiredDispClkHz(0),
 m_IsVminFloorRequestPending(false),
 m_VminFloorMode(VminFloorModeDisabled)
{
    memset(&m_VoltTuningOffsetsMv, 0, sizeof(m_VoltTuningOffsetsMv));
    memset(&m_PStateVoltTuningOffsetsMv, 0, sizeof(m_PStateVoltTuningOffsetsMv));
    memset(&m_ArbiterRequestParams, 0, sizeof(m_ArbiterRequestParams));
    m_pMutex = Tasker::AllocMutex("Perf::m_pMutex", Tasker::mtxUnchecked);

    m_ArbiterRequestParams.globalVoltageOffsetuV = UNKNOWN_VOLTAGE_OFFSET;

    MASSERT(pSubdevice);
    m_Callbacks.resize(NUM_PERF_CALLBACKS);
    //As long as RM does not offer any method to get the pstate Mods will assume
    // that pstate 12 supports deepidle
    m_DIPStates.insert(12);
    m_AllowedPerfSwitchTimeNs[VfSwitchVal::Mods] = (std::numeric_limits<UINT32>::max)();
    m_AllowedPerfSwitchTimeNs[VfSwitchVal::Pmu] = (std::numeric_limits<UINT32>::max)();
    m_AllowedPerfSwitchTimeNs[VfSwitchVal::RmLimits] = (std::numeric_limits<UINT32>::max)();
}

Perf::~Perf()
{
    m_PStateChangeCounter.ShutDown(false);
    m_ClockChangeCounter.ShutDown(false);

    for (UINT32 i = 0; i < m_Callbacks.size(); i++)
    {
        m_Callbacks[i].Disconnect();
    }
    Tasker::FreeMutex(m_pMutex);
}

RC Perf::Cleanup()
{
    RC rc;
    if (IsLocked())
    {
        CHECK_RC(ClearForcedPState());
    }
    return rc;
}

RC Perf::GetCoreVoltageMv(UINT32 *pVoltage)
{
    MASSERT(pVoltage);

    RC rc;
    LW2080_CTRL_PERF_GET_SAMPLED_VOLTAGE_INFO_PARAMS Params = {0};
    rc = LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                       LW2080_CTRL_CMD_PERF_GET_SAMPLED_VOLTAGE_INFO,
                                       &Params, sizeof(Params));
    *pVoltage = Params.voltage;

    return rc;
}

Perf::FreqCtrlType Perf::ClkDomainType(Gpu::ClkDomain d) const
{
    map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator it = m_ClockDomains.find(d);
    if (m_ClockDomains.end() == it)
        return UNKNOWN_TYPE;

    return it->second.Type;
}

UINT32 Perf::GetClkDomainClkObjIndex(Gpu::ClkDomain d) const
{
    map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator it = m_ClockDomains.find(d);
    if (m_ClockDomains.end() == it)
    {
        Printf(Tee::PriLow,
               "GetClkDomainClkObjIndex: Invalid clock domain %s (%d)\n",
               PerfUtil::ClkDomainToStr(d), (INT32)d);
        return (std::numeric_limits<UINT32>::max)();
    }
    return it->second.objectIndex;
}

// Search for both 1x and 2x mask if applicable
UINT32 Perf::GetClockPartitionMask(Gpu::ClkDomain d) const
{
    Gpu::ClkDomain d2x = PerfUtil::ColwertTo2xClockDomain(d);

    map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator it = m_ClockDomains.find(d);
    if (m_ClockDomains.end() == it)
    {
        if (d2x != Gpu::ClkUnkUnsp)
        {
            it = m_ClockDomains.find(d2x);
            if (m_ClockDomains.end() == it)
                return 0;
        }
        else
        {
            return 0;
        }
    }
    return it->second.partitionMask;
}

UINT32 Perf::GetPStateClkDomainMask()
{
    if (!m_PStatesInfoRetrieved)
    {
        GetPStatesInfo();
    }
    return m_ClkDomainsMask;
}
UINT32 Perf::GetPStateVoltageDomainMask()
{
    if (!m_PStatesInfoRetrieved)
    {
        GetPStatesInfo();
    }
    return m_VoltageDomainsMask;
}

bool Perf::HasPStates()
{
    if (OK != Perf::GetPStatesInfo())
        return false;

    return (0 != m_CachedPStates.size());
}

bool Perf::CanModifyThisPState(UINT32 PStateNum)
{
    if (HasPStates())
    {
        if (PStateNum != ILWALID_PSTATE)
        {
            bool retval;
            if (OK != DoesPStateExist(PStateNum, &retval))
                retval = false;
            return retval;
        }
        else
        {
            // ILWALID_PSTATE means "current pstate", which only makes
            // sense when we have locked the RM out of changing pstates.
            return IsLocked();
        }
    }
    else
    {
        // No pstates, we allow ILWALID_PSTATE only.
        return PStateNum == ILWALID_PSTATE;
    }
}

// can the volt domain be set for the pstate
bool Perf::CanSetVoltDomailwiaPState(Gpu::VoltageDomain Domain)
{
    if (!HasPStates())
    {
        return false;
    }
    UINT32 RmVoltDomain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Domain);
    return (RmVoltDomain & GetPStateVoltageDomainMask()) != 0;
}

bool Perf::CanSetClockDomailwiaPState(Gpu::ClkDomain Domain)
{
    if (!HasPStates() || m_pSubdevice->HasBug(642033))
    {
        return false;
    }
    // Make sure it's a valid RM clock domain
    if (!IsRmClockDomain(Domain))
        Domain = PerfUtil::ColwertToOppositeClockDomain(Domain);
    UINT32 RmClkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Domain);
    return (RmClkDomain & GetPStateClkDomainMask()) != 0;
}

//! This is the older method of setting CoreVoltage - it is somewhat ambiguous
//  because in chips > GT21x, there's ColdMode PStates, and most of the time
//  we have two separate Voltage domains in RM (one for each of Cold/hot mode)
RC Perf::SetCoreVoltageMv(UINT32 VoltMv, UINT32 PStateMask)
{
    // We want to make sure
    // that we can really program the voltages in the perf table (sanity check
    // since we can't return error with JS's  Subdev.Perf.CoreVoltageMv = XXX;
    if (CanSetVoltDomailwiaPState(Gpu::CoreV))
    {
        return SetVoltage(Gpu::CoreV, VoltMv, PStateMask);
    }

    return SetPStateDisabledVoltageMv(VoltMv);
}

RC Perf::SetPStateDisabledVoltageMv(UINT32 voltageMv)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Perf::GetLwrrentPState(UINT32 *pPStateNum)
{
    MASSERT(pPStateNum);

    if (CACHE_CLEAN == m_PStateNumCache)
    {
        *pPStateNum = m_CachedPStateNum;
        return OK;
    }

    RC rc;
    CHECK_RC(GetPStatesInfo());
    if (!HasPStates())
    {
        *pPStateNum = ILWALID_PSTATE;
        m_CachedPStateNum = *pPStateNum;
        m_PStateNumCache = CACHE_CLEAN;
        return OK;
    }

    CHECK_RC(GetLwrrentPStateNoCache(pPStateNum));

    if (HardLock == GetPStateLockType())
    {
        // No need to call into RM again if we hold a hard lock -- pstate can't
        // change unless we ask it to.
        m_CachedPStateNum = *pPStateNum;
        m_PStateNumCache = CACHE_CLEAN;
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetLwrrentPStateNoCache(UINT32 * pLwrPState)
{
    LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS params = {0};
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    const RC rc = pLwRm->Control(pRmHandle,
                                 LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
                                 &params,
                                 sizeof(params));
    const UINT32 rmPState = params.lwrrPstate;

    if (OK != rc)
    {
        *pLwrPState = ILWALID_PSTATE;
        return rc;
    }
    if (rmPState == LW2080_CTRL_PERF_PSTATES_UNDEFINED)
    {
        *pLwrPState = ILWALID_PSTATE;
        return OK;
    }

    MASSERT(!(rmPState & (rmPState - 1)));
    *pLwrPState = Utility::CountBits(rmPState - 1);

    if (*pLwrPState == 0xFFFFFFFF)
    {
        Printf(Tee::PriError, "Unknown current PState : 0x%08x\n", rmPState);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC Perf::GetPStateErrorCodeDigits(UINT32 *pIdx)
{
    MASSERT(pIdx);
    *pIdx = 0;
    if (m_PStateVersion < PStateVersion::PSTATE_20)
    {
        return OK;
    }

    RC rc;
    PerfPoint pp;
    CHECK_RC(GetLwrrentPerfPoint(&pp));
    UINT64 GpcHz;
    CHECK_RC(GetGpcFreqOfPerfPoint(pp, &GpcHz));

    // RM provides us gpc2clk for the VF tables
    const vector<VfPoint> &Tbl = GetVfEntryTbl(pp.PStateNum, Gpu::ClkGpc2).Points;

    vector<VfPoint> sortedTbl(Tbl);
    std::sort(sortedTbl.begin(), sortedTbl.end());

    // Search for the index into the sorted VF table that matches current GpcHz
    // The table is sorted in increasing order of gpcclk freq
    for (UINT32 ii = 0; ii < sortedTbl.size(); ii++)
    {
        if (sortedTbl[ii].ClkFreqKHz >= (2*GpcHz/1000))
        {
            // 0 is reserved for no VF point
            *pIdx = sortedTbl[ii].VfIdx + 1;
            break;
        }
    }

    // If we didn't find a match in the VF table, check to see if we are at PS.max
    // or a really high explicit gpcclk frequency and choose the last index
    // from the sorted VF table.
    //
    // This can happen in PS30 because RM returns a quantized list of frequencies
    // for gpcclk where Fmax < PS.max so GpcHz will be greater than all entries
    // in the sorted VF table at PS.max
    //
    if (*pIdx == 0)
    {
        PerfPoint pt;
        CHECK_RC(GetLwrrentPerfPoint(&pt));
        if (pt.SpecialPt == GpcPerf_MAX || pt.SpecialPt == GpcPerf_EXPLICIT)
        {
            *pIdx = sortedTbl[sortedTbl.size()-1].VfIdx+1;
        }
    }

    // We should always be at some VF point in the PState range
    if (*pIdx == 0 || *pIdx > 999)
    {
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::SetUseCallbacks(PerfCallback CallbackType, string FuncName)
{
    JavaScriptPtr pJS;
    JSFunction   *pJsFunc  = 0;
    JSObject     *pGlobObj = pJS->GetGlobalObject();
    if (OK != pJS->GetProperty(pGlobObj, FuncName.c_str(), &pJsFunc))
    {
       Printf(Tee::PriError, "%s not defined.\n", FuncName.c_str());
       return RC::UNDEFINED_JS_METHOD;
    }

    switch (CallbackType)
    {
        case PRE_PSTATE:
        case POST_PSTATE:
        case PRE_CLEANUP:
            m_UsePStateCallbacks = true;
            break;

        default:
            Printf(Tee::PriNormal, "unknown callback type!\n");
            return RC::BAD_PARAMETER;
    };

    // connect the callback functions
    m_Callbacks[CallbackType].Connect(pGlobObj, pJsFunc);
    return OK;
}

//-----------------------------------------------------------------------------
RC Perf::SetGpcPerfClk(UINT32 PStateNum, UINT64 FreqHz)
{
    const PerfPoint perfPoint(PStateNum, GpcPerf_EXPLICIT, FreqHz);
    return SetPerfPoint(perfPoint);
}

//-----------------------------------------------------------------------------
//! Prepare the RM data structures for setting gpcclk, but not actually
// call set table
// WARNING: This is unsupported in pstates 3.0
RC Perf::GetRatiodClocks
(
    UINT32 PStateNum,
    UINT64 FreqHz, //! This is a 1x gpcclk!!!
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo
)
{
    RC rc;
    if (0 == FreqHz)
    {
        // don't do anything
        return OK;
    }

    LW2080_CTRL_PERF_GET_RATIO_CLK_FREQ_PARAMS RatParams = {0};
    RatParams.pstate = 1<<PStateNum;
    RatParams.freq   = static_cast<LwU32>(FreqHz*2 / 1000); // RM works in KHz
    RatParams.domain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc2);
    RatParams.perfClkDomInfoListSize = static_cast<LwU32>(pClkInfo->size());
    RatParams.perfClkDomInfoList     = LW_PTR_TO_LwP64(&(*pClkInfo)[0]);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PERF_GET_RATIO_CLK_FREQ,
                                           &RatParams,
                                           sizeof(RatParams)));

    // find domains that depend on GPC
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO>::iterator cIt;
    for (cIt = pClkInfo->begin(); cIt != pClkInfo->end(); ++cIt)
    {
        const Gpu::ClkDomain modsDomain =
            PerfUtil::ClkCtrl2080BitToGpuClkDomain(cIt->domain);

        if (m_ClockDomains.find(modsDomain) == m_ClockDomains.end())
            continue;

        // modsDomain can only be ClkGpc2 because ClkCtrl2080BitToGpuClkDomain()
        // can only map an RM domain bit to a 2x Gpu::ClkDomain
        if (modsDomain == Gpu::ClkGpc2)
        {
            cIt->freq = static_cast<LwU32>(2*FreqHz / 1000);
        }

        // We need to clear the 2080_CTRL_PERF_CLK_DOM_INFO_FLAGS_UPDATED field
        // which is set by GET_RATIO_CLK_FREQ.
        cIt->flags = FLD_SET_DRF(2080_CTRL_PERF_CLK_DOM_INFO, _FLAGS, _UPDATED,
                                 _NO, cIt->flags);
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC Perf::SetInflectionPoint(UINT32 pstateNum, UINT32 gpcPerfMode)
{
    PerfPoint perfPoint(pstateNum, static_cast<GpcPerfMode>(gpcPerfMode));

    // For pstates with only one gpcperfhz, alias named LocationStr to "max".
    perfPoint.SpecialPt = EffectiveGpcPerfMode(perfPoint);

    const PerfPoint stdPerfPoint = GetMatchingStdPerfPoint(perfPoint);
    if (stdPerfPoint.SpecialPt == NUM_GpcPerfMode)
    {
        return RC::SOFTWARE_ERROR;
    }
    return SetPerfPoint(stdPerfPoint);
}

//-----------------------------------------------------------------------------
// Get current inflection point
RC Perf::GetInflectionPoint(UINT32* pPStateNum, UINT32* pLocation) const
{
    MASSERT(pPStateNum);
    MASSERT(pLocation);
    if (m_LwrPerfPointPStateNum == ILWALID_PSTATE)
    {
        return RC::SOFTWARE_ERROR;
    }
    *pPStateNum = m_LwrPerfPointPStateNum;
    *pLocation = m_LwrPerfPointGpcPerfMode;
    return OK;
}
//-----------------------------------------------------------------------------
// Get current inflection point
string Perf::GetInflectionPtStr() const
{
    if (m_pSubdevice->IsSOC() && m_TegraIndex != TegraIndexUnset)
    {
        return Utility::StrPrintf("%u.%s", m_TegraIndex, m_TegraName.c_str());
    }
    else if (m_LwrPerfPointPStateNum == ILWALID_PSTATE)
    {
        return "DEFAULT_INFLECTION_PT";
    }
    else
    {
        MASSERT(m_PStatesInfoRetrieved);
        return PerfPoint(m_LwrPerfPointPStateNum, m_LwrPerfPointGpcPerfMode,
            0, m_LwrIntersectSettings).name(m_PStateVersion >= PStateVersion::PSTATE_30);
    }
}
//-----------------------------------------------------------------------------
// Get a copy of the inflection points
RC Perf::GetPerfPoints(Perf::PerfPoints * pPoints)
{
    MASSERT(pPoints);
    RC rc;
    CHECK_RC(GetPStatesInfo());
    *pPoints = m_PerfPoints;
    return rc;
}
//-----------------------------------------------------------------------------
// Set information about CheetAh perf point
RC Perf::SetTegraPerfPointInfo(UINT32 index, const string& name)
{
    if (!m_pSubdevice->IsSOC())
    {
        Printf(Tee::PriError, "Invalid call to a CheetAh-specific function\n");
        MASSERT(!"SetTegraPerfPointInfo called on dGPU");
        return RC::SOFTWARE_ERROR;
    }

    m_TegraIndex = index;
    m_TegraName  = name;
    return OK;
}

//-----------------------------------------------------------------------------
Perf::GpcPerfMode Perf::LabelGpcPerfPoint
(
    const Perf::PerfPoint & pp
) const
{
    Perf::GpcPerfMode gpcPerfMode = GpcPerf_EXPLICIT;
    for (PerfPoints::const_iterator stdPtIt = m_PerfPoints.begin();
         stdPtIt != m_PerfPoints.end();
         ++stdPtIt)
    {
        UINT64 ppGpcPerfHz = 0;
        pp.GetFreqHz(Gpu::ClkGpc, &ppGpcPerfHz);

        UINT64 stdPtGpcPerfHz = 0;
        stdPtIt->GetFreqHz(Gpu::ClkGpc, &stdPtGpcPerfHz);
        if (stdPtIt->PStateNum == pp.PStateNum &&
            ppGpcPerfHz == stdPtGpcPerfHz && ppGpcPerfHz != 0)
        {
            const GpcPerfMode pref[] =
            {
                GpcPerf_INTERSECT, // Intersect is not a gpc lock, don't use.
                GpcPerf_EXPLICIT,  // least meaningful label at lowest index
                GpcPerf_MID,
                GpcPerf_NOM,
                GpcPerf_MIN,
                GpcPerf_MIDPOINT,
                GpcPerf_TURBO,
                GpcPerf_TDP,
                GpcPerf_MAX,       // most meaningful label at highest index
            };
            const GpcPerfMode * prefPp =
                find(pref, pref + NUMELEMS(pref), pp.SpecialPt);
            const GpcPerfMode * prefStd =
                find(pref, pref + NUMELEMS(pref), stdPtIt->SpecialPt);
            if (prefPp < prefStd)
                gpcPerfMode = stdPtIt->SpecialPt;
        }
    }
    return gpcPerfMode;
}
//-----------------------------------------------------------------------------
// Generate PerfPoints to hit all the interesting combinations of pstate and
// each "decoupled" clock domain.
//
RC Perf::GetVfPointPerfPoints
(
    vector<Perf::PerfPoint> * pPoints,
    const vector<Gpu::ClkDomain> & domainsArg
)
{
    MASSERT(pPoints);
    pPoints->clear();

    // Find the TDP (total device power) gpcclk frequency (max safe frequency).
    RC rc;
    UINT64 tdpHz;
    if (OK != GetTDPGpcHz(&tdpHz))
    {
        tdpHz = 0;
    }

    bool useEveryGpcclk = false;
    for (size_t ii = 0; ii < domainsArg.size(); ii++)
    {
        if (domainsArg[ii] == Gpu::ClkGpc2 || domainsArg[ii] == Gpu::ClkGpc)
            useEveryGpcclk = true;
    }

    // For each pstate:
    for (PStateInfos::const_iterator pps = m_OrgPStates.begin();
         pps != m_OrgPStates.end();
         ++pps)
    {
        const UINT32 pstateNum = pps->PStateNum;
        ClkRange clkRange;
        CHECK_RC(GetClockRange(pstateNum, Gpu::ClkGpc, &clkRange));
        vector<PerfPoint> points;

        // First make PerfPoints that specify pstate and gpcperf.
        if (useEveryGpcclk)
        {
            // Generate a PerfPoint for every gpc2clk in the vftable.
            // NOTE: the VfEntryTbl comes from RM and uses gpc2clk frequencies
            const VfEntryTbl & vfEntryTbl = GetVfEntryTbl(pstateNum, Gpu::ClkGpc2);
            for (VfPoints::const_iterator pvf = vfEntryTbl.Points.begin();
                 pvf != vfEntryTbl.Points.end();
                 ++pvf)
            {
                // If the frequency in the table is outside of the pstate
                // frequency range, skip it
                if (pvf->ClkFreqKHz / 2 < clkRange.MinKHz ||
                    pvf->ClkFreqKHz / 2 > clkRange.MaxKHz)
                {
                    continue;
                }

                PerfPoint pp(pstateNum, GpcPerf_EXPLICIT,
                             pvf->ClkFreqKHz*1000ULL/2ULL);
                if (tdpHz)
                    pp.SetUnsafeIfFasterThan(tdpHz);

                // Label the gpc perf point
                pp.SpecialPt = LabelGpcPerfPoint(pp);

                points.push_back(pp);
            }
        }
        if (points.empty())
        {
            // Make at least one per pstate.
            PerfPoint pp(pstateNum, GpcPerf_MIN, clkRange.MinKHz * 1000ULL);
            points.push_back(pp);
        }

        // For each decoupled domain other than gpcclk:
        for (vector<Gpu::ClkDomain>::const_iterator pd = domainsArg.begin();
             pd != domainsArg.end(); ++pd)
        {
            const Gpu::ClkDomain domain = *pd;
            if (domain == Gpu::ClkGpc2 || domain == Gpu::ClkGpc)
                continue;

            const VfEntryTbl & vfEntryTbl = GetVfEntryTbl(pstateNum, domain);
            CHECK_RC(GetClockRange(pstateNum, domain, &clkRange));

            // Multiply each existing PerfPoint for this domain.
            // Ie. if there are 4 Gpc-based Perfpoints and
            // 3 possible DispClk settings, produce 12 new PerfPoints.
            vector<PerfPoint> newPerfPoints;
            for (VfPoints::const_iterator pvf = vfEntryTbl.Points.begin();
                 pvf != vfEntryTbl.Points.end();
                 ++pvf)
            {
                bool using1xClockDomain = PerfUtil::Is1xClockDomain(domain);
                if (PerfUtil::Is2xClockDomain(domain))
                    PerfUtil::Warn2xClock(domain);

                if ((pvf->ClkFreqKHz / (using1xClockDomain ? 2 : 1) < clkRange.MinKHz) ||
                     (pvf->ClkFreqKHz / (using1xClockDomain ? 2 : 1) > clkRange.MaxKHz))
                    continue;

                const ClkSetting cs(
                    domain,
                    pvf->ClkFreqKHz*1000ULL / (using1xClockDomain ? 2ULL : 1ULL),
                    0,
                    false);

                for (vector<PerfPoint>::const_iterator ppp = points.begin();
                     ppp != points.end();
                     ++ppp)
                {
                    PerfPoint pp = *ppp;
                    pp.Clks[domain] = cs;
                    newPerfPoints.push_back(pp);
                }
            }
            if (!newPerfPoints.empty())
                points.swap(newPerfPoints);
        } // domains loop

        // Copy the new PerfPoints from this pstate to the user's vector.
        pPoints->insert(pPoints->end(), points.begin(), points.end());
    } // pstate loop

    return OK;
}

//-----------------------------------------------------------------------------
RC Perf::OverrideInflectionPt(UINT32 pstateNum,
                              UINT32 gpcPerfMode,
                              const PerfPoint & NewPerf)
{
    Printf(Tee::PriError, "Perf.OverrideInflectionPt is no longer allowed.\n");
    return RC::BAD_PARAMETER;
}
//-----------------------------------------------------------------------------
RC Perf::SetPStateTable
(
    UINT32 PStateNum,
    const vector<LW2080_CTRL_PERF_CLK_DOM_INFO> & Clks,
    const vector<LW2080_CTRL_PERF_VOLT_DOM_INFO>& Voltages
)
{
    RC rc;
    UINT32 NumPStates = 0;
    CHECK_RC(GetNumPStates(&NumPStates));
    if (NumPStates == 0)
    {
        Printf(Tee::PriNormal, "no pstate\n");
        return RC::SOFTWARE_ERROR;      // or just OK?
    }

    if (PStateNum == ILWALID_PSTATE)
    {
        CHECK_RC(GetLwrrentPState(&PStateNum));
    }

    CHECK_RC(SetPStateTableInternal(PStateNum, Clks, Voltages));
    return rc;
}
//-----------------------------------------------------------------------------
RC Perf::SetPStateTableInternal
(
    UINT32 PStateNum,
    const vector<LW2080_CTRL_PERF_CLK_DOM_INFO> & Clks,
    const vector<LW2080_CTRL_PERF_VOLT_DOM_INFO>& Voltages
)
{
    Tasker::MutexHolder mh(GetMutex());
    RC rc;

    if (PStateNum == ILWALID_PSTATE)
    {
        CHECK_RC(GetLwrrentPState(&PStateNum));
    }

    for (UINT32 i = 0; i < Clks.size(); i++)
    {
        Printf(Tee::PriLow,
               "SetPStateTableInternal: clk domain 0x%x  freq %d\n",
               Clks[i].domain, Clks[i].freq);

        const Gpu::ClkDomain modsDomain =
                    PerfUtil::ClkCtrl2080BitToGpuClkDomain(Clks[i].domain);
        if (modsDomain == Gpu::ClkGpc2 || modsDomain == Gpu::ClkGpc)
        {
            m_ClockChangeCounter.AddExpectedChange(Clks[i].freq);
            if (!IsClockInRange(PStateNum, Clks[i].domain, Clks[i].freq))
                return RC::INCORRECT_FREQUENCY;
        }
    }

    for (UINT32 i = 0; i < Voltages.size(); i++)
    {
        if (m_PStateVersion >= PStateVersion::PSTATE_20)
        {
            Printf(Tee::PriError, "Set voltage in PState 2.0 is unsupported\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        Printf(Tee::PriLow,
               "SetPStateTableInternal: volt domain %d  mvolt %d\n",
               Voltages[i].domain, Voltages[i].data.logical.logicalVoltageuV / 1000);
    }

    LW2080_CTRL_PERF_SET_PSTATE_INFO_PARAMS Params = {0};
    Params.pstate = 1<<PStateNum;
    Params.flags = DRF_DEF(2080,
                           _CTRL_SET_PSTATE_INFO_FLAG,_MODE,
                           _INTERNAL_TEST);
    Params.perfClkDomInfoListSize  = (LwU32)Clks.size();
    if (Params.perfClkDomInfoListSize > 0)
        Params.perfClkDomInfoList  = LW_PTR_TO_LwP64(&Clks[0]);
    Params.perfVoltDomInfoListSize = (LwU32)Voltages.size();
    if (Params.perfVoltDomInfoListSize > 0)
        Params.perfVoltDomInfoList = LW_PTR_TO_LwP64(&Voltages[0]);
    UINT64 timeBefore = Xp::GetWallTimeUS();
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PERF_SET_PSTATE_INFO,
                                           &Params,
                                           sizeof(Params)));
    Printf(m_VfSwitchTimePriority,
           "LW2080_CTRL_CMD_PERF_SET_PSTATE_INFO took %lluus\n",
           Xp::GetWallTimeUS() - timeBefore);
    m_CacheStatus = CACHE_DIRTY;
    return rc;
}

// Fetch the data needed for a call to SetPStateTableInternal.
void Perf::FetchDefaultPStateTable
(
    UINT32 pstateNum,
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo,
    vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> *pVoltInfo
)
{
    MASSERT(pClkInfo && pVoltInfo);
    MASSERT(m_PStatesInfoRetrieved);

    pClkInfo->clear();
    pVoltInfo->clear();

    // Find this pstate in the pstates info recorded when mods started.
    vector<PStateInfo>::const_iterator psiIt;
    for (psiIt = m_OrgPStates.begin(); psiIt != m_OrgPStates.end(); ++psiIt)
    {
        if (pstateNum == psiIt->PStateNum)
            break;
    }
    if (psiIt == m_OrgPStates.end())
    {
        MASSERT(!"invalid pstate");
        return;
    }

    // Copy out the table of default clock values for this pstate.
    map<UINT32, ClkRange>::const_iterator cIt;
    for (cIt = psiIt->ClkVals.begin(); cIt != psiIt->ClkVals.end(); ++cIt)
    {
        if (!cIt->second.LwrrKHz)
            continue;

        LW2080_CTRL_PERF_CLK_DOM_INFO ci = {0};
        ci.domain = cIt->first;
        ci.freq   = cIt->second.LwrrKHz;
        ci.flags  = psiIt->ClkFlags.find(ci.domain)->second;
        pClkInfo->push_back(ci);
    }

    // Setting absolute voltage via pstate table unsupported in pstates 2.0.
    if (m_PStateVersion < PStateVersion::PSTATE_20)
    {
        // Copy out the table of default voltage values for this pstate.
        map<UINT32, VoltInfo>::const_iterator vIt;
        for (vIt = psiIt->VoltageVals.begin(); vIt != psiIt->VoltageVals.end(); ++vIt)
        {
            if (!vIt->second.LogicalMv)
                continue;

            LW2080_CTRL_PERF_VOLT_DOM_INFO vi = {0};
            vi.domain = vIt->first;
            vi.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
            vi.data.logical.logicalVoltageuV = vIt->second.LogicalMv * 1000;
            pVoltInfo->push_back(vi);
        }
    }
}

//------------------------------------------------------------------------------
RC Perf::RestorePStateTable
(
    UINT32 PStateRestoreMask,
    Perf::WhichDomains whichDomains
)
{
    RC rc;
    Tasker::MutexHolder mh(GetMutex());

    // The PState 3.0 RM implementation of SET_PSTATE_INFO only accepts master
    // and ratio clock domains
    if (m_PStateVersion >= PStateVersion::PSTATE_30)
    {
        if (whichDomains == AllDomains)
        {
            whichDomains = OnlyDecoupled;
        }
    }

    for (UINT32 i = 0 ; i < m_OrgPStates.size(); i++)
    {
        // We may need to selectively restore pstate tables for some or all
        // pstates depending on the PStateRestoreMask
        if (PStateRestoreMask != ILWALID_PSTATE &&
            (!(PStateRestoreMask &
               PStateNumTo2080Bit(m_OrgPStates[i].PStateNum))))
        {
            continue;
        }

        vector<LW2080_CTRL_PERF_CLK_DOM_INFO> Clks;
        vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> Volts;
        FetchDefaultPStateTable(m_OrgPStates[i].PStateNum, &Clks, &Volts);

        if (whichDomains == OnlyDecoupled)
        {
            // We want to restore only clock domains that are decoupled
            // or are ratio'd to a decoupled clock domain.
            Volts.clear();

            vector<LW2080_CTRL_PERF_CLK_DOM_INFO> ClksFiltered;
            vector<LW2080_CTRL_PERF_CLK_DOM_INFO>::const_iterator cIt;
            for (cIt = Clks.begin(); cIt != Clks.end(); ++cIt)
            {
                const LW2080_CTRL_PERF_CLK_DOM_INFO & ci = *cIt;
                const FreqCtrlType t = ClkDomainType(
                    PerfUtil::ClkCtrl2080BitToGpuClkDomain(ci.domain));

                if (t == DECOUPLED || t == RATIO)
                {
                    ClksFiltered.push_back(ci);
                }
            }
            Clks = ClksFiltered;
        }

        LW2080_CTRL_PERF_SET_PSTATE_INFO_PARAMS Params = {0};
        Params.pstate = m_OrgPStates[i].PStateBit;
        Params.flags = DRF_DEF(2080,
                               _CTRL_SET_PSTATE_INFO_FLAG,_MODE,
                               _INTERNAL_TEST);
        if (Clks.size())
        {
            Params.perfClkDomInfoListSize  = (LwU32)Clks.size();
            Params.perfClkDomInfoList  = LW_PTR_TO_LwP64(&Clks[0]);
        }
        if (Volts.size())
        {
            Params.perfVoltDomInfoListSize = (LwU32)Volts.size();
            Params.perfVoltDomInfoList = LW_PTR_TO_LwP64(&Volts[0]);
        }
        CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                               LW2080_CTRL_CMD_PERF_SET_PSTATE_INFO,
                                               &Params,
                                               sizeof(Params)));
    }

    m_CacheStatus = CACHE_DIRTY;
    return rc;
}

RC Perf::MaybeUnlock(PStateLockType newLock)
{
    // This function is called *before* setting a new pstate lock.

    if (!m_ForceUnlockBeforeLock)
    {
        const PStateLockType lwrLock = GetPStateLockType();
        if ((lwrLock == HardLock || lwrLock == SoftLock || lwrLock == VirtualLock)
            && (newLock == lwrLock))
        {
            // Hard->Hard, Soft->Soft, and Virtual->Virtual lock changes can be done safely w/o
            // releasing the old lock, RM will switch w/o glitching.
            return OK;
        }
        if ((lwrLock == VirtualLock && (newLock == HardLock || newLock == SoftLock)) ||
            (newLock == VirtualLock && (lwrLock == HardLock || lwrLock == SoftLock)))
        {
            // Hard|Soft->Virtual and Virtual->Hard|Soft means we will switch
            // between the two different pstate "locks" in RM.  To avoid
            // glitching, release the lwrrently-held lock *after* setting the
            // new lock, bug http://lwbugs/1055295.
            return OK;
        }
    }

    // Remaining cases are:
    //   glitching experiment
    //   Hard->Soft            must release old lock to switch types
    //   Soft->Hard            must release old lock to switch types
    //   NotLocked->any        release all lock just in case, it's harmless
    return ClearForcedPState();
}

LwU32 Perf::ForceFlag(Perf::PStateLockType t)
{
    if (t == Perf::HardLock)
    {
        return DRF_DEF(2080_CTRL_PERF,
                       _SET_FORCE_PSTATE_EX,
                       _FLAGS_PRIORITY,
                       _MODS);
    }
    else if (t == Perf::SoftLock)
    {
        return DRF_DEF(2080_CTRL_PERF,
                       _SET_FORCE_PSTATE_EX,
                       _FLAGS_PRIORITY,
                       _NORM);
    }
    return 0;
}

RC Perf::GetForcedDefaultPerfPoint
(
    UINT32 pstateNum,
    UINT32 fallback,
    Perf::PerfPoint *pPerfPoint
)
{
    RC rc;
    UINT64 gpcFreqHz;
    CHECK_RC(GetClockAtPState(pstateNum, Gpu::ClkGpc, &gpcFreqHz));
    pPerfPoint->PStateNum = pstateNum;
    pPerfPoint->Clks[Gpu::ClkGpc] = ClkSetting(Gpu::ClkGpc, gpcFreqHz);
    pPerfPoint->SpecialPt = GpcPerf_EXPLICIT;
    pPerfPoint->FallBack = fallback;

    // Get the specified pstate's nominal frequencies for all decoupled clocks.
    for (map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator it = m_ClockDomains.begin();
         m_ClockDomains.end() != it;
         ++it)
    {
        if (m_pSubdevice->HasDomain(it->first) && DECOUPLED == it->second.Type)
        {
            UINT64 clkHz;
            CHECK_RC(GetClockAtPState(pstateNum, it->first, &clkHz));
            pPerfPoint->Clks[it->first] = ClkSetting(it->first, clkHz, 0, false);
        }
    }
    return rc;
}

RC Perf::SuggestPState(UINT32 TargetState, UINT32 FallBack)
{
    RC rc;
    PerfPoint perfPoint;
    CHECK_RC(GetForcedDefaultPerfPoint(TargetState, FallBack, &perfPoint));
    return SetPStateLock(perfPoint, SoftLock);
}

RC Perf::FastSuggestPState(UINT32 TargetState, UINT32 FallBack)
{
    RC rc;
    PerfPoint perfPoint;
    CHECK_RC(GetForcedDefaultPerfPoint(TargetState, FallBack, &perfPoint));
    perfPoint.FastPStateSwitch = true;
    return SetPStateLock(perfPoint, SoftLock);
}

RC Perf::ForcePState(UINT32 TargetState, UINT32 FallBack)
{
    RC rc;
    PerfPoint perfPoint;
    CHECK_RC(GetForcedDefaultPerfPoint(TargetState, FallBack, &perfPoint));
    return SetPStateLock(perfPoint, HardLock);
}

RC Perf::FastForcePState(UINT32 TargetState, UINT32 FallBack)
{
    RC rc;
    PerfPoint perfPoint;
    CHECK_RC(GetForcedDefaultPerfPoint(TargetState, FallBack, &perfPoint));
    perfPoint.FastPStateSwitch = true;
    return SetPStateLock(perfPoint, HardLock);
}

//-----------------------------------------------------------------------------
RC Perf::DumpVPStateInfo()
{
    if (!m_VPStatesNum)
        return OK;

    LW2080_CTRL_PERF_GET_VIRTUAL_PSTATE_INFO_PARAMS params = {0};
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> ClkDomInfos;
    RC rc;

    static const CodeToStr namedVPStates[] =
    {
         { LW2080_CTRL_PERF_VIRTUAL_PSTATES_D2, "D2" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_D3, "D3" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_D4, "D4" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_D5, "D5" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_OVER_LWRRENT, "OVER_LWRRENT" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_VRHOT, "VRHOT" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_MAX_BATT, "MAX_BATT" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_MAX_SLI, "MAX_SLI" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_MAX_THERM_SUSTAIN, "MAX_THERM_SUSTAIN" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_BOOST, "BOOST" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_TURBO_BOOST, "TURBO_BOOST" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_RATED_TDP, "RATED_TDP" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_SLOWDOWN_PWR, "SLOWDOWN_PWR" }
        ,{ LW2080_CTRL_PERF_VIRTUAL_PSTATES_MID_POINT, "MID_POINT" }
    };

    // Report mapping of "named" vpstate to vpstate number.
    for (size_t i = 0; i < NUMELEMS(namedVPStates); i++)
    {
        if (0 == (m_VPStatesMask & namedVPStates[i].Code))
            continue;

        if (OK != GetVPStateInfo(namedVPStates[i].Code, 0, &params, &ClkDomInfos))
        {
            Printf(Tee::PriWarn, "Vbios claims to have vpstate %s, but "
                   "RM can't retrieve info about it.\n",
                   namedVPStates[i].Str);
            continue;
        }
        Printf(Tee::PriLow, "Have vpstate %s at index %u\n",
               namedVPStates[i].Str, params.index);
    }

    // Report pstate and gpcperf of each virtual pstate.
    for (size_t vpIdx = 0; vpIdx < m_VPStatesNum; vpIdx++)
    {
        int idx = static_cast<int>(vpIdx);
        UINT64 gpcHz;
        UINT32 pstate;
        CHECK_RC(ClockOfVPState(idx, false, &gpcHz, &pstate));

        Printf(Tee::PriLow, "vpstate[%d] = pstate %d, gpcclk %llu kHz\n",
               idx,
               pstate,
               gpcHz/1000);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Perf::ClockOfVPState
(
    int idx,
    bool isSymbolicIdx,
    UINT64 * pRtnGpcHz,
    UINT32 * pRtnPState
)
{
    MASSERT(pRtnGpcHz && pRtnPState);
    RC rc;
    LwU32 vpidx;
    LW2080_CTRL_PERF_GET_VIRTUAL_PSTATE_INFO_PARAMS params = {0};
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> ClkDomInfos;

    if (isSymbolicIdx)
    {
        CHECK_RC(GetVPStateInfo(idx, 0, &params, &ClkDomInfos));
        vpidx = params.index;
    }
    else
    {
        vpidx = static_cast<LwU32>(idx);
    }

    CHECK_RC(GetVPStateInfo(
                 LW2080_CTRL_PERF_VIRTUAL_PSTATES_INDEX_ONLY,
                 vpidx,
                 &params, &ClkDomInfos));

    // Walk ClkDomInfos to find gpcclk
    const LwU32 GpcDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc2);
    for (LwU32 domIdx = 0; domIdx < params.perfClkDomInfoListSize; domIdx++)
    {
        if (ClkDomInfos[domIdx].domain == GpcDomain)
        {
            // RM provided us gpc2clk, so divide by 2
            *pRtnGpcHz = static_cast<UINT64>(ClkDomInfos[domIdx].freq) * 1000ULL / 2ULL;
            break;
        }
    }

    // See LW2080_CTRL_PERF_PSTATES.
    // RM returns 1 for pstate 0, 2 for pstate 1, 4 for pstate 2, etc.
    *pRtnPState = Utility::CountBits(params.pstate - 1);
    return rc;
}

RC Perf::GetTDPGpcHz(UINT64* tdpGpcHz)
{
    int idx = GetVPStateId(GpcPerf_TDP);
    UINT32 dummyPState;
    return ClockOfVPState(idx, true, tdpGpcHz, &dummyPState);
}

RC Perf::GetRmClkDomains(vector<Gpu::ClkDomain> *pDomains, ClkDomainProp domainProp)
{
    RC rc;
    MASSERT(pDomains);

    if (Platform::UsesLwgpuDriver())
    {
        pDomains->push_back(Gpu::ClkGpc);
        pDomains->push_back(Gpu::ClkGpc2);
        return OK;
    }

    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_CLK_DOMAINS_INFO clkDomainsInfo = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO,
                                           &clkDomainsInfo,
                                           sizeof(clkDomainsInfo)));

    UINT32 numDomains = 0;
    for (UINT32 i = Gpu::ClkUnkUnsp; i < Gpu::ClkDomain_NUM; i++)
    {
        Gpu::ClkDomain dom = static_cast<Gpu::ClkDomain>(i);
        Gpu::ClkDomain opDom = PerfUtil::ColwertToOppositeClockDomain(dom);
        UINT32 clkBit = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
        UINT32 opClkBit = PerfUtil::GpuClkDomainToCtrl2080Bit(opDom);

        switch (domainProp)
        {
            case ClkDomainProp::READABLE:
                if (clkDomainsInfo.vbiosDomains &
                    clkDomainsInfo.readableDomains &
                    (clkBit | opClkBit))
                {
                    pDomains->push_back(dom);
                    numDomains++;
                }
                break;
            case ClkDomainProp::WRITEABLE:
                if (clkDomainsInfo.programmableDomains &
                    (clkBit | opClkBit))
                {
                    pDomains->push_back(dom);
                    numDomains++;
                }
                break;
            default:
                 MASSERT(!"Unknown clock domain type\n");
                 break;
        }
    }

    pDomains->resize(numDomains);
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetClkDomainInfoToJs(JSContext *pCtx, JSObject *pJsOutArray)
{
    RC rc;
    CHECK_RC(GetPStatesInfo());

    JavaScriptPtr pJs;
    map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator clkDomIter = m_ClockDomains.begin();
    for (UINT32 i = 0; clkDomIter != m_ClockDomains.end(); ++clkDomIter, i++)
    {
        JSObject *pClkInfo = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);
        CHECK_RC(pJs->SetProperty(pClkInfo, "Domain",
                                  (UINT32)clkDomIter->second.Domain));
        CHECK_RC(pJs->SetProperty(pClkInfo, "RmDomain",
                 PerfUtil::GpuClkDomainToCtrl2080Bit(clkDomIter->second.Domain)));
        CHECK_RC(pJs->SetProperty(pClkInfo, "Type", clkDomIter->second.Type));
        CHECK_RC(pJs->SetProperty(pClkInfo, "RatioDomain",
                 clkDomIter->second.RatioDomain));
        CHECK_RC(pJs->SetProperty(pClkInfo, "RmRatioDomain",
                 PerfUtil::GpuClkDomainToCtrl2080Bit(clkDomIter->second.RatioDomain)));
        CHECK_RC(pJs->SetProperty(pClkInfo, "Ratio", clkDomIter->second.Ratio));
        CHECK_RC(pJs->SetProperty(pClkInfo, "PStateFloor",
                                  clkDomIter->second.PStateFloor));
        CHECK_RC(pJs->SetProperty(pClkInfo, "IsRmClkDomain",
                                  clkDomIter->second.IsRmClkDomain));

        jsval ClkInfoJsval;
        CHECK_RC(pJs->ToJsval(pClkInfo, &ClkInfoJsval));
        CHECK_RC(pJs->SetElement(pJsOutArray, i, ClkInfoJsval));
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC Perf::GetVoltDomainInfoToJs
(
    UINT32             PStateNum,
    Gpu::VoltageDomain Domain,
    JSContext         *pCtx,
    JSObject          *pRetObj
)
{
    RC rc;
    UINT32 RmDomain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Domain);
    UINT32 PIdx = 0;
    CHECK_RC(GetPStateIdx(PStateNum, &PIdx));
    PStateInfo &Info = m_CachedPStates[PIdx];
    if (Info.VoltageVals.find(RmDomain) == Info.VoltageVals.end())
    {
        Printf(Tee::PriNormal, "unkown voltage domain\n");
        return RC::BAD_PARAMETER;
    }

    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(pRetObj, "LogicalMv",
                              Info.VoltageVals[RmDomain].LogicalMv));
    CHECK_RC(pJs->SetProperty(pRetObj, "VdtIdx",
                              Info.VoltageVals[RmDomain].VdtIdx));
    CHECK_RC(pJs->SetProperty(pRetObj, "RmDomain", RmDomain));
    return rc;
}

//-----------------------------------------------------------------------------
//! Return the most-specific VfEntryTbl for this pstate and domain.
//! Return a wildcard table if it is the only match for this pstate.
//! If no tables match at all, return a ref to an empty table.
//
const Perf::VfEntryTbl & Perf::GetVfEntryTbl
(
    UINT32 pstateNum,
    Gpu::ClkDomain domain
) const
{
    const static VfEntryTbl noSuchTable;
    const VfEntryTbl * p = &noSuchTable;

    // All VfEntryTbl entries come from RM
    if (!IsRmClockDomain(domain))
        domain = PerfUtil::ColwertToOppositeClockDomain(domain);

    for (VfEntryTbls::const_iterator it = m_VfEntryTbls.begin();
         it != m_VfEntryTbls.end();
         ++it)
    {
        if (((it->Domain == domain) &&
             (it->PStateMask & (1<<pstateNum))) &&
            ((p == &noSuchTable) ||
             (Utility::CountBits(it->PStateMask) <
              Utility::CountBits(p->PStateMask))))
        {
            p = &(*it);
        }
    }
    return *p;
}

//-----------------------------------------------------------------------------
RC Perf::GetVfPointsToJs
(
    UINT32 PStateNum,
    Gpu::ClkDomain domain,
    JSContext *pCtx, JSObject *pJsObj
)
{
    RC rc;
    const VfEntryTbl & vfEntryTbl = GetVfEntryTbl(PStateNum, domain);
    if (vfEntryTbl.Points.empty())
    {
        Printf(Tee::PriNormal, "No VF entries for PState %d\n", PStateNum);
        return RC::BAD_PARAMETER;
    }

    JavaScriptPtr pJs;
    JSObject *pVfpArray = JS_NewArrayObject(pCtx, 0, NULL);
    for (UINT32 i = 0; i < vfEntryTbl.Points.size(); i++)
    {
        JSObject *pVfp = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);
        jsval VfpJsval;

        bool using1xClockDomain = PerfUtil::Is1xClockDomain(domain);
        if (PerfUtil::Is2xClockDomain(domain))
            PerfUtil::Warn2xClock(domain);

        CHECK_RC(VfPointToJsObj(vfEntryTbl.Points[i], pVfp, using1xClockDomain));
        CHECK_RC(pJs->ToJsval(pVfp, &VfpJsval));
        CHECK_RC(pJs->SetElement(pVfpArray, i, VfpJsval));
    }
    jsval VfpArrayJsval;
    CHECK_RC(pJs->ToJsval(pVfpArray, &VfpArrayJsval));
    CHECK_RC(pJs->SetPropertyJsval(pJsObj, "VfpArray", VfpArrayJsval));
    CHECK_RC(pJs->SetProperty(pJsObj, "Domain", vfEntryTbl.Domain));
    CHECK_RC(pJs->SetProperty(pJsObj, "PStateNum", PStateNum));
    return rc;
}

RC Perf::GetVfPointsToJs
(
    Gpu::ClkDomain domain,
    JSContext *pCtx,
    JSObject *pJsObj
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetVfSwitchTimePriority(UINT32* priority)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetVfSwitchTimePriority(UINT32 priority)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::VfPointToJsObj(const VfPoint& vfPoint, JSObject* pJsObj, bool is1xDom) const
{
    RC rc;
    JavaScriptPtr pJs;
    if (is1xDom)
    {
        CHECK_RC(pJs->SetProperty(pJsObj, "FreqKHz", vfPoint.ClkFreqKHz / 2));
        CHECK_RC(pJs->SetProperty(pJsObj, "StepSizeKHz",
                                  vfPoint.StepSizeKHz / 2));
    }
    else
    {
        CHECK_RC(pJs->SetProperty(pJsObj, "FreqKHz", vfPoint.ClkFreqKHz));
        CHECK_RC(pJs->SetProperty(pJsObj, "StepSizeKHz",
                                  vfPoint.StepSizeKHz));
    }

    string voltageStr;
    if (m_PStateVersion >= PStateVersion::PSTATE_30)
    {
        voltageStr = "VfeIdx";
    }
    else
    {
        voltageStr = "VdtIdx";
    }
    CHECK_RC(pJs->SetProperty(pJsObj, voltageStr.c_str(),
                              vfPoint.VdtIdx));
    CHECK_RC(pJs->SetProperty(pJsObj, "VfIdx",
                              vfPoint.VfIdx));
    CHECK_RC(pJs->SetProperty(pJsObj, "Flags",
                              vfPoint.Flags));
    CHECK_RC(pJs->SetProperty(pJsObj, "IsDerived",
                              vfPoint.IsDerived));

    return rc;
}

RC Perf::GetStaticFreqsToJs
(
    UINT32 PStateNum,
    Gpu::ClkDomain domain,
    JSObject *pJsObj
) const
{
    RC rc;

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(domain))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(domain);
    }

    const VfEntryTbl & vfEntryTbl = GetVfEntryTbl(PStateNum, domain);
    if (vfEntryTbl.Points.empty())
    {
        // There are some weird SKUs where pstate 8 gpcclk is 135MHz but
        // the RM VF lwrve for gpcclk starts from 405MHz.
        Printf(Tee::PriWarn, "No %s VF entries for PState %d\n",
            PerfUtil::ClkDomainToStr(domain), PStateNum);
        return rc;
    }

    set<UINT32> freqs;
    JavaScriptPtr pJs;
    UINT32 jsElementIdx = 0;
    for (UINT32 pointIdx = 0; pointIdx < vfEntryTbl.Points.size(); pointIdx++)
    {
        // Prevent duplicate frequencies. This can happen because
        // there can be more than one voltage to support a given
        // frequency
        if (freqs.find(vfEntryTbl.Points[pointIdx].ClkFreqKHz) == freqs.end())
        {
            freqs.insert(static_cast<UINT32>(vfEntryTbl.Points[pointIdx].ClkFreqKHz * freqRatio));
        }
        else
        {
            continue;
        }
        CHECK_RC(pJs->SetElement(pJsObj, jsElementIdx++,
            static_cast<UINT32>(vfEntryTbl.Points[pointIdx].ClkFreqKHz * freqRatio)));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetVfPointsAtPState
(
    UINT32 PStateNum,
    Gpu::ClkDomain domain,
    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> *pVfPts
)
{
    MASSERT(pVfPts);
    RC rc;

    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> vfPointStatuses;
    CHECK_RC(GetVfPointsStatusForDomain(domain, &vfPointStatuses));

    ClkRange range;
    CHECK_RC(GetClockRange(PStateNum, domain, &range));

    UINT32 multiplier = (domain == Gpu::ClkPexGen) ? 1 : 1000;
    for (map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS>::const_iterator
         pointsItr = vfPointStatuses.begin();
         pointsItr != vfPointStatuses.end(); ++pointsItr)
    {
        if (((static_cast<UINT32>(pointsItr->second.freqMHz) * multiplier) >= range.MinKHz) &&
            ((static_cast<UINT32>(pointsItr->second.freqMHz) * multiplier) <= range.MaxKHz))
        {
            (*pVfPts)[pointsItr->first] = pointsItr->second;
        }
    }
    return rc;
}

RC Perf::GetVfPointsStatusForDomain
(
    Gpu::ClkDomain dom,
    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> *pLwrrentVfPoints
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
// Brief: This is PState 2.0's method of setting Voltage
// Voltage is a function of temperature, clocks, speedo; as a result, it doesn't
// make much sense to set an absolute voltage -> revert back to PState 1.0
// behaviour.
// In order to add 'margin' in our testing, we can add a voltage offset to the
// current VF lwrve - this essentially shifts the entire lwrve up / down.
// VDT (voltage descriptor table) is a list of tables that RM uses to decide
// what voltage to program. It is implemented similar to a linked list (using
// nextEntry member as the pointer to the next entry). For each clock frequency,
// RM would traverse from a starting entry indicated by the VF (voltage frequency)
// table, and then each VDT entry (example, CVB, cold mode) would return a
// +/- voltage. RM would sum up the offsets returned by each VDT entry and then
// apply the voltage.
// The way we can add an arbitrary offset is to define an empty VDT entry, and
// then use the VDT mutator RM function to change the offset of VDT.
RC Perf::SetDefaultVoltTuningOffsetMv(INT32 OffsetMv)
{
    m_VoltTuningOffsetsMv[VTUNE_DEFAULT] = OffsetMv;
    return SetVoltTuningParams(writeIfChanged);
}
RC Perf::SetGlobalVoltTuningOffsetMv(INT32 OffsetMv)
{
    m_VoltTuningOffsetsMv[VTUNE_GLOBAL] = OffsetMv;
    return SetVoltTuningParams(writeIfChanged);
}
RC Perf::SetPStateVoltTuningOffsetMv(INT32 OffsetMv, UINT32 PState)
{
    UINT32 lwrPState;
    RC rc;
    CHECK_RC(GetLwrrentPState(&lwrPState));

    for (unsigned i=0; i<m_OrgPStates.size(); i++)
    {
        if (m_OrgPStates[i].PStateNum == PState)
        {
            m_PStateVoltTuningOffsetsMv[PState] = OffsetMv;

            if (lwrPState == PState)
            {
                m_VoltTuningOffsetsMv[VTUNE_PSTATE] =
                    m_PStateVoltTuningOffsetsMv[PState];
                rc = SetVoltTuningParams(writeIfChanged);
            }
            return rc;
        }
    }
    // Unsupported pstate.
    return RC::BAD_PARAMETER;
}
RC Perf::SetPerfPtVoltTuningOffsetMv(INT32 OffsetMv)
{
    // Called once during HandleGpuPostInitArgs, to apply per-inflection-point
    // voltage offset from command-line.
    // When we merge the command-line handling stuff with ApplyPerfHacks we
    // can remove this interface, it's redundant.
    m_VoltTuningOffsetsMv[VTUNE_PERFPOINT] = OffsetMv;
    return SetVoltTuningParams(writeIfChanged);
}

// Suggest an absolute Vmin floor for RM arbiter to factor in globally
RC Perf::SuggestVminFloor
(
    VminFloorMode mode,
    UINT32 modeArg,
    INT32 floorOffsetmV
)
{
    RC rc;
    UINT32 vdtIdx = LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_INDEX_ILWALID;
    UINT32 CoreVoltageDomain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::CoreV);
    UINT32 PStateIdx;

    CHECK_RC(GetArbiterRequestParams());
    switch(mode)
    {
        case PStateVDTIdxMode:
            CHECK_RC(GetPStateIdx(modeArg, &PStateIdx));
            vdtIdx = m_CachedPStates[PStateIdx].VoltageVals[CoreVoltageDomain].VdtIdx;
            // Falls through as handling is the same as VDT Entry mode
        case VDTEntryIdxMode:
            if (vdtIdx == LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_INDEX_ILWALID)
                vdtIdx = modeArg;

            m_ArbiterRequestParams.voltReq.type =
                                  LW2080_CTRL_CMD_PMGR_VOLTAGE_REQUEST_TYPE_VDT;
            m_ArbiterRequestParams.voltReq.data.vdt.entryIndex = vdtIdx;
            break;
        case LogicalVoltageMode:
            m_ArbiterRequestParams.voltReq.type =
                              LW2080_CTRL_CMD_PMGR_VOLTAGE_REQUEST_TYPE_LOGICAL;
            m_ArbiterRequestParams.voltReq.data.logical.voltageuV = modeArg * 1000;
            break;
        default:
            Printf(Tee::PriError, "Invalid Vmin floor mode %d\n", mode);
            return RC::BAD_PARAMETER;
    }

    m_ArbiterRequestParams.clientVoltageOffsetsuV
         [LW2080_CTRL_PMGR_VOLTAGE_REQUEST_CLIENT_DEBUG] = floorOffsetmV * 1000;
    m_IsVminFloorRequestPending = true;

    CHECK_RC(SetVoltTuningParams(writeUnconditionally));
    m_VminFloorMode = mode;

    return rc;
}

//------------------------------------------------------------------------------
RC Perf::GetArbiterRequestParams()
{
    RC rc;
    if (m_ArbiterRequestParams.globalVoltageOffsetuV == UNKNOWN_VOLTAGE_OFFSET)
    {
        rc = LwRmPtr()->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_PMGR_VOLTAGE_REQUEST_ARBITER_CONTROL_GET,
            &m_ArbiterRequestParams,
            sizeof(m_ArbiterRequestParams));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Perf::SetVoltTuningParams(VoltTuningWriteMode m)
{
    RC rc;
    INT32 offsetMv = 0;
    for(unsigned i=0; i<NUMELEMS(m_VoltTuningOffsetsMv); i++)
    {
        offsetMv += m_VoltTuningOffsetsMv[i];
    }
    const INT32 offsetuV = offsetMv * 1000;

    CHECK_RC(GetArbiterRequestParams());

    if (writeIfChanged == m &&
        m_ArbiterRequestParams.globalVoltageOffsetuV == offsetuV &&
        !m_IsVminFloorRequestPending)
    {
        // No change.
        return OK;
    }

    m_ArbiterRequestParams.globalVoltageOffsetuV = offsetuV;

    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->ControlBySubdevice(
                 m_pSubdevice,
                 LW2080_CTRL_CMD_PMGR_VOLTAGE_REQUEST_ARBITER_CONTROL_SET,
                 &m_ArbiterRequestParams,
                 sizeof(m_ArbiterRequestParams)));

    m_IsVminFloorRequestPending = false;
    return rc;
}

//-----------------------------------------------------------------------------
INT32 Perf::GetVoltTuningOffsetMv()
{
    GetArbiterRequestParams();
    return m_ArbiterRequestParams.globalVoltageOffsetuV / 1000;
}

//-----------------------------------------------------------------------------
RC Perf::SetClientOffsetMv(UINT32 client, INT32 offsetMv)
{
    RC rc;

    if (client >= NUMELEMS(m_ArbiterRequestParams.clientVoltageOffsetsuV))
        return RC::SOFTWARE_ERROR;

    CHECK_RC(GetArbiterRequestParams());
    m_ArbiterRequestParams.clientVoltageOffsetsuV[client] = offsetMv * 1000;
    CHECK_RC(SetVoltTuningParams(writeUnconditionally));

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::SetOverrideOVOC(bool override)
{
    return OK;
}

//-----------------------------------------------------------------------------
RC Perf::SetRailVoltTuningOffsetuV(Gpu::SplitVoltageDomain Rail, INT32 OffsetuV)
{
    Printf(Tee::PriNormal, "Rail voltage set is unsupported below PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Perf::SetRailClkDomailwoltTuningOffsetuV
(
    Gpu::SplitVoltageDomain Rail,
    Gpu::ClkDomain ClkDomain,
    INT32 OffsetuV
)
{
    Printf(Tee::PriNormal, "Rail voltage set is unsupported below PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Perf::SetFreqTuningOffsetKHz(INT32 OffsetKHz, INT32 PStateNum)
{
    RC rc;

    CHECK_RC(GetPStatesInfo());

    if (m_PStateVersion < PStateVersion::PSTATE_20)
    {
        return OK;
    }

    // We need to adjust all of the Gpc dependant clocks by this offset.
    // for the given PState, PState == -1 implies all pstates.
    for (UINT32 i=0; i < m_CachedPStates.size(); i++)
    {
        if ((m_CachedPStates[i].PStateNum == (UINT32)PStateNum) ||
            (PStateNum == -1))
        {
            int appliedOffset = OffsetKHz - m_CachedPStates[i].FreqOffsetKHz;
            if (appliedOffset != 0)
            {
                CHECK_RC(ApplyFreqTuneOffset(OffsetKHz,
                                    m_CachedPStates[i].PStateNum));
                m_CachedPStates[i].FreqOffsetKHz = OffsetKHz;
            }
        }
    }
    return OK;
}

INT32 Perf::GetFreqTuningOffsetKHz(INT32 PStateNum)
{
    for (UINT32 i=0; i < m_CachedPStates.size(); i++)
    {
        if (m_CachedPStates[i].PStateNum == (UINT32)PStateNum)
            return m_CachedPStates[i].FreqOffsetKHz;
    }

    return 0;
}

RC Perf::SetFreqClkDomainOffsetkHz(Gpu::ClkDomain ClkDomain, INT32 FrequencyInkHz)
{
    Printf(Tee::PriNormal,
        "Clock Domain frequency offset set is unsupported below PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetFreqClkDomainOffsetPercent(Gpu::ClkDomain ClkDomain, FLOAT32 percent)
{
    Printf(Tee::PriError,
        "Clock domain percentage frequency offsets are unsupported before PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetClkProgEntriesMHz
(
    UINT32 pstateNum,
    Gpu::ClkDomain clkDom,
    ProgEntryOffset offset,
    map<LwU8, clkProgEntry>* pEntriesMHz
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetClkVfRelationshipOffsetkHz(UINT32 RelIdx, INT32 FrequencyInkHz)
{
    Printf(Tee::PriError,
        "Clock VF relationship entry frequency offset set is unsupported below PState 4.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetClkProgrammingTableOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz)
{
    Printf(Tee::PriError,
        "Clock Programming Table entry frequency offset set is unsupported below PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetClkProgrammingTableSlaveRatio
(
    UINT32 tableIdx,
    UINT32 slaveEntryIdx,
    UINT08 *pRatio
)
{
    Printf(Tee::PriError,
           "Clock Programming Table entry master slave ratio "
           "is unsupported below pstate 3.0");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetClkProgrammingTableSlaveRatio
(
    UINT32 tableIdx,
    UINT32 slaveEntryIdx,
    UINT08 ratio
)
{
    Printf(Tee::PriError,
           "Clock Programming Table entry master slave ratio modification "
           "is unsupported below pstate 3.0");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetVFPointsOffsetkHz
(
    UINT32 TableIdxStart,
    UINT32 TableIdxEnd,
    INT32 FrequencyInkHz,
    UINT32 lwrveIdx
)
{
    Printf(Tee::PriError,
        "VF points offset set is unsupported below PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetVFPointsOffsetuV
(
    UINT32 TableIdxStart,
    UINT32 TableIdxEnd,
    INT32 OffsetInuV
)
{
    Printf(Tee::PriError,
        "VF points offset set is unsupported below PState 3.0\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Retrieve all of the PState data in one call
RC Perf::GetPStates2Data(Perf::PStates2Data * pP2D)
{
    const UINT32 numClks = Utility::CountBits(m_ClkDomainsMask);
    const UINT32 numVolts = Utility::CountBits(m_VoltageDomainsMask);
    const UINT32 numPStates = (UINT32)m_CachedPStates.size();

    if (!numPStates)
        return RC::SOFTWARE_ERROR;

    memset(&pP2D->Params, 0, sizeof(LW2080_CTRL_PERF_GET_PSTATES20_DATA_PARAMS));
    pP2D->Params.numPstates  = numPStates;
    pP2D->Params.numClocks   = numClks;
    pP2D->Params.numVoltages = numVolts;

    vector<LW2080_CTRL_PERF_PSTATE20_CLK_DOM_INFO>  emptyClkDomBuffer(numClks);
    pP2D->Clocks.clear();
    pP2D->Clocks.resize(numPStates, emptyClkDomBuffer);

    vector<LW2080_CTRL_PERF_PSTATE20_VOLT_DOM_INFO> emptyVoltDomBuffer(numVolts);
    pP2D->Volts.clear();
    pP2D->Volts.resize (numPStates, emptyVoltDomBuffer);

    for (UINT32 psIdx=0; psIdx < numPStates; psIdx++)
    {
        pP2D->Params.pstate[psIdx].pstateID = m_CachedPStates[psIdx].PStateBit;
        pP2D->Params.pstate[psIdx].flags = 0;
        pP2D->Params.pstate[psIdx].perfClkDomInfoList =
            LW_PTR_TO_LwP64(&pP2D->Clocks[psIdx][0]);
        pP2D->Params.pstate[psIdx].perfVoltDomInfoList =
            LW_PTR_TO_LwP64(&pP2D->Volts[psIdx][0]);

        UINT32 clkIdx = 0;
        UINT32 vltIdx = 0;
        // setup the Clock / voltage domain in each list
        for (UINT32 bitNum = 0; bitNum < 32; bitNum++)
        {
            const UINT32 bitMask = 1<<bitNum;
            if (m_ClkDomainsMask & bitMask)
            {
                pP2D->Clocks[psIdx][clkIdx].domain = bitMask;
                clkIdx++;
            }
            if (m_VoltageDomainsMask & bitMask)
            {
                pP2D->Volts[psIdx][vltIdx].domain = bitMask;
                // is this needed?
                pP2D->Volts[psIdx][vltIdx].type =
                    LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT;
                vltIdx++;
            }
        }
    }
    RC rc;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
        LW2080_CTRL_CMD_PERF_GET_PSTATES20_DATA,
        &pP2D->Params,
        sizeof(pP2D->Params)));

    pP2D->Initialized = true;

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetClockRange
(
    UINT32 PStateNum,
    Gpu::ClkDomain Domain,
    ClkRange *pClkRange
)
{
    MASSERT(pClkRange);
    RC rc;
    CHECK_RC(GetPStatesInfo());

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(Domain))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(Domain);
        Domain = PerfUtil::ColwertToOppositeClockDomain(Domain);
    }

    UINT32 RmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Domain);
    UINT32 Idx = 0;
    CHECK_RC(GetPStateIdx(PStateNum, &Idx));
    map<UINT32, ClkRange> &ClkInfo = m_CachedPStates[Idx].ClkVals;
    if (ClkInfo.find(RmDomain) == ClkInfo.end())
    {
        return RC::ILWALID_CLOCK_DOMAIN;
    }
    *pClkRange = ClkInfo[RmDomain];

    // Adjust the range using freqFactor
    pClkRange->MinKHz = static_cast<UINT32>(pClkRange->MinKHz * freqRatio);
    pClkRange->MaxKHz = static_cast<UINT32>(pClkRange->MaxKHz * freqRatio);
    pClkRange->LwrrKHz = static_cast<UINT32>(pClkRange->LwrrKHz * freqRatio);

    return rc;
}

//-----------------------------------------------------------------------------
bool Perf::IsClockInRange(UINT32 PStateNum, UINT32 rmDomain, INT32 freqKHz)
{
    UINT32 Idx;
    RC rc;
    if (m_PStateVersion >= PStateVersion::PSTATE_20)
    {
        rc = GetPStatesInfo();
        if (rc != OK)
            return false;

        const INT32 freqTuneOffsetKHz =  GetFreqTuningOffsetKHz(PStateNum);
        rc = GetPStateIdx(PStateNum, &Idx);
        if (rc != OK)
            return false;

        const INT32 MinKHz = m_CachedPStates[Idx].ClkVals[rmDomain].MinKHz + freqTuneOffsetKHz;
        const INT32 MaxKHz = m_CachedPStates[Idx].ClkVals[rmDomain].MaxKHz + freqTuneOffsetKHz;
        if ((freqKHz < MinKHz) || (freqKHz > MaxKHz))
        {
            Printf(Tee::PriLow,
                  "clock freq not in a range for specified pstate. "
                  "PState=%d RmDomain=%x ClkDomain = %d FreqKHz=%d. MaxKHz=%d MinKHz=%d\n",
                   PStateNum, rmDomain, PerfUtil::ClkCtrl2080BitToGpuClkDomain(rmDomain),
                   freqKHz, MaxKHz, MinKHz);
            return false;
        }
    }
    return true;
}

bool Perf::IsClockInOriginalRange(UINT32 pstateNum, UINT32 rmDomain, UINT32 freqkHz)
{
    RC rc;
    if (m_PStateVersion >= PStateVersion::PSTATE_20)
    {
        UINT32 idx;
        rc = GetPStateIdx(pstateNum, &idx);
        if (OK != rc)
        {
            return false;
        }
        const UINT32 minkHz = m_OrgPStates[idx].ClkVals[rmDomain].MinKHz;
        const UINT32 maxkHz = m_OrgPStates[idx].ClkVals[rmDomain].MaxKHz;
        if (minkHz <= freqkHz && freqkHz <= maxkHz)
        {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
RC Perf::GetPStatesInfo()
{
    RC rc;

    if (m_CacheStatus == CACHE_CLEAN && m_PStatesInfoRetrieved)
        return OK;

    // Ignore P-states if they have been disabled
    JavaScriptPtr pJS;
    bool bPStateDisable = false;
    if (OK != pJS->GetProperty(pJS->GetGlobalObject(), "g_PStateDisable", &bPStateDisable))
        bPStateDisable = false;
    if (bPStateDisable && !m_pSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_PSTATES_IN_PMU))
    {
        m_CachedPStates.clear();
        m_PStatesInfoRetrieved = true;
        return rc;
    }

    Tasker::MutexHolder mh(GetMutex());

    // m_CacheStatus will become CLEAN after Init() so we have to save it now
    // to know if it was originally DIRTY.
    CacheState originalCacheState = m_CacheStatus;

    CHECK_RC(Init());

    // -pstate_disable case
    if (m_CachedPStates.empty())
    {
        m_PStatesInfoRetrieved = true;
        return rc;
    }

    // Update m_CachedPStates
    CHECK_RC(GetLwrrentPerfValues(m_CachedPStates[0].PStateNum));

    if (!m_PStatesInfoRetrieved && !m_CachedPStates.empty())
        m_OrgPStates = m_CachedPStates;

    // Build the list of named PerfPoints (e.g. MIN, MAX, INTERSECT)
    // Always rebuild these PerfPoints in PStates 3.0 because pstate clock
    // ranges can change due to VF offsets which means MIN/MAX may change.
    if ((!m_PStatesInfoRetrieved && !m_CachedPStates.empty()) ||
        (m_PStateVersion >= PStateVersion::PSTATE_30 && originalCacheState == CACHE_DIRTY))
    {
        // Clear any (possibly) previously retrieved named PerfPoints
        m_PerfPoints.clear();

        // Build TDP and TURBO PerfPoints first so we know if any
        // other named PerfPoints are unsafe
        CHECK_RC(GetVirtualPerfPoints(&m_PerfPoints));

        // tdpGpcPerfHz is the highest gpcclk that can safely be set while
        // running stress tests with power-capping disabled.
        UINT64 tdpGpcPerfHz = 0;
        if (HasVirtualPState(GpcPerf_TDP))
        {
            CHECK_RC(GetTDPGpcHz(&tdpGpcPerfHz));
        }
        else if (!m_PStatesInfoRetrieved && !m_pSubdevice->IsSOC()) // Only warn once
        {
            Printf(Tee::PriWarn, "TDP VPState is not set in the VBIOS on %s\n",
                   m_pSubdevice->GpuIdentStr().c_str());
        }

        if (m_PStateVersion >= PStateVersion::PSTATE_30 && originalCacheState == CACHE_DIRTY)
        {
            CHECK_RC(GenerateStandardPerfPoints(
                tdpGpcPerfHz, m_CachedPStates, &m_PerfPoints));
        }
        else
        {
             CHECK_RC(GenerateStandardPerfPoints(
                tdpGpcPerfHz, m_OrgPStates, &m_PerfPoints));
        }

    }

    m_PStatesInfoRetrieved = true;
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetLwrrentPerfValues(UINT32 PStateNum)
{
    RC rc;

    // the RM call fails if either one of these is 0
    if (Utility::CountBits(m_ClkDomainsMask) == 0)
    {
        m_CacheStatus = CACHE_CLEAN;
        m_PStatesInfoRetrieved = true;
        return OK;
    }

    if (m_CacheStatus == CACHE_DIRTY)
    {
        CHECK_RC(GetLwrrentPerfValuesNoCache(&m_CachedPStates));
        m_CacheStatus = CACHE_CLEAN;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetPStateIdx(UINT32 PStateNum, UINT32 *pPStateIdx) const
{
    // this should be a quick search
    for (UINT32 i = 0 ; i < m_CachedPStates.size(); i++)
    {
        if (PStateNum == m_CachedPStates[i].PStateNum)
        {
            *pPStateIdx = i;
            return OK;
        }
    }
    Printf(Tee::PriNormal, "PState %d not in perf table\n", PStateNum);
    return RC::SOFTWARE_ERROR;
}
//-----------------------------------------------------------------------------
bool Perf::IsPState20Supported()
{
    GetPStatesInfo();
    return m_PStateVersion >= PStateVersion::PSTATE_20;
}
//-----------------------------------------------------------------------------
bool Perf::IsPState30Supported()
{
    GetPStatesInfo();
    return m_PStateVersion >= PStateVersion::PSTATE_30;
}
//-----------------------------------------------------------------------------
bool Perf::IsPState35Supported()
{
    GetPStatesInfo();
    return m_PStateVersion >= PStateVersion::PSTATE_35;
}

bool Perf::IsPState40Supported()
{
    GetPStatesInfo();
    return m_PStateVersion >= PStateVersion::PSTATE_40;
}
//-----------------------------------------------------------------------------
RC Perf::GetNumPStates(UINT32 *pNumPStates)
{
    MASSERT(pNumPStates);
    RC rc;
    if (!m_PStatesInfoRetrieved)
    {
        CHECK_RC(GetPStatesInfo());
    }

    *pNumPStates = (UINT32)m_CachedPStates.size();
    return rc;
}
//-----------------------------------------------------------------------------
RC Perf::DoesPStateExist(UINT32 PStateNum, bool *pRetVal)
{
    MASSERT(pRetVal);
    RC rc;
    if (!m_PStatesInfoRetrieved)
    {
        CHECK_RC(GetPStatesInfo());
    }

    *pRetVal = false;
    for (UINT32 i = 0 ; i < m_CachedPStates.size(); i++)
    {
        if (PStateNum == m_CachedPStates[i].PStateNum)
        {
            *pRetVal = true;
            break;
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC Perf::GetAvailablePStates(vector<UINT32> *pPStates)
{
    MASSERT(pPStates);
    RC rc;
    if (!m_PStatesInfoRetrieved)
    {
        CHECK_RC(GetPStatesInfo());
    }

    // should be a very short copy
    for (UINT32 i = 0; i < m_CachedPStates.size(); i++)
    {
        pPStates->push_back(m_CachedPStates[i].PStateNum);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetDefaultClockAtPState
(
    UINT32 pstate,
    Gpu::ClkDomain domain,
    UINT64 *pFreqHz
)
{
    MASSERT(pFreqHz);
    RC rc;

    if ((!m_pSubdevice->HasDomain(domain)) ||
        (pstate == ILWALID_PSTATE) ||
        (!m_PStatesInfoRetrieved))
    {
        return RC::SOFTWARE_ERROR;
    }

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(domain))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(domain);
        domain = PerfUtil::ColwertToOppositeClockDomain(domain);
    }

    const UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(domain);
    UINT32 idx;
    CHECK_RC(GetPStateIdx(pstate, &idx));
    *pFreqHz = static_cast<UINT64>
        (m_OrgPStates[idx].ClkVals[rmDomain].LwrrKHz * 1000ULL * freqRatio);
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetClockAtPState(UINT32 PStateNum,
                          Gpu::ClkDomain Domain,
                          UINT64 *pFreqHz)
{
    MASSERT(pFreqHz);
    RC rc;

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    Gpu::ClkDomain oriDomain = Domain;
    if (!IsRmClockDomain(Domain))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(Domain);
        Domain = PerfUtil::ColwertToOppositeClockDomain(Domain);
        if (Domain == Gpu::ClkUnkUnsp)
        {
            Printf(Tee::PriError, "Specified clock domain doesn't exist.\n");
            return RC::ILWALID_CLOCK_DOMAIN;
        }
    }

    UINT32 RmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Domain);
    UINT32 multiplier = (Domain == Gpu::ClkPexGen) ? 1 : 1000;
    if (CACHE_DIRTY != m_CacheStatus)
    {
        UINT32 Idx = 0;
        CHECK_RC(GetPStateIdx(PStateNum, &Idx));
        *pFreqHz = static_cast<UINT64>
            (m_CachedPStates[Idx].ClkVals[RmDomain].LwrrKHz * freqRatio * multiplier);
        return rc;
    }

    CHECK_RC(GetClockAtPStateNoCache(PStateNum, oriDomain, pFreqHz));

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetGpcFreqOfPerfPoint
(
    const Perf::PerfPoint & perfPoint,
    UINT64 * pGpcPerfHz
)
{
    RC rc = RC::OK;
    MASSERT(pGpcPerfHz);
    PerfPoint stdPerfPoint(ILWALID_PSTATE, NUM_GpcPerfMode);
    if (perfPoint.SpecialPt != GpcPerf_EXPLICIT)
    {
        stdPerfPoint = GetMatchingStdPerfPoint(perfPoint);
    }
    const INT64 freqTuneOffsetHz =
        GetFreqTuningOffsetKHz(perfPoint.PStateNum) * 1000ULL;
    const bool IsIntersectPt = IsGpcClkAtIntersect(perfPoint);

    // Find a base GpcClk to detect underflow due to -ve offsets
    UINT64 GpcBaseClk = 0;
    bool ps30 = IsPState30Supported();

    if (IsIntersectPt)
    {
        if (ps30)
        {
            // Gpc clock is determined automatically for intersect points,
            // it should not be needed.
            *pGpcPerfHz = 0;
            return OK;
        }
        if (stdPerfPoint.SpecialPt == NUM_GpcPerfMode)
        {
            string specialPtName = perfPoint.name(ps30);
            Printf(Tee::PriError,
                    "PerfPoint %s unsupported on this board.\n", specialPtName.c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        UINT64 perfPointGpcPerHz = 0;
        perfPoint.GetFreqHz(Gpu::ClkGpc, &perfPointGpcPerHz);
        UINT64 stdPerfPointGpcPerfHz = 0;
        stdPerfPoint.GetFreqHz(Gpu::ClkGpc, &stdPerfPointGpcPerfHz);

        if ((perfPointGpcPerHz != 0) &&
            (perfPointGpcPerHz != stdPerfPointGpcPerfHz))
        {
            string specialPtName = perfPoint.name(ps30);
            Printf(Tee::PriWarn,
                   "Warning: PerfPoint %s GpcPerfHz %llu is ignored.\n"
                   "Actual GpcPerfHz comes from RM & vbios.\n",
                   specialPtName.c_str(), perfPointGpcPerHz);
        }
        GpcBaseClk = stdPerfPointGpcPerfHz;
    }
    else // non-intersect pts
    {
        bool hasGpcClkOverrideInPerfPoint = false;
        ClkMap::const_iterator gpcClkItr = perfPoint.Clks.find(Gpu::ClkGpc);
        if (gpcClkItr != perfPoint.Clks.end())
        {
            GpcBaseClk = gpcClkItr->second.FreqHz;
            hasGpcClkOverrideInPerfPoint = true;
        }

        ClkMap::const_iterator gpc2ClkItr = perfPoint.Clks.find(Gpu::ClkGpc2);
        if (gpc2ClkItr != perfPoint.Clks.end())
        {
            GpcBaseClk = gpc2ClkItr->second.FreqHz / 2;
            hasGpcClkOverrideInPerfPoint = true;
        }
        if (!hasGpcClkOverrideInPerfPoint)
        {
            if (stdPerfPoint.SpecialPt == NUM_GpcPerfMode)
            {
                Printf(Tee::PriNormal, "PerfPoint %s must specify GpcPerfHz.\n",
                       perfPoint.name(ps30).c_str());
                return RC::UNSUPPORTED_FUNCTION;
            }
            if (stdPerfPoint.GetFreqHz(Gpu::ClkGpc, &GpcBaseClk) != RC::OK)
            {
                Printf(Tee::PriError, "%d.%s does not specify gpcclk \n",
                    stdPerfPoint.PStateNum, SpecialPtToString(stdPerfPoint.SpecialPt));
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    *pGpcPerfHz = GpcBaseClk + freqTuneOffsetHz;
    // Negative offsets tend to underflow and wrap around to be illegal
    // frequencies that fail the clock range check later
    if ((freqTuneOffsetHz < 0) &&
        (static_cast<UINT32>(-freqTuneOffsetHz) > GpcBaseClk))
    {
        if (IsIntersectPt)
        {
            // Intersect points fill in 0 and don't fail
            *pGpcPerfHz = 0;
        }
        else
        {
            // Non-intersect points fail, and fill in some sane value based
            // on whether perfPoint.GpcPerfHz is non-zero or not
            *pGpcPerfHz = GpcBaseClk;
            Printf(Tee::PriError,
                   "Invalid gpcclk after offset %lld for perfPoint %s!\n",
                   freqTuneOffsetHz, perfPoint.name(ps30).c_str());
            return RC::INCORRECT_FREQUENCY;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf::GetClockAtPerfPoint
(
    const Perf::PerfPoint & perfPoint,
    Gpu::ClkDomain domain,
    UINT64 *pFreqHz  // return value
)
{
    if ((!m_pSubdevice->HasDomain(domain)) ||
        (!CanSetClockDomailwiaPState(domain)))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    MASSERT(pFreqHz);
    RC rc;
    const bool isRatiod = (m_ClockDomains[domain].RatioDomain == Gpu::ClkGpc2 ||
                           m_ClockDomains[domain].RatioDomain == Gpu::ClkGpc ||
                           domain == Gpu::ClkGpc2 ||
                           domain == Gpu::ClkGpc);

    if (isRatiod && (perfPoint.SpecialPt == GpcPerf_INTERSECT))
    {
        Printf(Tee::PriNormal, "Can't predict gpcclk-ratio'd clock of %s.\n",
               perfPoint.name(IsPState30Supported()).c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Is this domain overridden in the PerfPoint itself?
    // Search to see if there is a match in the PerfPoint's Clks for either
    // 1x or 2x domains
    bool using1xClockDomain = PerfUtil::Is1xClockDomain(domain);
    Gpu::ClkDomain d2x = PerfUtil::ColwertTo2xClockDomain(domain);
    Gpu::ClkDomain d1x = PerfUtil::ColwertTo1xClockDomain(domain);

    if (d1x != Gpu::ClkUnkUnsp &&
        perfPoint.Clks.find(d1x) != perfPoint.Clks.end())
    {
        if (using1xClockDomain)
            *pFreqHz = perfPoint.Clks.find(d1x)->second.FreqHz;
        else
            *pFreqHz = perfPoint.Clks.find(d1x)->second.FreqHz * 2;

        return OK;
    }
    else if (d2x != Gpu::ClkUnkUnsp &&
             perfPoint.Clks.find(d2x) != perfPoint.Clks.end())
    {
        if (using1xClockDomain)
            *pFreqHz = perfPoint.Clks.find(d2x)->second.FreqHz / 2;
        else
            *pFreqHz = perfPoint.Clks.find(d2x)->second.FreqHz;

        return OK;
    }
    // Not a 1x/2x domain
    else if (perfPoint.Clks.find(domain) != perfPoint.Clks.end())
    {
        *pFreqHz = perfPoint.Clks.find(domain)->second.FreqHz;
        return OK;
    }

    if (isRatiod)
    {
        // Override the ratio'd clocks to match the PerfPoint's gpcclk.
        UINT64 gpcPerfHz;
        CHECK_RC(GetGpcFreqOfPerfPoint(perfPoint, &gpcPerfHz));
        if (domain == Gpu::ClkGpc2 || domain == Gpu::ClkGpc)
        {
            *pFreqHz = gpcPerfHz * (using1xClockDomain ? 1 : 2);
        }
        else
        {
            FLOAT64 ratio = m_ClockDomains[domain].Ratio / 100.0;
            *pFreqHz = static_cast<UINT64>(gpcPerfHz * ratio * (using1xClockDomain ? 1 : 2));
        }
        return OK;
    }

    // For non-ratio domains, return the default clock for the pstate
    if (OK == GetClockAtPState(perfPoint.PStateNum, domain, pFreqHz))
        return OK;

    // Shouldn't get here.
    Printf(Tee::PriError, "Clock domain is not found\n");
    return RC::SOFTWARE_ERROR;
}

RC Perf::PerfPoint::GetFreqHz(Gpu::ClkDomain clkDom, UINT64* pFreqHz) const
{
    *pFreqHz = 0;
    if (PerfUtil::Is1xClockDomain(clkDom) || PerfUtil::Is2xClockDomain(clkDom))
    {
        Gpu::ClkDomain oppClkDom = PerfUtil::ColwertToOppositeClockDomain(clkDom);
        ClkMap::const_iterator ptClkItr = Clks.find(clkDom);
        ClkMap::const_iterator oppPtClkItr = Clks.find(oppClkDom);
        if (ptClkItr != Clks.end())
        {
            *pFreqHz = ptClkItr->second.FreqHz;
        }
        else if (oppPtClkItr != Clks.end())
        {
            if (PerfUtil::Is1xClockDomain(clkDom))
            {
                *pFreqHz = oppPtClkItr->second.FreqHz / 2;
            }
            else if (PerfUtil::Is2xClockDomain(clkDom))
            {
                *pFreqHz = oppPtClkItr->second.FreqHz * 2;
            }
        }
        else
        {
            return RC::ILWALID_CLOCK_DOMAIN;
        }
    }
    return RC::OK;
}

// !Measures the true frequency of a clock domain's PLL
RC Perf::MeasurePllClock
(
    Gpu::ClkDomain clkToGet,
    UINT32 pllIndex,
    UINT64 * pFreqHz
)
{
    if (!m_pSubdevice->HasDomain(clkToGet) || !pFreqHz)
    {
        return RC::BAD_PARAMETER;
    }
    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_CLK_MEASURE_PLL_FREQ_PARAMS params = {0};

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(clkToGet))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(clkToGet);
        clkToGet = PerfUtil::ColwertToOppositeClockDomain(clkToGet);
    }

    params.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkToGet);
    params.pllIdx = pllIndex;

    CHECK_RC(pLwRm->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_CLK_MEASURE_PLL_FREQ,
        &params,
        sizeof(params)));

    *pFreqHz = static_cast<UINT64>(params.freqKHz * freqRatio * 1000);

    return rc;
}

RC Perf::MeasureClock(Gpu::ClkDomain clk, UINT64 *pFreqHz)
{
    MASSERT(pFreqHz);
    if (!m_pSubdevice->HasDomain(clk) || !pFreqHz)
    {
        return RC::BAD_PARAMETER;
    }
    bool isClkPexGen = (clk == Gpu::ClkPexGen) ? true : false;
    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS params = {0};

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(clk))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(clk);
        clk = PerfUtil::ColwertToOppositeClockDomain(clk);
    }

    params.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clk);

    CHECK_RC(pLwRm->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
        &params,
        sizeof(params)));

    *pFreqHz = static_cast<UINT64>
        (params.freqKHz * (isClkPexGen ? 1ULL : 1000ULL) * freqRatio);

    return rc;
}

RC Perf::MeasureCoreClock(Gpu::ClkDomain clk, UINT64 *pEffectiveFreqHz)
{
    MASSERT(pEffectiveFreqHz);
    RC rc;
    vector<UINT64> effectiveFreqsHz;
    CHECK_RC(MeasureClockPartitions(clk, 0, &effectiveFreqsHz));
    *pEffectiveFreqHz = effectiveFreqsHz[0];
    return rc;
}

RC Perf::MeasureAvgClkCount(Gpu::ClkDomain clk, UINT64 *pClkCount)
{
    MASSERT(pClkCount);
    RC rc;

    // Fetch per-partition clock samples
    vector<LW2080_CTRL_CLK_CNTR_SAMPLE> clockSamples;
    CHECK_RC(MeasureClockPartitions(clk, GetClockPartitionMask(clk), nullptr, &clockSamples));
    if (clockSamples.empty())
    {
        Printf(Tee::PriError,
               "Could not measure clock counter for %s domain\n",
               PerfUtil::ClkDomainToStr(clk));
        return RC::SOFTWARE_ERROR;
    }

    // Average Clock Samples
    *pClkCount = 0;
    for (const auto sample : clockSamples)
    {
        *pClkCount += sample.tickCnt;
    }
    *pClkCount /= clockSamples.size();
    return rc;
}

RC Perf::MeasureClockPartitions
(
    Gpu::ClkDomain clkToGet,
    UINT32 partitionMask,
    vector<UINT64> *pEffectiveFreqsHz,
    vector<LW2080_CTRL_CLK_CNTR_SAMPLE>* pClockSamples
)
{
    RC rc;
    if ((UINT32) Utility::CountBits(partitionMask) > LW2080_CTRL_CLK_DOMAIN_PART_IDX_MAX)
        return RC::BAD_PARAMETER;

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(clkToGet))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(clkToGet);
        clkToGet = PerfUtil::ColwertToOppositeClockDomain(clkToGet);
    }

    if (!m_pSubdevice->HasDomain(clkToGet))
    {
        Printf(Tee::PriError,
               "GPU does not have clock %s\n",
               PerfUtil::ClkDomainToStr(clkToGet));
        return RC::BAD_PARAMETER;
    }

    // partitionMask == 0 is a valid situation for this function
    UINT32 clockPartitionMask = GetClockPartitionMask(clkToGet);
    if (!IS_MASK_SUBSET(partitionMask, clockPartitionMask))
    {
        Printf(Tee::PriError, "Input mask (0x%x) must be a subset of (0x%x)\n",
               partitionMask, clockPartitionMask);
        return RC::BAD_PARAMETER;
    }

    if (pEffectiveFreqsHz)
    {
        pEffectiveFreqsHz->clear();
    }
    if (pClockSamples)
    {
        pClockSamples->clear();
    }

    LW2080_CTRL_CLK_CNTR_MEASURE_AVG_FREQ_PARAMS avgFreqParams = {};
    avgFreqParams.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkToGet);
    avgFreqParams.b32BitCntr = LW_FALSE;

    UINT32 numPartitions = Utility::CountBits(partitionMask);
    if (0 == numPartitions)
    {
        avgFreqParams.numParts = 1;
        avgFreqParams.parts[0].partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;

        // Set to 0x1 so that when the measured frequencies are pulled from
        // LW2080_CTRL_CLK_CNTR_MEASURE_AVG_FREQ_PARAMS::parts, we don't
        // have to have separate handling for partition-less clock domains
        partitionMask = 0x1;
    }
    else
    {
        avgFreqParams.numParts = numPartitions;
        UINT32 ii = 0;
        INT32 bitIdx;
        UINT32 tmpMask = partitionMask;
        while ((bitIdx = Utility::BitScanForward(tmpMask)) >= 0)
        {
            avgFreqParams.parts[ii++].partIdx = bitIdx;
            tmpMask ^= 1 << bitIdx;
        }
        for ( ; ii < LW2080_CTRL_CLK_DOMAIN_PART_IDX_MAX; ii++)
        {
            avgFreqParams.parts[ii].partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;
        }
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ
       ,&avgFreqParams
       ,sizeof(avgFreqParams)));

    Utility::SleepUS(100);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ
       ,&avgFreqParams
       ,sizeof(avgFreqParams)));

    for (UINT32 i = 0; i < avgFreqParams.numParts; ++i)
    {
        if (pEffectiveFreqsHz)
        {
            pEffectiveFreqsHz->push_back(
                static_cast<UINT64>(avgFreqParams.parts[i].freqkHz * freqRatio * 1000ULL));
        }
        if (pClockSamples)
        {
            pClockSamples->push_back(avgFreqParams.parts[i].sample);
        }
    }

    return rc;
}

RC Perf::ForceClock
(
    Gpu::ClkDomain clkDomain,
    UINT64 targetkHz,
    UINT32 flags
)
{
    RC rc;

    if (!m_pSubdevice->CanClkDomainBeProgrammed(clkDomain))
    {
        Printf(Tee::PriError,
               "RM reports clk domain 0x%x (%s) not settable.\n",
               clkDomain, PerfUtil::ClkDomainToStr(clkDomain));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    LW2080_CTRL_CLK_SET_INFO_PARAMS ctrlParams = {0};
    LW2080_CTRL_CLK_INFO ctrlInfo[1];
    ctrlParams.flags = LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
    ctrlParams.clkInfoList = LW_PTR_TO_LwP64(&ctrlInfo);
    ctrlParams.clkInfoListSize = 1;
    memset(ctrlInfo, 0, sizeof(ctrlInfo));

    if (targetkHz > (std::numeric_limits<LwU32>::max)())
    {
        Printf(Tee::PriError, "GpuSubdevice::SetClock targetkHz larger than LwU32\n");
        return RC::SOFTWARE_ERROR;
    }

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(clkDomain))
    {
        freqRatio = PerfUtil::GetModsToRmFreqRatio(clkDomain);
        clkDomain = PerfUtil::ColwertToOppositeClockDomain(clkDomain);
    }

    ctrlInfo[0].targetFreq = static_cast<LwU32>(targetkHz * freqRatio);
    ctrlInfo[0].clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkDomain);
    if (flags & FORCE_PLL)
    {
        ctrlInfo[0].flags |= DRF_DEF(2080_CTRL_CLK_INFO, _FLAGS,
                                     _FORCE_PLL, _ENABLE);
    }
    if (flags & FORCE_BYPASS)
    {
        ctrlInfo[0].flags |= DRF_DEF(2080_CTRL_CLK_INFO, _FLAGS,
                                     _FORCE_BYPASS, _ENABLE);
    }
    if (flags & FORCE_NAFLL)
    {
        ctrlInfo[0].flags |= DRF_DEF(2080_CTRL_CLK_INFO, _FLAGS,
                                     _FORCE_NAFLL, _ENABLE);
    }

    if (clkDomain == Gpu::ClkGpc2 || clkDomain == Gpu::ClkGpc)
    {
        GetClockChangeCounter().AddExpectedChange(static_cast<UINT32>(targetkHz));
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_CLK_SET_INFO,
                                           &ctrlParams, sizeof(ctrlParams)));

    return rc;
}

RC Perf::GetVoltageAtPState(UINT32 PStateNum,
                            Gpu::VoltageDomain Domain,
                            UINT32 *pVoltageMv)
{
    MASSERT(pVoltageMv);
    RC rc;
    UINT32 Idx;
    CHECK_RC(GetPStateIdx(PStateNum, &Idx));
    UINT32 RmDomain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Domain);

    if (CACHE_DIRTY == m_CacheStatus)
    {
        UINT32 VoltageUv;
        CHECK_RC(GetVoltageAtPStateNoCache(PStateNum, Domain, &VoltageUv));
        m_CachedPStates[Idx].VoltageVals[RmDomain].LwrrentTargetMv = VoltageUv / 1000;
    }

    *pVoltageMv = m_CachedPStates[Idx].VoltageVals[RmDomain].LwrrentTargetMv;
    return rc;
}

RC Perf::GetPerfSpecTable
(
    LW2080_CTRL_PERF_GET_PERF_TEST_SPEC_TABLE_INFO_PARAMS * pSpecTable
)
{
    RC rc;
    MASSERT(pSpecTable);
    memset(pSpecTable, 0, sizeof(*pSpecTable));

    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                    LW2080_CTRL_CMD_PERF_GET_PERF_TEST_SPEC_TABLE_INFO,
                                    pSpecTable, sizeof(*pSpecTable)));

    return rc;
}

RC Perf::SetForcedPStateLockType(PStateLockType type)
{
    RC rc;

    m_ForcedPStateLockType = type;
    if (m_ForcedPStateLockType == HardLock)
    {
        UINT32 LwrrPState;
        CHECK_RC(GetLwrrentPState(&LwrrPState));
        CHECK_RC(ForcePState(LwrrPState,
                             LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    }

    return rc;
}

RC Perf::GetForcedPStateLockType(PStateLockType* pLockType) const
{
    *pLockType = m_ForcedPStateLockType;
    return OK;
}

RC Perf::SetUseHardPStateLocks(bool useHard)
{
    RC rc;

    m_ForcedPStateLockType = useHard ? HardLock : SoftLock;

    // make sure we relock to a HardLock
    if (useHard)
    {
        UINT32 LwrrPState;
        CHECK_RC(GetLwrrentPState(&LwrrPState));
        CHECK_RC(ForcePState(LwrrPState,
                             LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    }
    return rc;
}

RC Perf::GetUseHardPStateLocks(bool * pUseHard)
{
    MASSERT(pUseHard);

    if (m_ForcedPStateLockType == HardLock)
        *pUseHard = true;
    else
        *pUseHard = false;

    return OK;
}

RC Perf::UseIMPCompatibleLimits()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::RestoreDefaultLimits()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::PerfPointToJsObj
(
    const PerfPoint & perfPoint,
    JSObject *pObj,
    JSContext *pCtx
)
{
    RC rc;
    JavaScriptPtr pJs;

    CHECK_RC(pJs->SetProperty(pObj, "PStateNum", perfPoint.PStateNum));
    string LocationStr = Perf::SpecialPtToString(perfPoint.SpecialPt);
    CHECK_RC(pJs->SetProperty(pObj, "LocationStr", LocationStr));
    CHECK_RC(pJs->SetProperty(pObj, "InflectionPt",
                              static_cast<UINT32>(perfPoint.SpecialPt)));

    if (perfPoint.IntersectSettings.size())
    {
        JSObject *pISArray = JS_NewArrayObject(pCtx, 0, NULL);
        size_t isISArrayIndex = 0;
        for (set<IntersectSetting>::const_iterator isiter = perfPoint.IntersectSettings.begin();
             isiter != perfPoint.IntersectSettings.end();
             isiter++)
        {
            IntersectSetting const *is = &*isiter;
            JSObject *pISObj = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);

            CHECK_RC(pJs->SetProperty(pISObj, "Type", is->Type));
            string IVDStr = PerfUtil::SplitVoltDomainToStr(is->VoltageDomain);
            CHECK_RC(pJs->SetProperty(pISObj, "VoltageDomain", IVDStr));
            switch (is->Type)
            {
                case IntersectLogical:
                    CHECK_RC(pJs->SetProperty(pISObj, "LogicalVoltageuV",
                        is->LogicalVoltageuV));
                    break;
                case IntersectVDT:
                    CHECK_RC(pJs->SetProperty(pISObj, "VDTIndex",
                        is->VDTIndex));
                    break;
                case IntersectVFE:
                    CHECK_RC(pJs->SetProperty(pISObj, "VFEEquIndex",
                        is->VFEEquIndex));
                    break;
                case IntersectPState:
                    // No dedicated parameters
                    break;
                case IntersectVPState:
                    CHECK_RC(pJs->SetProperty(pISObj, "VPStateIndex",
                        is->VPStateIndex));
                    break;
                case IntersectVoltFrequency:
                case IntersectFrequency:
                    CHECK_RC(pJs->SetProperty(pISObj, "FrequencyClkDomain",
                        is->Frequency.ClockDomain));
                    CHECK_RC(pJs->SetProperty(pISObj, "FrequencyKHz",
                        is->Frequency.ValueKHz));
                    break;
                default:
                    Printf(Tee::PriError, "Unimplemented intersect type\n");
                    return RC::SOFTWARE_ERROR;
            }

            jsval isJsval;
            CHECK_RC(pJs->ToJsval(pISObj, &isJsval));
            CHECK_RC(pJs->SetElement(pISArray, static_cast<int>(isISArrayIndex++), isJsval));
        }
        jsval isArrayJsval;
        CHECK_RC(pJs->ToJsval(pISArray, &isArrayJsval));
        CHECK_RC(pJs->SetPropertyJsval(pObj, "IntersectSettings", isArrayJsval));
    }

    if (perfPoint.LinkSpeed)
        CHECK_RC(pJs->SetProperty(pObj, "LinkSpeed", perfPoint.LinkSpeed));
    if (perfPoint.LinkWidth)
        CHECK_RC(pJs->SetProperty(pObj, "LinkWidth", perfPoint.LinkWidth));
    if (perfPoint.FastPStateSwitch)
        CHECK_RC(pJs->SetProperty(pObj, "FastPStateSwitch",
                                  perfPoint.FastPStateSwitch));
    if (perfPoint.VoltTuningOffsetMv)
        CHECK_RC(pJs->SetProperty(pObj, "VoltTuningOffsetMv",
                                  perfPoint.VoltTuningOffsetMv));
    if (perfPoint.UnsafeDefault)
        CHECK_RC(pJs->SetProperty(pObj, "UnsafeDefault",
                                  perfPoint.UnsafeDefault));

    CHECK_RC(pJs->SetProperty(pObj, "Regime", perfPoint.Regime));

    if (!perfPoint.Clks.empty())
    {
        JSObject *pClkArray = JS_NewArrayObject(pCtx, 0, NULL);
        UINT32 ii = 0;
        for (ClkMap::const_iterator clkItr = perfPoint.Clks.begin();
             clkItr != perfPoint.Clks.end(); ++clkItr)
        {
            JSObject *pClkObj = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);
            CHECK_RC(pJs->SetProperty(pClkObj, "Domain",
                                      clkItr->first));
            string DomainStr = PerfUtil::ClkDomainToStr(clkItr->first);
            CHECK_RC(pJs->SetProperty(pClkObj, "DomainStr", DomainStr));

            if (clkItr->second.IsIntersect)
            {
                string IntersectPtStr =
                              Perf::SpecialPtToString(Perf::GpcPerf_INTERSECT);
                CHECK_RC(pJs->SetProperty(pClkObj,
                                          "FreqHz",
                                          IntersectPtStr));
            }
            else
                CHECK_RC(pJs->SetProperty(pClkObj, "FreqHz",
                                          clkItr->second.FreqHz));

            CHECK_RC(pJs->SetProperty(pClkObj, "Flags",
                                      clkItr->second.Flags));
            jsval ClkJsval;
            CHECK_RC(pJs->ToJsval(pClkObj, &ClkJsval));
            CHECK_RC(pJs->SetElement(pClkArray, static_cast<int>(ii++), ClkJsval));
        }
        jsval ClkArrayJsval;
        CHECK_RC(pJs->ToJsval(pClkArray, &ClkArrayJsval));
        CHECK_RC(pJs->SetPropertyJsval(pObj, "Clks", ClkArrayJsval));
    }
    if (perfPoint.Voltages.size())
    {
        JSObject *pVoltArray = JS_NewArrayObject(pCtx, 0, NULL);
        for (size_t i = 0; i < perfPoint.Voltages.size(); i++)
        {
            JSObject *pVoltObj = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);
            CHECK_RC(pJs->SetProperty(pVoltObj, "Domain",
                                      perfPoint.Voltages[i].Domain));
            string DomainStr = PerfUtil::VoltDomainToStr(
                perfPoint.Voltages[i].Domain);
            CHECK_RC(pJs->SetProperty(pVoltObj, "DomainStr", DomainStr));
            CHECK_RC(pJs->SetProperty(pVoltObj, "VoltMv",
                                      perfPoint.Voltages[i].VoltMv));
            jsval VoltJsval;
            CHECK_RC(pJs->ToJsval(pVoltObj, &VoltJsval));
            CHECK_RC(pJs->SetElement(pVoltArray, static_cast<int>(i),
                                     VoltJsval));
        }
        jsval VoltArrayJsval;
        CHECK_RC(pJs->ToJsval(pVoltArray, &VoltArrayJsval));
        CHECK_RC(pJs->SetPropertyJsval(pObj, "Voltages", VoltArrayJsval));
    }
    return rc;
}

//! This is more of a helper function (called from js_perf, dispatcher, etc.
//  takes a JsPerf object and turn it into a PerfPoint object
RC Perf::JsObjToPerfPoint(JSObject *pJsPerfPt, PerfPoint *pNewPerfPt)
{
    MASSERT(pJsPerfPt && pNewPerfPt);
    RC rc;
    JavaScriptPtr pJs;

    // Gpc2PerfHz and SpecialPt:
    //  - the JS object must set one or the other!
    //  - Gpc2PerfHz defaults to 0.
    //  - SpecialPt defaults to GpcPerf_EXPLICIT.

    JSString *locStrProp;

    UINT64 gpcPerfHz = 0;

    if (OK == pJs->GetProperty(pJsPerfPt, "LocationStr", &locStrProp))
    {
        string LocationStr;
        {
            JSContext *cx = nullptr;
            CHECK_RC(pJs->GetContext(&cx));
            LocationStr = DeflateJSString(cx, locStrProp);
        }

        vector<string> locationStrTokens;
        CHECK_RC(Utility::Tokenizer(LocationStr, ".", &locationStrTokens));
        if (locationStrTokens.size() == 0)
        {
            pNewPerfPt->SpecialPt = (GpcPerfMode)StringToSpecialPt("");
        }
        else
        {
            pNewPerfPt->SpecialPt = (GpcPerfMode)StringToSpecialPt(locationStrTokens[0]);
            if (locationStrTokens.size() > 1)
            {
                Printf(Tee::PriError,
                    "Unsupported LocationStr = \"%s\" - use "
                    "IntersectSettings to configure Intersect PerfPoints\n",
                    LocationStr.c_str());
                return RC::ILWALID_OBJECT_PROPERTY;
            }
        }
    }

    bool requirePStateNum = false;
    jsval intersectSettingsJsval;
    if (OK == pJs->GetPropertyJsval(pJsPerfPt, "IntersectSettings", &intersectSettingsJsval))
    {
        JsArray intersectSettingJsArray;
        if (OK != pJs->FromJsval(intersectSettingsJsval, &intersectSettingJsArray))
        {
            intersectSettingJsArray.push_back(intersectSettingsJsval);
        }
        for (UINT32 i = 0; i < intersectSettingJsArray.size(); i++)
        {
            JSObject *pIntersectSettingObj = NULL;
            CHECK_RC(pJs->FromJsval(intersectSettingJsArray[i], &pIntersectSettingObj));
            IntersectSetting is;

            CHECK_RC(pJs->GetProperty(pIntersectSettingObj, "Type", (UINT32*)&is.Type));

            string intersectVoltageDomainStr;
            if (OK == pJs->GetProperty(pIntersectSettingObj, "VoltageDomain",
                                       &intersectVoltageDomainStr))
            {
                is.VoltageDomain =
                    PerfUtil::SplitVoltDomainFromStr(intersectVoltageDomainStr.c_str());
            }

            switch (is.Type)
            {
                case IntersectLogical:
                    if (OK != pJs->GetProperty(pIntersectSettingObj, "LogicalVoltageuV",
                        (UINT32*)&is.LogicalVoltageuV))
                    {
                        Printf(Tee::PriError,
                            "Missing LogicalVoltageuV property for INTERSECT_LOGICAL\n");
                        return RC::ILWALID_OBJECT_PROPERTY;
                    }
                    break;
                case IntersectVDT:
                    if (OK != pJs->GetProperty(pIntersectSettingObj, "VDTIndex",
                        (UINT32*)&is.VDTIndex))
                    {
                        Printf(Tee::PriError,
                            "Missing VDTIndex property for INTERSECT_VDT\n");
                        return RC::ILWALID_OBJECT_PROPERTY;
                    }
                    break;
                case IntersectVFE:
                    if (OK != pJs->GetProperty(pIntersectSettingObj, "VFEEquIndex",
                        (UINT32*)&is.VFEEquIndex))
                    {
                        Printf(Tee::PriError,
                            "Missing VFEEquIndex property for INTERSECT_VFE\n");
                        return RC::ILWALID_OBJECT_PROPERTY;
                    }
                    break;
                case IntersectPState:
                    // No dedicated parameters.
                    requirePStateNum = true;
                    break;
                case IntersectVPState:
                    if (OK != pJs->GetProperty(pIntersectSettingObj, "VPStateIndex",
                        (UINT32*)&is.VPStateIndex))
                    {
                        Printf(Tee::PriError,
                            "Missing VPStateIndex property for INTERSECT_VPSTATE\n");
                        return RC::ILWALID_OBJECT_PROPERTY;
                    }
                    break;
                case IntersectVoltFrequency:
                case IntersectFrequency:
                {
                    const char *limitName = (is.Type == IntersectFrequency) ?
                        "INTERSECT_FREQUENCY" : "INTERSECT_VOLTFREQUENCY";
                    if (OK != pJs->GetProperty(pIntersectSettingObj, "FrequencyClkDomain",
                        (UINT32*)&is.Frequency.ClockDomain))
                    {
                        Printf(Tee::PriError,
                            "Missing FrequencyClkDomain property for %s\n", limitName);
                        return RC::ILWALID_OBJECT_PROPERTY;
                    }
                    if (OK != pJs->GetProperty(pIntersectSettingObj, "FrequencyKHz",
                        (UINT32*)&is.Frequency.ValueKHz))
                    {
                        Printf(Tee::PriError,
                            "Missing FrequencyKHz property for %s\n", limitName);
                        return RC::ILWALID_OBJECT_PROPERTY;
                    }
                    break;
                }
                default:
                    Printf(Tee::PriError, "Unrecognized intersect type = %d\n",
                        is.Type);
                    return RC::ILWALID_OBJECT_PROPERTY;
            }
            pNewPerfPt->IntersectSettings.insert(is);
        }
    }
    else
    {
        requirePStateNum = true;
    }

    pNewPerfPt->PStateNum = 0;
    if (requirePStateNum &&
        (OK != pJs->GetProperty(pJsPerfPt, "PStateNum", &pNewPerfPt->PStateNum)))
    {
        Printf(Tee::PriError, "Missing PStateNum PerfPoint property\n");
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    if (pNewPerfPt->SpecialPt != GpcPerf_EXPLICIT &&
        !(pNewPerfPt->SpecialPt == GpcPerf_INTERSECT &&
          !pNewPerfPt->IntersectSettings.empty()))
    {
        // Get a matching standard perfpoint
        const PerfPoint &perfPoint = *pNewPerfPt;
        const PerfPoint stdPerfPoint = GetMatchingStdPerfPoint(perfPoint);
        if (stdPerfPoint.SpecialPt == NUM_GpcPerfMode)
        {
            return RC::SOFTWARE_ERROR;
        }
        *pNewPerfPt = stdPerfPoint;
    }

    // don't check RC here since these are optional parameters
    pJs->GetProperty(pJsPerfPt, "FastPStateSwitch", &pNewPerfPt->FastPStateSwitch);
    pJs->GetProperty(pJsPerfPt,
                     "VoltTuningOffsetMv",
                     &pNewPerfPt->VoltTuningOffsetMv);
    jsval ClkJsval;
    if (OK == pJs->GetPropertyJsval(pJsPerfPt, "Clks", &ClkJsval))
    {
        JsArray ClkJsArray;
        CHECK_RC(pJs->FromJsval(ClkJsval, &ClkJsArray));
        for (UINT32 i = 0; i < ClkJsArray.size(); i++)
        {
            ClkSetting NewClk;
            JSObject *pClkObj = NULL;
            string FreqHzStr;
            CHECK_RC(pJs->FromJsval(ClkJsArray[i], &pClkObj));
            CHECK_RC(pJs->GetProperty(pClkObj, "Domain", (UINT32*)&NewClk.Domain));
            CHECK_RC(pJs->GetProperty(pClkObj, "FreqHz", &FreqHzStr));

            // Decoupled clocks can be specified with FreqHz = "intersect"
            NewClk.IsIntersect = (Utility::ToLowerCase(FreqHzStr) == "intersect")
                                 ? true : false;

            // Mark gpcClk @ intersect, if user specifies:
            // a) -gpcclk intersect, (OR)
            // b) -pstate XX.intersect
            if (!NewClk.IsIntersect &&
                (NewClk.Domain == Gpu::ClkGpc2 || NewClk.Domain == Gpu::ClkGpc))
            {
                if (pNewPerfPt->SpecialPt == GpcPerf_INTERSECT)
                    NewClk.IsIntersect = true;
            }

            if (!NewClk.IsIntersect || IsPState30Supported())
                CHECK_RC(pJs->GetProperty(pClkObj, "FreqHz", &NewClk.FreqHz));

            bool UseFixedFrequencyRegime;
            if (OK == pJs->GetProperty(pClkObj,
                                       "UseFixedFrequencyRegime",
                                       &UseFixedFrequencyRegime))
            {
                NewClk.Regime = UseFixedFrequencyRegime ?
                    Perf::FIXED_FREQUENCY_REGIME : Perf::DEFAULT_REGIME;
            }
            // Flags are optional
            NewClk.Flags = 0;
            pJs->GetProperty(pClkObj, "Flags", &NewClk.Flags);
            pNewPerfPt->Clks[NewClk.Domain] = NewClk;
        }
    }

    if (RC::OK != pJs->GetProperty(pJsPerfPt, "GpcPerfHz", &gpcPerfHz))
    {
        if (RC::OK == pJs->GetProperty(pJsPerfPt, "Gpc2PerfHz", &gpcPerfHz))
        {
            Printf(Tee::PriWarn, "Gpc2PerfHz is deprecated. Use GpcPerfHz instead.\n");
            gpcPerfHz /= 2;
        }
    }

    if (gpcPerfHz != 0)
    {
        UINT64 pNewPerfPtGpcPerfHz  = 0;
        if (pNewPerfPt->GetFreqHz(Gpu::ClkGpc, &pNewPerfPtGpcPerfHz) == RC::OK)
        {
         // When Gpc Freq is specified in the 'GpcPerfHz' field instead of the Clks array
            if (pNewPerfPtGpcPerfHz == 0)
            {
                pNewPerfPtGpcPerfHz = gpcPerfHz;
                pNewPerfPt->Clks[Gpu::ClkGpc] = ClkSetting(Gpu::ClkGpc, pNewPerfPtGpcPerfHz);
            }
        }
        else
        {
            pNewPerfPt->Clks[Gpu::ClkGpc] = ClkSetting(Gpu::ClkGpc, gpcPerfHz);
        }
    }

    if (pNewPerfPt->SpecialPt == GpcPerf_EXPLICIT)
    {
        if (pNewPerfPt->Clks[Gpu::ClkGpc].FreqHz == 0 &&
            pNewPerfPt->Clks[Gpu::ClkGpc2].FreqHz == 0)
        {
            Printf(Tee::PriError,
                   "JsPerfPoint must specify gpcclk or a LocationStr\n");
            return RC::ILWALID_OBJECT_PROPERTY;
        }
    }
    else if (m_PStateVersion < PStateVersion::PSTATE_30)
    {
        // For pstates with only one gpcperfhz, alias named LocationStr to "max".
        pNewPerfPt->SpecialPt = EffectiveGpcPerfMode(*pNewPerfPt);
    }

    jsval VoltJsval;
    if (OK == pJs->GetPropertyJsval(pJsPerfPt, "Voltages", &VoltJsval))
    {
        JsArray VoltJsArray;
        CHECK_RC(pJs->FromJsval(VoltJsval, &VoltJsArray));
        for (UINT32 i = 0; i < VoltJsArray.size(); i++)
        {
            VoltageSetting NewVolt;
            JSObject *pVoltObj = NULL;
            CHECK_RC(pJs->FromJsval(VoltJsArray[i], &pVoltObj));
            CHECK_RC(pJs->GetProperty(pVoltObj, "Domain", (UINT32*)&NewVolt.Domain));
            CHECK_RC(pJs->GetProperty(pVoltObj, "VoltMv", &NewVolt.VoltMv));
            pNewPerfPt->Voltages.push_back(NewVolt);
        }
    }

    pJs->GetProperty(pJsPerfPt, "LinkSpeed", &pNewPerfPt->LinkSpeed);
    pJs->GetProperty(pJsPerfPt, "LinkWidth", &pNewPerfPt->LinkWidth);

    UINT32 regimeNum;
    if (pJs->GetProperty(pJsPerfPt, "Regime", &regimeNum) == RC::OK)
    {
        pNewPerfPt->Regime = static_cast<Perf::RegimeSetting>(regimeNum);
    }

    return rc;
}

RC Perf::GetLwrrentPerfPoint(PerfPoint *pSetting)
{
    MASSERT(pSetting);
    RC rc;

    UINT32 NumPState = 0;
    CHECK_RC(GetNumPStates(&NumPState));
    if (NumPState == 0)
    {
        // no-op
        Printf(Tee::PriNormal,
               "PState needs to be enabled for GetLwrrentPerfPoint to work\n");
        return OK;
    }

    UINT32 gpcPerfMode = 0;
    rc = GetInflectionPoint(&pSetting->PStateNum, &gpcPerfMode);
    if (OK == rc)
    {
        pSetting->SpecialPt = static_cast<GpcPerfMode>(gpcPerfMode);
    }
    else
    {
        rc.Clear();
        CHECK_RC(GetLwrrentPState(&pSetting->PStateNum));
        pSetting->SpecialPt = GpcPerf_EXPLICIT;
    }
    pSetting->Clks.clear();
    pSetting->Voltages.clear();

    // find the PState number in m_CachedPStates;
    UINT32 PStateIdx = 0;
    CHECK_RC(GetPStateIdx(pSetting->PStateNum, &PStateIdx));

    map<UINT32, ClkRange>::const_iterator ClkIter =
        m_CachedPStates[PStateIdx].ClkVals.begin();
    for (; ClkIter != m_CachedPStates[PStateIdx].ClkVals.end(); ClkIter++)
    {
        ClkSetting NewClk;
        NewClk.Domain =
                     PerfUtil::ClkCtrl2080BitToGpuClkDomain(ClkIter->first);

        NewClk.IsIntersect =
                      m_CachedPStates[PStateIdx].IsClkIntersect[ClkIter->first];

        bool is2xDomain = PerfUtil::Is2xClockDomain(NewClk.Domain);
        if (is2xDomain)
        {
            NewClk.Domain = PerfUtil::ColwertTo1xClockDomain(NewClk.Domain);
        }

        // RM determines the frequency for clocks at their intersect point
        if (!NewClk.IsIntersect)
        {
            CHECK_RC(GetClockAtPState(
                pSetting->PStateNum, NewClk.Domain, &NewClk.FreqHz));
        }

        NewClk.Flags  = m_CachedPStates[PStateIdx].ClkFlags[ClkIter->first];
        pSetting->Clks[NewClk.Domain] = NewClk;
    }
    map<UINT32, VoltInfo>::const_iterator VolIter =
        m_CachedPStates[PStateIdx].VoltageVals.begin();
    for (; VolIter != m_CachedPStates[PStateIdx].VoltageVals.end(); ++VolIter)
    {
        VoltageSetting NewVolt;
        NewVolt.Domain = PerfUtil::VoltCtrl2080BitToGpuVoltDomain(VolIter->first);
        if (NewVolt.Domain == Gpu::ILWALID_VoltageDomain)
        {
            continue;
        }
        NewVolt.VoltMv = VolIter->second.LwrrentTargetMv;
        pSetting->Voltages.push_back(NewVolt);
    }

    pSetting->VoltTuningOffsetMv = m_VoltTuningOffsetsMv[VTUNE_PERFPOINT];

    return rc;
}

static LW2080_CTRL_PERF_CLK_DOM_INFO * GetClkParamToOverride
(
     UINT32 RmDomain,
     vector <LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo
)
{

    // Find the index to Clk parameter of same domain
    // In absence add a temporary entry to the list and return it.
    // *Returned entry must be overridden with correct params*
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> &ClkInfo = *pClkInfo;
    for (UINT32 i = 0; i < ClkInfo.size(); i++)
    {
        if (ClkInfo[i].domain == RmDomain)
            return &(ClkInfo[i]);
    }
    LW2080_CTRL_PERF_CLK_DOM_INFO Newentry = {0};
    Newentry.domain = RmDomain;
    Newentry.flags = 0;
    Newentry.freq = 0;
    ClkInfo.push_back(Newentry);
    return &ClkInfo.back();
}

// Set to a max possible value, unless user overrides it
const UINT32 Perf::DEFAULT_MAX_PERF_SWITCH_TIME_NS = (std::numeric_limits<UINT32>::max)();

//------------------------------------------------------------------------------
Perf::GpcPerfMode Perf::EffectiveGpcPerfMode(const Perf::PerfPoint & perfPoint) const
{
    UINT32 pstateIdx = 0;
    if (OK != GetPStateIdx(perfPoint.PStateNum, &pstateIdx))
        return perfPoint.SpecialPt;

    const UINT32 gpcRmDom = IsRmClockDomain(Gpu::ClkGpc) ?
        PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc) :
        PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc2);

    map<UINT32, ClkRange>::const_iterator it =
        m_OrgPStates[pstateIdx].ClkVals.find(gpcRmDom);
    if (it == m_OrgPStates[pstateIdx].ClkVals.end())
        return perfPoint.SpecialPt;
    const ClkRange &clkRange = it->second;

    if (clkRange.MinKHz == clkRange.MaxKHz)
    {
        // If a pstate has only one possible GpcPerfHz, we advertise only N.max
        // to avoid wasting test time, retesting at the same clocks.
        //
        // But we allow the user to use any GpcPerfMode as a colwenience, and
        // pretend they used N.max.  Bug 1003524 dislwsses this.
        return GpcPerf_MAX;
    }
    else
    {
        return perfPoint.SpecialPt;
    }
}

//------------------------------------------------------------------------------
Perf::PerfPoint Perf::GetMatchingStdPerfPoint
(
    const PerfPoint & point
)
{
    if (point.SpecialPt == NUM_GpcPerfMode || point.SpecialPt == GpcPerf_EXPLICIT)
    {
        return PerfPoint(ILWALID_PSTATE, NUM_GpcPerfMode);
    }

    // Need to hold the mutex in case GetPStatesInfo() is re-generating the
    // list of standard perfpoints
    Tasker::MutexHolder mh(GetMutex());

    // We should by default be returning a logic intersect even if
    // the intersect settings are not specified by the user.
    if (EffectiveGpcPerfMode(point) == GpcPerf_INTERSECT &&
        point.IntersectSettings.empty())
    {
        for (PerfPoints::const_iterator stdPtIt = m_PerfPoints.begin();
             stdPtIt != m_PerfPoints.end(); ++stdPtIt)
        {
            if (stdPtIt->PStateNum == point.PStateNum &&
                stdPtIt->SpecialPt == GpcPerf_INTERSECT &&
                stdPtIt->IntersectVoltageDomain() == Gpu::VOLTAGE_LOGIC)
                return *stdPtIt;
        }
        return PerfPoint(ILWALID_PSTATE, NUM_GpcPerfMode);
    }

    PerfPoints::const_iterator perfPointIt =
        m_PerfPoints.find(
            PerfPoint(point.PStateNum, EffectiveGpcPerfMode(point), 0,
                point.IntersectSettings));

    if (perfPointIt != m_PerfPoints.end())
        return *perfPointIt;
    else
    {
        Printf(Tee::PriError,
               "Cannot find a matching standard PerfPoint for %u.%s\n",
               point.PStateNum, SpecialPtToString(point.SpecialPt));
        Printf(Tee::PriNormal, "Pstate cache status = %s\n",
               m_CacheStatus == CACHE_DIRTY ? "dirty" : "clean");
        Printf(Tee::PriNormal, "All known perfpoints =");
        for (PerfPoints::const_iterator pp = m_PerfPoints.begin();
             pp != m_PerfPoints.end(); ++pp)
        {
            Printf(Tee::PriNormal, " %u.%s",
                   pp->PStateNum, SpecialPtToString(pp->SpecialPt));
        }
        Printf(Tee::PriNormal, "\n");
        return PerfPoint(ILWALID_PSTATE, NUM_GpcPerfMode);
    }
}

namespace
{
    struct ModeToRestore
    {
        UINT32 Display;
        UINT32 Width;
        UINT32 Height;
        UINT32 Depth;
        UINT32 RefreshRate;
        Display::FilterTaps FilterTaps;
        Display::ColorCompression ColorComp;
        UINT32 DdScalerMode;
    };
};

//------------------------------------------------------------------------------
RC Perf::SetPerfPoint(const PerfPoint &Setting)
{
    RC rc;

    UINT32 numPStates;
    CHECK_RC(GetNumPStates(&numPStates));
    if (!numPStates)
    {
        Printf(Tee::PriError,
               "PStates must be enabled to use SetPerfPoint\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(InnerSetPerfPoint(Setting));

    return rc;
}

RC Perf::InnerSetPerfPoint(const PerfPoint &Setting)
{
    RC rc;

    UINT64 gpcClkHz;
    CHECK_RC(GetGpcFreqOfPerfPoint(Setting, &gpcClkHz));

    // Build a vector of clock settings to program into the RM's pstate table.

    // Start with pstate defaults:
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> ClkInfo;
    vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> VoltInfo;
    FetchDefaultPStateTable(Setting.PStateNum, &ClkInfo, &VoltInfo);

    if (!(IsPState30Supported() && Setting.SpecialPt == GpcPerf_INTERSECT))
    {
        // Override domains tied to the PerfPoint's chosen gpcclk.
        CHECK_RC(GetRatiodClocks(Setting.PStateNum, gpcClkHz, &ClkInfo));
    }

    Perf::PStateLockType LockType = LockTypeOfPerfPoint(Setting);

    // Has -gpcclk intersect been set ?
    bool IsGpcClkSetToIntersect = IsClockRequestedAtIntersect(Setting,
                                                              Gpu::ClkGpc);

    // GpcClk can be set via -XX.intersect (OR) -gpcclk intersect
    bool gpcIsAtIntersect = (IsGpcClkSetToIntersect ||
                              IsGpcClkAtIntersect(Setting));

    // Merge in explicit clock domain overrides from the PerfPoint.
    for (ClkMap::const_iterator clkItr = Setting.Clks.begin();
         clkItr != Setting.Clks.end(); ++clkItr)
    {
        const ClkSetting & c = clkItr->second;
        const ClkDomainInfo & cdi = m_ClockDomains[c.Domain];

        if (!CanSetClockDomailwiaPState(c.Domain))
        {
            Printf(Tee::PriNormal, "cannot set %s domain with PState\n",
                       PerfUtil::ClkDomainToStr(c.Domain));
                return RC::UNSUPPORTED_FUNCTION;
        }

        if (0 == c.FreqHz)
            continue;

        if (VirtualLock == LockType)
        {
            // Decoupled clocks cannot be explicitly set when they are at their
            // intersect,(NOR) can clocks ratio'd to GPC clock when it's set to
            // intersect.
            if (((cdi.Type == DECOUPLED) && c.IsIntersect) ||
                ((c.Domain == Gpu::ClkGpc2 || c.Domain == Gpu::ClkGpc) && gpcIsAtIntersect) ||
                ((cdi.RatioDomain == Gpu::ClkGpc2 || cdi.RatioDomain == Gpu::ClkGpc) && (cdi.Type == RATIO) &&
                 gpcIsAtIntersect))
            {
                continue;
            }
        }

        // Check if clock domain isn't supported by RM, colwert in that case
        FLOAT32 freqRatio = 1.0f;
        Gpu::ClkDomain dom = c.Domain;
        if (!IsRmClockDomain(dom))
        {
            freqRatio = PerfUtil::GetModsToRmFreqRatio(dom);
            dom = PerfUtil::ColwertToOppositeClockDomain(dom);
        }

        const UINT32 RmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
        // find the Clk in ClkInfo (populated by GetRatiodClocks) to override:
        LW2080_CTRL_PERF_CLK_DOM_INFO *OvrrdClk;
        OvrrdClk = GetClkParamToOverride(RmDomain, &ClkInfo);
        OvrrdClk->freq = static_cast<LwU32>(c.FreqHz / 1000 * freqRatio);
        OvrrdClk->flags = 0;
        if (c.Flags & Perf::FORCE_PLL)
        {
            OvrrdClk->flags |= DRF_DEF(2080_CTRL_PERF_CLK_DOM_INFO,
                    _FLAGS, _FORCE_PLL, _ENABLE);
        }
        if (c.Flags & Perf::FORCE_BYPASS)
        {
            OvrrdClk->flags |= DRF_DEF(2080_CTRL_PERF_CLK_DOM_INFO,
                    _FLAGS, _FORCE_BYPASS, _ENABLE);
        }
        if (c.Flags & Perf::FORCE_NAFLL)
        {
            OvrrdClk->flags |= DRF_DEF(2080_CTRL_PERF_CLK_DOM_INFO,
                    _FLAGS, _FORCE_NAFLL, _ENABLE);
        }
    }

    for (UINT32 i = 0; i < Setting.Voltages.size(); i++)
    {
        const UINT32 RmDomain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Setting.Voltages[i].Domain);
        if (!CanSetVoltDomailwiaPState(Setting.Voltages[i].Domain))
        {
            Printf(Tee::PriNormal, "Voltage domain lwrrently not supported\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (0 == Setting.Voltages[i].VoltMv)
        {
            continue;
        }

        // Just don't touch voltage with pstate 2.0 for now... Once RM gives
        // us knobs to touch voltage, we'll worry about voltage restore later.
        if (m_PStateVersion >= PStateVersion::PSTATE_20)
        {
            continue;
        }
        LW2080_CTRL_PERF_VOLT_DOM_INFO NewVoltInfo = {0};
        NewVoltInfo.domain = RmDomain;
        NewVoltInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
        NewVoltInfo.data.logical.logicalVoltageuV =  Setting.Voltages[i].VoltMv * 1000;
        VoltInfo.push_back(NewVoltInfo);
    }

    bool dispClkWillBeDecoupled = gpcIsAtIntersect;
    std::vector<ModeToRestore> modesToRestore;
    DisplayIDs activeDisplays;
    Display *pDisplay = m_pSubdevice->GetParentDevice()->GetDisplay();
    if (!dispClkWillBeDecoupled &&
        (pDisplay != nullptr) &&
        (Xp::GetOperatingSystem() != Xp::OS_LINUXSIM) && // We lwrrently have no
        (Xp::GetOperatingSystem() != Xp::OS_WINSIM) &&   // way to stop VGA text
                                                         // mode scan out in sims
        (OK == pDisplay->GetScanningOutDisplays(&activeDisplays)))
    {
        for (UINT32 activeDisplayIdx = 0;
             activeDisplayIdx < activeDisplays.size();
             activeDisplayIdx++)
        {
            UINT32 displayToQuery = activeDisplays[activeDisplayIdx];

            ModeToRestore modeToRestore;
            if (OK != pDisplay->GetMode(
                    displayToQuery,
                    &modeToRestore.Width,
                    &modeToRestore.Height,
                    &modeToRestore.Depth,
                    &modeToRestore.RefreshRate,
                    &modeToRestore.FilterTaps,
                    &modeToRestore.ColorComp,
                    &modeToRestore.DdScalerMode))
            {
                continue;
            }

            UINT32 pixelClock = pDisplay->GetPixelClock(displayToQuery);

            const UINT32 rmDomain =
                PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkDisp);
            LW2080_CTRL_PERF_CLK_DOM_INFO *dispClk;
            dispClk = GetClkParamToOverride(rmDomain, &ClkInfo);
            if ((dispClk != nullptr) &&
                (1000*dispClk->freq < pixelClock))
            {
                UINT32 head;
                CHECK_RC(pDisplay->GetHead(displayToQuery, &head));
                CHECK_RC(pDisplay->DetachHead(head));
                modeToRestore.Display = displayToQuery;
                modesToRestore.push_back(modeToRestore);
            }
        }
    }

    CHECK_RC(MaybeUnlock(LockType));

    // Update RM's PState tables.
    CHECK_RC(SetPStateTableInternal(Setting.PStateNum, ClkInfo, VoltInfo));

    // This value will be sent to HW during SetIntersectPt or
    // ForcePStateInternal which call SetVoltTuningParams.
    m_VoltTuningOffsetsMv[VTUNE_PERFPOINT] = Setting.VoltTuningOffsetMv;

    // Set the pstate lock (soft/virtual/hard)
    CHECK_RC(SetPStateLock(Setting, LockType));

    m_LwrPerfPointGpcPerfMode = Setting.SpecialPt;
    m_LwrPerfPointPStateNum = Setting.PStateNum;
    m_LwrIntersectSettings = Setting.IntersectSettings;

    // Cache which clock domains were overriden to their intersect point
    UINT32 psIdx = 0;
    CHECK_RC(GetPStateIdx(Setting.PStateNum, &psIdx));
    for (ClkMap::const_iterator clkItr = Setting.Clks.begin();
         clkItr != Setting.Clks.end(); ++clkItr)
    {
        const ClkSetting & c = clkItr->second;

        // Check if clock domain isn't supported by RM, colwert in that case
        Gpu::ClkDomain dom = c.Domain;
        if (!IsRmClockDomain(dom))
        {
            dom = PerfUtil::ColwertToOppositeClockDomain(dom);
        }
        UINT32 RmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);

        m_CachedPStates[psIdx].IsClkIntersect[RmDomain] = c.IsIntersect;

        // Gpcclk can be at intersect via clock override, (OR) LocationStr
        if ((c.Domain == Gpu::ClkGpc2 || c.Domain == Gpu::ClkGpc) && !c.IsIntersect)
        {
            m_CachedPStates[psIdx].IsClkIntersect[RmDomain]
                                                = IsGpcClkAtIntersect(Setting);
        }
    }

    CHECK_RC(SetPerfPointPcieSettings(Setting));

    // Now that we have acquired the pstate locks, restore the pstate tables
    // for all pstates EXCEPT the current pstate
    CHECK_RC(RestorePStateTable(~PStateNumTo2080Bit(Setting.PStateNum),
                                AllDomains));

    for (UINT32 modeIdx = 0; modeIdx < modesToRestore.size(); modeIdx++)
    {
        ModeToRestore *modeToRestore = &modesToRestore[modeIdx];
        CHECK_RC(pDisplay->SetMode(
            modeToRestore->Display,
            modeToRestore->Width,
            modeToRestore->Height,
            modeToRestore->Depth,
            modeToRestore->RefreshRate,
            modeToRestore->FilterTaps,
            modeToRestore->ColorComp,
            modeToRestore->DdScalerMode));
    }
    return rc;
}

RC Perf::SetPerfPointPcieSettings(const PerfPoint& Setting) const
{
    RC rc;

    auto pGpuPcie = m_pSubdevice->GetInterface<Pcie>();
    if (Setting.LinkSpeed)
    {
        CHECK_RC(pGpuPcie->SetLinkSpeed(
                           static_cast<Pci::PcieLinkSpeed>(Setting.LinkSpeed)));
    }
    if (Setting.LinkWidth)
    {
        CHECK_RC(pGpuPcie->SetLinkWidth(Setting.LinkWidth));
    }

    return rc;
}

RC Perf::ComparePerfPoint(const PerfPoint &s1,
                          const PerfPoint &s2,
                          UINT32 PercentThreshold)
{
    RC rc;
    //
    // This was a feature that never worked consistently for tests.
    // Revisit this later on - need to ensure that Pwr capping is disabled.
    //
    return rc;
}

RC Perf::InjectPerfPoint(const InjectedPerfPoint &point)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetRegime(Gpu::ClkDomain dom, RegimeSetting regime)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetRegime(Gpu::ClkDomain dom, RegimeSetting* regime)
{
    return RC::UNSUPPORTED_FUNCTION;
}

Perf::RegimeSetting Perf::Regime2080CtrlBitToRegimeSetting(LwU32 rmRegime)
{
    switch (rmRegime)
    {
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR:
            return Perf::FIXED_FREQUENCY_REGIME;
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR:
            return Perf::VOLTAGE_REGIME;
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR:
            return Perf::FREQUENCY_REGIME;
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_ABOVE_NOISE_UNAWARE_VMIN:
            return Perf::VR_ABOVE_NOISE_UNAWARE_VMIN;
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN:
            return Perf::FFR_BELOW_DVCO_MIN;
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_WITH_CPM:
            return Perf::VR_WITH_CPM;
        default:
            Printf(Tee::PriError, "RM returned invalid regime %u\n",
                   rmRegime);
            return Perf::RegimeSetting_NUM;
    }
}

const char* Perf::RegimeToStr(RegimeSetting regime)
{
    switch (regime)
    {
        case FIXED_FREQUENCY_REGIME:
            return "fixed frequency";
        case VOLTAGE_REGIME:
            return "voltage";
        case FREQUENCY_REGIME:
            return "frequency";
        case DEFAULT_REGIME:
            return "default";
        case VR_ABOVE_NOISE_UNAWARE_VMIN:
            return "VR above noise unaware Vmin";
        case FFR_BELOW_DVCO_MIN:
            return "FFR below DVCO min";
        case VR_WITH_CPM:
            return "VR with CPM";
        default:
            return "unknown";
    }
}

//------------------------------------------------------------------------------
// local helper function for SetIntersectPt and ClearPerfLimits.
//
//  See bug 1381413 for dislwssion.  Also, see:
//  https://wiki.lwpu.com/engwiki/index.php/Resman/PState/PState_Limiting_Factors
namespace
{
    enum IntersectLimits
    {
        GpcClkMax,
        GpcClkMin,
        DispClkMax,
        DispClkMin,
        GpcVminCap,
        PstateMin,
        PstateMax,
        NUM_INTERSECT_LIMITS
    };
void InitPerfLimits(vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS> * pLimits)
{
    LW2080_CTRL_PERF_LIMIT_SET_STATUS ctor = {0};
    ctor.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;

    pLimits->clear();
    pLimits->resize(NUM_INTERSECT_LIMITS, ctor);

    // Limits are in lowest-priority to highest-priority order.
    // Higher priority perf-limits override lower-priority.

    (*pLimits)[GpcClkMax ].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_0_MAX;
    (*pLimits)[GpcClkMin ].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_0_MIN;
    (*pLimits)[DispClkMax].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_1_MAX;
    (*pLimits)[DispClkMin].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_1_MIN;
    (*pLimits)[GpcVminCap].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_2_MAX;

    // The ISMODEPOSSIBLE limit is set as a side-effect of display modeset.
    // It can raise pstate and dispclk.
    // The INTERNAL_CLIENT limits are higher priority than ISMODEPOSSIBLE.
    // We must use them for MAX's on pstate and dispclk.

    (*pLimits)[PstateMax  ].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_1_MAX;
    (*pLimits)[PstateMin  ].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_1_MIN;
}
} // anonymous

//------------------------------------------------------------------------------
RC Perf::SetIntersectPt(const PerfPoint &Setting)
{
    RC rc;
    UINT32 pstateNum = Setting.PStateNum;
    const UINT32 forcePStateExFlag = ForceFlag(m_ActivePStateLockType);

    UINT64 timeBefore = Xp::GetWallTimeUS();
    // First, restore default decoupled (and ratio'd) clocks of this pstate.
    CHECK_RC(RestorePStateTable(PStateNumTo2080Bit(pstateNum), OnlyDecoupled));
    Printf(m_VfSwitchTimePriority,
           "SetPStateLock (intersect): RestorePStateTable took %lluus\n",
           Xp::GetWallTimeUS() - timeBefore);

    // Update our voltage offset for this pstate, if any.
    m_VoltTuningOffsetsMv[VTUNE_PSTATE] =
        m_PStateVoltTuningOffsetsMv[pstateNum];
    CHECK_RC(SetVoltTuningParams(writeIfChanged));

    // Intersect points for multiple decoupled clocks:
    // Perf point where *any* one of the decoupled clocks are at intersect point
    // Possible cases: gpcclk = int, dispclk = nom/override (supported)
    //                 dispclk = int, gpcclk = nom/override (supported)
    //                 gpcclk = int, dispclk = int (supported)

    // Apply RM perf limits for all the decoupled clock domains supported in
    // the current PState implementation
    CHECK_RC(ApplyRmPerfLimits(Setting));

    m_PStateChangeCounter.AddExpectedChange(pstateNum);

    m_ActivePStateLockType = VirtualLock;

    timeBefore = Xp::GetWallTimeUS();
    // Now that we've set the PERF_LIMIT type lock, release any
    // FORCE_PSTATE_EX type locks we might be holding.
    rc = ClearForcePStateEx(forcePStateExFlag);
    Printf(m_VfSwitchTimePriority,
           "SetPStateLock (intersect): ClearForcePStateEx took %lluus\n",
           Xp::GetWallTimeUS() - timeBefore);

    return rc;
}

RC Perf::ApplyRmPerfLimits(const PerfPoint &Setting)
{
    RC rc;
    LW2080_CTRL_PERF_VOLT_DOM_INFO voltInfo = {0};
    voltInfo.domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::CoreV);

    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS params = {0};
    params.pstate = PStateNumTo2080Bit(Setting.PStateNum);
    params.perfVoltDomInfoListSize = 1;
    params.perfClkDomInfoListSize  = 0;
    params.perfVoltDomInfoList     = LW_PTR_TO_LwP64(&voltInfo);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                           LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                                           &params, sizeof(params)));

    vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS> limits;
    InitPerfLimits(&limits);

    // How do we decide what frequency to lock to ?
    // 1. Figure out which of gpcclk / dispclk are @ intersect
    // 2. Figure out if the non-intersect decoupled clock has any overrides
    //    a. If overrides are present apply them, even if we are @ intersect
    //    b. else, default to the nominal frequency at the current pstate
    // 3. Ensure the intersect decoupled clock has the perf limits set

    bool IsDispVoltCappingRequested = IsClockRequestedAtIntersect(Setting,
                                                                  Gpu::ClkDisp);
    bool IsGpcVoltCappingRequested = (IsClockRequestedAtIntersect(Setting,
                                                                Gpu::ClkGpc) ||
                                      IsGpcClkAtIntersect(Setting));

    // Move GPC towards max/nominal/user-specified frequency.
    UINT64 GpcClkFreqHz = 0;
    CHECK_RC(GetDecoupledClockAtPerfPtHz(Setting,
                                         Gpu::ClkGpc,
                                         &GpcClkFreqHz));

    limits[GpcClkMin].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;

    // RM expects gpc2clk, so multiply by 2
    limits[GpcClkMin].input.data.freq.freqKHz = static_cast<LwU32>(GpcClkFreqHz / 1000 * 2);
    limits[GpcClkMin].input.data.freq.domain = LW2080_CTRL_CLK_DOMAIN_GPC2CLK;

    limits[GpcClkMax].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;

    // RM expects gpc2clk, so multiply by 2
    limits[GpcClkMax].input.data.freq.freqKHz = static_cast<LwU32>(GpcClkFreqHz / 1000 * 2);
    limits[GpcClkMax].input.data.freq.domain = LW2080_CTRL_CLK_DOMAIN_GPC2CLK;

    // IF
    // * dispclk is moving to intersect, (OR)
    // * gpcclk is moving to intersect (OR)
    // MODS needs to set voltage capping for gpcclk
    if (IsDispVoltCappingRequested ||
        IsGpcVoltCappingRequested)
    {
        // Limit GPC to max allowed at pstate min voltage.
        limits[GpcVminCap].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE;
        limits[GpcVminCap].input.data.volt.info = voltInfo;
        limits[GpcVminCap].input.data.volt.clkDomain = LW2080_CTRL_CLK_DOMAIN_GPC2CLK;
    }

    if (DECOUPLED == ClkDomainType(Gpu::ClkDisp))
    {
        // Move DISP towards max/nominal/user-specified frequency.
        UINT64 DispClkFreqHz = 0;
        CHECK_RC(GetDecoupledClockAtPerfPtHz(Setting,
                                             Gpu::ClkDisp,
                                             &DispClkFreqHz));

        limits[DispClkMin].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;
        limits[DispClkMin].input.data.freq.freqKHz =
                                       static_cast<LwU32>(DispClkFreqHz / 1000);
        limits[DispClkMin].input.data.freq.domain =
                                                 LW2080_CTRL_CLK_DOMAIN_DISPCLK;

        limits[DispClkMax].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;
        limits[DispClkMax].input.data.freq.freqKHz =
                                       static_cast<LwU32>(DispClkFreqHz / 1000);
        limits[DispClkMax].input.data.freq.domain =
                                                 LW2080_CTRL_CLK_DOMAIN_DISPCLK;

        // IF
        // * dispclk is moving to intersect, (OR)
        // * gpcclk is moving to intersect (OR)
        if (IsDispVoltCappingRequested ||
            IsGpcVoltCappingRequested)
        {
            m_DispVminCapActive = true;
            CHECK_RC(ApplyDispVminCap(Setting.PStateNum));
        }
    }

    // Lock pstate.
    limits[PstateMin].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
    limits[PstateMin].input.data.pstate.pstateId = PStateNumTo2080Bit(Setting.PStateNum);
    limits[PstateMax].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
    limits[PstateMax].input.data.pstate.pstateId = PStateNumTo2080Bit(Setting.PStateNum);

    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS setLimitsParams = {0};
    setLimitsParams.numLimits = static_cast<LwU32>(limits.size());
    setLimitsParams.pLimits   = LW_PTR_TO_LwP64(&limits[0]);

    UINT64 timeBefore = Xp::GetWallTimeUS();
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                 m_pSubdevice, LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
                 &setLimitsParams, sizeof(setLimitsParams)));
    Printf(m_VfSwitchTimePriority,
           "SetPStateLock (intersect): LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS took %lluus\n",
           Xp::GetWallTimeUS() - timeBefore);
    return rc;
}

RC Perf::SetRequiredDispClk(UINT32 DispClkHz, bool AllowAbovePstateVmin)
{
    if (DispClkHz == 0)
    {
        m_RequiredDispClkHz = DispClkHz;
        return ApplyDispVminCap(m_LwrPerfPointPStateNum);
    }

    if (!m_DispVminCapActive)
        return OK;

    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_PERF_VOLT_DOM_INFO dispClkVoltInfo = {0};
    dispClkVoltInfo.domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::CoreV);

    LW2080_CTRL_PERF_LOOK_UP_VOLTAGE_PARAMS lookupVoltage = {0};
    lookupVoltage.pstate = PStateNumTo2080Bit(m_LwrPerfPointPStateNum);
    lookupVoltage.domain = LW2080_CTRL_CLK_DOMAIN_DISPCLK;
    lookupVoltage.freq = static_cast<LwU32>(DispClkHz / 1000);
    lookupVoltage.perfVoltDomInfoListSize = 1;
    lookupVoltage.perfVoltDomInfoList     = LW_PTR_TO_LwP64(&dispClkVoltInfo);
    CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice,
                                            LW2080_CTRL_CMD_PERF_LOOK_UP_VOLTAGE,
                                            &lookupVoltage, sizeof(lookupVoltage)));

    UINT32 lwrrTargetVoltageUv;
    CHECK_RC(GetVoltageAtPStateNoCache(
        m_LwrPerfPointPStateNum,
        Gpu::CoreV,
        &lwrrTargetVoltageUv));

    if (dispClkVoltInfo.lwrrTargetVoltageuV > lwrrTargetVoltageUv)
    {
        Printf(Tee::PriLow,
            "Disp clk target voltage = %d uV above pstate Vmin = %d uV - ",
            dispClkVoltInfo.lwrrTargetVoltageuV, lwrrTargetVoltageUv);
        if (AllowAbovePstateVmin)
        {
            Printf(Tee::PriLow, "allowed\n");
        }
        else
        {
            Printf(Tee::PriLow, "returnig MODE_IS_NOT_POSSIBLE\n");
            return RC::MODE_IS_NOT_POSSIBLE;
        }
    }
    m_RequiredDispClkHz = DispClkHz;

    return ApplyDispVminCap(m_LwrPerfPointPStateNum);
}

RC Perf::ApplyDispVminCap(UINT32 PStateNum)
{
    if (!m_DispVminCapActive)
        return OK;

    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_PERF_VOLT_DOM_INFO voltInfo = {0};
    voltInfo.domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::CoreV);

    if (m_RequiredDispClkHz == 0)
    {
        LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS pstateInfo = {0};
        pstateInfo.pstate = PStateNumTo2080Bit(PStateNum);
        pstateInfo.perfVoltDomInfoListSize = 1;
        pstateInfo.perfClkDomInfoListSize  = 0;
        pstateInfo.perfVoltDomInfoList     = LW_PTR_TO_LwP64(&voltInfo);
        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice,
                                                LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                                                &pstateInfo, sizeof(pstateInfo)));
    }
    else
    {
        LW2080_CTRL_PERF_LOOK_UP_VOLTAGE_PARAMS lookupVoltage = {0};
        lookupVoltage.pstate = PStateNumTo2080Bit(PStateNum);
        lookupVoltage.domain = LW2080_CTRL_CLK_DOMAIN_DISPCLK;
        lookupVoltage.freq = static_cast<LwU32>(m_RequiredDispClkHz / 1000);
        lookupVoltage.perfVoltDomInfoListSize = 1;
        lookupVoltage.perfVoltDomInfoList     = LW_PTR_TO_LwP64(&voltInfo);
        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice,
                                               LW2080_CTRL_CMD_PERF_LOOK_UP_VOLTAGE,
                                               &lookupVoltage, sizeof(lookupVoltage)));
    }

    LW2080_CTRL_PERF_LIMIT_SET_STATUS dispVminCapLimit = {0};

    dispVminCapLimit.limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_0_MAX;
    dispVminCapLimit.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE;
    dispVminCapLimit.input.data.volt.info = voltInfo;
    dispVminCapLimit.input.data.volt.clkDomain = LW2080_CTRL_CLK_DOMAIN_DISPCLK;

    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS setLimits = {0};
    setLimits.numLimits = 1;
    setLimits.pLimits   = LW_PTR_TO_LwP64(&dispVminCapLimit);
    CHECK_RC(pLwRm->ControlBySubdevice(
                 m_pSubdevice, LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
                 &setLimits, sizeof(setLimits)));

    return OK;
}

RC Perf::ClearPerfLimits()
{
    m_DispVminCapActive = false;
    RC rc;
    // Get limits used by SetIntersectPt, with default "DISABLED" type.
    vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS> limits;
    InitPerfLimits(&limits);
    LW2080_CTRL_PERF_LIMIT_SET_STATUS dispVminCaplimit;
    dispVminCaplimit.limitId  = LW2080_CTRL_PERF_LIMIT_CLIENT_LOOSE_0_MAX;
    dispVminCaplimit.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;
    limits.push_back(dispVminCaplimit);

    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS setLimitsParams = {0};
    setLimitsParams.numLimits = static_cast<LwU32>(limits.size());
    setLimitsParams.pLimits   = LW_PTR_TO_LwP64(&limits[0]);

    rc = LwRmPtr()->ControlBySubdevice(
        m_pSubdevice, LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
        &setLimitsParams, sizeof(setLimitsParams));

    // Update the "intersect" points of each clock domain
    // since the RM perf limits have been removed at this point
    UINT32 pstateIdx = 0;
    UINT32 lwrPState;
    CHECK_RC(GetLwrrentPState(&lwrPState));
    CHECK_RC(GetPStateIdx(lwrPState, &pstateIdx));
    map<UINT32, ClkRange>::const_iterator ClkIter =
                                     m_CachedPStates[pstateIdx].ClkVals.begin();
    for (; ClkIter != m_CachedPStates[pstateIdx].ClkVals.end(); ClkIter++)
    {
        // Assume no clock domains are at intersect unless the user requests so
        // via -XXXclk intersect, (OR) XX.intersect
        m_CachedPStates[pstateIdx].IsClkIntersect[ClkIter->first] = false;
    }

    if (m_ActivePStateLockType == VirtualLock)
        m_ActivePStateLockType = NotLocked;

    m_PStateNumCache = CACHE_DIRTY;
    return rc;
}

RC Perf::PrintPerfLimits() const
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::PrintRmPerfLimits(Tee::Priority pri) const
{
    RC rc;
    vector<LW2080_CTRL_PERF_LIMIT_INFO> infos(LW2080_CTRL_PERF_MAX_LIMITS);
    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(LW2080_CTRL_PERF_MAX_LIMITS);
    for (UINT32 i = 0; i < infos.size(); i++)
    {
        infos[i].limitId = i;
        statuses[i].limitId = i;
    }
    CHECK_RC(PerfUtil::GetRmPerfLimitsInfo(infos, m_pSubdevice));
    CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, m_pSubdevice));

    Printf(pri, "All RM Perf Limits:\n");
    for (size_t i = 0; i < infos.size(); i++)
    {
        PerfUtil::PrintRmPerfLimit(pri, infos[i], statuses[i], m_pSubdevice);
    }
    return rc;
}

RC Perf::GetDumpLimitsOnPerfSwitch(bool * p) const
{
    *p = (PerfUtil::s_LimitVerbosity != PerfUtil::NO_LIMITS);
    return OK;
}

RC Perf::SetDumpLimitsOnPerfSwitch(bool b) const
{
    if (b)
        PerfUtil::s_LimitVerbosity = PerfUtil::MODS_LIMITS | PerfUtil::ONLY_ENABLED_LIMITS;
    else
        PerfUtil::s_LimitVerbosity = PerfUtil::NO_LIMITS;
    return OK;
}

RC Perf::GetLimitVerbosity(UINT32* pLimitVerbosity)
{
    *pLimitVerbosity = PerfUtil::s_LimitVerbosity;
    return OK;
}

RC Perf::SetLimitVerbosity(UINT32 limitVerbosity)
{
    PerfUtil::s_LimitVerbosity = limitVerbosity;
    return OK;
}

//------------------------------------------------------------------------------
struct GpcPerfMode_info
{
    Perf::GpcPerfMode   mode;
    const char *         name;
    Perf::PStateLockType pstateLockType;
    LwU32                rmVPStateId;    // This is a 1-bit mask
};

// Keep this array in order so that s_GpcPerfMode_info[x].mode == x!

static const GpcPerfMode_info s_GpcPerfMode_info[Perf::NUM_GpcPerfMode] =
{
    {  Perf::GpcPerf_EXPLICIT,  ENCJSENT("explicit"),   Perf::HardLock, 0 }
    ,{ Perf::GpcPerf_MAX,       ENCJSENT("max"),        Perf::HardLock, 0 }
    ,{ Perf::GpcPerf_MID,       ENCJSENT("mid"),        Perf::HardLock, 0 }
    ,{ Perf::GpcPerf_NOM,       ENCJSENT("nom"),        Perf::HardLock, 0 }
    ,{ Perf::GpcPerf_MIN,       ENCJSENT("min"),        Perf::HardLock, 0 }
    ,{ Perf::GpcPerf_INTERSECT, ENCJSENT("intersect"),  Perf::VirtualLock, 0 }
    ,{ Perf::GpcPerf_TDP,       ENCJSENT("tdp"),        Perf::HardLock,
       LW2080_CTRL_PERF_VIRTUAL_PSTATES_RATED_TDP }
    ,{ Perf::GpcPerf_MIDPOINT,  ENCJSENT("midpoint"),   Perf::HardLock,
       LW2080_CTRL_PERF_VIRTUAL_PSTATES_MID_POINT }
    ,{ Perf::GpcPerf_TURBO,     ENCJSENT("turbo"),      Perf::HardLock,
       LW2080_CTRL_PERF_VIRTUAL_PSTATES_TURBO_BOOST }
};

LwU32 Perf::GetVPStateId(Perf::GpcPerfMode m) const
{
    // No user data here, invalid GpcPerfMode is a SW bug.
    MASSERT(static_cast<size_t>(m) < NUMELEMS(s_GpcPerfMode_info));
    // Array must be in order.
    MASSERT(m == s_GpcPerfMode_info[m].mode);

    return s_GpcPerfMode_info[m].rmVPStateId;
}

bool Perf::HasVirtualPState(Perf::GpcPerfMode m) const
{
    // Here we assume OneTimeInit has been called so we can be const.
    // We will mistakenly return false if called before OneTimeInit.
    // Need to rethink how Perf is initialized to fix this better.

    return (0 != (m_VPStatesMask & GetVPStateId(m)));
}

RC Perf::GetVPStateInfo
(
    LwU32 VPStateId,
    LwU32 VPStateIdx,
    LW2080_CTRL_PERF_GET_VIRTUAL_PSTATE_INFO_PARAMS * pParams,
    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> * pClkDomInfos
)
{
    MASSERT(pParams);
    MASSERT(pClkDomInfos);

    memset(pParams, 0, sizeof(*pParams));
    pParams->virtualPstate = VPStateId;
    pParams->index = VPStateIdx;

    AssignDomainsToTypeWithDomainBuffer(m_ClkDomainsMask, pClkDomInfos);

    pParams->perfClkDomInfoListSize = Utility::CountBits(m_ClkDomainsMask);
    pParams->perfClkDomInfoList = LW_PTR_TO_LwP64(&((*pClkDomInfos)[0]));

    RC rc;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                 m_pSubdevice,
                 LW2080_CTRL_CMD_PERF_GET_VIRTUAL_PSTATE_INFO,
                 pParams, sizeof(*pParams)));
    return rc;
}

Gpu::ClkDomain Perf::GetDomainFromClockProgrammingOrder(UINT32 idx) const
{
    return Gpu::ClkUnkUnsp;
}

Perf::PStateLockType Perf::LockTypeOfPerfPoint(const PerfPoint &Setting) const
{
    GpcPerfMode specialPt = Setting.SpecialPt;
    MASSERT((size_t)specialPt < NUMELEMS(s_GpcPerfMode_info));
    MASSERT(specialPt == s_GpcPerfMode_info[specialPt].mode);

    for (ClkMap::const_iterator clkItr = Setting.Clks.begin();
         clkItr != Setting.Clks.end(); ++clkItr)
    {
        // Is atlest one domain overridden in the PerfPoint at intersect ?
        if (clkItr->second.IsIntersect)
        {
            // Use a virtual lock for such perfpoints
            return VirtualLock;
        }
    }

    if (HardLock == s_GpcPerfMode_info[specialPt].pstateLockType)
    {
        if (m_ForcedPStateLockType == HardLock)
            return HardLock;
        else
            return SoftLock;
    }
    return s_GpcPerfMode_info[specialPt].pstateLockType;
}

// Helper API to determine if user has requested clock domain to be intersect
bool Perf::IsClockRequestedAtIntersect
(
    const PerfPoint &Setting,
    Gpu::ClkDomain Domain
) const
{
    ClkMap::const_iterator clkItr = Setting.Clks.find(Domain);
    if (clkItr != Setting.Clks.end())
    {
        return clkItr->second.IsIntersect;
    }

    // Domain does not exist in Setting, so it can't be at intersect
    return false;
}

// Helper API to determine if clock domain is already at intersect
bool Perf::IsClockAtIntersect(UINT32 PStateNum, Gpu::ClkDomain Domain)
{
    UINT32 psIdx = 0;
    if (OK != GetPStateIdx(PStateNum, &psIdx))
        return false;
    UINT32 RmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Domain);
    return m_CachedPStates[psIdx].IsClkIntersect[RmDomain];
}

bool Perf::IsGpcClkAtIntersect(const PerfPoint &Setting) const
{
    return (s_GpcPerfMode_info[Setting.SpecialPt].pstateLockType == VirtualLock);
}

UINT32 Perf::StringToSpecialPt(const string & LocationStr)
{
    const char * p = LocationStr.c_str();
    for (size_t i = 0; i < NUMELEMS(s_GpcPerfMode_info); i++)
    {
        if (0 == stricmp(p, s_GpcPerfMode_info[i].name))
            return (UINT32) s_GpcPerfMode_info[i].mode;
    }

    // Check if LocationStr starts with "intersect"
    if (strncmp(p, "intersect", strlen("intersect")) == 0)
        return Perf::GpcPerf_INTERSECT;

    Printf(Tee::PriNormal, "unknown location str\n");
    return GpcPerf_EXPLICIT;
}

const char* Perf::SpecialPtToString(UINT32 Point)
{
    if (Point >= NUMELEMS(s_GpcPerfMode_info))
    {
        Point = GpcPerf_EXPLICIT;
        Printf(Tee::PriNormal, "unknown point\n");
    }
    return s_GpcPerfMode_info[Point].name;
}

//------------------------------------------------------------------------------
bool Perf::IsDIPStateSupported(UINT32 DIPState)
{
    return m_DIPStates.count(DIPState) != 0;
}

//------------------------------------------------------------------------------
RC Perf::SetDIPStates(vector <UINT32> PStates)
{
    m_DIPStates =  set <UINT32> (PStates.begin(), PStates.end());
    return OK;
}

//------------------------------------------------------------------------------
RC Perf::GetDIPStates(vector<UINT32> * pPStates)
{
    MASSERT(pPStates != 0);
    pPStates->assign(m_DIPStates.begin(), m_DIPStates.end());
    return OK;
}

//-----------------------------------------------------------------------------
RC Perf::ClearVoltTuningOffsets()
{
    StickyRC rc;
    for(unsigned i=0; i<NUMELEMS(m_VoltTuningOffsetsMv); i++)
    {
        m_VoltTuningOffsetsMv[i] = 0;
    }

    if ((UNKNOWN_VOLTAGE_OFFSET !=
                                m_ArbiterRequestParams.globalVoltageOffsetuV) ||
        (m_VminFloorMode != VminFloorModeDisabled))
    {
        memset(&m_ArbiterRequestParams, 0, sizeof(m_ArbiterRequestParams));
        m_ArbiterRequestParams.voltReq.type =
                              LW2080_CTRL_CMD_PMGR_VOLTAGE_REQUEST_TYPE_DISABLE;
        rc = SetVoltTuningParams(writeUnconditionally);

        m_ArbiterRequestParams.globalVoltageOffsetuV = UNKNOWN_VOLTAGE_OFFSET;
        m_VminFloorMode = VminFloorModeDisabled;
    }

    return rc;
}

//-----------------------------------------------------------------------------
// External API to set clocks via pstate tables in RM
//-----------------------------------------------------------------------------
RC Perf::SetClockDomailwiaPState
(
    Gpu::ClkDomain clkDomain,
    UINT64 targetHz,
    UINT32 flags
)
{
    RC rc;
    Perf::PerfPoint Setting;
    CHECK_RC(GetLwrrentPerfPoint(&Setting));

    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> PerClkInfo(1);
    memset(&PerClkInfo[0], 0, sizeof(PerClkInfo[0]));
    PerClkInfo[0].domain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkDomain);
    PerClkInfo[0].freq = static_cast<LwU32>(targetHz / 1000);
    PerClkInfo[0].flags = 0;

    if (flags & FORCE_PLL)
    {
            PerClkInfo[0].flags |= DRF_DEF(2080_CTRL_PERF_CLK_DOM_INFO,
                                           _FLAGS, _FORCE_PLL, _ENABLE);
    }
    if (flags & FORCE_BYPASS)
    {
            PerClkInfo[0].flags |= DRF_DEF(2080_CTRL_PERF_CLK_DOM_INFO,
                                           _FLAGS, _FORCE_BYPASS, _ENABLE);
    }
    if (flags & FORCE_NAFLL)
    {
            PerClkInfo[0].flags |= DRF_DEF(2080_CTRL_PERF_CLK_DOM_INFO,
                                           _FLAGS, _FORCE_NAFLL, _ENABLE);
    }

    vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> Dummy;
    CHECK_RC(SetPStateTable(Setting.PStateNum, PerClkInfo,Dummy));

    const FreqCtrlType t = ClkDomainType(clkDomain);
    if (VirtualLock == LockTypeOfPerfPoint(Setting) && (t == DECOUPLED))
    {
        // For intersect points, re-apply the perf limits for decoupled clks
        Setting.Clks[clkDomain].IsIntersect = false;
        Setting.Clks[clkDomain].FreqHz = targetHz;
        Setting.Clks[clkDomain].Flags = flags;
        CHECK_RC(ApplyRmPerfLimits(Setting));
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Helper API to set the pstate locks for a given perf pt
//-----------------------------------------------------------------------------
RC Perf::SetPStateLock(const PerfPoint &Setting, PStateLockType Lock)
{
    RC rc;
    if (!HasPStates())
    {
        Printf(Tee::PriError,
               "Cannot set pstate locks, with pstates disabled !\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (m_UsePStateCallbacks)
    {
        CallbackArguments args;
        args.Push(m_pSubdevice->GetGpuInst());
        args.Push(Setting.PStateNum);
        CHECK_RC(m_Callbacks[PRE_PSTATE].Fire(Callbacks::STOP_ON_ERROR, args));
    }
    m_PStateChangeCounter.AddExpectedChange(Setting.PStateNum);

    CHECK_RC(InnerSetPStateLock(Setting, Lock));

    if (m_UsePStateCallbacks)
    {
        CallbackArguments args;
        args.Push(m_pSubdevice->GetGpuInst());
        args.Push(Setting.PStateNum);
        CHECK_RC(m_Callbacks[POST_PSTATE].Fire(Callbacks::STOP_ON_ERROR, args));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Helper API to determine what frequency must be requested for a decoupled
// clock when setting perf limits in RM
// Decoupled clock frequencies can be set in 3 ways:
// 1) If the domain is @intersect, we set to max frequencey
// 2) If the domain is NOT @intersect, but "another" decoupled domain is
//   @intersect, we default to nominal frequency
// 3) If the domain is overriden by user to a given frequency, we set that value
//-----------------------------------------------------------------------------
RC Perf::GetDecoupledClockAtPerfPtHz
(
    const PerfPoint &Setting,
    Gpu::ClkDomain Domain,
    UINT64 *pFrequency
)
{
    RC rc;
    MASSERT(Domain != Gpu::ClkUnkUnsp);
    const ClkDomainInfo & cdi = m_ClockDomains[Domain];

    if ((cdi.Type != DECOUPLED))
    {
        Printf(Tee::PriError, "Clock domain is not decoupled!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Is this the clock @ intersect, for the given intersect perf point ?
    bool IsClockAtIntersect = false;
    UINT64 OverrideFreqHz = 0;
    ClkMap::const_iterator clkItr = Setting.Clks.find(Domain);
    if (clkItr != Setting.Clks.end())
    {
        const ClkSetting& c = clkItr->second;
        IsClockAtIntersect = c.IsIntersect;
        OverrideFreqHz = c.FreqHz;
    }

    // gpcclk @ intersect == XX.intersect || -gpcclk intersect
    if (!IsClockAtIntersect && (Gpu::ClkGpc2 == Domain || Gpu::ClkGpc == Domain))
    {
        IsClockAtIntersect = IsGpcClkAtIntersect(Setting);

        // Gpcclk is special because it can be non-intersect, but
        // overriden based on the current per-point being tested
        if (!IsClockAtIntersect && (OverrideFreqHz == 0))
            CHECK_RC(GetGpcFreqOfPerfPoint(Setting, &OverrideFreqHz));
    }

    if (IsClockAtIntersect)
    {
       ClkRange minMaxClk;
       CHECK_RC(GetClockRange(Setting.PStateNum, Gpu::ClkGpc, &minMaxClk));
       *pFrequency = minMaxClk.MaxKHz * 1000ULL;
    }
    else if (OverrideFreqHz)
    {
        *pFrequency = OverrideFreqHz;
    }
    else
    {
        // By default return the nominal frequency
        // for non-intersect, non-overriden decoupled clocks
        CHECK_RC(GetDefaultClockAtPState(Setting.PStateNum,
                                         Domain,
                                         &OverrideFreqHz));
        *pFrequency = OverrideFreqHz;
    }

    return OK;
}

//-----------------------------------------------------------------------------
/* static */ RC Perf::PrintClockInfo(const PerfPoint *point, Tee::Priority pri)
{
    for (ClkMap::const_iterator clkItr = point->Clks.begin();
         clkItr != point->Clks.end(); ++clkItr)
    {
        const char *clkStr = PerfUtil::ClkDomainToStr(clkItr->first);
        double clkMHz = clkItr->second.FreqHz / 1000000.0;

        // The longest clock name, "ClkGpuCache", is 11 characters, so justify
        // clock names by 11 spaces. MHz may be in the thousands (4 spaces),
        // plus a decimal (1 space), plus 6 decimal places (6 spaces), so
        // justify MHz by 11 spaces as well.
        Printf(pri, "\t%-11s = %11f MHz\n", clkStr, clkMHz);
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC PStateOwner::ClaimPStates(GpuSubdevice* pSubdev)
{
    if (m_pPStateOwnerRefCnt)
    {
        m_pPStateOwnerRefCnt++;
        return OK;
    }

    m_pGpuSubdev = pSubdev;

    Perf* const pPerf = pSubdev->GetPerf();
    if (!pPerf)
    {
        Printf(Tee::PriError, "Perf object not found!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (pPerf->IsOwned())
    {
        return RC::RESOURCE_IN_USE;
    }

    pPerf->SetPStateOwner(true);
    m_pPStateOwnerRefCnt = 1;
    return OK;
}

void PStateOwner::ReleasePStates()
{
    if (m_pPStateOwnerRefCnt)
    {
        if (m_pPStateOwnerRefCnt > 1)
        {
            m_pPStateOwnerRefCnt--;
        }
        else
        {
            Perf* const pPerf = m_pGpuSubdev->GetPerf();
            if (pPerf)
            {
                pPerf->SetPStateOwner(false);
            }
            m_pPStateOwnerRefCnt = 0;
            m_pGpuSubdev = nullptr;
        }
    }
}

//-----------------------------------------------------------------------------
namespace
{
    void PStateChangeNotifyHandler(void* pvGpuSubdevice)
    {
        GpuSubdevice * pGpuSubdevice =
            reinterpret_cast<GpuSubdevice*>(pvGpuSubdevice);
        Perf * pPerf = pGpuSubdevice->GetPerf();

        UINT32 lwrPStateNum;
        /* ignore returned rc */
        pPerf->GetLwrrentPStateNoCache(&lwrPStateNum);

        pPerf->GetPStateChangeCounter().NotifyHandler(lwrPStateNum);
    }

    void ClockChangeNotifyHandler(void* pvGpuSubdevice)
    {
        GpuSubdevice * pGpuSubdevice =
            reinterpret_cast<GpuSubdevice*>(pvGpuSubdevice);
        Perf * pPerf = pGpuSubdevice->GetPerf();

        UINT32 lwrrPStateNum;
        UINT64 perfTableClockHz;
        pPerf->GetLwrrentPStateNoCache(&lwrrPStateNum);
        pPerf->GetClockAtPStateNoCache(lwrrPStateNum, Gpu::ClkGpc, &perfTableClockHz);

        UINT64 actualPllHz, targetPllHz, slowedHz;
        Gpu::ClkSrc src;
        pGpuSubdevice->GetClock(Gpu::ClkGpc, &actualPllHz,
                                &targetPllHz, &slowedHz, &src);

        pPerf->GetClockChangeCounter().NotifyHandler(static_cast<UINT32>(perfTableClockHz/1000),
                                                     static_cast<UINT32>(targetPllHz / 1000),
                                                     static_cast<UINT32>(slowedHz / 1000));
    }
}

RC PerfChangeCounter::InitializeCounter()
{
    RC rc;
    m_AnyRequested = false;
    m_NumErrors = 0;
    m_ExpectedNextPerf = 0;
    m_ExpectedLwrPerf = 0;
    return rc;
}

RC PerfChangeCounter::ShutDown
(
    bool doOneLastErrorCheck
)
{
    RC rc;
    CHECK_RC(GpuErrCounter::ShutDown(doOneLastErrorCheck));
    return rc;
}

void PerfChangeCounter::AddExpectedChange
(
    const UINT32 expected
)
{
    if (GetIsHooked())
    {
        Tasker::MutexHolder mh(GetMutex());
        m_ExpectedNextPerf = expected;
        m_AnyRequested = true;
    }
}

RC PerfChangeCounter::HookToRM
(
    const UINT32 eventType,
    void (*notifyHandler)(void*)
)
{
    RC rc;
    if (!GetIsHooked())
    {
        CHECK_RC(GetGpuSubdevice()->HookResmanEvent(
                    eventType,
                    notifyHandler,
                    GetGpuSubdevice(),
                    LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                    GpuSubdevice::NOTIFIER_MEMORY_DISABLED));
        m_IsHooked = true;
    }
    return rc;
}

RC PerfChangeCounter::UnhookFromRM
(
    const UINT32 eventType
)
{
    RC rc;
    if (GetIsHooked())
    {
        CHECK_RC(GetGpuSubdevice()->UnhookResmanEvent(eventType));
        m_IsHooked = false;
    }
    return rc;
}

RC PerfChangeCounter::GetChangeReasons
(
    ReasonsType* reasons
) const
{
    RC rc;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetGpuSubdevice(),
        LW2080_CTRL_CMD_PERF_GET_PERF_DECREASE_INFO,
        reasons,
        sizeof(*reasons)));

    return rc;
}

void PerfChangeCounter::PrintChangeReasons
(
    const Tee::Priority pri,
    ReasonsType* reasons
) const
{
    bool haveReason = false;
    if (reasons->thermalMask)
    {
        Printf(pri, "\tthermal slowdown for:");
        for (UINT32 m = 1; m <= reasons->thermalMask; m = m<<1)
        {
            if (m & reasons->thermalMask)
            {
                Printf(pri, " \"%s\"",
                       CODE_LOOKUP(m, SlowdownReasons_ToStr));
                haveReason = true;
            }
        }
        Printf(pri, "\n");
    }
    if (reasons->pstateMask)
    {
        Printf(pri, "\tpstate cap for:");
        for (UINT32 m = 1; m <= reasons->pstateMask; m = m<<1)
        {
            if (m & reasons->pstateMask)
            {
                Printf(pri, " \"%s\"",
                       CODE_LOOKUP(m, SlowdownReasons_ToStr));
                haveReason = true;
            }
        }
        Printf(pri, "\n");
    }
    if (!haveReason)
    {
        Printf(pri, "\tRM reports no reason for this perf change.\n");
    }
}

RC ClockChangeCounter::InitializeCounter()
{
    RC rc;
    CHECK_RC(PerfChangeCounter::InitializeCounter());
    CHECK_RC(ErrCounter::Initialize("ClockChange", 1, 0,
                                    nullptr, MODS_TEST, CLOCKS_CHANGE_PRI));
    CHECK_RC(HookToRM(LW2080_NOTIFIERS_CLOCKS_CHANGE,
                      ClockChangeNotifyHandler));
    return rc;
}

RC ClockChangeCounter::ShutDown
(
    bool doOneLastErrorCheck
)
{
    RC rc;
    CHECK_RC(UnhookFromRM(LW2080_NOTIFIERS_CLOCKS_CHANGE));
    CHECK_RC(PerfChangeCounter::ShutDown(doOneLastErrorCheck));
    return rc;
}

void ClockChangeCounter::NotifyHandler
(
    const UINT32 perfTableGpcKhz,
    const UINT32 targetGpcKhz,
    const UINT32 actualGpcKhz
)
{
    // perfTableGpcKhz is the "ideal" Gpcclk as listed in RM's PStateTable
    // targetGpcKhz is the clock MODS is targeting
    // actualGpcKhz is the effective Gpcclk detected on the gpu which includes pll constraints and thermal slowdown
    Tasker::MutexHolder mh(GetMutex());

    Perf* const pPerf = this->GetGpuSubdevice()->GetPerf();

    MASSERT(pPerf);

    if ((PerfUtil::s_LimitVerbosity & PerfUtil::RM_LIMITS) &&
        pPerf->IsPState30Supported())
    {
        if (!GetIsClockChangeExpected())
        {
            // RM changes clocks internally
            pPerf->PrintPerfLimits();
        }
        else
        {
            // MODS requested clock changes, reset the flag
            SetIsClockChangeExpected(false);
        }
        return;
    }

    if (!GetAnyRequested() && GetIsClockChangeExpected())
    {
        // Mods hasn't requested any clock lock yet.
        // So whatever RM is doing with clock is fine with mods.
        // This is not an error.
        Printf(Tee::PriDebug, "Startup clock change: ClkGpc = %d kHz\n",
            actualGpcKhz);
        SetIsClockChangeExpected(false);
        return;
    }
    else if (GetExpectedNextPerf() == perfTableGpcKhz
             || GetExpectedNextPerf() == targetGpcKhz
             || GetExpectedNextPerf() == actualGpcKhz)
    {
        SetExpectedNextPerf(0);
        SetExpectedLwrPerf(actualGpcKhz);
        Printf(Tee::PriDebug, "Expected clock change: ClkGpc = %d kHz\n",
               actualGpcKhz);
        return;
    }
    else if (GetExpectedLwrPerf() == actualGpcKhz)
    {
        Printf(Tee::PriDebug, "Duplicate clock change: ClkGpc = %d kHz\n",
               actualGpcKhz);
        return;
    }

    const Tee::Priority pri = (GetVerbose()) ? Tee::PriNormal : Tee::PriLow;

    Printf(pri, "Clock change: ClkGpc = %d kHz\n", actualGpcKhz);

    ReasonsType reasonsParams = {0};
    StickyRC rc = GetChangeReasons(&reasonsParams);

    if (OK != rc)
    {
        Printf(Tee::PriError, "Unable to retrieve clock change reasons.\n");
        IncNumErrors();
        return;
    }

    PrintChangeReasons(pri, &reasonsParams);

    //Get Clock Limits
    LW2080_CTRL_PERF_LIMITS_GET_STATUS_PARAMS statusParams = {0};
    LW2080_CTRL_PERF_LIMITS_INFO_V2_PARAMS infoParams = {0};
    LW2080_CTRL_PERF_LIMIT_GET_STATUS limitList[LW2080_CTRL_PERF_MAX_LIMITS];
    LW2080_CTRL_PERF_LIMIT_INFO *infoList = infoParams.limitsList;
    ct_assert(NUMELEMS(infoParams.limitsList) >= LW2080_CTRL_PERF_MAX_LIMITS);
    for(UINT32 i = 0; i < LW2080_CTRL_PERF_MAX_LIMITS; i++)
    {
        limitList[i].limitId = i;
        infoList[i].limitId = i;
    }
    statusParams.numLimits = LW2080_CTRL_PERF_MAX_LIMITS;
    statusParams.pLimits = LW_PTR_TO_LwP64(limitList);
    infoParams.numLimits = LW2080_CTRL_PERF_MAX_LIMITS;

    rc = LwRmPtr()->ControlBySubdevice(
        GetGpuSubdevice(),
        LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS,
        &statusParams,
        sizeof(statusParams));

    rc = LwRmPtr()->ControlBySubdevice(
        GetGpuSubdevice(),
        LW2080_CTRL_CMD_PERF_LIMITS_GET_INFO_V2,
        &infoParams,
        sizeof(infoParams));

    if (OK != rc)
    {
        Printf(Tee::PriError, "Unable to retrieve Perf Limits.");
        IncNumErrors();
        return;
    }

    //Print active clock limits
    Printf(pri, "\tlimits:\n");
    bool activeLimits = false;
    for(UINT32 i = 0; i < LW2080_CTRL_PERF_MAX_LIMITS; i++)
    {
        if (infoList[i].szName[0] == '\0')
        {
            continue;
        }
        const LW2080_CTRL_PERF_LIMIT_OUTPUT& output = limitList[i].output;
        if (output.bEnabled
            && output.clkDomain == LW2080_CTRL_CLK_DOMAIN_GPC2CLK)
        {
            activeLimits = true;
            Printf(pri, "\t\t%-21s ID:%#4x %3s%3s Value:%d kHz\n",
                   infoList[i].szName,
                   infoList[i].limitId,
                   (infoList[i].flags & 1) ? "min" : "",
                   (infoList[i].flags & 2) ? "max" : "",
                   output.value);
        }
    }
    if (!activeLimits)
    {
        Printf(pri, "\t\tNo active Gpcclk limits\n");
    }

    if (!reasonsParams.thermalMask
        && !reasonsParams.pstateMask
        && !activeLimits)
    {
        Printf(Tee::PriError, "Unexpected Clock Change: No change reasons "
                          "and no active limits for "
                          "Gpcclk = %d kHz\n", actualGpcKhz);
        IncNumErrors();
    }
}

RC PStateChangeCounter::InitializeCounter()
{
    RC rc;
    CHECK_RC(PerfChangeCounter::InitializeCounter());
    SetExpectedNextPerf(MAX_NUM_PSTATES);
    SetExpectedLwrPerf(MAX_NUM_PSTATES);
    CHECK_RC(ErrCounter::Initialize("PStateChange", 1, 0,
                                    nullptr, MODS_TEST, PSTATE_CHANGE_PRI));
    CHECK_RC(HookToRM(LW2080_NOTIFIERS_PSTATE_CHANGE,
                      PStateChangeNotifyHandler));

    return rc;
}

RC PStateChangeCounter::ShutDown
(
    bool doOneLastErrorCheck
)
{
    RC rc;
    CHECK_RC(UnhookFromRM(LW2080_NOTIFIERS_PSTATE_CHANGE));
    CHECK_RC(PerfChangeCounter::ShutDown(doOneLastErrorCheck));
    return rc;
}

void PStateChangeCounter::NotifyHandler
(
    UINT32 actualPStateNum
)
{
    Tasker::MutexHolder mh(this->GetMutex());
    const Tee::Priority errPri = Tee::PriError;
    constexpr auto dbgPri = Tee::PriDebug;

    if (!GetAnyRequested())
    {
        // Mods hasn't requested any pstate lock yet.
        // So whatever RM is doing with pstate is fine with mods.
        // This is not an error.
        Printf(dbgPri, "Startup pstate change: %d\n", actualPStateNum);
        return;
    }
    if (GetExpectedNextPerf() == actualPStateNum)
    {
        // This is the pstate change we asked for, no error.
        SetExpectedNextPerf(MAX_NUM_PSTATES);
        SetExpectedLwrPerf(actualPStateNum);
        Printf(dbgPri, "Expected pstate change: %d\n", actualPStateNum);
        return;
    }
    if (GetExpectedLwrPerf() == actualPStateNum)
    {
        // RM has just told us it switched to the pstate we were already at.
        // Why does it notify us multiple times?
        // This is irritating.
        Printf(dbgPri, "Duplicate pstate notification: %d\n", actualPStateNum);
        return;
    }

    if (actualPStateNum == 0)
    {
        // http://lwbugs/980424
        // Sometimes we get a duplicate notification of 0 rather than expected.
        // Ignore these for now, until we can get RM help figuring out why.
        // They probably don't indicate a HW problem...
        Printf(dbgPri, "False(?) pstate 0 notification, ignoring.\n");
        return;
    }

    // Unexpected -- RM chose a different pstate on its own for some reason.

    Printf(errPri, "Unexpected PState change: %d (expected %d)\n",
           actualPStateNum, GetExpectedLwrPerf());
    IncNumErrors();

    ReasonsType params = {0};
    RC rc = GetChangeReasons(&params);

    if (OK != rc)
    {
        Printf(errPri, "\tUnable to retrieve pstate change reasons.\n");
    }
    else
    {
        PrintChangeReasons(errPri, &params);
    }
}

//-----------------------------------------------------------------------------
// PStates2Data code begins here
Perf::PStates2Data::PStates2Data(const Perf::PStates2Data & rhs)
    : Initialized(true),
      Params(rhs.Params),
      Clocks(rhs.Clocks),
      Volts(rhs.Volts)
{
    // Fix up the pointers in Params.
    for (UINT32 psIdx=0; psIdx < Params.numPstates; psIdx++)
    {
        Params.pstate[psIdx].perfClkDomInfoList =
            LW_PTR_TO_LwP64(&Clocks[psIdx][0]);
        Params.pstate[psIdx].perfVoltDomInfoList =
            LW_PTR_TO_LwP64(&Volts[psIdx][0]);
    }
}

RC Perf::VoltFreqLookup
(
    Gpu::ClkDomain clkDom,
    Gpu::SplitVoltageDomain voltDom,
    VFLookupType lookupType,
    UINT32 vfInput,
    UINT32 *vfOutput
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetVfeIndepVarsToJs
(
    JSContext *pCtx,
    JSObject *pJsOutArray
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::EnableArbitration(Gpu::ClkDomain dom)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::DisableArbitration(Gpu::ClkDomain dom)
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool Perf::IsArbitrated(Gpu::ClkDomain dom) const
{
    return false;
}

RC Perf::DisablePerfLimit(UINT32 limitId) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetVPStateIdx(UINT32 vpstateID, UINT32 *pIdx) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

// Checks for RM supported domains
bool Perf::IsRmClockDomain(Gpu::ClkDomain dom) const
{
    map<Gpu::ClkDomain, ClkDomainInfo>::const_iterator clkDomIter = m_ClockDomains.find(dom);
    return ((clkDomIter != m_ClockDomains.end()) &&
            (clkDomIter->second.IsRmClkDomain));
}

Perf::VFPointType Perf::GetVFPointType(LwU32 rmVFPointType)
{
    VFPointType ret;

    switch (rmVFPointType)
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            ret = VFPointType::Freq;
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            ret = VFPointType::Volt;
            break;
        }
        default:
        {
            ret = VFPointType::Unsupported;
        }
    }

    return ret;
}

RC Perf::GetClockSamples(vector<ClkSample> *pSamples)
{
    MASSERT(pSamples);

    StickyRC rc;
    size_t numDomains = pSamples->size();
    if (!numDomains)
    {
        // No domain requested for
        return OK;
    }

    UINT32 pmuBootMode = 0;
    if (OK == Registry::Read("ResourceManager", "RmPmuBootstrapMode", &pmuBootMode))
    {
        if (pmuBootMode != 0)
        {
            Printf(Tee::PriNormal, "PMU not bootstraped - cannot get clock\n");
            for (UINT32 i = 0; i < numDomains; i++)
            {
                (*pSamples)[i].ActualkHz      = 1;
                (*pSamples)[i].TargetkHz      = 1;
                (*pSamples)[i].EffectivekHz   = 1;
            }
            return OK;
        }
    }

    UINT64 actualHz, targetHz, effectiveHz;
    UINT32 factor;
    Gpu::ClkSrc clkSource;

    if (Platform::UsesLwgpuDriver())
    {
        for (UINT32 i = 0; i < numDomains; i++)
        {
            Gpu::ClkDomain dom = (*pSamples)[i].Domain;
            rc = m_pSubdevice->GetClock(dom, &actualHz, &targetHz, &effectiveHz, &clkSource);

            factor = 1000; // Hz to kHz
            (*pSamples)[i].ActualkHz      = static_cast<UINT32>(actualHz / factor);
            (*pSamples)[i].TargetkHz      = static_cast<UINT32>(targetHz / factor);
            (*pSamples)[i].EffectivekHz   = static_cast<UINT32>(effectiveHz / factor);
            (*pSamples)[i].Src            = clkSource;
        }
        return rc;
    }

    FLOAT32 freqRatio;
    vector<Gpu::ClkDomain> uniqueClkDomains(numDomains);
    vector<std::pair<UINT32, FLOAT32>> freqRatios(numDomains);
    const UINT32 IlwalidIdx = UINT_MAX;

    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_GET_EXTENDED_INFO_PARAMS ctrlParams = {0};
    ctrlParams.flags = 0;

    // Colwert between MODS and RM clocks domains + gather unique
    // clock domains to be queried for
    UINT32 numUniqueDomains = 0;
    for (UINT32 i = 0; i < numDomains; i++)
    {
        freqRatio = 1.0f;
        Gpu::ClkDomain dom = (*pSamples)[i].Domain;

        if (dom == Gpu::ClkM &&
            (m_pSubdevice->DeviceId() == Gpu::LwDeviceId::GV100 ||
             m_pSubdevice->DeviceId() == Gpu::LwDeviceId::GP100))
        {
            // Read registers directly for these clocks.
            // Handled by Device specific GetClock
            rc = m_pSubdevice->GetClock(dom, &actualHz, &targetHz, &effectiveHz, &clkSource);
            factor = 1000; // Hz to kHz
            (*pSamples)[i].ActualkHz      = static_cast<UINT32>(actualHz / factor);
            (*pSamples)[i].TargetkHz      = static_cast<UINT32>(targetHz / factor);
            (*pSamples)[i].EffectivekHz   = static_cast<UINT32>(effectiveHz / factor);
            (*pSamples)[i].Src            = clkSource;

            freqRatios[i] = make_pair(IlwalidIdx, freqRatio);
        }
        else
        {
            if (!IsRmClockDomain(dom))
            {
                freqRatio = PerfUtil::GetRmToModsFreqRatio(dom);
                dom       = PerfUtil::ColwertToOppositeClockDomain(dom);
            }

            auto it = find(uniqueClkDomains.begin(), uniqueClkDomains.end(), dom);
            if (it == uniqueClkDomains.end())
            {
                // Add a new clk domain to be queried for
                freqRatios[i] = make_pair(numUniqueDomains, freqRatio);
                ctrlParams.clkInfos[numUniqueDomains].clkInfo.clkDomain =
                                    PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
                uniqueClkDomains[numUniqueDomains++] = dom;
            }
            else
            {
                // Store the index of the same domain
                UINT32 idx = static_cast<UINT32>(distance(uniqueClkDomains.begin(), it));
                freqRatios[i] = make_pair(idx, freqRatio);
            }
        }
    }
    ctrlParams.numClkInfos = numUniqueDomains;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdevice),
                        LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
                        &ctrlParams,
                        sizeof(ctrlParams));

    // Populate the output ClkSamples vector
    const Avfs* pAvfs = m_pSubdevice->GetAvfs();
    for (UINT32 i = 0; i < numDomains; i++)
    {
        // Get the index into ctrlParams that the stored freq ratio
        // should be applied to
        UINT32 idx = freqRatios[i].first;
        if (idx == IlwalidIdx)
            continue;

        freqRatio = freqRatios[i].second;
        if (pAvfs && pAvfs->IsNafllClkDomain((*pSamples)[idx].Domain))
        {
            (*pSamples)[i].Src = Gpu::ClkSrcNAFLL;
        }
        else
        {
            (*pSamples)[i].Src = m_pSubdevice->Ctrl2080ClkSrcToClkSrc(
                                    ctrlParams.clkInfos[idx].clkInfo.clkSource);
        }
        (*pSamples)[i].ActualkHz    = static_cast<UINT32>(
                                      ctrlParams.clkInfos[idx].clkInfo.actualFreq * freqRatio);
        (*pSamples)[i].TargetkHz    = static_cast<UINT32>(
                                      ctrlParams.clkInfos[idx].clkInfo.targetFreq * freqRatio);
        (*pSamples)[i].EffectivekHz = static_cast<UINT32>(
                                      ctrlParams.clkInfos[idx].effectiveFreq * freqRatio);
    }

    return rc;
}

bool Perf::ClkMonStatus::operator==(const ClkMonStatus& rhs) const
{
    return !operator!=(rhs);
}

bool Perf::ClkMonStatus::operator!=(const ClkMonStatus& rhs) const
{
    if (clkDom != rhs.clkDom)
    {
        return true;
    }
    if (faultMask != rhs.faultMask)
    {
        return true;
    }
    if (index != rhs.index)
    {
        return true;
    }
    return false;
}

bool Perf::ClkMonStatus::operator<(const ClkMonStatus& rhs) const
{
    if (clkDom != rhs.clkDom)
    {
        return clkDom < rhs.clkDom;
    }
    if (faultMask != rhs.faultMask)
    {
        return faultMask < rhs.faultMask;
    }
    if (index != rhs.index)
    {
        return index < rhs.index;
    }
    return false;
}

void Perf::ClkMonStatus::PrintFaults(const Tee::Priority pri) const
{
    if (!faultMask)
    {
        if (clkDom != Gpu::ClkGpc && clkDom != Gpu::ClkM)
            Printf(pri, "%s has no clock monitor faults\n", PerfUtil::ClkDomainToStr(clkDom));
        else
            Printf(pri, "%s Partition%d has no clock monitor faults\n",
                   PerfUtil::ClkDomainToStr(clkDom), index);
        return;
    }
    string msg = Utility::StrPrintf("%s clock monitor faults detected:",
        PerfUtil::ClkDomainToStr(clkDom));
    if (clkDom == Gpu::ClkGpc || clkDom == Gpu::ClkM)
        msg = Utility::StrPrintf("%s Partition%d clock monitor faults detected:",
              PerfUtil::ClkDomainToStr(clkDom), index);
    UINT32 maskCpy = faultMask;
    INT32 bitIdx;
    while ((bitIdx = Utility::BitScanForward(maskCpy)) >= 0)
    {
        UINT32 faultType = 1 << bitIdx;
        maskCpy ^= faultType;
        msg += Utility::StrPrintf(" %s", ClkMonFaultTypeToStr(faultType));
    }
    Printf(pri, "%s\n", msg.c_str());
}

const char* Perf::ClkMonStatus::ClkMonFaultTypeToStr(UINT32 faultType)
{
    switch (faultType)
    {
        case LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_DC_FAULT:
            return "dc";
        case LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_LOWER_THRESH_FAULT:
            return "lower_threshold";
        case LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_HIGHER_THRESH_FAULT:
            return "higher_threshold";
        case LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_OVERFLOW_ERROR:
            return "overflow";
        default:
            MASSERT(!"Unknown clock monitor fault type");
            return "unknown";
    }
}

RC Perf::GetClkMonFaults(const vector<Gpu::ClkDomain> &clkDominsToMonitor,
                         ClkMonStatuses* pStatuses)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::ClearClkMonFaults(const vector<Gpu::ClkDomain>& clkDoms)
{
    return RC::UNSUPPORTED_FUNCTION;
}

vector<Gpu::ClkDomain> Perf::GetClkDomainsWithMonitors() const
{
    vector<Gpu::ClkDomain> retVal;
    for (auto & clkDomainInfo : PerfUtil::s_ClockDomainInfos)
    {
        auto it = m_ClockDomains.find(clkDomainInfo.modsId);
        // s_ClockDomainInfos contains all the clocks that can be supported in HW,
        // so we separate clocks based on whether they clock monitoring is supported by
        // RM which can be found by checking SupportsClkMon
        // If RM doesn't support it, then we check if Clock Monitoring is supported
        // by checking for If the FMON registers of those nonHalClocks are supported
        if (it != m_ClockDomains.end() && it->second.SupportsClkMon)
        {
            retVal.push_back(it->first);
        }
        else if (m_pSubdevice->IsSupportedNonHalClockMonitor(clkDomainInfo.modsId))
        {
            retVal.push_back(clkDomainInfo.modsId);
        }
    }
    return retVal;
}

RC Perf::SetClkMonThresholds(const vector<ClkMonOverride>& overrides)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::ClearClkMonThresholds(const vector<Gpu::ClkDomain>& clkDoms)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetUseMinSramBin(const string &skuName)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::ScaleSpeedo(FLOAT32 factor)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::OverrideKappa(FLOAT32 factor)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetOCBilwalue(UINT32 *pOCBilwalue)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetOCBinFuseValue(UINT32 *pOCBinFuseValue)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetOverclockRange(const Gpu::ClkDomain clkDom, OverclockRange* pOcRange)
{
    MASSERT(pOcRange);
    return RC::UNSUPPORTED_FUNCTION;
}

Gpu::SplitVoltageDomain Perf::RailIdxToGpuSplitVoltageDomain(UINT32 railIdx)
{
    return m_pSubdevice->GetVolt3x()->RailIdxToGpuSplitVoltageDomain(railIdx);
}

RC Perf::GetActivePerfLimitsToJs(JSContext *pCtx, JSObject* pJsObj)
{
    MASSERT(pCtx);
    MASSERT(pJsObj);
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetFmonTestPoint(UINT32 gpcclk_kHz, UINT32 margin_kHz, UINT32 margin_uV)
{
    return RC::UNSUPPORTED_FUNCTION;
}


RC Perf::UnlockClockDomains(const vector<Gpu::ClkDomain>& clkDoms)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetFrequencieskHz(Gpu::ClkDomain dom, vector<UINT32> *freqskHz) const
{
    MASSERT(freqskHz);
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::SetVfePollingPeriodMs(UINT32 periodMs)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Perf::GetVfePollingPeriodMs(UINT32* periodMs)
{
    MASSERT(periodMs);
    return RC::UNSUPPORTED_FUNCTION;
}
