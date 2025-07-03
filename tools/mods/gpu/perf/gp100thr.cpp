/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/

/*
 For reference //hw/doc/gpu/pascal/gp100/gen_manuals/dev_therm.ref
 */
#include "gpu/perf/clockthrottle.h"
#include "pascal/gp100/dev_therm.h"
#include <math.h>

//------------------------------------------------------------------------------
class GP100ClockThrottle : public ClockThrottle
{
public:
    GP100ClockThrottle(GpuSubdevice * pGpuSub) : ClockThrottle(pGpuSub)
    {
        memset(&m_LwrActual, 0, sizeof(m_LwrActual));
    }
    ~GP100ClockThrottle() override
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
RC GP100ClockThrottle::Grab()
{
    SaveReg(LW_THERM_SLOWDOWN_PWM_0);
    SaveReg(LW_THERM_SLOWDOWN_PWM_1);
    SaveReg(LW_THERM_CONFIG2);

    for (INT32 c = 0; c < LW_THERM_CLK_TIMING_0__SIZE_1 ; c++)
    {
        SaveReg(LW_THERM_CLK_TIMING_0(c));
    }

    for (INT32 c = 0; c < LW_THERM_CLK_SLOWDOWN_1__SIZE_1; c++)
    {
        SaveReg(LW_THERM_CLK_SLOWDOWN_1(c));
    }

    //Pstates 3.0 says NOPE NOPE NOPE
    //RC rc = ClockThrottle::Grab();
    return OK;
}

//------------------------------------------------------------------------------
RC GP100ClockThrottle::Set (const Params * pRequested, Params * pActual)
{
    MASSERT(pRequested);
    MASSERT(pActual);
    MASSERT(pRequested->DutyPct <= 100 && pRequested->DutyPct >= 0);
    MASSERT(pRequested->SlowFrac > 0.0 && pRequested->SlowFrac <= 1.0);
    MASSERT(pRequested->PulseHz);

    PrintParams(Tee::PriLow, *pRequested);

    // handle PulseHz and DutyPct
    FLOAT32 utilsClkHz = 108000000.0f;
    FLOAT32 requestedHz = static_cast<FLOAT32>(pRequested->PulseHz);

    FLOAT32 totalPulses = utilsClkHz / requestedHz;
    FLOAT32 highPulses  = totalPulses * pRequested->DutyPct / 100.0f ;

    UINT32 totalPulsesUint = UINT32(totalPulses);
    UINT32 highPulsesUint  = UINT32(highPulses);

    MASSERT(totalPulsesUint <= LW_THERM_SLOWDOWN_PWM_0_PERIOD_MAX );
    MASSERT(highPulsesUint  <= LW_THERM_SLOWDOWN_PWM_1_HI_MAX );
    pActual->PulseHz = static_cast<UINT32>(utilsClkHz / FLOAT32(totalPulsesUint));
    pActual->DutyPct = FLOAT32(highPulsesUint) / FLOAT32(totalPulsesUint);

    // handle SlowFrac
    // extended mode table used
    // [0:1/1, 1:1/2, 2:1/3, 3:1/4, 4:1/5, 5:1/6,
    //  6:1/7, 7:1/8, 8:1/9      ...       31:1/32 ]
    FLOAT32 minDelta=1.0f;
    UINT32 MAX_THERM_FACTOR_EXT_MODE = 32;
    UINT32 minIdx = MAX_THERM_FACTOR_EXT_MODE;

    for(UINT32 i = 0; i < MAX_THERM_FACTOR_EXT_MODE; i++)
    {
        FLOAT32 slowRatio = 1.0f/FLOAT32(i+1);
        FLOAT32 slowRatioDelta = fabs(
            FLOAT32(pRequested->SlowFrac) - slowRatio
            );
        if(slowRatioDelta < minDelta)
        {
            minIdx = i;
            minDelta = slowRatioDelta;
        }
    }
    MASSERT(minIdx < MAX_THERM_FACTOR_EXT_MODE);
    pActual->SlowFrac = 1.0f/FLOAT32(minIdx+1);

    //handle GradStepDur
    // new meanings :
    // 0  : CLIFF
    // 4  : LW_THERM_CONFIG2_GRAD_STEP_DURATION_4
    // 8  : LW_THERM_CONFIG2_GRAD_STEP_DURATION_8
    // 16 : LW_THERM_CONFIG2_GRAD_STEP_DURATION_16
    // etc...
    // 512 :  LW_THERM_CONFIG2_GRAD_STEP_DURATION_512
    const bool useCliff  = (pRequested->GradStepDur == 0);
    UINT32 gradStepValue = 0;
    pActual->GradStepDur = 0;
    if (!useCliff)
    {
        static map<UINT32, UINT32> gradStepMap;
        if (!gradStepMap.size())
        {
            gradStepMap[ 4 ] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_4;
            gradStepMap[ 8 ] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_8;
            gradStepMap[ 16] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_16;
            gradStepMap[ 32] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_32;
            gradStepMap[ 64] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_64;
            gradStepMap[128] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_128;
            gradStepMap[256] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_256;
            gradStepMap[512] =  LW_THERM_CONFIG2_GRAD_STEP_DURATION_512;
        }

        // This logic mimics that of pre-pascal
        for (auto const & lwrStep : gradStepMap)
        {
            // If this HW setting is within 50% of requested, stop searching
            // This is the same algorithm used pre-pascal
            if (pRequested->GradStepDur < (lwrStep.first + lwrStep.first / 2))
            {
                pActual->GradStepDur = lwrStep.first;
                gradStepValue = lwrStep.second;
                break;
            }
        }
    }
    MASSERT(useCliff || pActual->GradStepDur);

    //ok everything is set up; change regs
    UINT32 new_LW_THERM_SLOWDOWN_PWM_0 = 0;

    new_LW_THERM_SLOWDOWN_PWM_0 =
        FLD_SET_DRF_NUM(_THERM,
                        _SLOWDOWN_PWM_0,
                        _PERIOD,
                        totalPulsesUint,
                        new_LW_THERM_SLOWDOWN_PWM_0 );
    new_LW_THERM_SLOWDOWN_PWM_0 =
        FLD_SET_DRF    (_THERM,
                        _SLOWDOWN_PWM_0,
                        _EN,
                        _ENABLE,
                        new_LW_THERM_SLOWDOWN_PWM_0 );

    UINT32 new_LW_THERM_SLOWDOWN_PWM_1 = 0;
    new_LW_THERM_SLOWDOWN_PWM_1 =
        FLD_SET_DRF_NUM(_THERM,
                        _SLOWDOWN_PWM_1,
                        _HI,
                        highPulsesUint,
                        new_LW_THERM_SLOWDOWN_PWM_1 );

    vector<UINT32> new_LW_THERM_CLK_SLOWDOWN_1(
        LW_THERM_CLK_SLOWDOWN_1__SIZE_1,
        0);

    for (UINT32 c = 0; c < LW_THERM_CLK_SLOWDOWN_1__SIZE_1; c++)
    {
        new_LW_THERM_CLK_SLOWDOWN_1[c] =
            FLD_SET_DRF_NUM(_THERM,
                            _CLK_SLOWDOWN_1,
                            _SW_THERM_FACTOR,
                            (minIdx << 1),
                            new_LW_THERM_CLK_SLOWDOWN_1[c]);

        new_LW_THERM_CLK_SLOWDOWN_1[c] =
            FLD_SET_DRF    (_THERM,
                            _CLK_SLOWDOWN_1,
                            _SW_THERM_TRIGGER,
                            _UNSET,
                            new_LW_THERM_CLK_SLOWDOWN_1[c]);
    }

    UINT32 new_LW_THERM_CONFIG2 = 0;
    vector<UINT32> new_LW_THERM_CLK_TIMING_0(
        LW_THERM_CLK_TIMING_0__SIZE_1,
        0);

    new_LW_THERM_CONFIG2 =
        FLD_SET_DRF_NUM(_THERM,
                        _CONFIG2,
                        _GRAD_STEP_DURATION,
                        gradStepValue,
                        new_LW_THERM_CONFIG2);

    new_LW_THERM_CONFIG2 =
        FLD_SET_DRF_NUM(_THERM,
                        _CONFIG2,
                        _GRAD_STEP_DURATION_CYCLE,
                        1,
                        new_LW_THERM_CONFIG2);

    new_LW_THERM_CONFIG2 =
        FLD_SET_DRF_NUM(_THERM,
                        _CONFIG2,
                        _SLOWDOWN_FACTOR_EXTENDED,
                        1,
                        new_LW_THERM_CONFIG2);

    if(!useCliff) {
        new_LW_THERM_CONFIG2 =
            FLD_SET_DRF_NUM(_THERM,
                            _CONFIG2,
                            _GRAD_ENABLE,
                            1,
                            new_LW_THERM_CONFIG2);
    }

    for(UINT32 c = 0; c < LW_THERM_CLK_TIMING_0__SIZE_1; c++)
    {
        if(!useCliff) {
            new_LW_THERM_CLK_TIMING_0[c] =
                FLD_SET_DRF(_THERM,
                            _CLK_TIMING_0,
                            _GRAD_SLOWDOWN,
                            _ENABLED,
                            new_LW_THERM_CLK_TIMING_0[c]);
        }
    }

    //finally turn on
    GpuSub()->RegWr32(LW_THERM_SLOWDOWN_PWM_1, new_LW_THERM_SLOWDOWN_PWM_1);
    GpuSub()->RegWr32(LW_THERM_SLOWDOWN_PWM_0, new_LW_THERM_SLOWDOWN_PWM_0);
    GpuSub()->RegWr32(LW_THERM_CONFIG2,        new_LW_THERM_CONFIG2);

    for(UINT32 c = 0; c< LW_THERM_CLK_TIMING_0__SIZE_1; c++)
    {
        GpuSub()->RegWr32(
            LW_THERM_CLK_TIMING_0(c),
            new_LW_THERM_CLK_TIMING_0[c] );
    }
    /* other domains are invalid per colwersation with alex gu */
    for(UINT32 c = 0; c<1/*LW_THERM_CLK_SLOWDOWN_1__SIZE_1*/; c++)
    {
        GpuSub()->RegWr32(
            LW_THERM_CLK_SLOWDOWN_1(c),
            new_LW_THERM_CLK_SLOWDOWN_1[c] );
    }

    Printf(Tee::PriLow, "LW_THERM_SLOWDOWN_PWM_0 : 0x%08x \n",
           GpuSub()->RegRd32(LW_THERM_SLOWDOWN_PWM_0));
    Printf(Tee::PriLow, "LW_THERM_SLOWDOWN_PWM_1 : 0x%08x \n",
           GpuSub()->RegRd32(LW_THERM_SLOWDOWN_PWM_1));
    Printf(Tee::PriLow, "LW_THERM_CONFIG2        : 0x%08x \n",
           GpuSub()->RegRd32(LW_THERM_CONFIG2));
    Printf(Tee::PriLow, "LW_THERM_CLK_TIMING_0   : 0x%08x \n",
           GpuSub()->RegRd32(LW_THERM_CLK_TIMING_0(0)));
    Printf(Tee::PriLow, "LW_THERM_CLK_SLOWDOWN_1 : 0x%08x \n",
           GpuSub()->RegRd32(LW_THERM_CLK_SLOWDOWN_1(0)));

    return OK;
}

//------------------------------------------------------------------------------
RC GP100ClockThrottle::Release()
{
    // Note: base-class will restore all SaveReg registers.
    return ClockThrottle::Release();
}

//------------------------------------------------------------------------------
ClockThrottle * GP100ClockThrottleFactory (GpuSubdevice * pGpuSub)
{
    if ((pGpuSub->DeviceId() >= Gpu::GP100))
    {
        return new GP100ClockThrottle(pGpuSub);
    }
    else
    {
        return nullptr;
    }
}
