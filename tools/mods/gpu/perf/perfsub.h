/* * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  perfsub.h
 * @brief APIs that maintain various performance settings
 *
 */

#pragma once
#ifndef INCLUDED_PERFSUB_H
#define INCLUDED_PERFSUB_H

#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/gpu.h"
#include "core/include/callback.h"
#include "gpu/utility/gpuerrcount.h"
#include "core/include/setget.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "ctrl/ctrl2080/ctrl2080pmgr.h"
#include <vector>
#include <set>
#include <map>

#define MAX_NUM_PSTATES (12+1)

class GpuSubdevice;
struct JSObject;
struct JSContext;

//------------------------------------------------------------------------------
//! Generic base class for detecting RM perf notification events
//! There are 2 kinds of perf notification events that we can hook into:
//! 1) Pstate change notifications
//! 2) Clock change notifications
class PerfChangeCounter  : public GpuErrCounter
{
public:
    PerfChangeCounter(GpuSubdevice * pGpuSubdevice)
        : GpuErrCounter(pGpuSubdevice)
        , m_Verbose(false)
        , m_IsHooked(false)
        , m_AnyRequested(false)
        , m_NumErrors(0)
        , m_ExpectedNextPerf(0)
        , m_ExpectedLwrPerf(0)
    {
    }

    virtual RC InitializeCounter();
    virtual RC ShutDown(bool);
    virtual void AddExpectedChange(const UINT32 expected);

    void SetVerbose(bool b) { m_Verbose = b; }
    bool GetVerbose() const { return m_Verbose; }

protected:
    bool GetIsHooked() const { return m_IsHooked; }
    bool GetAnyRequested() const { return m_AnyRequested; }
    void IncNumErrors() { m_NumErrors++; }
    UINT32 GetExpectedNextPerf() const   { return m_ExpectedNextPerf; }
    void   SetExpectedNextPerf(UINT32 i) { m_ExpectedNextPerf = i; }
    UINT32 GetExpectedLwrPerf() const    { return m_ExpectedLwrPerf; }
    void   SetExpectedLwrPerf(UINT32 i)  { m_ExpectedLwrPerf = i; }

    RC HookToRM(const UINT32 eventType, void (*notifyHandler)(void*));
    RC UnhookFromRM(const UINT32 eventType);

    typedef LW2080_CTRL_CMD_PERF_GET_PERF_DECREASE_INFO_PARAMS ReasonsType;
    RC GetChangeReasons(ReasonsType* reasons) const;
    void PrintChangeReasons(const Tee::Priority pri, ReasonsType* reasons) const;

    virtual RC OnError(const ErrorData * unused)
    {
        return RC::UNEXPECTED_RESULT;
    }

    RC ReadErrorCount(UINT64 * pCount)
    {
        pCount[0] = m_NumErrors;
        return OK;
    }

private:
    bool m_Verbose;
    bool m_IsHooked;
    bool m_AnyRequested;
    UINT64 m_NumErrors;
    UINT32 m_ExpectedNextPerf;
    UINT32 m_ExpectedLwrPerf;

};

//------------------------------------------------------------------------------
//! Count pstate-change notifications from RM, compare them to those requested
//! by mods.  Report errors when we don't get the pstates we expected.
//!
//! In pstates 2.0, mods normally uses "soft" pstate locks, allowing RM to
//! change clocks/voltage to stay in power/temp/voltage safety limits but not
//! to lower clocks just to save power when the gpu is idle.
//! RM should be able to stay in the requested pstate.
//!
//! One possible cause of RM spontaneously switching to a lower pstate is
//! a bad temperature or power sensor.  This class detects that sort of issue.
//!
class PStateChangeCounter : public PerfChangeCounter
{
public:
    PStateChangeCounter(GpuSubdevice * pGpuSubdevice)
        : PerfChangeCounter(pGpuSubdevice)
    {
    }
    virtual RC InitializeCounter();
    virtual RC ShutDown(bool);
    void NotifyHandler(const UINT32 actualPStateNum);

};

//------------------------------------------------------------------------------
//! Count clock change notifications from RM and report them along with the
//! the reason if reported by RM
class ClockChangeCounter : public PerfChangeCounter
{
public:
    ClockChangeCounter(GpuSubdevice * pGpuSubdevice)
        : PerfChangeCounter(pGpuSubdevice),
          m_IsClockChangeExpected(false)
    {
    }
    virtual RC InitializeCounter();
    virtual RC ShutDown(bool);
    void NotifyHandler(const UINT32 perfTableGpc2KHz,
                       const UINT32 targetGpc2Khz,
                       const UINT32 actualGpc2KHz);
    SETGET_PROP(IsClockChangeExpected, bool);

private:
    bool m_IsClockChangeExpected;
};

class Perf
{
public:
    Perf(GpuSubdevice* pSubdevice, bool owned);
    virtual ~Perf();

    enum class VFPointType : UINT08
    {
        Volt,
        Freq,
        Unsupported,
    };

    enum class PStateVersion : UINT08
    {
        PSTATE_UNSUPPORTED,
        PSTATE_10, // Ancient history...
        PSTATE_20, // GM20X and older
        PSTATE_30, // GP100, GP10X, and GV100
        PSTATE_35, // TU10X and newer
        PSTATE_40, // GA10X and newer
    };

    // Specify an option for how GpcClk is set (in a PerfPoint)
    enum GpcPerfMode
    {
        GpcPerf_EXPLICIT   //!< User specifies an exact GpcPerf Hz
        ,GpcPerf_MAX       //!< Highest gpc of a pstate
        ,GpcPerf_MID       //!< Mid point between min,max
        ,GpcPerf_NOM       //!< gpc of the pstate when mods first reads it
        ,GpcPerf_MIN       //!< Lowest gpc of a pstate
        ,GpcPerf_INTERSECT //!< max gpc at min lwvdd of pstate
        ,GpcPerf_TDP       //!< gpc of the "rated" or "base" virtual pstate
        ,GpcPerf_MIDPOINT  //!< gpc of the "mid_point" virtual pstate
        ,GpcPerf_TURBO     //!< gpc of the "boost" virtual pstate
        ,NUM_GpcPerfMode   // must be last
    };
    enum IntersectType
    {
        IntersectLogical,
        IntersectVDT,
        IntersectVFE,
        IntersectPState,
        IntersectVPState,
        IntersectVoltFrequency,
        IntersectFrequency,
    };

    // Enum to present the values of index 1 of 12 digit MODS errorcode string
    enum class PerfPointType
    {
        Unknown = 0,
        Explicit = 0,
        Min,
        Max,
        Tdp,
        Turbo,
        IntersectVolt,
        IntersectVoltFreq,
        IntersectPState,
        IntersectVPState,
        MultipleIntersect,
    };

    // Enum to present the values of index 2 of 12 digit MODS errorcode string
    enum class IntersectRail
    {
        Unknown = 0,
        Logic,
        Sram,
        Msvdd
    };

    // Enum to present the values of index 3 of 12 digit MODS errorcode string
    enum class ClkDomainForIntersect
    {
        Unknown = 0,
        NotApplicable,
        Gpc2Clk,
        DispClk,
        DramClk
    };

    struct IntersectSetting
    {
        IntersectType Type;
        Gpu::SplitVoltageDomain VoltageDomain;
        union
        {
            UINT32 LogicalVoltageuV;
            UINT32 VDTIndex;
            UINT32 VFEEquIndex;
            UINT32 VPStateIndex;
            struct
            {
                Gpu::ClkDomain ClockDomain;
                UINT32         ValueKHz;
            } Frequency;
        };
        IntersectSetting()
        : Type(IntersectPState)
        , VoltageDomain(Gpu::VOLTAGE_LOGIC)
        , LogicalVoltageuV(0xFFFFFFFF)
        {}
        bool operator<(const IntersectSetting &is) const;
        bool operator==(const IntersectSetting &is) const;
    };

    // is there a clean way to do this
    enum
    {
        ILWALID_PSTATE = 0xFFFFFFFF
    };

    // An NAFLL sourced domain will be in the DEFAULT_REGIME unless
    // forced to one of the other three, in which case, we should
    // clean up the states on MODS shutdown.
    enum RegimeSetting
    {
        FIXED_FREQUENCY_REGIME
       ,VOLTAGE_REGIME
       ,FREQUENCY_REGIME
       ,VR_ABOVE_NOISE_UNAWARE_VMIN
       ,FFR_BELOW_DVCO_MIN
       ,VR_WITH_CPM
       ,DEFAULT_REGIME
       ,RegimeSetting_NUM
    };

    static const char * const DEFAULT_INFLECTION_PT;

    struct ClkSetting
    {
        Gpu::ClkDomain Domain;
        UINT64         FreqHz;
        UINT32         Flags;  // eg FORCE_PLL, FORCE_BYPASS, FORCE_NAFLL
        bool           IsIntersect;
        RegimeSetting  Regime;

        ClkSetting()
        : Domain(Gpu::ClkUnkUnsp),
          FreqHz(0),
          Flags(0),
          IsIntersect(false),
          Regime(RegimeSetting_NUM)
        {}
        ClkSetting(Gpu::ClkDomain d, UINT64 hz, UINT32 flags = 0, bool IsIntersect = false)
        : Domain(d),
          FreqHz(hz),
          Flags(flags),
          IsIntersect(IsIntersect),
          Regime(RegimeSetting_NUM)
        {}
        bool operator==(const ClkSetting& rhs) const;
        void Print(Tee::Priority pri) const;
    };
    typedef map<Gpu::ClkDomain, ClkSetting> ClkMap;

    struct VoltageSetting
    {
        Gpu::VoltageDomain Domain;
        UINT32             VoltMv;

        VoltageSetting()
        : Domain(Gpu::CoreV),
          VoltMv(0)
        {}
    };

    struct SplitVoltageSetting
    {
        Gpu::SplitVoltageDomain domain;
        UINT32 voltMv;

        SplitVoltageSetting() : domain(Gpu::VOLTAGE_LOGIC), voltMv(0) {}
        SplitVoltageSetting(Gpu::SplitVoltageDomain dom, UINT32 mV) :
            domain(dom), voltMv(mV) {}
        void Print(Tee::Priority pri) const;
    };

    struct PerfPoint
    {
        bool       FastPStateSwitch   = false;
        UINT32     FallBack           = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
        GpcPerfMode SpecialPt         = GpcPerf_EXPLICIT;
        UINT32     PStateNum          = ILWALID_PSTATE;
        INT32      VoltTuningOffsetMv = 0;
        UINT32     LinkSpeed          = 0;
        UINT32     LinkWidth          = 0;
        bool       UnsafeDefault      = false;
        ClkMap     Clks               = { };
        vector<VoltageSetting> Voltages;
        set<IntersectSetting>  IntersectSettings;
        RegimeSetting Regime          = RegimeSetting_NUM;

        PerfPoint() = default;

        PerfPoint
        (
            UINT32 pstate,
            GpcPerfMode m
        )
        :   SpecialPt(m),
            PStateNum(pstate)
        {
        }

        PerfPoint //TODO Do we need this constructor
        (
            UINT32 pstate,
            GpcPerfMode m,
            UINT64 gpcPerfHz,
            set<IntersectSetting> is = set<IntersectSetting>()
        )
        :   SpecialPt(m),
            PStateNum(pstate),
            IntersectSettings(move(is))
        {
            if (gpcPerfHz)
            {
                // PerfPunish::MakePerfPoint passes a 0 value to gpcPerfHz,
                // this check ensure a value is not entered into the Clks array
                Clks[Gpu::ClkGpc] = ClkSetting(Gpu::ClkGpc, gpcPerfHz);
            }
        }

        void SetUnsafeIfFasterThan(UINT64 hz)
        {
            UINT64 gpcPerfHz = 0;
            (void) GetFreqHz(Gpu::ClkGpc, &gpcPerfHz);
            if (gpcPerfHz && hz && (gpcPerfHz > hz))
                UnsafeDefault = true;
        }
        Gpu::SplitVoltageDomain IntersectVoltageDomain() const;
        bool operator< (const PerfPoint & that) const;
        bool operator== (const PerfPoint & rhs) const;
        bool operator!= (const PerfPoint & rhs) const
        {
            return !(*this == rhs);
        }
        string name(bool showRail) const;
        RC GetFreqHz(Gpu::ClkDomain clkDom, UINT64* pFreqHz) const;
    };
    typedef set<PerfPoint> PerfPoints;

    struct InjectedPerfPoint
    {
        vector<ClkSetting> clks;
        vector<SplitVoltageSetting> voltages;
        void Print(Tee::Priority) const;
    };

    virtual RC Cleanup();

    bool HasPStates();

    //! Get core voltage as reported by LW2080_CTRL_CMD_PERF_GET_SAMPLED_VOLTAGE_INFO
    virtual RC GetCoreVoltageMv(UINT32 *pVoltage);
    //! Sets core voltage by pstate if possible, otherwise resorts to very old API
    virtual RC SetCoreVoltageMv(UINT32 Voltage, UINT32 PStateNum = ILWALID_PSTATE);
    //! Set core voltage when PStates are disabled (only used on CheetAh)
    virtual RC SetPStateDisabledVoltageMv(UINT32 voltageMv);

    //! Sets the core voltage in the pstate
    virtual RC SetVoltage(Gpu::VoltageDomain Domain, UINT32 VoltMv, UINT32 PState) = 0;

    enum PerfCallback
    {
        PRE_PSTATE,
        POST_PSTATE,
        PRE_CLEANUP,
        NUM_PERF_CALLBACKS
    };
    RC SetUseCallbacks(PerfCallback CallbackType, string JSFuncName);

    RC SuggestPState(UINT32 TargetState, UINT32 FallBack);
    RC FastSuggestPState(UINT32 TargetState, UINT32 FallBack);
    RC ForcePState(UINT32 Target, UINT32 FallBack);
    RC FastForcePState(UINT32 Target, UINT32 FallBack);

    //! Get current pstate. Use local cache if possible
    virtual RC GetLwrrentPState(UINT32 *pStateNum);
    RC GetLwrrentPStateNoCache(UINT32 * pLwrPState);
    virtual RC ClearForcedPState() = 0;
    //! for each available pstate print out the clock and voltage settings
    virtual RC PrintAllPStateInfo() = 0;

    //! NOTE: this can return 1x or 2x depending on input clock domain
    RC GetDefaultClockAtPState(UINT32 PStateNum,
                               Gpu::ClkDomain Domain,
                               UINT64 *pFreqHz);

    //! Get current clock speed for the given domain from the given pstate
    //! Use local cache if possible
    //! NOTE: this will return a 2x clock if available
    virtual RC GetClockAtPState(UINT32 PStateNum,
                                Gpu::ClkDomain Domain,
                                UINT64 *pFreqHz);

    //! NOTE: this will return a 1x or 2x clock depending on input domain
    virtual RC GetClockAtPStateNoCache(UINT32 PStateNum,
                                       Gpu::ClkDomain Domain,
                                       UINT64 *pFreqHz) = 0;
    virtual RC GetClockAtPerfPoint(const PerfPoint & perfPoint,
                                   Gpu::ClkDomain Domain,
                                   UINT64 *pFreqHz);

    //! Measure clock freq from the given pllIndex using the clock pulse counters
    //! Uses LW2080_CTRL_CMD_CLK_MEASURE_PLL_FREQ
    RC MeasurePllClock(Gpu::ClkDomain clkToGet, UINT32 pllIndex, UINT64 *pFreqHz);

    //! Measure clock freq using the clock pulse counters, blocks for 40us
    //! Uses LW2080_CTRL_CMD_CLK_MEASURE_FREQ
    //! Should be applicable to any clock domain
    RC MeasureClock(Gpu::ClkDomain clkToGet, UINT64 *pFreqHz);

    //! Measures a clock domain's frequency per partition
    //! Fails if clkToGet is not a core clock (GPC, SYS, LTC, XBAR)
    //! A partition would be an NAFLL for GP100+
    //! Note: this call takes a (non-blocking) 1ms to execute
    //! It samples clock counters using two calls to
    //! LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ 1ms apart
    //! Uses MeasureClockPartitions
    RC MeasureCoreClock(Gpu::ClkDomain clk, UINT64 *pEffectiveFreqHz);

    //! Use per-partition clock counters to measure avg clock count of a core clock domain
    RC MeasureAvgClkCount(Gpu::ClkDomain clk, UINT64 *pClkCount);

    //! Use clock counters to measure partitions of a core clock domain
    RC MeasureClockPartitions
    (
        Gpu::ClkDomain clkToGet,
        UINT32 partitionMask,
        vector<UINT64> *pEffectiveFreqsHz,
        vector<LW2080_CTRL_CLK_CNTR_SAMPLE>* pClockSamples
    );
    RC MeasureClockPartitions
    (
        Gpu::ClkDomain clkToGet,
        UINT32 partitionMask,
        vector<UINT64> *pEffectiveFreqsHz
    )
    {
        return MeasureClockPartitions(clkToGet, partitionMask, pEffectiveFreqsHz, nullptr);
    }

    //! Force a programmable clock domain to a frequency via
    //! LW2080_CTRL_CMD_CLK_SET_INFO. This bypasses perf
    //! arbitration and does not guarantee the voltage needed to
    //! support the target frequency.
    virtual RC ForceClock
    (
        Gpu::ClkDomain clkDomain,
        UINT64 targetkHz,
        UINT32 flags
    );

    //! For the given perfPoint location (max, min, mid etc),
    //! determine appropriate gpc2clk frequency to set to.
    //! For intersect, this value will be irrelevant (except on Android)
    RC GetGpcFreqOfPerfPoint
    (
        const PerfPoint& perfPoint,
        UINT64* pGpcFreqHz
    );
    RC GetVoltageAtPState(UINT32 PStateNum,
                          Gpu::VoltageDomain Domain,
                          UINT32 *pVoltageMv);

    virtual RC GetPexSpeedAtPState(UINT32 PStateNum, UINT32 *pLinkSpeed) = 0;
    virtual RC GetLinkWidthAtPState(UINT32 PStateNum, UINT32 *pLinkWidth) = 0;

    RC GetNumPStates(UINT32 *pNumPStates);

    //! check if the state exists
    RC DoesPStateExist(UINT32 PStateNum, bool *pRetVal);
    virtual bool HasVirtualPState(GpcPerfMode m) const;

    RC GetAvailablePStates(vector<UINT32> *pPStates);

    GpuSubdevice * GetGpuSubdevice() { return m_pSubdevice; }

    static UINT32 StringToSpecialPt(const string & LocationStr);
    static const char* SpecialPtToString(UINT32 Point);
    RC JsObjToPerfPoint(JSObject *pJsPerfPt, PerfPoint *pNewPerfPt);
    RC PerfPointToJsObj
    (
        const PerfPoint & perfPoint,
        JSObject *pObj,
        JSContext *pCtx
    );

    virtual RC GetVdtToJs(JSContext *pCtx, JSObject *pJsObj) = 0;

    //! \brief Returns the current VF points for a clock domain
    //!
    //! The VF points returned by this function will change with
    //! temperature (i.e. the VF points returned here are dynamic)
    //!
    //! \param dom - Clock domain
    //! \param pLwrrentVfPoints - a map from RM index to actual VF point
    virtual RC GetVfPointsStatusForDomain
    (
        Gpu::ClkDomain dom,
        map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> *pLwrrentVfPoints
    );

    // Offset RM's callwlated LWVDD via PMGR_VOLTAGE_REQUEST_ARBITER_CONTROL.
    // Mods tracks global, pstate, and perfpoint values which are summed and
    // sent to the RM during each SetPerfPoint call.
    // This is for the "global" arbiter offset on the max of client requests.
    RC SetDefaultVoltTuningOffsetMv(INT32 OffsetMv);
    RC SetGlobalVoltTuningOffsetMv(INT32 OffsetMv);
    RC SetPStateVoltTuningOffsetMv(INT32 OffsetMv, UINT32 PState);
    RC SetPerfPtVoltTuningOffsetMv(INT32 OffsetMv);

    virtual RC SetOverrideOVOC(bool override);

    virtual RC SetRailVoltTuningOffsetuV(Gpu::SplitVoltageDomain Rail, INT32 OffsetuV);
    virtual RC SetRailClkDomailwoltTuningOffsetuV
    (
        Gpu::SplitVoltageDomain Rail,
        Gpu::ClkDomain ClkDomain,
        INT32 OffsetuV
    );

    // Offset RM's callwlated LWVDD via PMGR_VOLTAGE_REQUEST_ARBITER_CONTROL.
    // This is for just one of the clients (pstate, gpc2clk, dispclk).
    RC SetClientOffsetMv(UINT32 client, INT32 offsetMv);

    enum VminFloorMode
    {
        PStateVDTIdxMode,
        VDTEntryIdxMode,
        LogicalVoltageMode,
        VminFloorModeDisabled
    };

    // Suggest an absolute Vmin floor for RM arbiter to factor in globally
    RC SuggestVminFloor
    (
        VminFloorMode mode,
        UINT32 modeArg,
        INT32 floorOffsetmV
    );

    // PStates 2.0 only - offset the pstate minimum LWVDD required by mclk.
    // Actual LWVDD is max(pstate, VDT) + arbiter_offset.
    // Possibly obsolete, given SetClientOffsetMv.
    virtual RC SetPStateOffsetMv(UINT32 PStateNum, INT32 OffsetMv) = 0;

    // PStates 2.0 only - offset the VDT tuning entry for callwlating the
    // required LWVDD of the current gpc2clk.
    // Requires vbios setup so that the VDT chains to a "tuning entry".
    // Actual LWVDD is max(pstate, VDT) + arbiter_offset.
    // Possibly obsolete, given SetClientOffsetMv.
    virtual RC SetVdtTuningOffsetMv(INT32 OffsetMv) = 0;

    //! Clear all mods volt-tuning offsets back to 0
    virtual RC ClearVoltTuningOffsets();

    INT32 GetVoltTuningOffsetMv();

    // For PState 2.0 only
    INT32 GetFreqTuningOffsetKHz(INT32 PStateNum);
    RC SetFreqTuningOffsetKHz(INT32 OffsetKHz, INT32 PStateNum);

    struct clkProgEntry
    {
        UINT16 entryFreqMHz;
        INT16 freqDeltaMinMHz;
        INT16 freqDeltaMaxMHz;
        bool bOCOVEnabled;
    };

    // For PState 3.0 only
    virtual RC SetFreqClkDomainOffsetkHz(Gpu::ClkDomain ClkDomain, INT32 FrequencyInkHz);
    virtual RC SetFreqClkDomainOffsetPercent(Gpu::ClkDomain ClkDomain, FLOAT32 percent);
    enum class ProgEntryOffset { DISABLED, ENABLED };
    virtual RC GetClkProgEntriesMHz
    (
        UINT32 pstateNum,
        Gpu::ClkDomain clkDom,
        ProgEntryOffset offset,
        map<LwU8, clkProgEntry>* pEntriesMHz
    );
    virtual RC GetVfRelEntriesMHz
    (
        UINT32 pstateNum,
        Gpu::ClkDomain clkDom,
        const ProgEntryOffset applyOffset,
        map<LwU8, clkProgEntry>* pEntriesMHz
    )
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC GetClkProgrammingTableSlaveRatio
    (
        UINT32 tableIdx,
        UINT32 slaveEntryIdx,
        UINT08 *pRatio
    );
    virtual RC SetClkProgrammingTableOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz);
    virtual RC SetClkProgrammingTableSlaveRatio
    (
        UINT32 tableIdx,
        UINT32 slaveEntryIdx,
        UINT08 ratio
    );
    virtual RC SetVFPointsOffsetkHz
    (
        UINT32 TableIdxStart,
        UINT32 TableIdxEnd,
        INT32 FrequencyInkHz,
        UINT32 lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI
    );
    virtual RC SetVFPointsOffsetuV
    (
        UINT32 TableIdxStart,
        UINT32 TableIdxEnd,
        INT32 OffsetInuV
    );
    virtual RC SetClkVfRelationshipOffsetkHz(UINT32 RelIdx, INT32 FrequencyInkHz);
    //! The default amount of time SetPerfPoint is allowed to take without a timeout error
    static const UINT32 DEFAULT_MAX_PERF_SWITCH_TIME_NS;

    PerfPoint GetMatchingStdPerfPoint(const PerfPoint & point);
    virtual RC GetLwrrentPerfPoint(PerfPoint *settings);

    //! Set up timer, get gpc2clk for this perf point, then call InnerSetPerfPoint
    virtual RC SetPerfPoint(const PerfPoint &Setting);

    //! See comment in perfsub_30.h about when this should be removed
    virtual RC InjectPerfPoint(const InjectedPerfPoint &point);

    //! Set the regime of an NAFLL clock domain
    virtual RC SetRegime(Gpu::ClkDomain dom, RegimeSetting regime);

    //! Get the regime of an NAFLL clock domain. Only looks at one of the
    //! NAFLLs for the domain, so it may be innaclwrate on multi-partition
    //! domains.
    virtual RC GetRegime(Gpu::ClkDomain dom, RegimeSetting* regime);

    static Perf::RegimeSetting Regime2080CtrlBitToRegimeSetting(LwU32 rmRegime);
    static const char* RegimeToStr(RegimeSetting regime);

    RC ComparePerfPoint(const PerfPoint &s1,
                        const PerfPoint &s2,
                        UINT32 PercentThreshold);

    virtual RC SetIntersectPt(const PerfPoint &Setting);
    RC GetDecoupledClockAtPerfPtHz
    (
        const PerfPoint &Setting,
        Gpu::ClkDomain Domain,
        UINT64 *pFrequency
    );
    RC SetRequiredDispClk(UINT32 DispClkHz, bool AllowAbovePstateVmin);

    struct ClkRange
    {
        UINT32 MinKHz;
        UINT32 MaxKHz;
        UINT32 LwrrKHz;
    };

    // Can return either 1x or 2x ranges depending on input domain
    RC GetClockRange
    (
        UINT32 PStateNum,
        Gpu::ClkDomain Domain,
        ClkRange *pClkRange
    );

    //! NOTE: this expects 2x clock if available
    bool IsClockInRange
    (
        UINT32 PStateNum,
        UINT32 rmDomain,
        INT32 freqKHz
    );
    //! Checks if the given clock domain is within its original pstate range
    //! Works on any clock domain
    bool IsClockInOriginalRange
    (
        UINT32 pstateNum,
        UINT32 rmDomain,
        UINT32 freqkHz
    );

    // VfPoints that come from RM will use 2x clocks when applicable
    struct VfPoint
    {
        UINT32 ClkFreqKHz;    // start of the Vf point range
        UINT32 Flags;
        union
        {
            UINT32 VdtIdx;        // VD tabel entry
            UINT32 VfeIdx;        // VFE index
        };
        UINT32 VfIdx;         // VF entry index in VBIOS (PS2.0) or generated by RM (PS3.0)
        UINT32 StepSizeKHz;   // distance from the next point
        bool   IsDerived;     // whether this point is derived from VF entries
        UINT32 DvcoOffsetCode; // distance of VF point in secondary VF lwrve from Primary
        bool operator< (const VfPoint &other) const
        {
            return this->ClkFreqKHz < other.ClkFreqKHz;
        }
    };
    typedef vector<VfPoint> VfPoints;

    RC GetVfPointsToJs(UINT32 PStateNum,
                       Gpu::ClkDomain domain,
                       JSContext *pCtx,
                       JSObject *pJsObj);

    // Overloaded version to get ALL VF points including
    // those outside of all PState ranges. The underlying RMCTRL
    // call is only supported in PStates 3.0
    virtual RC GetVfPointsToJs(Gpu::ClkDomain domain,
                               JSContext *pCtx,
                               JSObject *pJsObj);

    RC GetStaticFreqsToJs
    (
        UINT32 PStateNum,
        Gpu::ClkDomain domain,
        JSObject *pJsObj
    ) const;

    RC VfPointToJsObj(const VfPoint& vfPoint, JSObject* pJsObj, bool is1xDom) const;

    //! \brief Returns the dynamic list of VF points where the frequencies
    //!        are bounded by the PState range
    RC GetVfPointsAtPState
    (
        UINT32 PStateNum,
        Gpu::ClkDomain domain,
        map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> *pVfPts
    );

    virtual RC GetPStateErrorCodeDigits(UINT32 *pThreeDigits);

    static UINT32 PStateNumTo2080Bit(UINT32 PStateNum){return (1<<PStateNum);};
    UINT32 GetPStateClkDomainMask();
    UINT32 GetPStateVoltageDomainMask();
    bool CanModifyThisPState(UINT32 PStateNum);
    bool CanSetVoltDomailwiaPState(Gpu::VoltageDomain Domain);
    bool CanSetClockDomailwiaPState(Gpu::ClkDomain Domain);

    //! Sets GPC2 clock and its ratiod clocks.
    RC SetGpcPerfClk(UINT32 PStateNum, UINT64 FreqHz);

    RC GetPerfPoints(PerfPoints *pPoints);

    GpcPerfMode LabelGpcPerfPoint(const PerfPoint & pp) const;

    //! Generate PerfPoints to hit all the interesting combinations of pstate and
    //! each "decoupled" clock domain. Used in test 145 (PerfSwitch)
    virtual RC GetVfPointPerfPoints
    (
        vector<PerfPoint> *pPoints,
        const vector<Gpu::ClkDomain> & domains
    );

    //! deprecated
    RC SetInflectionPoint(UINT32 PStateNum, UINT32 Location);
    RC GetInflectionPoint(UINT32* pPStateNum, UINT32* pLocation) const;
    string GetInflectionPtStr() const;
    RC OverrideInflectionPt(UINT32 PStateNum,
                            UINT32 gpcPerfMode,
                            const PerfPoint & NewPerf);
    RC SetTegraPerfPointInfo(UINT32 index, const string& name);

    // External API for setting pstate tables in RM
    RC SetPStateTable(UINT32 PStateNum,
                      const vector<LW2080_CTRL_PERF_CLK_DOM_INFO> & Clks,
                      const vector<LW2080_CTRL_PERF_VOLT_DOM_INFO>& Voltages);
    enum WhichDomains
    {
        AllDomains,   // all domains
        OnlyDecoupled // all decoupled domains (gpc2clk, maybe display)
    };

    //! Restores the pstate settings to original values at mods init for all
    //! pstates specified by PStateRestoreMask, optionally only restore
    //! decoupled domains
    virtual RC RestorePStateTable(UINT32 PStateRestoreMask, WhichDomains d);
    RC RestorePStateTable()
    {
        // ILWALID_PSTATE is a wildcard meaning "all pstates".
        return RestorePStateTable(ILWALID_PSTATE, AllDomains);
    }

    virtual RC GetClkDomainInfoToJs(JSContext *pCtx, JSObject *pJsOutArray);
    RC GetVoltDomainInfoToJs(UINT32             PStateNum,
                             Gpu::VoltageDomain Domain,
                             JSContext         *pCtx,
                             JSObject          *pRetObj);

    PStateChangeCounter & GetPStateChangeCounter()
    {
        return m_PStateChangeCounter;
    }

    ClockChangeCounter & GetClockChangeCounter()
    {
        return m_ClockChangeCounter;
    }

    RC GetPerfSpecTable(LW2080_CTRL_PERF_GET_PERF_TEST_SPEC_TABLE_INFO_PARAMS *pSpecTable);

    virtual RC SetUseHardPStateLocks(bool useHard);
    RC GetUseHardPStateLocks(bool * pUseHard);

    enum ClkFlags
    {
        FORCE_PLL         = 0x01,
        FORCE_BYPASS      = 0x02,
        FORCE_NAFLL       = 0x04,
        FORCE_LOOSE_LIMIT = 0x08,
        FORCE_STRICT_LIMIT = 0x10,
        FORCE_IGNORE_LIMIT = 0x20,
        FORCE_PERF_LIMIT_MIN = 0x40,
        FORCE_PERF_LIMIT_MAX = 0x80,
    };
    enum PStateSwitchType
    {
        NORMAL_SWITCH, // Wait's for vblank
        FAST_SWITCH // Doesn't wait for next vblank to switch pstates
    };
    enum PStateLockType
    {
        NotLocked,       //! No pstates, or we're not holding any lock
        HardLock,        //! ForcePState was called/use MODS_RULES limits
        SoftLock,        //! SuggestPState was called/use CLIENT_LOW limits
        DefaultLock,     //! Use CLIENT_STRICT limits to lock a PerfPoint. N/A before PS30
        StrictLock,      //! Force CLIENT_STRICT limits and never go to SoftLock. N/A before PS30
        VirtualLock      //! Locked to voltage or virtual pstate. N/A to PS30
    };
    static LwU32 ForceFlag(PStateLockType lockType);

    virtual RC SetForcedPStateLockType(PStateLockType lockType);
    RC GetForcedPStateLockType(PStateLockType* pLockType) const;
    PStateLockType GetPStateLockType() const { return m_ActivePStateLockType; }
    virtual RC UseIMPCompatibleLimits();
    virtual RC RestoreDefaultLimits();
    bool IsOwned() const { return m_Owned; }
    void SetPStateOwner ( bool IsOwned) { m_Owned = IsOwned;}
    bool IsLocked() const { return GetPStateLockType() != NotLocked; }
    bool IsPState20Supported();
    bool IsPState30Supported();
    bool IsPState35Supported();
    bool IsPState40Supported();
    // External API to set clocks via pstate tables in RM
    RC SetClockDomailwiaPState
    (
        Gpu::ClkDomain clkDomain,
        UINT64 targetHz,
        UINT32 flags
    );

    // Should we unlock before every pstate lock operation?
    // This is always done for VirtualLock operations.
    // We can force it for Hard and Soft locks, for backwards compatibility.
    RC GetForceUnlockBeforeLock(bool * p) const
    {
        *p = m_ForceUnlockBeforeLock;
        return OK;
    }
    RC SetForceUnlockBeforeLock(bool b)
    {
        m_ForceUnlockBeforeLock = b;
        return OK;
    }

    // On MacOSX platform we see code that clears the perf levels set by MODS
    // so that RM can do modesets. These API will help toggle this feature
    // since having it enabled interferes with shmooing
    bool GetClearPerfLevelsForUi() const { return m_ClearPerfLevelsForUi; }
    void SetClearPerfLevelsForUi(bool b) { m_ClearPerfLevelsForUi = b; }

    RC GetDeepIdlePState(UINT32 *pPStateNum)
    {
        *pPStateNum = *(m_DIPStates.begin());
        return OK;
    }
    RC SetDIPStates(vector <UINT32> PStates);
    RC GetDIPStates(vector<UINT32> * pPStates);
    RC ClearDIPStates(){ m_DIPStates.clear(); return OK;}
    bool IsDIPStateSupported(UINT32 DIPState);

    //! Print all perf limits according to PerfUtil::s_LimitVerbosity
    virtual RC PrintPerfLimits() const;

    //! Print all current RM perf limits (including disabled)
    RC PrintRmPerfLimits(Tee::Priority pri) const;

    RC GetDumpLimitsOnPerfSwitch(bool * p) const;
    RC SetDumpLimitsOnPerfSwitch(bool b) const;
    RC GetLimitVerbosity(UINT32* pLimitVerbosity);
    RC SetLimitVerbosity(UINT32 limitVerbosity);

    struct PStates2Data
    {
        bool Initialized;
        LW2080_CTRL_PERF_GET_PSTATES20_DATA_PARAMS Params = {};
        vector<vector<LW2080_CTRL_PERF_PSTATE20_CLK_DOM_INFO> > Clocks;
        vector<vector<LW2080_CTRL_PERF_PSTATE20_VOLT_DOM_INFO> > Volts;

        PStates2Data() : Initialized(false) {}
        PStates2Data(const PStates2Data &);
    };
    RC GetPStates2Data(PStates2Data *);

    enum FreqCtrlType
    {
        FIXED,      // domain is fixed at initial value by devinit
        PSTATE,     // follows PState - like PState 1.0
        DECOUPLED,  // decoupled -> a master of a decoupled group
        RATIO,      // domain is a member of group pgrogrammed at a ratio of the master
        PROGRAMMABLE, // Pstate 4.0 domain could be either master or slave configuration
                      // on a given voltage Rail
        UNKNOWN_TYPE,
    };
    FreqCtrlType ClkDomainType(Gpu::ClkDomain d) const;
    //! Given a clock domain, return the index into the RMCTRL GET_INFO
    //! structure that will give information for that clock domain.
    UINT32 GetClkDomainClkObjIndex(Gpu::ClkDomain d) const;

    //! Each index should have variable propRegimeID in LW2080_CTRL_CLK_CLK_PROP_REGIME_INFO.
    //! We should find the matching index for propRegimeID
    virtual RC GetClkPropRegimeIndex(UINT32 propRegimeID, UINT32 *index) const
        {return RC::UNSUPPORTED_FUNCTION; }

    //! Retrieve the clock domain from the RMCTRL GET_INFO given the RM clock index
    virtual Gpu::ClkDomain GetClkObjIndexClkDomain(UINT32 clkIdx) const
        {return Gpu::ClkDomain_NUM; }
    
    //! Retrieve the partition mask for the given clock domain
    //! The partition mask refers to "partitions" of a clock domain.
    //! For example, a chip with 6 GPCs would have 6 clock partitions
    //! Right now, partitions can be used to read individual NAFLL frequencies
    UINT32 GetClockPartitionMask(Gpu::ClkDomain d) const;

    RC GetDumpVfTable(bool* b) { *b = m_DumpVfTable; return OK; }
    RC SetDumpVfTable(bool  b) { m_DumpVfTable = b;  return OK; }

    enum VfeOverrideType
    {
        OVERRIDE_NONE = LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_NONE,
        OVERRIDE_VALUE = LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_VALUE,
        OVERRIDE_OFFSET = LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_OFFSET,
        OVERRIDE_SCALE = LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_SCALE
    };
    enum VfeVarClass
    {
        CLASS_ILWALID = LW2080_CTRL_PERF_VFE_VAR_TYPE_ILWALID,
        CLASS_FREQ = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY,
        CLASS_VOLT = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE,
        CLASS_TEMP = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP,
        CLASS_FUSE = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE,
        CLASS_PRODUCT = LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT,
        CLASS_GLOBAL = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED,
        CLASS_SUM = LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM,
        CLASS_FUSE_20 = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE_20
    };
    virtual RC OverrideVfeVar
    (
        VfeOverrideType overrideType
       ,VfeVarClass varClass
       ,INT32 index // index into the VFE vars table
       ,FLOAT32 val
    )
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    virtual RC OverrideVfeVarRange
    (
        VfeVarClass varClass
       ,INT32 index // index into the VFE vars table
       ,pair<FLOAT32, FLOAT32> range
    )
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    virtual RC PrintVfeIndepVars() { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetVfeIndepVarsToJs
    (
        JSContext *pCtx,
        JSObject *pJsOutArray
    );

    virtual RC PrintVfeEqus() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC OverrideVfeEqu
    (
        UINT32 idx
       ,const LW2080_CTRL_PERF_VFE_EQU_CONTROL& vfeEqu
    )
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC OverrideVfeEquOutputRange
    (
        UINT32 idx
       ,bool bMax
       ,FLOAT32 val
    )
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    static RC PrintClockInfo(const PerfPoint *point,
                             Tee::Priority pri = Tee::PriNormal);

    virtual RC GetVfSwitchTimePriority(UINT32 *priority);
    virtual RC SetVfSwitchTimePriority(UINT32 priority);

    virtual Gpu::ClkDomain GetDomainFromClockProgrammingOrder(UINT32 idx) const;

    enum VFLookupType
    {
        VoltToFreq,
        FreqToVolt
    };

    enum ClockDomainType
    {
        Master,
        Slave,
        None
    };

    enum VfSwitchVal
    {
        Mods,
        RmLimits,
        Pmu,
        NumTypes
    };

    void SetAllowedPerfSwitchTimeNs(VfSwitchVal type, UINT32 time) 
    { m_AllowedPerfSwitchTimeNs[type] = time; }

    UINT32 GetAllowedPerfSwitchTimeNs(VfSwitchVal type)
    { return m_AllowedPerfSwitchTimeNs[type]; }
    
    virtual RC VoltFreqLookup
    (
        Gpu::ClkDomain clkDom,
        Gpu::SplitVoltageDomain voltDom,
        VFLookupType lookupType,
        UINT32 vfInput,
        UINT32 *vfOutput
    );

    // Enable perf arbitration for a clock domain. All decoupled and ratio
    // domains are arbitrated by default.
    virtual RC EnableArbitration(Gpu::ClkDomain dom);

    // Disable perf arbitration for a clock domain
    virtual RC DisableArbitration(Gpu::ClkDomain dom);

    // Returns true if RM is lwrrently arbitrating dom
    virtual bool IsArbitrated(Gpu::ClkDomain dom) const;

    // Disables the specified perf limit
    virtual RC DisablePerfLimit(UINT32 limitId) const;

    // Returns LW2080_CTRL_PERF_VPSTATE_INDEX_ILWALID if vpstateID is not
    // set in the VBIOS. Otherwise, it returns an index corresponding to the
    // entry in the Virtual P-State Table in the VBIOS for the given vpstateID.
    // Valid values for *pIdx will range from 0 to
    // (LW2080_CTRL_PERF_VPSTATE_INDEX_ILWALID-1) inclusive.
    virtual RC GetVPStateIdx(UINT32 vpstateID, UINT32 *pIdx) const;

    // Checks for RM supported domains
    virtual bool IsRmClockDomain(Gpu::ClkDomain dom) const;

    SETGET_PROP(UsePStateCallbacks, bool);
    GET_PROP(PStateVersion, Perf::PStateVersion);
    GET_PROP(ArbiterEnabled, bool);

    virtual VFPointType GetVFPointType(LwU32 rmVFPointType);

    virtual RC LockArbiter() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC UnlockArbiter() { return RC::UNSUPPORTED_FUNCTION; }
    bool IsArbiterLocked() const { return !m_ArbiterEnabled; }

    virtual RC LockChangeSequencer() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC UnlockChangeSequencer() { return RC::UNSUPPORTED_FUNCTION; }
    virtual bool IsChangeSequencerLocked() const { return false; }
    RC GetUnlockChangeSequencer(bool* b) { *b = m_UnlockChangeSequencer; return OK; }
    RC SetUnlockChangeSequencer(bool  b) { m_UnlockChangeSequencer = b;  return OK; }

    struct ClkSample
    {
        Gpu::ClkDomain Domain      = Gpu::ClkUnkUnsp;
        UINT32       ActualkHz     = 0;
        UINT32       TargetkHz     = 0;
        UINT32       EffectivekHz  = 0;
        Gpu::ClkSrc  Src           = Gpu::ClkSrcDefault;

        ClkSample(Gpu::ClkDomain domain = Gpu::ClkUnkUnsp)
        : Domain(domain)
        {}
    };

    //! Get frequencies of requested clock domains
    virtual RC GetClockSamples(vector<ClkSample> *pSamples);
    enum class ClkDomainProp : UINT08
    {
        READABLE,
        WRITEABLE
    };
    RC GetRmClkDomains(vector<Gpu::ClkDomain> *pDomains, ClkDomainProp prop);

    struct ClkMonStatus
    {
        Gpu::ClkDomain clkDom = Gpu::ClkUnkUnsp;
        UINT32 faultMask = 0;
        UINT32 index = std::numeric_limits<UINT32>::max();

        ClkMonStatus() = default;
        explicit ClkMonStatus(Gpu::ClkDomain c, UINT32 i) : clkDom(c), index(i) {}
        ClkMonStatus(Gpu::ClkDomain c, UINT32 mask, UINT32 i) : clkDom(c), faultMask(mask),
        index(i)
        {}
        bool operator==(const ClkMonStatus& rhs) const;
        bool operator!=(const ClkMonStatus& rhs) const;
        bool operator<(const ClkMonStatus& rhs) const;

        void PrintFaults(Tee::Priority pri) const;
        static const char* ClkMonFaultTypeToStr(UINT32 faultType);
    };
    using ClkMonStatuses = vector<ClkMonStatus>;

    virtual RC GetClkMonFaults(const vector<Gpu::ClkDomain> &clkDominsToMonitor,
                               ClkMonStatuses* pStatuses);
    virtual RC ClearClkMonFaults(const vector<Gpu::ClkDomain>& clkDoms);
    vector<Gpu::ClkDomain> GetClkDomainsWithMonitors() const;

    enum class ClkMonThreshold { LOW, HIGH };
    struct ClkMonOverride
    {
        Gpu::ClkDomain clkDom;
        ClkMonThreshold thresh;
        FLOAT32 val;
    };
    virtual RC SetClkMonThresholds(const vector<ClkMonOverride>& overrides);
    virtual RC ClearClkMonThresholds(const vector<Gpu::ClkDomain>& clkDoms);
    virtual RC SetUseMinSramBin(const string &skuName);

    //! \brief Scale speedo by a floating point factor
    //!
    //! \param factor - How much to scale speedo (must be greater than 0)
    //!
    //! Example: Original_speedo=1800, factor=1.02 -> new_speedo=1836
    //! Speedo will always stay in the VFE variable range set by VBIOS
    //! even if factor is really big or small.
    virtual RC ScaleSpeedo(FLOAT32 factor);

    //! \brief Override kappa by a floating point number
    //!
    //! \param factor - The overrided kappa value
    virtual RC OverrideKappa(FLOAT32 factor);

    virtual RC GetOCBilwalue(UINT32 *pOCBilwalue);
    virtual RC GetOCBinFuseValue(UINT32 *pOCBinFuseValue);

    struct OverclockRange
    {
        INT32 MinOffsetkHz = 0;
        INT32 MaxOffsetkHz = 0;
    };
    virtual RC GetOverclockRange(Gpu::ClkDomain clkDom, OverclockRange* pOcRange);
    Gpu::SplitVoltageDomain RailIdxToGpuSplitVoltageDomain(UINT32 railIdx);

    virtual RC GetActivePerfLimitsToJs(JSContext *pCtx, JSObject* pJsObj);
    virtual RC SetFmonTestPoint(UINT32 gpcclk_kHz, UINT32 margin_kHz, UINT32 margin_uV);
    virtual RC UnlockClockDomains(const vector<Gpu::ClkDomain>& clkDoms);
    virtual RC GetFrequencieskHz(Gpu::ClkDomain dom, vector<UINT32> *freqskHz) const;

    virtual RC SetVfePollingPeriodMs(UINT32 periodMs);
    virtual RC GetVfePollingPeriodMs(UINT32* periodMs);

protected:
    GpuSubdevice   *m_pSubdevice;
    bool            m_InitDone;
    bool            m_PStatesInfoRetrieved;
    bool            m_DumpVfTable;
    UINT32          m_CachedPStateNum;
    UINT32          m_ClkDomainsMask;
    UINT32          m_VoltageDomainsMask;
    UINT32          m_VPStatesMask;
    UINT32          m_VPStatesNum;
    PStateLockType  m_ActivePStateLockType;
    enum CacheState
    {
        CACHE_DIRTY,
        CACHE_CLEAN,
    };
    CacheState            m_CacheStatus;
    CacheState            m_PStateNumCache;
    PStateVersion         m_PStateVersion = PStateVersion::PSTATE_UNSUPPORTED;
    bool                  m_UsePStateCallbacks;
    PStateLockType        m_ForcedPStateLockType;
    UINT32                m_LwrPerfPointPStateNum;
    GpcPerfMode           m_LwrPerfPointGpcPerfMode;
    set<IntersectSetting> m_LwrIntersectSettings;
    ClockChangeCounter    m_ClockChangeCounter;
    Tee::Priority         m_VfSwitchTimePriority;
    bool                  m_ArbiterEnabled;
    bool                  m_UnlockChangeSequencer;

    struct RailVfItem
    {
        UINT08  Type = LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_NONE;
        union
        {
            struct
            {
               UINT08 clkVfRelIdxFirst;
               UINT08 clkVfRelIdxLast;
               LW2080_CTRL_BOARDOBJGRP_MASK_E32 slaveDomainsMask;
               LW2080_CTRL_BOARDOBJGRP_MASK_E32 masterSlaveDomainsMask;
            }master;

            struct
            {
                UINT08 masterIdx;
            }slave;
        };
    };

    struct ClkDomainInfo
    {
        Gpu::ClkDomain Domain;
        FreqCtrlType   Type;
        Gpu::ClkDomain RatioDomain; // if this is ratio, specify the master here
        UINT32         Ratio;       // ratio Domain to RatioDomain in percentage
        UINT32         partitionMask; // Mask of available partitions (eg NAFLLs) for a clock domain
        bool           PStateFloor; // whether the domain must stay above the nominal value of the current pstate
        UINT32         objectIndex; // Index into LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS domains array
        bool           IsRmClkDomain; // Whether this is a RM supported clock domain
        bool           SupportsClkMon = false;
        struct FreqDeltaRangeMHz
        {
            INT16 minFreqMHz = 0;
            INT16 maxFreqMHz = 0;
        }freqDeltaRangeMHz; // OC min and max frequency

        struct InfoType40
        {
            UINT08  clkEnumIdxFirst;
            UINT08  clkEnumIdxLast;
            UINT08  preVoltOrderingIndex;
            UINT08  postVoltOrderingIndex;
            UINT08  clkVFLwrveCount;
            UINT32  railMask;
            RailVfItem RailVfItems[LW2080_CTRL_CLK_CLK_DOMAIN_PROG_RAIL_VF_ITEM_MAX];
        }infoType40;

        ClkDomainInfo()
        : Domain(Gpu::ClkUnkUnsp),
          Type(FIXED),
          RatioDomain(Gpu::ClkUnkUnsp),
          Ratio(0),
          partitionMask(0),
          PStateFloor(false),
          objectIndex(0),
          IsRmClkDomain(false)
        {}
    };

    struct VoltInfo
    {
        UINT32 LwrrentTargetMv;
        UINT32 LogicalMv;
        UINT32 VdtIdx;
    };

    struct PStateInfo
    {
        UINT32 PStateNum;   // used by user
        UINT32 PStateBit;   // used by RM
        INT32  FreqOffsetKHz; // used by user
        map<UINT32, ClkRange> ClkVals;       // key of map is RM's clk domain mask
        map<UINT32, UINT32> ClkFlags;
        map<UINT32, bool> IsClkIntersect;
        map<UINT32, VoltInfo> VoltageVals;   // key of map is RM's volt domain mask
    };

    typedef vector<PStateInfo> PStateInfos;
    PStateInfos m_OrgPStates;
    PStateInfos m_CachedPStates;

    PerfPoints m_PerfPoints;

    enum VoltTuningWriteMode
    {
        writeUnconditionally,
        writeIfChanged,
        writeIfPStateLocked
    };

    // Actual value in the VTD is the sum of all 4 values below
    enum VoltTuneOffsetType {
        VTUNE_DEFAULT,    // initial value requested by operating system
        VTUNE_GLOBAL,     // across all perfpoints in all pstates
        VTUNE_PSTATE,     // across all perfpoints in a given pstate
        VTUNE_PERFPOINT,  // the current perfpoint offset.
        VTUNE_NUM_TYPES
    };
    INT32 m_VoltTuningOffsetsMv[VTUNE_NUM_TYPES];
    // What additional offset to apply for a given pstate
    INT32 m_PStateVoltTuningOffsetsMv[MAX_NUM_PSTATES];

    virtual RC SetVoltTuningParams(VoltTuningWriteMode m);

    // key off MODS's clock domain enum
    map<Gpu::ClkDomain, ClkDomainInfo> m_ClockDomains;

    // RM supported clock domains
    map<Gpu::ClkDomain, ClkDomainInfo> m_RmClockDomains;

    //! Assign each set bit of domains to the domain field in T
    //! for each element in domInfoBuffer. T *must* have a "domain" field
    //! For example, will work with LW2080_CTRL_PERF_CLK_DOM_INFO
    template<typename T>
    void AssignDomainsToTypeWithDomainBuffer
    (
        UINT32 domains,
        vector<T> *domInfoBuffer
    )
    {
        domInfoBuffer->clear();
        for (UINT32 BitNum = 0; BitNum < 32; ++BitNum)
        {
            UINT32 BitMask = 1 << BitNum;
            if (domains & BitMask)
            {
                T domInfo = {};
                domInfo.domain = BitMask;
                domInfoBuffer->push_back(domInfo);
            }
        }
    }

    struct VfEntryTbl
    {
        Gpu::ClkDomain  Domain;
        UINT32          PStateMask;
        VfPoints        Points; //! Will use 2x frequencies when applicable

        VfEntryTbl() : Domain(Gpu::ClkUnkUnsp), PStateMask(0) {};
    };
    typedef vector<VfEntryTbl> VfEntryTbls;
    VfEntryTbls m_VfEntryTbls;

    const VfEntryTbl & GetVfEntryTbl
    (
        UINT32 pstateNum,
        Gpu::ClkDomain domain
    ) const;

    vector<Callbacks> m_Callbacks;

    void* GetMutex(){return m_pMutex;}

    RC GetPStateIdx(UINT32 PStateNum, UINT32 *pPStateIdx) const;
    RC GetPStatesInfo();

    RC DumpVPStateInfo();
    virtual RC ClockOfVPState
    (
        int idx,
        bool isSymbolicIdx,
        UINT64 * pRtnGpc2Hz,
        UINT32 * pRtnPState
    );
    virtual RC GetTDPGpcHz(UINT64* tdpGpcHz);

    virtual LwU32 GetVPStateId(GpcPerfMode m) const;

    GpcPerfMode EffectiveGpcPerfMode(const PerfPoint & perfPoint) const;

    // Internal API for setting pstate tables in RM
    virtual RC SetPStateTableInternal(UINT32 PStateNum,
                        const vector<LW2080_CTRL_PERF_CLK_DOM_INFO> & Clks,
                        const vector<LW2080_CTRL_PERF_VOLT_DOM_INFO>& Voltages);

    //! Call RM to obtain the current voltage for the given domain
    virtual RC GetVoltageAtPStateNoCache(UINT32 PStateNum,
                                         Gpu::VoltageDomain Domain,
                                         UINT32 *pVoltageUv) = 0;

    //! Call LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO and use the results to
    //! update the corresponding dynamic info in pstatesInfo
    virtual RC GetLwrrentPerfValuesNoCache(PStateInfos *pPstatesInfo) = 0;

    //! Actually perform perf point switch
    virtual RC InnerSetPerfPoint(const PerfPoint &Setting);
    RC SetPStateLock(const PerfPoint &Setting, PStateLockType Lock);

    RC SetPerfPointPcieSettings(const PerfPoint& Setting) const;

    RC GetForcedDefaultPerfPoint
    (
        UINT32 pstateNum,
        UINT32 fallback,
        PerfPoint *pSetting
    );

    RC MaybeUnlock(PStateLockType newLockType);

    //! Clears any locks that we lwrrently have on a current pstate
    //! Used before locking to a new pstate in order to relinquish old locks
    //! and at the end of a mods run.
    virtual RC ClearForcePStateEx(UINT32 Flags) = 0;

    //! Similar to above, but uses a different API to accomplish a similar task
    virtual RC ClearPerfLimits();

    virtual RC CheckVfSwitchTimeForGivenType(VfSwitchVal type)
    { return RC::UNSUPPORTED_FUNCTION; }

private:
    void          *m_pMutex;
    PStateChangeCounter m_PStateChangeCounter;
    bool           m_Owned;
    bool           m_ForceUnlockBeforeLock;
    bool           m_ClearPerfLevelsForUi;
    bool           m_DispVminCapActive;
    UINT32         m_RequiredDispClkHz;

    set <UINT32>  m_DIPStates; //Use to override DI PStates

    //! Holds the amount of time SetPerfPoint is allowed to take before a timeout error
    map <Perf::VfSwitchVal, UINT32>     m_AllowedPerfSwitchTimeNs;

    virtual RC ApplyFreqTuneOffset(INT32 FreqOffsetKhz, UINT32 PStateNum) = 0;

    //! Given a PState number, get the current clock and voltage values
    //! Updates m_CachedPStates if the cache is dirty.
    RC GetLwrrentPerfValues(UINT32 PStateNum);

    virtual RC ParseVfTable() = 0;

    RC ApplyRmPerfLimits(const PerfPoint &Setting);
    RC ApplyDispVminCap(UINT32 PStateNum);
    PStateLockType LockTypeOfPerfPoint(const PerfPoint & perfPoint) const;
    bool IsGpcClkAtIntersect(const PerfPoint &Setting) const;
    bool IsClockAtIntersect(UINT32 PStateNum, Gpu::ClkDomain Domain);
    bool IsClockRequestedAtIntersect
    (
        const PerfPoint &Setting,
        Gpu::ClkDomain Domain
    ) const;
    RC GetVPStateInfo
    (
        LwU32 VPStateId,
        LwU32 VPStateIdx,
        LW2080_CTRL_PERF_GET_VIRTUAL_PSTATE_INFO_PARAMS * pParams,
        vector<LW2080_CTRL_PERF_CLK_DOM_INFO> * pClkDomInfos
    );
    virtual RC GetVirtualPerfPoints(PerfPoints *points) = 0;

    //! Brief: What's retrieved in init
    // 1) Number of PStates - each of the PState bits
    // 2) The supported clock and voltage domains
    // 3) PState 2.0 related info:
    //    * the type of clock (fixed/pstate/ratio)
    //    * the master domain (if it is ratio)
    //    * Min/Max (if it is pstate/ratio)
    virtual RC Init() = 0;

    virtual RC GenerateStandardPerfPoints
    (
        UINT64 fastestSafeGpc2Hz,
        const PStateInfos &pstateInfo,
        PerfPoints *pPoints
    ) = 0;

    // NOTE: this function returns 2x clocks when available
    virtual RC GetRatiodClocks
    (
        UINT32 PStateNum,
        UINT64 FreqHz,
        vector<LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo
    );
    void FetchDefaultPStateTable
    (
        UINT32 pstateNum,
        vector<LW2080_CTRL_PERF_CLK_DOM_INFO> *pClkInfo,
        vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> *pVoltInfo
    );

    enum { UNKNOWN_VOLTAGE_OFFSET = 2*1000*1000 };  // 2V, out of range
    LW2080_CTRL_CMD_PMGR_VOLTAGE_REQUEST_ARBITER_CONTROL_PARAMS m_ArbiterRequestParams;
    RC GetArbiterRequestParams();
    bool  m_IsVminFloorRequestPending;
    VminFloorMode m_VminFloorMode;

    static constexpr UINT32 TegraIndexUnset = numeric_limits<UINT32>::max();
    UINT32 m_TegraIndex = TegraIndexUnset;
    string m_TegraName;

    //! Call the appropriate RM control calls to lock to the specified pstate
    virtual RC InnerSetPStateLock
    (
        const PerfPoint &Setting,
        PStateLockType Lock
    ) = 0;
};

class PStateOwner
{
public:
    PStateOwner(): m_pPStateOwnerRefCnt(0), m_pGpuSubdev(nullptr){}
    ~PStateOwner() { ReleasePStates(); }
    RC ClaimPStates(GpuSubdevice* pGpuSubdevice);
    void ReleasePStates();

private:
    // non-copyable
    PStateOwner(const PStateOwner&);
    PStateOwner& operator=(const PStateOwner&);
    UINT32 m_pPStateOwnerRefCnt;
    GpuSubdevice* m_pGpuSubdev;
};

#endif
