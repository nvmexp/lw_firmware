/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011, 2016, 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gpupm.cpp
 * @brief  Implemetation of the generic portion of the GPU
 *         performance monitor interface
 *
 */

#include "gpu/include/gpupm.h"
#include "gpu/include/gpusbdev.h"
#include "gpupm_p.h"
#include "ctrl/ctrl2080.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"


// Offset from the base FBP PMM register to the register for
// subsequent FBPs
#define FBP_PMM_OFFSET      0x1000
// Offset between L2C PM registers between FBPs
#define FBP_L2C_PM_REG_SEPARATION    0x2000
// L2 Cache PM signals (there is no header file where these are
// available to software)
#define SIG_L2C_REQUEST_ACTIVE      0x21
#define SIG_L2C_HIT                 0x02
#define SIG_L2C_MISS                0x03

//----------------------------------------------------------------------------
//! \brief Constructor
//!
GpuPerfmon::GpuPerfmon(GpuSubdevice *pSubdev)
:   m_pGpuSubdev(pSubdev),
    m_ThreadID(Tasker::NULL_THREAD),
    m_bStopAcquiring(false),
    m_PerfmonReserved(false),
    m_PollingError(false),
    m_Pri(Tee::PriLow)
{
}

//----------------------------------------------------------------------------
//! \brief Destructor
//!
GpuPerfmon::~GpuPerfmon()
{
    // This MASSERT means we forgot to call ShutDown() before deleting.
    MASSERT(m_Experiments.empty());
}

//----------------------------------------------------------------------------
//! \brief Return the PMM offsets between FBBs
//!
UINT32 GpuPerfmon::GetFbpPmmOffset()
{
    return FBP_PMM_OFFSET;
}

//----------------------------------------------------------------------------
//! \brief Setup an experiment on the specified Fbp
//!
//! \param FbpNum  : FBP number for the experiment
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC GpuPerfmon::SetupFbpExperiment(UINT32 FbpNum, UINT32 PmIdx)
{
    Printf(GetPrintPri(), "GpuPerfmon::SetupFbpExperiment(fbp=%d pmIdx=%d)\n",
           FbpNum, PmIdx);
    GpuSubdevice *pSubdev = GetGpuSubdevice();
    RegHal &gpuRegs = pSubdev->Regs();
    gpuRegs.Write32(MODS_PERF_PMASYS_FBP_TRIGGER_START_MASK,
                    gpuRegs.LookupMask(MODS_PERF_PMASYS_FBP_TRIGGER_START_MASK_ENGINE));
    gpuRegs.Write32(MODS_PERF_PMASYS_FBP_TRIGGER_STOP_MASK,
                    gpuRegs.LookupMask(MODS_PERF_PMASYS_FBP_TRIGGER_STOP_MASK_ENGINE));
    gpuRegs.Write32(MODS_PERF_PMASYS_FBP_TRIGGER_CONFIG_MIXED_MODE,
                    gpuRegs.LookupMask(MODS_PERF_PMASYS_FBP_TRIGGER_CONFIG_MIXED_MODE_ENGINE));
    gpuRegs.Write32(MODS_PERF_PMASYS_TRIGGER_GLOBAL_ENABLE_ENABLED);

    ResetFbpPmControl(FbpNum, PmIdx);

    const UINT32 fbpOffset = FbpNum * GetFbpPmmOffset();
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CLAMP_CYA_CONTROL, PmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_CLAMP_CYA_CONTROL_ENABLECYA_DISABLE));

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Reset the PMM registers on the specified FBP
//!
//! \param FbpNum  : FBP number to reset
//!
void GpuPerfmon::ResetFbpPmControl(UINT32 FbpNum, UINT32 PmIdx)
{
    Printf(GetPrintPri(), "GF100GpuPerfmon::ResetFbpPmControl(fbp=%d pmIdx=%d)\n",
           FbpNum, PmIdx);
    RegHal &gpuRegs = m_pGpuSubdev->Regs();
    const UINT32 fbpOffset = FbpNum * GetFbpPmmOffset();
    GpuSubdevice * pSb = m_pGpuSubdev;
    // Clear the CONTROL register first so others have a stable value
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CONTROL, PmIdx)    + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENTCNT, PmIdx)   + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIGGERCNT, PmIdx) + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_SAMPLECNT, PmIdx)  + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_THRESHOLD, PmIdx)  + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_RELEASE, PmIdx)    + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_ENGINE_SEL, PmIdx) + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG1_SEL, PmIdx)  + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG1_OP, PmIdx)   + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_SAMPLE_SEL, PmIdx) + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_SAMPLE_OP, PmIdx)  + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_SETFLAG_OP, PmIdx) + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CLRFLAG_OP, PmIdx) + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENT_SEL, PmIdx)  + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENT_OP, PmIdx)   + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG0_SEL, PmIdx)  + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG0_OP, PmIdx)   + fbpOffset, 0);
    pSb->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CLAMP_CYA_CONTROL, PmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_CLAMP_CYA_CONTROL_ENABLECYA_ENABLE));
}
//----------------------------------------------------------------------------
//! \brief Trigger the counters (this should cause the shadowed counters to
//!        copy into the real counters as well)
//!
void GpuPerfmon::TriggerCounters()
{
    Printf(GetPrintPri(), "GpuPerfmon::TriggerCounters()\n");
    RegHal &gpuRegs = m_pGpuSubdev->Regs();
    gpuRegs.Write32(MODS_PERF_PMASYS_TRIGGER_GLOBAL_MANUAL_START_PULSE);
}

void GpuPerfmon::ResetFbpPerfmon()
{
    Printf(GetPrintPri(), "GpuPerfmon::ResetFbpPerfmon()\n");
    RegHal &gpuRegs = m_pGpuSubdev->Regs();
    gpuRegs.Write32(MODS_PLTCG_LTCS_MISC_LTC_PM_ENABLE_DISABLED);
}

//----------------------------------------------------------------------------
//! \brief Clean up.  Should be called before destructor.
//!
//! Note: This method isn't part of the destructor because
//! EndExperiments() use GpuPerfmon virtual methods.  In the
//! GpuPerfmon destructor, the subclass's vtable is replaced by the
//! GpuPerfmon vtable, so we call the wrong methods or even read past
//! the end of the vtable.
//!
void GpuPerfmon::ShutDown()
{
    // End any existing experiments and delete them
    EndExperiments();

    for (size_t i = 0; i < m_Experiments.size(); i++)
    {
        delete m_Experiments[i];
    }
    m_Experiments.clear();
}

//----------------------------------------------------------------------------
RC GpuPerfmon::ReservePerfMon(bool reserve)
{
    if (m_PerfmonReserved == reserve)
    {
        return OK;
    }
    LW2080_CTRL_PERF_RESERVE_PERFMON_HW_PARAMS params = { 0 };
    params.bAcquire = reserve;
    const RC rc = LwRmPtr()->ControlBySubdevice(m_pGpuSubdev,
            LW2080_CTRL_CMD_PERF_RESERVE_PERFMON_HW,
            &params, sizeof(params));
    if (OK == rc)
    {
        m_PerfmonReserved = reserve;
    }
    else
    {
        Printf(Tee::PriError, "RM failed to %sreserve perfmon\n", reserve?"":"un");
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Start all experiments
//!
//! \return OK if starting was successful, not OK otherwise
//!
RC GpuPerfmon::BeginExperiments()
{
    RC rc;

    if (m_Experiments.empty())
        return OK;

    CHECK_RC(ReservePerfMon(true));

    bool bResetFbpPerfmon = false;
    for (size_t i = 0; i < m_Experiments.size(); i++)
    {
        if ((m_Experiments[i]->GetPmType() == GpuPerfmon::Experiment::Fb) &&
                !bResetFbpPerfmon)
        {
            ResetFbpPerfmon();
            bResetFbpPerfmon = true;
        }

        CHECK_RC(m_Experiments[i]->Start());
    }

    if (m_ThreadID == Tasker::NULL_THREAD)
    {
        m_PollingError = false;
        m_bStopAcquiring = false;
        m_ThreadID = Tasker::CreateThread(AcquireDataHandler, this,
                                          Tasker::MIN_STACK_SIZE,
                                          "Perfmon::AcquireDataTask");
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief End all experiments
//!
//! \return OK if ending was successful, not OK otherwise
//!
RC GpuPerfmon::EndExperiments()
{
    StickyRC rc;

    if (m_ThreadID != Tasker::NULL_THREAD &&
        m_ThreadID != Tasker::LwrrentThread())
    {
        m_bStopAcquiring = true;
        rc = Tasker::Join(m_ThreadID);
        m_ThreadID = Tasker::NULL_THREAD;
    }

    // Check for error after join to make sure that AcquireDataHandler
    // thread had a chance to execute in case EndExperiments is called
    // very shortly after BeginExperiments
    if (m_PollingError)
    {
        // The shadow registers did not latch.
        // This is typically a programming error, for example when
        // we don't know the right signals for a new chip.
        rc = RC::SOFTWARE_ERROR;
    }

    for (size_t i = 0; i < m_Experiments.size(); i++)
    {
        rc = m_Experiments[i]->End();
    }

    rc = ReservePerfMon(false);

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Destroy the specified experiment
//!
//! \return OK if destruction was successful, not OK otherwise
//!
RC GpuPerfmon::DestroyExperiment(const Experiment *pExperiment)
{
    vector<Experiment *>::iterator ppExperiment = m_Experiments.begin();

    for (; ppExperiment != m_Experiments.end(); ppExperiment++)
    {
        if (*ppExperiment == pExperiment)
        {
            delete pExperiment;
            m_Experiments.erase(ppExperiment);
            return OK;
        }
    }

    return RC::SOFTWARE_ERROR;
}

//----------------------------------------------------------------------------
//! \brief Protected function to add an experiment to the list
//!
//! \param pExperiment : The experiment to add
//!
//! \return OK if adding the experiment was successful, not OK otherwise
//!
RC GpuPerfmon::AddExperiment(Experiment *pExperiment)
{
    for (size_t i = 0; i < m_Experiments.size(); i++)
    {
        if (m_Experiments[i]->IsConflicting(pExperiment))
        {
            return RC::SOFTWARE_ERROR;
        }
    }
    m_Experiments.push_back(pExperiment);
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Get the results for the experiment
//!
//! \param pHandle : Pointer to the expermiment handle to get the results for
//!
void GpuPerfmon::GetResults(const Experiment *pHandle, Results * pResults)
{
    if ( pHandle )
        pHandle->GetResults(pResults);
}

//----------------------------------------------------------------------------
//! \brief Static handler for acquiring perfmon data
//!
//! \param pThis : Pointer to the perfmon object
//!
void GpuPerfmon::AcquireDataHandler(void *pThis)
{
    GpuPerfmon *pPerfmon = static_cast<GpuPerfmon*>(pThis);
    // This is a guess for emulation. We know that memory transactions are very
    // slow on emulation.
    FLOAT64 sleepMs = (pPerfmon->m_pGpuSubdev->IsEmulation()) ? 850.0 : 1.0;
    do
    {

        Tasker::Sleep(sleepMs);
        pPerfmon->ReadCounters();
    }
    while (!pPerfmon->m_bStopAcquiring);
}

//----------------------------------------------------------------------------
//! \brief Reads counters using shadow registers
//!
void GpuPerfmon::ReadCounters()
{
    // Issue the global trigger in PMA to all engines
    TriggerCounters();

    // Wait (the signal should propagate within 300 clocks)
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        Tasker::Sleep(1);
    }
    else
    {
        Platform::ClockSimulator(300);
    }

    // Attempt to read data from the shadow registers
    for (size_t iexp=0; iexp < m_Experiments.size(); iexp++)
    {
        if (!m_Experiments[iexp]->AclwmulateData())
        {
            m_PollingError = true;
            Printf(Tee::PriLow, "Error: Perfmon counter not latched!\n");
        }
    }
}

//----------------------------------------------------------------------------
//! \brief Constructor
//!
GpuPerfmon::Experiment::Experiment(GpuPerfmon *pPerfmon,
                                   GpuPerfmon::ExperimentType type,
                                   PmType pmType)
:   m_pPerfmon(pPerfmon),
    m_Type(type),
    m_PmType(pmType),
    m_bStarted(false)
{
    m_Results.Type = m_Type;
}

//----------------------------------------------------------------------------
//! \brief Get the results for the experiment
//!
//! \param pResults : Pointer to the returned results
//!
/* virtual */ void GpuPerfmon::Experiment::GetResults(GpuPerfmon::Results * pResults) const
{
    memcpy(pResults, &m_Results, sizeof(GpuPerfmon::Results));
}

//----------------------------------------------------------------------------
//! \brief Constructor
//!
L2CacheExperiment::L2CacheExperiment
(
    GpuPerfmon *pPerfmon,
    GpuPerfmon::Experiment::PmType pmType,
    UINT32 fbpNum,
    UINT32 l2slice
)
:  GpuPerfmon::Experiment(pPerfmon, GpuPerfmon::ExpL2Cache, pmType),
   m_PmIdx(0),
   m_FbpNum(fbpNum),
   m_L2Slice(l2slice)
{
}

//----------------------------------------------------------------------------
//! \brief default is to just return the current Perfmon index.
UINT32 L2CacheExperiment::CalcPmIdx()
{
    return m_PmIdx;
}
//----------------------------------------------------------------------------
//! \brief Check whether the experiment conflicts with the specified experiment
//!
//! \param pExp : Experiment to check for conflicts
//!
//! \return True if there is a conflict, false otherwise
//!
/* virtual */ bool L2CacheExperiment::IsConflicting(Experiment *pExp) const
{
    if (pExp->GetType() != GpuPerfmon::ExpL2Cache)
        return false;

    return ((pExp->GetPmType() == GpuPerfmon::Experiment::Fb) &&
            (((L2CacheExperiment*)pExp)->GetFbp() == GetFbp()));
}

//----------------------------------------------------------------------------
//! \brief Reset the gathered stats for the experiment
//!
void L2CacheExperiment::ResetStats()
{
    GpuPerfmon::Results *pResults = GetResultsPtr();
    pResults->Data.CacheResults.HitCount = 0;
    pResults->Data.CacheResults.MissCount = 0;
}

//----------------------------------------------------------------------------
//! \brief Accumulate stats for the experiment
//!
//! \param HitCount  : The L2 Cache Hit Count to add to the current total
//! \param MissCount : The L2 Cache Miss Count to add to the current total
//!
void L2CacheExperiment::AclwmulateStats(UINT32 HitCount, UINT32 MissCount)
{
    GpuPerfmon::Results *pResults = GetResultsPtr();
    pResults->Data.CacheResults.HitCount += (UINT64)HitCount;
    pResults->Data.CacheResults.MissCount += (UINT64)MissCount;
}

//----------------------------------------------------------------------------
//! \brief Start the experiments
//!
//! \return OK if starting was successful, not OK otherwise
//!
/* virtual */ RC L2CacheExperiment::Start()
{
    RC rc;
    GpuPerfmon *pGpuPerfmon         = GetPerfmon();
    GpuSubdevice *pSubdev           = pGpuPerfmon->GetGpuSubdevice();
    Tee::Priority pri               = pGpuPerfmon->GetPrintPri();
    RegHal &gpuRegs                 = pSubdev->Regs();
    UINT32 regValue                 = 0;
    ResetStats();

    // use default PerfMon
    const UINT32 pmIdx = GetPmIdx();
    const UINT32 fbpNum = GetFbp();
    const UINT32 fbpOffset = fbpNum * FBP_PMM_OFFSET;

    CHECK_RC(pGpuPerfmon->SetupFbpExperiment(fbpNum, pmIdx));

    UINT32 ltcPmOffset = gpuRegs.LookupAddress(MODS_PLTCG_LTC0_MISC_LTC_PM) +
                         fbpNum * FBP_L2C_PM_REG_SEPARATION;

    regValue = pSubdev->RegRd32(ltcPmOffset);
    if (GetL2Slice() == 0)
    {
        gpuRegs.SetField(&regValue, MODS_PLTCG_LTC0_MISC_LTC_PM_SELECT_GRP0);
        gpuRegs.SetField(&regValue, MODS_PLTCG_LTC0_MISC_LTC_PM_LTS_MUX_SELECT_GRP0);
    }
    else
    {
        gpuRegs.SetField(&regValue, MODS_PLTCG_LTC0_MISC_LTC_PM_SELECT_GRP1);
        gpuRegs.SetField(&regValue, MODS_PLTCG_LTC0_MISC_LTC_PM_LTS_MUX_SELECT_GRP1);
    }
    gpuRegs.SetField(&regValue, MODS_PLTCG_LTC0_MISC_LTC_PM_ENABLE_ENABLED);
    pSubdev->RegWr32(ltcPmOffset, regValue);

    //Note LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER is not really an address but we have to
    //     use LookupAddress to get the value.
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_ENGINE_SEL, pmIdx) + fbpOffset,
                    gpuRegs.LookupAddress(MODS_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER));

    // Measure Hits in the event counter
    Printf(pri, "Measure hits in the event counter\n");
    regValue = gpuRegs.SetField(MODS_PERF_PMMFBP_EVENT_SEL_SEL1, SIG_L2C_REQUEST_ACTIVE);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_EVENT_SEL_SEL0, SIG_L2C_HIT);
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENT_SEL, pmIdx) + fbpOffset,
                     regValue);

    // Count any time m_SigLtcRegActive & m_SigLtcHit are true
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENT_OP, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_EVENT_OP_FUNC, 0x8888));

    // Measure misses in the trig0 counter
    Printf(pri, "Measure misses in the trig0 counter\n");
    regValue = gpuRegs.SetField(MODS_PERF_PMMFBP_TRIG0_SEL_SEL1, SIG_L2C_REQUEST_ACTIVE);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_TRIG0_SEL_SEL0, SIG_L2C_MISS);
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG0_SEL, pmIdx) + fbpOffset,
                     regValue);

    // Count anytime m_SigLtcRegActive & m_SigLtcMiss are true.
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG0_OP, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_TRIG0_OP_FUNC, 0x8888));

    // Set control register last to turn on perfmon
    regValue = gpuRegs.SetField(MODS_PERF_PMMFBP_CONTROL_MODE_B);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_CONTROL_ADDTOCTR_INCR);
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CONTROL, pmIdx) + fbpOffset,
                     regValue);

    SetStarted(true);
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Accumulate data for the experiment
//!
//! \return true if theshadowed counter values should be released (i.e. data
//!         was aclwmulated or the counters overflowed)
//!
/* virtual */ bool L2CacheExperiment::AclwmulateData()
{
    if (!IsStarted())
        return true;

    GpuPerfmon *pGpuPerfmon = GetPerfmon();
    Printf(pGpuPerfmon->GetPrintPri(),
           "GF100L2CacheExperiment::AclwmulateData(fbp:%d slice:%d)\n",
           GetFbp(), GetL2Slice());

    GpuSubdevice *pSubdev   = pGpuPerfmon->GetGpuSubdevice();
    const UINT32 pmIdx      = GetPmIdx();
    const UINT32 fbpOffset  = GetFbp() * pGpuPerfmon->GetFbpPmmOffset();
    RegHal &gpuRegs         = pSubdev->Regs();
    UINT32 control          =
        pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CONTROL, pmIdx) + fbpOffset);

    bool   bRelease         = false;

    if (gpuRegs.TestField(control, MODS_PERF_PMMFBP_CONTROL_SHADOW_STATE_VALID))
    {
        AclwmulateStats(
            pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENTCNT, pmIdx) + fbpOffset),
            pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIGGERCNT, pmIdx) + fbpOffset)
        );
        bRelease = true;
    }
    else if (gpuRegs.TestField(control, MODS_PERF_PMMFBP_CONTROL_SHADOW_STATE_OVERFLOW))
    {
        Printf(pGpuPerfmon->GetPrintPri(), "Error: L2 perfmon overflow!\n");
        bRelease = true;
    }

    if (bRelease)
    {
        pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_RELEASE, pmIdx) + fbpOffset,
                         gpuRegs.SetField(MODS_PERF_PMMFBP_RELEASE_SHADOWS_DOIT));
    }

    return bRelease;
}


//----------------------------------------------------------------------------
//! \brief End the experiment
//!
//! \return OK if ending was successful, not OK otherwise
//!
/* virtual */ RC L2CacheExperiment::End()
{
    SetStarted(false);
    GpuPerfmon *pGpuPerfmon = GetPerfmon();
    Printf(pGpuPerfmon->GetPrintPri(), "L2CacheExperiment::End()\n");
    GpuSubdevice *pSubdev   = pGpuPerfmon->GetGpuSubdevice();
    const UINT32 pmIdx      = GetPmIdx();
    const UINT32 fbpOffset  = GetFbp() * pGpuPerfmon->GetFbpPmmOffset();
    RegHal &gpuRegs         = pSubdev->Regs();
    UINT32 control =
        pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CONTROL, pmIdx) + fbpOffset);

    if (gpuRegs.TestField(control, MODS_PERF_PMMFBP_CONTROL_SHADOW_STATE_VALID))
    {
        AclwmulateStats(
            pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENTCNT, pmIdx) + fbpOffset),
            pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIGGERCNT, pmIdx) + fbpOffset)
        );

        pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_RELEASE, pmIdx) + fbpOffset,
                         gpuRegs.SetField(MODS_PERF_PMMFBP_RELEASE_SHADOWS_DOIT));
    }

    AclwmulateStats(
        pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_RAWCNT0, pmIdx) + fbpOffset),
        pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_RAWCNT1, pmIdx) + fbpOffset)
    );

    // Reset the experiment registers
    UINT32 regAddr = gpuRegs.LookupAddress(MODS_PLTCG_LTC0_MISC_LTC_PM) +
                                   GetFbp() * FBP_L2C_PM_REG_SEPARATION;
    UINT32 temp = pSubdev->RegRd32(regAddr);
    gpuRegs.SetField(&temp, MODS_PLTCG_LTC0_MISC_LTC_PM_ENABLE_DISABLED);
    pSubdev->RegWr32(regAddr, temp);

    pGpuPerfmon->ResetFbpPmControl(GetFbp(), pmIdx);

    return OK;
}

