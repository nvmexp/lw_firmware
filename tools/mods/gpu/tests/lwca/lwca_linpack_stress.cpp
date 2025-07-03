/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/
#include "lwda_linpack_cask.h"

class LwdaLinpackStress : public LwdaLinpackCask
{
    public:
        LwdaLinpackStress();
        ~LwdaLinpackStress() override = default;

        RC InitFromJs() override;

        RC Setup() override;
        RC Cleanup() override;

        SETGET_PROP(KernelPeriodNs, UINT32);
        SETGET_PROP(KernelDutyPct,  double);
        SETGET_PROP(MinSleepNs,     UINT32);
        SETGET_PROP(PTimerNs,       UINT32);
        SETGET_PROP(MainLoopCount,  UINT32);

    private:
        static constexpr UINT32 PTIMER_MIN_TICK_NS = 32;
        static constexpr UINT32 PTIMER_MAX_TICK_NS = 4096;
        static const KernelMap s_KernelMap;

        void PrintJsProperties(Tee::Priority pri);
        const KernelMap& GetKernelMap() const override;
        UINT64 CalcOpCount(const UINT32 mSize,
                           const UINT32 nSize,
                           const UINT32 kSize,
                           const bool initialize) const override;
        RC InitCaskGemm(cask::Gemm* pGemm, bool initialize) override;
        UINT32 m_InitTimerFreq = 0;
        bool m_PulseParamsPrinted = false;

        // JS properties
        UINT32 m_KernelPeriodNs = 1024 * 1024; //   1 ms
        double m_KernelDutyPct = 30.0;         // 300 us
        UINT32 m_MinSleepNs = 128;
        UINT32 m_PTimerNs = PTIMER_MIN_TICK_NS;
        UINT32 m_MainLoopCount = 0;
};

const LwdaLinpackCask::KernelMap LwdaLinpackStress::s_KernelMap =
{
    // HMMA FP16<-FP16
    {"sm80_xmma_mods_gemm_f16f16_f16f32_f32_nt_n_tilesize128x128x64_stage2_warpsize2x2x1_tensor16x8x16",
        {Instr::HMMA_16816_F32_F16, ""}},
};

LwdaLinpackStress::LwdaLinpackStress()
{
    SetName("LwdaLinpackStress");
}

const LwdaLinpackCask::KernelMap& LwdaLinpackStress::GetKernelMap() const
{
    return s_KernelMap;
}

RC LwdaLinpackStress::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaLinpackCask::InitFromJs());

    // Fail for incompatible linpack modes
    if (m_TestMode != MODE_NORMAL)
    {
        Printf(Tee::PriError,
               "Only TestMode 0 (MODE_NORMAL) is lwrrently supported in %s\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    // Check test-specific args
    if (!m_KernelPeriodNs || (m_KernelPeriodNs & (m_KernelPeriodNs - 1)))
    {
        Printf(Tee::PriError, "KernelPeriodNs (%d) must be a non-zero power of 2\n", m_KernelPeriodNs);
        return RC::BAD_PARAMETER;
    }

    if ((m_PTimerNs < PTIMER_MIN_TICK_NS || m_PTimerNs > PTIMER_MAX_TICK_NS) ||
        (m_PTimerNs & (m_PTimerNs - 1)))
    {
        Printf(Tee::PriError,
               "PTimerNs (%d) must be a power of 2 between 32 and 4096\n", m_PTimerNs);
        return RC::BAD_PARAMETER;
    }

    if (!m_KernelDutyPct)
    {
        Printf(Tee::PriError, "KernelDutyPct must be non-zero\n");
        return RC::BAD_PARAMETER;
    }

    return rc;
}

void LwdaLinpackStress::PrintJsProperties(Tee::Priority pri)
{
    LwdaLinpackCask::PrintJsProperties(pri);
    Printf(pri, "   KernelPeriodNs:       %u\n", m_KernelPeriodNs);
    Printf(pri, "   KernelDutyPct:        %f\n", m_KernelDutyPct);
    Printf(pri, "   MinSleepNs:           %u\n", m_MinSleepNs);
    Printf(pri, "   PTimerNs:             %u\n", m_PTimerNs);
    if (m_MainLoopCount)
    {
        Printf(pri, "   MainLoopCount:        %u\n", m_MainLoopCount);
    }
}

RC LwdaLinpackStress::Setup()
{
    // Override PTIMER frequency to allow for finer resolution testing
    RegHal& regs = GetBoundGpuSubdevice()->Regs();
    if (!Platform::IsVirtFunMode() && !Platform::UsesLwgpuDriver() &&
        regs.IsSupported(MODS_PTIMER_GR_TICK_FREQ))
    {
        const UINT32 freqVal = Utility::Log2i(m_PTimerNs / PTIMER_MIN_TICK_NS);

        // Backup previous timer freq
        // (This test is device exclusive so we can avoid worrying about race conditions)
        m_InitTimerFreq = regs.Read32(MODS_PTIMER_GR_TICK_FREQ_SELECT);
        // Override timer freq
        VerbosePrintf("Overriding PTIMER TICK_FREQ_SELECT %d->%d\n",
                      m_InitTimerFreq, freqVal);
        regs.Write32(MODS_PTIMER_GR_TICK_FREQ_SELECT, freqVal);
    }
    return LwdaLinpackCask::Setup();
}

RC LwdaLinpackStress::Cleanup()
{
    // Restore PTIMER frequency
    RegHal& regs = GetBoundGpuSubdevice()->Regs();
    if (!Platform::IsVirtFunMode() && !Platform::UsesLwgpuDriver() &&
        regs.IsSupported(MODS_PTIMER_GR_TICK_FREQ))
    {
        const UINT32 freqVal = Utility::Log2i(m_PTimerNs / PTIMER_MIN_TICK_NS);

        // Restore previous timer freq
        VerbosePrintf("Restoring PTIMER TICK_FREQ_SELECT %d->%d\n",
                      freqVal, m_InitTimerFreq);
        regs.Write32(MODS_PTIMER_GR_TICK_FREQ_SELECT, m_InitTimerFreq);
        m_InitTimerFreq = 0;
    }
    return LwdaLinpackCask::Cleanup();
}

UINT64 LwdaLinpackStress::CalcOpCount(const UINT32 mSize,
                                      const UINT32 nSize,
                                      const UINT32 kSize,
                                      const bool initialize) const
{
    const UINT64 opsPerMainLoop = static_cast<UINT64>(mSize) * nSize * (kSize * 2 - 1);
    const UINT64 opsPerEpilogue = static_cast<UINT64>(mSize) * nSize * 3;
    if (initialize)
    {
        // Init kernel launches only need the main GEMM loop
        // Scale the GEMM with MainLoopCount
        return opsPerMainLoop * m_MainLoopCount;
    }
    else
    {
        // Main kernel launches have an eplogue to scale Alpha/Beta
        // Scale the GEMM with MainLoopCount
        return opsPerMainLoop * m_MainLoopCount + opsPerEpilogue;
    }
}

RC LwdaLinpackStress::InitCaskGemm(cask::Gemm* pGemm, bool initialize)
{
    RC rc;

    // Common GEMM init
    CHECK_RC(LwdaLinpackCask::InitCaskGemm(pGemm, initialize));

    // If MainLoopCount wasn't set target a time based on the current clock
    // (Use a fixed value of loops if clocks aren't supported)
    if (m_MainLoopCount == 0)
    {
        Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
        bool bClocksSupported = (Platform::GetSimulationMode() != Platform::Fmodel &&
                                 !Platform::IsVirtFunMode() && pPerf->IsPState30Supported());
        if (!bClocksSupported)
        {
            m_MainLoopCount = 1024;
        }
        else
        {
            // Callwlate estimated arch throughput
            const double archEfficiency = GetBoundGpuSubdevice()->GetArchEfficiency(m_Instr);
            const double issueRate = GetBoundGpuSubdevice()->GetIssueRate(m_Instr);
            UINT64 clocksPerSec = 0;
            CHECK_RC(pPerf->MeasureCoreClock(Gpu::ClkGpc, &clocksPerSec));
            const double archOpCountPerSec =
                archEfficiency * issueRate * GetBoundLwdaDevice().GetShaderCount() * clocksPerSec;

            // Use operations per loop and arch throughput to estimate the value of MainLoopCount
            // needed to achieve a kernel runtime of ~100 ms
            constexpr double targetRuntimeSec = 0.1;
            const double opsPerLoop = static_cast<double>(m_Msize) * m_Nsize * (m_Ksize * 2 - 1);
            const double loopsPerSec = ((m_KernelDutyPct / 100.0 ) * archOpCountPerSec) /
                                       (opsPerLoop);
            VerbosePrintf("Target Kernel Runtime (Sec) = %f\n", targetRuntimeSec);
            m_MainLoopCount = static_cast<UINT32>(ceil(targetRuntimeSec * loopsPerSec)); 
        }
    }

    // Configure pulsing parameters
    const UINT32 periodMask = m_KernelPeriodNs - 1;
    const UINT32 pulseNs = static_cast<UINT32>(m_KernelPeriodNs * (m_KernelDutyPct / 100.0));
    if (!m_PulseParamsPrinted)
    {
        VerbosePrintf("Target MainLoopCount = %d\n", m_MainLoopCount);
        VerbosePrintf("Target PeriodNs      = %u\n", m_KernelPeriodNs);
        VerbosePrintf("Target PulseNs       = %u\n", pulseNs);
        m_PulseParamsPrinted = true;
    }
    pGemm->setModsRuntimeParams(periodMask, pulseNs, m_MinSleepNs, m_MainLoopCount);
    return rc;
}

JS_CLASS_INHERIT(LwdaLinpackStress, LwdaLinpackCask, "Run CASK-derived mods-specific GEMM tests.");
CLASS_PROP_READWRITE(LwdaLinpackStress, KernelPeriodNs, UINT32, "");
CLASS_PROP_READWRITE(LwdaLinpackStress, KernelDutyPct,  double, "");
CLASS_PROP_READWRITE(LwdaLinpackStress, MinSleepNs, UINT32, "");
CLASS_PROP_READWRITE(LwdaLinpackStress, PTimerNs, UINT32, "");
CLASS_PROP_READWRITE(LwdaLinpackStress, MainLoopCount, UINT32, "");
