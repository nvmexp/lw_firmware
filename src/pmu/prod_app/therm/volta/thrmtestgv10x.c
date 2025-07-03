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
 * @file    thrmtestgv10x.c
 * @brief   PMU HAL functions related to therm tests for GV10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

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
/*
 * Following temperature values are used in therm tests to force/compare
 * different temperatures.
 */
#define THERM_TEST_HS_TEMP                            RM_PMU_CELSIUS_TO_LW_TEMP(10)
#define THERM_TEST_HS_TEMP_DELTA                      RM_PMU_CELSIUS_TO_LW_TEMP(13)
#define THERM_TEST_TSOSC_TEMP_CELSIUS                 (50)
#define THERM_TEST_TSOSC_TEMP                         RM_PMU_CELSIUS_TO_LW_TEMP(75)
#define THERM_TEST_CALIBRATION_TEMP                   RM_PMU_CELSIUS_TO_LW_TEMP(85)
#define THERM_TEST_TSENSE_TEMP                        RM_PMU_CELSIUS_TO_LW_TEMP(81)
#define THERM_TEST_DEDICATED_OVERT_TEMP               RM_PMU_CELSIUS_TO_LW_TEMP(70)
#define THERM_TEST_THERM_MON_TSENSE_TEMP              RM_PMU_CELSIUS_TO_LW_TEMP(75)
#define THERM_TEST_THERM_MON_TSENSE_TEMP_DELTA        RM_PMU_CELSIUS_TO_LW_TEMP(5)

// To match with HW, SW should ignore lowest three bits of callwlated result.
#define THERM_TEST_HW_UNSUPPORTED_BIT_MASK  (0xFFFFFFF8)

// Invalid test temperature.
#define THERM_TEST_TEMP_ILWALID             (LW_S32_MAX)

// K0 and K1 is FXP_1.9.
#define THERM_TEST_DYNAMIC_HS_COEFF_K0      (0x100)
#define THERM_TEST_DYNAMIC_HS_COEFF_K1      (0x100)

/*
 * This define contains the software override value of the slope. It is a 10
 * bit positive integer. This define should actually contain "slope << 14",
 * since "slope" is a fraction. For example, if the intended slope is 0.02,
 * the define will be 0.02 * 2^14 = 327, or 0x147.
 */
#define THERM_TEST_SLOPE_A                  (0x147)
#define THERM_TEST_OFFSET_B                 (0x1D28)

// Invalid raw code
#define THERM_TEST_RAW_CODE_ILWALID         (LW_S32_MAX)

// 1 Microsecond delay for use in thermal monitors test
#define THERM_TEST_DELAY_1_MICROSECOND      (1)

/* ------------------------- Types Definitions ----------------------------- */
/*
 * This structure is used to cache all HW values for registers that each
 * test uses and restore them back after each test/subtest completes. Inline
 * comments for each variable to register mapping.
 */
typedef struct
{
    LwU32   regIntrEn;                                           // LW_CPWR_THERM_EVENT_INTR_ENABLE
    LwU32   regUseA;                                             // LW_CPWR_THERM_USE_A
    LwU32   regCoeffIdx;                                         // LW_CPWR_THERM_GPC_TSOSC_COEFF_INDEX
    LwU32   regTsoscIdx;                                         // LW_CPWR_THERM_GPC_TSOSC_INDEX
    LwU32   regEnMask;                                           // LW_CPWR_THERM_GPC_TSOSC_EN_MASK
    LwU32   regSensor7;                                          // LW_CPWR_THERM_SENSOR_7
    LwU32   regSensor6;                                          // LW_CPWR_THERM_SENSOR_6
    LwU32   regDedOvert;                                         // LW_CPWR_THERM_EVT_DEDICATED_OVERT
    LwU32   regGlobalOverride;                                   // LW_CPWR_THERM_GPC_TSOSC_GLOBAL_OVERRIDE
    LwU32   regSensor0;                                          // LW_CPWR_THERM_SENSOR_0
    LwU32   regDynCoeff;                                         // LW_CPWR_THERM_DYNAMIC_HOTSPOT_COEFF
    LwU32   regSensor9;                                          // LW_CPWR_THERM_SENSOR_9
    LwU32   regDynOffset;                                        // LW_CPWR_THERM_DYNAMIC_HOTSPOT_HS_OFFSET
    LwU32   regCalTemp;                                          // LW_CPWR_THERM_GPC_TSOSC_CAL_TEMP
    LwU32   regTsoscOverride[LW_CPWR_THERM_GPC_TSOSC_COUNT];     // LW_CPWR_THERM_GPC_TSOSC_OVERRIDE per TSOSC
    LwU32   regTsoscHs[LW_CPWR_THERM_GPC_TSOSC_COUNT];           // LW_CPWR_THERM_GPC_TSOSC_HS_OFFSET per TSOSC
    LwU32   regTsoscDebug[LW_CPWR_THERM_GPC_TSOSC_COUNT];        // LW_CPWR_THERM_GPC_TSOSC_DEBUG per TSOSC
    LwU32   regSlopeA[LW_CPWR_THERM_GPC_TSOSC_COEFF_COUNT];      // LW_CPWR_THERM_GPC_TSOSC_SLOPE_A per two TSOSCs
    LwU32   regOffsetB[LW_CPWR_THERM_GPC_TSOSC_COEFF_COUNT];     // LW_CPWR_THERM_GPC_TSOSC_OFFSET_B per two TSOSCs
    LwU32   regThermEvt1;                                        // LW_CPWR_THERM_EVT_THERMAL_1
    LwU32   regMonCtrl[LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1]; // LW_CPWR_THERM_INTR_MONITOR_CTRL
    LwU32   regOvertCtrl;                                        // LW_CPWR_THERM_OVERT_CTRL
    LwU32   regOvertEn;                                          // LW_CPWR_THERM_OVERT_EN
    LwU32   regPower6;                                           // LW_CPWR_THERM_POWER_6
} THERM_TEST_REG_CACHE;

THERM_TEST_REG_CACHE  thermRegCache
    GCC_ATTRIB_SECTION("dmem_libThermTest", "thermRegCache");
THERM_TEST_REG_CACHE *pRegCache = NULL;

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS   s_thermTestTsoscExelwte_GV10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscExelwte_GV10X"); 
static FLCN_STATUS   s_thermTestTsoscMaxAvgExelwte_GV10X(RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscMaxAvgExelwte_GV10X");
static void   s_thermTestTsoscRegCacheInit_GV10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscRegCacheInit_GV10X");
static void   s_thermTestTsoscRegCacheRestore_GV10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscRegCacheRestore_GV10X");
static FLCN_STATUS   s_thermTestRegCacheInitRestore_GV10X(LwBool bInit)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestRegCacheInitRestore_GV10X");
static void   s_thermTestDisableSlowdownInterrupt_GV10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDisableSlowdownInterrupt_GV10X");
static LwU32  s_thermTsoscGetRawFromTempInt_GV10X(LwS32 slopeA, LwS32 offsetB, LwTemp temp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTsoscGetRawFromTempInt_GV10X");
static void   s_thermGetTsoscTempParams_GV10X(LwSFXP16_16 *pSlopeA, LwS32 *pOffsetB, LwU8 tsoscIdx)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermGetTsoscTempParams_GV10X");
static LwTemp s_thermTestTsoscTempGet_GV10X(LwBool bHsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscTempGet_GV10X");
static void   s_thermTestTsoscSetRawCode_GV10X(LwU32 rawCode, LwU32 regTsoscDebug)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscSetRawCode_GV10X");
static void   s_thermTestTsoscTempFake_GV10X(LwU32 regTsoscOverride, LwTemp gpuTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsoscTempFake_GV10X");
static void   s_thermDedicatedOvertSet_GV10X(LwU32 regOvert, LwTemp dedOvertTemp, LwU8 provIdx)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermDedicatedOvertSet_GV10X");
static void   s_thermTestEnableTsoscs_GV10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestEnableTsoscs_GV10X");
static void   s_thermTestSetGPCCoeffIdx_GV10X(LwU32 regCoeffIdx, LwU8 idx)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSetGPCCoeffIdx_GV10X");
static void   s_thermTestSetTsoscHotspot_GV10X(LwU32 regTsoscHs, LwTemp hsTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSetTsoscHotspot_GV10X");
static void   s_thermTestSetTsoscGlobalOverride_GV10X(LwU32 regGlobalOverride, LwTemp overrideTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestSetTsoscGlobalOverride_GV10X");
static void   s_thermTestTsenseTempFake_GV10X(LwU32 regSensor0, LwTemp gpuTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestTsenseTempFake_GV10X");
static void   s_thermTestGetCalibrationTemp_GV10X(LwU32 regCalTemp, LwTemp overrideTemp, LwTemp *pCalTemp)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestGetCalibrationTemp_GV10X");
static void   s_thermTestDisableOvertForAllEvents_GV10X(void)
    GCC_ATTRIB_SECTION("imem_thermLibTest", "s_thermTestDisableOvertForAllEvents_GV10X");

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
thermTestIntSensors_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;
    pParams->outData   = 
        LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SUCCESS);
    FLCN_STATUS status;

    // Check TSENSE sensors.
    status = thermTestIntSensors_GP10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
      goto thermTestIntSensors_GV10X_exit;
    }

    // Check TSOSC sensors.
    status = s_thermTestTsoscExelwte_GV10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestIntSensors_GV10X_exit;
    }

    // Test TSOSC_*_MAX, TSOSC_*_AVG, and MAX.
    status = s_thermTestTsoscMaxAvgExelwte_GV10X(pParams);
    if ((pParams->outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS) ||
        (status != FLCN_OK))
    {
        goto thermTestIntSensors_GV10X_exit;
    }

thermTestIntSensors_GV10X_exit:
    return status;
}

/*!
 * @brief  Test DYNAMIC_HOTSPOT hybrid sensor.
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
thermTestDynamicHotspot_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwTemp      tsoscTemp;
    LwTemp      tsenseTemp;
    LwTemp      dynHsTemp;
    LwU32       reg32;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GV10X();

    // Enable TSOSCs.
    s_thermTestEnableTsoscs_GV10X();

    // Fake temperature of all TSOSCs through global override.
    s_thermTestSetTsoscGlobalOverride_GV10X(pRegCache->regGlobalOverride, THERM_TEST_TSOSC_TEMP);

    // Configure sensor 9 to TSOSC_MAX source.
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_MAX, _YES));

    //
    // Read temperature for TSOSC_MAX. Fail if the temperature is
    // not what is faked. Here we can use _AVG too as all TSOSCs are
    // faked to same value.
    //
    tsoscTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX);
    if (tsoscTemp != THERM_TEST_TSOSC_TEMP)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT,
            TSOSC_MAX_FAKING_FAILURE);
        goto thermTestDynamicHotspot_GV10X_exit;
    }

    // Fake the temperature for TSENSE.
    s_thermTestTsenseTempFake_GV10X(pRegCache->regSensor0, THERM_TEST_TSENSE_TEMP);

    //
    // Read temperature for TSENSE. Fail if the temperature is
    // not what is faked.
    //
    tsenseTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);
    if (tsenseTemp != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT,
            TSENSE_FAKING_FAILURE);
        goto thermTestDynamicHotspot_GV10X_exit;
    }

    // Set the dynamic hotspot coefficients.
    reg32 = pRegCache->regDynCoeff;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _DYNAMIC_HOTSPOT_COEFF, _K0,
                                THERM_TEST_DYNAMIC_HS_COEFF_K0, reg32);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _DYNAMIC_HOTSPOT_COEFF, _K1,
                                THERM_TEST_DYNAMIC_HS_COEFF_K1, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_DYNAMIC_HOTSPOT_COEFF, reg32);

    //
    // Callwlate the DYNAMIC_HOTSPOT value using equation 
    // DYNAMIC_HOTSPOT = K0*TSENSE + K1*TSOSC_MAX + K2.
    // All math is done in 24.8 format. K0 and K1 is in FXP_1.9.
    //

    dynHsTemp = ((THERM_TEST_DYNAMIC_HS_COEFF_K0 * tsenseTemp) >> 9) + 
                ((THERM_TEST_DYNAMIC_HS_COEFF_K1 * tsoscTemp) >> 9);

    // Read the dynamic hotspot without hotspot offset from HW and fail if SW and HW values don't match.
    if (dynHsTemp != thermGetTempInternal_HAL(&Therm,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_DYNAMIC_HOTSPOT))
     {
         pParams->outStatus =
             LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
         pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT,
             DYNAMIC_HOTSPOT_FAILURE);
         goto thermTestDynamicHotspot_GV10X_exit;
     }

    // Set hotspot offset for DYNAMIC_HOTSPOT.
    reg32 = pRegCache->regDynOffset;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _DYNAMIC_HOTSPOT_HS_OFFSET,
                            _FIXED_POINT, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_HS_TEMP), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_DYNAMIC_HOTSPOT_HS_OFFSET, reg32);

    // Add the hotspot offset value to the dynamic hotspot value callwlated above in SW.
    dynHsTemp = dynHsTemp + THERM_TEST_HS_TEMP;

    // Read the dynamic hotspot with hotspot offset from HW and fail if SW and HW values don't match.
    if (dynHsTemp != thermGetTempInternal_HAL(&Therm,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_DYNAMIC_HOTSPOT_OFFSET))
    {
         pParams->outStatus =
             LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
         pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT,
             DYNAMIC_HOTSPOT_OFFSET_FAILURE);
         goto thermTestDynamicHotspot_GV10X_exit;
     }

     pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestDynamicHotspot_GV10X_exit:

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Test priority for faking TSOSC temperature
 *         (i.e local vs global vs rawcode override).
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
thermTestTempOverride_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwS32       slopeA;
    LwS32       offsetB;
    LwU32       rawCode;
    LwTemp      tsoscLwrr;
    LwTemp      calTemp;
    LwU8        idx;
    FLCN_STATUS status;
    LwU32       enMask = 0;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GV10X();

    s_thermTestGetCalibrationTemp_GV10X(pRegCache->regCalTemp,
        THERM_TEST_CALIBRATION_TEMP, &calTemp);

    // Enable TSOSCs.
    s_thermTestEnableTsoscs_GV10X();

    // Get the enable mask.
    enMask = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);

    // Loop over all enabled tsoscs to fake the temperature values.
    FOR_EACH_INDEX_IN_MASK(32, idx, enMask)
    {
        // Set GPC index to select tsosc sensor.
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        // Fake the individual tsosc temperature.
        s_thermTestTsoscTempFake_GV10X(pRegCache->regTsoscOverride[idx],
                                      THERM_TEST_TSOSC_TEMP);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Use global override to fake all TSOSC sensors to a value greater than
    // the value used in faking individual TSOSC sensors.
    //
    s_thermTestSetTsoscGlobalOverride_GV10X(pRegCache->regGlobalOverride,
        (THERM_TEST_TSOSC_TEMP + THERM_TEST_HS_TEMP));

    //
    // As all TSOSCs are faked to same value using local/global override MAX/AVG
    // will report same temperature, so using MAX here instead of reading each
    // TSOSC's value individually.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_MAX, _YES));

    //
    // If both global and individual TSOSC temperatures are faked, the
    // temperature for individual TSOSC should take priority.
    //
    if (thermGetTempInternal_HAL(&Therm,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX) != THERM_TEST_TSOSC_TEMP)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
            GLOBAL_OVERRIDE_PRIORITY_FAILURE);
        goto thermTestTempOverride_GV10X_exit;
    }

    // Loop over all enabled tsoscs to fake raw values temperature values.
    FOR_EACH_INDEX_IN_MASK(32, idx, enMask)
    {
        // Set TSOSC index to select correct tsosc sensor.
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        // Set TSOSC coefficient index to get appropriate slope & offset values.
        s_thermTestSetGPCCoeffIdx_GV10X(pRegCache->regCoeffIdx, (idx/2));

        // Get the slope and offset values.
        s_thermGetTsoscTempParams_GV10X(&slopeA, &offsetB, idx);

        //
        // Get the raw value corresponding to the test temperature greater than
        // the one used for local and global override to check priority.
        //
        rawCode = s_thermTsoscGetRawFromTempInt_GV10X(slopeA, offsetB,
                  RM_PMU_CELSIUS_TO_LW_TEMP(THERM_TEST_TSOSC_TEMP +
                                            THERM_TEST_HS_TEMP_DELTA));
        if (rawCode == THERM_TEST_RAW_CODE_ILWALID)
        {
            pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData =
                LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                    RAW_CODE_FAILURE);
            goto thermTestTempOverride_GV10X_exit;
        }

        // Fake the temperature via raw code.
        s_thermTestTsoscSetRawCode_GV10X(rawCode, pRegCache->regTsoscDebug[idx]);

        //
        // Until here as individual, global and rawCode override are set, the local override
        // faking takes priority.
        //
        tsoscLwrr = s_thermTestTsoscTempGet_GV10X(LW_FALSE);
        if (tsoscLwrr != THERM_TEST_TSOSC_TEMP)
        {
             pParams->outStatus =
                 LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
             pParams->outData   =
                 LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
                     TSOSC_RAW_TEMP_FAILURE);
             goto thermTestTempOverride_GV10X_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // If both global and individual TSOSC temperatures are faked, the
    // temperature for individual TSOSC should take priority. Raw code has 
    // lowest priority. If selwre_override is selected then higher of raw 
    // code and override value will be selected.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_MAX, _YES));

    if (thermGetTempInternal_HAL(&Therm,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX) != THERM_TEST_TSOSC_TEMP)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE,
            RAW_OVERRIDE_PRIORITY_FAILURE);
        goto thermTestTempOverride_GV10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestTempOverride_GV10X_exit:

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

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
thermTestThermalMonitors_GV10X
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
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GV10X();

    //
    // Disable filtering of thermal events. No need to filter in the test otherwise
    // need to add delay until filtering is complete and until monitors can start
    // counting on the event.
    //
    reg32 = pRegCache->regPower6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_PERIOD,
                _NONE, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_6, _THERMAL_FILTER_SCALE,
                _16US, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, reg32);

    // Disable overt for Thermal Event 1.
    reg32 = pRegCache->regOvertEn;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_EN, _THERMAL_1, _DISABLED, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, reg32);

    // Configure Thermal Event 1 to trigger on TSENSE sensor.
    reg32 = pRegCache->regThermEvt1;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _EVT_THERMAL_1, _TEMP_SENSOR_ID, _TSENSE, reg32);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_THERMAL_1, _TEMP_THRESHOLD,
                RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_THERM_MON_TSENSE_TEMP), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_THERMAL_1, reg32);

    // Override TSENSE using SW override.
    s_thermTestTsenseTempFake_GV10X(pRegCache->regSensor0,
        THERM_TEST_THERM_MON_TSENSE_TEMP + THERM_TEST_THERM_MON_TSENSE_TEMP_DELTA);

    // Loop over each Therm Monitor
    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        // Clear & Enable the Monitor
        reg32 = pRegCache->regMonCtrl[idx];
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

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

    return FLCN_OK;
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
thermTestDedicatedOvert_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       rawCode;
    LwU32       reg32;
    LwTemp      gpuTemp;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GV10X();

    //
    // Turn on TSENSE and set polling interval as 0, so that we don't
    // have to wait every x ms for the new raw value to be reflected in 
    // TSENSE. Wait for _TS_STABLE_CNT period after sensor is powered 
    // on for a valid value.
    //
    reg32 = pRegCache->regSensor6;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_6, _POWER, _ON, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_6, _POLLING_INTERVAL_PERIOD, _NONE, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, reg32);

    OS_PTIMER_SPIN_WAIT_US(DRF_VAL(_CPWR_THERM, _SENSOR_1, _TS_STABLE_CNT,
        REG_RD32(CSB, LW_CPWR_THERM_SENSOR_1)));

    //
    // Dedicated overt doesn't use _SW_OVERRIDE temperature for any of the temperature sources.
    // It always uses the _REAL values from HW. So we fake raw code values for TSENSE instead
    // of faking output of TSENSE directly via SW_OVERRIDE.
    //
    rawCode = thermGetRawFromTempInt_HAL(&Therm, THERM_TEST_TSENSE_TEMP);

    // Fake the temperature by setting the raw override.
    reg32 = pRegCache->regSensor7;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OUT,
                rawCode, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OVERRIDE,
                _ENABLE, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, reg32);

    // Get the actual tsense value.
    gpuTemp = thermGetTempInternal_HAL(&Therm, 
                LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);
    if (gpuTemp != THERM_TEST_TSENSE_TEMP)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = 
            LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT,
                TSENSE_RAW_MISMATCH_FAILURE);
        goto thermTestDedicatedOvert_GV10X_exit;
    }

    // Enable overt temperature on boot.
    reg32 = pRegCache->regOvertCtrl;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _OTOB_ENABLE, _ON, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, reg32);

    // Set the dedicated overt threshold and source.
    s_thermDedicatedOvertSet_GV10X(pRegCache->regDedOvert,
        THERM_TEST_DEDICATED_OVERT_TEMP,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    //
    // Ideally, we should not reach here as GPU is already off the bus and its
    // an unrecoverable scenario.
    //
    pParams->outStatus =
        LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
    pParams->outData = 
        LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT, SHUTDOWN_FAILURE);

thermTestDedicatedOvert_GV10X_exit:

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Negative testing for dedicated overt. Checks that SW override temperatures
 *         don't trigger a dedicated overt event.
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
thermTestDedicatedOvertNegativeCheck_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       reg32;
    LwU8        idx;
    FLCN_STATUS status;
    LwU32       enMask = 0;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GV10X();

    // Override TSENSE using SW override.
    s_thermTestTsenseTempFake_GV10X(pRegCache->regSensor0, THERM_TEST_TSENSE_TEMP);

    // Enable overt temperature on boot.
    reg32 = pRegCache->regOvertCtrl;
    reg32 = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _OTOB_ENABLE, _ON, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, reg32);

    s_thermDedicatedOvertSet_GV10X(pRegCache->regDedOvert,
        THERM_TEST_DEDICATED_OVERT_TEMP,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    // Check the overt control register for overt assertion state, fail if it's asserted.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES, reg32))
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = 
            LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE,
                SHUTDOWN_FAILURE_TSENSE);
        goto thermTestDedicatedOvertNegativeCheck_GV10X_exit;
    }

    // Enable TSOSCs.
    s_thermTestEnableTsoscs_GV10X();

    //
    // First use global override to fake all TSOSC sensors since local override has higher 
    // priority than global override.
    //
    s_thermTestSetTsoscGlobalOverride_GV10X(pRegCache->regGlobalOverride,
        (THERM_TEST_TSOSC_TEMP + THERM_TEST_HS_TEMP));

    //
    // Set the dedicated overt temp to a value which will be less than
    // OVERRIDE/GLOBAL_OVERRIDE temperature. This is to verify GPU
    // does not shut down if temperature is faked via
    // TSOSC_OVERRIDE/TSOSC_GLOBAL_OVERRIDE. If GPU shuts down, the
    // test has failed. AVG will consider values for all enabled & valid TSOSC readings.
    //
    s_thermDedicatedOvertSet_GV10X(pRegCache->regDedOvert,
        THERM_TEST_DEDICATED_OVERT_TEMP,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);

    // Check the overt control register for overt assertion state, fail if it's asserted.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES, reg32))
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = 
            LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE,
                SHUTDOWN_FAILURE_TSOSC_GLOBAL);
        goto thermTestDedicatedOvertNegativeCheck_GV10X_exit;
    }

    // Get the enable mask.
    enMask = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);

    //
    // Loop over all enabled tsoscs to fake the temperature values. 
    // Use same values for each TSOSC as we don't need different values.
    //
    FOR_EACH_INDEX_IN_MASK(32, idx, enMask)
    {
        // Set GPC index to select tsosc sensor.
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        // Fake the individual tsosc temperature.
        s_thermTestTsoscTempFake_GV10X(pRegCache->regTsoscOverride[idx],
                                      THERM_TEST_TSOSC_TEMP);

        //
        // Check the overt control register for overt assertion state due to 
        // any of the individual TSOSCs, fail if it's asserted.
        //
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
        if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _INT_OVERT_ASSERTED, _YES, reg32))
        {
            pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = 
                LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE,
                    SHUTDOWN_FAILURE_TSOSC_LOCAL);
            goto thermTestDedicatedOvertNegativeCheck_GV10X_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestDedicatedOvertNegativeCheck_GV10X_exit:

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Save/restore the init values of the registers in the reg cache.
 *
 * @param[in]   bInit     Bool to save/restore regcache.
 * 
 * @return      FLCN_OK               regCache save/restore successful.
 * @return      FLCN_ERR_NO_FREE_MEM  No free memory to allocate regCache.
 */
static FLCN_STATUS
s_thermTestRegCacheInitRestore_GV10X
(
    LwBool bInit
)
{
    if (pRegCache == NULL)
    {
        memset(&thermRegCache, 0, sizeof(THERM_TEST_REG_CACHE));
        pRegCache = &thermRegCache;
    }

    if (bInit)
    {
        // Initialise the register cache.
        s_thermTestTsoscRegCacheInit_GV10X();
    }
    else
    {
        s_thermTestTsoscRegCacheRestore_GV10X();
    }

    return FLCN_OK;
}

/*!
 * @brief  Tests TSOSC internal sensors.
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
s_thermTestTsoscExelwte_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwTemp      tsoscTemp;
    LwTemp      testTemp;
    LwTemp      calTemp;
    LwTemp      overtTemp;
    LwU32       rawCode;
    LwU32       rawCodeMin;
    LwU32       rawCodeMax;
    LwSFXP16_16 slopeA;
    LwS32       offsetB;
    LwU8        idx;
    FLCN_STATUS status;
    LwU32       enMask = 0;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Disable slowdown and interrupts so that any temperature changes
    // in the test do not have any side effects.
    //
    s_thermTestDisableSlowdownInterrupt_GV10X();

    // Disable overt for all events.
    s_thermTestDisableOvertForAllEvents_GV10X();

    //
    // Get the range of raw values to be tested. For this,
    // get the dedicated overt threshold temperature value.
    // We need to colwert raw value to temperature as long as the temperature
    // is less than dedicated overt threshold, to ensure chip shutdown does
    // not take place while running the test.
    //
    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    //
    overtTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
        _CPWR_THERM, _EVT_DEDICATED_OVERT, _THRESHOLD,
        REG_RD32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT));

    //
    // Use TSENSE for overt so as not to interfere with test.
    // Generally this is set to GPC_TSOSC_OFFSET_MAX so it triggeres
    // unnecessary overt. This can be set to anything other than the offset
    // ones. Setting to any offseted source will cross the dedicated overt
    // threshold as we set rawCode just shy of overt threshold by one but then
    // we also set offset which will then cross the overt threshold.
    //
    s_thermDedicatedOvertSet_GV10X(pRegCache->regDedOvert,
        overtTemp, LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    //
    // Cache the calibration temperature for callwlating temperature
    // in software.
    //
    s_thermTestGetCalibrationTemp_GV10X(pRegCache->regCalTemp,
        THERM_TEST_CALIBRATION_TEMP, &calTemp);

    // Enable TSOSCs.
    s_thermTestEnableTsoscs_GV10X();

    // Get the enable mask.
    enMask = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);

    FOR_EACH_INDEX_IN_MASK(32, idx, enMask)
    {
        // Set TSOSC index to select correct tsosc sensor.
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        // Set TSOSC coefficient index to get appropriate slope & offset values.
        s_thermTestSetGPCCoeffIdx_GV10X(pRegCache->regCoeffIdx, (idx/2));

        // Get a valid slopeA and offsetB for the TSOSC sensor.
        s_thermGetTsoscTempParams_GV10X(&slopeA, &offsetB, idx);

        rawCodeMax = s_thermTsoscGetRawFromTempInt_GV10X(slopeA, offsetB, overtTemp);
        if (rawCodeMax == THERM_TEST_RAW_CODE_ILWALID)
        {
            pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                   TSOSC_RAW_CODE_MAX_FAILURE);
            goto thermTestTsoscExelwte_GV10X_exit;
        }

        rawCodeMin = s_thermTsoscGetRawFromTempInt_GV10X(slopeA, offsetB, 
                        RM_PMU_CELSIUS_TO_LW_TEMP(THERM_INT_SENSOR_WORKING_TEMP_MIN));
        if (rawCodeMin == THERM_TEST_RAW_CODE_ILWALID)
        {
            pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                   TSOSC_RAW_CODE_MIN_FAILURE);
            goto thermTestTsoscExelwte_GV10X_exit;
        }

        for (rawCode = rawCodeMin; rawCode < rawCodeMax; rawCode++)
        {
            //
            // temp = slope_A * (rawCode - offset_B) + calibration_temp
            // Colwert each raw value into temperature and see if it falls
            // within bounds of valid temp. Mask off 3 LSB since physical 
            // temperature is in 9.5 format.
            //
            testTemp = ((slopeA * ((LwS32)rawCode - offsetB)) >> 8) + calTemp;
            testTemp &= THERM_TEST_HW_UNSUPPORTED_BIT_MASK;

            // Set the raw code in hardware.
            s_thermTestTsoscSetRawCode_GV10X(rawCode, pRegCache->regTsoscDebug[idx]);

            // Get the tsosc temperature from hardware.
            tsoscTemp = s_thermTestTsoscTempGet_GV10X(LW_FALSE);

            //
            // Check if the rawTemp callwlated in SW matches the actual
            // tsosc value. Fail if not.
            //
            if (testTemp != tsoscTemp)
            {
                pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                                   TSOSC_RAW_TEMP_FAILURE);
                goto thermTestTsoscExelwte_GV10X_exit;
            }

            // Set the hotspot offset for the selected tsosc sensor.
            s_thermTestSetTsoscHotspot_GV10X(pRegCache->regTsoscHs[idx], THERM_TEST_HS_TEMP);

            testTemp += THERM_TEST_HS_TEMP;

            // Get the tsosc + HS temperature.
            tsoscTemp = s_thermTestTsoscTempGet_GV10X(LW_TRUE);
            if (testTemp != tsoscTemp) 
            {
                pParams->outStatus =
                     LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
                 pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                     TSOSC_RAW_TEMP_HS_FAILURE);
                 goto thermTestTsoscExelwte_GV10X_exit;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestTsoscExelwte_GV10X_exit:

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Tests MAX, TSOSC_MAX, TSOSC_AVG internal sensors.
 *
 * @param[out]  pParams   Test Params, indicating status of test 
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU
 * @return      FLCN_ERR_NO_FREE_MEM   If there is no free memory
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For 
 *              pass/fail condition, refer to pParams->outStatus.
 */
static FLCN_STATUS
s_thermTestTsoscMaxAvgExelwte_GV10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32       enMask     = 0;
    LwU32       tsoscCount = 0;
    LwS32       maxTemp    = 0;
    LwS32       avgTemp    = 0;
    LwS32       testTemp   = THERM_TEST_TSOSC_TEMP_CELSIUS;
    LwTemp      tsoscTemp;
    LwU8        idx;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_thermTestRegCacheInitRestore_GV10X(LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Setup init HW state for test. Disable all interrupts and slowdowns.
    s_thermTestDisableSlowdownInterrupt_GV10X();

    //
    // Enable same indexed TSOSC(s) as in test _thermTestTsoscExelwte.
    // As this test follows after _thermTestTsoscExelwte, here DO NOT re-enable
    // a disabled TSOSC as its NOT expected in production. We would simply
    // read LW_CPWR_THERM_GPC_TSOSC_EN_MASK (which copies value from fuse) and use it.
    //
    s_thermTestEnableTsoscs_GV10X();

    // Get the enable mask.
    enMask = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);

    //
    // Get the number of TSOSCs enabled. The count won't be zero ever.
    // Check comment in s_thermTestEnableTsoscs_GV10X.
    //
    tsoscCount = enMask;
    NUMSETBITS_32(tsoscCount);

    // Loop over all enabled tsoscs to get max and average temperature values.
    FOR_EACH_INDEX_IN_MASK(32, idx, enMask)
    {
        // Set GPC index to select tsosc sensor.
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        // Set TSOSC Coefficient index
        s_thermTestSetGPCCoeffIdx_GV10X(pRegCache->regCoeffIdx, (idx/2));

        // Fake the tsosc temperature.
        s_thermTestTsoscTempFake_GV10X(pRegCache->regTsoscOverride[idx],
            RM_PMU_CELSIUS_TO_LW_TEMP(testTemp));

        tsoscTemp = s_thermTestTsoscTempGet_GV10X(LW_FALSE);

        if (tsoscTemp != RM_PMU_CELSIUS_TO_LW_TEMP(testTemp))
        {
            pParams->outStatus =
                LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                TSOSC_TEMP_FAKING_FAILURE);
            goto thermTestTsoscMaxAvgExelwte_GV10X_exit;
        }

        // Set the hotspot offset for the selected tsosc sensor.
        s_thermTestSetTsoscHotspot_GV10X(pRegCache->regTsoscHs[idx], THERM_TEST_HS_TEMP);

        maxTemp  = LW_MAX(maxTemp, testTemp);
        avgTemp += testTemp;

        // 
        // Increase temperature by number of TSOSCs enabled to avoid
        // precision loss when callwlating average of all TSOSCs.
        //
        testTemp += tsoscCount;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Callwlate the average TSOSC temp.
    avgTemp = avgTemp / tsoscCount;

    // Get the actual tsosc avg temperature value.
    tsoscTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);
    if (LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(tsoscTemp) != avgTemp)
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
            TSOSC_AVG_FAILURE);
        goto thermTestTsoscMaxAvgExelwte_GV10X_exit;
    }

    // Get the actual tsosc offset avg temperature value.
    tsoscTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_AVG);
    if (LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(tsoscTemp) != 
           (avgTemp + LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(THERM_TEST_HS_TEMP)))
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
            TSOSC_OFFSET_AVG_FAILURE);
        goto thermTestTsoscMaxAvgExelwte_GV10X_exit;
    }

    // Get the actual tsosc max temperature value.
    tsoscTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX);
    if (tsoscTemp != RM_PMU_CELSIUS_TO_LW_TEMP(maxTemp))
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
            TSOSC_MAX_FAILURE);
        goto thermTestTsoscMaxAvgExelwte_GV10X_exit;
    }

    // Get the actual tsosc offset max temperature value.
    tsoscTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);
    if (tsoscTemp != ((RM_PMU_CELSIUS_TO_LW_TEMP(maxTemp) + THERM_TEST_HS_TEMP)))
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
            TSOSC_OFFSET_MAX_FAILURE);
        goto thermTestTsoscMaxAvgExelwte_GV10X_exit;
    }

    //
    // Configure SENSOR_MAX to point to TSOSC_MAX,TSOSC_OFFSET_MAX,
    // TSOSC_OFFSET_AVG and TSOSC_AVG. This is done, as, until this point 
    // we have only checked for individual MAX/AVG/MAX+hotspot/AVG+hotspot
    // This ensures SENSOR_9_MAX actually touches all registers and returns
    // correct value.
    //
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_MAX, _YES) |
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_AVG, _YES) |
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_OFFSET_MAX, _YES) |                    
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _GPC_TSOSC_OFFSET_AVG, _YES));

    // Check if _SENSOR_MAX reflects correct value.
    tsoscTemp = thermGetTempInternal_HAL(&Therm,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);
    if (tsoscTemp != thermGetTempInternal_HAL(&Therm,
           LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX))
    {
        pParams->outStatus =
            LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
            MAX_VALUE_FAILURE);
        goto thermTestTsoscMaxAvgExelwte_GV10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestTsoscMaxAvgExelwte_GV10X_exit:

    s_thermTestRegCacheInitRestore_GV10X(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Save the init values of the registers in the reg cache.
 */
static void
s_thermTestTsoscRegCacheInit_GV10X()
{
    LwU8  idx;

    // Attach and Load _IMEM overlay.
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(thermLibSensor2X));

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        pRegCache->regMonCtrl[idx] = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx));
    }

    pRegCache->regThermEvt1      = REG_RD32(CSB, LW_CPWR_THERM_EVT_THERMAL_1);
    pRegCache->regIntrEn         = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    pRegCache->regUseA           = REG_RD32(CSB, LW_CPWR_THERM_USE_A);
    pRegCache->regCoeffIdx       = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_COEFF_INDEX);
    pRegCache->regTsoscIdx       = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_INDEX);
    pRegCache->regEnMask         = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);
    pRegCache->regSensor7        = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_7);
    pRegCache->regSensor6        = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_6);
    pRegCache->regDedOvert       = REG_RD32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT);
    pRegCache->regGlobalOverride = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_GLOBAL_OVERRIDE);
    pRegCache->regSensor0        = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_0);
    pRegCache->regDynCoeff       = REG_RD32(CSB, LW_CPWR_THERM_DYNAMIC_HOTSPOT_COEFF);
    pRegCache->regSensor9        = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_9);
    pRegCache->regDynOffset      = REG_RD32(CSB, LW_CPWR_THERM_DYNAMIC_HOTSPOT_HS_OFFSET);
    pRegCache->regCalTemp        = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_CAL_TEMP);
    pRegCache->regOvertCtrl      = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);
    pRegCache->regOvertEn        = REG_RD32(CSB, LW_CPWR_THERM_OVERT_EN);
    pRegCache->regPower6         = REG_RD32(CSB, LW_CPWR_THERM_POWER_6);

    // Cache all registers indexed by TSOSC_INDEX.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_GPC_TSOSC_SENSOR_MASK)
    {
        //
        // Set the correct TSOSC index to cache all the indirectly mapped 
        // registers via TSOSC_INDEX.
        //
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        pRegCache->regTsoscOverride[idx] = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_OVERRIDE);
        pRegCache->regTsoscHs[idx]       = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_HS_OFFSET);
        pRegCache->regTsoscDebug[idx]    = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_DEBUG);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Cache all registers indexed by TSOSC_COEFF_INDEX .
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_GPC_TSOSC_COEFF_SENSOR_MASK)
    {
        //
        // Set the correct TSOSC_COEFF index to cache all the indirectly mapped 
        // registers via TSOSC_COEFF_INDEX.
        //
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_COEFF_INDEX, idx);

        pRegCache->regSlopeA[idx] = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_SLOPE_A);
        pRegCache->regOffsetB[idx] = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_OFFSET_B);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief  Restore the registers from the reg cache.
 */
static void
s_thermTestTsoscRegCacheRestore_GV10X()
{
    LwU8  idx;

    REG_WR32(CSB, LW_CPWR_THERM_POWER_6, pRegCache->regPower6);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, pRegCache->regOvertCtrl);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_CAL_TEMP, pRegCache->regCalTemp);
    REG_WR32(CSB, LW_CPWR_THERM_DYNAMIC_HOTSPOT_HS_OFFSET, pRegCache->regDynOffset);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9, pRegCache->regSensor9);
    REG_WR32(CSB, LW_CPWR_THERM_DYNAMIC_HOTSPOT_COEFF, pRegCache->regDynCoeff);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_0, pRegCache->regSensor0);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_GLOBAL_OVERRIDE, pRegCache->regGlobalOverride);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_DEDICATED_OVERT, pRegCache->regDedOvert);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_6, pRegCache->regSensor6);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, pRegCache->regSensor7);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK, pRegCache->regEnMask);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, pRegCache->regUseA);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, pRegCache->regIntrEn);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_THERMAL_1, pRegCache->regThermEvt1);

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), pRegCache->regMonCtrl[idx]);
    }

    // Restore all registers indexed by TSOSC_COEFF_INDEX.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_GPC_TSOSC_COEFF_SENSOR_MASK)
    {
        //
        // Set the correct TSOSC_COEFF index to restore all the indirectly mapped 
        // registers via TSOSC_COEFF_INDEX.
        //
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_COEFF_INDEX, idx);

        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_SLOPE_A, pRegCache->regSlopeA[idx]);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_OFFSET_B, pRegCache->regOffsetB[idx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Restore all registers indexed by TSOSC_INDEX.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_GPC_TSOSC_SENSOR_MASK)
    {
        //
        // Set the correct TSOSC index to restore all the indirectly mapped 
        // registers via TSOSC_INDEX.
        //
        thermGpuGpcTsoscIdxSet_HAL(&Therm, idx);

        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_OVERRIDE, pRegCache->regTsoscOverride[idx]);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_HS_OFFSET, pRegCache->regTsoscHs[idx]);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_DEBUG, pRegCache->regTsoscDebug[idx]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // In the end restore the TSOSC_INDEX and TSOSC_COEFF_INDEX.
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_INDEX, pRegCache->regTsoscIdx);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_COEFF_INDEX, pRegCache->regCoeffIdx);

    // In the end restore OVERT_EN so as not to trigger overt accidentally.
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, pRegCache->regOvertEn);

    // Detach _IMEM overlay.
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(thermLibSensor2X));
}

/*!
 * @brief  Set the raw code override.
 *
 * @param[in]   rawCode       The raw code override to be set.
 * @param[in]   regTsoscDebug The register value to override.
 */
static void
s_thermTestTsoscSetRawCode_GV10X
(
    LwU32  rawCode,
    LwU32  regTsoscDebug
)
{
    // Fake the temperature by setting the raw override.
    regTsoscDebug = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_DEBUG,
                        _RAW_TEMP_VALUE, rawCode, regTsoscDebug);
    regTsoscDebug = FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_DEBUG,
                        _RAW_TEMP_OVERRIDE, _YES, regTsoscDebug);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_DEBUG, regTsoscDebug);
}

/*!
 * @brief   Fake the TSOSC temperature.
 *
 * @param[in]   regTsoscOverride  The original value of TSOSC_OVERRIDE.
 * @param[in]   gpuTemp           The temperature to fake.
 */
static void
s_thermTestTsoscTempFake_GV10X
(
    LwU32  regTsoscOverride,
    LwTemp gpuTemp
)
{
    regTsoscOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_OVERRIDE,
       _TEMP_VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp), regTsoscOverride);
    regTsoscOverride = FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_OVERRIDE,
      _TEMP_SELECT, _OVERRIDE, regTsoscOverride);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_OVERRIDE, regTsoscOverride);
}

/*!
 * @brief  Disable interrupts and slowdowns.
 *
 * @param[in]   regIntrEn  Value of register *INTR_EN.
 * @param[in]   regUseA    Value of the register *USE_A.
 */
static void
s_thermTestDisableSlowdownInterrupt_GV10X()
{
    LwU32 idx;
    LwU32 regIntrEn = pRegCache->regIntrEn;
    LwU32 regUseA   = pRegCache->regUseA;

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
 * @brief  Gets the value of slope and offset for the indexed TSOSC sensor.
 *
 * @param[out]   pSlopeA     Value of the slope in normalized form(F0.16).
 * @param[out]   pOffsetB    Value of the offset.
 * @param[in]    tsoscIdx    GPC index of the TSOSC sensor.
 */
static void
s_thermGetTsoscTempParams_GV10X
(
    LwSFXP16_16 *pSlopeA,
    LwS32       *pOffsetB,
    LwU8         tsoscIdx
)
{
    LwU32   regSlopeA;
    LwU32   regOffsetB;
    LwU32   reg32;
    LwU8    coeffIdx;

    *pOffsetB = 0;
    *pSlopeA  = 0;
    coeffIdx  = tsoscIdx / 2;

    regSlopeA  = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_SLOPE_A);
    regOffsetB = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_OFFSET_B);

    // Get the slope and offset for the selected TSOSC sensor.
    if ((tsoscIdx % 2) == 0)
    {
        *pSlopeA  = (LwS32)DRF_VAL(_CPWR_THERM, _GPC_TSOSC_SLOPE_A,
                       _VALUE0, regSlopeA);
        *pOffsetB = (LwS32) DRF_VAL(_CPWR_THERM, _GPC_TSOSC_OFFSET_B,
                       _VALUE0, regOffsetB);
    }
    else
    {
        *pSlopeA  = (LwS32)DRF_VAL(_CPWR_THERM, _GPC_TSOSC_SLOPE_A,
                       _VALUE1, regSlopeA);
        *pOffsetB = (LwS32) DRF_VAL(_CPWR_THERM, _GPC_TSOSC_OFFSET_B,
                       _VALUE1, regOffsetB);
    }

    //
    // SLOPE_A contains the slope value. It's a 10 bit positive integer.
    // The real slope value is actually SLOPE_A << 14 since slope is a
    // fractional value. The ten bit register can represent a slope value
    // between 0~0.0624. A typical slope value is less than 0.04. Hence if
    // slopeA is zero, set it to 0.02.
    //

    if (*pSlopeA == LW_TYPES_FXP_ZERO)
    {
        reg32 = regSlopeA;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_SLOPE_A, _VALUE0,
                                    THERM_TEST_SLOPE_A, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_SLOPE_A, _VALUE1,
                                    THERM_TEST_SLOPE_A, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_SLOPE_A, reg32);
       *pSlopeA = THERM_TEST_SLOPE_A;
    }
   *pSlopeA = *pSlopeA << (16-14);

    // Set a proper offset so that the resultant raw value is a positive number.
    if (*pOffsetB == LW_TYPES_FXP_ZERO)
    {
        reg32 = regOffsetB;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_OFFSET_B, _VALUE0,
                                THERM_TEST_OFFSET_B, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_OFFSET_B, _VALUE1,
                                THERM_TEST_OFFSET_B, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_OFFSET_B, reg32);
       *pOffsetB = THERM_TEST_OFFSET_B;
    }
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
static LwU32
s_thermTsoscGetRawFromTempInt_GV10X
(
    LwSFXP16_16 slopeA,
    LwS32       offsetB,
    LwTemp      temp
)
{
    LwS32       rawCode;
    LwTemp      calTemp;
    LwSFXP16_16 tmp;

    //
    // We need a slope to proceed. If its "0" simply bail out
    // (and return "0").
    //
    if (slopeA == LW_TYPES_FXP_ZERO)
    {
        rawCode = THERM_TEST_RAW_CODE_ILWALID;
        goto s_thermTsoscGetRawFromTempInt_GV10X_exit;
    }

    //
    // All math is done in F16.16 format. Since we are using normalized A value
    // temperature in [C] is callwlated as: Temp = slope_A * (rawCode - offset_B) + calibration_temp.
    // We are using ilwerse formula rawCode = (((Temp - calibration_temp))/A) + B)
    // corrected by adding (A-1) to compensate for integer arithmetics error,
    // that will happen in HW.
    //
       
    // 
    // Get the calibration temperature value. 
    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    //
    calTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
        _CPWR_THERM, _GPC_TSOSC_CAL_TEMP, _VALUE,
        REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_CAL_TEMP));

    tmp     = (LwSFXP16_16)((temp - calTemp) << 8);
    rawCode = LW_DIV_AND_CEIL(tmp, slopeA);
    rawCode += offsetB;

    if (rawCode < 0)
    {
        rawCode = THERM_TEST_RAW_CODE_ILWALID;
    }

s_thermTsoscGetRawFromTempInt_GV10X_exit:
    return rawCode;
}

/*!
 * @brief  Gets the value of TSOSC temperature.
 *
 * @param[in]   bHsTemp    Bool to indicate temperature value with hotspot.
 * 
 * @return      Temperature value.
 */
static LwTemp
s_thermTestTsoscTempGet_GV10X
(
    LwBool bHsTemp
)
{
    LwTemp tsoscTemp;

    if (bHsTemp)
    {
        // Get the tsosc + HS temperature.
        tsoscTemp = REG_RD32(CSB,
            LW_CPWR_THERM_TEMP_SENSOR_GPC_TSOSC_OFFSET_LWRR);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSOSC_OFFSET_LWRR,
                        _STATE, _ILWALID, tsoscTemp))
        {
            tsoscTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsoscTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_TSOSC_OFFSET_LWRR, _FIXED_POINT,
                tsoscTemp);
        }
    }
    else
    {
        // Get the actual tsosc temperature value.
        tsoscTemp = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSOSC_LWRR);
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSOSC_LWRR,
                        _STATE, _ILWALID, tsoscTemp))
        {
            tsoscTemp = THERM_TEST_TEMP_ILWALID;
        }
        else
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            tsoscTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                _CPWR_THERM, _TEMP_SENSOR_GPC_TSOSC_LWRR, _FIXED_POINT,
                tsoscTemp);
        }
    }
    return tsoscTemp;
}

/*!
 * @brief  Sets the value and source for DEDICATED_OVERT
 *
 * @param[in]   regOvert     Register value of DEDICATED_OVERT.
 * @param[in]   dedOvertTemp Value of temperature to set.
 * @param[in]   provIdx      Provider index.
 */
static void
s_thermDedicatedOvertSet_GV10X
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
                       _TEMP_SENSOR_ID, _GPC_TSOSC_AVG, regOvert);
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
 * @brief  Conditionally enable all TSOSCs.
 */
static void
s_thermTestEnableTsoscs_GV10X(void)
{
    LwU32 reg32;
    LwU8  idx;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);
    NUMSETBITS_32(reg32);

    //
    // If all TSOSCs are disabled, enable all TSOSCs.
    // We do not change the EN_MASK which if it is populated from fuse by VBIOS.
    // All TSOSCs are not expected to be disabled by HW hence it's safe to
    // assume that the EN_MASK won't be zero when populated from fuse.
    //
    if (reg32 == 0)
    {
        FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_GPC_TSOSC_SENSOR_MASK)
        {
           reg32 = FLD_IDX_SET_DRF(_CPWR_THERM, _GPC_TSOSC_EN_MASK, _CTRL,
                                      idx, _YES, reg32);
        }
        FOR_EACH_INDEX_IN_MASK_END;
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK, reg32);
    }
}

/*!
 * @brief  Sets GPC TSOSC coefficient index.
 *
 * @param[in]   regCoeffIdx Register value of GPC tsosc coefficient index.
 * @param[in]   idx         Coefficient index to be set.
 */
static void
s_thermTestSetGPCCoeffIdx_GV10X
(
    LwU32 regCoeffIdx,
    LwU8  idx
)
{
    // Set TSOSC coefficient index to get appropriate slope & offset values.
    regCoeffIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_COEFF_INDEX,
                        _VALUE, idx, regCoeffIdx);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_COEFF_INDEX, regCoeffIdx);
}

/*!
 * @brief  Sets GPC TSOSC hostspot offset.
 *
 * @param[in]   regTsoscHs Register value of Tsosc hotspot offset.
 * @param[in]   hsTemp     Hotspot value to be set for TSOSC in FXP24.8.
 */
static void
s_thermTestSetTsoscHotspot_GV10X
(
    LwU32  regTsoscHs,
    LwTemp hsTemp
)
{
    // Set the hotspot offset for the selected tsosc sensor.
    regTsoscHs = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_HS_OFFSET, _FIXED_POINT,
                        RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(hsTemp), regTsoscHs);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_HS_OFFSET, regTsoscHs);
}

/*!
 * @brief  Sets global override temperature for TSOSCs.
 *
 * @param[in]   regGlobalOverride Register value for global override.
 * @param[in]   overrideTemp      Global override temperature to be set in FXP24.8.
 */
static void
s_thermTestSetTsoscGlobalOverride_GV10X
(
    LwU32  regGlobalOverride,
    LwTemp overrideTemp
)
{
    regGlobalOverride = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_GLOBAL_OVERRIDE,
                            _TEMP_VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(overrideTemp), regGlobalOverride);
    regGlobalOverride = FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_GLOBAL_OVERRIDE,
                            _TEMP_SELECT, _OVERRIDE, regGlobalOverride);
    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_GLOBAL_OVERRIDE, regGlobalOverride);
}

/*!
 * @brief  Fake output of tsense.
 *
 * @param[in]   regSensor0 Register value for SENSOR_0.
 * @param[in]   gpuTemp    Temperature to be faked in FXP24.8.
 */
static void
s_thermTestTsenseTempFake_GV10X
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
 * @brief  Get the calibration temperature, set if it's zero.
 *
 * @param[in]   regCalTemp   Register value for SENSOR_0.
 * @param[in]   overrideTemp Calibration temperature to be set in FXP24.8.
 * @param[out]  pCalTemp     Value of calibration temperature in FXP24.8
 */
static void
s_thermTestGetCalibrationTemp_GV10X
(
    LwU32   regCalTemp,
    LwTemp  overrideTemp,
    LwTemp *pCalTemp
)
{
    if (FLD_TEST_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_CAL_TEMP, _VALUE, 0, regCalTemp))
    {
        regCalTemp = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_CAL_TEMP, _VALUE,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(overrideTemp), regCalTemp);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_CAL_TEMP, regCalTemp);
    }

    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    *pCalTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
        _CPWR_THERM, _GPC_TSOSC_CAL_TEMP, _VALUE, regCalTemp);
}

/*!
 * @brief  Disable overt for all events.
 */
static void
s_thermTestDisableOvertForAllEvents_GV10X()
{
    LwU8  idx;
    LwU32 regOvertEn = pRegCache->regOvertEn;

    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_TRIGGER)
    {
        regOvertEn = FLD_IDX_SET_DRF(_CPWR_THERM, _OVERT_EN, _CTRL,
                        idx, _DISABLED, regOvertEn);
    }
    FOR_EACH_INDEX_IN_MASK_END;
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, regOvertEn);
}
