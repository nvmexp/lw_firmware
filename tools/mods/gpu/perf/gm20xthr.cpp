/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2016, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/

/*
 For reference //hw/doc/gpu/maxwell/gm204/gen_manuals/dev_therm.ref
 */
#include "gpu/perf/clockthrottle.h"
#include "maxwell/gm204/dev_therm.h"
#include <math.h>

//------------------------------------------------------------------------------
class GM20xClockThrottle : public ClockThrottle
{
public:
    GM20xClockThrottle(GpuSubdevice * pGpuSub) : ClockThrottle(pGpuSub)
    {
        memset(&m_LwrActual, 0, sizeof(m_LwrActual));
    }
    ~GM20xClockThrottle() override
    {}
    RC Grab() override;
    RC Set (const Params * pRequested, Params * pActual) override;
    RC Release() override;

    UINT32 GetDomainMask() const override
    {
        // Hw claims to support sysclk and ltcclk also, try that later.
        return (1 << Gpu::ClkGpc2) | (1 << Gpu::ClkGpc);
    }

    UINT32 GetLwrrFreqInt() override
    {
        return m_LwrActual.PulseHz;
    }
    UINT32 GetNumFreq() override
    {
        // To make the GLHistogram + ClockPulseTest + GLCompbust test work correctly,
        // we must return the same number of frequencies as the ClockPulseTest's
        // FPK_CPULSE_FREQ_HZ FancyPicker is configured for.
        //
        // The correct answer is (by default) 22*16.
        //
        // @todo -- fix the JS code to pull the number of frequencies from
        // directly from the FancyPicker, instead of duplicating magic numbers.
        return 22 * 16;
    }

private:

    Params m_LwrActual;  // for GetLwrFreqInt only
};

//------------------------------------------------------------------------------
RC GM20xClockThrottle::Grab()
{
    SaveReg(LW_THERM_SLOWDOWN_PWM_0);
    SaveReg(LW_THERM_SLOWDOWN_PWM_1);
    SaveReg(LW_THERM_CONFIG2);

    for (int c = 0; c < LW_THERM_CLK_SLOWDOWN_0__SIZE_1; c++)
    {
        SaveReg(LW_THERM_CLK_SLOWDOWN_1(c));
    }

    RC rc = ClockThrottle::Grab();
    return rc;
}

//------------------------------------------------------------------------------
RC GM20xClockThrottle::Set (const Params * pRequested, Params * pActual)
{
    MASSERT(pRequested);
    MASSERT(pActual);
    MASSERT(pRequested->DutyPct <= 100 && pRequested->DutyPct >= 0);
    MASSERT(pRequested->SlowFrac > 0.0 && pRequested->SlowFrac <= 1.0);

    memset(pActual, 0, sizeof(*pActual));

    // @todo Disable HW features that will keep the test from working.
    // Base-class Grab already disabled thermal-slowdown via RM calls.
    // GF100 version didn't really do anything here, maybe we don't need to
    // either?

    // Look at pRequested->GradStepDur to program the "shape" of the pulse.
    // We will only support "gradual" mode here, as we did for GF100.
    // The old square-wave shape is not used in production clock-gating and is
    // too harsh for any board to survive.

    UINT32 gradStep;
    for (gradStep = LW_THERM_CONFIG2_GRAD_STEP_DURATION_4;
         gradStep < LW_THERM_CONFIG2_GRAD_STEP_DURATION_128K;
         gradStep++)
    {
        // HW register value for DURATION_4 is 0, _8 is 1, etc.
        const UINT32 stepDur = 1<<(gradStep+2);

        // If this HW setting is within 50% of requested, stop searching.
        if (pRequested->GradStepDur < (stepDur + stepDur/2))
            break;
    }
    pActual->GradStepDur = 1<<(gradStep+2);

    // Choose SLOWDOWN_PWM regs for pulse frequency and duty-percent.
    // These are 24-bit counters off of 108mhz utilsclk.
    // Min possible duration of a pulse cycle is affected by GradStepDur.
    // (i.e. we need enough time to lower and then restore clocks)

    const double utilSclkHz = 108e6;
    double hz = max<double>(pRequested->PulseHz, 1);
    UINT32 pulseClocks = static_cast<UINT32> (0.5 + utilSclkHz / hz);
    const UINT32 minCount = pActual->GradStepDur * 2;
    const UINT32 maxCount = 0xffffff;
    pulseClocks = max(minCount, pulseClocks);
    pulseClocks = min(maxCount, pulseClocks);
    pActual->PulseHz = static_cast<UINT32>(0.5 + utilSclkHz / pulseClocks);

    UINT32 lowClocks = static_cast<UINT32>
        (0.5 + pulseClocks * (100.0 - pRequested->DutyPct) / 100.0);
    lowClocks = max(pActual->GradStepDur, lowClocks);
    pActual->DutyPct = static_cast<FLOAT32>
        ((pulseClocks - lowClocks) * 100.0 / pulseClocks);

    // Find the slowdown factor that is closest to the requested fraction.
    // Extended mode uses linear 5.1 rather than power-of-two dividers.
    // @todo -- reduce slowdown to account for insufficient slew time at
    // high frequencies, gf100 code does this.

    double smallestDiff = 1.0;
    UINT32 slowFactor = 0;
    for (UINT32 ii = 0; ii < 64; ii++)
    {
        double frac = 1.0 / (1.0 + 0.5*ii);
        double diff = fabs(pRequested->SlowFrac - frac);
        if (diff < smallestDiff)
        {
            smallestDiff = diff;
            slowFactor = ii;
            pActual->SlowFrac = FLOAT32(frac);
        }
    }

    // Time to bang some registers.

    UINT32 r;
    r = GpuSub()->RegRd32(LW_THERM_SLOWDOWN_PWM_0);
    r = FLD_SET_DRF_NUM(_THERM, _SLOWDOWN_PWM_0, _PERIOD, pulseClocks, r);
    r = FLD_SET_DRF(_THERM, _SLOWDOWN_PWM_0, _EN, _ENABLE, r);
    GpuSub()->RegWr32(LW_THERM_SLOWDOWN_PWM_0, r);

    // Note: _HI counts time where the slowdown signal is asserted high.
    // I.e. it controls the time when GpcClk is low.  Hardware guys...
    r = GpuSub()->RegRd32(LW_THERM_SLOWDOWN_PWM_1);
    r = FLD_SET_DRF_NUM(_THERM, _SLOWDOWN_PWM_1, _HI, lowClocks, r);
    GpuSub()->RegWr32(LW_THERM_SLOWDOWN_PWM_1, r);

    r = GpuSub()->RegRd32(LW_THERM_CONFIG2);
    r = FLD_SET_DRF_NUM(_THERM, _CONFIG2, _GRAD_STEP_DURATION, gradStep, r);
    // @todo GRAD_STEP_DURATION_CYCLE, should we change it?
    // @todo verify that SLOWDOWN_FACTOR_EXTENDED is set (should be by RM)
    // @todo verify that GRAD_ENABLE is set (should be by RM)
    GpuSub()->RegWr32(LW_THERM_CONFIG2, r);

    for (int c = 0; c < LW_THERM_CLK_SLOWDOWN_0__SIZE_1; c++)
    {
        r = GpuSub()->RegRd32(LW_THERM_CLK_SLOWDOWN_1(c));
        r = FLD_SET_DRF_NUM(_THERM, _CLK_SLOWDOWN_1, _SW_THERM_FACTOR, slowFactor, r);
        // Disable pulse-eater mode.
        r = FLD_SET_DRF(_THERM, _CLK_SLOWDOWN_1, _SW_THERM_TRIGGER, _UNSET, r);
        GpuSub()->RegWr32(LW_THERM_CLK_SLOWDOWN_1(c), r);
    }

    m_LwrActual = *pActual;
    return OK;
}

//------------------------------------------------------------------------------
RC GM20xClockThrottle::Release()
{
    // Note: base-class will restore all SaveReg registers.
    return ClockThrottle::Release();
}

//------------------------------------------------------------------------------
ClockThrottle * GM20xClockThrottleFactory (GpuSubdevice * pGpuSub)
{
    if ((pGpuSub->DeviceId() >= Gpu::GM200))
    {
        return new GM20xClockThrottle(pGpuSub);
    }
    else
    {
        return 0;
    }
}
