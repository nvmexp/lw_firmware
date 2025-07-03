/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtestbatu10x.c
 * @brief   PMU HAL functions related to therm BA (Block Activity) tests for TU10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_chiplet_pwr.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"

#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// BA Defines
#define THERM_TEST_BA_EDPP_SIGNIFICAND      (0x2)
#define THERM_TEST_BA_HIGH_THRESHOLD        (0xe000)
#define THERM_TEST_BA_LOW_THRESHOLD         (0x3000)
#define THERM_TEST_DELAY_US                 (10)
#define THERM_TEST_WINDOW                   (0)

/* ------------------------- Types Definitions ----------------------------- */

typedef struct
{
    LwU32  gpcConfig1;
    LwU32  fbpConfig1;
    LwU32  thermConfig1;
    LwU32  thermUseA;
    LwU32  thermUseB;
    LwU32  thermEvtBaW0T1;
    LwU32  thermEventIntrEnable;
    LwU32  eventSelect;
    LwU32  thermPeakPwrConfig1;
    LwU32  thermPeakPwrConfig2;
    LwU32  thermPeakPwrConfig3;
    LwU32  thermPeakPwrConfig4;
    LwU32  thermPeakPwrConfig5;
    /* there is no CONFIG6 and CONFIG7 */
    LwU32  thermPeakPwrConfig8;
    LwU32  thermPeakPwrConfig9;
    /* CONFIG10 is for reading HW state */
    LwU32  thermPeakPwrConfig11;
    LwU32  thermPeakPwrConfig12;
    /* CONFIG13 is for reading HW state */
} THERM_TEST_BA_EDPP_SLWDN_CACHE_REGISTERS;

typedef struct
{
    LwU32  peakPwrConfig1;
    LwU32  voltageHwAdc;
    LwU32  eventSelect;
    LwU32  useA;
    LwU32  useB;
    LwU32  evtVoltageHwAdc;
} THERM_TEST_HW_ADC_SLWDN_CACHE_REGISTERS;

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Helper function that determines if clock slowdown is happening right now
 *
 * @return LW_TRUE     Clock slowdown oclwred
 *         LW_FALSE    Clock slowdown did not occur
 *
 */
static LwBool
s_thermTestIsSlowdownHappening(void)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));
    if (FLD_TEST_DRF(_CPWR_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DIV2, reg32))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/*!
 * @brief Helper function to check if the peak power event has been triggered
 *
 * @return LW_TRUE    Peak power event has been triggered
 *         LW_FALSE   Peak power event has not been triggered
 *
 * @note event trigger is different from event latch. event trigger is the signal
 *       that tracks whether the event is happening or not. event trigger can be made
 *       to follow whether the peak power satisfies the thresholds in real-time or
 *       can follow the latched value of if one of the thresholds is satisfied. This
 *       latched value is called the event latch.
 *
 */
static LwBool
s_thermTestIsPeakPwrEventTriggered(void)
{
    LwU32 reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);

    if (FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _BA_W0_T1, _TRIGGERED, reg32))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Tests EDPp BA slowdown
 *
 * @param[out]  pParams   Test Params, inidicating status of test
 *                         and output value\
 *
 * @return      FLCN_OK               If test is supported on the system
 *              FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU
 *
 * @note        return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus
 *
 */
FLCN_STATUS
thermTestPeakSlowdown_TU10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32                                      reg32;
    THERM_TEST_BA_EDPP_SLWDN_CACHE_REGISTERS   orig;
    FLCN_STATUS                                status = FLCN_OK;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    orig.gpcConfig1           = REG_RD32(BAR0, LW_PCHIPLET_PWR_GPCS_CONFIG_1);
    orig.fbpConfig1           = REG_RD32(BAR0, LW_PCHIPLET_PWR_FBPS_CONFIG_1);
    orig.thermUseA            = REG_RD32(CSB,  LW_CPWR_THERM_USE_A);
    orig.thermUseB            = REG_RD32(CSB,  LW_CPWR_THERM_USE_B);
    orig.thermConfig1         = REG_RD32(CSB,  LW_CPWR_THERM_CONFIG1);
    orig.thermEvtBaW0T1       = REG_RD32(CSB,  LW_CPWR_THERM_EVT_BA_W0_T1);
    orig.thermEventIntrEnable = REG_RD32(CSB,  LW_CPWR_THERM_EVENT_INTR_ENABLE);
    orig.eventSelect          = REG_RD32(CSB,  LW_CPWR_THERM_EVENT_SELECT);
    orig.thermPeakPwrConfig1  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig2  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG2(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig3  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG3(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig4  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG4(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig5  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG5(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig8  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG8(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig9  = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG9(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig11 = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG11(THERM_TEST_WINDOW));
    orig.thermPeakPwrConfig12 = REG_RD32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG12(THERM_TEST_WINDOW));

    // Enable BA collection logic
    reg32 = orig.thermConfig1;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1, reg32);

    reg32 = orig.gpcConfig1;
    reg32 = FLD_SET_DRF(_PCHIPLET_PWR_GPCS, _CONFIG_1, _BA_ENABLE, _YES, reg32);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPCS_CONFIG_1, reg32);

    reg32 = orig.fbpConfig1;
    reg32 = FLD_SET_DRF(_PCHIPLET_PWR_FBPS, _CONFIG_1, _BA_ENABLE, _YES, reg32);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_FBPS_CONFIG_1, reg32);

    // enable clock slowdown by setting BA_W0_T1 in useA and useB to YES
    reg32 = DRF_DEF(_CPWR_THERM, _USE_A, _BA_W0_T1, _YES);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, reg32);

    reg32 = DRF_DEF(_CPWR_THERM, _USE_B, _BA_W0_T1, _YES);
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, reg32);

    //
    // Configure therm event for BA. We set the clock slowdown event to follow
    // the latched value of the peak detection event because setting clock
    // slowdown to be level-sensitive of the peak detection event would happen
    // to quickly for us to observe
    //
    reg32 = orig.thermEvtBaW0T1;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_BA_W0_T1, _SLOW_FACTOR,
                        LW_CPWR_THERM_DIV_BY2, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_BA_W0_T1, _MODE, _NORMAL, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_BA_W0_T1, reg32);

    //
    // Setup init HW state for test
    // Disable all slowdowns except those triggered by BA_W0_T1
    //
    reg32 = DRF_DEF(_CPWR_THERM, _EVENT_INTR_ENABLE, _BA_W0_T1, _YES);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, reg32);

    //
    // Set the clock slowdown event to follow the latched value of the peak detection
    // event so that SW can observe clock slowdown. Setting slowdown to be level-sensitive
    // of the peak detection event would happen to quickly for us to observe
    //
    reg32 = orig.eventSelect;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVENT_SELECT, _BA_W0_T1, _LATCH, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_SELECT, reg32);

    // configure peakpower config1
    reg32 = DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_STEP_SIZE, _INIT)    |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_WINDOW_SIZE, _MIN)   |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_PEAK_EN, _ENABLED)   |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_SRC_LWVDD, _ENABLED) |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_RST, _TRIGGER)   |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_EN, _ENABLED);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW), reg32);

    //
    // configure the high thresholds (config 2 and 3). Set the 1st high threshold to our
    // threshold, and disable the 2nd high threshold.
    //
    reg32 = orig.thermPeakPwrConfig2;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG2,
        _BA_THRESHOLD_1H_VAL, THERM_TEST_BA_HIGH_THRESHOLD, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG2, _BA_THRESHOLD_1H_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(THERM_TEST_WINDOW), reg32);

    reg32 = orig.thermPeakPwrConfig3;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG3, _BA_THRESHOLD_2H_EN, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG3(THERM_TEST_WINDOW), reg32);

    //
    // configure the low thresholds (config 4 and 5). Set the 1st low threshold to our
    // threshold, and disable the 2nd low threshold. Because of Bug 2034250, SW is unable
    // to clear the peak power event if the low threshold is disabled.
    //
    reg32 = orig.thermPeakPwrConfig4;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG4,
            _BA_THRESHOLD_1L_VAL, THERM_TEST_BA_LOW_THRESHOLD, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG4, _BA_THRESHOLD_1L_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(THERM_TEST_WINDOW), reg32);

    reg32 = orig.thermPeakPwrConfig5;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG5, _BA_THRESHOLD_2L_EN, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG5(THERM_TEST_WINDOW), reg32);

    //
    // Configure factor A as 0 so that all BA values are zeroed. We will use factor C to
    // control what the value of ba_sum_shift is
    //
    status = thermTestSetLwvddFactorA_HAL(&Therm, THERM_TEST_WINDOW, 0);
    if (status != FLCN_OK)
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                           FACTOR_A_SET_FAILURE);
        goto thermTestPeakSlowdown_TU10X_exit;
    }

    // init the leakage C to 0
    status = thermTestSetLwvddLeakageC_HAL(&Therm, THERM_TEST_WINDOW, 0);
    if (status != FLCN_OK)
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                           LEAKAGE_C_SET_FAILURE);
        goto thermTestPeakSlowdown_TU10X_exit;
    }

    // configure the peak power predictor
    reg32 = orig.thermPeakPwrConfig11;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG11, _C0_SIGNIFICAND,
                             THERM_TEST_BA_EDPP_SIGNIFICAND, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG11, _C0_RSHIFT,
                        _0, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG11, _C1_SIGNIFICAND,
                        _INIT, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG11(THERM_TEST_WINDOW), reg32);

    reg32 = orig.thermPeakPwrConfig12;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG12, _C2_SIGNIFICAND,
                        _INIT, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG12, _DBA_PERIOD,
                        _MIN, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG12(THERM_TEST_WINDOW), reg32);

    // Clear any BA slowdown event that may have been triggered during setup
    reg32 = DRF_DEF(_CPWR_THERM, _EVENT_LATCH, _BA_W0_T1, _CLEAR);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_LATCH, reg32);

    // check that other events got cleared
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
    if (!FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _BA_W0_T1, _NORMAL, reg32))
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                           PREV_EVT_NOT_CLEARED_FAILURE);
        goto thermTestPeakSlowdown_TU10X_exit;
    }

    //
    // trigger the peak event by suddenly setting C to HIGH_THRESHOLD * FUDGE_FACTOR, where
    // 0 < FUDGE_FACTOR < 1. Therefore, C will always be set to something less than HIGH_THRESHOLD.
    // The event should be triggered because the value that goes to the comparitors is the sum of
    // window period integrator and peak power detector. The sudden change in C should make
    // this sum go above the threshold
    //
    appTaskCriticalEnter();
    {
        status = thermTestSetLwvddLeakageC_HAL(&Therm,
                                               THERM_TEST_WINDOW,
                                               (THERM_TEST_BA_HIGH_THRESHOLD + THERM_TEST_BA_LOW_THRESHOLD) / 2);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                               LEAKAGE_C_SET_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        // Check if the event was latched
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_LATCH);
        if  (!FLD_TEST_DRF(_CPWR_THERM, _EVENT_LATCH, _BA_W0_T1, _TRIGGERED, reg32))
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                               EVT_LATCH_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        // Check if the event has been triggered. Fail if not.
        if (!s_thermTestIsPeakPwrEventTriggered())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                               EVT_TRIGGER_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        // check if clock slowdown is happening
        if (!s_thermTestIsSlowdownHappening())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                               CLK_SLOWDOWN_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        // lower C below the low threshold
        status = thermTestSetLwvddLeakageC_HAL(&Therm,
                                               THERM_TEST_WINDOW,
                                               THERM_TEST_BA_LOW_THRESHOLD / 2);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                                                               LEAKAGE_C_SET_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        //
        // wait for some time for the peak detector to level off. This is so
        // that the peak predicted power should drop below the high threshold over time
        // without setting LEAKAGE_C.
        //
        OS_PTIMER_SPIN_WAIT_US(THERM_TEST_DELAY_US);

        // clear the latched peak event.
        reg32 = DRF_DEF(_CPWR_THERM, _EVENT_LATCH, _BA_W0_T1, _CLEAR);
        REG_WR32(CSB, LW_CPWR_THERM_EVENT_LATCH, reg32);

        // verify that the trigger is cleared
        if (s_thermTestIsPeakPwrEventTriggered())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                               EVT_CLEAR_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        // verify that clk slowdown is cleared
        if (s_thermTestIsSlowdownHappening())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN,
                               CLK_SLOWDOWN_CLEAR_FAILURE);
            goto thermTestPeakSlowdown_TU10X_critical_exit;
        }

        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
        pParams->outData   =
            LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, SUCCESS);

thermTestPeakSlowdown_TU10X_critical_exit:
        lwosNOP();
    }
    appTaskCriticalExit();

thermTestPeakSlowdown_TU10X_exit:
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPCS_CONFIG_1, orig.gpcConfig1);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_FBPS_CONFIG_1, orig.fbpConfig1);
    REG_WR32(CSB,  LW_CPWR_THERM_USE_A, orig.thermUseA);
    REG_WR32(CSB,  LW_CPWR_THERM_USE_B, orig.thermUseB);
    REG_WR32(CSB,  LW_CPWR_THERM_CONFIG1, orig.thermConfig1);
    REG_WR32(CSB,  LW_CPWR_THERM_EVT_BA_W0_T1, orig.thermEvtBaW0T1);
    REG_WR32(CSB,  LW_CPWR_THERM_EVENT_INTR_ENABLE, orig.thermEventIntrEnable);
    REG_WR32(CSB,  LW_CPWR_THERM_EVENT_SELECT, orig.eventSelect);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW), orig.thermPeakPwrConfig1);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG2(THERM_TEST_WINDOW), orig.thermPeakPwrConfig2);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG3(THERM_TEST_WINDOW), orig.thermPeakPwrConfig3);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG4(THERM_TEST_WINDOW), orig.thermPeakPwrConfig4);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG5(THERM_TEST_WINDOW), orig.thermPeakPwrConfig5);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG8(THERM_TEST_WINDOW), orig.thermPeakPwrConfig8);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG9(THERM_TEST_WINDOW), orig.thermPeakPwrConfig9);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG11(THERM_TEST_WINDOW), orig.thermPeakPwrConfig11);
    REG_WR32(CSB,  LW_CPWR_THERM_PEAKPOWER_CONFIG12(THERM_TEST_WINDOW), orig.thermPeakPwrConfig12);

    return FLCN_OK;
}

/*!
 * @brief  Tests HW ADC slowdown
 *
 * @param[out]  pParams   Test Params, inidicating status of test
 *                         and output value\
 *
 * @return      FLCN_OK               If test is supported on the system
 *              FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU
 *
 * @note        return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus
 *
 */
FLCN_STATUS
thermTestHwAdcSlowdown_TU10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    THERM_TEST_HW_ADC_SLWDN_CACHE_REGISTERS   orig;
    LwU32                                     reg32;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // save off of the original settings
    orig.peakPwrConfig1  = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW));
    orig.voltageHwAdc = REG_RD32(CSB, LW_CPWR_THERM_VOLTAGE_HW_ADC);
    orig.eventSelect = REG_RD32(CSB, LW_CPWR_THERM_EVENT_SELECT);
    orig.useA = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    orig.useB = REG_RD32(CSB, LW_CPWR_THERM_USE_B);
    orig.evtVoltageHwAdc = REG_RD32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC);

    // initialize HW_ADC trigger to normal mode and set the slow factor
    reg32 = orig.evtVoltageHwAdc;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_VOLTAGE_HW_ADC, _SLOW_FACTOR,
                        LW_CPWR_THERM_DIV_BY2, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_VOLTAGE_HW_ADC, _MODE, _CLEARED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC, reg32);

    //
    // turn of HW ADC high and low threshold comparison. since this test uses _FORCE
    // they are not necessary.
    //
    reg32 = orig.voltageHwAdc;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _VOLTAGE_HW_ADC, _THRESHOLD_LOW_EN, _DIS, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _VOLTAGE_HW_ADC, _THRESHOLD_HIGH_EN, _DIS, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_VOLTAGE_HW_ADC, reg32);

    // enable HW ADC triggered clock slowdown
    reg32 = DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_T1_HW_ADC_MODE, _ADC);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW), reg32);

    reg32 = orig.useA;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _USE_A, _VOLTAGE_HW_ADC, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, reg32);

    reg32 = orig.useB;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _USE_B, _VOLTAGE_HW_ADC, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, reg32);

    // set HW ADC events to use level sensitive mode
    reg32 = orig.eventSelect;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVENT_SELECT, _VOLTAGE_HW_ADC, _NORMAL, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_SELECT, reg32);

    // check that the HW ADC event trigger hasn't been asserted
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
    if (!FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _VOLTAGE_HW_ADC, _NORMAL, reg32))
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN,
                                                           INTR_NOT_CLEARED_FAILURE);
        goto thermTestHwAdcSlowdown_TU10X_exit;
    }

    // force the HW ADC event to trigger
    appTaskCriticalEnter();
    {

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_VOLTAGE_HW_ADC, _MODE, _FORCED, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC, reg32);

        // check if the slowdown event has been triggered. fail if not.
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if (!FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _VOLTAGE_HW_ADC, _TRIGGERED, reg32))
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN,
                               EVT_TRIGGER_FAILURE);
            goto thermTestHwAdcSlowdown_TU10X_critical_exit;
        }

        // check that clock slowdown happened
        if (!s_thermTestIsSlowdownHappening())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN,
                                                               CLK_SLOWDOWN_FAILURE);
            goto thermTestHwAdcSlowdown_TU10X_critical_exit;
        }

        // stop forcing the HW ADC to event to trigger
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_VOLTAGE_HW_ADC, _MODE, _CLEARED, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC, reg32);

        // check that the HW ADC event goes away
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if (!FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _VOLTAGE_HW_ADC, _NORMAL, reg32))
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN,
                               EVT_CLEAR_FAILURE);
            goto thermTestHwAdcSlowdown_TU10X_critical_exit;
        }

        // check that clock slowdown goes away
        if (s_thermTestIsSlowdownHappening())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN,
                                                               CLK_SLOWDOWN_CLEAR_FAILURE);
            goto thermTestHwAdcSlowdown_TU10X_critical_exit;
        }

        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN, SUCCESS);

thermTestHwAdcSlowdown_TU10X_critical_exit:
        lwosNOP();
    }
    appTaskCriticalExit();

thermTestHwAdcSlowdown_TU10X_exit:
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW), orig.peakPwrConfig1);
    REG_WR32(CSB, LW_CPWR_THERM_VOLTAGE_HW_ADC, orig.voltageHwAdc);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_SELECT, orig.eventSelect);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, orig.useA);
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, orig.useB);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_VOLTAGE_HW_ADC, orig.evtVoltageHwAdc);

    return FLCN_OK;
}
