/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmgrtestga100.c
 * @brief   PMU HAL functions related to PMGR tests for GA100+
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
#define PMGR_TEST_ADC_LIMIT_MIN                                         (0x58)
#define PMGR_TEST_ADC_STEP_PERIOD                                        (0x1)
#define PMGR_TEST_ADC_RAW_SHIFT_FACTOR                                    (25)
#define PMGR_TEST_ADC_PWM_PERIOD                                         (200)
#define PMGR_TEST_ADC_PWM_DUTYCYCLE                                      (100)
#define PMGR_TEST_ADC_SAMPLE_DELAY                                        (10)
#define PMGR_TEST_ADC_LIMIT_MIN_IPC                                       (64)
#define PMGR_TEST_ADC_LIMIT_MAX_IPC                                      (127)
#define PMGR_TEST_ADC_STEP_PERIOD_IPC                                     (15)

#define PMGR_TEST_IPC_REF_VAL_A                                   (0x20000000)
#define PMGR_TEST_IPC_REF_VAL_B                                   (0x08000000)
#define PMGR_TEST_IPC_REF_VAL_C                                   (0x30000000)
#define PMGR_TEST_IPC_REF_VAL_D                                   (0x10000000)
#define PMGR_TEST_IPC_REF_VAL_E                                   (0x09000000)
#define PMGR_TEST_IPC_REF_VAL_IPC_CHECK                           (0x2FE00000)
#define PMGR_TEST_IPC_IIR_DOWNSHIFT                                       (24)
#define PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_A                                 (20)
#define PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_B                                 (12)
#define PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_C                                  (8)
#define PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_D                                  (9)
#define PMGR_TEST_IPC_PROP_DOWNSHIFT_IPC_A                                (12)
#define PMGR_TEST_IPC_PROP_DOWNSHIFT_IPC_B                                 (5)
#define PMGR_TEST_IPC_IIR_GAIN                                             (4)

#define PMGR_TEST_EXPECTED_HI_OFFSET_A                                  (0x10)
#define PMGR_TEST_EXPECTED_HI_OFFSET_B                                   (0x1)
#define PMGR_TEST_EXPECTED_HI_OFFSET_C                                  (0x20)
#define PMGR_TEST_EXPECTED_HI_OFFSET_IPC_A                               (128)
#define PMGR_TEST_EXPECTED_HI_OFFSET_IPC_B                               (256)
#define PMGR_TEST_EXPECTED_HI_OFFSET_IPC_C                               (512)

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
    LwU32   regIpcDebug;
    LwU32   regMulCtrlIdx;
    LwU32   regMulCtrlData[LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX + 1];
    LwU32   regAdcCtrl;
    LwU32   regIIRValIdx;
    LwU32   regAccValIdx;
    LwU32   regMulValIdx;
    LwU32   regMulAccValIdx;
    LwU32   regAdcPwm;
    LwU32   regIpcCtrl[LW_CPWR_THERM_IPC_CTRL__SIZE_1];
    LwU32   regIpcRef[LW_CPWR_THERM_IPC_REF__SIZE_1];
    LwU32   regAdcSwReset;
} PMGR_TEST_REG_CACHE;


PMGR_TEST_REG_CACHE  pmgrRegCache
    GCC_ATTRIB_SECTION("dmem_libPmgrTest", "pmgrRegCache");
PMGR_TEST_REG_CACHE *pPmgrRegCache = NULL;

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
static void s_pmgrTestIPCParamsSet(LwU8 ipcIdx, LwU8 iirGain, LwU8 iirLength, LwU8 iirDownShift, LwU8 propDownShift)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCParamsSet");
static void s_pmgrTestIPCEnable(LwU8 ipcIdx, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCEnable");
static void s_pmgrTestIPCRefValSet(LwU8 ipcIdx, LwU32 refVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIPCRefValSet");
static void s_pmgrTestIpcDebugValueSnap(LwU8 selIdx, LwU16 *pSelVal)
    GCC_ATTRIB_SECTION("imem_libPmgrTest", "s_pmgrTestIpcDebugValueSnap");
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
pmgrTestAdcInit_GA100
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
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto pmgrTestAdcInit_GA100_exit;
    }

    s_pmgrTestADCInitAndReset();

    // Check if all IIR values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
         idx <= LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestIIRValForChannelGet(idx, &iirValue);

        if (iirValue != LW_CPWR_THERM_ADC_IIR_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_IIR_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }
    }

    // Check if all ACC values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
         idx <= LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestAccIIRValForChannelGet(idx, &accValueLB, &accValueUB, &accValueSCnt);

        if (accValueLB != LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_ACC_LB_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }

        if (accValueUB != LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_ACC_UB_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }

        if (accValueSCnt != LW_CPWR_THERM_ADC_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_ACC_SCNT_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }
    }

    // Check if all MUL values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
         idx <= LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestMulIIRValForChannelPairGet(idx, &mulValue);

        if (mulValue != LW_CPWR_THERM_ADC_MUL_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }
    }

    // Check if all aclwmulated MUL values are zero post reset.
    for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
         idx <= LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
    {
        s_pmgrTestAccMulIIRValForChannelPairGet(idx, &accMulValueLB, &accMulValueUB,
            &accMulValueSCnt);

        if (accMulValueLB != LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_ACC_LB_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }

        if (accMulValueUB != LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_ACC_UB_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }

        if (accMulValueSCnt != LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA_VALUE_INIT)
        {
            pParams->outStatus =
                LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT,
                NON_ZERO_MUL_ACC_SCNT_VALUE);
            goto pmgrTestAdcInit_GA100_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestAdcInit_GA100_exit:

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
pmgrTestAdcCheck_GA100
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU8  idx;
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
            goto pmgrTestAdcCheck_GA100_exit;
        }

        s_pmgrTestADCInitAndReset();

        // Configure ADC_DEBUG register.
        s_pmgrTestADCDebugMinLimitSet(PMGR_TEST_ADC_LIMIT_MIN);
        s_pmgrTestADCDebugMaxLimitSet(PMGR_TEST_ADC_LIMIT_MIN);
        s_pmgrTestADCDebugStepPeriodSet(PMGR_TEST_ADC_STEP_PERIOD);
        s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_INC,
            LW_CPWR_THERM_ADC_DEBUG_SW_FORCED_ADC_INIT);

        // Configure channel pairs.
        for (idx = LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MIN;
             idx <= LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_MAX; idx++)
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

        // Check if all IIR values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
             idx <= LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
        {
            s_pmgrTestIIRValForChannelGet(idx, &hwIIRVal);

            swIIRVal = ((PMGR_TEST_ADC_LIMIT_MIN - adcRawCorrVal) <<
                         PMGR_TEST_ADC_RAW_SHIFT_FACTOR);

            if (swIIRVal != hwIIRVal)
            {
                pParams->outStatus =
                    LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK,
                    IIR_VALUE_MISMATCH);
                goto pmgrTestAdcCheck_GA100_exit;
            }
        }

        // Check if all ACC values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MIN;
             idx <= LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_MAX; idx++)
        {
            // Previous sample.
            s_pmgrTestIIRValForChannelGet(idx, &hwIIRVal);
            hwIIRVal64 = (LwU64)hwIIRVal;

            s_pmgrTestAccIIRValForChannelGet(idx, &hwAccValLB, &hwAccValUB, &sampleCnt_P);
            hwAccVal64_P = ((((LwU64)hwAccValUB) << 32) | hwAccValLB);

            OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

            // Snap again to get updated values.
            s_pmgrTestADCValuesSnap();

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
                goto pmgrTestAdcCheck_GA100_exit;
            }
        }

        // Check if all MUL values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
             idx <= LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
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
                goto pmgrTestAdcCheck_GA100_exit;
            }
        }

        // Check if all aclwmulated MUL values confirm to the math.
        for (idx = LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MIN;
             idx <= LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_MAX; idx++)
        {
            // Previous sample
            s_pmgrTestMulIIRValForChannelPairGet(idx, &hwMulVal);
            hwMulVal64 = (LwU64)hwMulVal;

            s_pmgrTestAccMulIIRValForChannelPairGet(idx, &hwAccMulValueLB, &hwAccMulValueUB,
                &sampleCnt_P);
            hwAccMulVal64_P = ((((LwU64)hwAccMulValueUB) << 32) | hwAccMulValueLB);

            OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

            // Snap again to get updated values.
            s_pmgrTestADCValuesSnap();

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
                goto pmgrTestAdcCheck_GA100_exit;
            }
        }

        pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestAdcCheck_GA100_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
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
pmgrTestHiOffsetMaxCheck_GA100
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU16       hwHiOffset;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto pmgrTestHiOffsetMaxCheck_GA100_exit;
    }

    s_pmgrTestADCInitAndReset();

    // Configure ADC_DEBUG register.
    s_pmgrTestADCDebugMinLimitSet(PMGR_TEST_ADC_LIMIT_MIN);
    s_pmgrTestADCDebugMaxLimitSet(PMGR_TEST_ADC_LIMIT_MIN);
    s_pmgrTestADCDebugStepPeriodSet(PMGR_TEST_ADC_STEP_PERIOD);
    s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_INC,
        LW_CPWR_THERM_ADC_DEBUG_SW_FORCED_ADC_INIT);

    // Configure channel pair 0.
    s_pmgrTestChannelPairConfigure(LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_CHP0_CFG,
        LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH0,
        LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH1, LW_TRUE);

    // Configure ADC_CTRL and ADC_PWM and snap values.
    s_pmgrTestADCSensingEnable(LW_TRUE);
    s_pmgrTestADCAclwmulationEnable(LW_TRUE);
    s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX);

    s_pmgrTestADCPWMConfigure(PMGR_TEST_ADC_PWM_PERIOD, PMGR_TEST_ADC_PWM_DUTYCYCLE);

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Configure IPC 0.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CH0);
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
                          LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
                          PMGR_TEST_IPC_IIR_DOWNSHIFT,
                          LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(0, LW_TRUE);
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_A);

    // Configure IPC 1.
    s_pmgrTestIPCChannelSet(1, LW_CPWR_THERM_IPC_CTRL_OP_CHP0);
    s_pmgrTestIPCParamsSet(1, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
                          LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
                          PMGR_TEST_IPC_IIR_DOWNSHIFT,
                          LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(1, LW_TRUE);
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_B);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Get the HW HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET, &hwHiOffset);

    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_A)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            HI_OFFSET_A_FAILURE);
        goto pmgrTestHiOffsetMaxCheck_GA100_exit;
    }

    // Change ref val of IPC 0 so that IPC 1 contributes to HI_OFFSET.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_C);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET, &hwHiOffset);

    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_B)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            HI_OFFSET_B_FAILURE);
        goto pmgrTestHiOffsetMaxCheck_GA100_exit;
    }

    // Change ref val of IPC 0 and 1 so that IPC 0 now contributes to HI_OFFSET.
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_D);
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_E);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET, &hwHiOffset);

    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK,
            HI_OFFSET_C_FAILURE);
        goto pmgrTestHiOffsetMaxCheck_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestHiOffsetMaxCheck_GA100_exit:
    s_pmgrTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Test case to check if IPC functions properly.
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
pmgrTestIPCParamsCheck_GA100
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    LwU32       hwIIRVal_P;
    LwU32       hwIIRVal_C;
    LwU32       hwMulVal_P;
    LwU32       hwMulVal_C;
    LwU16       hwHiOffset_P;
    LwU16       hwHiOffset_C;
    LwU16       hwHiOffset;
    FLCN_STATUS status;

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_pmgrTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    s_pmgrTestADCInitAndReset();

    // Configure ADC_DEBUG register.
    s_pmgrTestADCDebugMinLimitSet(PMGR_TEST_ADC_LIMIT_MIN_IPC);
    s_pmgrTestADCDebugMaxLimitSet(PMGR_TEST_ADC_LIMIT_MAX_IPC);
    s_pmgrTestADCDebugStepPeriodSet(PMGR_TEST_ADC_STEP_PERIOD_IPC);
    s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_CMOV,
        LW_CPWR_THERM_ADC_DEBUG_SW_FORCED_ADC_INIT);

    // Configure channel pair 0.
    s_pmgrTestChannelPairConfigure(LW_CPWR_THERM_ADC_MUL_CTRL_INDEX_INDEX_CHP0_CFG,
        LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH0,
        LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH1, LW_TRUE);

    // Configure ADC_CTRL and ADC_PWM.
    s_pmgrTestADCSensingEnable(LW_TRUE);
    s_pmgrTestADCAclwmulationEnable(LW_TRUE);
    s_pmgrTestADCNumActiveChannelsSet(LW_CPWR_THERM_ADC_CTRL_ACTIVE_CHANNELS_NUM_MAX);
    s_pmgrTestADCSampleDelaySet(PMGR_TEST_ADC_SAMPLE_DELAY);
    s_pmgrTestADCPWMConfigure(PMGR_TEST_ADC_PWM_PERIOD, PMGR_TEST_ADC_PWM_DUTYCYCLE);

    // Configure IPC 0.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CH0);
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(0, LW_TRUE);
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_A);

    // Configure IPC 1.
    s_pmgrTestIPCChannelSet(1, LW_CPWR_THERM_IPC_CTRL_OP_CHP0);
    s_pmgrTestIPCParamsSet(1, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(1, LW_TRUE);
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_B);

    // Trigger SW reset.
    s_pmgrTestADCReset();

    // Wait for 1 millisecond to let the values populate.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Read values.
    s_pmgrTestIIRValForChannelGet(LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH0,
        &hwIIRVal_P);
    s_pmgrTestMulIIRValForChannelPairGet(LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP0,
        &hwMulVal_P);
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset_P);

    // Wait again for 1.2 millisecond to let the values change.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND +
        PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap values.
    s_pmgrTestADCValuesSnap();

    // Read values again.
    s_pmgrTestIIRValForChannelGet(LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH0,
        &hwIIRVal_C);
    s_pmgrTestMulIIRValForChannelPairGet(LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP0,
        &hwMulVal_C);
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset_C);

    // As ADC is configured in CMOV mode, previous and current values shouldn't be same.
    if (hwIIRVal_P == hwIIRVal_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            CMOV_IIR_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    if (hwMulVal_P == hwMulVal_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            CMOV_MUL_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    if (hwHiOffset_P == hwHiOffset_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            CMOV_HI_OFFSET_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    // Configure ADC_DEBUG register.
    s_pmgrTestADCDebugMinLimitSet(PMGR_TEST_ADC_LIMIT_MIN);
    s_pmgrTestADCDebugMaxLimitSet(PMGR_TEST_ADC_LIMIT_MIN);
    s_pmgrTestADCDebugStepPeriodSet(PMGR_TEST_ADC_STEP_PERIOD);
    s_pmgrTestADCDebugModeSet(LW_CPWR_THERM_ADC_DEBUG_MODE_INC,
        LW_CPWR_THERM_ADC_DEBUG_SW_FORCED_ADC_INIT);

    // Configure all instances of IPC and check if HI_OFFSET is correctly callwlated.
    s_pmgrTestIPCChannelSet(0, LW_CPWR_THERM_IPC_CTRL_OP_CH0);
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_A,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(0, LW_TRUE);
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_D);

    s_pmgrTestIPCChannelSet(1, LW_CPWR_THERM_IPC_CTRL_OP_CH1);
    s_pmgrTestIPCParamsSet(1, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_A,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(1, LW_TRUE);
    s_pmgrTestIPCRefValSet(1, PMGR_TEST_IPC_REF_VAL_D);

    s_pmgrTestIPCChannelSet(2, LW_CPWR_THERM_IPC_CTRL_OP_CH2);
    s_pmgrTestIPCParamsSet(2, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_A,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(2, LW_TRUE);
    s_pmgrTestIPCRefValSet(2, PMGR_TEST_IPC_REF_VAL_D);

    s_pmgrTestIPCChannelSet(3, LW_CPWR_THERM_IPC_CTRL_OP_CH3);
    s_pmgrTestIPCParamsSet(3, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_A,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(3, LW_TRUE);
    s_pmgrTestIPCRefValSet(3, PMGR_TEST_IPC_REF_VAL_D);

    // Wait for 1.2 millisecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_1_MILLISECOND +
        PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset);
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_IPC_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            IPC_ALL_EN_FALIURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    // Check with different parameters of IPC_CTRL.

    // Disbale all IPC instances expect 0th one.
    s_pmgrTestIPCEnable(1, LW_FALSE);
    s_pmgrTestIPCEnable(2, LW_FALSE);
    s_pmgrTestIPCEnable(3, LW_FALSE);

    // Configure IPC IIR_DOWNSHIFT and check if HI_OFFSET is callwlated correctly.
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_B,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);
    s_pmgrTestIPCEnable(0, LW_TRUE);
    s_pmgrTestIPCRefValSet(0, PMGR_TEST_IPC_REF_VAL_IPC_CHECK);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset);
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_IPC_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            IIR_DOWNSHIFT_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    // Configure PROPORTIONAL_DOWNSHIFT and disable IIR_DOWNSHIFT.
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_DOWNSHIFT_INIT,
        PMGR_TEST_IPC_PROP_DOWNSHIFT_IPC_A);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset);
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_IPC_C)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            PROP_DOWNSHIFT_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    // Configure both PROPORTIONAL_DOWNSHIFT and IIR_DOWNSHIFT.
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_C,
        PMGR_TEST_IPC_PROP_DOWNSHIFT_IPC_B);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset);
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_IPC_B)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            IIR_AND_PROP_DOWNSHIFT_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    // Change IIR_DOWNSHIFT and check HI_OFFSET again.
    s_pmgrTestIPCParamsSet(0, LW_CPWR_THERM_IPC_CTRL_IIR_GAIN_INIT,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_D,
        PMGR_TEST_IPC_PROP_DOWNSHIFT_IPC_B);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset);
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_IPC_A)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            IIR_DOWNSHIFT_CHANGE_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    // Configure IIR_GAIN and IIR_DOWNSHIFT.
    s_pmgrTestIPCParamsSet(0, PMGR_TEST_IPC_IIR_GAIN,
        LW_CPWR_THERM_IPC_CTRL_IIR_LENGTH_INIT,
        PMGR_TEST_IPC_IIR_DOWNSHIFT_IPC_B,
        LW_CPWR_THERM_IPC_CTRL_PROPORTIONAL_DOWNSHIFT_INIT);

    // Wait for 200 microsecond to let the HI_OFFSET be reflected.
    OS_PTIMER_SPIN_WAIT_US(PMGR_TEST_DELAY_200_MICROSECOND);

    // Snap HI_OFFSET value.
    s_pmgrTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_HI_OFFSET,
        &hwHiOffset);
    if (hwHiOffset != PMGR_TEST_EXPECTED_HI_OFFSET_IPC_A)
    {
        pParams->outStatus =
            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC;
        pParams->outData = LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK,
            IIR_GAIN_DOWNSHIFT_FAILURE);
        goto pmgrTestIPCParamsCheck_GA100_exit;
    }

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS;

pmgrTestIPCParamsCheck_GA100_exit:
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

    if (mode == LW_CPWR_THERM_ADC_DEBUG_MODE_SW_OVERRIDE)
    {
        regAdc = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_DEBUG, _SW_FORCED_ADC,
                    overrideVal, regAdc);
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_DEBUG, regAdc);
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
    LwU32 regAdcCtrl = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

    regAdcCtrl = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _SNAP, _TRIGGER, regAdcCtrl);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, regAdcCtrl);

    // Wait until snap is not completed.
    while (FLD_TEST_DRF(_CPWR_THERM, _ADC_CTRL, _SNAP, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL)))
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
 * @brief  Reset the ADC sensing feature.
 */
static void
s_pmgrTestADCReset(void)
{
    LwU32 regAdcReset = REG_RD32(CSB, LW_CPWR_THERM_ADC_SW_RESET);

    regAdcReset = FLD_SET_DRF(_CPWR_THERM, _ADC_SW_RESET, _RST, _TRIGGER,
                    regAdcReset);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_SW_RESET, regAdcReset);

    // Wait until reset is not completed.
    while (FLD_TEST_DRF(_CPWR_THERM, _ADC_SW_RESET, _RST, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_SW_RESET)))
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
                        _VALUE_CHP_CFG_ENABLE, _YES, regIdxData);
    }
    else
    {
        regIdxData = FLD_SET_DRF(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                        _VALUE_CHP_CFG_ENABLE, _NO, regIdxData);
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

    // Set channel pair index.
    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP, chIdx, regIpc);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx), regIpc);
}

/*!
 * @brief  Sets the IPC parameters for the IPC instance specified by @ref ipcIdx.
 *
 * @param[in]  ipcIdx           Index of the IPC instance to configure
 * @param[in]  iirGain          IIR Gain to configure
 * @param[in]  iirDownShift     IIR Downshift to configure
 * @param[in]  propDownShift    Proportional Downshift to configure
 */
static void
s_pmgrTestIPCParamsSet
(
    LwU8 ipcIdx,
    LwU8 iirGain,
    LwU8 iirLength,
    LwU8 iirDownShift,
    LwU8 propDownShift
)
{
    LwU32 regIpc = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx));

    // Configure and flush the config.
    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _IIR_GAIN, iirGain,
                regIpc);
    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _IIR_LENGTH, iirLength,
                regIpc);
    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _IIR_DOWNSHIFT,
                iirDownShift, regIpc);
    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _PROPORTIONAL_DOWNSHIFT,
                propDownShift, regIpc);

    REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(ipcIdx), regIpc);
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
        LW_CPWR_THERM_ADC_DEBUG_SW_FORCED_ADC_INIT);

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
    pPmgrRegCache->regAdcPwm = REG_RD32(CSB, LW_CPWR_THERM_ADC_PWM);
    pPmgrRegCache->regAdcSwReset = REG_RD32(CSB, LW_CPWR_THERM_ADC_SW_RESET);

    // Cache the IPC control registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL__SIZE_1; ++idx)
    {
        pPmgrRegCache->regIpcCtrl[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL(idx));
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF__SIZE_1; ++idx)
    {
        pPmgrRegCache->regIpcRef[idx] = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF(idx));
    }
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
    REG_WR32(CSB, LW_CPWR_THERM_ADC_PWM, pPmgrRegCache->regAdcPwm);

    //
    // Wait for any pending resets and restore the SW_RESET register config
    // to avoid triggering a reset here.
    //
    while (FLD_TEST_DRF(_CPWR_THERM, _ADC_SW_RESET, _RST, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_ADC_SW_RESET)))
    {
        // NOP
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_SW_RESET, pPmgrRegCache->regAdcSwReset);

    // Restore the IPC control registers.
    for (idx = 0; idx < LW_CPWR_THERM_IPC_CTRL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(idx), pPmgrRegCache->regIpcCtrl[idx]);
    }
    for (idx = 0; idx < LW_CPWR_THERM_IPC_REF__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(idx), pPmgrRegCache->regIpcRef[idx]);
    }

    // At the end trigger the reset so that we restore the GPU ADC in a sane config.
    s_pmgrTestADCReset();
}

