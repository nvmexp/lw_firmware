/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2014,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file ClockPulse test
//!
//! Program the GPU's thermal-slowdown feature to "pulse" the effective clock
//! speed and thus provide a stress to the power supply.
//!
//! Intended to be run as a background test, while graphics tests run in other
//! threads, to detect any data corruption caused by power-supply glitches.

#include "gpu/tests/gputest.h"
#include "gpu/perf/clockthrottle.h"
#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/perf/perfutil.h"

//------------------------------------------------------------------------------
class ClockPulseTest : public GpuTest
{
public:
    ClockPulseTest();
    virtual ~ClockPulseTest();
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // JS config values:
    GpuTestConfiguration *  m_pTstCfg;
    FancyPicker::FpContext* m_pFpCtx;
    FancyPickerArray *      m_pPickers;
    ClockThrottle  *        m_pClockThrottle;
    PStateOwner             m_PStateOwner;
    bool                    m_KeepRunning;
    bool                    m_Debug;
    UINT32                  m_DomainMask;

public:
    // JS interface functions:
    RC SetDefaultsForPicker(UINT32 idx);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(Debug, bool);
    SETGET_PROP(DomainMask, UINT32);

    string GetLwrrFreqLabel() { return m_pClockThrottle->GetLwrrFreqLabel(); }
    UINT32 GetLwrrFreqIdx()   { return m_pClockThrottle->GetLwrrFreqIdx  (); }
    UINT32 GetLwrrFreqInt()   { return m_pClockThrottle->GetLwrrFreqInt  (); }
    UINT32 GetNumFreq()       { return m_pClockThrottle->GetNumFreq      (); }
};

//------------------------------------------------------------------------------
ClockPulseTest::ClockPulseTest()
:  m_pClockThrottle(0)
   ,m_KeepRunning(false)
   ,m_Debug(false)
   ,m_DomainMask(0)
{
    SetName("ClockPulseTest");
    m_pTstCfg = GetTestConfiguration();
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_CPULSE_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

//------------------------------------------------------------------------------
ClockPulseTest::~ClockPulseTest()
{
}

//------------------------------------------------------------------------------
bool ClockPulseTest::IsSupported()
{
    return GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_CLOCK_THROTTLE);
}

//------------------------------------------------------------------------------
RC ClockPulseTest::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[idx];
    UINT32 onesource = 1620000000;
    INT32 LowDivider  = 11;  // Divider for +33% 108 Mhz (ONESOURCE0/11 == 147.2 Mhz)
    INT32 HighDivider = 22;  // Divider for -33% 108 Mhz (ONESOURCE0/22 ==  73.6 Mhz)
    INT32 MaxCountLog = 17;  // maximum number of Utilsclk we can use to construct waveform
    //INT32 MinCountLog =  2;  // minimum number of Utilsclk we can use to construct waveform
    //there appears to be a HW limitation, prevent used of highest 3 octaves
    INT32 MinCountLog =  5;  // minimum number of Utilsclk we can use to construct waveform
    switch (idx)
    {
    case FPK_CPULSE_FREQ_HZ:
        // Please see gpu/utility/gf100thr.cpp:InitTables for a more full explaination.
        // Set default pulse frequency list to hit all possible Fermi pulse rates.
        // Other chips will use the closest reachable freq for that chip.
        fp.ConfigList();
        for(INT32 i=MaxCountLog; i>=MinCountLog; i--)
        {
            for (FLOAT32 j=HighDivider-0.5f; j>=LowDivider; j-=0.5f)
            {
                //for some reason we get x4 clks
                LwU32 temp = (UINT32)(onesource/j/(1<<i))/*/4*/;
                fp.AddListItem( temp );
            }
        }
        break;

    case FPK_CPULSE_DUTY_PCT:
        fp.ConfigConst(50);
        break;

    case FPK_CPULSE_DWELL_MSEC:
        fp.ConfigConst(100);
        break;

    case FPK_CPULSE_SLOW_RATIO:
        // Default to highest slowdown ratio supported by the gpu
        fp.FConfigConst(0.5F);
        break;

    case FPK_CPULSE_GRAD_STEP_DUR:
        // Use the fastest/harshest "advanced" mode for gf100.
        fp.ConfigConst(32);
        break;

    case FPK_CPULSE_VERBOSE:
        fp.ConfigConst(0);
        break;

    default:
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC ClockPulseTest::Setup()
{
    RC rc;

    // Fail if wrong SW branch for this HW.

    CHECK_RC(GpuTest::Setup());

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());

    // Reserve PState changing privileges for this test.
    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));

    // Reserve the clock-pwm registers for this test.
    CHECK_RC(GetBoundGpuSubdevice()->ReserveClockThrottle(&m_pClockThrottle));

    // Update clock-domains mask.
    const UINT32 oldDomainMask = m_DomainMask;
    if (m_DomainMask)
        m_pClockThrottle->SetDomainMask(m_DomainMask);
    m_DomainMask = m_pClockThrottle->GetDomainMask();

    // If user requested specific domains (rather than leaving it at default),
    // report which supported domains will actually be "throttled".
    if (oldDomainMask)
    {
        MASSERT(Gpu::ClkDomain_NUM <= 32);
        for (UINT32 i = 0; i < Gpu::ClkDomain_NUM; i++)
        {
            if (0 == (m_DomainMask & (1<<i)))
                continue;
            Printf(Tee::PriNormal, "Will pulse %s\n",
                   PerfUtil::ClkDomainToStr(Gpu::ClkDomain(i)));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC ClockPulseTest::Cleanup()
{
    StickyRC rc;

    if (m_pClockThrottle)
    {
        rc = GetBoundGpuSubdevice()->ReleaseClockThrottle(m_pClockThrottle);
        m_pClockThrottle = 0;
    }
    m_PStateOwner.ReleasePStates();
    rc = GpuTest::Cleanup();

    return rc;
}

//------------------------------------------------------------------------------
RC ClockPulseTest::Run()
{
    RC rc;
    // Disable RM from touching any thermal-slowdown HW, and save registers
    // that we will be hacking.
    CHECK_RC(m_pClockThrottle->Grab());

    UINT32 LoopsPerFrame = m_pTstCfg->RestartSkipCount();
    UINT32 StartLoop = m_pTstCfg->StartLoop();
    UINT32 EndLoop   = StartLoop + (m_pTstCfg->Loops() * GetNumFreq());
    UINT32 Seed      = m_pTstCfg->Seed();

    m_pClockThrottle->SetDebug(m_Debug);

    for (m_pFpCtx->LoopNum = StartLoop;
         m_KeepRunning || m_pFpCtx->LoopNum < EndLoop;
         ++m_pFpCtx->LoopNum)
    {
        // Handling restart properly is kind of overkill for this test, but if
        // I skip it, someone's "list" style fancypicker won't work right...
        if ((m_pFpCtx->LoopNum == StartLoop) ||
            ((m_pFpCtx->LoopNum % LoopsPerFrame) == 0))
        {
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / LoopsPerFrame;
            m_pFpCtx->Rand.SeedRandom(Seed + m_pFpCtx->LoopNum);
            m_pPickers->Restart();
        }

        // Pick all options for this loop.
        ClockThrottle::Params requested;
        ClockThrottle::Params actual;
        UINT32           sleepMs;
        Tee::Priority    pri;
        sleepMs               = (*m_pPickers)[FPK_CPULSE_DWELL_MSEC].Pick();
        requested.PulseHz     = (*m_pPickers)[FPK_CPULSE_FREQ_HZ].Pick();
        requested.DutyPct     = static_cast<FLOAT32>((*m_pPickers)[FPK_CPULSE_DUTY_PCT].Pick());
        requested.SlowFrac    = (*m_pPickers)[FPK_CPULSE_SLOW_RATIO].FPick();
        requested.GradStepDur = (*m_pPickers)[FPK_CPULSE_GRAD_STEP_DUR].Pick();
        if (0 !=                (*m_pPickers)[FPK_CPULSE_VERBOSE].Pick())
            pri = Tee::PriNormal;
        else
            pri = Tee::PriSecret;

        if (requested.DutyPct < 1 || requested.DutyPct > 100)
        {
            Printf(Tee::PriNormal, "dutyPct %f invalid, overriding.\n",
                   requested.DutyPct);
            requested.DutyPct = 50;
        }
        if (requested.SlowFrac > 1.0 || requested.SlowFrac < 0.01)
        {
            Printf(Tee::PriNormal, "slowFrac %f invalid, overriding.\n",
                   requested.SlowFrac);
            requested.SlowFrac = 0.25F;
        }

        // Tell the user what we picked.
        Printf(pri, "(Loop %d) requesting: ", m_pFpCtx->LoopNum);
        ClockThrottle::PrintParams(pri, requested);
        Printf(pri, "\n");

        // Send options to HW.
        CHECK_RC(m_pClockThrottle->Set(&requested, &actual));

        // Tell the user what we got.
        Printf(pri, "(Loop %d)     actual: ", m_pFpCtx->LoopNum);
        ClockThrottle::PrintParams(pri, actual);
        Printf(pri, "\n");

        if (!m_KeepRunning)
        {
            //not the bgtest -- freq shmoo mode
            // duration is controlled by Loops*NumFreq*DrawRepeats
            CallbackArguments args;
            CHECK_RC(FireCallbacks(ModsTest::MISC_A, Callbacks::STOP_ON_ERROR,
                                   args, "LaunchPayload"));
        }
        else //the bgtest
        {
            //Duration is controlled by FgTest setting !KeepRunning
            Printf(pri, "(Loop %d) sleep %d ms\n", m_pFpCtx->LoopNum, sleepMs);
            // Sleep.  Presumably another thread is exercising graphics.
            Tasker::Sleep(sleepMs);
        }
    }

    // Restore registers and restore RM control of thermal slowdown.
    CHECK_RC(m_pClockThrottle->Release());

    return rc;
}

JS_CLASS_INHERIT(ClockPulseTest, GpuTest, "Toggle gpu clocks for power stress.");

CLASS_PROP_READWRITE(ClockPulseTest, KeepRunning, bool,
                     "If true, keep running (for use as bg test).");

CLASS_PROP_READWRITE(ClockPulseTest, Debug, bool,
                     "If true, dump Debug Info.");

CLASS_PROP_READONLY  (ClockPulseTest, LwrrFreqLabel,  string, "");
CLASS_PROP_READONLY  (ClockPulseTest, LwrrFreqIdx,  UINT32, "");
CLASS_PROP_READONLY  (ClockPulseTest, LwrrFreqInt,  UINT32, "");
CLASS_PROP_READONLY  (ClockPulseTest, NumFreq,   UINT32, "");

CLASS_PROP_READWRITE(ClockPulseTest, DomainMask, UINT32,
        "Mask of clock domains to pulse (defaults to shader domain only).");

PROP_CONST(ClockPulseTest, PRE_FILL,      ModsTest::MISC_A);
