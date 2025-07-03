/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtestbagv10x.c
 * @brief   PMU HAL functions related to therm BA (Block Activity) tests for GV10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_chiplet_pwr.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"

#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// BA Defines
#define THERM_TEST_BA_EDPC_STEP_SIZE        (0x2)
#define THERM_TEST_BA_EDPC_WINDOW_SIZE      (0x2)
#define THERM_TEST_BA_HIGH_THRESHOLD        (0x5000)
#define THERM_TEST_BA_LOW_THRESHOLD         (0x3000)
#define THERM_TEST_BA_VARIATE_THRESHOLD     (0x1050)
#define THERM_TEST_DELAY_US                 (100)
#define THERM_TEST_WINDOW                   (0)

/* ------------------------- Types Definitions ----------------------------- */

// Cache registers used for BA slowdown test
typedef struct
{
    LwU32  config1;
    LwU32  peakPwrConfig1;
    LwU32  baW0t1;
    LwU32  eventIntrEnable;
    LwU32  useA;
    LwU32  useB;
    LwU32  config2;
    LwU32  config4;
    LwU32  factorA;
    LwU32  leakageC;
    LwU32  eventSelect;
} THERM_TEST_BA_SLWDN_TEMP_CACHE_REGISTERS;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static LwBool
s_thermTestIsBAEventTriggered(void)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_LATCH);
    if (FLD_TEST_DRF(_CPWR_THERM, _EVENT_LATCH, _BA_W0_T1, _TRIGGERED, reg32))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

static LwBool
s_thermTestIsSlowdownOclwrred(void)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));
    if (FLD_TEST_DRF(_CPWR_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DIV2, reg32))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Tests BA slowdown
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
thermTestBaSlowdown_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32                                      reg32;
    LwU8                                       idx;
    THERM_TEST_BA_SLWDN_TEMP_CACHE_REGISTERS   orig;
    FLCN_STATUS                                status;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    orig.config1         = REG_RD32(CSB, LW_CPWR_THERM_CONFIG1);
    orig.peakPwrConfig1  = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW));
    orig.baW0t1          = REG_RD32(CSB, LW_CPWR_THERM_EVT_BA_W0_T1);
    orig.eventIntrEnable = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    orig.useA            = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    orig.useB            = REG_RD32(CSB, LW_CPWR_THERM_USE_B);
    orig.config2         = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(THERM_TEST_WINDOW));
    orig.config4         = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(THERM_TEST_WINDOW));
    orig.factorA         = thermTestGetLwvddFactorA_HAL(&Therm, THERM_TEST_WINDOW);
    orig.leakageC        = thermTestGetLwvddLeakageC_HAL(&Therm, THERM_TEST_WINDOW);
    orig.eventSelect     = REG_RD32(CSB, LW_CPWR_THERM_EVENT_SELECT);

    //
    // Setup init HW state for test
    // Disable all interrupts. Disable slowdowns except for BA_W0_T1
    //
    reg32 = orig.eventIntrEnable;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_BA)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ENABLE ,
                _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, reg32);

    // enable clock slowdown by setting BA_W0_T1 in useA and useB to YES
    reg32 = orig.useA;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_BA)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_A, _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    reg32 = FLD_SET_DRF(_CPWR_THERM, _USE_A, _BA_W0_T1, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, reg32);

    reg32 = orig.useB;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_BA)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_B, _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    reg32 = FLD_SET_DRF(_CPWR_THERM, _USE_B, _BA_W0_T1, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, reg32);

    // Enable BA collection logic
    reg32 = orig.config1;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1, reg32);

    // Configure therm event for BA
    reg32 = orig.baW0t1;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_BA_W0_T1, _SLOW_FACTOR,
                        LW_CPWR_THERM_DIV_BY2, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_BA_W0_T1, _MODE, _NORMAL, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_BA_W0_T1, reg32);

    // configure peakpower config1
    reg32 = DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_STEP_SIZE, THERM_TEST_BA_EDPC_STEP_SIZE)     |
            DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_WINDOW_SIZE, THERM_TEST_BA_EDPC_WINDOW_SIZE) |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_PEAK_EN, _DISABLED)                          |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_SRC_LWVDD, _ENABLED)                         |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_RST, _TRIGGER)                           |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_EN, _ENABLED);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW), reg32);

    // Configure high threshold
    reg32 = orig.config2;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG2,
        _BA_THRESHOLD_1H_VAL, THERM_TEST_BA_HIGH_THRESHOLD, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG2,
        _BA_THRESHOLD_1H_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(THERM_TEST_WINDOW), reg32);

    // Configure low threshold
    reg32  = orig.config4;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG4,
        _BA_THRESHOLD_1L_VAL, THERM_TEST_BA_LOW_THRESHOLD, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG4,
        _BA_THRESHOLD_1L_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(THERM_TEST_WINDOW), reg32);

    appTaskCriticalEnter();
    {
        // Configure factor A as 0
        status = thermTestSetLwvddFactorA_HAL(&Therm,
                                              THERM_TEST_WINDOW,
                                              0);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                             FACTOR_A_SET_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // set EVENT_SELECT to NORMAL (i.e. level sensitive of the BA slowdown event)
        reg32 = orig.eventSelect;
        reg32 = FLD_SET_DRF(_CPWR_THERM, _EVENT_SELECT, _BA_W0_T1, _NORMAL, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_EVENT_SELECT, reg32);

        // Set C to something below the high threshold
        status = thermTestSetLwvddLeakageC_HAL(&Therm,
                                               THERM_TEST_WINDOW,
                                               THERM_TEST_BA_HIGH_THRESHOLD - 1);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                               LEAKAGE_C_SET_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // Check if previous events have been cleared
        if (s_thermTestIsBAEventTriggered())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                               PREV_EVT_NOT_CLEARED_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // Configure factor C as > high threshold to trigger slowdown
        status = thermTestSetLwvddLeakageC_HAL(&Therm,
                                               THERM_TEST_WINDOW,
                                               THERM_TEST_BA_HIGH_THRESHOLD + THERM_TEST_BA_VARIATE_THRESHOLD);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                               LEAKAGE_C_SET_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // Check if BA event has been latched
        if (!s_thermTestIsBAEventTriggered())
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                               EVT_TRIGGER_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // Check that clock slowdown oclwrred
        if (!s_thermTestIsSlowdownOclwrred())
        {
            pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                    HIGH_THRESHOLD_SLOWDOWN_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        //
        // Configure factor C as : high threshold > C > low threshold
        // and ensure slowdown is retained.
        //
        status = thermTestSetLwvddLeakageC_HAL(&Therm,
                                               THERM_TEST_WINDOW,
                                               THERM_TEST_BA_HIGH_THRESHOLD - THERM_TEST_BA_VARIATE_THRESHOLD);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                               LEAKAGE_C_SET_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // Check CLK_STATUS register to ensure slowdown is still retained
        if (!s_thermTestIsSlowdownOclwrred())
        {
            pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, WINDOW_SLOWDOWN_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        // Configure factor C as < low threshold to restore slowdown status
        status = thermTestSetLwvddLeakageC_HAL(&Therm,
                                               THERM_TEST_WINDOW,
                                               THERM_TEST_BA_LOW_THRESHOLD - THERM_TEST_BA_VARIATE_THRESHOLD);
        if (status != FLCN_OK)
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                                                               LEAKAGE_C_SET_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        OS_PTIMER_SPIN_WAIT_US(THERM_TEST_DELAY_US);

        //Check if slowdown goes away if event is restored
        if (s_thermTestIsSlowdownOclwrred())
        {
            pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                    SLOWDOWN_RESTORE_FAILURE);
            goto thermTestBaSlowdown_GV10X_exit;
        }

        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
        pParams->outData   =
            LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SUCCESS);

thermTestBaSlowdown_GV10X_exit:
        lwosNOP();
    }
    appTaskCriticalExit();

    PMU_HALT_COND(thermTestSetLwvddFactorA_HAL(&Therm, THERM_TEST_WINDOW, orig.factorA) == FLCN_OK);
    PMU_HALT_COND(thermTestSetLwvddLeakageC_HAL(&Therm, THERM_TEST_WINDOW, orig.leakageC) == FLCN_OK);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(THERM_TEST_WINDOW), orig.config4);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(THERM_TEST_WINDOW), orig.config2);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, orig.useA);
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, orig.useB);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, orig.eventIntrEnable);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_BA_W0_T1, orig.baW0t1);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(THERM_TEST_WINDOW), orig.peakPwrConfig1);
    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1, orig.config1);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_SELECT, orig.eventSelect);

    return FLCN_OK;
}

