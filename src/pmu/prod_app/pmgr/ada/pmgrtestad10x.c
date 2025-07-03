/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmgrtestad10x.c
 * @brief   PMU HAL functions related to PMGR tests for AD10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmu_objpmu.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define PMGR_TEST_ADC_VAL_UNCORRECTED                                   (0x58)
#define PMGR_TEST_ADC_VAL_UNCORRECTED_IPC                               (0x90)
#define PMGR_TEST_ADC_STEP_PERIOD                                        (0x1)
#define PMGR_TEST_ADC_SHIFT_FACTOR_FXP                                    (21)
#define PMGR_TEST_ADC_PWM_PERIOD                                        (0x28)
#define PMGR_TEST_ADC_PWM_HI                                            (0x14)
#define PMGR_TEST_ADC_SAMPLE_DELAY                                        (10)
#define PMGR_TEST_ADC_LIMIT_MIN_IPC                                       (64)
#define PMGR_TEST_ADC_LIMIT_MAX_IPC                                      (127)
#define PMGR_TEST_ADC_STEP_PERIOD_IPC                                     (15)
#define PMGR_TEST_ADC_IIR_VALUE_INDEX_INDEX_MAX                           (21)

// IPC Macros

#define PMGR_TEST_PER_IPC_PARAM_COUNT                                     (17)

#define PMGR_TEST_IPC_CTRL_DATA_VALUE_AX0_AY_DS                   (0x00000001)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_AX1_AX2                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_AY1_AY2                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_A_FLOOR                     (0x00080000)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_A_CEIL                      (0x0007ffff)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_BX0_BY_DS                   (0x00000001)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_BX1_BX2                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_BY1_BY2                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_B_FLOOR                     (0x00080000)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_B_CEIL                      (0x0007ffff)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_CX0_CY_DS_IPC_DS            (0x000d0000)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_CX1_CX2                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_CY1_CY2                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_C_FLOOR                            (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_C_CEIL                      (0x0007ffff)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_LAST_FLOOR_C_TARGET                (0x0)
#define PMGR_TEST_IPC_CTRL_DATA_VALUE_LAST_CEIL_INTEGRATOR        (0x0007ffff)

#define PMGR_TEST_IPC_CTRL_FA_INDEX_INDEX_VID_DOWNSHIFT           (0x00000009)

#define PMGR_TEST_IPC_REF_VAL_IPC_0_SINGLE_IPC_CH                 (0x07000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_0_THREE_IPC_CH                  (0x07000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_1_THREE_IPC_CH                  (0x06000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_2_THREE_IPC_CH                  (0x05000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_0_SINGLE_IPC_CHP                (0x00240000)
#define PMGR_TEST_IPC_REF_VAL_IPC_0_THREE_IPC_CHP                 (0x00240000)
#define PMGR_TEST_IPC_REF_VAL_IPC_1_THREE_IPC_CHP                 (0x00140000)
#define PMGR_TEST_IPC_REF_VAL_IPC_2_THREE_IPC_CHP                 (0x00040000)
#define PMGR_TEST_IPC_REF_VAL_IPC_0_SINGLE_IPC_SUM                (0x0a000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_0_THREE_IPC_SUM_SUM_CH          (0x0b000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_1_THREE_IPC_SUM_SUM_CH          (0x01000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_2_THREE_IPC_SUM_SUM_CH          (0x0b000000)

#define PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CH                        (24)
#define PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_CH                         (40)
#define PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CHP                        (8)
#define PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_CHP                        (12)
#define PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_SUM                        (5)
#define PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_SUM_SUM_CH                  (4)

#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT1_SINGLE_IPC_SUM                  (1)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT2_SINGLE_IPC_SUM                  (1)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT3_SINGLE_IPC_SUM                (255)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT4_SINGLE_IPC_SUM                 (16)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT1_THREE_IPC_SUM_SUM_CH            (1)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT2_THREE_IPC_SUM_SUM_CH            (1)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT3_THREE_IPC_SUM_SUM_CH          (255)
#define PMGR_TEST_SUM1_SCALE_FACTOR_INPUT4_THREE_IPC_SUM_SUM_CH           (16)
#define PMGR_TEST_SUM2_SCALE_FACTOR_INPUT1_THREE_IPC_SUM_SUM_CH          (255)
#define PMGR_TEST_SUM2_SCALE_FACTOR_INPUT2_THREE_IPC_SUM_SUM_CH          (255)
#define PMGR_TEST_SUM2_SCALE_FACTOR_INPUT3_THREE_IPC_SUM_SUM_CH          (255)
#define PMGR_TEST_SUM2_SCALE_FACTOR_INPUT4_THREE_IPC_SUM_SUM_CH          (255)

#define PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_SINGLE_IPC_CH                      (8)
#define PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_THREE_IPC_CH                       (8)
#define PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_SINGLE_IPC_CHP                     (6)
#define PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_THREE_IPC_CHP                      (6)
#define PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_SINGLE_IPC_SUM                     (8)
#define PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_THREE_IPC_SUM_SUM_CH               (8)

#define PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET                       (0x0)

// Beacon Macros
#define PMGR_TEST_BEACON1_INDEX                                            (0)
#define PMGR_TEST_BEACON2_INDEX                                            (1)
#define PMGR_TEST_BEACON_THRESHOLD_LOW                                  (0x16)
#define PMGR_TEST_BEACON_THRESHOLD_HIGH                                 (0x32)

// Offset Macros
#define PMGR_TEST_OFFSET1_INDEX                                            (0)
#define PMGR_TEST_OFFSET2_INDEX                                            (1)
#define PMGR_TEST_OFFSET1_OFFSET_MAP_INDEX                                 (5)
#define PMGR_TEST_OFFSET2_OFFSET_MAP_INDEX                                (14)
#define PMGR_TEST_OFFSET1_IIR_ACC_VALUE_POST_OFFSET                      (0x0)
#define PMGR_TEST_OFFSET2_IIR_ACC_VALUE_POST_OFFSET                      (0x0)
#define PMGR_TEST_OFFSET1_MUL_VALUE_POST_OFFSET                          (0x0)
#define PMGR_TEST_OFFSET2_MUL_VALUE_POST_OFFSET                          (0x0)
#define PMGR_TEST_OFFSET1_MUL_ACC_VALUE_POST_OFFSET                      (0x0)
#define PMGR_TEST_OFFSET2_MUL_ACC_VALUE_POST_OFFSET                      (0x0)

// Delay macros

#define PMGR_TEST_DELAY_1_MILLISECOND                                   (1000)
#define PMGR_TEST_DELAY_200_MICROSECOND                                  (200)

/* ------------------------- Types Definitions ------------------------------ */
/*
 * This structure is used to cache all HW values for registers that each
 * test uses and restore them back after each test/subtest completes. Inline
 * comments for each variable to register mapping.
 */
typedef struct
{
    LwU32   regAdcDebug;
    LwU32   regAdcDebug2;
    LwU32   regIpcDebug;
    LwU32   regMulCtrlIdx;
    LwU32   regMulCtrlData[LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX + 1];
    LwU32   regAdcCtrl;
    LwU32   regAdcSnapshot;
    LwU32   regAdcCtrl2;
    LwU32   regAdcCtrl3;
    LwU32   regAdcCtrl4;
    LwU32   regIIRValIdx;
    LwU32   regAccValIdx;
    LwU32   regMulValIdx;
    LwU32   regMulAccValIdx;
    LwU32   regAdcPwm;
    LwU32   regAdcPwmGen;
    LwU32   regIntrEn0;
    LwU32   regIntr0;
    LwU32   regAdcOffsetMap;
    LwU32   regAdcOffsetMap2;
    LwU32   regIpcCtrl[LW_CPWR_THERM_IPC_CTRL__SIZE_1];
    LwU32   regIpcRef[LW_CPWR_THERM_IPC_REF__SIZE_1];
    LwU32   regIpcRefBound[LW_CPWR_THERM_IPC_REF_BOUND__SIZE_1];
    LwU32   regIpcRefBoundCeil[LW_CPWR_THERM_IPC_REF_BOUND_CEIL__SIZE_1];
    LwU32   regIpcRefBoundFloor[LW_CPWR_THERM_IPC_REF_BOUND_FLOOR__SIZE_1];
    LwU32   regAdcSwReset;
    LwU32   regIpcCtrlIdx;
    LwU32   regIpcCtrlData;
    LwU32   regIpcCtrlSum1Input;
    LwU32   regIpcCtrlSum2Input;
    LwU32   regIpcCtrlSum1Sf;
    LwU32   regIpcCtrlSum2Sf;
} PMGR_TEST_REG_CACHE,
*PPMGR_TEST_REG_CACHE;

PMGR_TEST_REG_CACHE  pmgrRegCache
    GCC_ATTRIB_SECTION("dmem_libPmgrTest", "pmgrRegCache");
PPMGR_TEST_REG_CACHE pPmgrRegCache = NULL;

/* ------------------------- Public Function Prototypes  -------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static void s_pmgrTestADCDebugMinLimitSet(LwU8 minLimit)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCDebugMinLimitSet");
static void s_pmgrTestADCDebugMaxLimitSet(LwU8 maxLimit)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCDebugMaxLimitSet");
static void s_pmgrTestADCDebugStepPeriodSet(LwU8 stepPeriod)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCDebugStepPeriodSet");
static void s_pmgrTestADCDebugModeSet(LwU8 mode, LwU8 overrideVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCDebugModeSet");
static void s_pmgrTestADCSensingEnable(LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCSensingEnable");
static void s_pmgrTestADCAclwmulationEnable(LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCAclwmulationEnable");
static void s_pmgrTestADCValuesSnap(void)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCValuesSnap");
static void s_pmgrTestADCNumActiveChannelsSet(LwU8 numChannels)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCNumActiveChannelsSet");
static void s_pmgrTestADCIIRLengthSet(LwU8 iirLength)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCIIRLengthSet");
static void s_pmgrTestADCSampleDelaySet(LwU16 sampleDelay)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCSampleDelaySet");
static void s_pmgrTestADCPWMConfigure(LwU16 period, LwU16 dutyCycle)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCPWMConfigure");
static void s_pmgrTestADCPWMGenConfigure(LwBool bEnable, LwU16 dutyCycleLast)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCPWMGenConfigure");
static void s_pmgrTestADCReset(void)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCReset");
static void s_pmgrTestIIRValForChannelGet(LwU8 chIdx, LwU32 *pValue)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIIRValForChannelGet");
static void s_pmgrTestAccIIRValForChannelGet(LwU8 chIdx, LwU32 *pLB, LwU32 *pUB, LwU32 *pSCnt)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestAccIIRValForChannelGet");
static void s_pmgrTestMulIIRValForChannelPairGet(LwU8 chPairIdx, LwU32 *pValue)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestMulIIRValForChannelPairGet");
static void s_pmgrTestAccMulIIRValForChannelPairGet(LwU8 chPairIdx, LwU32 *pLB, LwU32 *pUB, LwU32 *pSCnt)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestAccMulIIRValForChannelPairGet");
static void s_pmgrTestChannelPairConfigure(LwU8 chPairIdx, LwU8 operand1, LwU8 operand2, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestChannelPairConfigure");
static void s_pmgrTestIPCChannelSet(LwU8 ipcIdx, LwU8 chIdx)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCChannelSet");
static void s_pmgrTestIPCParamsSet(LwU8 ipcIdx)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCParamsSet");
static void s_pmgrTestIPCEnable(LwU8 ipcIdx, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCEnable");
static void s_pmgrTestIPCRefValSet(LwU8 ipcIdx, LwU32 refVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCRefValSet");
static void s_pmgrTestIpcDebugValueSnap(LwU8 selIdx, LwU16 *pSelVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIpcDebugValueSnap");
static void s_pmgrTestADCBeaconChannelSet(LwU8 beaconChannelIdx, LwU8 beaconChannelVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCBeaconChannelSet");
static void s_pmgrTestADCBeaconThresholdSet(LwU8 beaconChannelIdx, LwU8 beaconChannelThreshold)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCBeaconThresholdSet");
static void s_pmgrTestADCBeaconComparatorSet(LwU8 beaconChannelIdx, LwBool bGreaterThan)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCBeaconComparatorSet");
static void s_pmgrTestADCBeaconInterruptEnable(LwU8 beaconChannelIdx, LwBool bInterruptEnable)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCBeaconInterruptEnable");
static void s_pmgrTestADCBeaconInterruptClear(LwU8 beaconChannelIdx)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCBeaconInterruptClear");
static void s_pmgrTestADCOffsetChannelSet(LwU8 offsetChannelIdx, LwU8 offsetChannelVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCOffsetChannelSet");
static void s_pmgrTestADCOffsetMapSet(LwU8 channelIdx, LwU8 channelOffsetVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCOffsetMapSet");
static void s_pmgrTestADCOffsetMap2Set(LwU8 channelIdx, LwU8 channelOffsetVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCOffsetMap2Set");
static void s_pmgrTestIpcCtrlSum1InputSet(LwU8 sumInputIdx, LwU8 sumInputVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIpcCtrlSum1InputSet");
static void s_pmgrTestIpcCtrlSum2InputSet(LwU8 sumInputIdx, LwU8 sumInputVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIpcCtrlSum2InputSet");
static void s_pmgrTestIpcCtrlSum1SfSet(LwU8 sumInputIdx, LwU8 sumSfVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIpcCtrlSum1SfSet");
static void s_pmgrTestIpcCtrlSum2SfSet(LwU8 sumInputIdx, LwU8 sumSfVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIpcCtrlSum2SfSet");
static void s_pmgrTestADCInitAndReset(void)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestADCInitAndReset");
static FLCN_STATUS s_pmgrTestRegCacheInitRestore(LwBool bInit)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestRegCacheInitRestore");
static void s_pmgrTestRegCacheInit(void)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestRegCacheInit");
static void s_pmgrTestRegCacheRestore(void)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestRegCacheRestore");

/* ------------------------- Compile Time Checks ---------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Test post init and reset behavior of ADC.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK   If test is supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
pmgrTestAdcInit_AD10X
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU8        idx;
    LwU32       iirValue;
    LwU32       accValueLB;
    LwU32       accValueUB;
    LwU32       accValueSCnt;
    LwU32       mulValue;
    LwU32       accMulValueLB;
    LwU32       accMulValueUB;
    LwU32       accMulValueSCnt;
    LwU32       expectedIIRValueInit
                    = LW_CPWR_THERM_ADC_IIR_VALUE_DATA_VALUE_INIT;
    LwU32       expectedAccValueInit
                    = LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT;
    LwU32       expectedMulValueInit
                    = LW_CPWR_THERM_ADC_MUL_VALUE_DATA_VALUE_INIT;
    LwU32       expectedMulAccValueInit
                    = LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto pmgrTestAdcInit_AD10X_exit;
    }

    s_pmgrTestADCInitAndReset();

    // Check if all IIR values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
         idx <= PMGR_TEST_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestIIRValForChannelGet(idx, &iirValue);

        if (iirValue != LW_CPWR_THERM_ADC_IIR_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_IIR_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &iirValue);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedIIRValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }
    }

    // Check if all ACC values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
         idx <= PMGR_TEST_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestAccIIRValForChannelGet(idx, &accValueLB, &accValueUB, &accValueSCnt);

        if (accValueLB != LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_ACC_LB_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &accValueLB);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedAccValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }

        if (accValueUB != LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_ACC_UB_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &accValueUB);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedAccValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }

        if (accValueSCnt != LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_ACC_SCNT_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &accValueSCnt);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedAccValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }
    }

    // Check if all MUL values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
         idx < LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestMulIIRValForChannelPairGet(idx, &mulValue);

        if (mulValue != LW_CPWR_THERM_ADC_MUL_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &mulValue);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedMulValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }
    }

    // Check if all aclwmulated MUL values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
         idx < LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestAccMulIIRValForChannelPairGet(idx, &accMulValueLB, &accMulValueUB,
            &accMulValueSCnt);

        if (accMulValueLB != LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_ACC_LB_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &accMulValueLB);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedMulAccValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }

        if (accMulValueUB != LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_ACC_UB_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &accMulValueUB);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedMulAccValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }

        if (accMulValueSCnt != LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_ACC_SCNT_VALUE);
            LwU64_ALIGN32_PACK(&pParams->observedVal, &accMulValueSCnt);
            LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedMulAccValueInit);
            goto pmgrTestAdcInit_AD10X_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestAdcInit_AD10X_exit:

    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Tests if various ADC values are computed correctly.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK   If test is supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
pmgrTestAdcCheck_AD10X
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU32 idx;
    LwU32 adcRawCorrVal;
    LwU32 hwIIRVal;
    LwU32 swIIRVal;
    LwU32 hwAccValLB;
    LwU32 hwAccValUB;

    LwU32 sampleCnt_P;
    LwU32 sampleCnt_C;
    LwU64 hwAccVal64_P;
    LwU64 hwAccVal64_C;
    LwU64 sampleCntDiff64;
    LwU64 hwAccValDiff64;
    LwU64 swAccValDiff64;

    LwU32 hwMulVal;
    LwU64 swMulVal64;
    LwU64 hwIIRVal64;
    LwU32 hwIIRValOp1;
    LwU32 hwIIRValOp2;
    LwU64 hwIIRValOp164;
    LwU64 hwIIRValOp264;
    LwU32 hwAccMulValueLB;
    LwU32 hwAccMulValueUB;

    LwU64 hwMulVal64;
    LwU64 hwAccMulVal64_P;
    LwU64 hwAccMulVal64_C;
    LwU64 hwAccMulValDiff64;
    LwU64 swAccMulValDiff64;

    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Cache all registers this test will modify.
        status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
        if (status != FLCN_OK)
        {
            goto pmgrTestAdcCheck_AD10X_exit;
        }

        s_pmgrTestADCInitAndReset();

        // Configure ADC_DEBUG2 register in SW_OVERRIDE mode.
        s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_SW_OVERRIDE,
            PMGR_TEST_ADC_VAL_UNCORRECTED);

        // Configure ADC PWM.
        s_pmgrTestADCPWMConfigure(PMGR_TEST_ADC_PWM_PERIOD, PMGR_TEST_ADC_PWM_HI);

        // Enable and configure ADC PWM Generator.
        s_pmgrTestADCPWMGenConfigure(LW_TRUE, PMGR_TEST_ADC_PWM_HI);

        // Configure channel pairs.
        for (idx = LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MIN;
             idx < LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; idx++)
        {
            s_pmgrTestChannelPairConfigure(idx, (idx * 2), (idx * 2 + 1), LW_TRUE);
        }

        // Configure ADC_CTRL.
        s_pmgrTestADCSensingEnable(LW_TRUE);
        s_pmgrTestADCAclwmulationEnable(LW_TRUE);
        s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX);

        // Trigger SW reset.
        s_pmgrTestADCReset();

        // Wait for 1 microsecond to let the aclwmulators.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        s_pmgrTestADCValuesSnap();

        // Get the ADC RAW correction value.
        adcRawCorrVal = DRF_VAL(_CPWR_THERM, _ADC_RAW_CORRECTION, _VALUE,
                            REG_RD32(CSB, LW_CPWR_THERM_ADC_RAW_CORRECTION));

        swIIRVal = ((PMGR_TEST_ADC_VAL_UNCORRECTED - adcRawCorrVal) <<
                     PMGR_TEST_ADC_SHIFT_FACTOR_FXP);

        // Check if all IIR values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
             idx <= PMGR_TEST_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
        {
            s_pmgrTestIIRValForChannelGet(idx, &hwIIRVal);

            if (swIIRVal != hwIIRVal)
            {
                pParams->outStatus =
                    LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK,
                    IIR_VALUE_MISMATCH);
                LwU64_ALIGN32_PACK(&pParams->observedVal, &hwIIRVal);
                LwU64_ALIGN32_PACK(&pParams->expectedVal, &swIIRVal);
                goto pmgrTestAdcCheck_AD10X_exit;
            }
        }

        // Check if all ACC values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
             idx <= PMGR_TEST_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
        {
            // Previous sample.
            s_pmgrTestIIRValForChannelGet(idx, &hwIIRVal);
            hwIIRVal64 = (LwU64)hwIIRVal;

            s_pmgrTestAccIIRValForChannelGet(idx, &hwAccValLB, &hwAccValUB, &sampleCnt_P);
            hwAccVal64_P = ((((LwU64)hwAccValUB) << 32) | hwAccValLB);

            // Next sample.
            s_pmgrTestAccIIRValForChannelGet(idx, &hwAccValLB, &hwAccValUB, &sampleCnt_C);
            hwAccVal64_C = ((((LwU64)hwAccValUB) << 32) | hwAccValLB);

            // Callwlate the diff in SW and check if HW confirms with SW value.
            sampleCntDiff64 = (LwU64)(sampleCnt_C - sampleCnt_P);
            lw64Sub(&hwAccValDiff64, &hwAccVal64_C, &hwAccVal64_P);
            lwU64Mul(&swAccValDiff64, &sampleCntDiff64, &hwIIRVal64);

            if (!(lw64CmpEQ(&swAccValDiff64, &hwAccValDiff64)))
            {
                pParams->outStatus =
                    LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK,
                    ACC_VALUE_MISMATCH);
                LwU64_ALIGN32_PACK(&pParams->observedVal, &hwAccValDiff64);
                LwU64_ALIGN32_PACK(&pParams->expectedVal, &swAccValDiff64);
                goto pmgrTestAdcCheck_AD10X_exit;
            }
        }

        // Check if all MUL values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
             idx < LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
        {
            // This is configured initially so get the channel values directly.
            s_pmgrTestIIRValForChannelGet((idx * 2 ), &hwIIRValOp1);
            s_pmgrTestIIRValForChannelGet((idx * 2 + 1), &hwIIRValOp2);

            hwIIRValOp164 = (LwU64)hwIIRValOp1;
            hwIIRValOp264 = (LwU64)hwIIRValOp2;

            lwU64Mul(&swMulVal64, &hwIIRValOp164, &hwIIRValOp264);

            s_pmgrTestMulIIRValForChannelPairGet(idx, &hwMulVal);

            // Lower 32 bits are dropped by HW. Compare 32 MSB bits.
            LwU32 swMulVal = (LwU32)(swMulVal64 >> 32);

            if (swMulVal != hwMulVal)
            {
                pParams->outStatus =
                    LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK,
                    MUL_VALUE_MISMATCH);
                LwU64_ALIGN32_PACK(&pParams->observedVal, &hwMulVal);
                LwU64_ALIGN32_PACK(&pParams->expectedVal, &swMulVal);
                goto pmgrTestAdcCheck_AD10X_exit;
            }
        }

        // Check if all aclwmulated MUL values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
             idx < LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
        {
            // Previous sample
            s_pmgrTestMulIIRValForChannelPairGet(idx, &hwMulVal);
            hwMulVal64 = (LwU64)hwMulVal;

            s_pmgrTestAccMulIIRValForChannelPairGet(idx, &hwAccMulValueLB, &hwAccMulValueUB,
                &sampleCnt_P);
            hwAccMulVal64_P = ((((LwU64)hwAccMulValueUB) << 32) | hwAccMulValueLB);

            // Next sample.
            s_pmgrTestAccMulIIRValForChannelPairGet(idx, &hwAccMulValueLB, &hwAccMulValueUB,
                &sampleCnt_C);
            hwAccMulVal64_C = ((((LwU64)hwAccMulValueUB) << 32) | hwAccMulValueLB);

            // Callwlate the diff in SW and check if HW confirms with SW value.
            sampleCntDiff64 = (LwU64)(sampleCnt_C - sampleCnt_P);
            lw64Sub(&hwAccMulValDiff64, &hwAccMulVal64_C, &hwAccMulVal64_P);
            lwU64Mul(&swAccMulValDiff64, &sampleCntDiff64, &hwMulVal64);

            if (!(lw64CmpEQ(&swAccMulValDiff64, &hwAccMulValDiff64)))
            {
                pParams->outStatus =
                    LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK,
                    MUL_ACC_VALUE_MISMATCH);
                LwU64_ALIGN32_PACK(&pParams->observedVal, &hwAccMulValDiff64);
                LwU64_ALIGN32_PACK(&pParams->expectedVal, &swAccMulValDiff64);
                goto pmgrTestAdcCheck_AD10X_exit;
            }
        }

        pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestAdcCheck_AD10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Sets the number of bits for VID_DOWNSHIFT for PWMVID 0.
 *
 * @param[in]  pwmVidIdx     Index of the PWMVID generator to configure
 * @param[in]  downShiftVal  Number of bits for VID_DOWNSHIFT
 */
void
pmgrTestIpcPwmVidDownshiftSet_AD10X
(
    LwU8 pwmVidIdx,
    LwU8 downShiftVal
)
{
    LwU32 regFaIdx = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_FA_INDEX);

    regFaIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_FA_INDEX, _INDEX,
                PMGR_TEST_IPC_CTRL_FA_INDEX_INDEX_VID_DOWNSHIFT, regFaIdx);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_FA_INDEX, regFaIdx);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_FA_DATA, downShiftVal);
}

/*!
 * @brief  Test to check if HI_OFFSET is taken max each time.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK   If test is supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
pmgrTestHiOffsetMaxCheck_AD10X
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU16       hwHiOffset;
    LwU8        idx;
    LwU32       expectedHiOffsetSingleIpcCh
                    = PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CH;
    LwU32       expectedHiOffsetThreeIpcCh
                    = PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_CH;
    LwU32       expectedHiOffsetSingleIpcChp
                    = PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CHP;
    LwU32       expectedHiOffsetThreeIpcChp
                    = PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CHP;
    LwU32       expectedHiOffsetSingleIpcSum
                    = PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_SUM;
    LwU32       expectedHiOffsetThreeIpcSumSumCh
                    = PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_SUM_SUM_CH;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    s_pmgrTestADCInitAndReset();

    // Configure ADC_DEBUG2 register in SW_OVERRIDE mode.
    s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_SW_OVERRIDE,
        PMGR_TEST_ADC_VAL_UNCORRECTED_IPC);

    // Configure ADC PWM.
    s_pmgrTestADCPWMConfigure(PMGR_TEST_ADC_PWM_PERIOD, PMGR_TEST_ADC_PWM_HI);

    // Enable and configure ADC PWM Generator.
    s_pmgrTestADCPWMGenConfigure(LW_TRUE, PMGR_TEST_ADC_PWM_HI);

    // Configure channel pairs.
    for (idx = LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MIN;
         idx <= LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestChannelPairConfigure(idx, (idx * 2), (idx * 2 + 1), LW_TRUE);
    }

    // Configure ADC_CTRL and ADC_PWM.
    s_pmgrTestADCSensingEnable(LW_TRUE);
    s_pmgrTestADCAclwmulationEnable(LW_TRUE);
    s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX);

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Set Input of IPC 0 to be coming from CH1.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CH1);

    // Enable IPC 0.
    s_pmgrTestIPCEnable(0, LW_TRUE);

    // Set Params for IPC instance 0.
    s_pmgrTestIPCParamsSet(0);

    // Program the threshold value for IPC 0.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_0_SINGLE_IPC_CH);

    // Set VID_DOWNSHIFT to 8 for PWMVID 0.
    pmgrTestIpcPwmVidDownshiftSet_HAL(&Pmgr, 0, PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_SINGLE_IPC_CH);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET,
        &hwHiOffset);

    // Disable IPC 0.
    s_pmgrTestIPCEnable(0, LW_FALSE);

    // Check if HI_OFFSET conforms to expected value.
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CH)
    {
        pParams->outStatus   =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData     = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            SINGLE_IPC_CH_FAILURE);
        LwU64_ALIGN32_PACK(&pParams->observedVal, &hwHiOffset);
        LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedHiOffsetSingleIpcCh);
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Set Input of IPC 0 to be coming from CH1.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CH1);

    // Enable IPC 0.
    s_pmgrTestIPCEnable(0, LW_TRUE);

    // Set Input of IPC 1 to be coming from CH14.
    s_pmgrTestIPCChannelSet(1, LW_CPWR_THERM_IPC_CTRL_OP_CH14);

    // Enable IPC 1.
    s_pmgrTestIPCEnable(1, LW_TRUE);

    // Set Input of IPC 2 to be coming from CH5.
    s_pmgrTestIPCChannelSet(2, LW_CPWR_THERM_IPC_CTRL_OP_CH5);

    // Enable IPC 2.
    s_pmgrTestIPCEnable(2, LW_TRUE);

    // Set Params for IPC instance 0.
    s_pmgrTestIPCParamsSet(0);

    // Set Params for IPC instance 1.
    s_pmgrTestIPCParamsSet(1);

    // Set Params for IPC instance 2.
    s_pmgrTestIPCParamsSet(2);

    // Program the threshold value for IPC 0.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_0_THREE_IPC_CH);

    // Program the threshold value for IPC 1.
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_IPC_1_THREE_IPC_CH);

    // Program the threshold value for IPC 2.
    s_pmgrTestIPCRefValSet(2, PMGR_TEST_IPC_REF_VAL_IPC_2_THREE_IPC_CH);

    // Set VID_DOWNSHIFT to 8 for PWMVID 0.
    pmgrTestIpcPwmVidDownshiftSet_HAL(&Pmgr, 0, PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_THREE_IPC_CH);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET,
        &hwHiOffset);

    // Disable IPC 0, 1 and 2.
    s_pmgrTestIPCEnable(0, LW_FALSE);
    s_pmgrTestIPCEnable(1, LW_FALSE);
    s_pmgrTestIPCEnable(2, LW_FALSE);

    // Check if HI_OFFSET conforms to expected value.
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_CH)
    {
        pParams->outStatus   =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData     = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            THREE_IPC_CH_FAILURE);
        LwU64_ALIGN32_PACK(&pParams->observedVal, &hwHiOffset);
        LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedHiOffsetThreeIpcCh);
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Set Input of IPC 0 to be coming from CHP0.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CHP0);

    // Enable IPC 0.
    s_pmgrTestIPCEnable(0, LW_TRUE);

    // Set Params for IPC instance 0.
    s_pmgrTestIPCParamsSet(0);

    // Program the threshold value for IPC 0.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_0_SINGLE_IPC_CHP);

    // Set VID_DOWNSHIFT to 6 for PWMVID 0.
    pmgrTestIpcPwmVidDownshiftSet_HAL(&Pmgr, 0, PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_SINGLE_IPC_CHP);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET,
        &hwHiOffset);

    // Disable IPC 0.
    s_pmgrTestIPCEnable(0, LW_FALSE);

    // Check if HI_OFFSET conforms to expected value.
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_CHP)
    {
        pParams->outStatus   =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData     = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            SINGLE_IPC_CHP_FAILURE);
        LwU64_ALIGN32_PACK(&pParams->observedVal, &hwHiOffset);
        LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedHiOffsetSingleIpcChp);
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Set Input of IPC 0 to be coming from CHP0.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CHP0);

    // Enable IPC 0.
    s_pmgrTestIPCEnable(0, LW_TRUE);

    // Set Input of IPC 1 to be coming from CHP2.
    s_pmgrTestIPCChannelSet(1, LW_CPWR_THERM_IPC_CTRL_OP_CHP2);

    // Enable IPC 1.
    s_pmgrTestIPCEnable(1, LW_TRUE);

    // Set Input of IPC 2 to be coming from CHP4.
    s_pmgrTestIPCChannelSet(2, LW_CPWR_THERM_IPC_CTRL_OP_CHP4);

    // Enable IPC 2.
    s_pmgrTestIPCEnable(2, LW_TRUE);

    // Set Params for IPC instance 0.
    s_pmgrTestIPCParamsSet(0);

    // Set Params for IPC instance 1.
    s_pmgrTestIPCParamsSet(1);

    // Set Params for IPC instance 2.
    s_pmgrTestIPCParamsSet(2);

    // Program the threshold value for IPC 0.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_0_THREE_IPC_CHP);

    // Program the threshold value for IPC 1.
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_IPC_1_THREE_IPC_CHP);

    // Program the threshold value for IPC 2.
    s_pmgrTestIPCRefValSet(2, PMGR_TEST_IPC_REF_VAL_IPC_2_THREE_IPC_CHP);

    // Set VID_DOWNSHIFT to 6 for PWMVID 0.
    pmgrTestIpcPwmVidDownshiftSet_HAL(&Pmgr, 0, PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_THREE_IPC_CHP);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET,
        &hwHiOffset);

    // Disable IPC 0, 1 and 2.
    s_pmgrTestIPCEnable(0, LW_FALSE);
    s_pmgrTestIPCEnable(1, LW_FALSE);
    s_pmgrTestIPCEnable(2, LW_FALSE);

    // Check if HI_OFFSET conforms to expected value.
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_CHP)
    {
        pParams->outStatus   =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData     = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            THREE_IPC_CHP_FAILURE);
        LwU64_ALIGN32_PACK(&pParams->observedVal, &hwHiOffset);
        LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedHiOffsetThreeIpcChp);
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Configure SUM1 Input.
    s_pmgrTestIpcCtrlSum1InputSet(0, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(0));
    s_pmgrTestIpcCtrlSum1InputSet(1, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(1));
    s_pmgrTestIpcCtrlSum1InputSet(2, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(9));
    s_pmgrTestIpcCtrlSum1InputSet(3, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(14));

    // Configure SUM1 Scale Factor.
    s_pmgrTestIpcCtrlSum1SfSet(0, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT1_SINGLE_IPC_SUM);
    s_pmgrTestIpcCtrlSum1SfSet(1, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT2_SINGLE_IPC_SUM);
    s_pmgrTestIpcCtrlSum1SfSet(2, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT3_SINGLE_IPC_SUM);
    s_pmgrTestIpcCtrlSum1SfSet(3, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT4_SINGLE_IPC_SUM);

    // Set Input of IPC 0 to be coming from SUM1.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_SUM1);

    // Enable IPC 0.
    s_pmgrTestIPCEnable(0, LW_TRUE);

    // Set Params for IPC instance 0.
    s_pmgrTestIPCParamsSet(0);

    // Program the threshold value for IPC 0.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_0_SINGLE_IPC_SUM);

    // Set VID_DOWNSHIFT to 8 for PWMVID 0.
    pmgrTestIpcPwmVidDownshiftSet_HAL(&Pmgr, 0, PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_SINGLE_IPC_SUM);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET,
        &hwHiOffset);

    // Disable IPC 0.
    s_pmgrTestIPCEnable(0, LW_FALSE);

    // Reset configuration of IPC SUM1 Input registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_INPUT__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum1InputSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_NO_CH);
    }

    // Reset configuration of IPC SUM1 Scale Factor registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM1_SF_SF__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum1SfSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM1_SF_SF_INIT);
    }

    // Check if HI_OFFSET conforms to expected value.
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_SINGLE_IPC_SUM)
    {
        pParams->outStatus   =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData     = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            SINGLE_IPC_SUM_FAILURE);
        LwU64_ALIGN32_PACK(&pParams->observedVal, &hwHiOffset);
        LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedHiOffsetSingleIpcSum);
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Configure SUM1 Input.
    s_pmgrTestIpcCtrlSum1InputSet(0, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(0));
    s_pmgrTestIpcCtrlSum1InputSet(1, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(1));
    s_pmgrTestIpcCtrlSum1InputSet(2, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(9));
    s_pmgrTestIpcCtrlSum1InputSet(3, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CH(14));

    // Configure SUM2 Input.
    s_pmgrTestIpcCtrlSum2InputSet(0, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CHP(4));
    s_pmgrTestIpcCtrlSum2InputSet(1, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CHP(1));
    s_pmgrTestIpcCtrlSum2InputSet(2, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CHP(2));
    s_pmgrTestIpcCtrlSum2InputSet(3, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_CHP(0));

    // Configure SUM1 Scale Factor.
    s_pmgrTestIpcCtrlSum1SfSet(0, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT1_THREE_IPC_SUM_SUM_CH);
    s_pmgrTestIpcCtrlSum1SfSet(1, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT2_THREE_IPC_SUM_SUM_CH);
    s_pmgrTestIpcCtrlSum1SfSet(2, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT3_THREE_IPC_SUM_SUM_CH);
    s_pmgrTestIpcCtrlSum1SfSet(3, PMGR_TEST_SUM1_SCALE_FACTOR_INPUT4_THREE_IPC_SUM_SUM_CH);

    // Configure SUM2 Scale Factor.
    s_pmgrTestIpcCtrlSum2SfSet(0, PMGR_TEST_SUM2_SCALE_FACTOR_INPUT1_THREE_IPC_SUM_SUM_CH);
    s_pmgrTestIpcCtrlSum2SfSet(1, PMGR_TEST_SUM2_SCALE_FACTOR_INPUT2_THREE_IPC_SUM_SUM_CH);
    s_pmgrTestIpcCtrlSum2SfSet(2, PMGR_TEST_SUM2_SCALE_FACTOR_INPUT3_THREE_IPC_SUM_SUM_CH);
    s_pmgrTestIpcCtrlSum2SfSet(3, PMGR_TEST_SUM2_SCALE_FACTOR_INPUT4_THREE_IPC_SUM_SUM_CH);

    // Set Input of IPC 0 to be coming from SUM1.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_SUM1);

    // Enable IPC 0.
    s_pmgrTestIPCEnable(0, LW_TRUE);

    // Set Input of IPC 1 to be coming from SUM2.
    s_pmgrTestIPCChannelSet(1, LW_CPWR_THERM_IPC_CTRL_OP_SUM2);

    // Enable IPC 1.
    s_pmgrTestIPCEnable(1, LW_TRUE);

    // Set Input of IPC 2 to be coming from CH7.
    s_pmgrTestIPCChannelSet(2, LW_CPWR_THERM_IPC_CTRL_OP_CH7);

    // Enable IPC 2.
    s_pmgrTestIPCEnable(2, LW_TRUE);

    // Set Params for IPC instance 0.
    s_pmgrTestIPCParamsSet(0);

    // Set Params for IPC instance 1.
    s_pmgrTestIPCParamsSet(1);

    // Set Params for IPC instance 2.
    s_pmgrTestIPCParamsSet(2);

    // Program the threshold value for IPC 0.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_0_THREE_IPC_SUM_SUM_CH);

    // Program the threshold value for IPC 1.
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_IPC_1_THREE_IPC_SUM_SUM_CH);

    // Program the threshold value for IPC 2.
    s_pmgrTestIPCRefValSet(2, PMGR_TEST_IPC_REF_VAL_IPC_2_THREE_IPC_SUM_SUM_CH);

    // Set VID_DOWNSHIFT to 8 for PWMVID 0.
    pmgrTestIpcPwmVidDownshiftSet_HAL(&Pmgr, 0, PMGR_TEST_IPC_PWM_VID_DOWNSHIFT_THREE_IPC_SUM_SUM_CH);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(PMGR_TEST_IPC_DEBUG_SEL_IDX_PWM0_HI_OFFSET,
        &hwHiOffset);

    // Disable IPC 0, 1 and 2.
    s_pmgrTestIPCEnable(0, LW_FALSE);
    s_pmgrTestIPCEnable(1, LW_FALSE);
    s_pmgrTestIPCEnable(2, LW_FALSE);

    // Reset configuration of IPC Sum Input registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_INPUT__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum1InputSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_NO_CH);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT_INPUT__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum2InputSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT_NO_CH);
    }

    // Reset configuration of IPC Sum Scale Factor registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM1_SF_SF__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum1SfSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM1_SF_SF_INIT);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM2_SF_SF__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum2SfSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM2_SF_SF_INIT);
    }

    // Check if HI_OFFSET conforms to expected value.
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_THREE_IPC_SUM_SUM_CH)
    {
        pParams->outStatus   =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData     = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            THREE_IPC_SUM_SUM_CH_FAILURE);
        LwU64_ALIGN32_PACK(&pParams->observedVal, &hwHiOffset);
        LwU64_ALIGN32_PACK(&pParams->expectedVal, &expectedHiOffsetThreeIpcSumSumCh);
        goto pmgrTestHiOffsetMaxCheck_AD10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestHiOffsetMaxCheck_AD10X_exit:
    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Tests if Beacon functions correctly.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK   If test is supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
pmgrTestBeaconCheck_AD10X
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU32 beacon1InterruptVal;
    LwU32 beacon2InterruptVal;

    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Cache all registers this test will modify.
        status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
        if (status != FLCN_OK)
        {
            goto pmgrTestBeaconCheck_AD10X_exit;
        }

        s_pmgrTestADCInitAndReset();

        // Configure ADC_DEBUG2 register in SW_OVERRIDE mode.
        s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_SW_OVERRIDE,
            PMGR_TEST_ADC_VAL_UNCORRECTED);

        // Configure ADC PWM.
        s_pmgrTestADCPWMConfigure(PMGR_TEST_ADC_PWM_PERIOD, PMGR_TEST_ADC_PWM_HI);

        // Enable and configure ADC PWM Generator.
        s_pmgrTestADCPWMGenConfigure(LW_TRUE, PMGR_TEST_ADC_PWM_HI);

        // Configure ADC_CTRL.
        s_pmgrTestADCSensingEnable(LW_TRUE);
        s_pmgrTestADCAclwmulationEnable(LW_TRUE);
        s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX);

        // Trigger SW reset.
        s_pmgrTestADCReset();

        // Wait for 1 microsecond to let the aclwmulators.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        s_pmgrTestADCValuesSnap();

        // Configure Channel 0 as BEACON1. 
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON1_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_CH(0));

        // Force HW IIR value to be GREATER THAN the BEACON1 Threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON1_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_LOW);

        // Set BEACON1 Comparator Function to LESS THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON1_INDEX,
            LW_FALSE);

        // Configure BEACON2 to be inactive. 
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON2_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);

        // Enable BEACON1 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON1_INDEX, 
            LW_TRUE);

        // Wait for 1 millisecond to let the interrupt generate.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        // Check if BEACON1 interrupt is pending.
        beacon1InterruptVal = DRF_IDX_VAL(_CPWR_THERM, _INTR_0, _ISENSE_BEACON_INTR,
            PMGR_TEST_BEACON1_INDEX, REG_RD32(CSB, LW_CPWR_THERM_INTR_0));

        // Reverse condition that caused BEACON1 interrupt to be pending.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON1_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_HIGH);

        // Disable BEACON1 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON1_INDEX, 
            LW_FALSE);

        // Clear BEACON1 interrupt.
        s_pmgrTestADCBeaconInterruptClear(PMGR_TEST_BEACON1_INDEX);

        // Configure BEACON1 to be inactive.
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON1_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);

        // Reset BEACON1 threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON1_INDEX,
            LW_CPWR_THERM_ADC_CTRL3_BEACON_THRESH_INIT);

        // Reset BEACON1 comparator to GREATER THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON1_INDEX,
            LW_TRUE);

        if (beacon1InterruptVal != LW_CPWR_THERM_INTR_0_ISENSE_BEACON_INTR_PENDING)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK,
                BEACON1_INTERRUPT_NOT_PENDING);
            goto pmgrTestBeaconCheck_AD10X_exit;
        }

        // Trigger SW reset.
        s_pmgrTestADCReset();

        // Wait for 1 millisecond to let the values populate.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        // Snap values.
        s_pmgrTestADCValuesSnap();

        // Configure Channel 21 as BEACON2. 
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON2_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_CH(21));

        // Force HW IIR value to be LESS THAN the BEACON2 Threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON2_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_HIGH);

        // Set BEACON2 Comparator Function to GREATER THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON2_INDEX,
            LW_TRUE);

        // Configure BEACON1 to be inactive. 
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON1_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);

        // Enable BEACON2 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON2_INDEX, 
            LW_TRUE);

        // Wait for 1 millisecond to let the interrupt generate.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        // Check if BEACON2 interrupt is pending.
        beacon2InterruptVal = DRF_IDX_VAL(_CPWR_THERM, _INTR_0, _ISENSE_BEACON_INTR,
            PMGR_TEST_BEACON2_INDEX, REG_RD32(CSB, LW_CPWR_THERM_INTR_0));

        // Reverse condition that caused BEACON2 interrupt to be pending.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON2_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_LOW);

        // Disable BEACON2 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON2_INDEX, 
            LW_FALSE);

        // Clear BEACON2 interrupt.
        s_pmgrTestADCBeaconInterruptClear(PMGR_TEST_BEACON2_INDEX);

        // Configure BEACON2 to be inactive.
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON2_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);

        // Reset BEACON2 threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON2_INDEX,
            LW_CPWR_THERM_ADC_CTRL3_BEACON_THRESH_INIT);

        // Reset BEACON2 comparator to GREATER THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON2_INDEX,
            LW_TRUE);

        if (beacon2InterruptVal != LW_CPWR_THERM_INTR_0_ISENSE_BEACON_INTR_PENDING)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK,
                BEACON2_INTERRUPT_NOT_PENDING);
            goto pmgrTestBeaconCheck_AD10X_exit;
        }

        // Trigger SW reset.
        s_pmgrTestADCReset();

        // Wait for 1 millisecond to let the values populate.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        // Snap values.
        s_pmgrTestADCValuesSnap();

        // Configure Channel 5 as BEACON1. 
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON1_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_CH(5));

        // Force HW IIR value to be LESS THAN the BEACON1 Threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON1_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_HIGH);

        // Set BEACON1 Comparator Function to GREATER THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON1_INDEX,
            LW_TRUE);

        // Configure Channel 14 as BEACON2. 
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON2_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_CH(14));

        // Force HW IIR value to be GREATER THAN the BEACON2 Threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON2_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_LOW);

        // Set BEACON2 Comparator Function to LESS THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON2_INDEX,
            LW_FALSE);

        // Enable BEACON1 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON1_INDEX, 
            LW_TRUE);

        // Wait for 1 millisecond to let the interrupt generate.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        // Enable BEACON2 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON2_INDEX, 
            LW_TRUE);

        // Wait for 1 millisecond to let the interrupt generate.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        // Check if BEACON1 interrupt is pending.
        beacon1InterruptVal = DRF_IDX_VAL(_CPWR_THERM, _INTR_0, _ISENSE_BEACON_INTR,
            PMGR_TEST_BEACON1_INDEX, REG_RD32(CSB, LW_CPWR_THERM_INTR_0));

        // Reverse condition that caused BEACON1 interrupt to be pending.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON1_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_LOW);

        // Disable BEACON1 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON1_INDEX, 
            LW_FALSE);

        // Clear BEACON1 interrupt.
        s_pmgrTestADCBeaconInterruptClear(PMGR_TEST_BEACON1_INDEX);

        // Configure BEACON1 to be inactive.
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON1_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);

        // Reset BEACON1 threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON1_INDEX,
            LW_CPWR_THERM_ADC_CTRL3_BEACON_THRESH_INIT);

        // Reset BEACON1 comparator to GREATER THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON1_INDEX,
            LW_TRUE);

        // Check if BEACON2 interrupt is pending.
        beacon2InterruptVal = DRF_IDX_VAL(_CPWR_THERM, _INTR_0, _ISENSE_BEACON_INTR,
            PMGR_TEST_BEACON2_INDEX, REG_RD32(CSB, LW_CPWR_THERM_INTR_0));

        // Reverse condition that caused BEACON2 interrupt to be pending.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON2_INDEX,
            PMGR_TEST_BEACON_THRESHOLD_HIGH);

        // Disable BEACON2 interrupt.
        s_pmgrTestADCBeaconInterruptEnable(PMGR_TEST_BEACON2_INDEX, 
            LW_FALSE);

        // Clear BEACON2 interrupt.
        s_pmgrTestADCBeaconInterruptClear(PMGR_TEST_BEACON2_INDEX);

        // Configure BEACON2 to be inactive.
        s_pmgrTestADCBeaconChannelSet(PMGR_TEST_BEACON2_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);

        // Reset BEACON2 threshold.
        s_pmgrTestADCBeaconThresholdSet(PMGR_TEST_BEACON2_INDEX,
            LW_CPWR_THERM_ADC_CTRL3_BEACON_THRESH_INIT);

        // Reset BEACON2 comparator to GREATER THAN.
        s_pmgrTestADCBeaconComparatorSet(PMGR_TEST_BEACON2_INDEX,
            LW_TRUE);

        if (beacon1InterruptVal != LW_CPWR_THERM_INTR_0_ISENSE_BEACON_INTR_PENDING &&
            beacon2InterruptVal != LW_CPWR_THERM_INTR_0_ISENSE_BEACON_INTR_PENDING)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK,
                BOTH_BEACON_INTERRUPTS_NOT_PENDING);
            goto pmgrTestBeaconCheck_AD10X_exit;
        }
        else if (beacon1InterruptVal != LW_CPWR_THERM_INTR_0_ISENSE_BEACON_INTR_PENDING)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK,
                BEACON1_INTERRUPT_NOT_PENDING_BEACON2_INTERRUPT_PENDING);
            goto pmgrTestBeaconCheck_AD10X_exit;
        }
        else if (beacon2InterruptVal != LW_CPWR_THERM_INTR_0_ISENSE_BEACON_INTR_PENDING)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK,
                BEACON2_INTERRUPT_NOT_PENDING_BEACON1_INTERRUPT_PENDING);
            goto pmgrTestBeaconCheck_AD10X_exit;
        }

        pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestBeaconCheck_AD10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Tests if Offset functions correctly.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK   If test is supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
pmgrTestOffsetCheck_AD10X
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU32 idx;
    LwU32 adcRawCorrVal;
    LwU32 hwIIRValOffset1;
    LwU32 hwIIRValOffset2;
    LwU32 swIIRVal;
    LwU32 hwAccValOffset1LB;
    LwU32 hwAccValOffset2LB;
    LwU32 hwAccValOffset1UB;
    LwU32 hwAccValOffset2UB;
    LwU32 hwAccValOffset164_C;
    LwU32 hwAccValOffset264_C;

    LwU32 sampleCntOffset1_C;
    LwU32 sampleCntOffset2_C;

    LwU32 hwMulValOffset1;
    LwU32 hwMulValOffset2;
    LwU32 hwAccMulValueOffset1LB;
    LwU32 hwAccMulValueOffset2LB;
    LwU32 hwAccMulValueOffset1UB;
    LwU32 hwAccMulValueOffset2UB;
    LwU32 hwAccMulValOffset164_C;
    LwU32 hwAccMulValOffset264_C;

    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Cache all registers this test will modify.
        status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
        if (status != FLCN_OK)
        {
            goto pmgrTestOffsetCheck_AD10X_exit;
        }

        s_pmgrTestADCInitAndReset();

        // Configure ADC_DEBUG2 register in SW_OVERRIDE mode.
        s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_SW_OVERRIDE,
            PMGR_TEST_ADC_VAL_UNCORRECTED);

        // Configure ADC PWM.
        s_pmgrTestADCPWMConfigure(PMGR_TEST_ADC_PWM_PERIOD, PMGR_TEST_ADC_PWM_HI);

        // Enable and configure ADC PWM Generator.
        s_pmgrTestADCPWMGenConfigure(LW_TRUE, PMGR_TEST_ADC_PWM_HI);

        // Configure channel pairs.
        for (idx = LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MIN;
             idx < LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; idx++)
        {
            s_pmgrTestChannelPairConfigure(idx, (idx * 2), (idx * 2 + 1), LW_TRUE);
        }

        // Configure ADC_CTRL.
        s_pmgrTestADCSensingEnable(LW_TRUE);
        s_pmgrTestADCAclwmulationEnable(LW_TRUE);
        s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX);

        // Configure Channel 0 as OFFSET1.
        s_pmgrTestADCOffsetChannelSet(PMGR_TEST_OFFSET1_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_OFFSET_CH(0));

        // Configure channel 5 in the OFFSET MAP for OFFSET1.
        s_pmgrTestADCOffsetMapSet(PMGR_TEST_OFFSET1_OFFSET_MAP_INDEX,
            LW_CPWR_THERM_ADC_OFFSET_MAP_CH_OFFSET1);

        // Configure Channel 8 as OFFSET2.
        s_pmgrTestADCOffsetChannelSet(PMGR_TEST_OFFSET2_INDEX, 
            LW_CPWR_THERM_ADC_CTRL2_OFFSET_CH(8));

        // Configure channel 14 in the OFFSET MAP for OFFSET2.
        s_pmgrTestADCOffsetMapSet(PMGR_TEST_OFFSET2_OFFSET_MAP_INDEX,
            LW_CPWR_THERM_ADC_OFFSET_MAP_CH_OFFSET2);

        // Trigger SW reset.
        s_pmgrTestADCReset();

        // Wait for 1 microsecond to let the aclwmulators.
        OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

        s_pmgrTestADCValuesSnap();

        // Get the ADC RAW correction value.
        adcRawCorrVal = DRF_VAL(_CPWR_THERM, _ADC_RAW_CORRECTION, _VALUE,
                            REG_RD32(CSB, LW_CPWR_THERM_ADC_RAW_CORRECTION));

        // Fetch IIR values for the channel using OFFSET1.
        s_pmgrTestIIRValForChannelGet(PMGR_TEST_OFFSET1_OFFSET_MAP_INDEX,
            &hwIIRValOffset1);

        // Fetch IIR values for the channel using OFFSET2.
        s_pmgrTestIIRValForChannelGet(PMGR_TEST_OFFSET2_OFFSET_MAP_INDEX,
            &hwIIRValOffset2);

        // Fetch SW IIR value and adjust by raw correction factor.
        swIIRVal = ((PMGR_TEST_ADC_VAL_UNCORRECTED - adcRawCorrVal) <<
                     PMGR_TEST_ADC_SHIFT_FACTOR_FXP);

        // IIR values should not be affected by Offset action.
        if (swIIRVal != hwIIRValOffset1 && swIIRVal != hwIIRValOffset2)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                BOTH_OFFSETS_IIR_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (swIIRVal != hwIIRValOffset1)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET1_IIR_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (swIIRVal != hwIIRValOffset2)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET2_IIR_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }

        // Get updated HW IIR ACC values for the channel using OFFSET1.
        s_pmgrTestAccIIRValForChannelGet(PMGR_TEST_OFFSET1_OFFSET_MAP_INDEX,
            &hwAccValOffset1LB, &hwAccValOffset1UB, &sampleCntOffset1_C);
        hwAccValOffset164_C = ((((LwU64)hwAccValOffset1UB) << 32) | hwAccValOffset1LB);

        // Get updated HW IIR ACC values for the channel using OFFSET2.
        s_pmgrTestAccIIRValForChannelGet(PMGR_TEST_OFFSET2_OFFSET_MAP_INDEX,
            &hwAccValOffset2LB, &hwAccValOffset2UB, &sampleCntOffset2_C);
        hwAccValOffset264_C = ((((LwU64)hwAccValOffset2UB) << 32) | hwAccValOffset2LB);

        // IIR ACC values should be affected by Offset action.
        if (hwAccValOffset164_C != PMGR_TEST_OFFSET1_IIR_ACC_VALUE_POST_OFFSET &&
            hwAccValOffset264_C != PMGR_TEST_OFFSET2_IIR_ACC_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                BOTH_OFFSETS_IIR_ACC_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (hwAccValOffset164_C != PMGR_TEST_OFFSET1_IIR_ACC_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET1_IIR_ACC_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (hwAccValOffset264_C != PMGR_TEST_OFFSET2_IIR_ACC_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET2_IIR_ACC_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }

        //
        // CHP_(idx) has CH_(idx*2) and CH_(idx*2 + 1) as operands. We need to
        // obtain one of the operands for the channel pair differently for odd
        // and for even channels.
        // For odd channels, we would have to do (PMGR_TEST_OFFSET*_OFFSET_MAP_INDEX - 1)/2.
        // For even channels, we would have to do (PMGR_TEST_OFFSET*_OFFSET_MAP_INDEX)/2.
        //

        // Fetch MUL values for the channel pair using OFFSET1.
        s_pmgrTestMulIIRValForChannelPairGet((PMGR_TEST_OFFSET1_OFFSET_MAP_INDEX-1)/2,
            &hwMulValOffset1);

        // Fetch MUL values for the channel pair using OFFSET2.
        s_pmgrTestMulIIRValForChannelPairGet(PMGR_TEST_OFFSET2_OFFSET_MAP_INDEX/2,
            &hwMulValOffset2);

        // MUL values should be affected by Offset action.
        if (hwMulValOffset1 != PMGR_TEST_OFFSET1_MUL_VALUE_POST_OFFSET &&
            hwMulValOffset2 != PMGR_TEST_OFFSET2_MUL_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                BOTH_OFFSETS_MUL_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (hwMulValOffset1 != PMGR_TEST_OFFSET1_MUL_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET1_MUL_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (hwMulValOffset2 != PMGR_TEST_OFFSET2_MUL_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET2_MUL_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }

        // Get updated HW MUL ACC values for the channel pair using OFFSET1.
        s_pmgrTestAccMulIIRValForChannelPairGet((PMGR_TEST_OFFSET1_OFFSET_MAP_INDEX - 1)/2,
            &hwAccMulValueOffset1LB, &hwAccMulValueOffset1UB, &sampleCntOffset1_C);
        hwAccMulValOffset164_C = ((((LwU64)hwAccMulValueOffset1UB) << 32) | hwAccMulValueOffset1LB);

        // Get updated HW MUL ACC values for the channel pair using OFFSET2.
        s_pmgrTestAccMulIIRValForChannelPairGet(PMGR_TEST_OFFSET2_OFFSET_MAP_INDEX/2,
            &hwAccMulValueOffset2LB, &hwAccMulValueOffset2UB, &sampleCntOffset2_C);
        hwAccMulValOffset264_C = ((((LwU64)hwAccMulValueOffset2UB) << 32) | hwAccMulValueOffset2LB);

        // MUL ACC values should be affected by Offset action.
        if (hwAccMulValOffset164_C != PMGR_TEST_OFFSET1_MUL_ACC_VALUE_POST_OFFSET &&
            hwAccMulValOffset264_C != PMGR_TEST_OFFSET2_MUL_ACC_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                BOTH_OFFSETS_MUL_ACC_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (hwAccMulValOffset164_C != PMGR_TEST_OFFSET1_MUL_ACC_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET1_MUL_ACC_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }
        else if (hwAccMulValOffset264_C != PMGR_TEST_OFFSET2_MUL_ACC_VALUE_POST_OFFSET)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK,
                OFFSET2_MUL_ACC_VALUE_ERROR);
            goto pmgrTestOffsetCheck_AD10X_exit;
        }

        pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestOffsetCheck_AD10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief  Sets the specified min ADC limit in the ADC_DEBUG register.
 *
 * @param[in] minLimit  Min ADC limit to set
 */
static void
s_pmgrTestADCDebugMinLimitSet
(
    LwU8  minLimit
)
{
    LwU32 regAdc = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG);

    regAdc = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_DEBUG, _MIN_ADC,
                    minLimit, regAdc);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG, regAdc);
}

/*!
 * @brief  Sets the specified max ADC limit in the ADC_DEBUG register.
 *
 * @param[in] maxLimit  Max ADC limit to set
 */
static void
s_pmgrTestADCDebugMaxLimitSet
(
    LwU8  maxLimit
)
{
    LwU32 regAdc = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG);

    regAdc = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_DEBUG, _MAX_ADC,
                    maxLimit, regAdc);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG, regAdc);
}

/*!
 * @brief  Sets the specified step period in the ADC_DEBUG register.
 *
 * @param[in] stepPeriod  Step Period to set
 */
static void
s_pmgrTestADCDebugStepPeriodSet
(
    LwU8  stepPeriod
)
{
    LwU32 regAdc = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG);

    regAdc = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_DEBUG, _STEP_PERIOD,
                    stepPeriod, regAdc);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG, regAdc);
}

/*!
 * @brief  Sets the specified mode and optionally SW override value
 *         (if requested) in the ADC_DEBUG register.
 *
 * @param[in] mode         One among ADC_DEBUG_MODE_*
 * @param[in] overrideVal  Optional SW override value
 *                         (if mode is ADC_DEBUG_MODE_SW_OVERRIDE)
 */
static void
s_pmgrTestADCDebugModeSet
(
    LwU8  mode,
    LwU8  overrideVal
)
{

    LwU32 regAdc = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG);

    regAdc = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_DEBUG, _MODE, mode, regAdc);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG, regAdc);

    if (mode == LW_CPWR_THERM_ADC_DEBUG_MODE_SW_OVERRIDE)
    {
        regAdc = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG2);

        regAdc = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_DEBUG2, _SW_FORCED_ADC,
                    overrideVal, regAdc);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG2, regAdc);
    }
}

/*!
 * @brief  Enables/Disables ADC sensing feature.
 *
 * @param[in]   bEnable Boolean indicating if we want to enable ADC sensing
 *                      feature. LW_TRUE enables the ADC sensing feature and
 *                      LW_FALSE disables it
 */
static void
s_pmgrTestADCSensingEnable
(
    LwBool bEnable
)
{
    LwU32 regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

    if (bEnable)
    {
        regAdcCtrl = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _ADC_SENSING, _ON,
                        regAdcCtrl);
    }
    else
    {
        regAdcCtrl = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _ADC_SENSING, _OFF,
                        regAdcCtrl);
    }

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, regAdcCtrl);
}

/*!
 * @brief  Enables/Disables the ADC aclwmulators.
 *
 * @param[in]   bEnable Boolean indicating if we want to enable the ADC
 *                      aclwmulators. LW_TRUE enables the ADC aclwmulators and
 *                      LW_FALSE disables them
 */
static void
s_pmgrTestADCAclwmulationEnable
(
    LwBool bEnable
)
{
    LwU32 regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

    if (bEnable)
    {
        regAdcCtrl = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _ADC_ACC, _ON, regAdcCtrl);
    }
    else
    {
        regAdcCtrl = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _ADC_ACC, _OFF, regAdcCtrl);
    }

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, regAdcCtrl);
}

/*!
 * @brief  Snaps the current state of various values provided by ADC.
 */
static void
s_pmgrTestADCValuesSnap(void)
{
    LwU32 regAdcSnapshot = REG_RD32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT);

    regAdcSnapshot = FLD_SET_DRF(_CPWR_THERM, _ADC_SNAPSHOT, _SNAP, _TRIGGER, regAdcSnapshot);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT, regAdcSnapshot);

    // Wait until snap is not completed.
    while (FLD_TEST_DRF(_CPWR_THERM, _ADC_SNAPSHOT, _SNAP, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT)))
    {
        // NOP
    }
}

/*!
 * @brief  Snaps the current state of various values provided by ADC.
 *
 * @param[in] numChannels Number of channels that the ADC will monitor
 */
static void
s_pmgrTestADCNumActiveChannelsSet
(
    LwU8 numChannels
)
{
    LwU32 regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

    regAdcCtrl = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL, _ACTIVE_CHANNELS_NUM,
                    numChannels, regAdcCtrl);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, regAdcCtrl);
}

/*!
 * @brief  Sets the IIR length of the ADC controller.
 *
 * @param[in] iirLength  IIR length to set
 */
static void
s_pmgrTestADCIIRLengthSet
(
    LwU8 iirLength
)
{
    LwU32 regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

    regAdcCtrl = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL, _ADC_IIR_LENGTH,
                    iirLength, regAdcCtrl);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, regAdcCtrl);
}

/*!
 * @brief  Sets the sample delay of the ADC controller.
 *
 * @param[in] sampleDelay  Sample delay to set
 */
static void
s_pmgrTestADCSampleDelaySet
(
    LwU16 sampleDelay
)
{
    LwU32 regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

    regAdcCtrl = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL, _SAMPLE_DELAY,
                    sampleDelay, regAdcCtrl);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, regAdcCtrl);
}

/*!
 * @brief  Sets the period and dutycycle of the ADC controller.
 *
 * @param[in] period    Period to set of the ADC controller
 * @param[in] dutyCycle Dutycycle to set of the ADC controller
 */
static void
s_pmgrTestADCPWMConfigure
(
    LwU16 period,
    LwU16 dutyCycle
)
{
    LwU32 regAdcPwm = REG_RD32(CSB, LW_CPWR_THERM_ADC_PWM);

    regAdcPwm = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_PWM, _PERIOD, period,
                    regAdcPwm);
    regAdcPwm = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_PWM, _HI, dutyCycle,
                    regAdcPwm);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_PWM, regAdcPwm);
}

/*!
 * @brief Configures the PWM generator.
 *
 * @param[in] bEnable       Boolean indicating whether to enable the PWM Generator.
 * @param[in] dutyCycleLast Dutycycle for the last channel
 */
static void
s_pmgrTestADCPWMGenConfigure
(
    LwBool bEnable,
    LwU16  dutyCycleLast
)
{
    LwU32 regAdcPwmGen = REG_RD32(CSB, LW_CPWR_THERM_ADC_PWM_GEN);

    regAdcPwmGen = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_PWM_GEN, _HI_LAST, dutyCycleLast,
                       regAdcPwmGen);

    if (bEnable)
    {
        regAdcPwmGen = FLD_SET_DRF(_CPWR_THERM, _ADC_PWM_GEN, _ENABLE, _YES,
                           regAdcPwmGen);
    }
    else
    {
        regAdcPwmGen = FLD_SET_DRF(_CPWR_THERM, _ADC_PWM_GEN, _ENABLE, _NO,
                           regAdcPwmGen);
    }

    REG_WR32(CSB, LW_CPWR_THERM_ADC_PWM_GEN, regAdcPwmGen);
}

/*!
 * @brief  Reset the ADC sensing feature.
 */
static void
s_pmgrTestADCReset(void)
{
    LwU32 regAdcReset = REG_RD32(CSB, LW_CPWR_THERM_ADC_RESET);

    regAdcReset = FLD_SET_DRF(_CPWR_THERM, _ADC_RESET, _SW_RST, _TRIGGER,
                    regAdcReset);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_RESET, regAdcReset);

    // Wait until reset is not completed.
    while (FLD_TEST_DRF(_CPWR_THERM, _ADC_RESET, _SW_RST, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_RESET)))
    {
        // NOP
    }
}

/*!
 * @brief  Gets the IIR value for the channel specified by @ref chIdx
 *
 * @param[in]   chIdx  Index denoting the channel whose value should be read
 *                     It should be among LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_<xyz>
 * @param[out]  pValue Value of channel specified by @ref chIdx
 */
static void
s_pmgrTestIIRValForChannelGet
(
    LwU8   chIdx,
    LwU32 *pValue
)
{
    LwU32 regIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX);

    // Set channel index.
    regIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_IIR_VALUE_INDEX, _INDEX, chIdx,
                regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX, regIdx);

    // Read the IIR value.
    *pValue= DRF_VAL(_CPWR_THERM, _ADC_IIR_VALUE_DATA, _VALUE,
                REG_RD32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_DATA));
}

/*!
 * @brief  Gets the aclwmulated IIR value and number of samples for the channel
 *         specified by @ref chIdx.
 *
 * @param[in]   chIdx  Index denoting the channel whose value should be read
 *                     It should be strictly less than
 *                     LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX
 * @param[out]  pLB    Lower dword of aclwmulated value of channel specified by @ref chIdx
 * @param[out]  pUb    Lower dword of aclwmulated value of channel specified by @ref chIdx
 * @param[out]  pSCnt  Number of samples considered by the aclwmulator of channel
 *                     specified by @ref chIdx
 */
static void
s_pmgrTestAccIIRValForChannelGet
(
    LwU8   chIdx,
    LwU32 *pLB,
    LwU32 *pUB,
    LwU32 *pSCnt
)
{
    LwU32 regIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX);

    // Set index for LB.
    regIdx  = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
                (chIdx*3), regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, regIdx);

    // Read LB.
    *pLB = DRF_VAL(_CPWR_THERM, _ADC_ACC_VALUE_DATA, _VALUE,
                REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA));

    // Set index for UB.
    regIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
        ((chIdx * 3) + 1), regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, regIdx);

    // Read UB.
    *pUB = DRF_VAL(_CPWR_THERM, _ADC_ACC_VALUE_DATA, _VALUE,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA));

    // Set index for sample count.
    regIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
        ((chIdx * 3) + 2), regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, regIdx);

    // Read sample count.
    *pSCnt = DRF_VAL(_CPWR_THERM, _ADC_ACC_VALUE_DATA, _VALUE,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA));
}

/*!
 * @brief  Gets the multiplied IIR value (power) for the channel pair specified
 *         by @ref chPairIdx.
 *
 * @param[in]   chPairIdx Index of the channel pair whose value should be read
 *                        It should be among
 *                        LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP<x>
 * @param[out]  pValue    Multiplied IIR value (power) for the channel pair
 *                        specified by @ref chPairIdx
 */
static void
s_pmgrTestMulIIRValForChannelPairGet
(
    LwU8   chPairIdx,
    LwU32 *pValue
)
{
    LwU32 regIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_INDEX);

    // Set channel pair index.
    regIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_VALUE_INDEX, _INDEX,
                chPairIdx, regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_INDEX, regIdx);

    // Read the multiplied value (power) for the specified channel pair.
    *pValue = DRF_VAL(_CPWR_THERM, _ADC_MUL_VALUE_DATA, _VALUE,
                REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_DATA));
}

/*!
 * @brief  Gets the aclwmulated multiplied IIR value (power) for the channel
 *         pair specified by @ref chPairIdx and the number of samples contributing
 *         to the aclwmulation.
 *
 * @param[in]   chPairIdx Index of the channel pair whose value should be read
 *                        It should be among
 *                        LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP<x>
 * @param[out]  pValue    Multiplied IIR value (power) for the channel pair
 *                        specified by @ref chPairIdx
 */
static void
s_pmgrTestAccMulIIRValForChannelPairGet
(
    LwU8   chPairIdx,
    LwU32 *pLB,
    LwU32 *pUB,
    LwU32 *pSCnt
)
{
    LwU32 regIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX);

    // Set index for LB.
    regIdx  = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_ACC_VALUE_INDEX, _INDEX,
                (chPairIdx * 3), regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX, regIdx);

    // Read LB.
    *pLB = DRF_VAL(_CPWR_THERM, _ADC_MUL_ACC_VALUE_DATA, _VALUE,
                REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA));

    // Set index for UB.
    regIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_ACC_VALUE_INDEX, _INDEX,
        ((chPairIdx * 3) + 1), regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX, regIdx);

    // Read UB.
    *pUB = DRF_VAL(_CPWR_THERM, _ADC_MUL_ACC_VALUE_DATA, _VALUE,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA));

    // Set index for sample count.
    regIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_ACC_VALUE_INDEX, _INDEX,
        ((chPairIdx * 3) + 2), regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX, regIdx);

    // Read sample count.
    *pSCnt = DRF_VAL(_CPWR_THERM, _ADC_MUL_ACC_VALUE_DATA, _VALUE,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA));
}

/*!
 * @brief  Configures the channel pair specified by @chPairIdx with operands
 *         @ref operand1 and @ref operand2 and optionally enable it if
 *         @ref bEnable is LW_TRUE.
 *
 * @param[in]  chPairIdx Index of the channel pair to configure
 * @param[in]  operand1  Operand 1 for the channel pair to set
 * @param[in]  operand2  Operand 2 for the channel pair to set
 * @param[in]  bEnable   Boolean indicating if we want to enable this channel pair
 *                       LW_TRUE enables the channel pair and LW_FALSE disables it
 */
static void
s_pmgrTestChannelPairConfigure
(
    LwU8   chPairIdx,
    LwU8   operand1,
    LwU8   operand2,
    LwBool bEnable
)
{
    LwU32 regIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX);
    LwU32 regIdxData = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_DATA);

    // Set the channel pair index which is to be configured.
    regIdx  = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_INDEX, _INDEX,
                chPairIdx, regIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, regIdx);

    // Configure operands for the selected channel pair.
    regIdxData = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                    _VALUE_CHP_CFG_OP1, operand1, regIdxData);
    regIdxData = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                    _VALUE_CHP_CFG_OP2, operand2, regIdxData);

    // Enable if requested else disable.
    if (bEnable)
    {
        regIdxData = FLD_SET_DRF(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                        _VALUE_CHP_CFG_ENABLE, _ON, regIdxData);
    }
    else
    {
        regIdxData = FLD_SET_DRF(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                        _VALUE_CHP_CFG_ENABLE, _OFF, regIdxData);
    }

    // Write the overall config for the channel pair.
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_DATA, regIdxData);
}

/*!
 * @brief  Sets the channel that IPC instance specified by @ref ipcIdx will monitor.
 *
 * @param[in]  ipcIdx Index of the IPC instance to configure
 * @param[in]  chIdx  Channel index that this IPC instance will monitor
 *                    Should be among LW_CPWR_THERM_IPC_CTRL_OP_*
 */
static void
s_pmgrTestIPCChannelSet
(
    LwU8 ipcIdx,
    LwU8 chIdx
)
{
    LwU32 regIpc = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx));

    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP, chIdx, regIpc);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx), regIpc);
}


/*!
 * @brief  Sets the same IPC parameters for the #IPC instance specified by @ref ipcIdx.
 *
 * @param[in]  ipcIdx Index of the IPC instance to configure
 */
static void
s_pmgrTestIPCParamsSet
(
    LwU8 ipcIdx
)
{
    LwU32 regIpcCtrlIdx = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_INDEX);

    //
    // Set the LW_CPWR_THERM_IPC_CTRL_INDEX to point to the first parameter of the IPC
    // instance specified by @ref ipcIdx.
    //
    regIpcCtrlIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_INDEX, _INDEX,
        ((ipcIdx)*PMGR_TEST_PER_IPC_PARAM_COUNT), regIpcCtrlIdx);

    //
    // Enable auto-increment of the LW_CPWR_THERM_IPC_CTRL_INDEX register on
    // writing to the LW_CPWR_THERM_IPC_CTRL_DATA register.
    //
    regIpcCtrlIdx = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL_INDEX, _INDEX_WAUTOINCR,
        _ENABLE, regIpcCtrlIdx);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_INDEX, regIpcCtrlIdx);

    // Set the Parameters of the IPC instance specified by @ref ipcIdx
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_AX0_AY_DS);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_AX1_AX2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_AY1_AY2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_A_FLOOR);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_A_CEIL);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_BX0_BY_DS);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_BX1_BX2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_BY1_BY2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_B_FLOOR);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_B_CEIL);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_CX0_CY_DS_IPC_DS);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_CX1_CX2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_CY1_CY2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_C_FLOOR);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_C_CEIL);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_LAST_FLOOR_C_TARGET);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, PMGR_TEST_IPC_CTRL_DATA_VALUE_LAST_CEIL_INTEGRATOR);

    regIpcCtrlIdx = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_INDEX);

    //
    // Disable auto-increment of the LW_CPWR_THERM_IPC_CTRL_INDEX register on
    // writing to the LW_CPWR_THERM_IPC_CTRL_DATA register.
    //
    regIpcCtrlIdx = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL_INDEX, _INDEX_WAUTOINCR,
        _DISABLE, regIpcCtrlIdx);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_INDEX, regIpcCtrlIdx);
}

/*!
 * @brief  Enables/Disables the IPC instance specified by @ref ipcIdx.
 *
 * @param[in]   bEnable Boolean indicating if we want to enable the IPC instance
 *                      LW_TRUE enables the IPC instance and LW_FALSE disables it
 */
static void
s_pmgrTestIPCEnable
(
    LwU8   ipcIdx,
    LwBool bEnable
)
{
    LwU32 regIpc = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx));

    if (bEnable)
    {
        regIpc = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _ON, regIpc);
    }
    else
    {
        regIpc = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _OFF, regIpc);
    }

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx), regIpc);
}

/*!
 * @brief  Sets the IPC reference value for the IPC instance specified by @ref ipcIdx.
 *
 * @param[in]  ipcIdx Index of the IPC instance to configure
 * @param[in]  refVal Reference value to configure for this IPC instance
 */
static void
s_pmgrTestIPCRefValSet
(
    LwU8  ipcIdx,
    LwU32 refVal
)
{
    LwU32 regIpcRef = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF(ipcIdx));

    // Configure and flush the config.
    regIpcRef = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_REF, _VALUE, refVal,
                    regIpcRef);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(ipcIdx), regIpcRef);
}

/*!
 * @brief  Snaps the value specified by @ref selIdx in @ref pSelIdx.
 *
 * @param[in]   selIdx  Index denoting the value to snap among
 *                      LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_<xyz>
 * @param[out]  pSelVal Snapped value specified by @ref selIdx
 */
static void
s_pmgrTestIpcDebugValueSnap
(
    LwU8   selIdx,
    LwU16 *pSelVal
)
{
    LwU32 regIpcDebug = REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG);

    regIpcDebug = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_DEBUG, _SEL_IDX, selIdx,
                    regIpcDebug);
    regIpcDebug = FLD_SET_DRF(_CPWR_THERM, _IPC_DEBUG, _MODE_CTRL, _SNAP,
                    regIpcDebug);
    regIpcDebug = FLD_SET_DRF(_CPWR_THERM, _IPC_DEBUG, _SNAP, _TRIGGER,
                    regIpcDebug);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_DEBUG, regIpcDebug);

    // Wait until snap is not completed.
    while (FLD_TEST_DRF(_CPWR_THERM, _IPC_DEBUG, _SNAP, _NOT_DONE,
            REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG)))
    {
        // NOP
    }

    // Read the snapped value.
    *pSelVal = (LwU16)DRF_VAL(_CPWR_THERM, _IPC_DEBUG, _SEL_VAL,
                        REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG));
}

/*!
 * @brief  Sets the beacon channel specified by @ref beaconChannelIdx to @ref beaconChannelVal.
 *
 * @param[in]  beaconChannelIdx  Index denoting the beacon channel whose value is to be set
 * @param[in]  beaconChannelVal  Value that the beacon channel specified by @ref beaconChannelIdx should be set to
 */
static void
s_pmgrTestADCBeaconChannelSet
(
    LwU8  beaconChannelIdx,
    LwU8  beaconChannelVal
)
{
    LwU32 regAdcCtrl2 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL2);

    regAdcCtrl2 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _BEACON, beaconChannelIdx,
                      beaconChannelVal, regAdcCtrl2);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL2, regAdcCtrl2);
}

/*!
 * @brief  Sets the threshold of the beacon channel specified by @ref beaconChannelIdx to @ref beaconChannelThreshold.
 *
 * @param[in]  beaconChannelIdx        Index denoting the beacon channel whose threshold is to be set
 * @param[in]  beaconChannelThreshold  Value that the threshold of the beacon channel specified by @ref beaconChannelIdx should be set to
 */
static void
s_pmgrTestADCBeaconThresholdSet
(
    LwU8  beaconChannelIdx,
    LwU8  beaconChannelThreshold
)
{
    LwU32 regAdcCtrl3 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL3);

    regAdcCtrl3 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL3, _BEACON_THRESH, beaconChannelIdx,
                    beaconChannelThreshold, regAdcCtrl3);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL3, regAdcCtrl3);
}

/*!
 * @brief  Sets the comparator of the beacon channel specified by @ref beaconChannelIdx to @ref beaconChannelComparator.
 *
 * @param[in]  beaconChannelIdx  Index denoting the beacon channel whose comparator is to be set
 * @param[in]  bGreaterThan      Boolean denoting whether the the comparator of the beacon channel specified by @ref beaconChannelIdx should
 *                               be set to Greater Than
 */
static void
s_pmgrTestADCBeaconComparatorSet
(
    LwU8    beaconChannelIdx,
    LwBool  bGreaterThan
)
{
    LwU32 regAdcCtrl3 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL3);

    if (bGreaterThan)
    {
        regAdcCtrl3 = FLD_IDX_SET_DRF(_CPWR_THERM, _ADC_CTRL3, _BEACON_COMP, beaconChannelIdx,
                _GT, regAdcCtrl3);
    }
    else
    {
        regAdcCtrl3 = FLD_IDX_SET_DRF(_CPWR_THERM, _ADC_CTRL3, _BEACON_COMP, beaconChannelIdx,
                _LT, regAdcCtrl3);
    }

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL3, regAdcCtrl3);
}

/*!
 * @brief  Enables the interrupt for the beacon channel specified by @ref beaconChannelIdx.
 *
 * @param[in]  beaconChannelIdx  Index denoting the beacon channel whose value is to be set
 * @param[in]  InterruptEnable   Boolean denoting whether the interrupt for the beacon channel specified by @ref beaconChannelIdx
 *                               is to be enabled or disabled
 */
static void
s_pmgrTestADCBeaconInterruptEnable
(
    LwU8    beaconChannelIdx,
    LwBool  bInterruptEnable
)
{
    LwU32 regIntrEn0 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);

    if (bInterruptEnable)
    {
        regIntrEn0 = FLD_IDX_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON_INTR, beaconChannelIdx,
                _ENABLED, regIntrEn0);
    }
    else
    {
        regIntrEn0 = FLD_IDX_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON_INTR, beaconChannelIdx,
                _DISABLED, regIntrEn0);
    }

    REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, regIntrEn0);
}

/*!
 * @brief  Clears the interrupt for the beacon channel specified by @ref beaconChannelIdx.
 *
 * @param[in]  beaconChannelIdx  Index denoting the beacon channel whose interrupt is to be cleared
 */
static void
s_pmgrTestADCBeaconInterruptClear
(
    LwU8    beaconChannelIdx
)
{
    LwU32 regIntr0 = REG_RD32(CSB, LW_CPWR_THERM_INTR_0);

    regIntr0 = FLD_IDX_SET_DRF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON_INTR, beaconChannelIdx,
                   _CLEAR, regIntr0);

    REG_WR32(CSB, LW_CPWR_THERM_INTR_0, regIntr0);
}

/*!
 * @brief  Sets the offset channel specified by @ref offsetChannelIdx to @ref offsetChannelVal.
 *
 * @param[in]  offsetChannelIdx  Index denoting the offset channel whose value is to be set
 * @param[in]  offsetChannelVal  Value that the offset channel specified by @ref offsetChannelIdx should be set to
 */
static void
s_pmgrTestADCOffsetChannelSet
(
    LwU8  offsetChannelIdx,
    LwU8  offsetChannelVal
)
{
    LwU32 regAdcCtrl2 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL2);

    regAdcCtrl2 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _OFFSET, offsetChannelIdx,
                      offsetChannelVal, regAdcCtrl2);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL2, regAdcCtrl2);
}

/*!
 * @brief  Sets the offset map of the channel specified by @ref channelIdx to @ref channelOffsetVal, 
 *         in the LW_CPWR_THERM_ADC_OFFSET_MAP register.
 *
 * @param[in]  channelIdx        Index denoting the channel whose offset is to be set
 * @param[in]  channelOffsetVal  Value that the offset of the channel specified by @ref channelIdx should be set to
 */
static void
s_pmgrTestADCOffsetMapSet
(
    LwU8  channelIdx,
    LwU8  channelOffsetVal
)
{
    LwU32 regAdcOffsetMap = REG_RD32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP);

    regAdcOffsetMap = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_OFFSET_MAP, _CH, channelIdx,
                          channelOffsetVal, regAdcOffsetMap);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP, regAdcOffsetMap);
}

/*!
 * @brief  Sets the offset map of the channel specified by @ref channelIdx to @ref channelOffsetVal, 
 *         in the LW_CPWR_THERM_ADC_OFFSET_MAP2 register.
 *
 * @param[in]  channelIdx        Index denoting the channel whose offset is to be set
 * @param[in]  channelOffsetVal  Value that the offset of the channel specified by @ref channelIdx should be set to
 */
static void
s_pmgrTestADCOffsetMap2Set
(
    LwU8  channelIdx,
    LwU8  channelOffsetVal
)
{
    LwU32 regAdcOffsetMap2 = REG_RD32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP2);

    regAdcOffsetMap2 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_OFFSET_MAP2, _CH, channelIdx,
                           channelOffsetVal, regAdcOffsetMap2);

    REG_WR32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP2, regAdcOffsetMap2);
}

/*!
 * @brief  Sets the value of the sum input specified by @ref sumInputIdx to @ref sumInputVal, 
 *         in the LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT register.
 *
 * @param[in]  sumInputIdx  Index denoting the input number (4 inputs possible for each SUM instance)
 * @param[in]  suminputVal  Value that the Input of the sum input specified by @ref sumInputIdx should be set to
 */
static void
s_pmgrTestIpcCtrlSum1InputSet
(
    LwU8  sumInputIdx,
    LwU8  sumInputVal
)
{
    LwU32 regIpcCtrlSum1Input = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT);

    regIpcCtrlSum1Input = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM1_INPUT, _INPUT, sumInputIdx,
                              sumInputVal, regIpcCtrlSum1Input);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT, regIpcCtrlSum1Input);
}

/*!
 * @brief  Sets the value of the sum input specified by @ref sumInputIdx to @ref sumInputVal, 
 *         in the LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT register.
 *
 * @param[in]  sumInputIdx  Index denoting the input number (4 inputs possible for each SUM instance)
 * @param[in]  suminputVal  Value that the Input of the sum input specified by @ref sumInputIdx should be set to
 */
static void
s_pmgrTestIpcCtrlSum2InputSet
(
    LwU8  sumInputIdx,
    LwU8  sumInputVal
)
{
    LwU32 regIpcCtrlSum2Input = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT);

    regIpcCtrlSum2Input = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM2_INPUT, _INPUT, sumInputIdx,
                              sumInputVal, regIpcCtrlSum2Input);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT, regIpcCtrlSum2Input);
}

/*!
 * @brief  Sets the SF of the sum input specified by @ref sumInputIdx to @ref sumSfVal, 
 *         in the LW_CPWR_THERM_IPC_CTRL_SUM1_SF register.
 *
 * @param[in]  sumInputIdx  Index denoting the input number (4 inputs possible for each SUM instance)
 * @param[in]  sumSfVal  Value that the SF of the sum input specified by @ref sumInputIdx should be set to
 */
static void
s_pmgrTestIpcCtrlSum1SfSet
(
    LwU8  sumInputIdx,
    LwU8  sumSfVal
)
{
    LwU32 regIpcCtrlSum1Sf = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_SF);

    regIpcCtrlSum1Sf = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM1_SF, _SF, sumInputIdx,
                              sumSfVal, regIpcCtrlSum1Sf);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_SF, regIpcCtrlSum1Sf);
}

/*!
 * @brief  Sets the SF of the sum input specified by @ref sumInputIdx to @ref sumSfVal, 
 *         in the LW_CPWR_THERM_IPC_CTRL_SUM2_SF register.
 *
 * @param[in]  sumInputIdx  Index denoting the input number (4 inputs possible for each SUM instance)
 * @param[in]  sumSfVal  Value that the SF of the sum input specified by @ref sumInputIdx should be set to
 */
static void
s_pmgrTestIpcCtrlSum2SfSet
(
    LwU8  sumInputIdx,
    LwU8  sumSfVal
)
{
    LwU32 regIpcCtrlSum2Sf = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_SF);

    regIpcCtrlSum2Sf = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM2_SF, _SF, sumInputIdx,
                              sumSfVal, regIpcCtrlSum2Sf);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_SF, regIpcCtrlSum2Sf);
}

/*!
 * @brief  Initialize and reset the ADC configuration.
 */
static void
s_pmgrTestADCInitAndReset(void)
{
    LwU8 idx;

    // Configure ADC debug register.
    s_pmgrTestADCDebugMinLimitSet(LW_CPWR_THERM_ADC_DEBUG_MIN_ADC_INIT);
    s_pmgrTestADCDebugMaxLimitSet(LW_CPWR_THERM_ADC_DEBUG_MAX_ADC_INIT);
    s_pmgrTestADCDebugStepPeriodSet(LW_CPWR_THERM_ADC_DEBUG_STEP_PERIOD_INIT);
    s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_DISABLED,
        LW_CPWR_THERM_ADC_DEBUG2_SW_FORCED_ADC_INIT);

    // Reset multiplier configuration for all channel pairs.
    for (idx = LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MIN;
         idx <= LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestChannelPairConfigure(
            idx,
            LW_CPWR_THERM_ADC_MUL_CTRL_DATA_VALUE_CHP_CFG_INIT,
            LW_CPWR_THERM_ADC_MUL_CTRL_DATA_VALUE_CHP_CFG_INIT,
            LW_FALSE);
    }

    // Reset ADC control configuration.
    s_pmgrTestADCSensingEnable(LW_FALSE);
    s_pmgrTestADCAclwmulationEnable(LW_FALSE);
    s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_INIT);
    s_pmgrTestADCIIRLengthSet(LW_CPWR_THERM_ADC_CTRL_ADC_IIR_LENGTH_INIT);
    s_pmgrTestADCSampleDelaySet(LW_CPWR_THERM_ADC_CTRL_SAMPLE_DELAY_INIT);

    // Reset the PWM Generator.
    s_pmgrTestADCPWMGenConfigure(LW_FALSE, LW_CPWR_THERM_ADC_PWM_GEN_HI_LAST_INIT);

    // Reset Beacon channel configuration.
    for (idx = 0; idx < LW_CPWR_THERM_ADC_CTRL2_BEACON__SIZE_1; idx++)
    {
        s_pmgrTestADCBeaconChannelSet(idx, LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE);
    }

    // Reset Offset channel configuration.
    for (idx = 0; idx < LW_CPWR_THERM_ADC_CTRL2_OFFSET__SIZE_1; idx++)
    {
        s_pmgrTestADCOffsetChannelSet(idx, LW_CPWR_THERM_ADC_CTRL2_OFFSET_NONE);
    }

    // Reset configuration of Offset Map registers.
    for (idx = 0; idx < LW_CPWR_THERM_ADC_OFFSET_MAP_CH__SIZE_1; idx++)
    {
        s_pmgrTestADCOffsetMapSet(idx, LW_CPWR_THERM_ADC_OFFSET_MAP_CH_NO_OFFSET);
    }
    for (idx = 0; idx < LW_CPWR_THERM_ADC_OFFSET_MAP2_CH__SIZE_1; idx++)
    {
        s_pmgrTestADCOffsetMap2Set(idx, LW_CPWR_THERM_ADC_OFFSET_MAP2_CH_NO_OFFSET);
    }

    // Reset configuration of IPC Sum Input registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_INPUT__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum1InputSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT_NO_CH);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT_INPUT__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum2InputSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT_NO_CH);
    }

    // Reset configuration of IPC Sum Scale Factor registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM1_SF_SF__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum1SfSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM1_SF_SF_INIT);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL_SUM2_SF_SF__SIZE_1; idx++)
    {
        s_pmgrTestIpcCtrlSum2SfSet(idx, LW_CPWR_THERM_IPC_CTRL_SUM2_SF_SF_INIT);
    }

    // Trigger SW reset.
    s_pmgrTestADCReset();
}

/*!
 * @brief  Save/restore the init values of the registers in the reg cache.
 *
 * @param[in]   bInit  Bool to save/restore regcache
 *
 * @return      FLCN_OK               Register cache save/restore successful
 */
static FLCN_STATUS
s_pmgrTestRegCacheInitRestore
(
    LwBool bInit
)
{
    if (pPmgrRegCache == NULL)
    {
        memset(&pmgrRegCache, 0, sizeof(PMGR_TEST_REG_CACHE));
        pPmgrRegCache = &pmgrRegCache;
    }

    if (bInit)
    {
        // Initialise the register cache.
        s_pmgrTestRegCacheInit();
    }
    else
    {
        // Restore the register cache.
        s_pmgrTestRegCacheRestore();
    }

    return FLCN_OK;
}

/*!
 * @brief  Save the current values of the registers in the register cache.
 */
static void
s_pmgrTestRegCacheInit()
{
    LwU8  idx;
    LwU32 reg32;

    // Cache debug registers.
    pPmgrRegCache->regAdcDebug = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG);
    pPmgrRegCache->regAdcDebug2 = REG_RD32(CSB, LW_CPWR_THERM_ADC_DEBUG2);
    pPmgrRegCache->regIpcDebug = REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG);

    // Cache the ADC_MUL_CTRL_DATA for each index and restore back ADC_MUL_CTRL_INDEX value.
    pPmgrRegCache->regMulCtrlIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX);
    for (idx = 0; idx <= LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; ++idx)
    {
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_INDEX, _INDEX,
                    idx, pPmgrRegCache->regMulCtrlIdx);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, reg32);

        pPmgrRegCache->regMulCtrlData[idx] =
            REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_DATA);
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, pPmgrRegCache->regMulCtrlIdx);

    // Cache all the indexes. No need to cache data as it's read-only.
    pPmgrRegCache->regIIRValIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX);
    pPmgrRegCache->regAccValIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX);
    pPmgrRegCache->regMulValIdx = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_INDEX);
    pPmgrRegCache->regMulAccValIdx =
        REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX);

    // Cache the ADC control registers.
    pPmgrRegCache->regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);
    pPmgrRegCache->regAdcSnapshot = REG_RD32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT);
    pPmgrRegCache->regAdcCtrl2 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL2);
    pPmgrRegCache->regAdcCtrl3 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL3);
    pPmgrRegCache->regAdcCtrl4 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL4);

    // Cache the PWM configuration registers.
    pPmgrRegCache->regAdcPwm    = REG_RD32(CSB, LW_CPWR_THERM_ADC_PWM);
    pPmgrRegCache->regAdcPwmGen = REG_RD32(CSB, LW_CPWR_THERM_ADC_PWM_GEN);

    // Cache the Interrupt registers.
    pPmgrRegCache->regIntrEn0 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
    pPmgrRegCache->regIntr0 = REG_RD32(CSB, LW_CPWR_THERM_INTR_0);

    // Cache the registers that maps the OFFSET to specific ADC channels.
    pPmgrRegCache->regAdcOffsetMap = REG_RD32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP);
    pPmgrRegCache->regAdcOffsetMap2 = REG_RD32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP2);

    pPmgrRegCache->regAdcSwReset = REG_RD32(CSB, LW_CPWR_THERM_ADC_RESET);

    // Cache the IPC control registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL__SIZE_1; ++idx)
    {
        pPmgrRegCache->regIpcCtrl[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL(idx));
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF__SIZE_1; ++idx)
    {
        pPmgrRegCache->regIpcRef[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF(idx));
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF_BOUND__SIZE_1 ; ++idx)
    {
        pPmgrRegCache->regIpcRefBound[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF_BOUND(idx));
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF_BOUND_CEIL__SIZE_1 ; ++idx)
    {
        pPmgrRegCache->regIpcRefBoundCeil[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF_BOUND_CEIL(idx));
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF_BOUND_FLOOR__SIZE_1 ; ++idx)
    {
        pPmgrRegCache->regIpcRefBoundFloor[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF_BOUND_FLOOR(idx));
    }

    pPmgrRegCache->regIpcCtrlIdx = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_INDEX);
    pPmgrRegCache->regIpcCtrlData = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA);

    pPmgrRegCache->regIpcCtrlSum1Input = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT);
    pPmgrRegCache->regIpcCtrlSum2Input = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT);

    pPmgrRegCache->regIpcCtrlSum1Sf = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_SF);
    pPmgrRegCache->regIpcCtrlSum2Sf = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_SF);
}

/*!
 * @brief  Restore the registers config from the register cache.
 */
static void
s_pmgrTestRegCacheRestore()
{
    LwU8  idx;
    LwU32 reg32;

    // Restore the debug registers.
    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG, pPmgrRegCache->regAdcDebug);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG2, pPmgrRegCache->regAdcDebug2);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_DEBUG, pPmgrRegCache->regIpcDebug);

    // Restore the ADC_MUL_CTRL_DATA for each index and restore back ADC_MUL_CTRL_INDEX value.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX);
    for (idx = 0; idx <= LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; ++idx)
    {
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_INDEX, _INDEX, idx,
                    reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, reg32);

        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_DATA,
            pPmgrRegCache->regMulCtrlData[idx]);
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, pPmgrRegCache->regMulCtrlIdx);

    // Restore all the indexes. No need to restore data as it's read-only.
    REG_WR32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX, pPmgrRegCache->regIIRValIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, pPmgrRegCache->regAccValIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_INDEX, pPmgrRegCache->regMulValIdx);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX,
        pPmgrRegCache->regMulAccValIdx);

    // Restore the ADC control registers.
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, pPmgrRegCache->regAdcCtrl);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT, pPmgrRegCache->regAdcSnapshot);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL2, pPmgrRegCache->regAdcCtrl2);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL3, pPmgrRegCache->regAdcCtrl3);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL4, pPmgrRegCache->regAdcCtrl4);

    // Restore the PWM configuration registers.
    REG_WR32(CSB, LW_CPWR_THERM_ADC_PWM, pPmgrRegCache->regAdcPwm);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_PWM_GEN, pPmgrRegCache->regAdcPwmGen);

    // Restore the Interrupt registers.
    REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, pPmgrRegCache->regIntrEn0);
    REG_WR32(CSB, LW_CPWR_THERM_INTR_0, pPmgrRegCache->regIntr0);

    // Restore the registers that maps the OFFSET to specific ADC channels.
    REG_WR32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP, pPmgrRegCache->regAdcOffsetMap);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP2, pPmgrRegCache->regAdcOffsetMap2);

    //
    // Wait for any pending resets and restore the SW_RESET register config
    // to avoid triggering a reset here.
    //
    while (FLD_TEST_DRF(_CPWR_THERM, _ADC_RESET, _SW_RST, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_RESET)))
    {
        // NOP
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_RESET, pPmgrRegCache->regAdcSwReset);

    // Restore the IPC control registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(idx), pPmgrRegCache->regIpcCtrl[idx]);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(idx), pPmgrRegCache->regIpcRef[idx]);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF_BOUND__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_REF_BOUND(idx), pPmgrRegCache->regIpcRefBound[idx]);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF_BOUND_CEIL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_REF_BOUND_CEIL(idx), pPmgrRegCache->regIpcRefBoundCeil[idx]);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF_BOUND_FLOOR__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_REF_BOUND_FLOOR(idx), pPmgrRegCache->regIpcRefBoundFloor[idx]);
    }

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_INDEX, pPmgrRegCache->regIpcCtrlIdx);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_DATA, pPmgrRegCache->regIpcCtrlData);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT, pPmgrRegCache->regIpcCtrlSum1Input);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT, pPmgrRegCache->regIpcCtrlSum2Input);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_SF, pPmgrRegCache->regIpcCtrlSum1Sf);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_SF, pPmgrRegCache->regIpcCtrlSum2Sf);

    // At the end trigger the reset so that we restore the GPU ADC in a sane config.
    s_pmgrTestADCReset();
}