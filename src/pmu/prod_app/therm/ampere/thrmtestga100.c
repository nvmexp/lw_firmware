/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtestga100.c
 * @brief   PMU HAL functions related to therm tests for GA100
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_chiplet_pwr.h"
#include "dev_chiplet_pwr_addendum.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"

#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Invalid test temperature.
#define THERM_TEST_TEMP_ILWALID                 (LW_S32_MAX)

// To match with HW, SW should ignore lowest three bits of callwlated result.
#define THERM_TEST_HW_UNSUPPORTED_BIT_MASK      (0xFFFFFFF8)

#define THERM_TEST_HS_TEMP                      RM_PMU_CELSIUS_TO_LW_TEMP(10)
#define THERM_TEST_HS_TEMP_DELTA                RM_PMU_CELSIUS_TO_LW_TEMP(13)
#define THERM_TEST_DEDICATED_OVERT_TEMP         RM_PMU_CELSIUS_TO_LW_TEMP(70)
#define THERM_TEST_TSENSE_TEMP                  RM_PMU_CELSIUS_TO_LW_TEMP(75)
#define THERM_TEST_THERM_MON_TSENSE_TEMP        RM_PMU_CELSIUS_TO_LW_TEMP(75)
#define THERM_TEST_THERM_MON_TSENSE_TEMP_DELTA  RM_PMU_CELSIUS_TO_LW_TEMP(5)
#define THERM_TEST_TEMP_CELSIUS_50              (50)
#define THERM_TEST_TEMP_CELSIUS_40              (40)
#define THERM_TEST_TEMP_CELSIUS_45              (45)

#define LW_GPC_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_INDEX_MAX + 1)
#define LW_BJT_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_BJT_INDEX_MAX + 1)

// SK-TODO: On Silicon Jump Factor needs to lowered.
#define LW_THERM_GPC_TSENSE_RAWCODE_JUMP_FACTOR (0x200)

// 1 Microsecond delay for use in thermal monitors test
#define THERM_TEST_DELAY_1_MICROSECOND          (1)

/* ------------------------- Types Definitions ----------------------------- */
/*
 * This structure is used to cache all HW values for registers that each
 * test uses and restore them back after each test/subtest completes. Inline
 * comments for each variable to register mapping.
 */
typedef struct
{
    LwU32   regIntrEn;                                  // LW_CPWR_THERM_EVENT_INTR_ENABLE
    LwU32   regUseA;                                    // LW_CPWR_THERM_USE_A
    LwU32   regOvertEn;                                 // LW_CPWR_THERM_OVERT_EN
    LwU32   regDedOvert;                                // LW_CPWR_THERM_EVT_DEDICATED_OVERT
    LwU32   regSensor0;                                 // LW_CPWR_THERM_SENSOR_0
    LwU32   regSensor6;                                 // LW_CPWR_THERM_SENSOR_6
    LwU32   regSensor7;                                 // LW_CPWR_THERM_SENSOR_7
    LwU32   regOvertCtrl;                               // LW_CPWR_THERM_OVERT_CTRL
    LwU32   regGlobalOverride;                          // LW_CPWR_THERM_GPC_TSENSE_GLOBAL_OVERRIDE
    LwU32   regEvtThermal1;                             // LW_CPWR_THERM_EVT_THERMAL_1
    LwU32   regPower6;                                  // LW_CPWR_THERM_POWER_6
    LwU32   regPwrRawCodeOverrideBroadcast;             // LW_PCHIPLET_PWR_GPCS_RAWCODE_OVERRIDE

    // Index Registers
    LwU32   regGpcTsenseIdx;                            // LW_CPWR_THERM_GPC_TSENSE_INDEX
    LwU32   regGpcTsenseOverrideIdx;                    // LW_CPWR_GPC_TSENSE_OVERRIDE_INDEX
    LwU32   regGpcTsenseHsOffsetIdx;                    // LW_CPWR_GPC_TSENSE_HS_OFFSET_INDEX

    // Per GPC and BJT registers
    LwU32   regGpcTsenseLwrr[LW_GPC_MAX][LW_BJT_MAX];               // LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR
    LwU32   regGpcTsenseOverride[LW_GPC_MAX][LW_BJT_MAX];           // LW_CPWR_THERM_GPC_TSENSE_OVERRIDE
    LwU32   regGpcTsenseHsOffset[LW_GPC_MAX][LW_BJT_MAX];           // LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET

    // Per GPC registers
    LwU32   regPwrRawCodeOverride[LW_GPC_MAX];                      // LW_PCHIPLET_PWR_GPC##idxGpc##_RAWCODE_OVERRIDE

    LwU32   regMonCtrl[LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1];    // LW_CPWR_THERM_INTR_MONITOR_CTRL
} THERM_TEST_REG_CACHE_GA100;

THERM_TEST_REG_CACHE_GA100  thermRegCacheGA100
    GCC_ATTRIB_SECTION("dmem_libThermTest", "thermRegCacheGA100");
THERM_TEST_REG_CACHE_GA100 *pRegCacheGA100 = NULL;

/* ------------------------- Prototypes -------------------- */
static FLCN_STATUS s_thermTestTempColwersion_GA100(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTempColwersion_GA100");

static FLCN_STATUS s_thermTestMaxAvgExelwte_GA100(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestMaxAvgExelwte_GA100");

static FLCN_STATUS s_thermTestGlobalSnapshotFunc_GA100(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotFunc_GA100");

static FLCN_STATUS s_thermTestGlobalSnapshotHwPipeline_GA100(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotHwPipeline_GA100");

static void s_thermTestRegCacheInit_GA100(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheInit_GA100");

static void s_thermTestRegCacheRestore_GA100(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheRestore_GA100");

static FLCN_STATUS s_thermTestRegCacheInitRestore_GA100(LwBool)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheInitRestore_GA100");

static void s_thermTestDisableSlowdownInterrupt_GA100(LwU32 regIntrEn, LwU32 regUseA)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDisableSlowdownInterrupt_GA100");

static void s_thermTestDisableOvertForAllEvents_GA100(LwU32 regOvertEn)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDisableOvertForAllEvents_GA100");

static void s_thermTestDedicatedOvertSet_GA100(LwU32 regOvert, LwTemp dedOvertTemp, LwU8 provIdx)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDedicatedOvertSet_GA100");

static void s_thermTestGpcBjtIndexSet_GA100(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcBjtIndexSet_GA100");

static void s_thermTestGpcTsenseTempParamsGet_GA100(LwUFXP16_16 *pSlopeA, LwUFXP16_16 *pOffsetB)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseTempParamsGet_GA100");

static LwU16 s_thermTestRawCodeForTempGet_GA100(LwUFXP16_16 slopeA, LwUFXP16_16 offsetB, LwTemp temp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRawCodeForTempGet_GA100");

static void s_thermTestGpcRawCodeBroadcast_GA100(LwU32 regPwrRawCodeOverride, LwU32 rawCode)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcRawCodeBroadcast_GA100");

static LwTemp s_thermTestGpcTsenseTempGet_GA100(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseTempGet_GA100");

static LwU32 s_thermTestGpcEnMaskGet_GA100(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcEnMaskGet_GA100_GA100");

static LwU32 s_thermTestBjtEnMaskForGpcGet_GA100(LwU8 idxGpc)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestBjtEnMaskForGpcGet_GA100");

static void s_thermTestGpcBjtOverrideIndexSet_GA100(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcBjtOverrideIndexSet_GA100");

static void s_thermTestGpcBjtHsOffsetIndexSet_GA100(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcBjtHsOffsetIndexSet_GA100");

static void s_thermTestTempSet_GA100(LwU32 regTempOverride, LwTemp temp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTempSet_GA100");

static void s_thermTestHotspotOffsetSet_GA100(LwU32 regHsOffset, LwTemp temp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestHotspotOffsetSet_GA100");

static void s_thermTestGlobalOverrideSet_GA100(LwU32 regGlobalOverride, LwTemp overrideTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalOverrideSet_GA100");

static void s_thermTestSysTsenseTempSet_GA100(LwU32 regSensor0, LwTemp gpuTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSysTsenseTempSet_GA100");

static void s_thermTestParamSetThermalEvent1_GA100(LwU32 regVal, LwTemp threshold, LwU8 sensorId)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestParamSetThermalEvent1_GA100");

static void s_thermTestGlobalSnapshotTrigger_GA100(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotTrigger_GA100");

static LwU32 s_thermTestFilterIntervalTimeUsGet_GA100(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestFilterIntervalTimeUsGet_GA100");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Tests internal sensors.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the system.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
thermTestIntSensors_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData   = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SUCCESS);

    // Test GPU_MAX, GPU_AVG and MAX.
    status = s_thermTestMaxAvgExelwte_GA100(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestIntSensors_GA100_exit;
    }

    // Test temperature colwersion Logic for the HW.
    status = s_thermTestTempColwersion_GA100(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestIntSensors_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestIntSensors_GA100_exit:
    return status;
}

/*!
 * @brief  Tests Dedicated Overt hardware logic.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        This test is expected to fail, since violating DEDICATED_OVERT
 *              threshold should cause GPU to go off the bus.
 */
FLCN_STATUS
thermTestDedicatedOvert_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       rawCode;
    LwU32       reg32;
    LwTemp      sysTsenseTemp;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    //
    // Turn on TSENSE and set polling interval as 0, so that we don't
    // have to wait every x ms for the new raw value to be reflected in
    // TSENSE. Wait for _TS_STABLE_CNT period after sensor is powered
    // on for a valid value.
    //
    reg32 = pRegCacheGA100->regSensor6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_6, _POWER, _ON, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_6, _POLLING_INTERVAL_PERIOD,
                _NONE, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, reg32);

    OS_PTIMER_SPIN_WAIT_US(DRF_VAL(_CPWR_THERM, _SENSOR_1, _TS_STABLE_CNT,
                REG_RD32(CSB, LW_CPWR_THERM_SENSOR_1)));

    //
    // Dedicated overt doesn't use _SW_OVERRIDE temperature for any of the
    // temperature sources. It always uses the _REAL values from HW. So we fake
    // raw code values for TSENSE instead of faking output of TSENSE directly
    // via SW_OVERRIDE.
    //
    rawCode = thermGetRawFromTempInt_HAL(&Therm, THERM_TEST_TSENSE_TEMP);

    // Fake the temperature by setting the raw override.
    reg32 = pRegCacheGA100->regSensor7;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OUT,
                rawCode, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OVERRIDE,
                _ENABLE, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, reg32);

    // Get the actual tsense value.
    sysTsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);
    if (sysTsenseTemp != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT,
                TSENSE_RAW_MISMATCH_FAILURE);
        goto thermTestDedicatedOvert_GA100_exit;
    }

    // Enable overt temperature on boot.
    reg32 = pRegCacheGA100->regOvertCtrl;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _OTOB_ENABLE, _ON, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, reg32);

    // Set the dedicated overt threshold to lesser than set above.
    s_thermTestDedicatedOvertSet_GA100(pRegCacheGA100->regDedOvert,
                THERM_TEST_DEDICATED_OVERT_TEMP,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    //
    // On Emulator, GPU will not powerdown after overt trigger as lwvdd clamp wont be asserted.
    // So, GPU will NOT go off the bus but OVERT will be triggerred.
    // On Production / engineering board, GPU will go offline.
    //
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES, reg32))
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
        goto thermTestDedicatedOvert_GA100_exit;
    }

    //
    // Ideally, we should not reach here as GPU is already off the bus on Production board,
    // Or INT_OVERT_ASSERTED triggered on Emulator.
    //
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData =  LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT,
                SHUTDOWN_FAILURE);

thermTestDedicatedOvert_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return FLCN_OK;
}

/*!
 * @brief  Negative testing for dedicated overt.
 * Checks that SW override temperatures don't trigger a dedicated overt event.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value
 *
 * @return      FLCN_OK                If test is supported on the GPU
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus. If GPU goes
 *              off the bus for this test, this test has failed.
 */
FLCN_STATUS
thermTestDedicatedOvertNegativeCheck_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       reg32;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwU32       sysTsenseTemp;
    LwU8        idxGpc;
    LwU8        idxBjt;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    // Override TSENSE using SW override.
    s_thermTestSysTsenseTempSet_GA100(pRegCacheGA100->regSensor0,
                THERM_TEST_TSENSE_TEMP);

    // Get the actual tsense value.
    sysTsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);
    if (sysTsenseTemp != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT,
                TSENSE_RAW_MISMATCH_FAILURE);
        goto thermTestDedicatedOvertNegativeCheck_GA100_exit;
    }

    // Enable overt temperature on boot.
    reg32 = pRegCacheGA100->regOvertCtrl;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _OTOB_ENABLE, _ON, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, reg32);

    s_thermTestDedicatedOvertSet_GA100(pRegCacheGA100->regDedOvert,
                THERM_TEST_DEDICATED_OVERT_TEMP,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    // Check overt control register for overt assertion state, fail if asserted.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES,
                reg32))
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE,
                SHUTDOWN_FAILURE_TSENSE);
        goto thermTestDedicatedOvertNegativeCheck_GA100_exit;
    }

    //
    // First use global override to fake all GPC TSENSEs since local override
    // has higher priority than global override.
    //
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                (THERM_TEST_TSENSE_TEMP + THERM_TEST_HS_TEMP));

    //
    // Set the dedicated overt temp to a value which will be less than
    // OVERRIDE/GLOBAL_OVERRIDE temperature. This is to verify GPU
    // does not shut down if temperature is faked via
    // TSENSE_OVERRIDE/TSENSE_GLOBAL_OVERRIDE. If GPU shuts down, the
    // test has failed. AVG will consider values for all enabled & valid TSENSE readings.
    //
    s_thermTestDedicatedOvertSet_GA100(pRegCacheGA100->regDedOvert,
                THERM_TEST_DEDICATED_OVERT_TEMP,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);

    // Check overt control register for overt assertion state, fail if asserted.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES,
                reg32))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE,
                SHUTDOWN_FAILURE_TSOSC_GLOBAL);
        goto thermTestDedicatedOvertNegativeCheck_GA100_exit;
    }

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA100();

    //
    // Loop over all enabled GPC TSENSEs to fake the temperature values.
    // Use same values for each GPC TSENSE as we don't need different values.
    //
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Select BJT in OVERRIDE_INDEX.
            s_thermTestGpcBjtOverrideIndexSet_GA100(idxGpc, idxBjt);

            // Fake Lwrr Temp for GPC TSENSE Selected.
            s_thermTestTempSet_GA100(
                pRegCacheGA100->regGpcTsenseOverride[idxGpc][idxBjt],
                THERM_TEST_TSENSE_TEMP);

            //
            // Check the overt control register for overt assertion state due
            // to any of the individual GPC TSENSE, fail if it's asserted.
            //
            reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
            if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED,
                    _YES, reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData =
                        LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE,
                        SHUTDOWN_FAILURE_TSOSC_LOCAL);
                goto thermTestDedicatedOvertNegativeCheck_GA100_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestDedicatedOvertNegativeCheck_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return FLCN_OK;
}

/*!
 * @brief  Tests Snapshot functionality.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the system.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
thermTestGlobalSnapshot_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData   = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SUCCESS);

    // Test HW Global Snapshot functionality.
    status = s_thermTestGlobalSnapshotFunc_GA100(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestGlobalSnapshot_GA100_exit;
    }

    // Test HW Pipeline functionality over nonSnapshot values.
    status = s_thermTestGlobalSnapshotHwPipeline_GA100(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestGlobalSnapshot_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestGlobalSnapshot_GA100_exit:
    return status;
}

/*!
 * @brief  Tests Thermal Monitors.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value
 *
 * @return      FLCN_OK                If test is supported on the system
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus
 */
FLCN_STATUS
thermTestThermalMonitors_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       reg32;
    LwU32       val1;
    LwU32       val2;
    LwU8        idx;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    //
    // Disable filtering of thermal events. No need to filter in the test otherwise
    // need to add delay until filtering is complete and until monitors can start
    // counting on the event.
    //
    reg32 = pRegCacheGA100->regPower6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_PERIOD,
                _NONE, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_SCALE,
                _16US, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, reg32);

    // Disable overt for Thermal Event 1.
    reg32 = pRegCacheGA100->regOvertEn;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_EN, _THERMAL_1, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, reg32);

    // Configure Thermal Event 1 to trigger on TSENSE sensor.
    reg32 = pRegCacheGA100->regEvtThermal1;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_THERMAL_1, _TEMP_SENSOR_ID, _TSENSE, reg32);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_THERMAL_1, _TEMP_THRESHOLD,
                RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_THERM_MON_TSENSE_TEMP), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_THERMAL_1, reg32);

    // Override TSENSE using SW override.
    s_thermTestSysTsenseTempSet_GA100(pRegCacheGA100->regSensor0,
        THERM_TEST_THERM_MON_TSENSE_TEMP + THERM_TEST_THERM_MON_TSENSE_TEMP_DELTA);

    // Loop over each Therm Monitor
    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        // Clear & Enable the Monitor
        reg32 = pRegCacheGA100->regMonCtrl[idx];
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _EN, _ENABLE, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _CLEAR, _TRIGGER, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), reg32);

        // Configure the monitor to monitor Active High, Level Triggered Thermal 1 event
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx));
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _TYPE, _LEVEL, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _POLARITY, _HIGH_ACTIVE, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _SOURCE, _THERMAL_1, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), reg32);

        // Read the monitor value for first time.
        val1 = DRF_VAL(_CPWR_THERM, _INTR_MONITOR_STATE, _VALUE,
                    REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_STATE(idx)));

        // Wait for 1 microsecond to let the monitor value increment
        OS_PTIMER_SPIN_WAIT_US(THERM_TEST_DELAY_1_MICROSECOND);

        // Read the monitor value again for the second time
        val2 = DRF_VAL(_CPWR_THERM, _INTR_MONITOR_STATE, _VALUE,
                    REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_STATE(idx)));

        // Bail out if the value read second time is not more than the first one
        if (val2 <= val1)
        {
            pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_MONITORS,
                INCREMENT_FAILURE);
            goto thermTestThermalMonitors_GP10X_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestThermalMonitors_GP10X_exit:

    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);

    return FLCN_OK;
}

/* ------------------- Static Functions ------------------------ */
/*!
 * @brief  Tests GPC TSENSE Temp Colwersion Logic.
 *
 * @param[in/out]  pParams   Test Params, indicating status of test
 *                           and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
static FLCN_STATUS
s_thermTestTempColwersion_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwUFXP16_16 slopeA  = LW_TYPES_FXP_ZERO;
    LwUFXP16_16 offsetB = LW_TYPES_FXP_ZERO;
    LwTemp      overtTempThres;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwU32       rawCode;
    LwTemp      tsenseTemp;
    LwTemp      tsenseHsTemp;
    LwTemp      testTemp;
    LwTemp      testHsTemp;
    LwU16       rawCodeMin;
    LwU16       rawCodeMax;
    LwU8        idxGpc;
    LwU8        idxBjt;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the inital context for HW.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes in the
    // test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    // Disable overt for all events.
    s_thermTestDisableOvertForAllEvents_GA100(pRegCacheGA100->regOvertEn);

    //
    // Get the range of raw values to be tested. For this, get the dedicated
    // overt threshold temperature value. We need to colwert raw value to
    // temperature as long as the temperature is less than dedicated overt
    // threshold, to ensure chip shutdown do not take place while running test.
    //
    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    //
    overtTempThres = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                    _CPWR_THERM, _EVT_DEDICATED_OVERT, _THRESHOLD,
                    REG_RD32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT));

    // Set any SENSOR_ID other than GPC TSENSE to avoid triggering overt.
    s_thermTestDedicatedOvertSet_GA100(pRegCacheGA100->regDedOvert,
                overtTempThres, LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    // Get a valid slopeA and offsetB for the TSENSE sensor.
    s_thermTestGpcTsenseTempParamsGet_GA100(&slopeA, &offsetB);
    if (slopeA == LW_TYPES_FXP_ZERO)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SLOPEA_NULL_FAILURE);
            goto s_thermTestTempColwersion_GA100_exit;
    }

    // Get min rawcode to fake.
    rawCodeMin = s_thermTestRawCodeForTempGet_GA100(slopeA, offsetB,
                RM_PMU_CELSIUS_TO_LW_TEMP(THERM_INT_SENSOR_WORKING_TEMP_MIN));

    // Get max rawcode to fake.
    rawCodeMax = s_thermTestRawCodeForTempGet_GA100(slopeA, offsetB, overtTempThres);

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA100();

    // Loop for each Enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Select GPCs.
        s_thermTestGpcBjtIndexSet_GA100(idxGpc, LW_THERM_GPC_TSENSE_INDEX_GPC_BJT_INDEX_MIN);

        for (rawCode = rawCodeMin; rawCode < rawCodeMax;)
        {
            //
            // temp = (slope_A * rawCode - offset_B).
            // Colwert each raw value into temperature and see if it falls
            // within bounds of valid temp. Mask off 3 LSB since physical
            // temperature is in 9.5 format.
            //
            testTemp = (LwTemp)(((LwSFXP16_16)(slopeA * rawCode) - (LwSFXP16_16)offsetB) >> 8);
            testTemp &= THERM_TEST_HW_UNSUPPORTED_BIT_MASK;

            //
            // Fake the rawcode in GPCPWR. Its per GPC.
            // WAR for bug http://lwbugs/200550505
            // No one to one mapping from GPC PWR CHIPLET to LW_THERM. So, broadcast instaed of unicast.
            //
            s_thermTestGpcRawCodeBroadcast_GA100(pRegCacheGA100->regPwrRawCodeOverrideBroadcast,
                    rawCode);

            //
            // RawCode Faked, add delay to reflect in therm.
            // SK-TODO: This is not working as expected on emulation.
            // Need to find the correct delay to provide here.
            //
            OS_PTIMER_SPIN_WAIT_US(30 * 1000);

            // Get BJT-s enabled for the GPC index.
            bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

            FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
            {
                // Select BJT for Above GPC selected.
                s_thermTestGpcBjtIndexSet_GA100(idxGpc, idxBjt);

                // Get the tsense temperature of index already set above.
                tsenseTemp = s_thermTestGpcTsenseTempGet_GA100(LW_FALSE);

                //
                // Check if the rawTemp callwlated in SW matches the actual
                // GPC TSENSE value. Fail if not.
                //
                if (testTemp != tsenseTemp)
                {
                    pParams->outStatus =
                            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                    pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(
                            INT_SENSORS,
                            TSOSC_RAW_TEMP_FAILURE);
                    goto s_thermTestTempColwersion_GA100_exit;
                }

                // Select BJT in HS_OFFSET_INDEX.
                s_thermTestGpcBjtHsOffsetIndexSet_GA100(idxGpc, idxBjt);

                // Set the hotspot offset for the selected GPC TSENSE sensor.
                s_thermTestHotspotOffsetSet_GA100(
                        pRegCacheGA100->regGpcTsenseHsOffset[idxGpc][idxBjt],
                        THERM_TEST_HS_TEMP);

                testHsTemp = testTemp + THERM_TEST_HS_TEMP;

                // Get the GPC TSENSE + HS temperature.
                tsenseHsTemp = s_thermTestGpcTsenseTempGet_GA100(LW_TRUE);
                if (testHsTemp != tsenseHsTemp)
                {
                    pParams->outStatus =
                        LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                    pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(
                        INT_SENSORS, TSOSC_RAW_TEMP_HS_FAILURE);
                    goto s_thermTestTempColwersion_GA100_exit;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;

            // This test may take long on emulator. Jump factor to complete test soon.
            if (rawCode == (rawCodeMax - 1))
            {
                break;
            }
            rawCode = LW_MIN((rawCode + LW_THERM_GPC_TSENSE_RAWCODE_JUMP_FACTOR),
                        (rawCodeMax - 1));
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestTempColwersion_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return status;
}

/*!
 * @brief  Tests MAX, GPU_MAX, GPU_AVG internal sensors.
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 * @return      FLCN_ERR_NO_FREE_MEM   If there is no free memory.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
static FLCN_STATUS
s_thermTestMaxAvgExelwte_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       bjtCount  = 0;
    LwS32       maxTemp   = 0;
    LwS32       avgTemp   = 0;
    LwS32       testTemp  = THERM_TEST_TEMP_CELSIUS_50;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwTemp      tsenseTemp;
    LwU32       testCount;
    LwU8        idxGpc;
    LwU8        idxBjt;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the initial context for HW.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA100();

    // Loop each enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

        // Maintain count of BJT-s.
        testCount = bjtEnMask;
        NUMSETBITS_32(testCount);
        bjtCount += testCount;

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Select BJT in OVERRIDE_INDEX.
            s_thermTestGpcBjtOverrideIndexSet_GA100(idxGpc, idxBjt);

            // Fake Lwrr Temp for GPC TSENSE Selected.
            s_thermTestTempSet_GA100(
                pRegCacheGA100->regGpcTsenseOverride[idxGpc][idxBjt],
                RM_PMU_CELSIUS_TO_LW_TEMP(testTemp));

            // Select BJT in TSENSE_INDEX.
            s_thermTestGpcBjtIndexSet_GA100(idxGpc, idxBjt);

            // Get Lwrr temp.
            tsenseTemp = s_thermTestGpcTsenseTempGet_GA100(LW_FALSE);
            if (tsenseTemp != RM_PMU_CELSIUS_TO_LW_TEMP(testTemp))
            {
                pParams->outStatus =
                    LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                    TSOSC_TEMP_FAKING_FAILURE);
                goto s_thermTestMaxAvgExelwte_GA100_exit;
            }

            // Select BJT in HS_OFFSET_INDEX.
            s_thermTestGpcBjtHsOffsetIndexSet_GA100(idxGpc, idxBjt);

            // Set the hotspot offset for the selected GPC TSENSE sensor.
            s_thermTestHotspotOffsetSet_GA100(
                pRegCacheGA100->regGpcTsenseHsOffset[idxGpc][idxBjt],
                THERM_TEST_HS_TEMP);

            maxTemp  = LW_MAX(maxTemp, testTemp);
            avgTemp += testTemp;

            //
            // Increase temperature by actual BJT count to avoid
            // precision loss when callwlating average of all GPC TSENSEs.
            // SK-TODO: Need to increment by proper count.
            //
            testTemp += testCount;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Callwlate the average TSENSE temp.
    if (bjtCount != 0)
    {
        avgTemp = (avgTemp / bjtCount);
    }

    // Get the actual GPU tsense avg temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);
    if (LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(tsenseTemp) != avgTemp)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_AVG_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA100_exit;
    }

    // Get the actual tsense offset avg temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_AVG);
    if (LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(tsenseTemp) !=
        (avgTemp + LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(THERM_TEST_HS_TEMP)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_OFFSET_AVG_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA100_exit;
    }

    // Get the actual tsense max temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX);
    if (tsenseTemp != RM_PMU_CELSIUS_TO_LW_TEMP(maxTemp))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_MAX_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA100_exit;
    }

    // Get the actual tsense offset max temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);
    if (tsenseTemp != ((RM_PMU_CELSIUS_TO_LW_TEMP(maxTemp) + THERM_TEST_HS_TEMP)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_OFFSET_MAX_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA100_exit;
    }

    //
    // Configure SENSOR_MAX to point to TSENSE_MAX, TSENSE_OFFSET_MAX,
    // TSENSE_OFFSET_AVG and TSENSE_AVG. This is done, as, until this point
    // we have only checked for individual MAX/AVG/MAX+hotspot/AVG+hotspot
    // This ensures SENSOR_9_MAX actually touches all registers and returns
    // correct value.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_MAX, _YES) |
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_AVG, _YES) |
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_OFFSET_MAX, _YES) |
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_OFFSET_AVG, _YES));

    // Check if _SENSOR_MAX reflects correct value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);
    if (tsenseTemp != thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                MAX_VALUE_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestMaxAvgExelwte_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return status;
}

/*!
 * @brief  Tests Global snapshot. It should freeze only snapshot values,
 *          not non-snapshot values.
 *
 * @param[in/out]  pParams   Test Params, indicating status of test
 *                           and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
static FLCN_STATUS
s_thermTestGlobalSnapshotFunc_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwTemp      tempOverride_X;
    LwTemp      tempAvgSnapshot_X;
    LwTemp      tempAvgNonSnapshot_X;
    LwTemp      tempOverride_Y;
    LwTemp      tempAvgSnapshot_Y;
    LwTemp      tempAvgNonSnapshot_Y;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the inital context for HW.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes
    // in the test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    // Override temp for all GPC.
    tempOverride_X = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_40);
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                tempOverride_X);

    // Verify global override temp with GPU_AVG temp.
    tempAvgNonSnapshot_X = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPU_AVG, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPU_AVG));
    if (tempAvgNonSnapshot_X != tempOverride_X)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                TEMP_OVERRIDE_FAILURE);
        goto s_thermTestGlobalSnapshotFunc_GA100_exit;
    }

    // Trigger Global Snapshot.
    s_thermTestGlobalSnapshotTrigger_GA100();

    // Verify override temp with snapshot temp followed by global snapshot.
    tempAvgSnapshot_X = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPU_AVG_SNAPSHOT, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPU_AVG_SNAPSHOT));
    if (tempAvgSnapshot_X != tempAvgNonSnapshot_X)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                OVERRIDE_SNAPSHOT_FAILURE);
        goto s_thermTestGlobalSnapshotFunc_GA100_exit;
    }

    // Override temp for all GPC as non snapshot values.
    tempOverride_Y = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_50);
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                tempOverride_Y);

    // Verify global override temp with GPU_AVG temp.
    tempAvgNonSnapshot_Y = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPU_AVG, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPU_AVG));
    if (tempAvgNonSnapshot_Y != tempOverride_Y)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                TEMP_OVERRIDE_FAILURE);
        goto s_thermTestGlobalSnapshotFunc_GA100_exit;
    }

    // Post fake Snapshot value should match previous snapshot value.
    tempAvgSnapshot_Y = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPU_AVG_SNAPSHOT, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPU_AVG_SNAPSHOT));
    if (tempAvgSnapshot_Y != tempAvgSnapshot_X)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                SAVE_NON_SNAPSHOT_FAILURE);
        goto s_thermTestGlobalSnapshotFunc_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestGlobalSnapshotFunc_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return status;
}

/*!
 * @brief  Tests HW pipeline for snapshot. HW pipeline should not be affected
 *         by Snapshot values.
 *
 * @param[in/out]  pParams   Test Params, indicating status of test
 *                           and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
static FLCN_STATUS
s_thermTestGlobalSnapshotHwPipeline_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwTemp      tempEvent1Th;
    LwTemp      tempOverride;
    LwTemp      tempAvgSnapshot;
    LwU32       filterTimeUs;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the inital context for HW.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes
    // in the test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    // Set sensor id and threshold for EVT_THERMAL_1.
    tempEvent1Th = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_45);
    s_thermTestParamSetThermalEvent1_GA100(pRegCacheGA100->regEvtThermal1,
                tempEvent1Th,
                LW_CPWR_THERM_EVT_THERMAL_1_TEMP_SENSOR_ID_GPU_AVG);

    filterTimeUs = s_thermTestFilterIntervalTimeUsGet_GA100();

    // Override temp for all GPC with temp below event1 threshold.
    tempOverride = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_40);
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                tempOverride);

    // Trigger Global Snapshot.
    s_thermTestGlobalSnapshotTrigger_GA100();

    //
    // Verify AVG temp with temp set below event threshold.
    // Event should not be triggered here.
    //
    tempAvgSnapshot = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPU_AVG_SNAPSHOT, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPU_AVG_SNAPSHOT));
    if (tempAvgSnapshot != tempOverride)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                HW_PIPELINE_TEMP_OVERRIDE_FAILURE);
        goto s_thermTestGlobalSnapshotHwPipeline_GA100_exit;
    }

    // Wait for Filter Period to reflect event.
    OS_PTIMER_SPIN_WAIT_US(4 * filterTimeUs);

    if (FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _THERMAL_1, _TRIGGERED,
                REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                UNDER_TEMP_EVENT_TRIGGER_FAILURE);
        goto s_thermTestGlobalSnapshotHwPipeline_GA100_exit;
    }

    //
    // Override temp for all GPC with temp above event1 threshold.
    // Event shall be triggered here.
    //
    tempOverride = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_50);
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                tempOverride);

    // Wait for Filter Period to reflect event.
    OS_PTIMER_SPIN_WAIT_US(4 * filterTimeUs);

    if (!FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _THERMAL_1, _TRIGGERED,
                REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                OVER_TEMP_EVENT_TRIGGER_FAILURE);
        goto s_thermTestGlobalSnapshotHwPipeline_GA100_exit;
    }

    //
    // Event Triggered so recover by overrriding temp below event1 threshold and
    // waiting for filter time period to get it normal.
    //
    tempOverride = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_40);
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                tempOverride);

    // Wait for Filter Period to reflect event.
    OS_PTIMER_SPIN_WAIT_US(4 * filterTimeUs);

    // Event1 should not be triggered here.
    if (FLD_TEST_DRF(_CPWR_THERM, _EVENT_TRIGGER, _THERMAL_1, _TRIGGERED,
                REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT,
                EVENT_TRIGGER_ROLLBACK_FAILURE);
        goto s_thermTestGlobalSnapshotHwPipeline_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestGlobalSnapshotHwPipeline_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return status;
}

/*!
 * @brief  Test priority for faking GPC TSENSE temperature
 *         (i.e local per BJT > global > rawcode override).
 *
 * @param[out]  pParams   Test Params, indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
thermTestTempOverride_GA100
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwUFXP16_16 slopeA  = LW_TYPES_FXP_ZERO;
    LwUFXP16_16 offsetB = LW_TYPES_FXP_ZERO;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwU32       rawCode;
    LwTemp      tsenseTemp;
    LwU8        idxGpc;
    LwU8        idxBjt;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GA100(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA100(pRegCacheGA100->regIntrEn,
                pRegCacheGA100->regUseA);

    //
    // Get GPC Enabled Mask.
    // SK-TODO: Global Override overrides temperature for floorswept GPC also.
    // Ignoring floorswept GPC and relying on BJT only.
    //
    gpcEnMask = ((1 << LW_GPC_MAX) - 1);

    // Loop each enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Select BJT in OVERRIDE_INDEX.
            s_thermTestGpcBjtOverrideIndexSet_GA100(idxGpc, idxBjt);

            // Fake Current Temp for GPC TSENSE Selected.
            s_thermTestTempSet_GA100(
                pRegCacheGA100->regGpcTsenseOverride[idxGpc][idxBjt],
                THERM_TEST_TSENSE_TEMP);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Use global override to fake all TSENSE sensors to a value greater than
    // the value used in faking individual TSENSE sensors.
    //
    s_thermTestGlobalOverrideSet_GA100(pRegCacheGA100->regGlobalOverride,
                (THERM_TEST_TSENSE_TEMP + THERM_TEST_HS_TEMP));

    //
    // As all GPC Tsense-s are faked to same value using local/global override,
    // MAX/AVG will report same temperature, so using MAX here instead of reading
    // each GPC TSENSE-s values individually.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_MAX, _YES));

    //
    // If both global and individual TSENSE temperatures are faked,
    // the temperature for individual TSENSE should take priority.
    //
    if (thermGetTempInternal_HAL(&Therm,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX) != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                GLOBAL_OVERRIDE_PRIORITY_FAILURE);
        goto thermTestTempOverride_GA100_exit;
    }

    // Get a valid slopeA and offsetB for GPC TSENSE-s.
    s_thermTestGpcTsenseTempParamsGet_GA100(&slopeA, &offsetB);
    if (slopeA == LW_TYPES_FXP_ZERO)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                SLOPEA_NULL_FAILURE);
        goto thermTestTempOverride_GA100_exit;
    }

    // Get rawcode corrosponding to temperature to fake.
    rawCode = s_thermTestRawCodeForTempGet_GA100(slopeA, offsetB,
                RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TSENSE_TEMP +
                THERM_TEST_HS_TEMP_DELTA));

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA100();

    // Loop for each Enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        //
        // Fake the rawcode in GPCPWR. Its per GPC.
        // WAR for bug http://lwbugs/200550505
        // No one to one mapping from GPC PWR CHIPLET to LW_THERM. So, broadcast instaed of unicast.
        //
        s_thermTestGpcRawCodeBroadcast_GA100(
                pRegCacheGA100->regPwrRawCodeOverrideBroadcast,
                rawCode);

        // RawCode Faked, add delay to reflect in therm.
        OS_PTIMER_SPIN_WAIT_US(30 * 1000);

        // Get BJT-s enabled for the GPC index.
        bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Select BJT for Above GPC selected.
            s_thermTestGpcBjtIndexSet_GA100(idxGpc, idxBjt);

            // Get the tsense temperature of index already set above.
            tsenseTemp = s_thermTestGpcTsenseTempGet_GA100(LW_FALSE);

            //
            // Until here as individual, global and rawCode override are set,
            // the local override faking takes priority.
            //
            if (tsenseTemp != THERM_TEST_TSENSE_TEMP)
            {
                pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                        TSOSC_RAW_TEMP_FAILURE);
                goto thermTestTempOverride_GA100_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // If both global and individual TSOSC temperatures are faked, the
    // temperature for individual TSOSC should take priority. Raw code has
    // lowest priority. If selwre_override is selected then higher of raw
    // code and override value will be selected.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_MAX, _YES));

    if (thermGetTempInternal_HAL(&Therm,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX) != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                RAW_OVERRIDE_PRIORITY_FAILURE);
        goto thermTestTempOverride_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestTempOverride_GA100_exit:
    s_thermTestRegCacheInitRestore_GA100(LW_FALSE);
    return FLCN_OK;
}

/*!
 * @brief Interface to Save Registers from HW to regCache.
 *
 * @param[in] void
 * @return void
 *
 */
static void
s_thermTestRegCacheInit_GA100(void)
{
    LwU32   gpcEnMask;
    LwU32   bjtEnMask;
    LwU8    idxGpc;
    LwU8    idxBjt;
    LwU8    idx;

    pRegCacheGA100->regIntrEn           = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    pRegCacheGA100->regUseA             = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    pRegCacheGA100->regOvertEn          = REG_RD32(CSB, LW_CPWR_THERM_OVERT_EN);
    pRegCacheGA100->regOvertCtrl        = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    pRegCacheGA100->regSensor0          = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_0);
    pRegCacheGA100->regSensor6          = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_6);
    pRegCacheGA100->regSensor7          = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_7);
    pRegCacheGA100->regEvtThermal1      = REG_RD32(CSB, LW_CPWR_THERM_EVT_THERMAL_1);
    pRegCacheGA100->regPower6                = REG_RD32(CSB, LW_CPWR_THERM_POWER_6);
    pRegCacheGA100->regPwrRawCodeOverrideBroadcast = REG_RD32(BAR0, LW_PCHIPLET_PWR_GPCS_RAWCODE_OVERRIDE);

    pRegCacheGA100->regGpcTsenseIdx         = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX);
    pRegCacheGA100->regGpcTsenseOverrideIdx = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE_INDEX);
    pRegCacheGA100->regGpcTsenseHsOffsetIdx = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET_INDEX);
    pRegCacheGA100->regDedOvert             = REG_RD32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT);
    pRegCacheGA100->regGlobalOverride       = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_GLOBAL_OVERRIDE);

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        pRegCacheGA100->regMonCtrl[idx] = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx));
    }

    gpcEnMask = s_thermTestGpcEnMaskGet_GA100();

    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Save LW_PCHIPLET_PWR_GPC_##idx##_RAWCODE_OVERRIDE.
            pRegCacheGA100->regPwrRawCodeOverride[idxGpc] = REG_RD32(BAR0,
                LW_PCHIPLET_PWR_GPC_RAWCODE_OVERRIDE(idxGpc));

            // Select BJT in TSENSE_INDEX and save related register arrays.
            s_thermTestGpcBjtIndexSet_GA100(idxGpc, idxBjt);
            pRegCacheGA100->regGpcTsenseLwrr[idxGpc][idxBjt] = REG_RD32(
                CSB, LW_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR);

            // Select BJT in OVERRIDE_INDEX and save related register arrays.
            s_thermTestGpcBjtOverrideIndexSet_GA100(idxGpc, idxBjt);
            pRegCacheGA100->regGpcTsenseOverride[idxGpc][idxBjt] = REG_RD32(
                CSB, LW_THERM_GPC_TSENSE_OVERRIDE);

            // Select BJT in HS_OFFSET_INDEX and save related register arrays.
            s_thermTestGpcBjtHsOffsetIndexSet_GA100(idxGpc, idxBjt);
            pRegCacheGA100->regGpcTsenseHsOffset[idxGpc][idxBjt] = REG_RD32(
                CSB, LW_THERM_GPC_TSENSE_HS_OFFSET);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Interface to Restore Registers from regCache to HW.
 */
static void
s_thermTestRegCacheRestore_GA100(void)
{
    LwU32   gpcEnMask;
    LwU32   bjtEnMask;
    LwU32   filterTimeUs;
    LwU8    idxGpc;
    LwU8    idxBjt;
    LwU8    idx;

    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, pRegCacheGA100->regIntrEn);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, pRegCacheGA100->regUseA);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_0, pRegCacheGA100->regSensor0);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, pRegCacheGA100->regSensor6);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, pRegCacheGA100->regSensor7);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_THERMAL_1, pRegCacheGA100->regEvtThermal1);
    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, pRegCacheGA100->regPower6);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPCS_RAWCODE_OVERRIDE, pRegCacheGA100->regPwrRawCodeOverrideBroadcast);

    REG_WR32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT, pRegCacheGA100->regDedOvert);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_GLOBAL_OVERRIDE, pRegCacheGA100->regGlobalOverride);

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), pRegCacheGA100->regMonCtrl[idx]);
    }

    gpcEnMask = s_thermTestGpcEnMaskGet_GA100();

    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = s_thermTestBjtEnMaskForGpcGet_GA100(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Restore LW_PCHIPLET_PWR_GPC_##idx##_RAWCODE_OVERRIDE.
            REG_WR32(BAR0, LW_PCHIPLET_PWR_GPC_RAWCODE_OVERRIDE(idxGpc),
                        pRegCacheGA100->regPwrRawCodeOverride[idxGpc]);

            // Select BJT in TSENSE_INDEX and restore related register arrays.
            s_thermTestGpcBjtIndexSet_GA100(idxGpc, idxBjt);
            REG_WR32(CSB, LW_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR,
                        pRegCacheGA100->regGpcTsenseLwrr[idxGpc][idxBjt]);

            // Select BJT in OVERRIDE_INDEX and restore related register arrays.
            s_thermTestGpcBjtOverrideIndexSet_GA100(idxGpc, idxBjt);
            REG_WR32(CSB, LW_THERM_GPC_TSENSE_OVERRIDE,
                        pRegCacheGA100->regGpcTsenseOverride[idxGpc][idxBjt]);

            // Select BJT in HS_OFFSET_INDEX and restore related register arrays.
            s_thermTestGpcBjtHsOffsetIndexSet_GA100(idxGpc, idxBjt);
            REG_WR32(CSB, LW_THERM_GPC_TSENSE_HS_OFFSET,
                        pRegCacheGA100->regGpcTsenseHsOffset[idxGpc][idxBjt]);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Restore Indices after restoring register values for all index.
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX,
                        pRegCacheGA100->regGpcTsenseIdx);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE_INDEX,
                        pRegCacheGA100->regGpcTsenseOverrideIdx);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET_INDEX,
                        pRegCacheGA100->regGpcTsenseHsOffsetIdx);

    // Wait for filter period to disable events raised.
    filterTimeUs = s_thermTestFilterIntervalTimeUsGet_GA100();
    OS_PTIMER_SPIN_WAIT_US(filterTimeUs);

    // In the end restore OVERT_EN so as not to trigger overt accidentally.
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, pRegCacheGA100->regOvertCtrl);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, pRegCacheGA100->regOvertEn);
}

/*!
 * @brief Interface to save/Restore Registers in regCache.
 *
 * @param[in] bSave Bool flag to Save/Restore regCache.
 *
 * @return
 * Success: FLCN_OK
 * Failure: FLCN_ERR_NO_FREE_MEM : Memory Allocation Failed
 *
 */
static FLCN_STATUS
s_thermTestRegCacheInitRestore_GA100
(
    LwBool bSave
)
{
    if (pRegCacheGA100 == NULL)
    {
        memset(&thermRegCacheGA100, 0, sizeof(THERM_TEST_REG_CACHE_GA100));
        pRegCacheGA100 = &thermRegCacheGA100;
    }

    if (bSave)
    {
        s_thermTestRegCacheInit_GA100();
    }
    else
    {
        s_thermTestRegCacheRestore_GA100();
    }

    return FLCN_OK;
}

/*!
 * @brief Interface to disable slowdown and disable interrupts.
 */
static void
s_thermTestDisableSlowdownInterrupt_GA100
(
    LwU32 regIntrEn,
    LwU32 regUseA
)
{
    LwU32 idx;

    // Disable interrupts.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        regIntrEn = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ENABLE,
                        _CTRL, idx, _NO, regIntrEn);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, regIntrEn);

    // Disable slowdown.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_THERMAL)
    {
        regUseA = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_A, _CTRL, idx,
                        _NO, regUseA);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, regUseA);
}

/*!
 * @brief Disable overt for all events.
 */
static void
s_thermTestDisableOvertForAllEvents_GA100
(
    LwU32 regOvertEn
)
{
    LwU8 idx;

    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_TRIGGER)
    {
        regOvertEn = FLD_IDX_SET_DRF(_CPWR_THERM, _OVERT_EN, _CTRL,
                        idx, _DISABLED, regOvertEn);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, regOvertEn);
}

/*!
 * @brief  Programs the GPC and BJT indices in the GPC_TSENSE_INDEX register.
 *         Read Increment will be disabled.
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcBjtIndexSet_GA100
(
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    LwU32 gpcTsenseIndex = 0;

    // Set GPC_BJT_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_INDEX,
                _GPC_BJT_INDEX, idxBjt, gpcTsenseIndex);

    // Set GPC_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_INDEX,
                _GPC_INDEX, idxGpc, gpcTsenseIndex);

    // Disable Read Increment.
    gpcTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INDEX, _READINCR,
                                _DISABLED, gpcTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX, gpcTsenseIndex);
}

/*!
 * @brief  Selects GPC TSENSE and GPC_TSENSE_OVERRIDE_INDEX.
 *         Read/Write increment will be disabled.
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcBjtOverrideIndexSet_GA100
(
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    LwU32 gpcTsenseIndex = 0;

    // Set GPC_BJT_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_OVERRIDE_INDEX,
                _GPC_BJT_INDEX, idxBjt, gpcTsenseIndex);

    // Set GPC_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_OVERRIDE_INDEX,
                _GPC_INDEX, idxGpc, gpcTsenseIndex);

    // Disable ReadInc.
    gpcTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_OVERRIDE_INDEX,
                _READINCR, _DISABLED, gpcTsenseIndex);

    // Disable WriteInc.
    gpcTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_OVERRIDE_INDEX,
                _WRITEINCR, _DISABLED, gpcTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE_INDEX, gpcTsenseIndex);
}

/*!
 * @brief  Selects GPC TSENSE and GPC_TSENSE_HS_OFFSET_INDEX.
 *         Read/Write increment will be disabled.
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcBjtHsOffsetIndexSet_GA100
(
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    LwU32 gpcTsenseIndex = 0;

    // Set GPC_BJT_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_HS_OFFSET_INDEX,
                _GPC_BJT_INDEX, idxBjt, gpcTsenseIndex);

    // Set GPC_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_HS_OFFSET_INDEX,
                _GPC_INDEX, idxGpc, gpcTsenseIndex);

    // Disable ReadInc.
    gpcTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_HS_OFFSET_INDEX,
                _READINCR, _DISABLED, gpcTsenseIndex);

    // Disable WriteInc.
    gpcTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_HS_OFFSET_INDEX,
                _WRITEINCR, _DISABLED, gpcTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET_INDEX, gpcTsenseIndex);
}

/*!
 * @brief  Gets the value of slope and offset for the indexed GPC TSENSE sensor.
 *
 * @param[out]   pSlopeA     Value of the slope in normalized form(F0.16).
 * @param[out]   pOffsetB    Value of the offset form(F16.16).
 */
static void
s_thermTestGpcTsenseTempParamsGet_GA100
(
    LwUFXP16_16 *pSlopeA,
    LwUFXP16_16 *pOffsetB
)
{
    LwU32 regSlopeA;
    LwU32 regOffsetB;
    LwU32 regSensor1;
    LwU32 regSensor2;
    LwU32 regSensor3;

    *pOffsetB = LW_TYPES_FXP_ZERO;
    *pSlopeA  = LW_TYPES_FXP_ZERO;

    regSensor1 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_1);
    regSensor2 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_2);
    regSensor3 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_3);

    //
    // Retrieve SlopeA (F0.14) from Sensor1 if SW_ON otherwise from SENSOR3.
    // It is a ten bit positive integer. This field should actually contain
    // "slope << 14", since "slope"  is a fraction. i.e. SlopeA = slope * 2^14.
    // For example, if the intended slope is 0.05, you would program
    // 0.05 * 2^14 = 819, or 0x333.
    //
    if (FLD_TEST_DRF(_CPWR_THERM, _SENSOR_1, _SELECT_SW_A, _ON, regSensor1))
    {
        regSlopeA = DRF_VAL(_CPWR_THERM, _SENSOR_2, _SW_A, regSensor2);
    }
    else
    {
        regSlopeA = DRF_VAL(_CPWR_THERM, _SENSOR_3, _HW_A, regSensor3);
    }

    //
    // B (on GF11x: 11-bit positive fixed point number with one decimal place,
    // and formula has changed to T = A * RAW - B).
    // Use Absolute value here, so, temp = A * R - B.
    //
    // It is an eleven bit positive fixed point number with one decimal place.
    // The actual intercept is a negative number, so we use its absolute value.
    // For example, if the intended intercept is -700, you would program
    // 700 * 2 = 1400 or 0x578.
    //
    if (FLD_TEST_DRF(_CPWR_THERM, _SENSOR_1, _SELECT_SW_B, _ON, regSensor1))
    {
        regOffsetB = DRF_VAL(_CPWR_THERM, _SENSOR_2, _SW_B, regSensor2);
    }
    else
    {
        regOffsetB = DRF_VAL(_CPWR_THERM, _SENSOR_3, _HW_B, regSensor3);
    }

    // Normalize to F0.16
    *pSlopeA = regSlopeA << (16-14);
    *pOffsetB = regOffsetB << (16-1);
}

/*!
 * @brief  Gets the value of the raw code from the slope,
 *         offset and temperature.
 *
 * @param[in]   slopeA     Value of the slope in normalized form(F0.16).
 * @param[in]   offsetB    Value of the offset.
 * @param[in]   temp       temperature value.
 *
 * @return      Value of the callwlated raw code.
 */
static LwU16
s_thermTestRawCodeForTempGet_GA100
(
    LwUFXP16_16 slopeA,
    LwUFXP16_16 offsetB,
    LwTemp      temp
)
{
    LwUFXP16_16 tmp;

    //
    // All math is done in F16.16 format. Since we are using normalized A value
    // temperature in [C] is callwlated as: Temp = slope_A * rawCode - offset_B.
    // We are using ilwerse formula rawCode = ((Temp + offset_B) / slope_A).
    // corrected by adding (A-1) to compensate for integer arithmetics error,
    // that will happen in HW.
    //
    tmp = ((LwUFXP16_16)(temp << 8)) + offsetB;

    // 16.16 / 16.16, so result is integer.
    return LW_DIV_AND_CEIL(tmp, slopeA);
}

#if 0
/*!
 * @brief  Set the raw code override in GPCPWR.
 *         In GA100, Rawcode can be overridden per GPC only.
 *
 * @param[in]   rawCode                 The raw code override to be set.
 * @param[in]   idxGpc                  GPC index.
 */
static void
s_thermTestGpcRawCodeSet_GA100
(
    LwU32   regPwrRawCodeOverride,
    LwU32   rawCode,
    LwU8    idxGpc
)
{
    LwU32 regPwrGpcTsense;

    // Fake the temperature by setting the raw override.
    regPwrRawCodeOverride = FLD_SET_DRF_NUM(_PCHIPLET_PWR,
                                        _GPC_RAWCODE_OVERRIDE, _VALUE,
                                        rawCode, regPwrRawCodeOverride);
    regPwrRawCodeOverride = FLD_SET_DRF(_PCHIPLET_PWR,
                                        _GPC_RAWCODE_OVERRIDE, _EN,
                                        _YES, regPwrRawCodeOverride);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPC_RAWCODE_OVERRIDE(idxGpc),
                regPwrRawCodeOverride);

    // Set STREAM_RAW_CODE_ENABLED and set CONFIG_SEND to transferred to Wrapper.
    regPwrGpcTsense = REG_RD32(BAR0, LW_PCHIPLET_PWR_GPC_TSENSE(idxGpc));
    regPwrGpcTsense = FLD_SET_DRF(_PCHIPLET_PWR, _GPC_TSENSE, _STREAM_RAW_CODE,
                _ENABLED, regPwrGpcTsense);
    regPwrGpcTsense = FLD_SET_DRF(_PCHIPLET_PWR, _GPC_TSENSE, _CONFIG, _SEND,
                regPwrGpcTsense);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPC_TSENSE(idxGpc), regPwrGpcTsense);
}
#endif

/*!
 * @brief  Broadcast the raw code override in GPCPWR.
 *         In GA100, Rawcode can be overridden per GPC only.
 *
 * @param[in]   regRawCodeOverride      The raw code override register value.
 * @param[in]   rawCode                 The raw code override to be set.
 */
static void
s_thermTestGpcRawCodeBroadcast_GA100
(
    LwU32   regPwrRawCodeOverride,
    LwU32   rawCode
)
{
    LwU32 regPwrGpcTsense;

    // Fake the temperature by setting the raw override.
    regPwrRawCodeOverride = FLD_SET_DRF_NUM(_PCHIPLET_PWR,
                                        _GPCS_RAWCODE_OVERRIDE, _VALUE,
                                        rawCode, regPwrRawCodeOverride);
    regPwrRawCodeOverride = FLD_SET_DRF(_PCHIPLET_PWR,
                                        _GPCS_RAWCODE_OVERRIDE, _EN,
                                        _YES, regPwrRawCodeOverride);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPCS_RAWCODE_OVERRIDE, regPwrRawCodeOverride);

    // Set STREAM_RAW_CODE_ENABLED and set CONFIG_SEND to transferred to Wrapper.
    regPwrGpcTsense = REG_RD32(BAR0, LW_PCHIPLET_PWR_GPCS_TSENSE);
    regPwrGpcTsense = FLD_SET_DRF(_PCHIPLET_PWR, _GPCS_TSENSE, _STREAM_RAW_CODE,
                _ENABLED, regPwrGpcTsense);
    regPwrGpcTsense = FLD_SET_DRF(_PCHIPLET_PWR, _GPCS_TSENSE, _CONFIG, _SEND,
                regPwrGpcTsense);
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPCS_TSENSE, regPwrGpcTsense);
}

/*!
 * @brief  Gets the value of GPC TSENSE temperature.
 *
 * @param[in]   bHsTemp       Bool to indicate temperature value with hotspot.
 * @param[in]   ptsenseIdx    Retrieve GPC TSENSE Index for which temp is returned.
 *
 * @return      Temperature value.
 */
static LwTemp
s_thermTestGpcTsenseTempGet_GA100
(
    LwBool bHsTemp
)
{
    LwTemp  tsenseTemp;

    if (bHsTemp)
    {
        // Get the GPC TSENSE + HS temperature.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR, _FIXED_POINT,
                tsenseTemp);
        }
    }
    else
    {
        // Get the actual GPC TSENSE temperature value.
        tsenseTemp = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE_LWRR,
                _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE_LWRR, _FIXED_POINT,
                tsenseTemp);
        }
    }

    return tsenseTemp;
}

/*!
 * @brief  Get mask of GPCs that are enabled. 1: enabled, 0: disabled.
 *
 * @return      Enabled GPC-s mask.
 */
static LwU32
s_thermTestGpcEnMaskGet_GA100(void)
{
    LwU32 gpcMask;
    LwU32 maxGpcMask;

    //
    // GPC MASK Fuse value interpretation - for each bit: 1: disable 0: enable.
    // Colwert it into a normal bitmask with - 1: enabled, 0: disabled.
    //
    gpcMask = fuseRead(LW_FUSE_STATUS_OPT_GPC);
    maxGpcMask = ((1 << LW_GPC_MAX) - 1);

    gpcMask = (~gpcMask & maxGpcMask);

    return gpcMask;
}

/*!
 * @brief  Get mask of BJTs that are enabled for GPC. 1: enabled, 0: disabled.
 *
 * @param[in]   idxGpc  GPC TSENSE index.
 *
 * @return      Enabled BJT-s mask. Returns BJT mask for all GPCs for idxGpc LW_U8_MAX.
 */
static LwU32
s_thermTestBjtEnMaskForGpcGet_GA100
(
    LwU8 idxGpc
)
{
    LwU32 bjtMask;
    LwU32 maxBjtMask;

    //
    // BJT Mask Fuse value interpretation -
    // b0: GPC0BJT0, b1: GPC0BJT1, b2: GPC0BJT2;
    // b3: GPC1BJT0, b4: GPC1BJT1, b5: GPC0BJT2; similarly it continues.
    //
    bjtMask = DRF_VAL(_FUSE_OPT, _TS_DIS_MASK, _DATA, fuseRead(LW_FUSE_OPT_TS_DIS_MASK));
    maxBjtMask = ((1 << LW_BJT_MAX) - 1);

    return ((idxGpc == LW_U8_MAX) ? (~bjtMask) : (~(bjtMask >> (idxGpc * LW_BJT_MAX)) & maxBjtMask));
}

/*!
 * @brief   Set GPC TSENSE temperature for GPC/BJT selected.
 *
 * @param[in]   regGpcTsenseOverride  The original value of TSENSE_OVERRIDE.
 * @param[in]   gpuTemp               The temperature to fake in 24.8.
 */
static void
s_thermTestTempSet_GA100
(
    LwU32   regTempOverride,
    LwTemp  gpuTemp
)
{
    regTempOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_OVERRIDE,
        _TEMP_VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp), regTempOverride);
    regTempOverride = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_OVERRIDE,
        _TEMP_SELECT, _OVERRIDE, regTempOverride);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE, regTempOverride);
}

/*!
 * @brief   Set TSENSE_HS_OFFSET temperature for GPC/BJT selected.
 *
 * @param[in]   regGpcTsenseOverride  The original value of HS_OFFSET_OVERRIDE.
 * @param[in]   hsOffset               The temperature to fake in 24.8.
 */
static void
s_thermTestHotspotOffsetSet_GA100
(
    LwU32   regHsOffset,
    LwTemp  hsOffset
)
{
    regHsOffset = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_HS_OFFSET,
        _FIXED_POINT, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(hsOffset), regHsOffset);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET, regHsOffset);
}

/*!
 * @brief  Sets the value and source for DEDICATED_OVERT.
 *
 * @param[in]   regOvert     Register value of DEDICATED_OVERT.
 * @param[in]   dedOvertTemp Value of temperature to set.
 * @param[in]   provIdx      Provider index.
 */
static void
s_thermTestDedicatedOvertSet_GA100
(
    LwU32  regOvert,
    LwTemp dedOvertTemp,
    LwU8   provIdx
)
{
    switch (provIdx)
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE:
        {
            regOvert = FLD_SET_DRF(_CPWR_THERM, _EVT_DEDICATED_OVERT,
                       _TEMP_SENSOR_ID, _TSENSE, regOvert);
            break;
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG:
        {
            regOvert = FLD_SET_DRF(_CPWR_THERM, _EVT_DEDICATED_OVERT,
                        _TEMP_SENSOR_ID, _GPU_AVG, regOvert);
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

    regOvert = FLD_SET_DRF(_CPWR_THERM, _EVT_DEDICATED_OVERT, _FILTER_PERIOD,
                           _NONE, regOvert);
    regOvert = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_DEDICATED_OVERT, _THRESHOLD,
                           RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(dedOvertTemp), regOvert);

    REG_WR32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT, regOvert);
}

/*!
 * @brief  Sets global override temperature for GPC TSENSEs.
 *
 * @param[in]   regGlobalOverride Register value for global override.
 * @param[in]   overrideTemp      Global override temperature to be set in FXP24.8.
 */
static void
s_thermTestGlobalOverrideSet_GA100
(
    LwU32  regGlobalOverride,
    LwTemp overrideTemp
)
{
    regGlobalOverride = FLD_SET_DRF_NUM(_CPWR_THERM,
                            _GPC_TSENSE_GLOBAL_OVERRIDE,
                            _VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(overrideTemp),
                            regGlobalOverride);

    regGlobalOverride = FLD_SET_DRF(_CPWR_THERM,
                            _GPC_TSENSE_GLOBAL_OVERRIDE,
                            _EN, _ENABLE,
                            regGlobalOverride);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_GLOBAL_OVERRIDE, regGlobalOverride);
}

/*!
 * @brief  Fake output of SYS TSENSE.
 *
 * @param[in]   regSensor0 Register value for SENSOR_0.
 * @param[in]   gpuTemp    Temperature to be faked in FXP24.8.
 */
static void
s_thermTestSysTsenseTempSet_GA100
(
    LwU32  regSensor0,
    LwTemp gpuTemp
)
{
    regSensor0 = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_0, _TEMP_OVERRIDE,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp), regSensor0);
    regSensor0 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_0, _TEMP_SELECT,
                            _OVERRIDE, regSensor0);

    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_0, regSensor0);
}

/*!
 * @brief  Set SensorId and Temp Threshold for EVT_THERMAL_1.
 *
 * @param[in]   regVal      Initial Register value.
 * @param[in]   threshold   Threshold to trigger event.
 * @param[in]   sensorId    sensorId for the event.
 */
static void
s_thermTestParamSetThermalEvent1_GA100
(
    LwU32   regVal,
    LwTemp  threshold,
    LwU8    sensorId
)
{
    regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_THERMAL_1,
                _TEMP_THRESHOLD, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(threshold), regVal);

    regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_THERMAL_1,
                _TEMP_SENSOR_ID, sensorId, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_EVT_THERMAL_1, regVal);
}

/*!
 * @brief  Triggers Global Snapshot.
 */
static void
s_thermTestGlobalSnapshotTrigger_GA100(void)
{
    LwU32 regVal = 0;

    regVal = FLD_SET_DRF(_CPWR_THERM, _TSENSE_GLOBAL_SNAPSHOT,
                _CMD, _TRIGGER, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_TSENSE_GLOBAL_SNAPSHOT, regVal);
}

/*!
 * @brief  Get Filter Period Time Interval to wait in usec.
 *
 * @return      Get filter time based on scale.
 */
static LwU32
s_thermTestFilterIntervalTimeUsGet_GA100(void)
{
    LwU32   regValPower6;
    LwU8    filterPeriod;
    LwU8    scaleFactor;

    regValPower6 = REG_RD32(CSB, LW_CPWR_THERM_POWER_6);

    filterPeriod = DRF_VAL(_CPWR_THERM, _POWER_6,
                _THERMAL_FILTER_PERIOD, regValPower6);

    scaleFactor = DRF_VAL(_CPWR_THERM, _POWER_6,
                _THERMAL_FILTER_SCALE, regValPower6);

    return (filterPeriod * (1 << ((scaleFactor + 1) << 2)));
}
