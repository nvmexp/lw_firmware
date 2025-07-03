/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file perfpunish.cpp
//! \brief Implements the switching logic of PerfPunish
//!
//! This file implements the core PerfPunish algorithm, which rapidly switches
//! pstate/gpcclk while running another test. The logic that creates and runs
//! the foreground test(s) and ilwokes PerfPunish can be found in perfpunish.js.
//!

#include "core/include/abort.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/perf/avfssub.h"
#include "gpu/perf/perfprofiler.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfsub_30.h"
#include "gpu/perf/perfsub_40.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/voltsub.h"
#include "gpu/tests/gputest.h"

#include <atomic>

static const UINT32 s_ClkPropTopIDs[] =
{
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_GRAPHICS,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_COMPUTE,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_0,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_1,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_2,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_3,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_4,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_5,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_6,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_7,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_8,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_9,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_10,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_11,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_12,
    LW2080_CTRL_CLK_CLK_PROP_TOP_ID_RSVD_13
};

//! \brief Punishes the GPU with gpcclk/pstate switches
class PerfPunish : public GpuTest
{
public:
    PerfPunish();
    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC SetDefaultsForPicker(UINT32 idx) override;

    //! \brief Starts running the Punisher thread
    RC Start();

    //! \brief Stops/joins the Punisher thread
    RC Stop();

    //! \brief Returns true if the Punisher thread is being kept alive
    bool GetIsPunishing() const;

    //! \brief Returns the amount of time the Punisher thread has been running
    FLOAT64 GetElapsedTimeMs();

    SETGET_PROP(PunishTimeMs, UINT32);
    SETGET_PROP(QueryPeriodMs, FLOAT32);
    SETGET_PROP(IgnoreRMGpcclkLimits, bool);
    SETGET_PROP(ThrashPowerCapping, bool);
    SETGET_PROP(DumpCoverage, bool);
    SETGET_PROP(MaxGpcFreqkHz, UINT32);
    SETGET_PROP(SwitchClkPropTopology, bool);
    SETGET_PROP(SendDramLimitStrict, bool);
    SETGET_PROP(RangeMinUV, UINT32);
    SETGET_PROP(RangeMaxUV, UINT32);
    SETGET_PROP(MaxModsVfSwitchTimeUs, UINT64);
    SETGET_PROP(MaxPmuVfSwitchTimeUs, UINT64);
    SETGET_PROP(MeasureVfSwitchTime, bool);

private:
    FancyPicker::FpContext *m_pFpCtx = nullptr;
    Perf* m_pPerf = nullptr;
    Volt3x* m_pVolt3x = nullptr;
    Avfs* m_pAvfs = nullptr;

    // Perf state
    PStateOwner m_PStateOwner = {};
    Perf::PerfPoint m_OrigPerfPoint = {};
    Perf::PStateLockType m_OrigPSLock = Perf::DefaultLock;
    vector<Gpu::ClkDomain> m_ClfcClkDoms;
    set<Gpu::SplitVoltageDomain> m_ClvcVoltDoms = {};
    vector<UINT32> m_PStates = {};
    UINT32 m_OrigClkTopID;

    // Utility class for reading/printing the clocks/voltages/pstate
    PerfProfiler m_PerfProfiler = {};

    using PStateNum = UINT32;
    using PStateFreqkHz = UINT32;

    // Temporary state that should be cleared before each Start()

    // List of all gpcclk available at each pstate
    map<PStateNum, vector<PStateFreqkHz>> m_PStateFreqskHz = {};

    // For each pstate/voltage_domain combination, keep track of which gpcclk
    // frequency corresponds to PState Vmin.
    map<tuple<PStateNum, Gpu::SplitVoltageDomain>, UINT32> m_VminGpcclkMHz = {};

    // Punisher thread state
    Tasker::ThreadID m_PunisherThreadId = Tasker::NULL_THREAD;
    atomic<bool> m_IsPunishing;
    StickyRC m_StickyRC = OK;
    UINT64 m_StartTime = 0; // Time Punisher thread started
    UINT64 m_LastTime = 0; // Last time we queried RM for Vmin/capping limits

    // Keep track of which PState we are lwrrently in and how long we will be there
    PStateNum m_LwrPState = Perf::ILWALID_PSTATE;
    UINT32 m_VfSwitchesPerPState = 1;
    UINT32 m_LwrGpcclkkHz = 0;

    // Keep track of the RM power and thermal gpcclk limits
    UINT32 m_PowerLimitkHz = 0;
    UINT32 m_ThermLimitkHz = 0;
    bool m_LastSwitchAbovePowerCap = false;

    // End temporary state

    // Min & Max GPC frequency above VMIN that are
    // in range of frequencies where PerfPunish needs to spend more time
    UINT32 m_RangeGPCMinMHz = 0;
    UINT32 m_RangeGPCMaxMHz = 0;

    // Test arguments
    UINT32 m_PunishTimeMs = 0; // How long to run Punisher()
    FLOAT32 m_QueryPeriodMs = 400.0f; // Time between RM Vmin/capping queries
    bool m_IgnoreRMGpcclkLimits = false; // Do not use RM power/thermal limits when deciding gpcclk
    bool m_ThrashPowerCapping = true; // Allow MODS to briefly violate power capping
    bool m_DumpCoverage = false; // Print summary of all clock/voltage/pstate switches
    UINT32 m_MaxGpcFreqkHz = 0; // Limits the maximum gpcclk frequency
    bool m_SwitchClkPropTopology = false; // Allow PerfPunish to switch clock topology
    bool m_SendDramLimitStrict = true; // Allow PerfPunish to send a DRAM strict limit for GpcPerf_EXPLICIT
    UINT32 m_RangeMinUV = 0; // Min voltage above VIM that is considered in the low margin region
    UINT32 m_RangeMaxUV = 0; // Max Voltage above VMIN that is considered in the low margin region
    UINT64 m_MaxModsVfSwitchTimeUs = 0; // Limit set for the time taken by MODS for a VF switch
    UINT64 m_MaxPmuVfSwitchTimeUs = 0; // Limit set for the time taken by PMU for a VF switch
    bool m_MeasureVfSwitchTime = false; // Log the VF switch time for the VF switches done
    UINT32 m_OrgVfSwitchTimePriority = 0; // Original VF switch time priority

    //! \brief Calls the Punisher thread
    static void PunishThread(void *Arg);

    //! \brief The Punisher thread - switches gpcclk/pstate
    void Punisher();

    //! \brief Used to start/stop the Punisher thread
    void SetIsPunishing(bool val);

    //! \brief Picks m_LwrPState and m_VfSwitchesPerPState
    RC PickPStateSettings(UINT32 vminPick);

    //! \brief Pick a gpcclk frequency using some fancy algorithm
    //!
    //! \param intPick Whether or not to intersect to pstate/gpcclk together
    //! \param vminPick Should we pick gpcclk above or below Vmin?
    //! \param outFreqkHz Picked gpcclk freq
    RC PickGpcclk(UINT32 intPick, UINT32 vminPick);

    //! \brief Returns a PerfPoint based on gpcclk, pstate, and intersect picks
    Perf::PerfPoint MakePerfPoint(UINT32 intPick, UINT32 gpcclk_kHz, bool inRange);

    //! \brief Finds all gpcclk frequencies available for each pstate
    RC FindPStateFreqs();

    //! \brief Query the current Vmin and power/thermal-capping limits
    RC QueryRMLimits();

    //! \brief Query the current Vmin and power/thermal-capping limits
    // Pstate version 40
    RC QueryRMLimitsPS40();

    RC SetupClkTopToTest();

    RC SetClockTopology(UINT32 clkTopPick);

    RC SaveRestoreClkTop(bool save);

    //! \brief QueryRMLimits if QueryPeriodMs elapsed
    RC QueryRMIfTime();

    //! \brief Clears the temporary state that the PerfPunish algorithm generates
    void ResetTemporaryState();

    //! \brief Clears state that persists between multiple rounds of Punisher()
    void ClearPersistentState();

    //! \brief Needed to enable/disable CLFC to prevent RM from switching LWVDD behind our back
    RC SetClfcEnable(bool bEnable) const;

    //! \brief Helper function to run some JS function like PerfPunish.Setup()
    RC RunJsFunc(const char* jsFuncName);
};

PerfPunish::PerfPunish()
{
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_PERFPUNISH_NUM_PICKERS);
    m_IsPunishing.store(false);
}

bool PerfPunish::IsSupported()
{
    m_pPerf = GetBoundGpuSubdevice()->GetPerf();
    m_pVolt3x = GetBoundGpuSubdevice()->GetVolt3x();
    m_pAvfs = GetBoundGpuSubdevice()->GetAvfs();

    if (!GpuTest::IsSupported())
        return false;
    if (!(m_pVolt3x && m_pVolt3x->IsInitialized()))
        return false;
    if (!(m_pPerf && m_pPerf->IsPState30Supported() && m_pPerf->HasPStates()))
        return false;
    if (!m_pAvfs)
        return false;
    if (GetBoundGpuSubdevice()->IsEmOrSim())
        return false;
    if (!m_pPerf->IsArbitrated(Gpu::ClkGpc))
        return false;

    return true;
}

RC PerfPunish::Setup()
{
    RC rc;


    CHECK_RC(GpuTest::Setup());
    if (m_RangeMinUV != 0 && m_RangeMaxUV < m_RangeMinUV)
    {
        Printf(Tee::PriError, "RangeMaxUV should be greater than RangeMinUV \n");
        return RC::BAD_PARAMETER;
    }

    // Init random seed and other stuff
    ResetTemporaryState();

    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));

    // Save the status of all CLFCs/CLVCs so that we can restore them correctly later
    CHECK_RC(m_pAvfs->GetEnabledFreqControllers(&m_ClfcClkDoms));
    CHECK_RC(SetClfcEnable(false));
    CHECK_RC(m_pAvfs->GetEnabledVoltControllers(&m_ClvcVoltDoms));
    CHECK_RC(m_pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, false));

    // Save original Clock Topology
    if (m_SwitchClkPropTopology)
    {
        CHECK_RC(SaveRestoreClkTop(true));
    }

    m_OrigPSLock = m_pPerf->GetPStateLockType();

    CHECK_RC(FindPStateFreqs());

    CHECK_RC(RunJsFunc("JsFuncSetup"));

    // Set FancyPickers that depend on GpuSubdevice properties
    FancyPicker *pPicker = &(*GetFancyPickerArray())[FPK_PERFPUNISH_PSTATE_NUM];
    if (!pPicker->WasSetFromJs())
    {
        pPicker->ConfigRandom();
        for (const auto pstate : m_PStates)
        {
            pPicker->AddRandItem(1, pstate);
        }
        pPicker->CompileRandom();
    }

    pPicker = &(*GetFancyPickerArray())[FPK_PERFPUNISH_INTERSECT];
    if (!pPicker->WasSetFromJs())
    {
        pPicker->ConfigRandom();
        pPicker->AddRandItem(70, FPK_PERFPUNISH_INTERSECT_disabled);

        // TODO : Add xbarclk and consider MSVDD
        // GPCCLK has its Vmin only on LWVDD in PState 4.0
        if (m_pPerf->IsPState40Supported())
        {
            // We only support Intesect with lwvdd on PS40
            pPicker->AddRandItem(30, FPK_PERFPUNISH_INTERSECT_lwvdd);
        }
        else
        {

            for (const auto& voltDom : m_pVolt3x->GetAvailableDomains())
            {
                if (voltDom == Gpu::VOLTAGE_LOGIC)
                {
                    pPicker->AddRandItem(30, FPK_PERFPUNISH_INTERSECT_lwvdd);
                }
                else if (voltDom == Gpu::VOLTAGE_SRAM)
                {
                    pPicker->AddRandItem(20, FPK_PERFPUNISH_INTERSECT_lwvdds);
                }
                else
                {
                    return RC::ILWALID_VOLTAGE_DOMAIN;
                }
            }
        }
        pPicker->CompileRandom();
    }

    // Setup Picker for Clock Prop Topology switching
    if (m_pPerf->IsPState40Supported() && m_SwitchClkPropTopology)
    {
        CHECK_RC(SetupClkTopToTest());
    }

    if (m_pPerf->IsPState40Supported() && m_MeasureVfSwitchTime)
    {
        Perf40* pPerf40 = dynamic_cast<Perf40*>(m_pPerf);
        MASSERT(pPerf40);
        CHECK_RC(pPerf40->GetVfSwitchTimePriority(&m_OrgVfSwitchTimePriority));
        CHECK_RC(m_pPerf->SetVfSwitchTimePriority(Tee::PriNormal));
    }
    return rc;
}

RC PerfPunish::Run()
{
    return RunJsFunc("JsFuncRun");
}

RC PerfPunish::Cleanup()
{
    StickyRC stickyRc;

    stickyRc = m_pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, true);
    stickyRc = SetClfcEnable(true);

    m_PStateOwner.ReleasePStates();

    ClearPersistentState();

    stickyRc = GpuTest::Cleanup();

    return stickyRc;
}

void PerfPunish::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "PerfPunish Js Properties:\n");

    Printf(pri, "\t%-31s %u\n", "PunishTimeMs:", m_PunishTimeMs);
    Printf(pri, "\t%-31s %.1f\n", "QueryPeriodMs:", m_QueryPeriodMs);
    Printf(pri, "\t%-31s %s\n", "IgnoreRMGpcclkLimits:", m_IgnoreRMGpcclkLimits ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "ThrashPowerCapping:", m_ThrashPowerCapping ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "DumpCoverage:", m_DumpCoverage ? "true" : "false");
    Printf(pri, "\t%-31s %u\n", "MaxGpcFreqkHz:", m_MaxGpcFreqkHz);
    Printf(pri, "\t%-31s %u\n", "RangeMinUV:", m_RangeMinUV);
    Printf(pri, "\t%-31s %u\n", "RangeMaxUV:", m_RangeMaxUV);
    Printf(pri, "\t%-31s %llu\n", "MaxModsVfSwitchTimeUs:", m_MaxModsVfSwitchTimeUs);
    Printf(pri, "\t%-31s %llu\n", "MaxPmuVfSwitchTimeUs:", m_MaxPmuVfSwitchTimeUs);
    Printf(pri, "\t%-31s %s\n", "MeasureVfSwitchTime:", m_MeasureVfSwitchTime ? "true": "false");
}

RC PerfPunish::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *pPicker = &(*GetFancyPickerArray())[idx];
    MASSERT(pPicker);
    RC rc;

    CHECK_RC(pPicker->ConfigRandom());

    switch (idx)
    {
        case FPK_PERFPUNISH_VF_SWITCHES_PER_PSTATE:
            pPicker->AddRandItem(70, 1);
            pPicker->AddRandItem(20, 2);
            pPicker->AddRandItem(8,  3);
            pPicker->AddRandItem(2,  4);
            break;

        case FPK_PERFPUNISH_PSTATE_NUM:
            pPicker->AddRandItem(1, 0);
            break;

        case FPK_PERFPUNISH_INTERSECT:
            pPicker->AddRandItem(1, FPK_PERFPUNISH_INTERSECT_disabled);
            break;

        case FPK_PERFPUNISH_VMIN:
            pPicker->AddRandItem(80, FPK_PERFPUNISH_VMIN_above_or_equal);
            pPicker->AddRandItem(20, FPK_PERFPUNISH_VMIN_below);
            break;

        case FPK_PERFPUNISH_TIMING_US:
            pPicker->AddRandRange(1, 5000, 7000);
            break;

        case FPK_PERFPUNISH_GPCCLK:
            pPicker->AddRandItem(0, 0);
            break;

        case FPK_PERFPUNISH_CLK_TOPOLOGY:
            pPicker->AddRandItem(1, LW2080_CTRL_CLK_CLK_PROP_TOP_ID_GRAPHICS);
            break;

        case FPK_PERFPUNISH_RANGE:
            pPicker->AddRandItem(80, FPK_PERFPUNISH_RANGE_IN);
            pPicker->AddRandItem(20, FPK_PERFPUNISH_RANGE_OUT);
            break;

        default:
            MASSERT(!"Unknown PerfPunish picker");
            return RC::SOFTWARE_ERROR;
    }

    pPicker->CompileRandom();

    return rc;
}

void PerfPunish::PunishThread(void *Arg)
{
    MASSERT(Arg);
    PerfPunish* pPerfPunish = static_cast<PerfPunish*>(Arg);
    pPerfPunish->Punisher();
}

RC PerfPunish::Start()
{
    RC rc;

    if (GetIsPunishing())
    {
        MASSERT(!"Punisher thread already started");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_PerfProfiler.Setup(m_pPerf->GetGpuSubdevice()));

    if (m_ThrashPowerCapping)
    {
        CHECK_RC(m_pPerf->SetForcedPStateLockType(Perf::HardLock));
    }

    if (m_pPerf->IsPState40Supported())
    {
        CHECK_RC(QueryRMLimitsPS40());
    }
    else
    {
        CHECK_RC(QueryRMLimits());
    }
    m_LastTime = Xp::QueryPerformanceCounter();
    SetIsPunishing(true);

    VerbosePrintf("Creating Punisher thread\n");
    if (m_PunisherThreadId != Tasker::NULL_THREAD)
        return RC::SOFTWARE_ERROR;

    m_PunisherThreadId = Tasker::CreateThread(PerfPunish::PunishThread, this, 0, "Punisher");
    if (m_PunisherThreadId == Tasker::NULL_THREAD)
    {
        Printf(Tee::PriError, "Failed to create Punisher thread\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC PerfPunish::Stop()
{
    StickyRC stickyRC;

    VerbosePrintf("Destroying Punisher thread\n");
    SetIsPunishing(false);
    stickyRC = Tasker::Join(m_PunisherThreadId);
    stickyRC = m_StickyRC;

    // If we saved a valid PerfPoint, restore it
    if (m_OrigPerfPoint.PStateNum != Perf::ILWALID_PSTATE &&
        m_OrigPerfPoint.SpecialPt != Perf::GpcPerf_EXPLICIT)
    {
        stickyRC = m_pPerf->SetPerfPoint(m_OrigPerfPoint);
    }
    stickyRC = m_pPerf->SetForcedPStateLockType(m_OrigPSLock);

    // Restore original Clock Topology
    if (m_SwitchClkPropTopology)
    {
        stickyRC = SaveRestoreClkTop(false);
    }

    Tee::Priority statsPrintPri = m_DumpCoverage ? Tee::PriNormal : GetVerbosePrintPri();
    stickyRC = m_PerfProfiler.CheckVfSwitchLatencyTime(statsPrintPri);
    m_PerfProfiler.PrintSummary(statsPrintPri);
    SetPerformanceMetric(ENCJSENT("SwitchesPerSec"), m_PerfProfiler.GetSwitchesPerSecond());
    stickyRC = m_PerfProfiler.Cleanup();

    ResetTemporaryState();

    if (m_pPerf->IsPState40Supported() && m_MeasureVfSwitchTime)
    {
        stickyRC = m_pPerf->SetVfSwitchTimePriority(m_OrgVfSwitchTimePriority);
    }

    return stickyRC;
}

void PerfPunish::Punisher()
{
    Tasker::DetachThread detach;

    DEFER
    {
        SetIsPunishing(false);
    };

    FancyPickerArray &fpk = *GetFancyPickerArray();
    m_StartTime = Xp::QueryPerformanceCounter();

    while (GetIsPunishing())
    {
        m_StickyRC = QueryRMIfTime();

        const UINT32 intersectPick = fpk[FPK_PERFPUNISH_INTERSECT].Pick();
        UINT32 vminPick = fpk[FPK_PERFPUNISH_VMIN].Pick();

        if (m_RangeGPCMinMHz != 0)
        {
            vminPick = fpk[FPK_PERFPUNISH_RANGE].Pick();
        }

        if (--m_VfSwitchesPerPState == 0)
        {
            m_StickyRC = PickPStateSettings(vminPick);
        }

        m_StickyRC = PickGpcclk(intersectPick, vminPick);
        Perf::PerfPoint perfPt = MakePerfPoint(intersectPick, m_LwrGpcclkkHz,
        vminPick == FPK_PERFPUNISH_RANGE_IN);

        // Send a DRAM Strict Limit to RM for GpcPerf_EXPLICIT
        if (m_pPerf->IsPState40Supported() && m_SendDramLimitStrict)            
        {            
            Perf::ClkRange lwrDramRange;
            m_StickyRC = m_pPerf->GetClockRange(m_LwrPState, Gpu::ClkM, &lwrDramRange);
            perfPt.Clks[Gpu::ClkM] = Perf::ClkSetting(Gpu::ClkM,
                                                      lwrDramRange.MinKHz * 1000ULL,
                                                      Perf::FORCE_PERF_LIMIT_MIN);
            // TODO - Consider setting a Perf::SoftLock
        }

        m_StickyRC = m_pPerf->SetPerfPoint(perfPt);

        if (m_pPerf->IsPState40Supported() && m_SwitchClkPropTopology)
        {
            const UINT32 clkTopPick = fpk[FPK_PERFPUNISH_CLK_TOPOLOGY].Pick();
            m_StickyRC = SetClockTopology(clkTopPick);
        }

        m_PerfProfiler.SetGPCRange(m_RangeGPCMinMHz, m_RangeGPCMaxMHz);
        if (m_pPerf->IsPState40Supported())
        {
            Perf40* pPerf40 = dynamic_cast<Perf40*>(m_pPerf);
            if (pPerf40 != nullptr)
            {
                UINT32 priority = 0;
                pPerf40->GetVfSwitchTimePriority(&priority);
                if (priority != Tee::PriNone)
                {
                    m_PerfProfiler.RecordVfSwitchLatencyTime(
                        pPerf40->GetLwrrentVfSwitchTime(Perf::VfSwitchVal::Mods), 
                        pPerf40->GetLwrrentVfSwitchTime(Perf::VfSwitchVal::Pmu),
                        pPerf40->GetPmuTotalBuildTimeUs(),
                        pPerf40->GetPmuTotalExecTimeUs());
                    m_PerfProfiler.SetMaxVfSwitchLatencyTime(
                        m_MaxModsVfSwitchTimeUs, 
                        m_MaxPmuVfSwitchTimeUs);
                }
            }
        }
        m_StickyRC = m_PerfProfiler.Sample();
        m_PerfProfiler.PrintSample(GetVerbosePrintPri());

        const UINT32 sleepUs = fpk[FPK_PERFPUNISH_TIMING_US].Pick();
        Tasker::Sleep(static_cast<FLOAT64>(sleepUs) / 1000.0);
    }
}

void PerfPunish::SetIsPunishing(bool val)
{
    m_IsPunishing.store(val);
}

bool PerfPunish::GetIsPunishing() const
{
    return m_IsPunishing.load() && Abort::Check() == OK && m_StickyRC == OK;
}

FLOAT64 PerfPunish::GetElapsedTimeMs()
{
    if (!m_StartTime)
        return 0.0;

    return static_cast<FLOAT64>(Xp::QueryPerformanceCounter() - m_StartTime) /
        Xp::QueryPerformanceFrequency() * 1000.0;
}

RC PerfPunish::PickPStateSettings(UINT32 vminPick)
{
    RC rc;

    FancyPickerArray &fpk = *GetFancyPickerArray();

    UINT32 numAvailPStates;
    CHECK_RC(fpk[FPK_PERFPUNISH_PSTATE_NUM].GetNumItems(&numAvailPStates));
    UINT32 newPState;
    UINT32 numTries = 100;
    // SSG request is to supply RangeGPCMinUV and RangeGPCMaxUV. MODS will constantly
    // find RangeGPCMinMHz and RangeGPCMaxMHZ. If SSG tells that they need frequencies
    // in range to be picked 80% of the time and out of range to be picked 20% of the time.
    // Then we need to ensure that the PState that has the frequencies in range gets picked 
    // 80% of the time and PState with frequencies out of range gets picked 20% of the time.
    // That's why when picking the PState I check if the Maximum Frequency of the picked PState
    // is greater than the MinimunRangeFrequency if vminPick is In Range
    // Based on the results, m_PStateFreqskHz is in asceding order of frequencies
    if (vminPick == FPK_PERFPUNISH_RANGE_IN)
    {
        do
        {
            newPState = fpk[FPK_PERFPUNISH_PSTATE_NUM].Pick();
        } while ((newPState == m_LwrPState && numAvailPStates > 1 && --numTries)
                || m_PStateFreqskHz[newPState].back() < m_RangeGPCMinMHz*1e3);
    }
    else
    {
        do
        {
            newPState = fpk[FPK_PERFPUNISH_PSTATE_NUM].Pick();
        } while (newPState == m_LwrPState && numAvailPStates > 1 && --numTries);
    }

    m_LwrPState = newPState;

    m_VfSwitchesPerPState = fpk[FPK_PERFPUNISH_VF_SWITCHES_PER_PSTATE].Pick();

    // Avoid scenario where a pstate only supports one clock frequency and we
    // "switch" to it multiple times in a row.
    if (m_PStateFreqskHz[m_LwrPState].size() == 1)
    {
        m_VfSwitchesPerPState = 1;
    }

    return rc;
}

// Pick a gpcclk frequency in the range [lowBoundkHz, highBoundkHz]
RC PerfPunish::PickGpcclk(UINT32 intPick, UINT32 vminPick)
{
    RC rc;

    FancyPickerArray &fpk = *GetFancyPickerArray();
    fpk[FPK_PERFPUNISH_GPCCLK].ConfigRandom();

    UINT32 lowBoundkHz = 0;
    UINT32 highBoundkHz = UINT32(-1);
    const Gpu::SplitVoltageDomain voltDom = (intPick == FPK_PERFPUNISH_INTERSECT_lwvdds ?
        Gpu::VOLTAGE_SRAM : Gpu::VOLTAGE_LOGIC);

    const UINT32 VminFreqkHz = m_VminGpcclkMHz[make_tuple(m_LwrPState, voltDom)] * 1000;
    if (vminPick == FPK_PERFPUNISH_VMIN_below)
    {
        highBoundkHz = VminFreqkHz - 1;
    }
    else if (vminPick == FPK_PERFPUNISH_RANGE_IN || vminPick == FPK_PERFPUNISH_VMIN_above_or_equal)
    {
        lowBoundkHz = VminFreqkHz;
    }

    // TODO: Recheck the logic
    // m_LastSwitchAbovePowerCap is true only if m_PowerLimitkHz is true
    // m_PowerLimitkHz is positive and m_ThrashPowerCapping is false can never happen
    if (!m_IgnoreRMGpcclkLimits)
    {
        if ((m_LastSwitchAbovePowerCap || !m_ThrashPowerCapping) && m_PowerLimitkHz)
        {
            highBoundkHz = min(highBoundkHz, m_PowerLimitkHz);
        }
        if (m_ThermLimitkHz)
        {
            highBoundkHz = min(highBoundkHz, m_ThermLimitkHz);
        }
    }
    // Handle if RM power/thermal capping pushes us to Vmin or below.
    // Treat it as if we had picked VMIN_below.
    bool violentCapEvent = false;
    if (highBoundkHz < lowBoundkHz)
    {
        lowBoundkHz = 0;
        violentCapEvent = true;
    }

    if (m_MaxGpcFreqkHz)
    {
        highBoundkHz = min(highBoundkHz, m_MaxGpcFreqkHz);
        lowBoundkHz = min(lowBoundkHz, m_MaxGpcFreqkHz);
    }

    MASSERT(lowBoundkHz <= highBoundkHz);
    for (const auto pstateFreqkHz : m_PStateFreqskHz[m_LwrPState])
    {
        if ((pstateFreqkHz >= lowBoundkHz) && (pstateFreqkHz <= highBoundkHz))
        {
            if (m_RangeGPCMinMHz != 0)
            {
                if (vminPick == FPK_PERFPUNISH_RANGE_IN)
                {
                    if ((pstateFreqkHz >= m_RangeGPCMinMHz*1e3) &&
                        (pstateFreqkHz <= m_RangeGPCMaxMHz*1e3))
                    {
                        fpk[FPK_PERFPUNISH_GPCCLK].AddRandItem(1, pstateFreqkHz);
                    }
                }
                else if (vminPick == FPK_PERFPUNISH_RANGE_OUT)
                {
                    if ((pstateFreqkHz < m_RangeGPCMinMHz*1e3) ||
                        (pstateFreqkHz > m_RangeGPCMaxMHz*1e3))
                    {
                        fpk[FPK_PERFPUNISH_GPCCLK].AddRandItem(1, pstateFreqkHz);
                    }
                }
            }
            else
            {
                fpk[FPK_PERFPUNISH_GPCCLK].AddRandItem(1, pstateFreqkHz);
            }
        }
    }

    UINT32 numAvailGpcclk = 0;
    CHECK_RC(fpk[FPK_PERFPUNISH_GPCCLK].GetNumItems(&numAvailGpcclk));

    // All pstate points are above/equal Vmin when VminPick==below OR
    // All pstate points are below Vmin when VminPick==above_or_equal
    if (!numAvailGpcclk)
    {
        if (!violentCapEvent)
        {
            VerbosePrintf("All PState Points are below VMIN/RangeGPCMinMHz \n");
            // Safe points within RM Limits are below m_RangeGPCMinMHz
            if (m_RangeGPCMinMHz != 0)
            {
                for (const auto pstateFreqkHz : m_PStateFreqskHz[m_LwrPState])
                {
                    if ((pstateFreqkHz >= lowBoundkHz) && (pstateFreqkHz <= highBoundkHz))
                    {
                        fpk[FPK_PERFPUNISH_GPCCLK].AddRandItem(1, pstateFreqkHz);
                    }
                }
            }
            CHECK_RC(fpk[FPK_PERFPUNISH_GPCCLK].GetNumItems(&numAvailGpcclk));
            if (!numAvailGpcclk)
            {
                for (const auto pstateFreqkHz : m_PStateFreqskHz[m_LwrPState])
                {
                    fpk[FPK_PERFPUNISH_GPCCLK].AddRandItem(1, pstateFreqkHz);
                }
            }
        }
        else
        {
            // Power or thermal capping pushed us to a gpcclk that is below the
            // current pstate range. Rather than disturbing the pstate fancy
            // picker sequence, let's abort. This situation shouldn't happen.
            // If it does, it is probably unsafe to continue.
            Printf(Tee::PriError, "Unsafe to continue because of Power or Thermal Capping \n");
            return RC::CANNOT_SET_GRAPHICS_CLOCK;
        }
    }
    fpk[FPK_PERFPUNISH_GPCCLK].CompileRandom();

    UINT32 numTries = 100;
    UINT32 newGpckHz;
    do
    {
        newGpckHz = fpk[FPK_PERFPUNISH_GPCCLK].Pick();
    } while (newGpckHz == m_LwrGpcclkkHz && --numTries);

    m_LwrGpcclkkHz = newGpckHz;
    MASSERT(m_LwrGpcclkkHz);

    if (m_PowerLimitkHz)
    {
        m_LastSwitchAbovePowerCap = (m_LwrGpcclkkHz > m_PowerLimitkHz);
    }

    return rc;
}

Perf::PerfPoint PerfPunish::MakePerfPoint(UINT32 intPick, UINT32 gpcclk_kHz, bool inRange)
{
    VerbosePrintf("Setting: pstate=%u,gpcclk_kHz=%u", m_LwrPState, gpcclk_kHz);
    switch (intPick)
    {
        case FPK_PERFPUNISH_INTERSECT_lwvdd:
            VerbosePrintf(",int_lwvdd");
            break;
        case FPK_PERFPUNISH_INTERSECT_lwvdds:
            VerbosePrintf(",int_lwvdds");
            break;
        default:
            break;
    }
    VerbosePrintf(", inrange = %s\n", inRange?"true":"false");

    if (intPick == FPK_PERFPUNISH_INTERSECT_disabled)
    {
        return Perf::PerfPoint(m_LwrPState, Perf::GpcPerf_EXPLICIT, gpcclk_kHz*1000);
    }

    set<Perf::IntersectSetting> intSettings;
    Perf::IntersectSetting is;

    is.Type = Perf::IntersectPState;
    is.VoltageDomain = (intPick == FPK_PERFPUNISH_INTERSECT_lwvdd ?
        Gpu::VOLTAGE_LOGIC : Gpu::VOLTAGE_SRAM);
    intSettings.insert(is);

    is.Type = Perf::IntersectVoltFrequency;
    is.Frequency.ClockDomain = Gpu::ClkGpc;
    is.Frequency.ValueKHz = gpcclk_kHz;
    intSettings.insert(is);

    return Perf::PerfPoint(m_LwrPState, Perf::GpcPerf_INTERSECT, 0, intSettings);
}

RC PerfPunish::FindPStateFreqs()
{
    RC rc;

    vector<UINT32> gpcclkFreqskHz;
    CHECK_RC(m_pPerf->GetFrequencieskHz(Gpu::ClkGpc, &gpcclkFreqskHz));

    CHECK_RC(m_pPerf->GetAvailablePStates(&m_PStates));
    MASSERT(!m_PStates.empty());

    Perf::ClkRange gpcclkRange;
    for (const auto pstate : m_PStates)
    {
        CHECK_RC(m_pPerf->GetClockRange(pstate, Gpu::ClkGpc, &gpcclkRange));

        for (const auto freqkHz : gpcclkFreqskHz)
        {
            if (freqkHz >= gpcclkRange.MinKHz && freqkHz <= gpcclkRange.MaxKHz)
            {
                m_PStateFreqskHz[pstate].push_back(freqkHz);
            }
        }
        if (m_PStateFreqskHz[pstate].empty())
        {
            // There were no gpcclk frequencies within the pstate range. Likely VBIOS issue.
            Printf(Tee::PriError, "Missing gpcclk at P%u\n", pstate);
            return RC::SOFTWARE_ERROR;
        }

    }

    return rc;
}

RC PerfPunish::QueryRMLimits()
{
    RC rc;

    VerbosePrintf("Querying RM Vmin and capping limits:\n");

    for (const auto voltDom : m_pVolt3x->GetAvailableDomains())
    {
        // Find the per-rail Vmin
        LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = {};
        CHECK_RC(m_pVolt3x->GetVoltRailStatusViaId(voltDom, &railStatus));

        UINT32 lastPStateVminuV = 0;
        UINT32 lastGpcclkMHz = 0;

        // Find Pstate Vmin
        for (const auto pstate : m_PStates)
        {
            UINT32 pstateVminuV = 0;
            UINT32 tempVoltuV = 0;
            for (INT32 clk = 0; clk < Gpu::ClkDomain_NUM; clk++)
            {
                Gpu::ClkDomain clkDom = static_cast<Gpu::ClkDomain>(clk);
                if (GetBoundGpuSubdevice()->HasDomain(clkDom) &&
                    m_pPerf->IsRmClockDomain(clkDom) &&
                    m_pPerf->ClkDomainType(clkDom) == Perf::DECOUPLED)
                {
                    Perf::ClkRange range;
                    CHECK_RC(m_pPerf->GetClockRange(pstate, clkDom, &range));
                    CHECK_RC(m_pPerf->VoltFreqLookup(
                        clkDom, voltDom, Perf::FreqToVolt, range.MinKHz/1000, &tempVoltuV));
                    if (tempVoltuV > pstateVminuV)
                    {
                        pstateVminuV = tempVoltuV;
                    }
                }
            }
            pstateVminuV = max(railStatus.vminLimituV, pstateVminuV);
            if (pstateVminuV != lastPStateVminuV)
            {
                lastPStateVminuV = pstateVminuV;

                // Find the gpcclk frequency that corresponds to pstate Vmin
                CHECK_RC(m_pPerf->VoltFreqLookup(
                    Gpu::ClkGpc, voltDom, Perf::VoltToFreq, pstateVminuV,
                    &lastGpcclkMHz));
            }
            m_VminGpcclkMHz[make_tuple(pstate, voltDom)] = lastGpcclkMHz;

            VerbosePrintf("  P%d_%s_Vmin=%uuV, gpcclk=%uMHz\n",
                pstate, PerfUtil::SplitVoltDomainToStr(voltDom), pstateVminuV,
                m_VminGpcclkMHz[make_tuple(pstate, voltDom)]);
        }
    }

    if (m_ThrashPowerCapping)
    {
        // Find power/thermal capping limits
        vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
        statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
        statuses[1].limitId = LW2080_CTRL_PERF_LIMIT_THERM_POLICY_DOM_GRP_1;
        CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, GetBoundGpuSubdevice()));

        const bool usingGpc2Clk = m_pPerf->IsRmClockDomain(Gpu::ClkGpc2);
        for (const auto& status : statuses)
        {
            if (status.output.bEnabled)
            {
                if (status.limitId == LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1)
                {
                    m_PowerLimitkHz = status.output.value / (usingGpc2Clk ? 2 : 1);
                }
                else
                {
                    m_ThermLimitkHz = status.output.value / (usingGpc2Clk ? 2 : 1);
                }
            }
        }

        if (m_PowerLimitkHz)
        {
            VerbosePrintf("  RM power limit kHz=%u\n", m_PowerLimitkHz);
        }
        if (m_ThermLimitkHz)
        {
            VerbosePrintf("  RM therm limit kHz=%u\n", m_ThermLimitkHz);
        }
    }

    return rc;
}

RC PerfPunish::SetupClkTopToTest()
{
    VerbosePrintf("Pstate 4.0 - Setting up Clock Topology Switching\n");

    if (!m_pPerf->IsPState40Supported())
    {
        return RC::SOFTWARE_ERROR;
    }

    FancyPicker *pPicker = &(*GetFancyPickerArray())[FPK_PERFPUNISH_CLK_TOPOLOGY];

    if (!pPicker->WasSetFromJs())
    {
        pPicker->ConfigRandom();
        Perf40* pPerf40 = dynamic_cast<Perf40*>(m_pPerf);
        // Get Clock Prop Topology Information
        const LW2080_CTRL_CLK_CLK_PROP_TOPS_INFO* clkPropTops = pPerf40->GetClkPropTopsInfo();

        bool foundTopology = false;
        for (UINT32 i = 0; i < NUMELEMS(s_ClkPropTopIDs); i++)
        {
            UINT32 weightEachId = 0;
            UINT32 j = 0;
            LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(j, &clkPropTops->super.objMask.super)
                const LW2080_CTRL_CLK_CLK_PROP_TOP_INFO &propTopInfo = clkPropTops->propTops[j];
                if (propTopInfo.topId == s_ClkPropTopIDs[i])
                {
                    weightEachId++;
                    foundTopology = true;
                    break;
                }
            LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

            // Add a topology to be picked randomly
            VerbosePrintf("Adding Clock Topology %d with weight %d to Picker\n",
                    s_ClkPropTopIDs[i], weightEachId);
            pPicker->AddRandItem(weightEachId, s_ClkPropTopIDs[i]);
        }
        // Throw an error if we reach here without finding a valid Topology
        if (!foundTopology)
        {
            Printf(Tee::PriError, "Missing Clock Topology Data\n");
            return RC::SOFTWARE_ERROR;
        }
        pPicker->CompileRandom();
    }

    return RC::OK;
}

RC PerfPunish::SetClockTopology(UINT32 clkTopPick)
{
    if (!m_pPerf->IsPState40Supported())
    {
        return RC::SOFTWARE_ERROR;
    }

    RC rc;

    // Get current topology
    LW2080_CTRL_CLK_CLK_PROP_TOPS_STATUS clkPropTopsStatus = {};
    Perf40* pPerf40 = dynamic_cast<Perf40*>(m_pPerf);
    CHECK_RC(pPerf40->GetClkPropTopStatus(&clkPropTopsStatus));

    if (clkTopPick != clkPropTopsStatus.activeTopId)
    {
        // Get control
        LW2080_CTRL_CLK_CLK_PROP_TOPS_CONTROL clkPropTopControl = {};
        CHECK_RC(pPerf40->GetClkPropTopControl(&clkPropTopControl));

        // Set control
        clkPropTopControl.activeTopIdForced = clkTopPick;
        VerbosePrintf("Setting Clock Topology %d \n", clkTopPick);
        CHECK_RC(pPerf40->SetClkPropTopControl(&clkPropTopControl));
    }
    else
    {
        VerbosePrintf("Clock Topology %d is already set. Skipped\n", clkTopPick);
    }

    return rc;
}

RC PerfPunish::SaveRestoreClkTop(bool save)
{
    if (!m_pPerf->IsPState40Supported())
    {
        return RC::OK;
    }

    RC rc;

    // Get current topology
    LW2080_CTRL_CLK_CLK_PROP_TOPS_STATUS clkPropTopsStatus = {};
    Perf40* pPerf40 = dynamic_cast<Perf40*>(m_pPerf);
    CHECK_RC(pPerf40->GetClkPropTopStatus(&clkPropTopsStatus));

    if (save)
    {
        // Save the original topology so that we can restore later
        m_OrigClkTopID = clkPropTopsStatus.activeTopId;
    }
    else if (m_OrigClkTopID != clkPropTopsStatus.activeTopId)
    {
        LW2080_CTRL_CLK_CLK_PROP_TOPS_CONTROL clkPropTopControl = {};
        CHECK_RC(pPerf40->GetClkPropTopControl(&clkPropTopControl));

        VerbosePrintf("Restoring Clock Topology %d \n", m_OrigClkTopID);
        clkPropTopControl.activeTopIdForced = m_OrigClkTopID;
        CHECK_RC(pPerf40->SetClkPropTopControl(&clkPropTopControl));
    }

    return rc;
}

RC PerfPunish::QueryRMLimitsPS40()
{
    RC rc;

    VerbosePrintf("Pstate 4.0 - Querying RM Vmin and capping limits:\n");

    if (!m_pPerf->IsPState40Supported())
    {
        MASSERT(!"Function not supported");
        return RC::SOFTWARE_ERROR;
    }

    // Find the per-rail Vmin
    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = {};
    CHECK_RC(m_pVolt3x->GetVoltRailStatusViaId(Gpu::VOLTAGE_LOGIC, &railStatus));

    UINT32 lastPStateVminuV = 0;
    UINT32 lastGpcclkMHz = 0;

    if (m_RangeMinUV != 0)
    {
        CHECK_RC(m_pPerf->VoltFreqLookup(Gpu::ClkGpc, Gpu::VOLTAGE_LOGIC,
                                        Perf::VoltToFreq, m_RangeMinUV,
                                        &m_RangeGPCMinMHz));
        CHECK_RC(m_pPerf->VoltFreqLookup(Gpu::ClkGpc, Gpu::VOLTAGE_LOGIC,
                                        Perf::VoltToFreq, m_RangeMaxUV,
                                        &m_RangeGPCMaxMHz));
        VerbosePrintf("RangeUV(Min/Max) = (%d/%d), RangeGPCMHz(Min/Max) = (%d/%d) \n",
                      m_RangeMinUV, m_RangeMaxUV, m_RangeGPCMinMHz, m_RangeGPCMaxMHz);
    }
    // Find Pstate Vmin
    for (const auto pstate : m_PStates)
    {
        UINT32 pstateVminuV = 0;
        UINT32 tempVoltuV = 0;
        if (GetBoundGpuSubdevice()->HasDomain(Gpu::ClkGpc) &&
            m_pPerf->IsRmClockDomain(Gpu::ClkGpc) &&
            m_pPerf->ClkDomainType(Gpu::ClkGpc) == Perf::PROGRAMMABLE)
        {
            Perf::ClkRange range;
            CHECK_RC(m_pPerf->GetClockRange(pstate, Gpu::ClkGpc, &range));
            CHECK_RC(m_pPerf->VoltFreqLookup(Gpu::ClkGpc, Gpu::VOLTAGE_LOGIC,
                                             Perf::FreqToVolt, range.MinKHz/1000, &tempVoltuV));
            if (tempVoltuV > pstateVminuV)
            {
                pstateVminuV = tempVoltuV;
            }
        }   
        else
        {
            MASSERT(!"Invalid Clock Domain");
            return RC::ILWALID_CLOCK_DOMAIN;
        }

        pstateVminuV = max(railStatus.vminLimituV, pstateVminuV);
        if (pstateVminuV != lastPStateVminuV)
        {
            lastPStateVminuV = pstateVminuV;

            // Find the gpcclk frequency that corresponds to pstate Vmin
            CHECK_RC(m_pPerf->VoltFreqLookup(
                Gpu::ClkGpc, Gpu::VOLTAGE_LOGIC, Perf::VoltToFreq, pstateVminuV,
                &lastGpcclkMHz));
        }
        m_VminGpcclkMHz[make_tuple(pstate, Gpu::VOLTAGE_LOGIC)] = lastGpcclkMHz;

        VerbosePrintf("  P%d_%s_Vmin=%uuV, gpcclk=%uMHz\n",
            pstate, PerfUtil::SplitVoltDomainToStr(Gpu::VOLTAGE_LOGIC), pstateVminuV,
            m_VminGpcclkMHz[make_tuple(pstate, Gpu::VOLTAGE_LOGIC)]);
    }

    if (m_ThrashPowerCapping)
    {
        // Find power/thermal capping limits
        vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
        statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
        statuses[1].limitId = LW2080_CTRL_PERF_LIMIT_THERM_POLICY_DOM_GRP_1;
        CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, GetBoundGpuSubdevice()));

        const bool usingGpc2Clk = m_pPerf->IsRmClockDomain(Gpu::ClkGpc2);
        for (const auto& status : statuses)
        {
            if (status.output.bEnabled)
            {
                if (status.limitId == LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1)
                {
                    m_PowerLimitkHz = status.output.value / (usingGpc2Clk ? 2 : 1);
                }
                else
                {
                    m_ThermLimitkHz = status.output.value / (usingGpc2Clk ? 2 : 1);
                }
            }
        }

        if (m_PowerLimitkHz)
        {
            VerbosePrintf("  RM power limit kHz=%u\n", m_PowerLimitkHz);
        }
        if (m_ThermLimitkHz)
        {
            VerbosePrintf("  RM therm limit kHz=%u\n", m_ThermLimitkHz);
        }
    }

    return rc;
}

RC PerfPunish::QueryRMIfTime()
{
    const UINT64 lwrrTime = Xp::QueryPerformanceCounter();
    const FLOAT64 elapsedSec =
        static_cast<FLOAT64>(lwrrTime - m_LastTime) / Xp::QueryPerformanceFrequency();
    if (elapsedSec * 1000.0 >= static_cast<FLOAT64>(m_QueryPeriodMs))
    {
        m_LastTime = lwrrTime;
        if (m_pPerf->IsPState40Supported())
        {
            return QueryRMLimitsPS40();
        }
        return QueryRMLimits();
    }
    return RC::OK;
}

void PerfPunish::ResetTemporaryState()
{
    MASSERT(!m_IsPunishing.load());

    m_VminGpcclkMHz.clear();
    m_PunisherThreadId = Tasker::NULL_THREAD;
    m_StartTime = 0;
    m_LastTime = 0;
    m_LwrPState = Perf::ILWALID_PSTATE;
    m_VfSwitchesPerPState = 1;
    m_LwrGpcclkkHz = 0;
    m_PowerLimitkHz = 0;
    m_ThermLimitkHz = 0;
    m_LastSwitchAbovePowerCap = false;

    /* when the RC is not OK the items are not added
    and hence not compiled by the GPC_CLK fancy picker.
    Thus, no need to restart as it leads to failure in the restart
    because the picker is not ready to restart */
    if (m_StickyRC != RC::OK)
    {
        // Reseed/restart the FancyPickers
        m_pFpCtx->RestartNum = 0;
        m_pFpCtx->Rand.SeedRandom(GetTestConfiguration()->Seed());
        GetFancyPickerArray()->Restart();
    }
    m_StickyRC.Clear();
}

void PerfPunish::ClearPersistentState()
{
    m_ClfcClkDoms.clear();
    m_ClvcVoltDoms.clear();
    m_PStates.clear();
    m_PStateFreqskHz.clear();
}

RC PerfPunish::SetClfcEnable(bool bEnable) const
{
    if (m_ClfcClkDoms.empty())
        return OK;

    VerbosePrintf("%sing CLFC(s): ",
                  bEnable ? "Enabl" : "Disabl");
    for (const auto clkDom : m_ClfcClkDoms)
    {
        VerbosePrintf("%s ", PerfUtil::ClkDomainToStr(clkDom));
    }
    VerbosePrintf("\n");

    return m_pAvfs->SetFreqControllerEnable(m_ClfcClkDoms, bEnable);
}

RC PerfPunish::RunJsFunc(const char* jsFuncName)
{
    StickyRC stickyRc;

    JavaScriptPtr pJs;
    JsArray Args;
    jsval RetValJs = JSVAL_NULL;
    stickyRc = pJs->CallMethod(GetJSObject(), jsFuncName, Args, &RetValJs);
    if (OK == stickyRc)
    {
        UINT32 JSRetVal = RC::SOFTWARE_ERROR;
        stickyRc = pJs->FromJsval(RetValJs, &JSRetVal);
        stickyRc = JSRetVal;
    }

    return stickyRc;
}

// PerfPunish JavaScript interface

JS_CLASS_INHERIT(PerfPunish, GpuTest, "PerfSwitch on steroids");

JS_SMETHOD_LWSTOM(PerfPunish,
                  GetElapsedTimeMs,
                  0,
                  "Get the amount of time spent punishing")
{
    STEST_HEADER(0, 0, "Usage: let timeMs = PerfPunish.GetElapsedTimeMs()");
    STEST_PRIVATE(PerfPunish, pPerfPunish, "PerfPunish");

    if (OK != pJavaScript->ToJsval(pPerfPunish->GetElapsedTimeMs(), pReturlwalue))
    {
        pJavaScript->Throw(pContext, RC::SOFTWARE_ERROR,
            "Error in PerfPunish.GetElapsedTimeMs()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(PerfPunish,
                  IsPunishing,
                  0,
                  "True if the Punisher thread is kept alive")
{
    STEST_HEADER(0, 0, "Usage: let running = PerfPunish.IsPunishing()");
    STEST_PRIVATE(PerfPunish, pPerfPunish, "PerfPunish");

    if (OK != pJavaScript->ToJsval(pPerfPunish->GetIsPunishing(), pReturlwalue))
    {
        pJavaScript->Throw(pContext, RC::SOFTWARE_ERROR,
            "Error in PerfPunish.IsPunishing()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_STEST_LWSTOM(PerfPunish,
                Start,
                0,
                "Starts the Punisher thread")
{
    STEST_HEADER(0, 0, "Usage: PerfPunish.Start()");
    STEST_PRIVATE(PerfPunish, pPerfPunish, "PerfPunish");

    RETURN_RC(pPerfPunish->Start());
}

JS_STEST_LWSTOM(PerfPunish,
                Stop,
                0,
                "Stops the Punisher thread")
{
    STEST_HEADER(0, 0, "Usage: PerfPunish.Stop()");
    STEST_PRIVATE(PerfPunish, pPerfPunish, "PerfPunish");

    RETURN_RC(pPerfPunish->Stop());
}

CLASS_PROP_READWRITE(PerfPunish, PunishTimeMs, UINT32, "Time spent punishing");
CLASS_PROP_READWRITE(PerfPunish, QueryPeriodMs, FLOAT32, "Time spent between RM queries");
CLASS_PROP_READWRITE(PerfPunish, IgnoreRMGpcclkLimits, bool, "Ignore RM power/thermal limits");
CLASS_PROP_READWRITE(PerfPunish, ThrashPowerCapping, bool, "Ignore power capping for up to one switch");
CLASS_PROP_READWRITE(PerfPunish, DumpCoverage, bool, "Print switching stats after each test");
CLASS_PROP_READWRITE(PerfPunish, MaxGpcFreqkHz, UINT32, "Limits the maximum gpcclk frequency");
CLASS_PROP_READWRITE(PerfPunish, SwitchClkPropTopology, bool, "Switch clock topology");
CLASS_PROP_READWRITE(PerfPunish, SendDramLimitStrict, bool, "Send DRAM Strict Limit to RM");
CLASS_PROP_READWRITE(PerfPunish, RangeMinUV, UINT32, "Minimum Voltage for the low margin range");
CLASS_PROP_READWRITE(PerfPunish, RangeMaxUV, UINT32, "Maximum Voltage for the low margin range");
CLASS_PROP_READWRITE(PerfPunish, MaxModsVfSwitchTimeUs, UINT64,
    "Maximum time taken by MODS for each VF switch");
CLASS_PROP_READWRITE(PerfPunish, MaxPmuVfSwitchTimeUs, UINT64,
    "Maximum time taken by PMU for each VF switch");
CLASS_PROP_READWRITE(PerfPunish, MeasureVfSwitchTime, bool, "Measure time for each VF switch");