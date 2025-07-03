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

/**
 * @file  perfsub.h
 * @brief APIs that maintain various performance settings for pstates 3.0
 *
 */

#pragma once
#ifndef INCLUDED_PERFSUB_30_H
#define INCLUDED_PERFSUB_30_H

#include "gpu/perf/perfsub.h"

class GpuSubdevice;
class RC;

class Perf30 : public Perf
{
public:
    Perf30(GpuSubdevice *pSubdevice, bool owned);
    ~Perf30() override;

    RC Cleanup() override;

    RC GetVfSwitchTimePriority(UINT32* priority) override;
    RC SetVfSwitchTimePriority(UINT32 priority) override;

    RC GetCoreVoltageMv(UINT32 *pVoltage) override;
    RC SetCoreVoltageMv(UINT32 Voltage, UINT32 PStateNum = ILWALID_PSTATE) override;
    RC SetVoltage(Gpu::VoltageDomain Domain, UINT32 VoltMv, UINT32 PState) override;

    RC PrintAllPStateInfo() override;
    RC PrintPerfLimits() const override;

    RC OverrideVfeVar
    (
        VfeOverrideType overrideType
       ,VfeVarClass varClass
       ,INT32 index // index into the VFE vars table
       ,FLOAT32 val
    ) override;

    RC OverrideVfeVarRange
    (
       VfeVarClass varClass
       ,INT32 index // index into the VFE vars table
       ,pair<FLOAT32, FLOAT32> range
    ) override;

    RC GetVfeIndepVarsToJs
    (
        JSContext *pCtx,
        JSObject *pJsOutArray
    ) override;

    RC PrintVfeIndepVars() override;
    RC PrintVfeEqus() override;
    RC OverrideVfeEqu
    (
        UINT32 idx,
        const LW2080_CTRL_PERF_VFE_EQU_CONTROL& vfeEqu
    ) override;
    RC OverrideVfeEquOutputRange
    (
        UINT32 idx
       ,bool bMax
       ,FLOAT32 val
    ) override;

    RC GetClockAtPStateNoCache
    (
        UINT32 PStateNum,
        Gpu::ClkDomain Domain,
        UINT64 *pFreqHz
    ) override;

    RC GetPexSpeedAtPState(UINT32 PStateNum, UINT32 *pLinkSpeed) override;
    RC GetLinkWidthAtPState(UINT32 PStateNum, UINT32 *pLinkWidth) override;

    //! Unsupported in this subclass
    RC GetVdtToJs(JSContext *pCtx, JSObject *pJsObj) override;

    RC SetPStateOffsetMv(UINT32 PStateNum, INT32 OffsetMv) override;

    RC SetOverrideOVOC(bool override) override;

    RC SetRailVoltTuningOffsetuV(Gpu::SplitVoltageDomain Rail, INT32 OffsetuV) override;
    RC SetRailClkDomailwoltTuningOffsetuV
    (
        Gpu::SplitVoltageDomain Rail,
        Gpu::ClkDomain ClkDomain,
        INT32 OffsetuV
    ) override;

    RC SetFreqClkDomainOffsetkHz(Gpu::ClkDomain ClkDomain, INT32 FrequencyInkHz) override;
    RC SetFreqClkDomainOffsetPercent(Gpu::ClkDomain ClkDomain, FLOAT32 percent) override;

    // Return all clock programming entries for a domain that fall within
    // a PState's range. If pstateNum == ILWALID_PSTATE, retrieve all clock
    // programming entries for the domain
    RC GetClkProgEntriesMHz
    (
        UINT32 pstateNum,
        Gpu::ClkDomain clkDom,
        ProgEntryOffset offset,
        map<LwU8, clkProgEntry>* pEntriesMHz
    ) override;

    RC SetClkProgrammingTableOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz) override;
    RC GetClkProgrammingTableSlaveRatio
    (
        UINT32 tableIdx,
        UINT32 slaveEntryIdx,
        UINT08 *pRatio
    ) override;
    RC SetClkProgrammingTableSlaveRatio
    (
        UINT32 tableIdx,
        UINT32 slaveEntryIdx,
        UINT08 ratio
    ) override;
    Gpu::ClkDomain GetDomainFromClockProgrammingOrder(UINT32 idx) const override;
    RC SetVFPointsOffsetkHz
    (
        UINT32 TableIdxStart,
        UINT32 TableIdxEnd,
        INT32 FrequencyInkHz,
        UINT32 lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI
    ) override;
    RC SetVFPointsOffsetuV
    (
        UINT32 TableIdxStart,
        UINT32 TableIdxEnd,
        INT32 OffsetInuV
    ) override;

    //! Unsupported in this subclass
    RC SetVdtTuningOffsetMv(INT32 OffsetMv) override;

    RC ClearVoltTuningOffsets() override;

    //! Set to a specific frequency and voltage via a the backdoor inject API
    //! This API (along with InjectFrequencyAndVoltage) should be removed once
    //! the production equivalents are completed. Right now, these only exist
    //! because pstates 3.0 is not ready for production yet.
    //! Uses LW2080_CTRL_CMD_PERF_VF_CHANGE_INJECT
    RC InjectPerfPoint(const InjectedPerfPoint &point) override;

    //! Set the regime for the NAFLL clock domain
    //! Setting regime == DEFAULT_REGIME clears any forced regimes
    RC SetRegime(Gpu::ClkDomain dom, RegimeSetting regime) override;
    RC GetRegime(Gpu::ClkDomain dom, RegimeSetting* regime) override;

    RC SetIntersectPt(const PerfPoint &setting) override;

    //! Generate PerfPoints for a clock domain
    //! Used in test 145/245
    RC GetVfPointPerfPoints
    (
        vector<PerfPoint> *pPoints,
        const vector<Gpu::ClkDomain> & domains
    ) override;

    RC GetVfPointsStatusForDomain
    (
        Gpu::ClkDomain dom,
        map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> *pLwrrentVfPoints
    ) override;
    RC GetVfPointsToJs(Gpu::ClkDomain domain,
                       JSContext *pCtx,
                       JSObject *pJsObj) override;

    RC GetPStateErrorCodeDigits(UINT32 *pThreeDigits) override;

    RC VoltFreqLookup
    (
        Gpu::ClkDomain clkDom,
        Gpu::SplitVoltageDomain voltDom,
        VFLookupType lookupType,
        UINT32 vfInput,
        UINT32 *vfOutput
    ) override;

    RC EnableArbitration(Gpu::ClkDomain) override;
    RC DisableArbitration(Gpu::ClkDomain) override;
    bool IsArbitrated(Gpu::ClkDomain) const override;
    RC DisablePerfLimit(UINT32 limitId) const override;

    RC GetVPStateIdx(UINT32 vpstateID, UINT32 *pIdx) const override;

    //! Get hardware supported frequencies for a clock domain
    RC GetFrequencieskHz(Gpu::ClkDomain dom, vector<UINT32> *freqskHz) const override;

    // Fills a JS object with the names of all active perf limits for
    // pstate, clocks, and voltages.
    RC GetActivePerfLimitsToJs(JSContext *pCtx, JSObject* pJsObj) override;

    RC SetFmonTestPoint(UINT32 gpcclk_kHz, UINT32 margin_kHz, UINT32 margin_uV) override;
    RC UnlockClockDomains(const vector<Gpu::ClkDomain>& clkDoms) override;

    RC GetClkMonFaults(const vector<Gpu::ClkDomain> &clkDominsToMonitor,
                       ClkMonStatuses* pStatuses) override;
    RC ClearClkMonFaults(const vector<Gpu::ClkDomain>& clkDoms) override;
    RC SetClkMonThresholds(const vector<ClkMonOverride>& overrides) override;
    RC ClearClkMonThresholds(const vector<Gpu::ClkDomain>& clkDoms) override;
    RC SetUseMinSramBin(const string &skuName) override;
    RC ScaleSpeedo(FLOAT32 factor) override;
    RC GetOverclockRange(Gpu::ClkDomain clkDom, OverclockRange* pOcRange) override;
    RC ForceClock
    (
        Gpu::ClkDomain clkDomain,
        UINT64 targetkHz,
        UINT32 flags
    ) override;
    RC OverrideKappa(FLOAT32 factor) override;

protected:
    RC GetVoltageAtPStateNoCache
    (
        UINT32 PStateNum,
        Gpu::VoltageDomain Domain,
        UINT32 *pVoltageUv
    ) override;
    RC GetLwrrentPerfValuesNoCache(PStateInfos *pPstatesInfo) override;

    RC SetPStateTableInternal
    (
        UINT32 pstateNum,
        const vector<LW2080_CTRL_PERF_CLK_DOM_INFO> & clks,
        const vector<LW2080_CTRL_PERF_VOLT_DOM_INFO>& voltages
    ) override;

    LwU32 GetVPStateId(GpcPerfMode m) const override;
    bool HasVirtualPState(GpcPerfMode m) const override;
    RC ClockOfVPState
    (
        int idx,
        bool isSymbolicIdx,
        UINT64 * pRtnGpcHz,
        UINT32 * pRtnPState
    ) override;
    RC GetTDPGpcHz(UINT64* tdpGpcHz) override;

    RC LockArbiter() override;
    RC UnlockArbiter() override;

    RC LockChangeSequencer() override;
    RC UnlockChangeSequencer() override;
    bool IsChangeSequencerLocked() const override;

    RC SetVfePollingPeriodMs(UINT32 periodMs) override;
    RC GetVfePollingPeriodMs(UINT32* periodMs) override;

private:
    //! Map of noise aware domains to their regime status
    map<Gpu::ClkDomain, RegimeSetting> m_NoiseAwareDomains;
    map<UINT32, UINT32> m_PStateNumToRmPStateIndex;
    map<UINT32, UINT32> m_PStateNumToRmPStateLevel;
    INT32 m_RailVoltTuningOffsetsuV[Gpu::SplitVoltageDomain_NUM];
    map<UINT32, INT32> m_RailClkDomailwoltTuningOffsetsuV[Gpu::SplitVoltageDomain_NUM];
    map<Gpu::ClkDomain, INT32> m_FreqClkDomainOffsetkHz;
    map<Gpu::ClkDomain, vector<UINT32> > m_ClkDomRmVfIndices;
    bool m_OverrideOVOC;
    LW2080_CTRL_CLK_CLK_DOMAINS_CONTROL_PARAMS m_ClkDomainsControl;
    vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS> m_PerfLimits;
    PerfPoint m_LwrPerfPoint;
    PerfPoints m_VPStates;
    struct ProgEntrySetting
    {
        UINT32 index = 0;
        INT32 offsetkHz = 0;
        ProgEntrySetting(UINT32 idx, INT32 offkHz) : index(idx), offsetkHz(offkHz) {}
        static constexpr UINT32 ILWALID_IDX = (std::numeric_limits<UINT32>::max)();
    };
    vector<ProgEntrySetting> m_AdjustedDramclkProgEntries;
    set<Gpu::ClkDomain> m_DisabledClkDomains;

    enum VfSwitchType
    {
        VF_SWITCH_MODS,
        VF_SWITCH_RM_LIMITS,
        VF_SWITCH_CHANGE_SEQ,
        VF_SWITCH_NUM_TYPES
    };
    UINT64 m_AvgVfSwitchTimeUs[VF_SWITCH_NUM_TYPES] = {0};
    UINT64 m_MilwfSwitchTimeUs[VF_SWITCH_NUM_TYPES] = {0};
    UINT64 m_MaxVfSwitchTimeUs[VF_SWITCH_NUM_TYPES] = {0};
    UINT32 m_NumVfSwitches = 0;
    void UpdateVfSwitchStats(UINT32 vfSwitchType, UINT64 timeUs);

    map<Gpu::ClkDomain, FLOAT32> m_FreqClkPercentOffsets;

    bool m_RestoreClocksOnExit = true;
    bool m_HasPowerCapping;
    bool m_ShouldHavePowerCapping;
    UINT32 m_LooseLimitIdx = 0;
    UINT32 m_IntersectLimitIdx = 0;

    // The RM BOARDOBJ interface uses GET_INFO RMCTRL calls, which return
    // static information based on VBIOS tables. These values never change
    // at runtime. So caching them during initialization can reduce the
    // number of RMCTRL calls needed later at the expense of additional
    // system memory.
    struct CachedRmInfo
    {
        LW2080_CTRL_CLK_CLK_DOMAINS_INFO clkDomains = {};
        LW2080_CTRL_CLK_CLK_PROGS_INFO clkProgs = {};
        LW2080_CTRL_CLK_CLK_VF_POINTS_INFO vfPoints = {};
        LW2080_CTRL_PERF_PSTATES_INFO pstates = {};
        LW2080_CTRL_PERF_VPSTATES_INFO vpstates = {};
        LW2080_CTRL_PERF_PERF_LIMITS_INFO perfLimits = {};
        LW2080_CTRL_PERF_VFE_VARS_INFO vfeVars = {};
        LW2080_CTRL_PERF_VFE_EQUS_INFO vfeEqus = {};
        LW2080_CTRL_CLK_CLIENT_CLK_DOMAINS_INFO clientClkDoms = {};
    };
    CachedRmInfo m_CachedRmInfo = {};

    UINT32 m_NewDramclkVFPointIdx = ProgEntrySetting::ILWALID_IDX;
    INT32 m_NewDramclkVFPointOffsetkHz = 0;
    UINT32 m_LwrDramclkVFPointIdx = ProgEntrySetting::ILWALID_IDX;

    //! Return the index into GET_INFO or GET_CONTROL structure for the given
    //! pstate number (eg 0, 5, 8)
    //! Not to be confused with GetPStateIdx, which maps pstate numbers to
    //! indices in m_CachedPStates or m_OrgPStates
    //!
    //! The index returned by this function should be used only for directly
    //! accessing entries in a PSTATES description object such as
    //! LW2080_CTRL_PERF_PSTATES_{INFO,STATUS,CONTROL} objects returned by
    //! their respective RMCTRL interfaces. Iterating PSTATES should be done
    //! using the LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX macro, and
    //! RMCTRL parameters should use GetRmPStateLevel instead.
    RC GetRmPStateIndex(UINT32 pstateNum, UINT32 *rmPStateIndex) const;

    //! Return the RM perf level for the given pstate number
    //!
    //! Note that this level should be used instead of index for any
    //! calls to the RM that request a PSTATE index or PSTATE level
    RC GetRmPStateLevel(UINT32 pstateNum, UINT32 *rmPStateLevel) const;

    RC GetClkDomainsControl();
    RC SetClkDomainsControl();

    RC SetVoltTuningParams(VoltTuningWriteMode m) override;
    RC SetVFPointsOffset(VFPointType Type, UINT32 TableIdxStart, UINT32 TableIdxEnd, INT32 Value,
                         UINT32 lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI);
    RC ApplyFreqTuneOffset(INT32 freqOffsetKhz, UINT32 pstateNum) override;
    RC ParseVfTable() override;

    RC GetVirtualPerfPoints(PerfPoints *points) override;
    RC GetVPStatesInfo();

    RC Init() override;
    RC GenerateStandardPerfPoints
    (
        UINT64 fastestSafeGpc2Hz,
        const PStateInfos &pstateInfo,
        PerfPoints *pPoints
    ) override;
    RC GetRatiodClocks
    (
        UINT32 PStateNum,
        UINT64 FreqHz,
        vector<LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo
    ) override;

    //! Force all NAFLL clock domains to a regime
    RC ForceNafllRegimes(RegimeSetting regime);

    //! For each clock in clks, force to the fixed frequency regime if the
    //! Regime setting is set to FIXED_FREQUENCY_REGIME
    RC SetRegimesForClocks(const ClkMap& clks);

    RC GetLwrrentPerfPoint(PerfPoint *settings) override;

    RC InnerSetPerfPoint(const PerfPoint& Setting) override;
    RC InnerSetPStateLock
    (
        const PerfPoint &Setting,
        PStateLockType Lock
    ) override;

    //! Use LIMITS_SET_STATUS RMCTRL to apply frequency limits to all of the
    //! clocks in Setting.Clks. Master clocks use STRICT limit ids, slaves use
    //! LOOSE. Previous settings are cleared if left unspecified in Setting.Clks
    RC SetPStateLockViaLimits(const PerfPoint &Setting);
    RC ResetLimitsIfPStateLockChanged(PStateLockType newLockType);
    RC SetForcedPStateLockType(PStateLockType type) override;
    RC SetUseHardPStateLocks(bool useHard) override;
    RC UseIMPCompatibleLimits() override;
    RC RestoreDefaultLimits() override;
    RC ClearForcedPState() override;
    RC ClearForcePStateEx(UINT32 flags) override;
    RC ClearPerfLimits() override;

    //! Create an intersect perf limit from perfPoint.IntersectSettings
    RC ConstructIntersectLimits
    (
        const PerfPoint& perfPoint,
        LW2080_CTRL_PERF_LIMIT_INPUT* pIntersectLimit
    );

    RC InnerConstructIntersectLimit
    (
        const IntersectSetting& is,
        UINT32 pstateNum,
        LW2080_CTRL_PERF_LIMIT_INPUT* pIntersectLimit
    );

    //! Create any intersect bound limits needed to WAR RM bug 1786523
    RC ConstructBoundLimits
    (
        const PerfPoint& perfPoint,
        vector<LW2080_CTRL_PERF_LIMIT_INPUT>* pBoundLimits
    ) const;

    //! Assign intersectLimit and upperBoundLimit to corresponding limits in
    //! m_PerfLimits
    RC AssignIntersectAndBoundLimits
    (
        const LW2080_CTRL_PERF_LIMIT_INPUT& intersectLimit,
        const vector<LW2080_CTRL_PERF_LIMIT_INPUT>& boundLimits
    );

    //! Validate the IntersectSettings field in a PerfPoint
    RC CheckIntersectSettings(const set<IntersectSetting>& intersectSettings);

    //! Retrieve all of the VF points from all of the clock programming
    //! entries for this clock domain. Then copy them into the appropriate
    //! VfEntryTbls according to which pstate clock range they fall within
    RC GetClkDomailwfEntryTables
    (
        Gpu::ClkDomain dom,
        UINT32 clkProgIdxFirst,
        UINT32 clkProgIdxLast,
        const LW2080_CTRL_CLK_CLK_PROGS_INFO &clkProgTable,
        const LW2080_CTRL_CLK_CLK_VF_POINTS_INFO &vfPointsInfo
    );
    //! Put all of the VF points that are a part of the pstate
    //! into pPStatePoints
    //! pstates 3.0 represents pstates as a clock range per clock domain
    //! Thus, if a VfPoint is within that clock range, it is in the pstate
    RC FindVfPointsInPState
    (
        Gpu::ClkDomain dom,
        UINT32 pstateNum,
        const VfPoints &allPoints,
        VfPoints *pPStatePoints
    );
    //! Go through all of the clock programming entries for the given clock
    //! domain and pull out all of the VF points
    RC GetClkDomailwfPoints
    (
        Tee::Priority pri,
        Gpu::ClkDomain dom,
        UINT32 clkProgIdxFirst,
        UINT32 clkProgIdxLast,
        const LW2080_CTRL_CLK_CLK_PROGS_INFO &clkProgTable,
        const LW2080_CTRL_CLK_CLK_VF_POINTS_INFO &vfPointsInfo,
        VfPoints *pClkVfPoints
    );

    //! Clears m_PerfLimits, but does not send updated limits to RM
    void DisableAllPerfLimits();

    RC GetPerfLimitByID(UINT32 limitID, LW2080_CTRL_PERF_LIMIT_INPUT **limit);
    RC LookupPerfStrictLimits
    (
        Gpu::ClkDomain dom,
        LW2080_CTRL_PERF_LIMIT_INPUT **pMinLimit,
        LW2080_CTRL_PERF_LIMIT_INPUT **pMaxLimit
    );
    RC LookupPerfLooseLimits
    (
        LW2080_CTRL_PERF_LIMIT_INPUT **pMinLimit,
        LW2080_CTRL_PERF_LIMIT_INPUT **pMaxLimit
    );
    RC LookupPerfIntersectLimit(LW2080_CTRL_PERF_LIMIT_INPUT **pIntersectLimit);
    RC SendPerfLimitsToRM();
    //! Sends both perf limits and clock programming entry offsets to RM
    RC SendPerfPointToRM();

    RC CheckSplitRailDomainExists(Gpu::SplitVoltageDomain voltDom) const;

    // Do a sanity check on Setting
    RC ValidatePerfPoint(const PerfPoint& Setting);

    // Looks up limits for min, mid, nom and max PerfPoints
    RC LookupPStateLimits
    (
        LW2080_CTRL_PERF_LIMIT_INPUT **pMinLimit,
        LW2080_CTRL_PERF_LIMIT_INPUT **pMaxLimit
    );

    // Constructs a single limit for min, mid, nom and max PerfPoints
    RC ConstructPStateLimit
    (
        const PerfPoint &Setting,
        LW2080_CTRL_PERF_LIMIT_INPUT *pInputLimit
    ) const;

    // Constructs min/max limits for min, mid, nom and max PerfPoints
    RC ConstructPStatePerfLimits(const PerfPoint& Setting);

    // Constructs perf limits to lock to the frequencies specified in Clks
    // as well as GpcPerfHz
    RC ConstructClksPerfLimits(const PerfPoint& Setting, const ClkMap& Clks);

    // Determine if we need to offset a dramclk programming entry to meet
    // the target frequency
    RC ConfigureDramclkSettings(UINT32 pstateNum, const ClkSetting& clkSetting);

    // Clear all dramclk programming entry offsets MODS has applied (if any)
    RC ClearDramclkProgEntryOffsets();

    RC SetArbitrationStatus(Gpu::ClkDomain dom, bool enable);

    RC ValidateClkDomain(Gpu::ClkDomain);

    enum class ClockInjectionType : UINT08
    {
        ALL_CLOCKS,
        FIXED_CLOCKS
    };
    // Sets clock frequencies via InjectPerfPoint
    RC SetClocksViaInjection(const PerfPoint& Setting, ClockInjectionType injType);

    // Queries the RM perf change sequencer for information about the previous
    // perf switch
    RC HandleRmChgSeq();

    // Add a limit for the Vmin of the rail to which we are intersecting
    RC AppendVminLimit(LW2080_CTRL_PERF_LIMIT_INPUT* pIntersectLimit);

    void PrintPerfLimit
    (
        UINT32 limIdx,
        const LW2080_CTRL_PERF_PERF_LIMIT_INFO& limitInfo,
        const LW2080_CTRL_PERF_PERF_LIMIT_STATUS& limitStatus
    ) const;

    void PrintPerfLimitStatus_v35
    (
        const LW2080_CTRL_PERF_PERF_LIMIT_INFO& limitInfo,
        const LW2080_CTRL_PERF_PERF_LIMIT_STATUS& limitStatus
    ) const;

    void PrintArbiterOutput(const LW2080_CTRL_PERF_PERF_LIMITS_STATUS& limitsStatus) const;

    void PrintArbiterOutput_PMU
    (
        const LW2080_CTRL_PERF_PERF_LIMITS_STATUS& limitsStatus
    ) const;

    void PrintArbiterOutput_v35
    (
        const LW2080_CTRL_PERF_PERF_LIMITS_STATUS& limitsStatus
    ) const;

    enum class LockState
    {
        LOCKED,
        UNLOCKED
    };
    RC SetArbiterState(LockState state);
    RC SetChangeSequencerState(LockState state);

    RC GetCachedRmInfo();

    RC GetPerfLimitsStatus(LW2080_CTRL_PERF_PERF_LIMITS_STATUS* pStatus) const;

    //! \brief Returns the closest exact frequency on the VF lwrve to the input frequency
    RC QuantizeFrequency
    (
        Gpu::ClkDomain clkDom, //! Specifies the clock domain
        LwU16 inputFreq_MHz, //! The input frequency which will be quantized
        bool bFloor, //! Round down if true, rounds up if false
        LwU16* quantizedOutputFreq_MHz //! Output frequency quantized to the VF lwrve
    );
};

#endif
