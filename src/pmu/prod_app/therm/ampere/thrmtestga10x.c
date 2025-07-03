/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtestga10x.c
 * @brief   PMU HAL functions related to therm tests for GA10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"

#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define THERM_TEST_OFFSET_TEMP              RM_PMU_CELSIUS_TO_LW_TEMP(20)

// Invalid test temperature.
#define THERM_TEST_TEMP_ILWALID                 (LW_S32_MAX)

// To match with HW, SW should ignore lowest three bits of callwlated result.
#define THERM_TEST_HW_UNSUPPORTED_BIT_MASK      (0xFFFFFFF8)

#define THERM_TEST_HS_TEMP                      RM_PMU_CELSIUS_TO_LW_TEMP(10)
#define THERM_TEST_HS_TEMP_DELTA                RM_PMU_CELSIUS_TO_LW_TEMP(5)
#define THERM_TEST_DEDICATED_OVERT_TEMP         RM_PMU_CELSIUS_TO_LW_TEMP(70)
#define THERM_TEST_TSENSE_TEMP                  RM_PMU_CELSIUS_TO_LW_TEMP(75)
#define THERM_TEST_THERM_MON_TSENSE_TEMP        RM_PMU_CELSIUS_TO_LW_TEMP(75)
#define THERM_TEST_THERM_MON_TSENSE_TEMP_DELTA  RM_PMU_CELSIUS_TO_LW_TEMP(5)
#define THERM_TEST_HS_TEMP_SNAPSHOT             RM_PMU_CELSIUS_TO_LW_TEMP(1)
#define THERM_TEST_TEMP_CELSIUS_20              (20)
#define THERM_TEST_TEMP_CELSIUS_30              (30)
#define THERM_TEST_TEMP_CELSIUS_40              (40)
#define THERM_TEST_TEMP_CELSIUS_45              (45)
#define THERM_TEST_TEMP_CELSIUS_50              (50)

#define LW_GPC_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_INDEX_MAX + 1)
#define LW_BJT_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_BJT_INDEX_MAX + 1)
#define LW_LWL_MAX          (LW_THERM_LWL_TSENSE_INDEX_INDEX_MAX + 1)

// Rawcode Jump Factor
#define LW_THERM_GPC_TSENSE_RAWCODE_JUMP_FACTOR (0x1000)
#define LW_THERM_TEMP_SNAPSHOT_STRIDE           (2)

// 1 Microsecond delay for use in thermal monitors test
#define THERM_TEST_DELAY_1_MICROSECOND          (1)

/* ------------------------- Type Definitions ------------------------------- */
typedef LwU32 LW_THERMAL_TEST_STATUS;

/*
 * This structure is used to cache all HW values for registers that each
 * test uses and restore them back after each test/subtest completes. Inline
 * comments for each variable to register mapping.
 */
typedef struct
{
    LwU32   regIntrEn;                                  // @see LW_CPWR_THERM_EVENT_INTR_ENABLE
    LwU32   regUseA;                                    // @see LW_CPWR_THERM_USE_A
    LwU32   regOvertEn;                                 // @see LW_CPWR_THERM_OVERT_EN
    LwU32   regDedOvert;                                // @see LW_CPWR_THERM_EVT_DEDICATED_OVERT
    LwU32   regSensor6;                                 // @see LW_CPWR_THERM_SENSOR_6
    LwU32   regSensor7;                                 // @see LW_CPWR_THERM_SENSOR_7
    LwU32   regOvertCtrl;                               // @see LW_CPWR_THERM_OVERT_CTRL
    LwU32   regGlobalOverride;                          // @see LW_CPWR_THERM_TSENSE_GLOBAL_OVERRIDE
    LwU32   regEvtThermal1;                             // @see LW_CPWR_THERM_EVT_THERMAL_1
    LwU32   regPower6;                                  // @see LW_CPWR_THERM_POWER_6
    LwU32   regInterpolationCtrl;                       // @see LW_CPWR_THERM_GPC_TSENSE_INTERPOLATION_CTRL
    LwU32   regLwlRawCodeOverride;                      // @see LW_CPWR_THERM_LWL_RAWCODE_OVERRIDE
    LwU32   regSysTsenseOverride;                       // @see LW_CPWR_THERM_SYS_TSENSE_OVERRIDE
    LwU32   regSysTsenseTempCode;                       // @see LW_CPWR_THERM_SENSOR_0
    LwU32   regSysTsenseHsOffset;                       // @see LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET

    // Index Registers
    LwU32   regGpcTsenseIdx;                            // @see LW_CPWR_THERM_GPC_TSENSE_INDEX
    LwU32   regGpcTsenseOverrideIdx;                    // @see LW_CPWR_THERM_GPC_TSENSE_OVERRIDE_INDEX
    LwU32   regGpcTsenseHsOffsetIdx;                    // @see LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET_INDEX
    LwU32   regLwlTsenseOverrideIdx;                    // @see LW_CPWR_THERM_LWL_TSENSE_OVERRIDE_INDEX

    // Per GPC and BJT registers
    LwU32   regGpcTsense[LW_GPC_MAX][LW_BJT_MAX];                   // @see LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE
    LwU32   regGpcTsenseRawCode[LW_GPC_MAX][LW_BJT_MAX];            // @see LW_CPWR_THERM_GPC_TSENSE_RAW_CODE
    LwU32   regGpcTsenseOverride[LW_GPC_MAX][LW_BJT_MAX];           // @see LW_CPWR_THERM_GPC_TSENSE_OVERRIDE
    LwU32   regGpcTsenseHsOffset[LW_GPC_MAX][LW_BJT_MAX];           // @see LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET

    // Per GPC / LWL Tsense registers
    LwU32   regGpcRawCodeOverride[LW_GPC_MAX];                      // @see LW_CPWR_THERM_GPC_RAWCODE_OVERRIDE
    LwU32   regLwlTsenseOverride[LW_LWL_MAX];                       // @see LW_CPWR_THERM_LWL_TSENSE_OVERRIDE
    LwU32   regLwlTsenseRawCode[LW_LWL_MAX];                        // @see LW_CPWR_THERM_LWL_TSENSE_RAW_CODE
    LwU32   regLwlTsenseHsOffset[LW_LWL_MAX];                       // @see LW_CPWR_THERM_LWL_TSENSE_HS_OFFSET

    LwU32   regMonCtrl[LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1];    // @see LW_CPWR_THERM_INTR_MONITOR_CTRL
} THERM_TEST_REG_CACHE_GA10X;

/* ------------------------- Global Variables ------------------------------- */
THERM_TEST_REG_CACHE_GA10X  thermRegCacheGA10X
    GCC_ATTRIB_SECTION("dmem_libThermTest", "thermRegCacheGA10X");
THERM_TEST_REG_CACHE_GA10X *pRegCacheGA10X = NULL;

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_thermTestTempColwersion_GA10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTempColwersion_GA10X");

static FLCN_STATUS s_thermTestMaxAvgExelwte_GA10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestMaxAvgExelwte_GA10X");

static FLCN_STATUS s_thermTestGlobalSnapshotGpuCombined_GA10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotGpuCombined_GA10X");

static FLCN_STATUS s_thermTestGlobalSnapshotGpuSites_GA10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotGpuSites_GA10X");

static FLCN_STATUS s_thermTestGlobalSnapshotHwPipeline_GA10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotHwPipeline_GA10X");

static void s_thermTestRegCacheInit_GA10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheInit_GA10X");

static void s_thermTestRegCacheRestore_GA10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheRestore_GA10X");

static FLCN_STATUS s_thermTestRegCacheInitRestore_GA10X(LwBool)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheInitRestore_GA10X");

static void s_thermTestDisableSlowdownInterrupt_GA10X(LwU32 regIntrEn, LwU32 regUseA)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDisableSlowdownInterrupt_GA10X");

static void s_thermTestDisableOvertForAllEvents_GA10X(LwU32 regOvertEn)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDisableOvertForAllEvents_GA10X");

static void s_thermTestDedicatedOvertSet_GA10X(LwU32 regOvert, LwTemp dedOvertTemp, LwU8 provIdx)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDedicatedOvertSet_GA10X");

static void s_thermTestGpcIndexSet_GA10X(LwU8 idxGpc)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcIndexSet_GA10X");

static void s_thermTestGpcTsenseIndexSet_GA10X(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseIndexSet_GA10X");

static void s_thermTestGpcTsenseSnapshotIndexSet_GA10X(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseSnapshotIndexSet_GA10X");

static void s_thermTestGpcTsenseOverrideIndexSet_GA10X(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseOverrideIndexSet_GA10X");

static void s_thermTestGpcTsenseHsOffsetIndexSet_GA10X(LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseHsOffsetIndexSet_GA10X");

static void s_thermTestLwlTsenseHsOffsetIndexSet_GA10X(LwU8 idxLwl)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseHsOffsetIndexSet_GA10X");

static void s_thermTestLwlTsenseIndexSet_GA10X(LwU8 idxLwl)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseIndexSet_GA10X");

static void s_thermTestLwlTsenseSnapshotIndexSet_GA10X(LwU8 idxLwl)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseSnapshotIndexSet_GA10X");

static void s_thermTestLwlTsenseOverrideIndexSet_GA10X(LwU8 idxLwl)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseOverrideIndexSet_GA10X");

static void s_thermTestGpcTsenseTempParamsGet_GA10X(LwUFXP16_16 *pSlopeA, LwUFXP16_16 *pOffsetB)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseTempParamsGet_GA10X");

static LwU16 s_thermTestRawCodeForTempGet_GA10X(LwUFXP16_16 slopeA, LwUFXP16_16 offsetB, LwTemp temp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRawCodeForTempGet_GA10X");

static LwTemp s_thermTestGpcTsenseTempGet_GA10X(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseTempGet_GA10X");

static LwTemp s_thermTestGpcTsenseSnapshotTempGet_GA10X(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseSnapshotTempGet_GA10X");

static LwTemp s_thermTestLwlTsenseTempGet_GA10X(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseTempGet_GA10X");

static LwTemp s_thermTestLwlTsenseSnapshotTempGet_GA10X(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseSnapshotTempGet_GA10X");

static LwTemp s_thermTestSysTsenseSnapshotTempGet_GA10X(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSysTsenseSnapshotTempGet_GA10X");

static LwU32 s_thermTestGpcEnMaskGet_GA10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcEnMaskGet_GA10X_GA10X");

static LwU32 s_thermTestLwlEnMaskGet_GA10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlEnMaskGet_GA10X");

static void s_thermTestGpcTsenseTempSet_GA10X(LwU32 regTempOverride, LwTemp temp, LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseTempSet_GA10X");

static void s_thermTestGpcTsenseHsOffsetSet_GA10X(LwU32 regHsOffset, LwTemp temp, LwU8 idxGpc, LwU8 idxBjt)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcTsenseHsOffsetSet_GA10X");

static void s_thermTestLwlTsenseHsOffsetSet_GA10X(LwU32 regHsOffset, LwTemp temp, LwU8 idxLwl)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseHsOffsetSet_GA10X");

static void s_thermTestSysTsenseHsOffsetSet_GA10X(LwU32 regHsOffset, LwTemp temp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSysTsenseHsOffsetSet_GA10X");

static void s_thermTestGlobalOverrideSet_GA10X(LwU32 regGlobalOverride, LwTemp overrideTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalOverrideSet_GA10X");

static void s_thermTestSysTsenseTempSet_GA10X(LwU32 regSysTsenseOverride, LwTemp gpuTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSysTsenseTempSet_GA10X");

static void s_thermTestThermalEvent1ParamSet_GA10X(LwU32 regVal, LwTemp threshold, LwU8 sensorId)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestThermalEvent1ParamSet_GA10X");

static void s_thermTestGlobalSnapshotTrigger_GA10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotTrigger_GA10X");

static LwU32 s_thermTestFilterIntervalTimeUsGet_GA10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestFilterIntervalTimeUsGet_GA10X");

static void s_thermTestGpcRawCodeSet_GA10X(LwU32 regGpcRawCodeOverride, LwU32 rawCode, LwU8 idxGpc)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGpcRawCodeSet_GA10X");

static void s_thermTestThermalInterpolationDisable_GA10X(LwU32 regVal)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestThermalInterpolationDisable_GA10X");

static void s_thermTestLwlTsenseTempSet_GA10X(LwU32 regTempOverride, LwTemp temp, LwU8 idxLwl)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlTsenseTempSet_GA10X");

static void s_thermTestLwlRawCodeSet_GA10X(LwU32 regLwlRawCodeOverride, LwU32 rawCode)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestLwlRawCodeSet_GA10X");

static void s_thermTestSysTsenseRawCodeSet_GA10X(LwU32 sensor7, LwU32 rawCode)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSysTsenseRawCodeSet_GA10X");

static FLCN_STATUS s_thermTestTempColwersiolwerifyGpcTsense_GA10X(LwU32 rawCode, LwTemp rawCodeTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTempColwersiolwerifyGpcTsense_GA10X");

static FLCN_STATUS s_thermTestTempColwersiolwerifySysTsense_GA10X(LwU32 rawCode, LwTemp rawCodeTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTempColwersiolwerifySysTsense_GA10X");

static FLCN_STATUS s_thermTestTempColwersiolwerifyLwlTsense_GA10X(LwU32 rawCode, LwTemp rawCodeTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTempColwersiolwerifyLwlTsense_GA10X");

static void s_thermTestGlobalSnapshotGpuSitesFakeTemp_GA10X(LwS32 initFakeTemp, LwTemp hsOffset, LwS32 tempStride)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotGpuSitesFakeTemp_GA10X");

static LW_THERMAL_TEST_STATUS s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X(LwS32 initFakeTemp, LwTemp hsOffset, LwS32 tempStride)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Test the _OFFSET_TSENSE and _SENSOR_MAX registers
 *
 * @param[out]   pParam       Test parameters, indicating status/output data
 *                 outStatus  Indicates status of the test
 *                 outData    Indicates failure reason of the test
*/
void
thermTestOffsetMaxTempExelwte_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32  origOffset;
    LwU32  origMax;
    LwU32  reg32;
    LwTemp tsenseOffsetTemp;
    LwTemp tsenseTemp;
    LwTemp maxTemp;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Step1: Cache current HW state
    origOffset  = REG_RD32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET);
    origMax     = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_9);

    //
    // Step 2: Configure OFFSET value for _OFFSET_TSENSE using _SENSOR_8
    //
    // Note: HW uses 9.5 fixed point notation. Colwert LwTemp to 9.5 when
    // writing to HW
    //
    reg32 = origOffset;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _SYS_TSENSE_HS_OFFSET, _FIXED_POINT,
        RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_OFFSET_TEMP), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET, reg32);

    // Step 3: Configure SENSOR_MAX to point to _TSENSE and _OFFSET_TSENSE
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _SYS_TSENSE, _YES) |
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _OFFSET_SYS_TSENSE, _YES));

    // Step 4: Check if OFFSET_TSENSE reflects correct value
    tsenseTemp = thermGetTempInternal_HAL(&Therm, 
              LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    tsenseOffsetTemp = thermGetTempInternal_HAL(&Therm, 
              LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE_OFFSET);

    if (tsenseOffsetTemp != (tsenseTemp + THERM_TEST_OFFSET_TEMP))
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                               TSENSE_OFFSET_VALUE_FAILURE);
        goto thermTestOffsetMaxTempExelwte_GA10X_exit;
    }

    // Step 5: Check if _SENSOR_MAX reflects correct value
    maxTemp = thermGetTempInternal_HAL(&Therm,
                  LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX);

    if (maxTemp != tsenseOffsetTemp)
    {
        pParams->outData =
            LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, MAX_VALUE_FAILURE);
        goto thermTestOffsetMaxTempExelwte_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestOffsetMaxTempExelwte_GA10X_exit:

    // Restore original values
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9, origMax);
    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET, origOffset);
}

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
thermTestIntSensors_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData   = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SUCCESS);

    // Test GPU_MAX, GPU_AVG and MAX.
    status = s_thermTestMaxAvgExelwte_GA10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestIntSensors_GA10X_exit;
    }

    // Test temperature colwersion Logic for the HW.
    status = s_thermTestTempColwersion_GA10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestIntSensors_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestIntSensors_GA10X_exit:
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
thermTestDedicatedOvert_GA10X
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
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    //
    // Turn on TSENSE and set polling interval as 0, so that we don't
    // have to wait every x ms for the new raw value to be reflected in
    // TSENSE. Wait for _TS_STABLE_CNT period after sensor is powered
    // on for a valid value.
    //
    reg32 = pRegCacheGA10X->regSensor6;
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

    // Fake SYS TSENSE temperature by setting the raw override.
    s_thermTestSysTsenseRawCodeSet_GA10X(pRegCacheGA10X->regSensor7, rawCode);

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
        goto thermTestDedicatedOvert_GA10X_exit;
    }

    // Enable overt temperature on boot.
    reg32 = pRegCacheGA10X->regOvertCtrl;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _OTOB_ENABLE, _ON, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, reg32);

    // Set the dedicated overt threshold to lesser than set above.
    s_thermTestDedicatedOvertSet_GA10X(pRegCacheGA10X->regDedOvert,
                THERM_TEST_DEDICATED_OVERT_TEMP,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    //
    // On Emulator, GPU will not powerdown after overt trigger as lwvdd clamp
    // wont be asserted. So, GPU will NOT go off the bus but OVERT will be
    // triggerred. On Production / engineering board, GPU will go offline.
    //
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES, reg32))
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
        goto thermTestDedicatedOvert_GA10X_exit;
    }

    //
    // We should not reach here, GPU is already off the bus on Production board.
    // Or
    // INT_OVERT_ASSERTED triggered on Emulator.
    //
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData =  LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT,
                SHUTDOWN_FAILURE);

thermTestDedicatedOvert_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
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
thermTestDedicatedOvertNegativeCheck_GA10X
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
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    // Override TSENSE using SW override.
    s_thermTestSysTsenseTempSet_GA10X(pRegCacheGA10X->regSysTsenseOverride,
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
        goto thermTestDedicatedOvertNegativeCheck_GA10X_exit;
    }

    // Enable overt temperature on boot.
    reg32 = pRegCacheGA10X->regOvertCtrl;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _OTOB_ENABLE, _ON, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, reg32);

    s_thermTestDedicatedOvertSet_GA10X(pRegCacheGA10X->regDedOvert,
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
        goto thermTestDedicatedOvertNegativeCheck_GA10X_exit;
    }

    //
    // First use global override to fake all GPC TSENSEs since local override
    // has higher priority than global override.
    //
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
                (THERM_TEST_TSENSE_TEMP + THERM_TEST_HS_TEMP));

    //
    // Set the dedicated overt temp to a value which will be less than
    // OVERRIDE/GLOBAL_OVERRIDE temperature. This is to verify GPU does not
    // shutdown if temperature is faked via
    // TSENSE_OVERRIDE/TSENSE_GLOBAL_OVERRIDE. If GPU shuts down, test has
    // failed. AVG will consider values for all enabled & valid TSENSE readings.
    //
    s_thermTestDedicatedOvertSet_GA10X(pRegCacheGA10X->regDedOvert,
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
        goto thermTestDedicatedOvertNegativeCheck_GA10X_exit;
    }

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    //
    // Loop over all enabled GPC TSENSEs to fake the temperature values.
    // Use same values for each GPC TSENSE as we don't need different values.
    //
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Fake Lwrr Temp for GPC TSENSE.
            s_thermTestGpcTsenseTempSet_GA10X(
                pRegCacheGA10X->regGpcTsenseOverride[idxGpc][idxBjt],
                THERM_TEST_TSENSE_TEMP, idxGpc, idxBjt);

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
                goto thermTestDedicatedOvertNegativeCheck_GA10X_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestDedicatedOvertNegativeCheck_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
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
thermTestGlobalSnapshot_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData   = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, SUCCESS);

    // Test HW Global Snapshot values for individual Sensors.
    status = s_thermTestGlobalSnapshotGpuSites_GA10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestGlobalSnapshot_GA10X_exit;
    }

    // Test HW Global Snapshot for GPU combined temp.
    status = s_thermTestGlobalSnapshotGpuCombined_GA10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestGlobalSnapshot_GA10X_exit;
    }

    // Test HW Pipeline functionality over nonSnapshot values.
    status = s_thermTestGlobalSnapshotHwPipeline_GA10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestGlobalSnapshot_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestGlobalSnapshot_GA10X_exit:
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
thermTestTempOverride_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwUFXP16_16 slopeA  = LW_TYPES_FXP_ZERO;
    LwUFXP16_16 offsetB = LW_TYPES_FXP_ZERO;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwU32       lwlEnMask;
    LwU32       rawCode;
    LwTemp      tsenseTemp;
    LwTemp      gpcAvgTemp;
    LwTemp      gpuMaxTemp;
    LwU8        idxGpc;
    LwU8        idxBjt;
    LwU8        idxLwl;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    //
    // Fake temperature for GPC TSENSEs, SYS TSENSE and LWL TSENSEs
    // Individually. Then Gobal Override all the Sensors.
    // Priority for Individual override > Global Override.
    //

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    // Loop each enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Fake temperature for GPC TSENSE.
            s_thermTestGpcTsenseTempSet_GA10X(
                pRegCacheGA10X->regGpcTsenseOverride[idxGpc][idxBjt],
                THERM_TEST_TSENSE_TEMP, idxGpc, idxBjt);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Fake SYS Tsense temperature.
    s_thermTestSysTsenseTempSet_GA10X(pRegCacheGA10X->regSysTsenseOverride,
                THERM_TEST_TSENSE_TEMP + THERM_TEST_HS_TEMP_DELTA);

    // Get LWL TSENSE Enabled Mask.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();

    // Loop each enabled LWL TSENSEs.
    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        // Fake temperature for LWL TSENSE.
        s_thermTestLwlTsenseTempSet_GA10X(
                pRegCacheGA10X->regLwlTsenseOverride[idxLwl],
                THERM_TEST_TSENSE_TEMP + 2 * THERM_TEST_HS_TEMP_DELTA, idxLwl);
    }
    FOR_EACH_INDEX_IN_MASK_END

    //
    // Use global override to fake all TSENSE sensors to a value greater than
    // the value used in faking individual TSENSE sensors.
    //
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
                (THERM_TEST_TSENSE_TEMP + 3 * THERM_TEST_HS_TEMP_DELTA));

    //
    // As all GPC Tsense-s are faked to same value using local/global override,
    // MAX/AVG will report same temperature, so using MAX here instead of
    // reading each GPC TSENSE-s values individually.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_MAX, _YES));

    //
    // If both global and individual TSENSE temperatures are faked,
    // the temperature for individual TSENSE should take priority.
    //
    gpcAvgTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_AVG, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_AVG));
    if (gpcAvgTemp != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                GPC_AVG_TEMP_FAILURE);
        goto thermTestTempOverride_GA10X_exit;
    }

    gpuMaxTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX);
    if (gpuMaxTemp == (THERM_TEST_TSENSE_TEMP + 3 * THERM_TEST_HS_TEMP_DELTA))
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                GPU_MAX_TEMP_FAILURE);
        goto thermTestTempOverride_GA10X_exit;
    }

    //
    // Now, Fake rawcode for all GPC TSENSEs, SYS TSENSE and LWL TSENSEs
    // individually.
    // Priority for Individual override > Global Override > Rawcode Override.
    //

    // Get a valid slopeA and offsetB for GPC TSENSE-s.
    s_thermTestGpcTsenseTempParamsGet_GA10X(&slopeA, &offsetB);
    if (slopeA == LW_TYPES_FXP_ZERO)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                SLOPEA_NULL_FAILURE);
        goto thermTestTempOverride_GA10X_exit;
    }

    // Get rawcode corrosponding to temperature to fake.
    rawCode = s_thermTestRawCodeForTempGet_GA10X(slopeA, offsetB,
                RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TSENSE_TEMP +
                4 * THERM_TEST_HS_TEMP_DELTA));

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    // Loop for each Enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Fake the rawcode. Its per GPC.
        s_thermTestGpcRawCodeSet_GA10X(
                pRegCacheGA10X->regGpcRawCodeOverride[idxGpc],
                rawCode, idxGpc);

        // RawCode Faked, add delay is approx 2.1 ms to reflect in therm.
        OS_PTIMER_SPIN_WAIT_US(3 * 1000);

        // Get BJT-s enabled for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Select GPC-BJT to read GPC Tsense temperature.
            s_thermTestGpcTsenseIndexSet_GA10X(idxGpc, idxBjt);

            // Get the tsense temperature of index already set above.
            tsenseTemp = s_thermTestGpcTsenseTempGet_GA10X(LW_FALSE);

            //
            // Until here as individual, global and rawCode override are set,
            // the local override faking takes priority.
            //
            if (tsenseTemp != THERM_TEST_TSENSE_TEMP)
            {
                pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                        TSOSC_RAW_TEMP_FAILURE);
                goto thermTestTempOverride_GA10X_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Overide Rawcode for LWL TSENSEs.
    s_thermTestLwlRawCodeSet_GA10X(pRegCacheGA10X->regLwlRawCodeOverride, rawCode);
    OS_PTIMER_SPIN_WAIT_US(3 * 1000);

    // Override Rawcode for SYS TSENSE.
    s_thermTestSysTsenseRawCodeSet_GA10X(pRegCacheGA10X->regSensor7, rawCode);

    //
    // If both global and individual TSOSC temperatures are faked, the
    // temperature for individual TSOSC should take priority. Raw code has
    // lowest priority. If selwre_override is selected then higher of raw
    // code and override value will be selected.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
                DRF_DEF(_CPWR_THERM, _SENSOR_9, _MAX_GPU_MAX, _YES));

    //
    // If global, individual, rawcode temperatures are faked,
    // the temperature for individual faked should take priority.
    //
    gpcAvgTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_AVG, _FIXED_POINT,
                REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_AVG));
    if (gpcAvgTemp != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                GPC_AVG_RAWCODE_FAILURE);
        goto thermTestTempOverride_GA10X_exit;
    }

    gpuMaxTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX);
    if (gpuMaxTemp == (THERM_TEST_TSENSE_TEMP + 3 * THERM_TEST_HS_TEMP_DELTA))
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                GPU_MAX_RAWCODE_FAILURE);
        goto thermTestTempOverride_GA10X_exit;
    }
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestTempOverride_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
    return FLCN_OK;
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
thermTestThermalMonitors_GA10X
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
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    //
    // Disable filtering of thermal events. No need to filter in the test
    // otherwise need to add delay until filtering is complete and until
    // monitors can start counting on the event.
    //
    reg32 = pRegCacheGA10X->regPower6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_PERIOD,
                _NONE, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_SCALE,
                _16US, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, reg32);

    // Disable overt for Thermal Event 1.
    reg32 = pRegCacheGA10X->regOvertEn;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_EN, _THERMAL_1, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, reg32);

    // Configure Thermal Event 1 to trigger on TSENSE sensor.
    s_thermTestThermalEvent1ParamSet_GA10X(pRegCacheGA10X->regEvtThermal1,
                THERM_TEST_THERM_MON_TSENSE_TEMP,
                LW_CPWR_THERM_EVT_THERMAL_1_TEMP_SENSOR_ID_SYS_TSENSE);
            
    // Override TSENSE using SW override.
    s_thermTestSysTsenseTempSet_GA10X(pRegCacheGA10X->regSysTsenseOverride,
        THERM_TEST_THERM_MON_TSENSE_TEMP + THERM_TEST_THERM_MON_TSENSE_TEMP_DELTA);

    // Loop over each Therm Monitor
    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        // Clear & Enable the Monitor
        reg32 = pRegCacheGA10X->regMonCtrl[idx];
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _EN, _ENABLE, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                    _CLEAR, _TRIGGER, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), reg32);

        //
        // Configure the monitor to monitor Active High, Level Triggered
        // Thermal 1 event.
        //
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

        // Bail out if the value read second time is not more than first one.
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

    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);

    return FLCN_OK;
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * @brief  Tests Temperature Colwersion Logic for all the Sensors.
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
s_thermTestTempColwersion_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwUFXP16_16 slopeA  = LW_TYPES_FXP_ZERO;
    LwUFXP16_16 offsetB = LW_TYPES_FXP_ZERO;
    LwTemp      overtTempThres;
    LwU32       rawCode;
    LwTemp      testTemp;
    LwU16       rawCodeMin;
    LwU16       rawCodeMax;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the inital context for HW.
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes in the
    // test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    // Disable overt for all events.
    s_thermTestDisableOvertForAllEvents_GA10X(pRegCacheGA10X->regOvertEn);

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

    // Set SENSOR_ID other than SYS, GPC, LWL TSENSE to avoid triggering overt.
    s_thermTestDedicatedOvertSet_GA10X(pRegCacheGA10X->regDedOvert,
                    overtTempThres,
                    LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV__NUM_PROVS);

    // Get a valid slopeA and offsetB for the TSENSE sensor.
    s_thermTestGpcTsenseTempParamsGet_GA10X(&slopeA, &offsetB);
    if (slopeA == LW_TYPES_FXP_ZERO)
    {
        pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                SLOPEA_NULL_FAILURE);
        goto s_thermTestTempColwersion_GA10X_exit;
    }

    // Get min rawcode to fake.
    rawCodeMin = s_thermTestRawCodeForTempGet_GA10X(slopeA, offsetB,
                RM_PMU_CELSIUS_TO_LW_TEMP(THERM_INT_SENSOR_WORKING_TEMP_MIN));

    // Get max rawcode to fake.
    rawCodeMax = s_thermTestRawCodeForTempGet_GA10X(slopeA, offsetB, overtTempThres);

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

        // Verify Temp Colwersion for GPC TSENSE Sensors.
        status = s_thermTestTempColwersiolwerifyGpcTsense_GA10X(rawCode, testTemp);
        if (status != FLCN_OK)
        {
            pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                GPC_TSENSE_RAW_TEMP_FAILURE);
            goto s_thermTestTempColwersion_GA10X_exit;
        }

        // Verify Temp Colwersion for SYS TSENSE Sensors.
        status = s_thermTestTempColwersiolwerifySysTsense_GA10X(rawCode, testTemp);
        if (status != FLCN_OK)
        {
            pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                SYS_TSENSE_RAW_TEMP_FAILURE);
            goto s_thermTestTempColwersion_GA10X_exit;
        }

        // Verify Temp Colwersion for LWL TSENSE Sensors.
        status = s_thermTestTempColwersiolwerifyLwlTsense_GA10X(rawCode, testTemp);
        if (status != FLCN_OK)
        {
            pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                LWL_TSENSE_RAW_TEMP_FAILURE);
            goto s_thermTestTempColwersion_GA10X_exit;
        }

        //
        // This test may take long on emulator. Jump factor to complete
        // the test soon.
        //
        if (rawCode == (rawCodeMax - 1))
        {
            break;
        }
        rawCode = LW_MIN((rawCode + LW_THERM_GPC_TSENSE_RAWCODE_JUMP_FACTOR),
                    (rawCodeMax - 1));
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestTempColwersion_GA10X_exit:
    if (pParams->outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
    {
        s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
    }

    return status;
}

/*!
 * @brief  Tests Temp Colwersion Logic for GPC TSENSE sensors.
 *
 * @param[in] rawCode           Rawcode to be faked.
 * @param[in] rawCodeTemp       SW Temp callwlated for the Rawcode with slope and
 *                              offset.
 *
 * @return      FLCN_OK         If validation succeeds.
 * @return      FLCN_ERROR      If validation fails.
 */
static FLCN_STATUS
s_thermTestTempColwersiolwerifyGpcTsense_GA10X
(
    LwU32       rawCode,
    LwTemp      rawCodeTemp
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwTemp      tsenseTemp;
    LwU8        idxGpc;
    LwU8        idxBjt;

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    // Loop for each Enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Fake the GPC rawcode.
        s_thermTestGpcRawCodeSet_GA10X(
            pRegCacheGA10X->regGpcRawCodeOverride[idxGpc], rawCode, idxGpc);

        // RawCode Faked, add delay is approx 2.1 ms to reflect in therm.
        OS_PTIMER_SPIN_WAIT_US(10 * 1000);

        // Get BJT-s enabled for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Select GPC-BJT pair to read Tsense/Offset Tsense temperature.
            s_thermTestGpcTsenseIndexSet_GA10X(idxGpc, idxBjt);

            // Get the tsense temperature of GPC-BJT index set above.
            tsenseTemp = s_thermTestGpcTsenseTempGet_GA10X(LW_FALSE);

            // Validate Temp callwlated in SW matches actual temp for rawCode.
            if (tsenseTemp != rawCodeTemp)
            {
                status = FLCN_ERROR;
                goto s_thermTestTempColwersiolwerifyGpcTsense_GA10X;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END

s_thermTestTempColwersiolwerifyGpcTsense_GA10X:
    return status;
}

/*!
 * @brief  Tests Temp Colwersion Logic for SYS TSENSE.
 *
 * @param[in] rawCode           Rawcode to be faked.
 * @param[in] rawCodeTemp       SW Temp callwlated for the Rawcode with slope and
 *                              offset.
 *
 * @return      FLCN_OK         If validation succeeds.
 * @return      FLCN_ERROR      If validation fails.
 */
static FLCN_STATUS
s_thermTestTempColwersiolwerifySysTsense_GA10X
(
    LwU32       rawCode,
    LwTemp      rawCodeTemp
)
{
    FLCN_STATUS status = FLCN_OK;
    LwTemp      sysTsenseTemp;
    
    // Fake SYS TSENSE temperature by setting the raw override.
    s_thermTestSysTsenseRawCodeSet_GA10X(pRegCacheGA10X->regSensor7, rawCode);

    // RawCode Faked, add delay is approx 2.1 ms to reflect in therm.
    OS_PTIMER_SPIN_WAIT_US(3 * 1000);

    // Get the SYS TSENSE temperature.
    sysTsenseTemp = thermGetTempInternal_HAL(&Therm,
                        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    // Validate the Temp callwlated in SW matches actual temp for rawCode.
    if (sysTsenseTemp != rawCodeTemp)
    {
        status = FLCN_ERROR;
    }

    return status;
}

/*!
 * @brief  Tests Temp Colwersion Logic for LWL TSENSE sensors.
 *
 * @param[in] rawCode           Rawcode to be faked.
 * @param[in] rawCodeTemp       SW Temp callwlated for the Rawcode with slope and
 *                              offset.
 *
 * @return      FLCN_OK         If validation succeeds.
 * @return      FLCN_ERROR      If validation fails.
 */
static FLCN_STATUS
s_thermTestTempColwersiolwerifyLwlTsense_GA10X
(
    LwU32       rawCode,
    LwTemp      rawCodeTemp
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       lwlEnMask;
    LwTemp      tsenseTemp;
    LwU8        idxLwl;

    // Overide Rawcode for LWL TSENSEs.
    s_thermTestLwlRawCodeSet_GA10X(pRegCacheGA10X->regLwlRawCodeOverride,
                rawCode);

    // RawCode Faked, add delay is approx 2.1 ms to reflect in therm.
    OS_PTIMER_SPIN_WAIT_US(3 * 1000);

    // Get LWL TSENSE Enabled Mask.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();

    // Loop each enabled LWL TSENSEs.
    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        // Select LWL TSENSE for the index.
        s_thermTestLwlTsenseIndexSet_GA10X(idxLwl);

        // Read temp and validate.
        tsenseTemp = s_thermTestLwlTsenseTempGet_GA10X(LW_FALSE);

        // Validate the Temp callwlated in SW matches actual temp for rawCode.
        if (tsenseTemp != rawCodeTemp)
        {
            status = FLCN_ERROR;
            goto s_thermTestTempColwersiolwerifyLwlTsense_GA10X;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END

s_thermTestTempColwersiolwerifyLwlTsense_GA10X:
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
s_thermTestMaxAvgExelwte_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       totalBjtCount = 0;
    LwS32       maxTemp       = 0;
    LwS32       avgTemp       = 0;
    LwS32       testTemp      = THERM_TEST_TEMP_CELSIUS_40;
    LwU32       tempStride    = 2;                          // Starting with Even number with even stride would always have integer Average.
    LwU32       lwlBjtCount;
    LwU32       gpcBjtCount;
    LwU32       gpcEnMask;
    LwU32       bjtEnMask;
    LwU32       lwlEnMask;
    LwTemp      tsenseTemp;
    LwU8        idxGpc;
    LwU8        idxBjt;
    LwU8        idxLwl;
    FLCN_STATUS status;
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the initial context for HW.
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    //
    // Fake GPC TSENSE Temp and HS Offset.
    // Callwlate Max and AVG.
    //

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    // Loop each enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        // Maintain count of BJT-s.
        gpcBjtCount = bjtEnMask;
        NUMSETBITS_32(gpcBjtCount);
        totalBjtCount += gpcBjtCount;

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            // Fake Lwrr Temp for GPC TSENSE.
           s_thermTestGpcTsenseTempSet_GA10X(
                pRegCacheGA10X->regGpcTsenseOverride[idxGpc][idxBjt],
                RM_PMU_CELSIUS_TO_LW_TEMP(testTemp), idxGpc, idxBjt);

            // Select BJT in TSENSE_INDEX.
            s_thermTestGpcTsenseIndexSet_GA10X(idxGpc, idxBjt);

            // Get Lwrr temp.
            tsenseTemp = s_thermTestGpcTsenseTempGet_GA10X(LW_FALSE);
            if (tsenseTemp != RM_PMU_CELSIUS_TO_LW_TEMP(testTemp))
            {
                pParams->outStatus =
                    LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                    TSOSC_TEMP_FAKING_FAILURE);
                goto s_thermTestMaxAvgExelwte_GA10X_exit;
            }

            // Set the hotspot offset for the selected GPC TSENSE sensor.
            s_thermTestGpcTsenseHsOffsetSet_GA10X(
                pRegCacheGA10X->regGpcTsenseHsOffset[idxGpc][idxBjt],
                THERM_TEST_HS_TEMP, idxGpc, idxBjt);

            maxTemp  = LW_MAX(maxTemp, testTemp);
            avgTemp += testTemp;

            testTemp += tempStride;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Fake SYS TSENSE Temp and HS Offset.
    // Callwlate Max and AVG.
    //

    // Fake SYS TSENSE Temp.
    s_thermTestSysTsenseTempSet_GA10X(pRegCacheGA10X->regSysTsenseOverride,
                RM_PMU_CELSIUS_TO_LW_TEMP(testTemp));

    // Update MAX and AVG.
    maxTemp   = LW_MAX(maxTemp, testTemp);
    avgTemp  += testTemp;
    testTemp += tempStride;
    totalBjtCount++;

    // Set the hotspot offset for the selected GPC TSENSE sensor.
    s_thermTestSysTsenseHsOffsetSet_GA10X(
                pRegCacheGA10X->regSysTsenseHsOffset, THERM_TEST_HS_TEMP);

    //
    // Fake LWL TSENSE Temp and HS Offset.
    // Callwlate Max and AVG.
    //

    // Get LWL TSENSE Enabled Mask.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();
    lwlBjtCount  = lwlEnMask;
    NUMSETBITS_32(lwlBjtCount);
    totalBjtCount += lwlBjtCount;

    // Loop each enabled LWL TSENSEs.
    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        // Fake temperature for LWL TSENSE.
        s_thermTestLwlTsenseTempSet_GA10X(
                pRegCacheGA10X->regLwlTsenseOverride[idxLwl],
                RM_PMU_CELSIUS_TO_LW_TEMP(testTemp), idxLwl);

        // Set the hotspot offset for the selected LWL TSENSE sensor.
        s_thermTestLwlTsenseHsOffsetSet_GA10X(
                pRegCacheGA10X->regLwlTsenseHsOffset[idxLwl],
                THERM_TEST_HS_TEMP, idxLwl);

        // Update Max and Average.
        maxTemp  = LW_MAX(maxTemp, testTemp);
        avgTemp += testTemp;

        testTemp += tempStride;
    }
    FOR_EACH_INDEX_IN_MASK_END

    // Callwlate the average TSENSE temp.
    if (totalBjtCount != 0)
    {
        avgTemp = (avgTemp / totalBjtCount);
    }

    // Validate the actual GPU AVG temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);
    if (LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(tsenseTemp) != avgTemp)
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_AVG_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA10X_exit;
    }

    // Validate the actual GPU AVG Offset temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_AVG);
    if (LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(tsenseTemp) !=
        (avgTemp + LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(THERM_TEST_HS_TEMP)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_OFFSET_AVG_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA10X_exit;
    }

    // Validate the actual GPU MAX temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX);
    if (tsenseTemp != RM_PMU_CELSIUS_TO_LW_TEMP(maxTemp))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_MAX_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA10X_exit;
    }

    // Validate the actual GPU MAX Offset temperature value.
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);
    if (tsenseTemp != ((RM_PMU_CELSIUS_TO_LW_TEMP(maxTemp) + THERM_TEST_HS_TEMP)))
    {
        pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_OFFSET_MAX_FAILURE);
        goto s_thermTestMaxAvgExelwte_GA10X_exit;
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
        goto s_thermTestMaxAvgExelwte_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestMaxAvgExelwte_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
    return status;
}

/*!
 * @brief   Tests Global snapshot - GPU Combined Features.
 *          It should freeze only snapshot values, not non-snapshot values.
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
s_thermTestGlobalSnapshotGpuCombined_GA10X
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
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes
    // in the test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    // Override temp for all GPC.
    tempOverride_X = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_40);
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
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
        goto s_thermTestGlobalSnapshotGpuCombined_GA10X_exit;
    }

    // Trigger Global Snapshot.
    s_thermTestGlobalSnapshotTrigger_GA10X();

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
        goto s_thermTestGlobalSnapshotGpuCombined_GA10X_exit;
    }

    // Override temp for all GPC as non snapshot values.
    tempOverride_Y = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_50);
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
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
        goto s_thermTestGlobalSnapshotGpuCombined_GA10X_exit;
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
        goto s_thermTestGlobalSnapshotGpuCombined_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestGlobalSnapshotGpuCombined_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
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
s_thermTestGlobalSnapshotHwPipeline_GA10X
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
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes
    // in the test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    // Set sensor id and threshold for EVT_THERMAL_1.
    tempEvent1Th = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_45);
    s_thermTestThermalEvent1ParamSet_GA10X(pRegCacheGA10X->regEvtThermal1,
                tempEvent1Th,
                LW_CPWR_THERM_EVT_THERMAL_1_TEMP_SENSOR_ID_GPU_AVG);

    filterTimeUs = s_thermTestFilterIntervalTimeUsGet_GA10X();

    // Override temp for all GPC with temp below event1 threshold.
    tempOverride = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_40);
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
                tempOverride);

    // Trigger Global Snapshot.
    s_thermTestGlobalSnapshotTrigger_GA10X();

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
        goto s_thermTestGlobalSnapshotHwPipeline_GA10X_exit;
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
        goto s_thermTestGlobalSnapshotHwPipeline_GA10X_exit;
    }

    //
    // Override temp for all GPC with temp above event1 threshold.
    // Event shall be triggered here.
    //
    tempOverride = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_50);
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
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
        goto s_thermTestGlobalSnapshotHwPipeline_GA10X_exit;
    }

    //
    // Event Triggered so recover by overrriding temp below event1 threshold and
    // waiting for filter time period to get it normal.
    //
    tempOverride = RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TEMP_CELSIUS_40);
    s_thermTestGlobalOverrideSet_GA10X(pRegCacheGA10X->regGlobalOverride,
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
        goto s_thermTestGlobalSnapshotHwPipeline_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

s_thermTestGlobalSnapshotHwPipeline_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
    return status;
}

/*!
 * @brief  Tests Global snapshot - Individual Sensors.
 *         It should freeze only snapshot values, not non-snapshot values.
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
s_thermTestGlobalSnapshotGpuSites_GA10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwS32                  tempStride = LW_THERM_TEMP_SNAPSHOT_STRIDE;
    FLCN_STATUS            status     = FLCN_ERROR;
    LW_THERMAL_TEST_STATUS lwstatus;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Save the inital context for HW.
    status = s_thermTestRegCacheInitRestore_GA10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes
    // in the test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GA10X(pRegCacheGA10X->regIntrEn,
                pRegCacheGA10X->regUseA);

    // Override SYS, LWL and GPC Tsense Temperature and corresponding HS Offset.
    s_thermTestGlobalSnapshotGpuSitesFakeTemp_GA10X(THERM_TEST_TEMP_CELSIUS_20,
                THERM_TEST_HS_TEMP_SNAPSHOT, tempStride);

    // Trigger Global Snapshot.
    s_thermTestGlobalSnapshotTrigger_GA10X();

    // Verify Global Snapshot values.
    lwstatus = s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X(
                THERM_TEST_TEMP_CELSIUS_20, THERM_TEST_HS_TEMP_SNAPSHOT,
                tempStride);
    if (lwstatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
    {
        pParams->outData = lwstatus; 
        goto s_thermTestGlobalSnapshotGpuSites_GA10X_exit;
    }

    // Do Not Trigger Global Snapshot.

    // Override SYS, LWL and GPC Tsense Temperature and corresponding HS Offset.
    s_thermTestGlobalSnapshotGpuSitesFakeTemp_GA10X(THERM_TEST_TEMP_CELSIUS_30,
                THERM_TEST_HS_TEMP_SNAPSHOT, tempStride);

    // Verify Global Snapshot values to be same as faked temp before trigger.
    lwstatus = s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X(
                THERM_TEST_TEMP_CELSIUS_20, THERM_TEST_HS_TEMP_SNAPSHOT,
                tempStride);
    if (lwstatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
    {
        pParams->outData = lwstatus; 
        goto s_thermTestGlobalSnapshotGpuSites_GA10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
    status = FLCN_OK;

s_thermTestGlobalSnapshotGpuSites_GA10X_exit:
    s_thermTestRegCacheInitRestore_GA10X(LW_FALSE);
    return status;
}

/*!
 * @brief  Override SYS, LWL and GPC Tsense Temperature and corresponding HS Offset. 
 *
 * @param[in]   fakeTemp        Initial Temp to start faking. 
 * @param[in]   bHsTemp         HS Offset value. 
 * @param[out]  tempStride      Temp stride for each overriding.
 *
 */
static void
s_thermTestGlobalSnapshotGpuSitesFakeTemp_GA10X
(
    LwS32   fakeTemp,
    LwTemp  hsOffset,
    LwS32   tempStride
)
{
    LwU32   gpcEnMask;
    LwU32   gpcBjtEnMask;
    LwU32   lwlEnMask;
    LwU8    idxGpc;
    LwU8    idxBjt;
    LwU8    idxLwl;

    //
    // Override SYS Tsense Temperature.
    // Also, Fake corresponding HS Offset.
    //

    // Fake SYS Tsense temperature.
    s_thermTestSysTsenseTempSet_GA10X(pRegCacheGA10X->regSysTsenseOverride,
                RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp));

    // Fake HS Offset.
    s_thermTestSysTsenseHsOffsetSet_GA10X(pRegCacheGA10X->regSysTsenseHsOffset,
                hsOffset);

    fakeTemp += tempStride;

    //
    // Override LWL Tsense Temperature.
    // Also, Fake corresponding HS Offset.
    //

    // Get LWL TSENSE Enabled Mask.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();

    // Loop each enabled LWL TSENSEs.
    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        // Fake temperature for LWL TSENSE.
        s_thermTestLwlTsenseTempSet_GA10X(
                pRegCacheGA10X->regLwlTsenseOverride[idxLwl],
                RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp), idxLwl);

        // Fake HS Offset for LWL Tsense.
        s_thermTestLwlTsenseHsOffsetSet_GA10X(
                pRegCacheGA10X->regLwlTsenseHsOffset[idxLwl], hsOffset, idxLwl);

        fakeTemp += tempStride;
    }
    FOR_EACH_INDEX_IN_MASK_END

    //
    // Override GPC Tsense Temperature.
    // Also, Fake corresponding HS Offset.
    //

    // Get GPC Enabled Mask.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    // Loop each enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        gpcBjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, gpcBjtEnMask)
        {
            // Fake temperature for GPC TSENSE.
            s_thermTestGpcTsenseTempSet_GA10X(
                pRegCacheGA10X->regGpcTsenseOverride[idxGpc][idxBjt],
                RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp), idxGpc, idxBjt);

            // Set the hotspot offset for the selected GPC TSENSE sensor.
            s_thermTestGpcTsenseHsOffsetSet_GA10X(
                pRegCacheGA10X->regGpcTsenseHsOffset[idxGpc][idxBjt],
                hsOffset, idxGpc, idxBjt);

            fakeTemp += tempStride;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief  Verify SYS, LWL and GPC Tsense Temperature and corresponding HS Offset. 
 *
 * @param[in]   initFakeTemp    Initial Temp to start faking. 
 * @param[in]   bHsTemp         HS Offset value. 
 * @param[in]   tempStride      Temp stride for each overriding.
 *
 * @return      LW_OK                       If test passes on the GPU.
 * @return      GLOBAL_SNAPSHOT_* ERRORs    If test failed due to mismatch.
 */
static LW_THERMAL_TEST_STATUS
s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X
(
    LwS32   fakeTemp,
    LwTemp  hsOffset,
    LwS32   tempStride
)
{
    LW_THERMAL_TEST_STATUS status = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
    LwU32                  gpcEnMask;
    LwU32                  gpcBjtEnMask;
    LwU32                  lwlEnMask;
    LwTemp                 snapTemp;
    LwTemp                 snapOffTemp;
    LwU8                   idxGpc;
    LwU8                   idxBjt;
    LwU8                   idxLwl;

    // Verify SYS TSENSE Snapshot temp.
    snapTemp = s_thermTestSysTsenseSnapshotTempGet_GA10X(LW_FALSE);
    if (snapTemp != RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp))
    {
        status = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, 
                            SYS_TSENSE_SNAPSHOT_MISMATCH);
        goto s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit;
    }

    snapOffTemp = s_thermTestSysTsenseSnapshotTempGet_GA10X(LW_TRUE);
    if (snapOffTemp != (RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp) + hsOffset))
    {
        status = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, 
                            SYS_OFFSET_TSENSE_SNAPSHOT_MISMATCH);
        goto s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit;
    }

    fakeTemp += tempStride;

    // Verify LWL TSENSE Snapshot temp.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();

    // Loop each enabled LWL TSENSEs.
    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        // Select LWL TSENSE for the index.
        s_thermTestLwlTsenseSnapshotIndexSet_GA10X(idxLwl);

        // Read temp and validate.
        snapTemp = s_thermTestLwlTsenseSnapshotTempGet_GA10X(LW_FALSE);
        if (snapTemp != RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp))
        {
            status = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, 
                            LWL_TSENSE_SNAPSHOT_MISMATCH);
            goto s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit;
        }

        // Read Offset temp and validate.
        snapOffTemp = s_thermTestLwlTsenseSnapshotTempGet_GA10X(LW_TRUE);
        if (snapOffTemp != (RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp) + hsOffset))
        {
            status = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, 
                            LWL_OFFSET_TSENSE_SNAPSHOT_MISMATCH);
            goto s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit;
        }

        fakeTemp += tempStride;
    }
    FOR_EACH_INDEX_IN_MASK_END

    // Verify GPC TSENSE Snapshot temp.
    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    // Loop each enabled GPC.
    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        gpcBjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, gpcBjtEnMask)
        {
            // Select GPC-BJT pair to read Tsense/Offset Tsense temperature.
            s_thermTestGpcTsenseSnapshotIndexSet_GA10X(idxGpc, idxBjt);

            // Read temp and validate 
            snapTemp = s_thermTestGpcTsenseSnapshotTempGet_GA10X(LW_FALSE);
            if (snapTemp != RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp))
            {
                status = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, 
                            GPC_TSENSE_SNAPSHOT_MISMATCH);
                goto s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit;
            }

            // Read Offset temp and validate.
            snapOffTemp = s_thermTestGpcTsenseSnapshotTempGet_GA10X(LW_TRUE);
            if (snapOffTemp != (RM_PMU_CELSIUS_TO_LW_TEMP(fakeTemp) + hsOffset))
            {
                status = LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, 
                            GPC_OFFSET_TSENSE_SNAPSHOT_MISMATCH);
                goto s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit;
            }
            fakeTemp += tempStride;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

s_thermTestGlobalSnapshotGpuSitesVerifyTemp_GA10X_exit:
    return status;
}

/*!
 * @brief Interface to Save Registers from HW to regCache.
 *
 * @param[in] void
 * @return void
 *
 */
static void
s_thermTestRegCacheInit_GA10X(void)
{
    LwU32   gpcEnMask;
    LwU32   bjtEnMask;
    LwU32   lwlEnMask;
    LwU8    idxGpc;
    LwU8    idxBjt;
    LwU8    idxLwl;
    LwU8    idx;

    pRegCacheGA10X->regIntrEn               = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    pRegCacheGA10X->regUseA                 = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    pRegCacheGA10X->regOvertEn              = REG_RD32(CSB, LW_CPWR_THERM_OVERT_EN);
    pRegCacheGA10X->regOvertCtrl            = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    pRegCacheGA10X->regSensor6              = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_6);
    pRegCacheGA10X->regSensor7              = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_7);
    pRegCacheGA10X->regSysTsenseTempCode    = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_0);
    pRegCacheGA10X->regEvtThermal1          = REG_RD32(CSB, LW_CPWR_THERM_EVT_THERMAL_1);
    pRegCacheGA10X->regPower6               = REG_RD32(CSB, LW_CPWR_THERM_POWER_6);
    pRegCacheGA10X->regInterpolationCtrl    = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_INTERPOLATION_CTRL);
    pRegCacheGA10X->regLwlRawCodeOverride   = REG_RD32(CSB, LW_CPWR_THERM_LWL_RAWCODE_OVERRIDE);
    pRegCacheGA10X->regSysTsenseOverride    = REG_RD32(CSB, LW_CPWR_THERM_SYS_TSENSE_OVERRIDE);
    pRegCacheGA10X->regSysTsenseHsOffset    = REG_RD32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET);

    pRegCacheGA10X->regGpcTsenseIdx         = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX);
    pRegCacheGA10X->regGpcTsenseOverrideIdx = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE_INDEX);
    pRegCacheGA10X->regGpcTsenseHsOffsetIdx = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET_INDEX);
    pRegCacheGA10X->regLwlTsenseOverrideIdx = REG_RD32(CSB, LW_CPWR_THERM_LWL_TSENSE_OVERRIDE_INDEX);
    pRegCacheGA10X->regDedOvert             = REG_RD32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT);
    pRegCacheGA10X->regGlobalOverride       = REG_RD32(CSB, LW_CPWR_THERM_TSENSE_GLOBAL_OVERRIDE);

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        pRegCacheGA10X->regMonCtrl[idx] = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx));
    }

    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled Mask for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        // Select GPC Index to backup regGpcRawCodeOverride.
        s_thermTestGpcIndexSet_GA10X(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            //
            // Save Rawcode at LW_CPWR_THERM_GPC_RAWCODE_OVERRIDE of the GPC
            // selected. 
            //
            pRegCacheGA10X->regGpcRawCodeOverride[idxGpc] = REG_RD32(CSB,
                LW_CPWR_THERM_GPC_RAWCODE_OVERRIDE);

            // Select BJT in TSENSE_INDEX and save related register arrays.
            s_thermTestGpcTsenseIndexSet_GA10X(idxGpc, idxBjt);
            pRegCacheGA10X->regGpcTsense[idxGpc][idxBjt] = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE);
            pRegCacheGA10X->regGpcTsenseRawCode[idxGpc][idxBjt] = REG_RD32(CSB,
                LW_CPWR_THERM_GPC_TSENSE_RAW_CODE);

            // Select BJT in OVERRIDE_INDEX and save related register arrays.
            s_thermTestGpcTsenseOverrideIndexSet_GA10X(idxGpc, idxBjt);
            pRegCacheGA10X->regGpcTsenseOverride[idxGpc][idxBjt] = REG_RD32(CSB,
                LW_CPWR_THERM_GPC_TSENSE_OVERRIDE);

            // Select BJT in HS_OFFSET_INDEX and save related register arrays.
            s_thermTestGpcTsenseHsOffsetIndexSet_GA10X(idxGpc, idxBjt);
            pRegCacheGA10X->regGpcTsenseHsOffset[idxGpc][idxBjt] = REG_RD32(CSB,
                LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Backup LWL TSENSE related registers.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();

    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        s_thermTestLwlTsenseHsOffsetIndexSet_GA10X(idxLwl);
        pRegCacheGA10X->regLwlTsenseHsOffset[idxLwl] = REG_RD32(CSB, LW_CPWR_THERM_LWL_TSENSE_HS_OFFSET);

        s_thermTestLwlTsenseOverrideIndexSet_GA10X(idxLwl);
        pRegCacheGA10X->regLwlTsenseOverride[idxLwl] = REG_RD32(CSB, LW_CPWR_THERM_LWL_TSENSE_OVERRIDE);

        s_thermTestLwlTsenseIndexSet_GA10X(idxLwl);
        pRegCacheGA10X->regLwlTsenseRawCode[idxLwl] = REG_RD32(CSB, LW_CPWR_THERM_LWL_TSENSE_RAW_CODE);
    }
    FOR_EACH_INDEX_IN_MASK_END

    //
    // Disable Interpolation
    // TODO Need to find the placeholder once Interpolation test is added.
    // Lwrrenlty, there is no such plans due to unavailavility mods of framework
    //
    s_thermTestThermalInterpolationDisable_GA10X(pRegCacheGA10X->regInterpolationCtrl);
}

/*!
 * @brief Interface to Restore Registers from regCache to HW.
 */
static void
s_thermTestRegCacheRestore_GA10X(void)
{
    LwU32   gpcEnMask;
    LwU32   bjtEnMask;
    LwU32   lwlEnMask;
    LwU32   filterTimeUs;
    LwU8    idxGpc;
    LwU8    idxBjt;
    LwU8    idxLwl;
    LwU8    idx;

    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, pRegCacheGA10X->regIntrEn);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, pRegCacheGA10X->regUseA);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, pRegCacheGA10X->regSensor6);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, pRegCacheGA10X->regSensor7);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_THERMAL_1, pRegCacheGA10X->regEvtThermal1);
    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, pRegCacheGA10X->regPower6);
    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_OVERRIDE, pRegCacheGA10X->regSysTsenseOverride);
    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET, pRegCacheGA10X->regSysTsenseHsOffset);

    REG_WR32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT, pRegCacheGA10X->regDedOvert);
    REG_WR32(CSB, LW_CPWR_THERM_TSENSE_GLOBAL_OVERRIDE, pRegCacheGA10X->regGlobalOverride);

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), pRegCacheGA10X->regMonCtrl[idx]);
    }

    gpcEnMask = s_thermTestGpcEnMaskGet_GA10X();

    FOR_EACH_INDEX_IN_MASK(32, idxGpc, gpcEnMask)
    {
        // Get BJT-s enabled for the GPC index.
        bjtEnMask = thermTestBjtEnMaskForGpcGet_HAL(&Therm, idxGpc);

        // Select GPC Index to Restore regGpcRawCodeOverride.
        s_thermTestGpcIndexSet_GA10X(idxGpc);

        FOR_EACH_INDEX_IN_MASK(32, idxBjt, bjtEnMask)
        {
            //
            // Restore Rawcode at LW_CPWR_THERM_GPC_RAWCODE_OVERRIDE of the GPC
            // selected. 
            //
            REG_WR32(CSB, LW_CPWR_THERM_GPC_RAWCODE_OVERRIDE,
                        pRegCacheGA10X->regGpcRawCodeOverride[idxGpc]);

            // Select BJT in TSENSE_INDEX and restore related register arrays.
            s_thermTestGpcTsenseIndexSet_GA10X(idxGpc, idxBjt);
            REG_WR32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE,
                        pRegCacheGA10X->regGpcTsense[idxGpc][idxBjt]);

            // Restore original rawcode by rawcode override.
            s_thermTestGpcRawCodeSet_GA10X(
                        pRegCacheGA10X->regGpcRawCodeOverride[idxGpc],
                        DRF_VAL(_CPWR_THERM, _GPC_TSENSE_RAW_CODE, _VALUE, pRegCacheGA10X->regGpcTsenseRawCode[idxGpc][idxBjt]),
                        idxGpc);

            // Select BJT in OVERRIDE_INDEX and restore related register arrays.
            s_thermTestGpcTsenseOverrideIndexSet_GA10X(idxGpc, idxBjt);
            REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE,
                        pRegCacheGA10X->regGpcTsenseOverride[idxGpc][idxBjt]);

            // Select BJT in HS_OFFSET_INDEX and restore related register arrays.
            s_thermTestGpcTsenseHsOffsetIndexSet_GA10X(idxGpc, idxBjt);
            REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET,
                        pRegCacheGA10X->regGpcTsenseHsOffset[idxGpc][idxBjt]);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Restore LWL TSENSE related registers.
    lwlEnMask = s_thermTestLwlEnMaskGet_GA10X();

    FOR_EACH_INDEX_IN_MASK(32, idxLwl, lwlEnMask)
    {
        s_thermTestLwlTsenseOverrideIndexSet_GA10X(idxLwl);
        REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_OVERRIDE,
                        pRegCacheGA10X->regLwlTsenseOverride[idxLwl]);

        s_thermTestLwlTsenseHsOffsetIndexSet_GA10X(idxLwl);
        REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_HS_OFFSET,
                        pRegCacheGA10X->regLwlTsenseHsOffset[idxLwl]);
    }
    FOR_EACH_INDEX_IN_MASK_END

    // Restore Rawcode for LWL Tsense. Override is common even if differs.
    s_thermTestLwlRawCodeSet_GA10X(pRegCacheGA10X->regLwlRawCodeOverride,
                        DRF_VAL(_CPWR_THERM, _LWL_TSENSE_RAW_CODE, _VALUE, pRegCacheGA10X->regLwlTsenseRawCode[0]));

    // Restore SYS Tsense Rawcode.
    s_thermTestSysTsenseRawCodeSet_GA10X(pRegCacheGA10X->regSensor7,
                        DRF_VAL(_CPWR_THERM, _SENSOR_0, _TEMP_CODE, pRegCacheGA10X->regSysTsenseTempCode));

    // Restore Indices after restoring register values for all index.
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX,
                        pRegCacheGA10X->regGpcTsenseIdx);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE_INDEX,
                        pRegCacheGA10X->regGpcTsenseOverrideIdx);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET_INDEX,
                        pRegCacheGA10X->regGpcTsenseHsOffsetIdx);
    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_OVERRIDE_INDEX,
                        pRegCacheGA10X->regLwlTsenseOverrideIdx);

    // Wait for filter period to disable events raised.
    filterTimeUs = s_thermTestFilterIntervalTimeUsGet_GA10X();
    OS_PTIMER_SPIN_WAIT_US(filterTimeUs);

    // In the end restore OVERT_EN so as not to trigger overt accidentally.
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INTERPOLATION_CTRL, pRegCacheGA10X->regInterpolationCtrl);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, pRegCacheGA10X->regOvertCtrl);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, pRegCacheGA10X->regOvertEn);
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
s_thermTestRegCacheInitRestore_GA10X
(
    LwBool bSave
)
{
    if (pRegCacheGA10X == NULL)
    {
        memset(&thermRegCacheGA10X, 0, sizeof(THERM_TEST_REG_CACHE_GA10X));
        pRegCacheGA10X = &thermRegCacheGA10X;
    }

    if (bSave)
    {
        s_thermTestRegCacheInit_GA10X();
    }
    else
    {
        s_thermTestRegCacheRestore_GA10X();
    }

    return FLCN_OK;
}

/*!
 * @brief Interface to disable slowdown and disable interrupts.
 */
static void
s_thermTestDisableSlowdownInterrupt_GA10X
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
s_thermTestDisableOvertForAllEvents_GA10X
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
 * @brief  Programs the GPC indices in the GPC_INDEX register.
 *         Read and write Increment will be disabled.
 *
 *         Aperture Regsiters:
 *            - GPC_TSENSE
 *            - GPC_TS_ADJ
 *            - GPC_RAWCODE_OVERRIDE
 *            - GPC_TS_AUTO_CONTROL
 *
 * @param[in]   idxGpc      GPC index to select.
 */
static void
s_thermTestGpcIndexSet_GA10X
(
    LwU8    idxGpc
)
{
    LwU32 reg32 = 0;

    // Set GPC Index for GPC_INDEX register.
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_INDEX, _INDEX, idxGpc, reg32);

    // Disable Read Increment.
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _WRITEINCR, _DISABLED, reg32);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_INDEX, reg32);
}

/*!
 * @brief  Programs the GPC and BJT indices in the GPC_TSENSE_INDEX register.
 *         Read Increment will be disabled.
 *
 *         Aperture Registers:
 *            - GPC_TSENSE_RAW_CODE
 *            - TEMP_SENSOR_GPC_TSENSE
 *            - TEMP_SENSOR_OFFSET_GPC_TSENSE
 *            - TEMP_SENSOR_GPC_TSENSE_UNMUNGED
 *            - TEMP_SENSOR_OFFSET_GPC_TSENSE_UNMUNGED
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcTsenseIndexSet_GA10X
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
 * @brief  Programs the GPC and BJT SNAPSHOT indices.
 *         Read Increment will be disabled.
 *
 *         Aperture Registers:
 *            - TEMP_SENSOR_GPC_TSENSE_SNAPSHOT
 *            - TEMP_SENSOR_OFFSET_GPC_TSENSE_SNAPSHOT
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcTsenseSnapshotIndexSet_GA10X
(
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    LwU32 gpcTsenseIndex = 0;

    // Set GPC_BJT_INDEX for GPC_TSENSE_INDEX_SNAPSHOT.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_INDEX_SNAPSHOT,
                _GPC_BJT_INDEX, idxBjt, gpcTsenseIndex);

    // Set GPC_INDEX for GPC_TSENSE_INDEX.
    gpcTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_INDEX_SNAPSHOT,
                _GPC_INDEX, idxGpc, gpcTsenseIndex);

    // Disable Read Increment.
    gpcTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INDEX_SNAPSHOT,
                _READINCR,_DISABLED, gpcTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INDEX_SNAPSHOT, gpcTsenseIndex);
}

/*!
 * @brief  Selects GPC-BJT for overiding GPC TSENSE temperature.
 *         Read/Write increment will be disabled.
 *
 *         Aperture Registers:
 *            - GPC_TSENSE_OVERRIDE
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcTsenseOverrideIndexSet_GA10X
(
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    LwU32 gpcTsenseIndex = 0;

    // Set BJT_INDEX for GPC_TSENSE_INDEX.
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
 *         Aperture Registers:
 *            - GPC_TSENSE_HS_OFFSET
 *
 * @param[in]   idxGpc      GPC index to select.
 * @param[in]   idxBjt      BJT Index to select.
 */
static void
s_thermTestGpcTsenseHsOffsetIndexSet_GA10X
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
 * @brief  Selects LWL TSENSE and LWL_TSENSE_HS_OFFSET_INDEX.
 *         Read/Write increment will be disabled.
 *
 *         Aperture Register:
 *            - LWL_TSENSE_HS_OFFSET
 *
 * @param[in]   idxLwl      LWL index to select.
 */
static void
s_thermTestLwlTsenseHsOffsetIndexSet_GA10X
(
    LwU8    idxLwl
)
{
    LwU32 lwlTsenseIndex = 0;

    // Set LWL_INDEX for LWL_TSENSE_INDEX.
    lwlTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_TSENSE_HS_OFFSET_INDEX,
                _INDEX, idxLwl, lwlTsenseIndex);

    // Disable ReadInc.
    lwlTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_HS_OFFSET_INDEX,
                _READINCR, _DISABLED, lwlTsenseIndex);

    // Disable WriteInc.
    lwlTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_HS_OFFSET_INDEX,
                _WRITEINCR, _DISABLED, lwlTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_HS_OFFSET_INDEX, lwlTsenseIndex);
}

/*!
 * @brief  Programs the LWL TSENSEs indices in the LWl_TSENSE_INDEX register.
 *         Read Increment will be disabled.
 *
 *         Aperture Registers:
 *            - LWL_TSENSE_RAW_CODE
 *            - TEMP_SENSOR_LWL_TSENSE
 *            - TEMP_SENSOR_OFFSET_LWL_TSENSE
 *
 * @param[in]   idxLwl      LWL Index to select.
 */
static void
s_thermTestLwlTsenseIndexSet_GA10X
(
    LwU8    idxLwl
)
{
    LwU32 lwlTsenseIndex = 0;

    // Set LWL_INDEX for GPC_TSENSE_INDEX.
    lwlTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_TSENSE_INDEX, _INDEX,
                                idxLwl, lwlTsenseIndex);

    // Disable Read Increment.
    lwlTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_INDEX, _READINCR,
                                _DISABLED, lwlTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_INDEX, lwlTsenseIndex);
}

/*!
 * @brief  Programs the LWL TSENSEs Snapshot indices.
 *         Read Increment will be disabled.
 *
 *         Aperture Registers:
 *            - TEMP_SENSOR_LWL_TSENSE_SNAPSHOT
 *            - TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT
 *
 * @param[in]   idxLwl      LWL Index to select.
 */
static void
s_thermTestLwlTsenseSnapshotIndexSet_GA10X
(
    LwU8    idxLwl
)
{
    LwU32 lwlTsenseIndex = 0;

    // Set LWL_INDEX for GPC_TSENSE_INDEX.
    lwlTsenseIndex = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_TSENSE_INDEX_SNAPSHOT,
                                _INDEX, idxLwl, lwlTsenseIndex);

    // Disable Read Increment.
    lwlTsenseIndex = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_INDEX_SNAPSHOT,
                                _READINCR, _DISABLED, lwlTsenseIndex);

    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_INDEX_SNAPSHOT, lwlTsenseIndex);
}

/*!
 * @brief  Programs the LWL TSENSE Override indices.
 *         Read and write Increment will be disabled.
 *
 *         Aperture Regsiters:
 *            - LWL_TSENSE_OVERRIDE
 *
 * @param[in]   idxLwl      LWL TSENSE index to select.
 */
static void
s_thermTestLwlTsenseOverrideIndexSet_GA10X
(
    LwU8    idxLwl
)
{
    LwU32 reg32 = 0;

    // Set LWL Index for LWL_TSENSE_OVERRIDE_INDEX.
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_TSENSE_OVERRIDE_INDEX, _INDEX,
                                idxLwl, reg32);

    // Disable Read and Write Increment.
    reg32 = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_OVERRIDE_INDEX, _READINCR,
                                _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_OVERRIDE_INDEX, _WRITEINCR,
                                _DISABLED, reg32);

    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_OVERRIDE_INDEX, reg32);
}

/*!
 * @brief  Gets the value of slope and offset for the indexed GPC TSENSE sensor.
 *
 * @param[out]   pSlopeA     Value of the slope in normalized form(F0.16).
 * @param[out]   pOffsetB    Value of the offset form(F16.16).
 */
static void
s_thermTestGpcTsenseTempParamsGet_GA10X
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
s_thermTestRawCodeForTempGet_GA10X
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

/*!
 * @brief  Set the GPC raw code override and Send the config.
 *         In GA10X, Rawcode can be overridden per GPC only.
 *
 * @param[in] rawCode   Raw Code override to be set.
 * @param[in] idxGpc    GPC index.
 */
static void
s_thermTestGpcRawCodeSet_GA10X
(
    LwU32   regGpcRawCodeOverride,
    LwU32   rawCode,
    LwU8    idxGpc
)
{
    LwU32 reg32;

    // Select GPC to program GPC RawCode Override and STREAM_RAW_CODE.
    s_thermTestGpcIndexSet_GA10X(idxGpc);

    // Program the Raw Code for the GPC index selected. 
    regGpcRawCodeOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_RAWCODE_OVERRIDE,
                                _VALUE, rawCode,
                                regGpcRawCodeOverride);
    regGpcRawCodeOverride = FLD_SET_DRF(_CPWR_THERM, _GPC_RAWCODE_OVERRIDE,
                                _EN, _YES, regGpcRawCodeOverride);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_RAWCODE_OVERRIDE, regGpcRawCodeOverride);

    // Program STREAM_RAW_CODE_ENABLED.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE, _STREAM_RAW_CODE, _ENABLED,
                                reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE, reg32);

    // Select GPC to program GPC Config Copy.
    thermTestGpcTsenseConfigCopyIndexSet_HAL(&Therm, idxGpc);
    
    // Program CONFIG_SEND to transfer rawcode.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSENSE_CONFIG_COPY);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_CONFIG_COPY, _NEW_SETTING,
                                _SEND, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_CONFIG_COPY, reg32);
}

/*!
 * @brief  Gets the value of GPC TSENSE temp for the selected Index Tsense.
 *
 * @param[in]   bHsTemp       Bool to indicate temperature value with hotspot.
 * @param[in]   ptsenseIdx    Retrieve GPC TSENSE Index for which temp is returned.
 *
 * @return      Temperature value.
 */
static LwTemp
s_thermTestGpcTsenseTempGet_GA10X
(
    LwBool bHsTemp
)
{
    LwTemp  tsenseTemp;

    if (bHsTemp)
    {
        // Get the GPC TSENSE + HS temperature.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_OFFSET_GPC_TSENSE);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_OFFSET_GPC_TSENSE,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_OFFSET_GPC_TSENSE, _FIXED_POINT,
                tsenseTemp);
        }
    }
    else
    {
        // Get the actual GPC TSENSE temperature value.
        tsenseTemp = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE,
                _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE, _FIXED_POINT,
                tsenseTemp);
        }
    }

    return tsenseTemp;
}

/*!
 * @brief  Gets the value of LWL TSENSE temp for the selected Index.
 *
 * @param[in]   bHsTemp       Bool to indicate temperature value with hotspot.
 * @param[in]   ptsenseIdx    Retrieve GPC TSENSE Index for which temp is returned.
 *
 * @return      Temperature value.
 */
static LwTemp
s_thermTestLwlTsenseTempGet_GA10X
(
    LwBool bHsTemp
)
{
    LwTemp  tsenseTemp = THERM_TEST_TEMP_ILWALID;

    if (bHsTemp)
    {
        // Get the LWL TSENSE + HS temperature.
        tsenseTemp = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_OFFSET_LWL_TSENSE);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_OFFSET_LWL_TSENSE, _STATE,
                _VALID, tsenseTemp))
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_OFFSET_LWL_TSENSE, _FIXED_POINT,
                tsenseTemp);
        }
    }
    else
    {
        // Get the actual LWL TSENSE temperature value.
        tsenseTemp = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_LWL_TSENSE);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_LWL_TSENSE, _STATE,
                _VALID, tsenseTemp))
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(_CPWR_THERM,
                           _TEMP_SENSOR_LWL_TSENSE, _FIXED_POINT, tsenseTemp);
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
s_thermTestGpcEnMaskGet_GA10X(void)
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
 * @brief  Get mask of LWL Tsense-s that are enabled. 1: enabled, 0: disabled.
 */
static LwU32
s_thermTestLwlEnMaskGet_GA10X(void)
{
    LwU32 lwlMask;
    LwU32 maxLwlMask;

    //
    // BJT Mask Fuse value interpretation for each bit - 1: disable 0: enable.
    // b0: LWL_TSENSE_0, b1: LWL_TSENSE_0;
    //
    lwlMask = DRF_VAL(_FUSE_OPT, _LWL_TS_DIS_MASK, _DATA,
                            fuseRead(LW_FUSE_OPT_LWL_TS_DIS_MASK));
    maxLwlMask = ((1 << LW_LWL_MAX) - 1);

    return (~lwlMask & maxLwlMask);
}

/*!
 * @brief   Set GPC TSENSE temperature for GPC/BJT selected.
 *
 * @param[in] regGpcTsenseOverride  The original value of TSENSE_OVERRIDE.
 * @param[in] gpuTemp               The temperature to fake in 24.8.
 * @param[in] idxGpc                GPC index.
 * @param[in] idxBjt                BJT index.
 */
static void
s_thermTestGpcTsenseTempSet_GA10X
(
    LwU32   regTempOverride,
    LwTemp  gpuTemp,
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    // Select BJT in GPC Tsense OVERRIDE_INDEX.
    s_thermTestGpcTsenseOverrideIndexSet_GA10X(idxGpc, idxBjt);

    // Fake temperature override.
    regTempOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_OVERRIDE,
        _TEMP_VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp), regTempOverride);
    regTempOverride = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_OVERRIDE,
        _TEMP_SELECT, _OVERRIDE, regTempOverride);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_OVERRIDE, regTempOverride);
}

/*!
 * @brief   Set GPC TSENSE_HS_OFFSET temperature.
 *
 * @param[in] regGpcTsenseOverride  The original value of HS_OFFSET_OVERRIDE.
 * @param[in] hsOffset              The temperature to fake in 24.8.
 * @param[in] idxGpc                GPC index.
 * @param[in] idxBjt                BJT index.
 */
static void
s_thermTestGpcTsenseHsOffsetSet_GA10X
(
    LwU32   regHsOffset,
    LwTemp  hsOffset,
    LwU8    idxGpc,
    LwU8    idxBjt
)
{
    // Select GPC-BJT to program HS Offset.
    s_thermTestGpcTsenseHsOffsetIndexSet_GA10X(idxGpc, idxBjt);
    
    // Program HS Offset.
    regHsOffset = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_HS_OFFSET,
        _FIXED_POINT, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(hsOffset), regHsOffset);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_HS_OFFSET, regHsOffset);
}

/*!
 * @brief   Set LWL TSENSE_HS_OFFSET temperature.
 *
 * @param[in] regLwlTsenseOverride  The original value of HS_OFFSET_OVERRIDE.
 * @param[in] hsOffset              The temperature to fake in 24.8.
 * @param[in] idxLwl                LWL index.
 */
static void
s_thermTestLwlTsenseHsOffsetSet_GA10X
(
    LwU32   regHsOffset,
    LwTemp  hsOffset,
    LwU8    idxLwl
)
{
    // Select LWL Index to program HS Offset.
    s_thermTestLwlTsenseHsOffsetIndexSet_GA10X(idxLwl);
    
    // Program HS Offset.
    regHsOffset = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_TSENSE_HS_OFFSET,
        _FIXED_POINT, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(hsOffset), regHsOffset);

    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_HS_OFFSET, regHsOffset);
}

/*!
 * @brief   Set SYS TSENSE_HS_OFFSET temperature.
 *
 * @param[in] regHsOffset           The original value of HS_OFFSET_OVERRIDE.
 * @param[in] hsOffset              The temperature to fake in 24.8.
 */
static void
s_thermTestSysTsenseHsOffsetSet_GA10X
(
    LwU32   regHsOffset,
    LwTemp  hsOffset
)
{
    // Program HS Offset.
    regHsOffset = FLD_SET_DRF_NUM(_CPWR_THERM, _SYS_TSENSE_HS_OFFSET,
        _FIXED_POINT, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(hsOffset), regHsOffset);

    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET, regHsOffset);
}

/*!
 * @brief  Sets the value and source for DEDICATED_OVERT.
 *         If provider index is max, it sets the source to reserved.
 *
 * @param[in]   regOvert     Register value of DEDICATED_OVERT.
 * @param[in]   dedOvertTemp Value of temperature to set.
 * @param[in]   provIdx      Provider index.
 */
static void
s_thermTestDedicatedOvertSet_GA10X
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
                       _TEMP_SENSOR_ID, _SYS_TSENSE, regOvert);
            break;
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG:
        {
            regOvert = FLD_SET_DRF(_CPWR_THERM, _EVT_DEDICATED_OVERT,
                        _TEMP_SENSOR_ID, _GPU_AVG, regOvert);
            break;
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV__NUM_PROVS:
        {
            regOvert = FLD_SET_DRF(_CPWR_THERM, _EVT_DEDICATED_OVERT,
                        _TEMP_SENSOR_ID, _RSVD, regOvert);
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
s_thermTestGlobalOverrideSet_GA10X
(
    LwU32  regGlobalOverride,
    LwTemp overrideTemp
)
{
    regGlobalOverride = FLD_SET_DRF_NUM(_CPWR_THERM,
                            _TSENSE_GLOBAL_OVERRIDE,
                            _VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(overrideTemp),
                            regGlobalOverride);

    regGlobalOverride = FLD_SET_DRF(_CPWR_THERM,
                            _TSENSE_GLOBAL_OVERRIDE,
                            _EN, _ENABLE,
                            regGlobalOverride);

    REG_WR32(CSB, LW_CPWR_THERM_TSENSE_GLOBAL_OVERRIDE, regGlobalOverride);
}

/*!
 * @brief  Fake SYS TSENSE Temperature.
 *
 * @param[in]   regSysTsenseOverride Register value for SYS Tsense Override.
 * @param[in]   gpuTemp              Temperature to be faked in FXP24.8.
 */
static void
s_thermTestSysTsenseTempSet_GA10X
(
    LwU32  regSysTsenseOverride,
    LwTemp gpuTemp
)
{
    regSysTsenseOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _SYS_TSENSE_OVERRIDE, _TEMP_VALUE,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp), regSysTsenseOverride);
    regSysTsenseOverride = FLD_SET_DRF(_CPWR_THERM, _SYS_TSENSE_OVERRIDE, _TEMP_SELECT,
                            _OVERRIDE, regSysTsenseOverride);

    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_OVERRIDE, regSysTsenseOverride);
}

/*!
 * @brief  Set SensorId and Temp Threshold for EVT_THERMAL_1.
 *
 * @param[in]   regVal      Initial Register value.
 * @param[in]   threshold   Threshold to trigger event.
 * @param[in]   sensorId    sensorId for the event.
 */
static void
s_thermTestThermalEvent1ParamSet_GA10X
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
s_thermTestGlobalSnapshotTrigger_GA10X(void)
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
s_thermTestFilterIntervalTimeUsGet_GA10X(void)
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

/*!
 * @brief Disable Interpolation control. 
 *
 * @param[in]   regVal      Initial Register value.
 */
static void
s_thermTestThermalInterpolationDisable_GA10X
(
    LwU32   regVal
)
{
    regVal = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INTERPOLATION_CTRL,
                _GPU_AVG_SRC, _UNMUNGED, regVal);

    regVal = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INTERPOLATION_CTRL,
                _GPU_MAX_SRC, _UNMUNGED, regVal);

    regVal = FLD_SET_DRF(_CPWR_THERM, _GPC_TSENSE_INTERPOLATION_CTRL,
                _ENABLE, _NO, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_INTERPOLATION_CTRL, regVal);
}

/*!
 * @brief   Set LWL TSENSE temperature for the LWL Index.
 *
 * @param[in] regLwlTsenseOverride  The original value of LWL_TSENSE_OVERRIDE.
 * @param[in] gpuTemp               The temperature to fake in 24.8.
 * @param[in] idxLwl                LWL TSENSE index.
 */
static void
s_thermTestLwlTsenseTempSet_GA10X
(
    LwU32   regTempOverride,
    LwTemp  gpuTemp,
    LwU8    idxLwl
)
{
    // Select the LWL Tsense Index.
    s_thermTestLwlTsenseOverrideIndexSet_GA10X(idxLwl);

    // Fake the temperature.
    regTempOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_TSENSE_OVERRIDE,
                            _TEMP_VALUE,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp),
                            regTempOverride);
    regTempOverride = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_OVERRIDE,
                            _TEMP_SELECT, _OVERRIDE, regTempOverride);

    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_OVERRIDE, regTempOverride);
}

/*!
 * @brief  Set the LWL raw code override and Send the config.
 *         In GA10X, All LWL TSENSEs share same Rawcode.
 *
 * @param[in] regLwlRawcodeOverride Original value of Rawcode override register.
 * @param[in] rawCode               Raw Code override to be set.
 */
static void
s_thermTestLwlRawCodeSet_GA10X
(
    LwU32   regLwlRawCodeOverride,
    LwU32   rawCode
)
{
    LwU32 reg32;

    // Program the Raw Code for LWL TSENSEs. 
    regLwlRawCodeOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _LWL_RAWCODE_OVERRIDE,
                                _VALUE, rawCode,
                                regLwlRawCodeOverride);
    regLwlRawCodeOverride = FLD_SET_DRF(_CPWR_THERM, _LWL_RAWCODE_OVERRIDE,
                                _EN, _YES, regLwlRawCodeOverride);
    REG_WR32(CSB, LW_CPWR_THERM_LWL_RAWCODE_OVERRIDE, regLwlRawCodeOverride);

    // Program STREAM_RAW_CODE_ENABLED.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_LWL_TSENSE);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE, _STREAM_RAW_CODE, _ENABLED,
                                reg32);
    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE, reg32);

    // Program CONFIG_SEND to transfer rawcode.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_LWL_TSENSE_CONFIG_COPY);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _LWL_TSENSE_CONFIG_COPY, _NEW_SETTING,
                                _SEND, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_LWL_TSENSE_CONFIG_COPY, reg32);
}

/*!
 * @brief  Set the SYS TSENSE raw code override.
 *
 * @param[in] regSensor7 Original value of Rawcode override register.
 * @param[in] rawCode    Raw Code override to be set.
 */
static void
s_thermTestSysTsenseRawCodeSet_GA10X
(
    LwU32   regSensor7,
    LwU32   rawCode
)
{
    // Program SYS Tsense RawCode.
    regSensor7 = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OUT,
                                    rawCode, regSensor7);
    regSensor7 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OVERRIDE,
                                    _ENABLE, regSensor7);

    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, regSensor7);
}

/*!
 * @brief  Gets the value of SYS TSENSE SNAPSHOT temp for the selected Index tsense.
 *
 * @param[in]   bHsTemp       Bool to indicate temperature value with hotspot.
 *
 * @return      Temperature value.
 */
static LwTemp
s_thermTestSysTsenseSnapshotTempGet_GA10X
(
    LwBool bHsTemp
)
{
    LwTemp  tsenseTemp;

    if (bHsTemp)
    {
        // Get the GPC TSENSE + HS temperature.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                        _CPWR_THERM, _TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT,
                        _FIXED_POINT, tsenseTemp);
        }
    }
    else
    {
        // Get the actual GPC TSENSE temperature value.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_SYS_TSENSE_SNAPSHOT);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_SYS_TSENSE_SNAPSHOT,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_SYS_TSENSE_SNAPSHOT, _FIXED_POINT,
                tsenseTemp);
        }
    }

    return tsenseTemp;
}

/*!
 * @brief  Gets the value of LWL TSENSE SNAPSHOT temp for the selected Index tsense.
 *
 * @param[in]   bHsTemp       Bool to indicate temperature value with hotspot.
 *
 * @return      Temperature value.
 */
static LwTemp
s_thermTestLwlTsenseSnapshotTempGet_GA10X
(
    LwBool bHsTemp
)
{
    LwTemp  tsenseTemp;

    if (bHsTemp)
    {
        // Get the GPC TSENSE + HS temperature.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                        _CPWR_THERM, _TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT,
                        _FIXED_POINT, tsenseTemp);
        }
    }
    else
    {
        // Get the actual GPC TSENSE temperature value.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_LWL_TSENSE_SNAPSHOT);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_LWL_TSENSE_SNAPSHOT,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_LWL_TSENSE_SNAPSHOT, _FIXED_POINT,
                tsenseTemp);
        }
    }

    return tsenseTemp;
}

/*!
 * @brief  Gets the value of GPC TSENSE SNAPSHOT temp for the selected Index tsense.
 *
 * @param[in]   bHsTemp       Bool to indicate temperature value with hotspot.
 *
 * @return      Temperature value.
 */
static LwTemp
s_thermTestGpcTsenseSnapshotTempGet_GA10X
(
    LwBool bHsTemp
)
{
    LwTemp  tsenseTemp;

    if (bHsTemp)
    {
        // Get the GPC TSENSE + HS temperature.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_OFFSET_GPC_TSENSE_SNAPSHOT);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_OFFSET_GPC_TSENSE_SNAPSHOT,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                        _CPWR_THERM, _TEMP_SENSOR_OFFSET_GPC_TSENSE_SNAPSHOT,
                        _FIXED_POINT, tsenseTemp);
        }
    }
    else
    {
        // Get the actual GPC TSENSE temperature value.
        tsenseTemp = REG_RD32(CSB,
                LW_CPWR_THERM_TEMP_SENSOR_GPC_TSENSE_SNAPSHOT);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE_SNAPSHOT,
                        _STATE, _ILWALID, tsenseTemp))
        {
            tsenseTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsenseTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_TSENSE_SNAPSHOT, _FIXED_POINT,
                tsenseTemp);
        }
    }

    return tsenseTemp;
}

