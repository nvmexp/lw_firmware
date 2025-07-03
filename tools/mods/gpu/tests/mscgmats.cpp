/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mscgmats.h"
#include "core/include/color.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pwrwait.h"

MscgMatsTest::MscgMatsTest()
{
    SetName("MscgMatsTest");
    SetReportBandwidth(false);
}

bool MscgMatsTest::IsSupported()
{
    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    if (!NewWfMatsTest::IsSupported())
    {
        return false;
    }

    if (pSubdev->IsSOC())
    {
        return false;
    }

    std::unique_ptr<LowPowerPgBubble> pPgBubble = make_unique<LowPowerPgBubble>(pSubdev);
    // TODO: Add force pstate support in feature support check
    if (!pPgBubble->IsSupported(LowPowerPgWait::MSCG))
    {
        return false;
    }

    return true;
}

RC MscgMatsTest::Setup()
{
    RC rc;

    // Allocate a single pixel of FB memory
    m_Pixel.SetWidth(1);
    m_Pixel.SetHeight(1);
    m_Pixel.SetColorFormat(ColorUtils::A8R8G8B8);
    CHECK_RC(m_Pixel.Alloc(GetBoundGpuDevice()));
    m_Pixel.Map(Gpu::UNSPECIFIED_SUBDEV, GetBoundRmClient());

    CHECK_RC(NewWfMatsTest::Setup());

    if (Perf::ILWALID_PSTATE != m_ForcePstate)
    {
        Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
        CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_RestorePerfPt));
        CHECK_RC(pPerf->ForcePState(
            m_ForcePstate,
            LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR
        ));
    }

    m_LowPowerPgBubble = make_unique<LowPowerPgBubble>(GetBoundGpuSubdevice());
    m_LowPowerPgBubble->SetFeatureId(LowPowerPgWait::MSCG);
    CHECK_RC(m_LowPowerPgBubble->Setup(
        GetBoundGpuSubdevice()->GetDisplay(), nullptr));
    CHECK_RC(m_LowPowerPgBubble->PrintIsSupported(GetVerbosePrintPri()));
    m_LowPowerPgBubble->SetDelayAfterBubble(m_DelayAfterBubbleMs);

    return rc;
}

RC MscgMatsTest::Run()
{
    RC rc;

    UINT32 initialCount;
    CHECK_RC(m_LowPowerPgBubble->GetEntryCount(&initialCount));

    RunLiveFbioTuning();

    UINT32 finalCount;
    CHECK_RC(m_LowPowerPgBubble->GetEntryCount(&finalCount));

    if (finalCount < initialCount)
    {
        Printf(Tee::PriHigh, "MSCG entry count decreased from %u to %u!\n",
            initialCount, finalCount);
        return RC::LWRM_ERROR;
    }

    UINT32 runCount = finalCount - initialCount;
    FLOAT64 successPercent = min(100.0, (100.0 * runCount) /
        (m_MscgEntryAttempts ? m_MscgEntryAttempts : 1));

    Printf(
        GetVerbosePrintPri(),
        "MSCG entries:\n"
        "\tbefore test           = %u\n"
        "\tafter test            = %u\n"
        "\tduring test (delta)   = %u\n"
        "\tattempted during test = %u\n"
        "\tsuccess rate          = %.2f%%\n",
        initialCount,
        finalCount,
        runCount,
        m_MscgEntryAttempts,
        successPercent
    );

    if (successPercent < m_MinSuccessPercent)
    {
        Printf( Tee::PriHigh,
                "MSCG entry success rate of %f%% is below minimum of %f%%!\n",
                successPercent, m_MinSuccessPercent );

        return RC::EXCEEDED_EXPECTED_THRESHOLD;
    }

    return rc;
}

RC MscgMatsTest::Cleanup()
{
    StickyRC stickyRc;

    stickyRc = m_LowPowerPgBubble->Cleanup();

    // If we failed out of Setup before setting m_RestorePerfPt, then it's
    // possible that m_ForcePstate is not ILWALID_PSTATE but m_RestorePerfPt's
    // PStateNum is still ILWALID_PSTATE (set in the PerfPoint constructor),
    // meaning we never actually forced the pstate. So only restore the pstate
    // if m_RestorePerfPt.PStateNum is not ILWALID_PSTATE.
    if (Perf::ILWALID_PSTATE != m_RestorePerfPt.PStateNum)
    {
        Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
        stickyRc = pPerf->SetPerfPoint(m_RestorePerfPt);
    }

    stickyRc = NewWfMatsTest::Cleanup();

    m_Pixel.Free();

    return stickyRc;
}

void MscgMatsTest::PrintJsProperties(Tee::Priority pri)
{
    NewWfMatsTest::PrintJsProperties(pri);

    Printf(pri, "\t%-32s %f\n", "SleepMs:",           m_SleepMs);
    Printf(pri, "\t%-32s %u\n", "SleepLoops:",        m_SleepLoops);
    Printf(pri, "\t%-32s %f\n", "MinSuccessPercent:", m_MinSuccessPercent);

    if (Perf::ILWALID_PSTATE == m_ForcePstate)
        Printf(pri, "\t%-32s %s\n", "ForcePstate:", "none");
    else
        Printf(pri, "\t%-32s %u\n", "ForcePstate:", m_ForcePstate);
}

RC MscgMatsTest::SubroutineSleep()
{
    RC rc;

    CHECK_RC(WaitForBlitLoopDone(false, false));

    // Pulse MSCG on and off by repeatedly accessing m_Pixel and then sleeping

    void *address = m_Pixel.GetAddress();
    UINT32 pattern = m_PixelPattern;

    for (UINT32 i = 0; i < m_SleepLoops; i++)
    {
        CHECK_RC(m_LowPowerPgBubble->SwitchMode(GetVerbosePrintPri()));

        MEM_WR32(address, pattern);

        CHECK_RC(m_LowPowerPgBubble->ActivateBubble(m_SleepMs));

        if (MEM_RD32(address) != pattern)
            return RC::BAD_MEMORY;

        CHECK_RC(m_LowPowerPgBubble->ActivateBubble(m_SleepMs));

        m_LowPowerPgBubble->PrintStats(GetVerbosePrintPri());

        pattern = ~pattern;
    }

    m_MscgEntryAttempts += 2u * m_SleepLoops; // 2 sleeps per iteration above

    return rc;
}

JS_CLASS_INHERIT(MscgMatsTest, NewWfMatsTest,
                 "Memory test intended to exercise MSCG");

CLASS_PROP_READWRITE(MscgMatsTest, SleepMs, FLOAT64,
                     "Milliseconds to sleep so MSCG can engage");

CLASS_PROP_READWRITE(MscgMatsTest, SleepLoops, UINT32,
                     "Number of times to try to trigger MSCG each blit loop");

CLASS_PROP_READWRITE(MscgMatsTest, MinSuccessPercent, double,
                     "Fail if (times enterered MSCG) / (times tried to enter MSCG) < MinSuccessPercent / 100");

CLASS_PROP_READWRITE(MscgMatsTest, ForcePstate, UINT32,
                     "Force to pstate x.intersect");
CLASS_PROP_READWRITE(MscgMatsTest, DelayAfterBubbleMs, FLOAT64,
                     "Delay after finishing Pg bubble");
