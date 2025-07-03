/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pwmutil.h"

#include "core/include/abort.h"
#include "core/include/cpu.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "gpu/include/gpumgr.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/perf/thermsub.h"
#include "gpu/perf/clockthrottle.h"

#include <algorithm>
#include <numeric>

//-----------------------------------------------------------------------------
//                          PwmDispatcher methods
//-----------------------------------------------------------------------------

bool PwmDispatcher::HasActors() const
{
    return !m_Actors.empty();
}

RC PwmDispatcher::AddActor
(
    ActorType Type,
    JSObject *pParamsObj
)
{
    RC rc;
    unique_ptr<PwmActor> pActor = nullptr;
    switch (Type)
    {
        case FREQ_ACTOR:
            pActor = make_unique<PwmFreqActor>();
            break;
        case SLOWDOWN_ACTOR:
            pActor = make_unique<PwmSlowdownActor>();
            break;
        default:
            Printf(Tee::PriError, "PwmDispatcher: Unknown Actor type\n");
            return RC::BAD_PARAMETER;
    }

    rc = pActor->ParseParams(pParamsObj);
    pActor->PrintParams();
    if (rc != OK)
    {
        Printf(Tee::PriError,
            "PwmDispatcher: Invalid params. Cannot add PWM actor\n");
        return rc;
    }

    m_Actors.push_back(move(pActor));
    return rc;
}

RC PwmDispatcher::Start()
{
    RC rc;

    if (!HasActors())
    {
        Printf(Tee::PriError, "PwmDispatcher: No PWM actors to run\n");
        return RC::SOFTWARE_ERROR;
    }

    for (const auto & pActor : m_Actors)
    {
        pActor->SetForceMinDutyCycle(false);
        CHECK_RC(pActor->Setup());
        CHECK_RC(pActor->Start());
    }

    return rc;
}

RC PwmDispatcher::Stop()
{
    StickyRC stickyRc;
    for (const auto & pActor : m_Actors)
    {
        stickyRc = pActor->Stop();
        stickyRc = pActor->Cleanup();
    }
    return stickyRc;
}

UINT64 PwmDispatcher::GetTotalTimeMs(TimeQuery timeQueryType) const
{
    if (!HasActors())
    {
        MASSERT(!"PwmDispatcher has no actors");
        return 0;
    }

    UINT64 maxTimeUs = ~0ULL;
    for (const auto& actor : m_Actors)
    {
        UINT64 tempTimeUs = ~0ULL;
        switch (timeQueryType)
        {
            case TimeQuery::TARGET:
                tempTimeUs = actor->GetTotalTargetTimeUs();
                break;
            case TimeQuery::LOW:
                tempTimeUs = actor->GetTotalLowTimeUs();
                break;
            default:
                MASSERT(!"Unknown time query type in PwmDispatcher");
                break;
        }
        if (tempTimeUs < maxTimeUs)
        {
            maxTimeUs = tempTimeUs;
        }
    }
    MASSERT(maxTimeUs != ~0ULL);
    return maxTimeUs / 1000;
}

UINT64 PwmDispatcher::GetTotalTargetTimeMs() const
{
    return GetTotalTimeMs(TimeQuery::TARGET);
}

UINT64 PwmDispatcher::GetTotalLowTimeMs() const
{
    return GetTotalTimeMs(TimeQuery::LOW);
}

bool PwmDispatcher::IsRunning() const
{
    if (!HasActors())
        return false;

    for (const auto& pActor : m_Actors)
    {
        if (!pActor->GetIsRunning())
            return false;
    }

    return true;
}

void PwmDispatcher::ForceMinDutyCycle()
{
    MASSERT(!m_Actors.empty());
    for (auto& actor : m_Actors)
    {
        actor->SetForceMinDutyCycle(true);
    }
}

//-----------------------------------------------------------------------------
//                          PwmActor methods
//-----------------------------------------------------------------------------

GpuSubdevice* PwmActor::GetGpuSubdev()
{
    return m_pGpuSubdev;
}

namespace
{
    RC IlwalidInputErrMsg(const char* param)
    {
        Printf(Tee::PriError, "%s must be non-zero\n", param);
        return RC::BAD_PARAMETER;
    };
}

RC PwmActor::ParseParams(JSObject *pParamsObj)
{
    StickyRC stickyRc;
    JavaScriptPtr pJs;

    UINT32 GpuInst;
    stickyRc = pJs->GetProperty(pParamsObj, "GpuInst", &GpuInst);
    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)(DevMgrMgr::d_GraphDevMgr);
    if (!pGpuDevMgr)
    {
        Printf(Tee::PriError,
            "%s: Gpu subdevice manager not ready yet\n", m_ActorName.c_str());
        return RC::SOFTWARE_ERROR;
    }
    GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
    if (!pSubdev)
    {
        Printf(Tee::PriError, "%s: GpuInst %d unknown\n", m_ActorName.c_str(), GpuInst);
        return RC::BAD_PARAMETER;
    }
    m_pGpuSubdev = pSubdev;

    pJs->GetProperty(pParamsObj, "DutyHint", &m_DutyHint);
    pJs->GetProperty(pParamsObj, "DutyCyclePct", &m_DutyCyclePct);
    if (!m_DutyCyclePct)
    {
        // No duty cycle was specified by the user.
        // We need to figure it out dynamically
        m_AdjustDutyCyle = true;
        m_DutyCyclePct = m_DutyHint;
    }
    pJs->GetProperty(pParamsObj, "DutyCyclePeriodUs", &m_DutyCyclePeriodUs);
    if (!m_DutyCyclePeriodUs)
    {
        return IlwalidInputErrMsg("DutyCyclePeriodUs");
    }

    m_TargetActionDurationUs =
        static_cast<UINT32>(m_DutyCyclePeriodUs * m_DutyCyclePct * 0.01);
    m_LowerActionDurationUs = m_DutyCyclePeriodUs - m_TargetActionDurationUs;

    pJs->GetProperty(pParamsObj, "Name", &m_ActorName);

    bool verbose = false;
    if (OK == pJs->GetProperty(pParamsObj, "Verbose", &verbose) && verbose)
    {
        m_PrintPri = Tee::PriNormal;
        m_StatsPrintPri = Tee::PriNormal;
    }
    if (OK == pJs->GetProperty(pParamsObj, "PrintStats", &verbose) && verbose)
    {
        m_StatsPrintPri = Tee::PriNormal;
    }

    pJs->GetProperty(pParamsObj, "SamplerPeriodUs", &m_SamplerPeriodUs);
    if (!m_SamplerPeriodUs)
    {
        return IlwalidInputErrMsg("SamplerPeriodUs");
    }
    pJs->GetProperty(pParamsObj, "CyclesUntilFail", &m_CyclesUntilFail);
    if (!m_CyclesUntilFail)
    {
        return IlwalidInputErrMsg("CyclesUntilFail");
    }
    pJs->GetProperty(pParamsObj, "DutyCycleFailThreshold", &m_DutyCycleFailThreshold);
    if (!m_DutyCycleFailThreshold)
    {
        return IlwalidInputErrMsg("DutyCycleFailThreshold");
    }
    pJs->GetProperty(pParamsObj, "DutyControllerSkipCount", &m_DutyControllerSkipCount);
    if (!m_DutyControllerSkipCount)
    {
        return IlwalidInputErrMsg("DutyControllerSkipCount");
    }

    return stickyRc;
}

void PwmActor::PrintParams()
{
    Printf(m_PrintPri, "%s properties:\n", m_ActorName.c_str());

    Printf(m_PrintPri, "\tCyclesUntilFail: %u\n", m_CyclesUntilFail);
    Printf(m_PrintPri, "\tDutyControllerSkipCount: %u\n", m_DutyControllerSkipCount);
    Printf(m_PrintPri, "\tDutyCycleFailThreshold: %0.1f%%\n", m_DutyCycleFailThreshold);
    Printf(m_PrintPri, "\tDutyCyclePct: %0.1f%%\n", m_DutyCyclePct);
    Printf(m_PrintPri, "\tDutyCyclePeriodUs: %u\n", m_DutyCyclePeriodUs);
    Printf(m_PrintPri, "\tDutyHint: %0.3f\n", m_DutyHint);
    Printf(m_PrintPri, "\tPrintStats: %s\n", m_StatsPrintPri == Tee::PriNormal ? "true" : "false");
    Printf(m_PrintPri, "\tSamplerPeriodUs: %u\n", m_SamplerPeriodUs);
    Printf(m_PrintPri, "\tVerbose: %s\n", m_PrintPri == Tee::PriNormal ? "true" : "false");
}

static void RunPwmActorThread(void *Arg)
{
    PwmActor *pPwmActor = static_cast<PwmActor*>(Arg);
    pPwmActor->RunThread();
}

static void RunSamplerThread(void *Arg)
{
    PwmActor *pPwmActor = static_cast<PwmActor*>(Arg);
    pPwmActor->SamplerThread();
}

RC PwmActor::Start()
{
    RC rc;
    SetIsRunning(true);

    auto threadCreationErrorMsg = [](const char* msg) -> RC
    {
        Printf(Tee::PriError, "Failed to create PWM %s thread\n", msg);
        return RC::SOFTWARE_ERROR;
    };

    if (RequiresSamplerThread())
    {
        Printf(m_PrintPri, "Starting %s sampler thread\n", m_ActorName.c_str());
        m_SamplerThreadId = Tasker::CreateThread(RunSamplerThread, this, 0, "PwmActor::sampler");
        if (m_SamplerThreadId == Tasker::NULL_THREAD)
        {
            return threadCreationErrorMsg("sampler");
        }
    }

    Printf(m_PrintPri, "Starting %s\n", m_ActorName.c_str());
    m_ActorThreadId = Tasker::CreateThread(RunPwmActorThread, this, 0, "PwmActor");
    if (m_ActorThreadId == Tasker::NULL_THREAD)
    {
        return threadCreationErrorMsg("actor");
    }
    return rc;
}

RC PwmActor::Stop()
{
    StickyRC rc;
    Printf(m_PrintPri, "Stopping %s\n", m_ActorName.c_str());
    SetIsRunning(false);
    rc = JoinThreads(2000);
    rc = m_RunRc;
    rc = m_SamplerRc;

    PrintStats();

    m_NumDutyCycles = 0;
    m_TotalTargetTimeUs = 0;
    m_TotalLowTimeUs = 0;

    return rc;
}

void PwmActor::PrintStats() const
{
    // Report duty cycle statistics
    Printf(m_StatsPrintPri, "%s statistics:\n", m_ActorName.c_str());
    Printf(m_StatsPrintPri, " NumDutyCycles = %llu\n", m_NumDutyCycles);
    const FLOAT32 totalTimeUs = static_cast<FLOAT32>(m_TotalTargetTimeUs + m_TotalLowTimeUs);
    FLOAT32 avgDutyCycle = 0.0f;
    const FLOAT32 totalTargetTimeSec = m_TotalTargetTimeUs / 1000000.0f;
    const FLOAT32 totalLowTimeSec = m_TotalLowTimeUs / 1000000.0f;
    if (totalTimeUs)
    {
        avgDutyCycle = 100.0f * m_TotalTargetTimeUs / totalTimeUs;
        Printf(m_StatsPrintPri, " AverageDutyCycle = %.3f%%\n", avgDutyCycle);
    }
    Printf(m_StatsPrintPri, " TotalTargetTime = %.3f sec\n", totalTargetTimeSec);
    Printf(m_StatsPrintPri, " TotalLowTime = %.3f sec\n", totalLowTimeSec);
    Mle::Print(Mle::Entry::pwm_actor_stats)
        .duty_cycles(m_NumDutyCycles)
        .avg_duty_cycle(avgDutyCycle)
        .total_target_time_s(totalTargetTimeSec)
        .total_low_time_s(totalLowTimeSec);
}

static UINT32 CallwlateElapsedTimeUsFromTicks
(
    UINT64 BeforeTick,
    UINT64 AfterTick
)
{
    UINT64 PerfFreq = Xp::QueryPerformanceFrequency();
    UINT64 tickCount = AfterTick - BeforeTick;
    return static_cast<UINT32>(tickCount * 1000000 / PerfFreq);
}

void PwmActor::RunThread()
{
    m_RunRc.Clear();
    Tasker::DetachThread detach;

    DEFER
    {
        SetIsRunning(false);
    };

    while (GetIsRunning() && Abort::Check() == OK)
    {
        Printf(m_PrintPri, "%s::ExelwtePwm()\n", m_ActorName.c_str());
        m_RunRc = ExelwtePwm();
    }
}

void PwmActor::SamplerThread()
{
    MASSERT(!"SamplerThread was requested to run but is unavailable");
}

RC PwmActor::JoinThreads(UINT64 timeoutMs)
{
    StickyRC rc;

    if (m_SamplerThreadId != Tasker::NULL_THREAD)
    {
        rc = Tasker::Join(m_SamplerThreadId, static_cast<FLOAT64>(timeoutMs));
        m_SamplerThreadId = Tasker::NULL_THREAD;
    }
    if (m_ActorThreadId != Tasker::NULL_THREAD)
    {
        rc = Tasker::Join(m_ActorThreadId, static_cast<FLOAT64>(timeoutMs));
        m_ActorThreadId = Tasker::NULL_THREAD;
    }

    return rc;
}

void PwmActor::SetIsRunning(bool value)
{
    Cpu::AtomicWrite(&m_IsRunning, (value ? 1 : 0));
}

bool PwmActor::GetIsRunning() const
{
    return (Cpu::AtomicRead(const_cast<volatile INT32*>(&m_IsRunning)) == 1);
}

bool PwmActor::RequiresSamplerThread() const
{
    return false;
}

//-----------------------------------------------------------------------------
//                          PwmFreqActor methods
//-----------------------------------------------------------------------------

RC PwmFreqActor::ParseParams(JSObject *pParamsObj)
{
    RC rc;
    StickyRC stickyRc;

    stickyRc = PwmActor::ParseParams(pParamsObj);

    m_pPerf = GetGpuSubdev()->GetPerf();
    m_pPower = GetGpuSubdev()->GetPower();
    m_pAvfs = GetGpuSubdev()->GetAvfs();

    auto objErrMsg = [&](const char* obj) -> RC
    {
        Printf(Tee::PriError,
            "PwmFreqActor: %s object is not initialized\n", obj);
        return RC::SOFTWARE_ERROR;
    };
    if (!m_pPerf)
    {
        CHECK_RC(objErrMsg("Perf"));
    }
    if (!m_pPower)
    {
        CHECK_RC(objErrMsg("Power"));
    }

    JavaScriptPtr pJs;
    JSObject *pPPObj;
    stickyRc = pJs->GetProperty(pParamsObj, "TargetPerfPoint", &pPPObj);
    stickyRc = m_pPerf->JsObjToPerfPoint(pPPObj, &m_TargetPerfPoint);

    stickyRc = pJs->GetProperty(pParamsObj, "LowerPerfPoint", &pPPObj);
    stickyRc = m_pPerf->JsObjToPerfPoint(pPPObj, &m_LowerPerfPoint);

    // Optional total GPU power (TGP) target. If unspecified, we will use the
    // value from VBIOS/RM.
    if (OK == pJs->GetProperty(pParamsObj, "PowerTargetmW", &m_PowerTargetmW) &&
        m_PowerTargetmW < 1000)
    {
        Printf(Tee::PriError, "Invalid value for PowerTargetmW\n");
        return RC::BAD_PARAMETER;
    }

    pJs->GetProperty(pParamsObj, "DisablePowerCapping", &m_DisablePowerCapping);
    pJs->GetProperty(pParamsObj, "IgnorePowerErrors", &m_IgnorePowerErrors);
    pJs->GetProperty(pParamsObj, "IgnoreThermalErrors", &m_IgnoreThermalErrors);

    if (m_AdjustDutyCyle)
    {
        stickyRc = ParseControllerParams(pParamsObj);
    }

    if (m_ActorName.empty())
    {
        m_ActorName = "PwmFreqActor";
    }

    return stickyRc;
}

void PwmFreqActor::PrintParams()
{
    PwmActor::PrintParams();

    Printf(m_PrintPri, "\tDisablePowerCapping: %s\n", m_DisablePowerCapping ? "true" : "false");
    Printf(m_PrintPri, "\tDvGain: %0.8f\n", m_Gains.DvGain);
    Printf(m_PrintPri, "\tIgnorePowerErrors: %s\n", m_IgnorePowerErrors ? "true" : "false");
    Printf(m_PrintPri, "\tIgnoreThermalErrors: %s\n", m_IgnoreThermalErrors ? "true" : "false");
    Printf(m_PrintPri, "\tIntGain: %0.8f\n", m_Gains.IntGain);
    Printf(m_PrintPri, "\tLowerPerfPoint: P%u.%s\n",
        m_LowerPerfPoint.PStateNum, Perf::SpecialPtToString(m_LowerPerfPoint.SpecialPt));
    Printf(m_PrintPri, "\tPowerTargetmW: %u\n", m_PowerTargetmW);
    Printf(m_PrintPri, "\tPropGain: %0.8f\n", m_Gains.PropGain);
    Printf(m_PrintPri, "\tTargetPerfPoint: P%u.%s\n",
        m_TargetPerfPoint.PStateNum, Perf::SpecialPtToString(m_TargetPerfPoint.SpecialPt));

    m_PowerTempController.PrintParams();

    for (const auto& pSampler : m_PowerTempController.GetSamplers())
    {
        Printf(m_PrintPri, "\t%s limit: %0.3f%s\n",
            pSampler->GetName().c_str(), pSampler->GetExpectedAvg(), pSampler->GetUnits());
    }
}

RC PwmFreqActor::ParseControllerParams(JSObject* pParamsObj)
{
    RC rc;
    StickyRC stickyRc;

    JavaScriptPtr pJs;

    // Gain constants used for tuning the PID controller
    pJs->GetProperty(pParamsObj, "PropGain", &m_Gains.PropGain);
    pJs->GetProperty(pParamsObj, "IntegralGain", &m_Gains.IntGain);
    pJs->GetProperty(pParamsObj, "DerivativeGain", &m_Gains.DvGain);

    if (!(m_Gains.PropGain && m_Gains.IntGain))
    {
        return IlwalidInputErrMsg("PI gains");
    }

    if (m_DisablePowerCapping && !m_PowerTargetmW)
    {
        if (OK != m_pPower->GetPowerCap(Power::TGP, &m_PowerTargetmW) ||
            !m_PowerTargetmW)
        {
            Printf(Tee::PriError, "Cannot find TGP policy\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    m_PowerTempController = PowerTempController(m_Gains, m_PrintPri);

    m_PowerTempController.SetDutyHint(m_DutyHint);

    PowerSampler* pPowerSampler = new PowerSampler(m_pPower);
    pPowerSampler->SetExpectedAvg(static_cast<FLOAT32>(m_PowerTargetmW));
    m_PowerTempController.AddSampler(pPowerSampler);

    vector<ThermalSampler> thermSamplers;
    jsval tlimitsJsval;
    if (pJs->GetPropertyJsval(pParamsObj, "ThermalLimits", &tlimitsJsval) == OK)
    {
        // Parse the thermal controller/sampler properties
        UINT32 thermalSamplerSkipCount = 5;
        pJs->GetProperty(pParamsObj, "ThermalSamplerSkipCount", &thermalSamplerSkipCount);
        if (!thermalSamplerSkipCount)
        {
            return IlwalidInputErrMsg("ThermalSamplerSkipCount");
        }

        UINT32 thermalSkipCount = 5;
        pJs->GetProperty(pParamsObj, "ThermalPeriodSkipCount", &thermalSkipCount);
        if (!thermalSkipCount)
        {
            return IlwalidInputErrMsg("ThermalPeriodSkipCount");
        }
        m_PowerTempController.SetThermalSkipCount(thermalSkipCount);

        FLOAT32 thermMarginDegC = 5.0f;
        pJs->GetProperty(pParamsObj, "ThermalMarginDegC", &thermMarginDegC);
        if (!thermMarginDegC)
        {
            return IlwalidInputErrMsg("ThermalMarginDegC");
        }
        m_PowerTempController.SetThermalMarginDegC(thermMarginDegC);

        // Use 35% of PowerTarget as the maximum thermal compensation
        FLOAT32 thermBudgetmW = 0.35f * m_PowerTargetmW;
        pJs->GetProperty(pParamsObj, "MaxThermalCompensationmW", &thermBudgetmW);
        if (!thermBudgetmW)
        {
            return IlwalidInputErrMsg("MaxThermalCompensationmW");
        }
        m_PowerTempController.SetMaxThermalCompensationmW(thermBudgetmW);

        // Create a sampler for each thermal limit
        JsArray tLimitsArray;
        CHECK_RC(pJs->FromJsval(tlimitsJsval, &tLimitsArray));
        for (const auto& tlimitJsval : tLimitsArray)
        {
            JSObject* pThermCtrlObj = nullptr;
            CHECK_RC(pJs->FromJsval(tlimitJsval, &pThermCtrlObj));

            UINT32 chIdx = ~0U;
            stickyRc = pJs->GetProperty(pThermCtrlObj, "ChannelIndex", &chIdx);

            FLOAT32 targetDegC = -1.0f;
            stickyRc = pJs->GetProperty(pThermCtrlObj, "TargetDegC", &targetDegC);

            ThermalSampler* pThermSampler = new ThermalSampler(
                GetGpuSubdev()->GetThermal(), chIdx);

            pThermSampler->SetExpectedAvg(targetDegC);
            pThermSampler->SetSkipCount(thermalSamplerSkipCount);

            // Measure a sample now to verify that the channel index is valid
            rc = pThermSampler->MeasureSample();
            if (rc != RC::OK)
            {
                delete pThermSampler;
                return rc;
            }
            pThermSampler->AcquireSamples();

            m_PowerTempController.AddSampler(pThermSampler);
        }
    }

    return stickyRc;
}

RC PwmFreqActor::Setup()
{
    if (!m_pPerf)
    {
        Printf(Tee::PriError,
            "PwmFreqActor: Perf object is not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    vector<UINT32> availablePStates;
    CHECK_RC(m_pPerf->GetAvailablePStates(&availablePStates));
    UINT32 highestPState = *min_element(std::begin(availablePStates),
                                        std::end(availablePStates));
    m_TargetPerfPoint.PStateNum = highestPState;
    m_TargetPerfPoint.SpecialPt = Perf::GpcPerf_MAX;
    m_LowerPerfPoint.PStateNum = highestPState;
    m_LowerPerfPoint.SpecialPt = Perf::GpcPerf_INTERSECT;

    CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&m_OriPerfPoint));

    if (m_DisablePowerCapping)
    {
        // Set a HardLock so that our clock requests override power-capping
        m_OrigPerfLock = m_pPerf->GetPStateLockType();
        CHECK_RC(m_pPerf->SetForcedPStateLockType(Perf::HardLock));
    }
    else
    {
        // Set ClkUpScale to 1 so that RM power-capping will rebound gpcclk quickly
        CHECK_RC(m_pPower->IsClkUpScaleSupported(&m_HasClkUpScale));
        if (m_HasClkUpScale)
        {
            CHECK_RC(m_pPower->GetClkUpScaleFactor(&m_OriClkUpScale));
            CHECK_RC(m_pPower->SetClkUpScaleFactor(0x1000));
        }
        if (m_PowerTargetmW)
        {
            vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(1);
            statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
            CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, GetGpuSubdev()));
            if (statuses[0].output.bEnabled && statuses[0].output.value)
            {
                CHECK_RC(m_pPower->GetPowerCap(Power::TGP, &m_OrigPowerTargetmW));
                CHECK_RC(m_pPower->SetPowerCap(Power::TGP, m_PowerTargetmW));
            }
        }
    }

    CHECK_RC(m_pAvfs->GetEnabledFreqControllers(&m_ClfcClkDoms));
    CHECK_RC(m_pAvfs->SetFreqControllerEnable(m_ClfcClkDoms, false));
    CHECK_RC(m_pAvfs->GetEnabledVoltControllers(&m_ClvcVoltDoms));
    CHECK_RC(m_pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, false));

    return rc;
}

RC PwmFreqActor::Cleanup()
{
    StickyRC sticky;

    if (m_pPower)
    {
        if (m_HasClkUpScale && m_OriClkUpScale)
        {
            sticky = m_pPower->SetClkUpScaleFactor(m_OriClkUpScale);
        }
        if (m_OrigPowerTargetmW)
        {
            sticky = m_pPower->SetPowerCap(Power::TGP, m_OrigPowerTargetmW);
        }
    }

    if (m_pAvfs)
    {
        sticky = m_pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, true);
        m_ClvcVoltDoms.clear();
        sticky = m_pAvfs->SetFreqControllerEnable(m_ClfcClkDoms, true);
        m_ClfcClkDoms.clear();
    }

    m_PowerTempController.Reset();

    return sticky;
}

RC PwmFreqActor::Stop()
{
    StickyRC stickyRC;

    stickyRC = PwmActor::Stop();

    if (m_pPerf)
    {
        if (m_OriPerfPoint.PStateNum != Perf::ILWALID_PSTATE)
        {
            stickyRC = m_pPerf->SetPerfPoint(m_OriPerfPoint);
        }
        if (m_OrigPerfLock != Perf::NotLocked)
        {
            stickyRC = m_pPerf->SetForcedPStateLockType(m_OrigPerfLock);
        }
    }

    return stickyRC;
}

RC PwmFreqActor::PerformTargetAction()
{
    RC rc;
    CHECK_RC(m_pPerf->SetPerfPoint(m_TargetPerfPoint));
    Printf(m_PrintPri, "Switched to %s\n", (m_pPerf->GetInflectionPtStr()).c_str());
    return rc;
}

RC PwmFreqActor::PerformLowerAction()
{
    RC rc;
    CHECK_RC(m_pPerf->SetPerfPoint(m_LowerPerfPoint));
    Printf(m_PrintPri, "Switched to %s\n", (m_pPerf->GetInflectionPtStr()).c_str());
    return rc;
}

// Uses a PID controller to try and find an optimal duty cycle to meet m_PowerTargetmW
// over the long term.
void PwmFreqActor::CallwlateDutyCycle()
{
    m_DutyCyclePct = m_PowerTempController.CallwlateDutyCycle();
    MASSERT(m_DutyCyclePct >= 1.0f);
    MASSERT(m_DutyCyclePct <= m_PowerTempController.GetMaxDutyPct());

    // If we are limited by thermals, make sure that the target duty cycle
    // stays at or above the failure threshold.
    if (m_DutyCyclePct < m_DutyCycleFailThreshold &&
        GetDutyCycleLimiter() == DutyCycleLimiter::THERMAL)
    {
        m_DutyCyclePct = m_DutyCycleFailThreshold;
    }

    if (GetForceMinDutyCycle())
    {
        m_DutyCyclePct = 1.0f;
    }

    Printf(m_PrintPri, "New duty cycle pct = %.3f%%\n", m_DutyCyclePct);

    m_TargetActionDurationUs =
        static_cast<UINT32>(m_DutyCyclePeriodUs * m_DutyCyclePct * 0.01f);
    m_LowerActionDurationUs = m_DutyCyclePeriodUs - m_TargetActionDurationUs;

    return;
}

bool PwmFreqActor::RequiresSamplerThread() const
{
    // If we have to adjust the duty cycle dynamically, we need to sample power
    // in the background.
    return m_AdjustDutyCyle;
}

void PwmFreqActor::SamplerThread()
{
    m_SamplerRc.Clear();
    Tasker::DetachThread detach;

    UINT64 numSamples = 0;
    while (GetIsRunning() && Abort::Check() == OK)
    {
        for (const auto& pSampler : m_PowerTempController.GetSamplers())
        {
            if (numSamples % pSampler->GetSkipCount() == 0)
            {
                m_SamplerRc = pSampler->MeasureSample();
                if (m_SamplerRc != OK)
                {
                    Printf(Tee::PriError, "%s sampler error\n", pSampler->GetName().c_str());
                    return;
                }
            }
        }
        numSamples++;
        Utility::SleepUS(m_SamplerPeriodUs);
    }
}

PwmFreqActor::DutyCycleLimiter PwmFreqActor::GetDutyCycleLimiter() const
{
    if (m_PowerTempController.GetThermalErrDegC() > 0.0f)
        return DutyCycleLimiter::THERMAL;
    else
        return DutyCycleLimiter::POWER;
}

void PwmFreqActor::PrintStats() const
{
    PwmActor::PrintStats();
    m_PowerTempController.PrintSamplerStats(m_StatsPrintPri);
}

bool PwmFreqActor::IsOverTempLimit() const
{
    return m_PowerTempController.GetThermalErrDegC() >
        m_PowerTempController.GetThermalMarginDegC();
}

RC PwmFreqActor::ExelwtePwm()
{
    RC rc;

    UINT64 BeforeTick = Xp::QueryPerformanceCounter();
    CHECK_RC(PerformTargetAction());
    UINT64 AfterTick = Xp::QueryPerformanceCounter();
    m_TargetActionTimeTakenUs =
        CallwlateElapsedTimeUsFromTicks(BeforeTick, AfterTick);

    Printf(m_PrintPri, "Performing target action took %u us\n",
        m_TargetActionTimeTakenUs);

    const UINT32 targetDelayUs = m_TargetActionDurationUs > m_TargetActionTimeTakenUs ?
        m_TargetActionDurationUs - m_TargetActionTimeTakenUs : 0;
    Utility::SleepUS(targetDelayUs);
    AfterTick = Xp::QueryPerformanceCounter();
    m_TotalTargetTimeUs += CallwlateElapsedTimeUsFromTicks(BeforeTick, AfterTick);

    BeforeTick = Xp::QueryPerformanceCounter();
    CHECK_RC(PerformLowerAction());
    AfterTick = Xp::QueryPerformanceCounter();
    m_LowerActionTimeTakenUs =
        CallwlateElapsedTimeUsFromTicks(BeforeTick, AfterTick);
    Printf(m_PrintPri, "Performing lower action took %u us\n",
        m_LowerActionTimeTakenUs);

    const UINT32 lowerDelayUs = m_LowerActionDurationUs > m_LowerActionTimeTakenUs ?
        m_LowerActionDurationUs - m_LowerActionTimeTakenUs : 0;
    Utility::SleepUS(lowerDelayUs);
    AfterTick = Xp::QueryPerformanceCounter();
    m_TotalLowTimeUs += CallwlateElapsedTimeUsFromTicks(BeforeTick, AfterTick);

    m_NumDutyCycles++;

    if (m_AdjustDutyCyle && (m_NumDutyCycles % m_DutyControllerSkipCount == 0))
    {
        m_AvgDutyCycle -= m_AvgDutyCycle / m_CyclesUntilFail;
        CallwlateDutyCycle();
        m_AvgDutyCycle += m_DutyCyclePct / m_CyclesUntilFail;

        // Fail if the running average of m_DutyCyclePct falls below m_DutyCycleFailThreshold
        if (GetDutyCycleLimiter() == DutyCycleLimiter::POWER)
        {
            if (m_AvgDutyCycle < m_DutyCycleFailThreshold &&
                m_NumDutyCycles / m_DutyControllerSkipCount >= m_CyclesUntilFail)
            {
                const Tee::Priority pri = m_IgnorePowerErrors ? m_PrintPri : Tee::PriError;
                Printf(pri, "PwmFreqActor cannot meet power target\n");
                if (!m_IgnorePowerErrors)
                {
                    return RC::POWER_TOO_HIGH;
                }
            }
        }
        else if (IsOverTempLimit()) // DutyCycleLimiter::THERMAL
        {
            const Tee::Priority pri = m_IgnoreThermalErrors ? m_PrintPri : Tee::PriError;
            Printf(pri, "PwmFreqActor exceeded a temperature limit\n");
            if (!m_IgnoreThermalErrors)
            {
                return RC::TEMP_TOO_HIGH;
            }
        }
    }
 
    return rc;
}

GpuSampler::GpuSampler() :
    m_Mutex(Tasker::AllocMutex("GpuSampler::m_Mutex", Tasker::mtxUnchecked))
{
    MASSERT(m_Mutex);
}

GpuSampler::GpuSampler(GpuSampler&& other)
{
    m_Samples = move(other.m_Samples);
    m_SkipCount = other.m_SkipCount;
    m_Mutex = other.m_Mutex;
    m_ExpectedAvg = other.m_ExpectedAvg;
    m_Stats = other.m_Stats;

    other.m_SkipCount = 0;
    other.m_Mutex = nullptr;
    other.m_ExpectedAvg = 0.0f;
    other.m_Stats.Reset();
}

GpuSampler& GpuSampler::operator=(GpuSampler&& other)
{
    m_Samples = move(other.m_Samples);
    m_SkipCount = other.m_SkipCount;
    m_Mutex = other.m_Mutex;
    m_ExpectedAvg = other.m_ExpectedAvg;
    m_Stats = other.m_Stats;

    other.m_SkipCount = 0;
    other.m_Mutex = nullptr;
    other.m_ExpectedAvg = 0.0f;
    other.m_Stats.Reset();

    return *this;
}

GpuSampler::~GpuSampler()
{
    if (m_Mutex)
    {
        Tasker::FreeMutex(m_Mutex);
    }
}

vector<GpuSampler::Sample> GpuSampler::AcquireSamples()
{
    MASSERT(m_Mutex);
    Tasker::MutexHolder mh(m_Mutex);
    vector<Sample> samplesCopy(m_Samples);
    m_Samples.clear();
    if (!samplesCopy.empty())
    {
        Sample lastSample = samplesCopy.back();
        samplesCopy.pop_back();
        m_Samples.push_back(lastSample);
    }
    return samplesCopy;
}

void GpuSampler::AddSample(FLOAT32 sample)
{
    MASSERT(m_Mutex);

    m_Stats.numSamples++;
    if (sample < m_Stats.min)
    {
        m_Stats.min = sample;
    }
    if (sample > m_Stats.max)
    {
        m_Stats.max = sample;
    }

    m_Stats.avg = ((m_Stats.avg * (m_Stats.numSamples - 1)) + sample) / m_Stats.numSamples;

    Tasker::MutexHolder mh(m_Mutex);
    const size_t numSamples = m_Samples.size();
    const UINT64 newTickCount = Xp::QueryPerformanceCounter();
    // Set duration_us for the previous sample
    if (numSamples)
    {
        m_Samples[numSamples - 1].duration_us =
            CallwlateElapsedTimeUsFromTicks(m_LastSampleTickCount, newTickCount);
    }
    m_Samples.emplace_back(sample);
    m_LastSampleTickCount = newTickCount;
}

void GpuSampler::PrintStats(Tee::Priority pri)
{
    Printf(pri, " %s (%s) avg=%.3f min=%.3f max=%.3f numSamples=%llu\n",
        GetName().c_str(), GetUnits(), m_Stats.avg, m_Stats.min, m_Stats.max, m_Stats.numSamples);
     Mle::Print(Mle::Entry::pwm_sampler_stats)
         .name(GetName())
         .unit(GetUnits())
         .min_value(m_Stats.min)
         .max_value(m_Stats.max)
         .avg_value(m_Stats.avg)
         .samples(m_Stats.numSamples);
}

void GpuSampler::ResetStats()
{
    m_Stats.Reset();
}

PowerSampler::PowerSampler(Power* pPower) :
    GpuSampler(),
    m_pPower(pPower),
    m_pLwrSampler(new LwrrentSampler())
{
    
}

PowerSampler::PowerSampler(PowerSampler&& other) :
    GpuSampler(move(other))
{
    m_pPower = other.m_pPower;
    m_pLwrSampler = move(other.m_pLwrSampler);
    other.m_pPower = nullptr;
}

PowerSampler& PowerSampler::operator=(PowerSampler&& other)
{
    GpuSampler::operator=(move(other));

    m_pPower = other.m_pPower;
    m_pLwrSampler = move(other.m_pLwrSampler);
    other.m_pPower = nullptr;
    return *this;
}

RC PowerSampler::MeasureSample()
{
    MASSERT(m_pPower);
    RC rc;

    LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS sample = {};
    CHECK_RC(m_pPower->GetPowerSensorSample(&sample));

    INT32 channelIdx = m_pPower->FindChannelIdx(LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD);
    if (channelIdx != -1)
    {
        m_pLwrSampler->AddSample(static_cast<FLOAT32>(sample.channelStatus[channelIdx].tuple.lwrrmA));
    }
    AddSample(static_cast<FLOAT32>(sample.channelStatus[channelIdx].tuple.pwrmW));

    return rc;
}

string PowerSampler::GetName() const
{
    return "TGP";
}

const char* PowerSampler::GetUnits() const
{
    return "mW";
}

string LwrrentSampler::GetName() const
{
    return "TotalInputLwrrent";
}

const char* LwrrentSampler::GetUnits() const
{
    return "mA";
}

void PowerSampler::PrintStats(Tee::Priority pri) 
{
    GpuSampler::PrintStats(pri);
    m_pLwrSampler->PrintStats(pri);
}

void PowerSampler::ResetStats()
{
    GpuSampler::ResetStats();
    m_pLwrSampler->ResetStats();
}

ThermalSampler::ThermalSampler(Thermal* pTherm, UINT32 chIdx) :
    GpuSampler(),
    m_pTherm(pTherm),
    m_ChIdx(chIdx)
{
}

ThermalSampler::ThermalSampler(ThermalSampler&& other) :
    GpuSampler(move(other))
{
    m_pTherm = other.m_pTherm;
    m_ChIdx = other.m_ChIdx;

    other.m_pTherm = nullptr;
    other.m_ChIdx = 0;
}

ThermalSampler& ThermalSampler::operator=(ThermalSampler&& other)
{
    GpuSampler::operator=(move(other));

    m_pTherm = other.m_pTherm;
    m_ChIdx = other.m_ChIdx;

    other.m_pTherm = nullptr;
    other.m_ChIdx = 0;

    return *this;
}

RC ThermalSampler::MeasureSample()
{
    MASSERT(m_pTherm);
    RC rc;

    ThermalSensors::ChannelTempData tempData;
    CHECK_RC(m_pTherm->GetChipTempViaChannel(m_ChIdx, &tempData));

    AddSample(tempData.temp);

    return rc;
}

string ThermalSampler::GetName() const
{
    return Utility::StrPrintf("THERM_CH_%u", m_ChIdx);
}

const char* ThermalSampler::GetUnits() const
{
    return "degC";
}

DutyController::DutyController(Tee::Priority pri) :
    m_PrintPri(pri)
{
}

void DutyController::AddSampler(GpuSampler* pGpuSampler)
{
    m_Samplers.emplace_back(pGpuSampler);
}

vector<unique_ptr<GpuSampler>>& DutyController::GetSamplers()
{
    return m_Samplers;
}

void DutyController::PrintSamplerStats(Tee::Priority pri) const
{
    Printf(pri, "Sampler statistics:\n");
    for (const auto& pSamp : m_Samplers)
    {
        pSamp->PrintStats(pri);
    }
}

void DutyController::Reset()
{
    for (const auto& pSamp : m_Samplers)
    {
        pSamp->ResetStats();
    }
}

PIDGains::PIDGains(FLOAT32 propGain, FLOAT32 intGain, FLOAT32 dvGain) :
    PropGain(propGain),
    IntGain(intGain),
    DvGain(dvGain)
{
}

PIDDutyController::PIDDutyController(PIDGains gains, Tee::Priority pri) :
    DutyController(pri),
    m_PIDGains(gains)
{
}

FLOAT32 PIDDutyController::CallwlateDutyCycle()
{
    const FLOAT32 error = CallwlateError();

    const FLOAT32 derivativeError = error - m_LastError;
    m_LastError = error;

    m_IntegralError += error;

    // Clamp m_IntegralError so that we do not oversaturate it
    const FLOAT32 maxIntError = m_DutyCyclePct > m_DutyHint ?
        (100.0f - m_DutyHint) / m_PIDGains.IntGain : m_DutyHint / m_PIDGains.IntGain;
    m_IntegralError = min(m_IntegralError, maxIntError);
    m_IntegralError = max(m_IntegralError, -maxIntError);

    const FLOAT32 propTerm = m_PIDGains.PropGain * error;
    const FLOAT32 derivTerm = m_PIDGains.DvGain * derivativeError;
    const FLOAT32 intTerm = m_PIDGains.IntGain * m_IntegralError;
    const char* units = GetUnits();

    Printf(m_PrintPri, "  propError=%.3f%s, propTerm=%.3f\n",
        error, units, propTerm);
    Printf(m_PrintPri, "  intError=%.3f%s, intTerm=%.3f\n",
        m_IntegralError, units, intTerm);
    Printf(m_PrintPri, "  derivError=%.3f%s, derivTerm=%.3f\n",
        derivativeError, units, derivTerm);

    m_DutyCyclePct = m_DutyHint + propTerm + intTerm + derivTerm;
    m_DutyCyclePct = max(1.0f, min(m_MaxDutyPct, m_DutyCyclePct));

    return m_DutyCyclePct;
}

void PIDDutyController::Reset()
{
    m_DutyCyclePct = m_DutyHint;
    m_LastError = 0.0f;
    m_IntegralError = 0.0f;
    m_PeriodCycles = 0;

    DutyController::Reset();
}

PowerTempController::PowerTempController(PIDGains gains, Tee::Priority pri) :
    PIDDutyController(gains, pri)
{
}

FLOAT32 PowerTempController::CallwlateError()
{
    FLOAT32 powerErrormW = 0.0f;
    FLOAT32 thermErrorDegC = 0.0f;

    bool evaluatedThermals = false;
    for (const auto& pSamp : m_Samplers)
    {
        PowerSampler* pPowerSamp = dynamic_cast<PowerSampler*>(pSamp.get());
        ThermalSampler* pThermSamp = dynamic_cast<ThermalSampler*>(pSamp.get());
        if (pThermSamp && (m_PeriodCycles % m_ThermalSkipCount))
            continue;

        auto samples = pSamp->AcquireSamples();
        if (samples.empty())
            continue;
        UINT64 totalDuration_us = 0;
        FLOAT32 sampTotal = 0;
        for (UINT32 ii = 0; ii < samples.size(); ii++)
        {
            totalDuration_us += samples[ii].duration_us;
            sampTotal += samples[ii].value * samples[ii].duration_us;
        }
        const FLOAT32 average = sampTotal / totalDuration_us;

        Printf(m_PrintPri, "%s average = %.3f%s, numSamples=%u\n",
            pSamp->GetName().c_str(),
            average, pSamp->GetUnits(), static_cast<UINT32>(samples.size()));

        if (pPowerSamp)
        {
            powerErrormW = pPowerSamp->GetExpectedAvg() - average;
        }
        else if (pThermSamp)
        {
            evaluatedThermals = true;
            const FLOAT32 tempThermErr = average - pThermSamp->GetExpectedAvg() + m_ThermalMarginDegC;
            if (tempThermErr > thermErrorDegC)
            {
                thermErrorDegC = tempThermErr;
            }
        }
        else
        {
            MASSERT(!"Unknown GpuSampler type");
        }
    }
    m_PeriodCycles++;

    if (evaluatedThermals)
    {
        // Make sure the thermal error falls in the range [0, m_ThermalMarginDegC] so
        // that we don't exceed MaxThermalCompensationmW
        MASSERT(thermErrorDegC >= 0.0f);
        const FLOAT32 clampedErr = min(thermErrorDegC, m_ThermalMarginDegC);

        m_ThermCompensationmW = (clampedErr / m_ThermalMarginDegC) * m_MaxThermalCompensationmW;
        m_ThermalErrDegC = thermErrorDegC;
    }

    if (m_ThermCompensationmW)
    {
        Printf(m_PrintPri, "Thermal error = %.3fdegC, compensation = %.3fmW\n",
            m_ThermalErrDegC, m_ThermCompensationmW);
    }

    return powerErrormW - m_ThermCompensationmW;
}

const char* PowerTempController::GetUnits() const
{
    return "mW";
}

void PowerTempController::Reset()
{
    m_ThermalErrDegC = 0.0f;
    m_ThermCompensationmW = 0.0f;

    PIDDutyController::Reset();
}

void PowerTempController::PrintParams()
{
    Printf(m_PrintPri, "\tThermalSkipCount: %u\n", m_ThermalSkipCount);
    Printf(m_PrintPri, "\tThermalMarginDegC: %0.3f\n", m_ThermalMarginDegC);
    Printf(m_PrintPri, "\tMaxThermalCompensationmW: %0.3f\n", m_MaxThermalCompensationmW);
}

//-----------------------------------------------------------------------------
//                          PwmSlowdownActor methods
//-----------------------------------------------------------------------------

RC PwmSlowdownActor::ParseParams(JSObject* pParamsObj)
{
    RC rc;

    CHECK_RC(PwmActor::ParseParams(pParamsObj));
    
    JavaScriptPtr pJs;
    pJs->GetProperty(pParamsObj, "FreqHz", &m_FreqHz);
    pJs->GetProperty(pParamsObj, "SlowFactor", &m_SlowFactor);
    pJs->GetProperty(pParamsObj, "SlewRate", &m_SlewRate);
    pJs->GetProperty(pParamsObj, "FreqAdjustmentPct", &m_FreqAdjustmentPct);
    pJs->GetProperty(pParamsObj, "ControllerPeriodMs", &m_ControllerPeriodMs);
    pJs->GetProperty(pParamsObj, "OclGpio", &m_OclGpio);
    pJs->GetProperty(pParamsObj, "GpioLogicLevel", reinterpret_cast<UINT32*>(&m_GpioLogicLevel));

    if (m_ActorName.empty())
    {
        m_ActorName = "PwmSlowdownActor";
    }

    return rc;
}

void PwmSlowdownActor::PrintParams()
{
    PwmActor::PrintParams();

    Printf(m_PrintPri, "\tControllerPeriodMs: %u\n", m_ControllerPeriodMs);
    Printf(m_PrintPri, "\tFreqAdjustmentPct: %0.3f\n", m_FreqAdjustmentPct);
    Printf(m_PrintPri, "\tFreqHz: %0.3f\n", m_FreqHz);
    Printf(m_PrintPri, "\tSlowFactor: %u\n", m_SlowFactor);
    Printf(m_PrintPri, "\tSlewRate: %u\n", m_SlewRate);
    Printf(m_PrintPri, "\tOclGpio: %u\n", m_OclGpio);
    Printf(m_PrintPri, "\tGpioLogicLevel: %s\n",
        m_GpioLogicLevel == GpioLogicLevel::ACTIVE_HIGH ?
        "active high" : "active low");
}

RC PwmSlowdownActor::Setup()
{
    RC rc;

    GpuSubdevice* pSubdev = GetGpuSubdev();

    CHECK_RC(pSubdev->ReserveClockThrottle(&m_pClockThrottle));
    CHECK_RC(m_pClockThrottle->Grab());

    LW2080_CTRL_THERM_THERM_MONITORS_INFO_PARAMS monitorInfos = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        pSubdev
       ,LW2080_CTRL_CMD_THERMAL_MONITORS_GET_INFO
       ,&monitorInfos
       ,sizeof(monitorInfos)));

    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &monitorInfos.super.objMask.super)
        const auto& mon = monitorInfos.monitors[ii];
        if (mon.super.type == LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_EDPP_FONLY ||
            mon.super.type == LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_EDPP_VMIN)
        {
            m_EdppMonitorMask |= BIT(ii);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    if (!m_EdppMonitorMask)
    {
        Printf(Tee::PriWarn, "No Droopy-ICL EDPP monitors\n");
    }
    else
    {
        Printf(m_PrintPri, "Found EDPP monitors: mask 0x%08x\n", m_EdppMonitorMask);
        LW2080_CTRL_THERM_THERM_MONITORS_STATUS_PARAMS monitorStatuses = {};
        monitorStatuses.super.objMask.super.pData[0] = m_EdppMonitorMask;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            pSubdev
           ,LW2080_CTRL_CMD_THERMAL_MONITORS_GET_STATUS
           ,&monitorStatuses
           ,sizeof(monitorStatuses)));
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &monitorStatuses.super.objMask.super)
            m_EdppCounters[ii] = monitorStatuses.monitors[ii].counter;
            Printf(m_PrintPri, "EDPP monitor %u initial counter value: %llu\n",
                ii, m_EdppCounters[ii]);
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    }

    if (m_OclGpio != ILWALID_GPIO)
    {
        if (m_OclGpio > 63)
        {
            Printf(Tee::PriError, "OclGpio must be less than 64\n");
            return RC::ILWALID_ARGUMENT;
        }
        m_OclEvents = 0;
    }

    return rc;
}

RC PwmSlowdownActor::Cleanup()
{
    StickyRC stickyRC;

    if (m_pClockThrottle)
    {
        stickyRC = m_pClockThrottle->Release();
        stickyRC = GetGpuSubdev()->ReleaseClockThrottle(m_pClockThrottle);
    }
    return stickyRC;
}

RC PwmSlowdownActor::ExelwtePwm()
{
    RC rc;

    bool spikes = false;
    CHECK_RC(DetectPowerSpikes(&spikes));

    // Adjust and set the ClockThrottle if there are any power spikes and also
    // on the first iteration.
    if (spikes || !m_TotalTargetTimeUs)
    {
        if (spikes)
        {
            m_FreqHz *= (100.0f + m_FreqAdjustmentPct) / 100.0f;
        }

        ClockThrottle::Params requested, actual;
        requested.PulseHz = static_cast<UINT32>(m_FreqHz);
        requested.DutyPct = m_DutyCyclePct;
        requested.SlowFrac = 1.0f / m_SlowFactor;
        requested.GradStepDur = m_SlewRate;
        Printf(m_PrintPri, "%s requests freq=%uHz duty=%f%% slowFrac=%f slew=%u\n",
            m_ActorName.c_str(),
            requested.PulseHz,
            requested.DutyPct,
            requested.SlowFrac,
            requested.GradStepDur);
        CHECK_RC(m_pClockThrottle->Set(&requested, &actual));

        if (actual.PulseHz < static_cast<UINT32>(0.9f * requested.PulseHz))
        {
            Printf(Tee::PriError,
                "Thermal slowdown PWM cannot pulse fast enough (capped to %uHz)\n",
                actual.PulseHz);
            m_FreqHz = static_cast<FLOAT32>(actual.PulseHz);
            return RC::CANNOT_MEET_WAVEFORM;
        }
    }

    Utility::SleepUS(m_ControllerPeriodMs * 1000);

    const FLOAT32 dutyFrac = m_DutyCyclePct / 100.0f;
    m_TotalTargetTimeUs += static_cast<UINT64>(m_ControllerPeriodMs * 1000.0f * dutyFrac);
    m_TotalLowTimeUs += static_cast<UINT64>(m_ControllerPeriodMs * 1000.0f * (1.0f - dutyFrac));
    m_NumDutyCycles += static_cast<UINT64>(m_ControllerPeriodMs * (m_FreqHz / 1000.0f));

    return rc;
}

RC PwmSlowdownActor::DetectPowerSpikes(bool *pSpikes)
{
    MASSERT(pSpikes);
    *pSpikes = false;

    if (m_OclGpio != ILWALID_GPIO)
    {
        const UINT32 lwrOclEvents = atomic_exchange(&m_OclEvents, 0U);
        if (lwrOclEvents)
        {
            *pSpikes = true;
            Printf(m_PrintPri, "%u OCL event(s) detected\n", lwrOclEvents);
        }
    }

    if (!m_EdppMonitorMask)
        return RC::OK;

    RC rc;

    LW2080_CTRL_THERM_THERM_MONITORS_STATUS_PARAMS monitorStatuses = {};
    monitorStatuses.super.objMask.super.pData[0] = m_EdppMonitorMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetGpuSubdev()
       ,LW2080_CTRL_CMD_THERMAL_MONITORS_GET_STATUS
       ,&monitorStatuses
       ,sizeof(monitorStatuses)));
    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &monitorStatuses.super.objMask.super)
        if (monitorStatuses.monitors[ii].counter != m_EdppCounters[ii])
        {
            Printf(m_PrintPri, "EDPP event detected on monitor %u (counter %llu)\n",
                ii, monitorStatuses.monitors[ii].counter);
            m_EdppCounters[ii] = monitorStatuses.monitors[ii].counter;
            *pSpikes = true;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

bool PwmSlowdownActor::RequiresSamplerThread() const
{
    return m_OclGpio != ILWALID_GPIO;
}

void PwmSlowdownActor::SamplerThread()
{
    m_SamplerRc.Clear();
    Tasker::DetachThread detach;
    GpuSubdevice* pSubdev = GetGpuSubdev();
    while (GetIsRunning() && Abort::Check() == RC::OK)
    {
        const bool gpioVal = pSubdev->GpioGetInput(m_OclGpio);
        if ((!gpioVal && m_GpioLogicLevel == GpioLogicLevel::ACTIVE_LOW) ||
            (gpioVal && m_GpioLogicLevel == GpioLogicLevel::ACTIVE_HIGH))
        {
            m_OclEvents++;
        }
        Utility::SleepUS(m_SamplerPeriodUs);
    }
}

//-----------------------------------------------------------------------------
//                                      JS APIs
//-----------------------------------------------------------------------------

static JSBool C_PwmDispatcher_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    PwmDispatcher *pDispatcher = new PwmDispatcher();
    MASSERT(pDispatcher);

    return JS_SetPrivate(cx, obj, pDispatcher);
};

static void C_PwmDispatcher_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    PwmDispatcher *pDispatcher = (PwmDispatcher *)JS_GetPrivate(cx, obj);
    if (pDispatcher)
    {
        delete pDispatcher;
    }
};

static JSClass PwmDispatcher_class =
{
    "PwmDispatcher",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_PwmDispatcher_finalize
};

static SObject PwmDispatcher_Object
(
    "PwmDispatcher",
    PwmDispatcher_class,
    0,
    0,
    "PwmDispatcher JS Object",
    C_PwmDispatcher_constructor
);

JS_STEST_LWSTOM(PwmDispatcher,
                AddActor,
                2,
                "Add a new PWM Actor")
{
    STEST_HEADER(2, 2, "Usage: PwmDispatcher.AddActor(Type, ParamsObj)");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");
    STEST_ARG(0, UINT32, Type);
    STEST_ARG(1, JSObject*, pParamsObj);

    RETURN_RC(pPwmDispatcher->AddActor(static_cast<ActorType>(Type), pParamsObj));
}

JS_STEST_LWSTOM(PwmDispatcher,
                Start,
                0,
                "Run the PWM dispatcher")
{
    STEST_HEADER(0, 0, "Usage: PwmDispatcher.Start()");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");

    RETURN_RC(pPwmDispatcher->Start());
}

JS_STEST_LWSTOM(PwmDispatcher,
                Stop,
                0,
                "Stop the PWM dispatcher")
{
    STEST_HEADER(0, 0, "Usage: PwmDispatcher.Stop()");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");

    RETURN_RC(pPwmDispatcher->Stop());
}

JS_SMETHOD_LWSTOM(PwmDispatcher,
                  GetTotalTargetTimeMs,
                  0,
                  "Get the minimum cumulative target time of all actors")
{
    STEST_HEADER(0, 0, "Usage: let targetTime = PwmDispatcher.GetTotalTargetTimeMs()");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");

    if (OK != pJavaScript->ToJsval(pPwmDispatcher->GetTotalTargetTimeMs(), pReturlwalue))
    {
        pJavaScript->Throw(pContext, RC::SOFTWARE_ERROR,
            "Error in PwmDispatcher.GetTotalTargetTimeMs()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(PwmDispatcher,
                  GetTotalLowTimeMs,
                  0,
                  "Get the minimum cumulative low time of all actors")
{
    STEST_HEADER(0, 0, "Usage: let lowTime = PwmDispatcher.GetTotalLowTimeMs()");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");

    if (OK != pJavaScript->ToJsval(pPwmDispatcher->GetTotalLowTimeMs(), pReturlwalue))
    {
        pJavaScript->Throw(pContext, RC::SOFTWARE_ERROR,
            "Error in PwmDispatcher.GetTotalLowTimeMs()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(PwmDispatcher,
                  IsRunning,
                  0,
                  "True if all PwmActors are running")
{
    STEST_HEADER(0, 0, "Usage: let running = PwmDispatcher.IsRunning()");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");

    if (OK != pJavaScript->ToJsval(pPwmDispatcher->IsRunning(), pReturlwalue))
    {
        pJavaScript->Throw(pContext, RC::SOFTWARE_ERROR,
            "Error in PwmDispatcher.IsRunning()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_STEST_LWSTOM(PwmDispatcher, ForceMinDutyCycle, 0, "Force duty cycle to 1%%")
{
    STEST_HEADER(0, 0, "Usage: PwmDispatcher.ForceMinDutyCycle()");
    STEST_PRIVATE(PwmDispatcher, pPwmDispatcher, "PwmDispatcher");
    pPwmDispatcher->ForceMinDutyCycle();
    RETURN_RC(RC::OK);
}

// Constants related to PwmDispatcher class
JS_CLASS(PwmConst);
static SObject PwmConst_Object
(
    "PwmConst",
    PwmConstClass,
    0,
    0,
    "PwmConst JS Object"
);

PROP_CONST(PwmConst, FREQ_ACTOR, FREQ_ACTOR);
PROP_CONST(PwmConst, SLOWDOWN_ACTOR, SLOWDOWN_ACTOR);
PROP_CONST(PwmConst, ACTIVE_HIGH, static_cast<UINT32>(PwmSlowdownActor::GpioLogicLevel::ACTIVE_HIGH)); //$
PROP_CONST(PwmConst, ACTIVE_LOW, static_cast<UINT32>(PwmSlowdownActor::GpioLogicLevel::ACTIVE_LOW)); //$

