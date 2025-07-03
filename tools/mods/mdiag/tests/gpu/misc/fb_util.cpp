/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.

#include "mdiag/tests/stdtest.h"
#include "core/include/gpu.h"
#include "fb_util.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"

static int PWM_PERIOD           = 16384;
static float HIGH_UTIL_PCT      = 0.9;
static float MEDIUM_UTIL_PCT    = 0.5;
static float LOW_UTIL_PCT       = 0.1;

// define which tests are run
static bool fb_util_high    = false;
static bool fb_util_medium  = false;
static bool fb_util_low     = false;
extern const ParamDecl fb_util_params[] =
{
    SIMPLE_PARAM("-fb_util_high",   "FB high utilization (90%) test"),
    SIMPLE_PARAM("-fb_util_medium", "FB medium utilization (50%) test"),
    SIMPLE_PARAM("-fb_util_low",    "FB low utilization (10%) test"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

FB_UTIL_Test::FB_UTIL_Test(ArgReader *params):
    LWGpuSingleChannelTest(params)
{
    if(params->ParamPresent("-fb_util_high"))   fb_util_high = true;
    else fb_util_high = false;

    if(params->ParamPresent("-fb_util_medium")) fb_util_medium = true;
    else fb_util_medium = false;

    if(params->ParamPresent("-fb_util_low"))    fb_util_low = true;
    else fb_util_low = false;
}
FB_UTIL_Test::~FB_UTIL_Test(void)
{
}

STD_TEST_FACTORY(fb_util, FB_UTIL_Test)

int FB_UTIL_Test::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    m_arch = lwgpu->GetArchitecture();
    macros.MacroInit(m_arch);

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("1 - FB_UTIL_Test::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("FB_UTIL");
    getStateReport()->enable();
    return 1;
}
void FB_UTIL_Test::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("FB_UTIL_Test::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void FB_UTIL_Test::Run(void)
{
    InfoPrintf("FB Utilization Test starts!\n");
    int errCnt=0;

    SetStatus(TEST_INCOMPLETE);
    if (fb_util_high)   errCnt += fb_util_test(HIGH_UTIL_PCT);
    if (fb_util_medium) errCnt += fb_util_test(MEDIUM_UTIL_PCT);
    if (fb_util_low)    errCnt += fb_util_test(LOW_UTIL_PCT);

    if(errCnt)
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("FB_UTIL test Failed\n");
        InfoPrintf("FB_UTIL_Test:: Test Failed with %d errors\n", errCnt);
    }
    else
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        InfoPrintf("FB_UTIL_Test:: Test Passed\n");
    }
    InfoPrintf("FB Utilization Test END.\n");
}

int FB_UTIL_Test::fb_util_test(float util_expected)// returns error count
{
    int cnt_0, cnt_1, fb_busy_cycles, pwm_hi, error;
    error = 0;

    pwm_hi = PWM_PERIOD * util_expected;

    InfoPrintf("Config PWM alwtation logic in full-chip test-bench\n");
    Platform::EscapeWrite("fb_util_pwm_period", 0, 32, PWM_PERIOD);
    Platform::EscapeWrite("fb_util_pwm_hi", 0, 32, pwm_hi);
    
    Platform::Delay(5);

    InfoPrintf("Config idle counter 0 to always counting mode\n");
    lwgpu->RegWr32(0x0010be40, 0);      // LW_PPWR_PMU_IDLE_MASK(0)
    lwgpu->RegWr32(0x0010be80, 0);      // LW_PPWR_PMU_IDLE_MASK_1(0)
    lwgpu->RegWr32(0x0010bec0, 0);      // LW_PPWR_PMU_IDLE_MASK_2(0)
    lwgpu->RegWr32(0x0010bfc0, 0x3);    // LW_PPWR_PMU_IDLE_CTRL(0) field _VALUE[1:0] set to _ALWAYS(0x3)

    InfoPrintf("Config idle counter 1 to idle counting mode\n");
    lwgpu->RegWr32(0x0010be44, 0x80);   // LW_PPWR_PMU_IDLE_MASK(1) _FB_NOREQ[7:7]
    lwgpu->RegWr32(0x0010be84, 0);      // LW_PPWR_PMU_IDLE_MASK_1(1)
    lwgpu->RegWr32(0x0010bec4, 0);      // LW_PPWR_PMU_IDLE_MASK_2(1)
    lwgpu->RegWr32(0x0010bfc4, 0x1);    // LW_PPWR_PMU_IDLE_CTRL(1) field _VALUE[1:0] set to _IDLE(0x1)

    Platform::Delay(500); // let it run for 500 us

    InfoPrintf("Reading PMU idle counter results\n");
    cnt_0 = lwgpu->RegRd32(0x0010bf80); // LW_PPWR_PMU_IDLE_COUNT(0)
    cnt_1 = lwgpu->RegRd32(0x0010bf84); // LW_PPWR_PMU_IDLE_COUNT(1)
    fb_busy_cycles = cnt_0 - cnt_1;

    // Self check test
    InfoPrintf ("%s, Expected FB utilization is %f. Read back PMU count total elapsed clock cycles=%d, FB busy clock cycles=%d\n", __FUNCTION__, util_expected, cnt_0, fb_busy_cycles);
    if(fb_busy_cycles > cnt_0*util_expected*1.2 || fb_busy_cycles < cnt_0*util_expected*0.8) {
        error++;
        InfoPrintf("Error detected!\n");
    }

    // Disable PMU idle counter
    lwgpu->RegWr32(0x0010bfc0, 0x0);    // LW_PPWR_PMU_IDLE_CTRL(0) field _VALUE[1:0] set to _NEVER
    lwgpu->RegWr32(0x0010bfc4, 0x0);    // LW_PPWR_PMU_IDLE_CTRL(1) field _VALUE[1:0] set to _NEVER
    // Reset PMU idle counter
    lwgpu->RegWr32(0x0010bf80, 0x80000000); // LW_PPWR_PMU_IDLE_COUNT(0) field _RESET[31:31] set to _TRIGGER
    lwgpu->RegWr32(0x0010bf84, 0x80000000); // LW_PPWR_PMU_IDLE_COUNT(1) field _RESET[31:31] set to _TRIGGER

    return error;
}
