/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrm_test_gp10x.c
 * @brief   PMU HAL functions related to therm tests for GP10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"

#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

//
// Following temperature values are used in therm tests to force/compare
// different temperatures
//
#define THERM_TEST_VARIATION_TEMP           RM_PMU_CELSIUS_TO_LW_TEMP(5)
#define THERM_TEST_THRESHOLD_TEMP           RM_PMU_CELSIUS_TO_LW_TEMP(60)

// BA thresholds
#define THERM_TEST_BA_HIGH_THRESHOLD        (0x5000)
#define THERM_TEST_BA_LOW_THRESHOLD         (0x3000)
#define THERM_TEST_BA_VARIATE_THRESHOLD     (0x1050)

// To match with HW, SW should ignore lowest three bits of callwlated result
#define THERM_TEST_HW_UNSUPPORTED_BIT_MASK  (0xFFFFFFF8)

/* ------------------------- Type Definitions ------------------------------- */

// Cache registers used for THERM_SLOWDOWN test
typedef struct
{
    LwU32  eventIntrEnable;
    LwU32  useA;
    LwU32  evt;
    LwU32  sensorConst;
    LwU32  power6;
    LwU32  useB;
} THERM_TEST_SLWDN_TEMP_CACHE_REGISTERS;

// Cache registers used for BA slowdown test
typedef struct
{
    LwU32  config1;
    LwU32  peakPwrConfig1;
    LwU32  baW0t1;
    LwU32  eventIntrEnable;
    LwU32  useA;
    LwU32  factorA;
    LwU32  config2;
    LwU32  config4;
    LwU32  factorC;
} THERM_TEST_BA_SLWDN_TEMP_CACHE_REGISTERS;

/* ------------------------- Static Variables ------------------------------- */
static void s_thermTestTsenseExelwte_GP10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams)
     GCC_ATTRIB_SECTION("imem_thermLibTest", "_thermTsenseTest_GP10X");

static void s_thermTestConstMaxExelwte_GP10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "_thermConstMaxTempTest_GP10X");

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Tests internal Sensors.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                         and output value
 *
 * @return      FLCN_OK               If test is supported on the system
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus
 */
FLCN_STATUS
thermTestIntSensors_GP10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
    pParams->outData   =
        LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SUCCESS);

    // Check TSENSE
    s_thermTestTsenseExelwte_GP10X(pParams);
    if (pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
    {
        goto thermTestIntSensors_GP10X_exit;
    }

    // Check TSENSE_OFFSET, SENSOR_MAX
    thermTestOffsetMaxTempExelwte_HAL(&Therm, pParams);
    if (pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
    {
        goto thermTestIntSensors_GP10X_exit;
    }

    // Check CONSTANT, SENSOR_MAX
    s_thermTestConstMaxExelwte_GP10X(pParams);
    if (pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
    {
        goto thermTestIntSensors_GP10X_exit;
    }

thermTestIntSensors_GP10X_exit:
    return FLCN_OK;
}

/*!
 * @brief  Tests thermal slowdown
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
thermTestThermalSlowdown_GP10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32  reg32;
    LwU32  idx;
    THERM_TEST_SLWDN_TEMP_CACHE_REGISTERS
           orig;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Step 1: cache current HW state
    orig.eventIntrEnable  = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    orig.useA             = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    orig.useB             = REG_RD32(CSB, LW_CPWR_THERM_USE_B);
    orig.sensorConst      = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT);
    orig.power6           = REG_RD32(CSB, LW_CPWR_THERM_POWER_6);

    //
    // Step 2: setup init HW state for test
    // Disable all interrupts and slowdowns
    //
    reg32 = orig.eventIntrEnable;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ENABLE ,
                _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, reg32);

    reg32 = orig.useA;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_A, _CTRL, idx, _YES, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, reg32);

    reg32 = orig.useB;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_B, _CTRL, idx, _YES, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, reg32);

    // Configure FILTER_PERIOD
    reg32 = orig.power6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6,_THERMAL_FILTER_PERIOD,
                        _NONE, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_SCALE,
                        _16US, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, reg32);

    pParams->outData =
        LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SUCCESS);

    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        // Skip changing event thresholds for OVERT events
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_EN);
        if (FLD_IDX_TEST_DRF(_CPWR_THERM, _OVERT_EN, _CTRL, idx, _ENABLED, reg32))
        {
            continue;
        }

        reg32 = orig.evt = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(idx));
        //
        // Step 3: Configure each event to react to the following:
        // 1) React to a temperature threshold
        // 2) React to SENSOR_CONSTANT
        // 3) Produce 2X slowdown
        //
        reg32 = orig.evt;

        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_VARIATION_TEMP), reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_SETTINGS, _TEMP_SENSOR_ID,
                            _CONSTANT, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _SLOW_FACTOR,
                                LW_CPWR_THERM_DIV_BY2, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_SETTINGS, _MODE,
                            _NORMAL, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS(idx), reg32);

        // Step 4: Fake temperature to > temp threshold
        reg32 = orig.sensorConst;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _TEMP_SENSOR_CONSTANT,
                _FIXED_POINT, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_THRESHOLD_TEMP), reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _TEMP_SENSOR_CONSTANT, _SOURCE,
                            _SW_OVERRIDE, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT, reg32);

        // Check if the event has been triggered. Fail if not.
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if (FLD_IDX_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _STATE,
            idx, _NORMAL, reg32))
        {
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN,
                               EVT_TRIGGER_FAILURE);
            goto thermTestThermalSlowdown_GP10X_loop_end;
        }

        // Step 5: Check CLK_STATUS register to ensure slowdown has oclwred
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));
        if (!FLD_TEST_DRF(_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DIV2, reg32))
        {
            pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SLOWDOWN_FAILURE);
            goto thermTestThermalSlowdown_GP10X_loop_end;
        }

        // Step 6: Unfake temperature
        reg32 = orig.sensorConst;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _TEMP_SENSOR_CONSTANT, _FIXED_POINT,
                   RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_VARIATION_TEMP), reg32);
        REG_WR32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT, reg32);

        // Step 7: Check if slowdown goes away if event is restored
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));

        if (!FLD_TEST_DRF(_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DISABLED, reg32))
        {
            pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN,
                    SLOWDOWN_RESTORE_FAILURE);
            goto thermTestThermalSlowdown_GP10X_loop_end;
        }

thermTestThermalSlowdown_GP10X_loop_end:
        REG_WR32(CSB,
            LW_CPWR_THERM_EVT_SETTINGS(idx),
               orig.evt);
        if (pParams->outData !=
            LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SUCCESS))
        {
            goto thermTestThermalSlowdown_GP10X_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestThermalSlowdown_GP10X_exit:

    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, orig.power6);
    REG_WR32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT, orig.sensorConst);
    REG_WR32(CSB, LW_CPWR_THERM_USE_B, orig.useB);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, orig.useA);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, orig.eventIntrEnable);

    return FLCN_OK;
}

#ifdef LW_CPWR_THERM_PEAKPOWER_CONFIG1_WINDOW_PERIOD
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
thermTestBaSlowdown_GP10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32  reg32;
    LwU8   idx;
    THERM_TEST_BA_SLWDN_TEMP_CACHE_REGISTERS orig;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    orig.config1         = REG_RD32(CSB, LW_CPWR_THERM_CONFIG1);
    orig.peakPwrConfig1  = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(0));
    orig.baW0t1          = REG_RD32(CSB, LW_CPWR_THERM_EVT_BA_W0_T1);
    orig.eventIntrEnable = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    orig.useA            = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    orig.factorA         = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(0));
    orig.config2         = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(0));
    orig.config4         = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(0));
    orig.factorC         = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(0));

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

    reg32 = orig.useA;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_BA)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_A, _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    reg32 = FLD_SET_DRF(_CPWR_THERM, _USE_A, _BA_W0_T1, _YES, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, reg32);

    // Enable BA
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
    reg32 = orig.peakPwrConfig1;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                        _WINDOW_PERIOD, _MIN, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                        _BA_PEAK_EN, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                        _BA_SRC_LWVDD, _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                        _WINDOW_RST, _TRIGGER, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                        _WINDOW_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(0), reg32);

    // Configure high threshold
    reg32  = orig.config2;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG2,
        _BA_THRESHOLD_1H_VAL, THERM_TEST_BA_HIGH_THRESHOLD, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG2,
        _BA_THRESHOLD_1H_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(0), reg32);

    // Configure low threshold
    reg32  = orig.config4;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG4,
        _BA_THRESHOLD_1L_VAL, THERM_TEST_BA_LOW_THRESHOLD, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG4,
        _BA_THRESHOLD_1L_EN, _ENABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(0), reg32);

    appTaskCriticalEnter();

    // Configure factor A as 0
    reg32 = orig.factorA;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG8,
                        _FACTOR_A, _INIT, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG8,
                        _BA_SUM_SHIFT, _0, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(0), reg32);

    // Configure factor C as > high threshold to trigger slowdown
    reg32 = orig.factorC;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C,
        (THERM_TEST_BA_HIGH_THRESHOLD + THERM_TEST_BA_VARIATE_THRESHOLD), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(0), reg32);

    // Check if the event has been triggered. Fail if not.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
    if (FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _BA_W0_T1,
                     _NORMAL, reg32))
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                           EVT_TRIGGER_FAILURE);
        appTaskCriticalExit();
        goto thermTestBaSlowdown_GP10X_exit;
    }

    // Check CLK_STATUS register to ensure slowdown has oclwred
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));
    if (!FLD_TEST_DRF(_CPWR_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DIV2, reg32))
    {
        pParams->outData =
            LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                HIGH_THRESHOLD_SLOWDOWN_FAILURE);
        appTaskCriticalExit();
        goto thermTestBaSlowdown_GP10X_exit;
    }

    //
    // Configure factor C as : high threshold > C > low threshold
    // and ensure slowdown is retained.
    //
    reg32 = orig.factorC;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C,
        (THERM_TEST_BA_HIGH_THRESHOLD - THERM_TEST_BA_VARIATE_THRESHOLD), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(0), reg32);

    // Check CLK_STATUS register to ensure slowdown is still retained
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));
    if (!FLD_TEST_DRF(_CPWR_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DIV2, reg32))
    {
        pParams->outData =
            LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, WINDOW_SLOWDOWN_FAILURE);
        appTaskCriticalExit();
        goto thermTestBaSlowdown_GP10X_exit;
    }

    // Configure factor C as < low threshold to restore slowdown status
    reg32 = orig.factorC;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C,
        (THERM_TEST_BA_LOW_THRESHOLD - THERM_TEST_BA_VARIATE_THRESHOLD), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(0), reg32);

    //Check if slowdown goes away if event is restored
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_STATUS(LW_CPWR_THERM_CLK_GPCCLK));
    if (!FLD_TEST_DRF(_THERM, _CLK_STATUS, _SLOWDOWN_FACTOR, _DISABLED, reg32))
    {
        pParams->outData =
            LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN,
                SLOWDOWN_RESTORE_FAILURE);
        appTaskCriticalExit();
        goto thermTestBaSlowdown_GP10X_exit;
    }

    appTaskCriticalExit();

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
    pParams->outData   =
        LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SUCCESS);

thermTestBaSlowdown_GP10X_exit:

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(0), orig.factorC);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG4(0), orig.config4);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG2(0), orig.config2);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(0), orig.factorA);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, orig.useA);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, orig.eventIntrEnable);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_BA_W0_T1, orig.baW0t1);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(0), orig.peakPwrConfig1);
    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1, orig.config1);

    return FLCN_OK;
}
#endif

/* ------------------------- Private Functions ------------------------------- */
/*!
 * @brief   Test the  value of temperature for TSENSE
 *
 * @param[out]   pParam       Test parameters, indicating status/output data
 *                 outStatus  Indicates status of the test
 *                 outData    Indicates failure reason of the test
 */
static void
s_thermTestTsenseExelwte_GP10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwTemp  tsenseTemp;
    LwTemp  rawTemp;
    LwTemp  dedThresholdTemp;
    LwU32   origIntrEn;
    LwU32   origUseA;
    LwU32   origSensor7;
    LwU32   origSensor6;
    LwU32   reg32;
    LwS32   rawCode;
    LwU8    idx;
    LwU16   rawCodeMin;
    LwU16   rawCodeMax;

    // Step 1: Cache current HW state
    origIntrEn  = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    origUseA    = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    origSensor7 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_7);
    origSensor6 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_6);

    //
    // Step 2: Setup init HW state for test.
    // Disable all interrupts and slowdowns
    //
    reg32 = origIntrEn;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ENABLE,
                    _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, reg32);

    reg32 = origUseA;
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_A, _CTRL, idx, _NO, reg32);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, reg32);

    //
    // Step 3: Turn on TSENSE and set polling interval as 0, so that we don't
    // have to wait every x ms for the new raw value to be reflected in
    // TSENSE. Wait for _TS_STABLE_CNT period after sensor is powered
    // on for a valid value.
    //
    reg32 = origSensor6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_6, _POWER, _ON, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_6, _POLLING_INTERVAL_PERIOD,
                           _NONE, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, reg32);

    OS_PTIMER_SPIN_WAIT_US(DRF_VAL(_CPWR_THERM, _SENSOR_1, _TS_STABLE_CNT,
        REG_RD32(CSB, LW_CPWR_THERM_SENSOR_1)));

    //
    // Step 4: Get the range of raw values to be tested
    //
    // Get the dedicated overt threshold temperature value.
    // We need to colwert raw value to temperature as long as the temperature
    // is less than dedicated overt threshold, to ensure chip shutdown does
    // not take place while running the test.
    //
    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    //
    dedThresholdTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
        _CPWR_THERM, _EVT_DEDICATED_OVERT, _THRESHOLD,
        REG_RD32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT));

    rawCodeMax = thermGetRawFromTempInt_HAL(&Therm, dedThresholdTemp);
    rawCodeMin = thermGetRawFromTempInt_HAL(&Therm,
         RM_PMU_CELSIUS_TO_LW_TEMP(THERM_INT_SENSOR_WORKING_TEMP_MIN));

    reg32 = origSensor7;
    for (rawCode = rawCodeMin; rawCode < rawCodeMax; rawCode++)
    {
        //
        // Step 5: Colwert each raw value into temperature and see if it falls
        // within bounds of valid temp. Mask off 3 LSB since physical
        // temperature is in 9.5 format
        //
        rawTemp  = 0;
        rawTemp  = (ThermSensorA * rawCode + ThermSensorB) >> (16 - 8);
        rawTemp &= THERM_TEST_HW_UNSUPPORTED_BIT_MASK;

        // Step 6: Fake the temperature by setting the raw override
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OUT,
                                    rawCode, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OVERRIDE,
                               _ENABLE, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, reg32);

        // Step 7: Get the actual tsense value
        tsenseTemp = thermGetTempInternal_HAL(&Therm,
                     LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

        //
        // Step 8: Check if the rawTemp callwlated in SW matches the actual
        // TSENSE value. Fail if not.
        //
        if (rawTemp != tsenseTemp)
        {
            pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData   =
                LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSENSE_VALUE_FAILURE);
            break;
         }
    }

    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, origSensor6);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, origSensor7);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, origUseA);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, origIntrEn);
}

/*!
 * @brief   Test the _SENSOR_CONSTANT, and _SENSOR_MAX registers
 *
 * @param[out]   pParam       Test parameters, indicating status/output data
 *                 outStatus  Indicates status of the test
 *                 outData    Indicates failure reason of the test
*/
static void
s_thermTestConstMaxExelwte_GP10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32   origMax;
    LwU32   reg32;
    LwU32   origConst;
    LwTemp  maxTemp;
    LwTemp  constantTemp = RM_PMU_LW_TEMP_0_C;

    // Initialize outStatus
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Step 1: Cache current HW state
    origConst = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT);
    origMax   = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_9);

    // Step 2: Configure CONSTANT to a min value
    constantTemp = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_INT_SENSOR_WORKING_TEMP_MIN);

    //
    // Write the SW configured constant value to SENSOR_CONSTANT
    // HW uses 9.5 fixed point notation. Colwert LwTemp(24.8) to
    // HW supported format.
    //
    reg32 = origConst;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _TEMP_SENSOR_CONSTANT, _FIXED_POINT,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(constantTemp), reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _TEMP_SENSOR_CONSTANT, _SOURCE,
                            _SW_OVERRIDE, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT, reg32);

    // Step 3: Configure SENSOR_MAX to take values from SENSOR_CONSTANT only
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _CONSTANT, _YES));

    if (thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_CONST) != constantTemp)
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                               CONSTANT_VALUE_FAILURE);
        goto thermTestConstMaxExelwte_GP10X_exit;
    }

    // Step 4: Check if _SENSOR_MAX reflects correct value
    maxTemp = thermGetTempInternal_HAL(&Therm,
                  LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX);

    if (maxTemp != constantTemp)
    {
        pParams->outData =
            LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, MAX_VALUE_FAILURE);
        goto thermTestConstMaxExelwte_GP10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestConstMaxExelwte_GP10X_exit:

    // Restore original values
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9, origMax);
    REG_WR32(CSB, LW_CPWR_THERM_TEMP_SENSOR_CONSTANT, origConst);
}
