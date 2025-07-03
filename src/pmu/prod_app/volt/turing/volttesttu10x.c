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
 * @file    volttesttu10x.c
 * @brief   PMU HAL functions related to volt tests for TU10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "pmu_objpmu.h"

#include "config/g_volt_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Multiple PWM values used in test.
#define VOLT_TEST_PWM_PERIOD                                             (0xa0)
#define VOLT_TEST_PWM_HI_MIN                                             (0x50)
#define VOLT_TEST_PWM_DELTA                                              (0x10)
#define VOLT_TEST_PWM_DELTA_DROOPY                                        (0x5)
#define VOLT_TEST_PWM_HI_OFFSET_ZERO                                      (0x0)

// Macro indicating MAX number of EDPP events.
#define VOLT_TEST_MAX_EDPP_EVENTS                                           (2)

// Fixed slew rate config macros.
#define VOLT_TEST_VID3_PWM_RISING                                         (0x5)
#define VOLT_TEST_VID3_PWM_FALLING                                        (0x5)
#define VOLT_TEST_VID3_PWM_REPEAT                                         (0x2)
#define VOLT_TEST_VID3_PWM_STEP                                           (0x4)

// Delay for PWM to colwerge when using fixed slew rate.
#define VOLT_TEST_DELAY_50US_FOR_PWM_COLWERGE      (50)

// 1 Microsecond delay used to test thermal monitors over EDPP events.
#define VOLT_TEST_DELAY_1_MICROSECOND      (1)

// Slowdown factor for EDPP_FONLY event.
#define VOLT_TEST_EDPP_FONLY_SLOWDOWN_FACTOR                              (0x2)

/* ------------------------- Types Definitions ------------------------------ */
/*
 * This structure is used to cache all HW values for registers that each
 * test uses and restore them back after each test/subtest completes. Inline
 * comments for each variable to register mapping.
 */
typedef struct
{
    LwU32   regVid0[LW_CPWR_THERM_VID0_PWM__SIZE_1];             // LW_CPWR_THERM_VID0_PWM
    LwU32   regVid1[LW_CPWR_THERM_VID1_PWM__SIZE_1];             // LW_CPWR_THERM_VID1_PWM
    LwU32   regVid2[LW_CPWR_THERM_VID2_PWM__SIZE_1];             // LW_CPWR_THERM_VID2_PWM
    LwU32   regVid3[LW_CPWR_THERM_VID3_PWM__SIZE_1];             // LW_CPWR_THERM_VID3_PWM
    LwU32   regIpcDebug;                                         // LW_CPWR_THERM_IPC_DEBUG
    LwU32   regIntrEnable;                                       // LW_CPWR_THERM_EVENT_INTR_ENABLE
    LwU32   regUseA;                                             // LW_CPWR_THERM_USE_A
    LwU32   regMonCtrl[LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1]; // LW_CPWR_THERM_INTR_MONITOR_CTRL
    LwU32   regEDPpEvents[VOLT_TEST_MAX_EDPP_EVENTS];            // LW_CPWR_THERM_EVT_EDPP_*
    LwU32   regIntrType;                                         // LW_CPWR_THERM_EVENT_INTR_TYPE
    LwU32   regIntrOnTriggered;                                  // LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED
    LwU32   regIntrOnNormal;                                     // LW_CPWR_THERM_EVENT_INTR_ON_NORMAL
} VOLT_TEST_REG_CACHE;

VOLT_TEST_REG_CACHE  voltRegCache
    GCC_ATTRIB_SECTION("dmem_libVoltTest", "voltRegCache");
VOLT_TEST_REG_CACHE *pVoltRegCache = NULL;

/* ------------------------- Public Function Prototypes  -------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static void s_voltTestAssertedEDPpEventsGet(LwBool *pBEDPpVminAsserted, LwBool *pBEDPpFOnlyAsserted)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestAssertedEDPpEventsGet");
static void s_voltTestPwmPeriodSet(LwU8 idx, LwU16 pwmPeriod)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestPwmPeriodSet");
static void s_voltTestPwmDutycycleSet(LwU8 idx, LwU16 pwmDutycycle)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestPwmDutycycleSet");
static void s_voltTestIpcVminSet(LwU8 idx, LwU16 pwmVmin)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestIpcVminSet");
static void s_voltTestIpcDebugValueSnap(LwU8 selIdx, LwU16 *pSelVal)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestIpcDebugValueSnap");
static void s_voltTestUseIpcEnable(LwU8 idx, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestUseIpcEnable");
static void s_voltTestConfigureFixedSlewRate(LwU8 idx, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestConfigureFixedSlewRate");
static void s_voltTestTriggerVidPwmSettings(LwU8 idx)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestTriggerVidPwmSettings");
static void s_voltTestClearDisableInterruptsSlowdownForEDPpEvents(LwU32 regIntrEnable, LwU32 regUseA)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestClearDisableInterruptsSlowdownForEDPpEvents");
static void s_voltTestClearEDPpInterrupts(void)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestClearEDPpInterrupts");
static void s_voltTestConfigureEDPpInterrupts(LwU32 regIntrType, LwU32 regIntrOnTriggered, LwU32 regIntrOnNormal)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestConfigureEDPpInterrupts");
static FLCN_STATUS s_voltTestRegCacheInitRestore(LwBool bInit)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestRegCacheInitRestore");
static void s_voltTestRegCacheInit(void)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestRegCacheInit");
static void s_voltTestRegCacheRestore(void)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestRegCacheRestore");
static void s_voltTestConfigureEDPpSlowdown(LwU32 slowdownFactor, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libVoltTest", "s_voltTestConfigureEDPpSlowdown");

/* ------------------------- Compile Time Checks ---------------------------- */
ct_assert(LW_CPWR_THERM_VID0_PWM__SIZE_1 == LW_CPWR_THERM_VID1_PWM__SIZE_1);
ct_assert(LW_CPWR_THERM_VID1_PWM__SIZE_1 == LW_CPWR_THERM_VID2_PWM__SIZE_1);
ct_assert(LW_CPWR_THERM_VID2_PWM__SIZE_1 == LW_CPWR_THERM_VID3_PWM__SIZE_1);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Test Vmin Cap for PWM VID.
 *
 * @param[out]  pParams   Test Params indicating status of test
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
voltTestVminCap_TU10X
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    LwU8        idx;
    LwU16       postVminCapVal;

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_voltTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto voltTestVminCap_TU10X_exit;
    }

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        // Set PWM Period.
        s_voltTestPwmPeriodSet(idx, VOLT_TEST_PWM_PERIOD);

        // Set PWM Dutycycle.
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN - VOLT_TEST_PWM_DELTA));

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Disable overriding of HI_OFFSET.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_FALSE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        // Snap Post Vmin cap result & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &postVminCapVal);

        if (postVminCapVal != VOLT_TEST_PWM_HI_MIN)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(VMIN_CAP,
                VMIN_CAP_FAILURE);
            goto voltTestVminCap_TU10X_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS;

voltTestVminCap_TU10X_exit:

    s_voltTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Negative test to check for Vmin Cap for PWM VID.
 *
 * @param[out]  pParams   Test Params indicating status of test
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
voltTestVminCapNegativeCheck_TU10X
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    LwU8        idx;
    LwU16       postVminCapVal;

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_voltTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto voltTestVminCapNegativeCheck_TU10X_exit;
    }

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        // Set PWM Period.
        s_voltTestPwmPeriodSet(idx, VOLT_TEST_PWM_PERIOD);

        //
        // Set PWM dutycycle above Vmin and no capping should occur when
        // HI_OFFSET is zero.
        //
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Override HI_OFFSET to zero.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Snap Post Vmin cap result & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &postVminCapVal);

        if (postVminCapVal != (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA))
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(VMIN_CAP_NEGATIVE,
                UNEXPECTED_VOLTAGE);
            goto voltTestVminCapNegativeCheck_TU10X_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS;

voltTestVminCapNegativeCheck_TU10X_exit:

    s_voltTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Test droopy engagement behavior.
 *
 * @param[out]  pParams   Test Params indicating status of test
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
voltTestDroopyEngage_TU10X
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    LwU8        idx;
    LwU16       postVminCapVal;

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_voltTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto voltTestDroopyEngage_TU10X_exit;
    }

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        // Set PWM Period.
        s_voltTestPwmPeriodSet(idx, VOLT_TEST_PWM_PERIOD);

        //
        // Set PWM dutycycle above Vmin and no capping should occur when
        // HI_OFFSET is disabled.
        //
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Override HI_OFFSET to zero.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_FALSE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Snap Post Vmin cap result & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &postVminCapVal);
        if (postVminCapVal != (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA))
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(DROOPY_ENGAGE,
                UNEXPECTED_VOLTAGE_ON_ZERO_HI_OFFSET);
            goto voltTestDroopyEngage_TU10X_exit;
        }

        // Enable and override HI_OFFSET to a value & check with expected value.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, VOLT_TEST_PWM_DELTA_DROOPY);

        // Snap Post Vmin cap result & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &postVminCapVal);
        if (postVminCapVal != ((VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA) -
                                VOLT_TEST_PWM_DELTA_DROOPY))
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(DROOPY_ENGAGE,
                UNEXPECTED_VOLTAGE_ON_HI_OFFSET);
            goto voltTestDroopyEngage_TU10X_exit;
        }

        // Enable and override HI_OFFSET so that net PWM value is below IPC Vmin.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, (VOLT_TEST_PWM_DELTA +
                                                  VOLT_TEST_PWM_DELTA_DROOPY));

        // Snap Post Vmin cap result & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &postVminCapVal);
        if (postVminCapVal != VOLT_TEST_PWM_HI_MIN)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(DROOPY_ENGAGE,
                VMIN_CAP_FAILURE);
            goto voltTestDroopyEngage_TU10X_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS;

voltTestDroopyEngage_TU10X_exit:

    s_voltTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Test EDPp Therm Events.
 *
 * @param[out]  pParams   Test Params indicating status of test
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
voltTestEDPpThermEvents_TU10X
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    LwU8        idx;
    LwBool      bEDPpVminAsserted;
    LwBool      bEDPpFOnlyAsserted;
    LwU32       reg32;

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_voltTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto voltTestEDPpThermEvents_TU10X_exit;
    }

    // Configure EDPp interrupts.
    s_voltTestConfigureEDPpInterrupts(pVoltRegCache->regIntrType,
        pVoltRegCache->regIntrOnTriggered, pVoltRegCache->regIntrOnNormal);

    // Clear pending EDPp events and disable EDPp interrupts and slowdown.
    s_voltTestClearDisableInterruptsSlowdownForEDPpEvents(
        pVoltRegCache->regIntrEnable, pVoltRegCache->regUseA);

    // Configure EDPP_FONLY slowdown.
    s_voltTestConfigureEDPpSlowdown(VOLT_TEST_EDPP_FONLY_SLOWDOWN_FACTOR, LW_TRUE);

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        //
        // 1. Set PWM dutycycle below Vmin, only EDPP_VMIN is expected to be asserted.
        // EDPP_FONLY is not expected to be asserted.

        // Set PWM period.
        s_voltTestPwmPeriodSet(idx, VOLT_TEST_PWM_PERIOD);

        // Set PWM dutycycle above Vmin.
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Override HI_OFFSET to zero.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_FALSE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Clear EDPp interrupts to ensure there are no pending interrupts.
        s_voltTestClearEDPpInterrupts();

        // Set PWM dutycycle below Vmin, only EDPP_VMIN is expected to be asserted.
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN - VOLT_TEST_PWM_DELTA));

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        s_voltTestAssertedEDPpEventsGet(&bEDPpVminAsserted, &bEDPpFOnlyAsserted);
        if (!bEDPpVminAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_VMIN_ASSERTION_FAILED);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        if (bEDPpFOnlyAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_FONLY_UNEXPECTED_ASSERTION);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        //
        // 2. Set PWM dutycycle above Vmin, EDPP_VMIN is expected to be deasserted.
        // For deassertion we cant check *_INTR_PENDING as deasertion doesnt
        // clear the *_INTR_PEINDING state. So check *_INTR_TRIGGER.
        //
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if ((reg32 & LW_CPWR_THERM_EVENT_MASK_EDPP) != 0)
        {
            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_VMIN), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_VMIN_DEASSERTION_FAILED);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }

            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_FONLY), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_FONLY_UNEXPECTED_ASSERTION);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }
        }
        // Clear EDPp interrupts to ensure there are no pending interrupts.
        s_voltTestClearEDPpInterrupts();

        // 3. Set IPC Vmin above dutycycle, EDPP_VMIN is expected to be asserted.
        s_voltTestPwmDutycycleSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        s_voltTestAssertedEDPpEventsGet(&bEDPpVminAsserted, &bEDPpFOnlyAsserted);
        if (!bEDPpVminAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_VMIN_ASSERTION_FAILED);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        if (bEDPpFOnlyAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_FONLY_UNEXPECTED_ASSERTION);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        // 4. Set Vmin below PWM dutycycle, EDPP_VMIN is expected to be deasserted.
        s_voltTestIpcVminSet(idx, (VOLT_TEST_PWM_HI_MIN - VOLT_TEST_PWM_DELTA));

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if ((reg32 & LW_CPWR_THERM_EVENT_MASK_EDPP) != 0)
        {
            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_VMIN), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_VMIN_DEASSERTION_FAILED);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }

            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_FONLY), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_FONLY_UNEXPECTED_ASSERTION);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }
        }

        // Clear EDPp interrupts to ensure there are no pending interrupts.
        s_voltTestClearEDPpInterrupts();

        //
        // 5. On overriding of HI_OFFSET but no IPC Vmin Cap,
        // only EDPP_FONLY should be asserted.
        //
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Override HI_OFFSET.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, VOLT_TEST_PWM_DELTA);

        s_voltTestAssertedEDPpEventsGet(&bEDPpVminAsserted, &bEDPpFOnlyAsserted);
        if (!bEDPpFOnlyAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_FONLY_ASSERTION_FAILED);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        if (bEDPpVminAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_VMIN_UNEXPECTED_ASSERTION);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        // 6. On disabling of overriden HI_OFFSET EDPP_FONLY should be deasserted.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_FALSE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if ((reg32 & LW_CPWR_THERM_EVENT_MASK_EDPP) != 0)
        {
            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_FONLY), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_FONLY_DEASSERTION_FAILED);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }

            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_VMIN), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_VMIN_UNEXPECTED_ASSERTION);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }
        }

        // Clear EDPp interrupts to ensure there are no pending interrupts.
        s_voltTestClearEDPpInterrupts();

        //
        // 7. On overriding of HI_OFFSET with IPC Vmin Cap,
        // both EDPP_VMIN and EDPP_FONLY should be asserted.
        //
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA));

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Set HI_OFFSET override such that IPC Vmin cap oclwrs.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, (2*VOLT_TEST_PWM_DELTA));

        s_voltTestAssertedEDPpEventsGet(&bEDPpVminAsserted, &bEDPpFOnlyAsserted);
        if (!bEDPpFOnlyAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_FONLY_ASSERTION_FAILED);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        if (!bEDPpVminAsserted)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                EDPP_VMIN_ASSERTION_FAILED);
            goto voltTestEDPpThermEvents_TU10X_exit;
        }

        //
        // 8. On disabling of overriden HI_OFFSET both EDPP_VMIN and EDPP_FONLY
        // should be deasserted since no Vmin Cap or HI_OFFSET is available.
        //
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_FALSE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER);
        if ((reg32 & LW_CPWR_THERM_EVENT_MASK_EDPP) != 0)
        {
            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_FONLY), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_FONLY_DEASSERTION_FAILED);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }

            if (RM_PMU_THERM_IS_BIT_SET(DRF_BASE(LW_CPWR_THERM_EVENT_TRIGGER_EDPP_VMIN), reg32))
            {
                pParams->outStatus =
                    LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS,
                    EDPP_VMIN_DEASSERTION_FAILED);
                goto voltTestEDPpThermEvents_TU10X_exit;
            }
        }

        // Clear EDPp interrupts to ensure there are no pending interrupts.
        s_voltTestClearEDPpInterrupts();
    }

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS;

voltTestEDPpThermEvents_TU10X_exit:

    // Clear EDPP_FONLY slowdown.
    s_voltTestConfigureEDPpSlowdown(VOLT_TEST_EDPP_FONLY_SLOWDOWN_FACTOR, LW_FALSE);

    s_voltTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Test Thermal Monitors over EDPp events.
 *
 * @param[out]  pParams   Test Params indicating status of test
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
voltTestThermMonitorsOverEDPpEvents_TU10X
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    LwU8        monSource = LW_CPWR_THERM_INTR_MONITOR_CTRL_SOURCE_NONE;
    FLCN_STATUS status;
    LwU8        idxMon;
    LwU8        idxEDPp;
    LwU8        idxPwmVid;
    LwU32       reg32;
    LwU32       val1;
    LwU32       val2;

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;

    // vhiwarkar TODO: Use HW macro instead of VOLT_TEST_MAX_EDPP_EVENTS

    // Configure EDPP_FONLY slowdown.
    s_voltTestConfigureEDPpSlowdown(VOLT_TEST_EDPP_FONLY_SLOWDOWN_FACTOR, LW_TRUE);

    // Cache all registers this test will modify.
    status = s_voltTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto voltTestThermMonitorOverEDPpEvents_exit;
    }

    // Clear pending EDPp events and disable EDPp interrupts and slowdown.
    s_voltTestClearDisableInterruptsSlowdownForEDPpEvents(
        pVoltRegCache->regIntrEnable, pVoltRegCache->regUseA);

    //
    // For each PWM_VID and for each EDPP_* event, trigger the respective EDPP_*
    // event in HW and check if each of the THERM monitor can count over the event.
    //
    for (idxPwmVid = 0; idxPwmVid < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idxPwmVid)
    {
        FOR_EACH_INDEX_IN_MASK(32, idxEDPp, LW_CPWR_THERM_EVENT_MASK_EDPP)
        {
            switch (idxEDPp)
            {
                case LW_CPWR_THERM_EVENT_EDPP_VMIN:
                {
                    // Force EDPP_VMIN in HW by setting PWM period less that Vmin.
                    s_voltTestPwmPeriodSet(idxPwmVid, VOLT_TEST_PWM_PERIOD);
                    s_voltTestIpcVminSet(idxPwmVid, VOLT_TEST_PWM_HI_MIN);
                    s_voltTestPwmDutycycleSet(idxPwmVid, (VOLT_TEST_PWM_HI_MIN - VOLT_TEST_PWM_DELTA));
                    s_voltTestUseIpcEnable(idxPwmVid, LW_TRUE);
                    s_voltTestTriggerVidPwmSettings(idxPwmVid);
                    monSource = LW_CPWR_THERM_INTR_MONITOR_CTRL_SOURCE_EDPP_VMIN;
                    break;
                }
                case LW_CPWR_THERM_EVENT_EDPP_FONLY:
                {
                    // Force EDPP_FONLY in HW by forcing HI_OFFSET in HW.
                    voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, VOLT_TEST_PWM_DELTA);
                    monSource = LW_CPWR_THERM_INTR_MONITOR_CTRL_SOURCE_EDPP_FONLY;
                    break;
                }
                default:
                {
                    pParams->outStatus =
                        LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                    pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(THERM_MON_EDPP,
                        UNKNOWN_EDPP_EVENT);
                }
            }

            // Loop over each Therm Monitor.
            for (idxMon = 0; idxMon < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idxMon)
            {
                // Clear & Enable the Monitor.
                reg32 = pVoltRegCache->regMonCtrl[idxMon];
                reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                            _EN, _ENABLE, reg32);
                reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                            _CLEAR, _TRIGGER, reg32);
                REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idxMon), reg32);

                // Configure the monitor to monitor Active High, Level Triggered EDPp event.
                reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idxMon));
                reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                            _TYPE, _LEVEL, reg32);
                reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL,
                            _POLARITY, _HIGH_ACTIVE, reg32);
                reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _INTR_MONITOR_CTRL,
                            _SOURCE, monSource, reg32);
                REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idxMon), reg32);

                // Read the monitor value for first time.
                val1 = DRF_VAL(_CPWR_THERM, _INTR_MONITOR_STATE, _VALUE,
                            REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_STATE(idxMon)));

                // Wait for 1 millisecond to let the monitor value increment.
                OS_PTIMER_SPIN_WAIT_US(VOLT_TEST_DELAY_1_MICROSECOND);

                // Read the monitor value again for the second time.
                val2 = DRF_VAL(_CPWR_THERM, _INTR_MONITOR_STATE, _VALUE,
                            REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_STATE(idxMon)));

                // Bail out if the value read second time is not more than the first one.
                if (val2 <= val1)
                {
                    pParams->outStatus =
                        LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
                    pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(THERM_MON_EDPP,
                        INCREMENT_FAILURE);
                    goto voltTestThermMonitorOverEDPpEvents_exit;
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

    }

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS;

voltTestThermMonitorOverEDPpEvents_exit:
    // Clear EDPP_FONLY slowdown.
    s_voltTestConfigureEDPpSlowdown(VOLT_TEST_EDPP_FONLY_SLOWDOWN_FACTOR, LW_FALSE);
    s_voltTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Test fixed slew rate behavior.
 *
 * @param[out]  pParams   Test Params indicating status of test
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
voltTestFixedSlewRate_TU10X
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status;
    LwU8        idx;
    LwU16       pwmHiActive;

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;

    // Cache all registers this test will modify.
    status = s_voltTestRegCacheInitRestore(LW_TRUE);
    if (status != FLCN_OK)
    {
        goto voltTestFixedSlewRate_TU10X_exit;
    }

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        //
        // 1. Set PWM dutycycle above Vmin and no capping should occur when
        // HI_OFFSET is zero. This ensures we flow the pwm as it is forward to the PWM VID
        // when USE_FIXED_SLEWRATE is _DISABLE.
        //

        // Set PWM Period.
        s_voltTestPwmPeriodSet(idx, VOLT_TEST_PWM_PERIOD);

        s_voltTestPwmDutycycleSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Disable IPC.
        s_voltTestUseIpcEnable(idx, LW_FALSE);

        // Disable fixed slew rate config.
        s_voltTestConfigureFixedSlewRate(idx, LW_FALSE);

        // Override HI_OFFSET to zero.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_FALSE, VOLT_TEST_PWM_HI_OFFSET_ZERO);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Snap active PWM & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &pwmHiActive);
        if (pwmHiActive != VOLT_TEST_PWM_HI_MIN)
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(FIXED_SLEW_RATE,
                UNEXPECTED_VOLTAGE_ON_FSR_DISABLED);
            goto voltTestFixedSlewRate_TU10X_exit;
        }

        //
        // 2. Change PWM dutycycle to check for colwergence of active PWM
        // when USE_FIXED_SLEWRATE is _ENABLE and USE_IPC is _DISABLE.
        //
        s_voltTestPwmDutycycleSet(idx, (VOLT_TEST_PWM_HI_MIN + 2*VOLT_TEST_PWM_DELTA));

        // Enable and program fixed slew rate config.
        s_voltTestConfigureFixedSlewRate(idx, LW_TRUE);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Wait sufficient time for PWM to colwerge.
        OS_PTIMER_SPIN_WAIT_US(VOLT_TEST_DELAY_50US_FOR_PWM_COLWERGE);

        // Snap active PWM & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &pwmHiActive);
        if (pwmHiActive != (VOLT_TEST_PWM_HI_MIN + 2 * VOLT_TEST_PWM_DELTA))
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(FIXED_SLEW_RATE,
                UNEXPECTED_VOLTAGE_ON_FSR_ENABLED);
            goto voltTestFixedSlewRate_TU10X_exit;
        }

        //
        // 3. Change PWM dutycycle to check for colwergence of active PWM
        // when USE_FIXED_SLEWRATE is _ENABLE and USE_IPC is _ENABLE.
        //

        // Enable IPC.
        s_voltTestUseIpcEnable(idx, LW_TRUE);

        // Set IPC Vmin.
        s_voltTestIpcVminSet(idx, VOLT_TEST_PWM_HI_MIN);

        // Trigger change.
        s_voltTestTriggerVidPwmSettings(idx);

        // Enable and override HI_OFFSET to a value & check.
        voltTestHiOffsetOverrideEnable_HAL(&Volt, LW_TRUE, VOLT_TEST_PWM_DELTA);

        // Wait sufficient time for PWM to colwerge.
        OS_PTIMER_SPIN_WAIT_US(VOLT_TEST_DELAY_50US_FOR_PWM_COLWERGE);

        // Snap active PWM & compare with expected value.
        s_voltTestIpcDebugValueSnap(LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_PWM_HI_ACTIVE(idx), &pwmHiActive);
        if (pwmHiActive != (VOLT_TEST_PWM_HI_MIN + VOLT_TEST_PWM_DELTA))
        {
            pParams->outStatus =
                LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC;
            pParams->outData = LW2080_CTRL_VOLT_TEST_STATUS(FIXED_SLEW_RATE,
                UNEXPECTED_VOLTAGE_ON_FSR_IPC_ENABLED);
            goto voltTestFixedSlewRate_TU10X_exit;
        }
    }

    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS;

voltTestFixedSlewRate_TU10X_exit:

    s_voltTestRegCacheInitRestore(LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief  Enables/Disables the IPC HI_OFFSET override.
 *
 * @param[in]   bOverride   Boolean indicating if we need to override IPC HI_OFFSET.
 *                          LW_FALSE disables the override.
 * @param[in]   pwmHiOffset HI_OFFSET given in units of PWM to set if @ref
 *                          bOverride is LW_TRUE.
 */
void
voltTestHiOffsetOverrideEnable_TU10X
(
    LwBool bOverride,
    LwU16  pwmHiOffset
)
{
    LwU32 regIpcDebug = REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG);

    if (bOverride)
    {
        regIpcDebug = FLD_SET_DRF(_CPWR_THERM, _IPC_DEBUG, _IPC_OVERRIDE_EN,
                        _ON, regIpcDebug);
        regIpcDebug = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_DEBUG,
                        _IPC_HI_OFFSET_OVERRIDE, pwmHiOffset, regIpcDebug);
    }
    else
    {
        regIpcDebug = FLD_SET_DRF(_CPWR_THERM, _IPC_DEBUG, _IPC_OVERRIDE_EN,
                        _OFF, regIpcDebug);
    }
    REG_WR32(CSB, LW_CPWR_THERM_IPC_DEBUG, regIpcDebug);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief  Gets the assertion status for EDPP_VMIN & EDPP_FONLY thermal events.
 *
 * @param[out]   pEDPpVminAsserted Boolean container indication assertion
 *               status of EDPP_VMIN thermal event.
 * @param[out]   pEDPpVminAsserted Boolean container indication assertion
 *               status of EDPP_VMIN thermal event.
 */
static void
s_voltTestAssertedEDPpEventsGet
(
    LwBool *pBEDPpVminAsserted,
    LwBool *pBEDPpFOnlyAsserted
)
{
    LwU32 reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING);

    if (RM_PMU_THERM_IS_BIT_SET(LW_CPWR_THERM_EVENT_EDPP_VMIN, reg32))
    {
        *pBEDPpVminAsserted = LW_TRUE;
    }
    else
    {
        *pBEDPpVminAsserted = LW_FALSE;
    }

    if (RM_PMU_THERM_IS_BIT_SET(LW_CPWR_THERM_EVENT_EDPP_FONLY, reg32))
    {
        *pBEDPpFOnlyAsserted = LW_TRUE;
    }
    else
    {
        *pBEDPpFOnlyAsserted = LW_FALSE;
    }
}

/*!
 * @brief  Sets the specified PWM period for PWM VID specified by @ref idx.
 *
 * @param[in]   idx       PWM VID index.
 * @param[in]   pwmPeriod PWM period to set.
 */
static void
s_voltTestPwmPeriodSet
(
    LwU8  idx,
    LwU16 pwmPeriod
)
{
    LwU32 regPeriod = REG_RD32(CSB, LW_CPWR_THERM_VID0_PWM(idx));

    regPeriod = FLD_SET_DRF_NUM(_CPWR_THERM, _VID0_PWM, _PERIOD, pwmPeriod,
                    regPeriod);
    REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(idx), regPeriod);
}

/*!
 * @brief  Sets the specified PWM dutycycle for PWM VID specified by @ref idx.
 *
 * @param[in]   idx          PWM VID index.
 * @param[in]   pwmDutycycle PWM dutycycle to set.
 */
static void
s_voltTestPwmDutycycleSet
(
    LwU8  idx,
    LwU16 pwmDutycycle
)
{
    LwU32 regDutycycle = REG_RD32(CSB, LW_CPWR_THERM_VID1_PWM(idx));

    regDutycycle = FLD_SET_DRF_NUM(_CPWR_THERM, _VID1_PWM, _HI,
                    pwmDutycycle, regDutycycle);
    REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM(idx), regDutycycle);
}

/*!
 * @brief  Sets the specified IPC Vmin for PWM VID specified by @ref idx.
 *
 * @param[in]   idx     PWM VID index.
 * @param[in]   pwmVmin IPC Vmin given in units of PWM to set.
 */
static void
s_voltTestIpcVminSet
(
    LwU8  idx,
    LwU16 pwmVmin
)
{
    LwU32 regVmin = REG_RD32(CSB, LW_CPWR_THERM_VID2_PWM(idx));

    regVmin = FLD_SET_DRF_NUM(_CPWR_THERM, _VID2_PWM, _IPC_HI_MIN, pwmVmin,
                regVmin);
    REG_WR32(CSB, LW_CPWR_THERM_VID2_PWM(idx), regVmin);
}

/*!
 * @brief  Snaps the value specified by @ref selIdx in @ref pSelIdx.
 *
 * @param[in]   selIdx  Index denoting the value to snap among
                        LW_CPWR_THERM_IPC_DEBUG_SEL_IDX_<xyz>.
 * @param[out]  pSelVal Snapped value specified by @ref selIdx.
 */
static void
s_voltTestIpcDebugValueSnap
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
 * @brief  Enables/Disables IPC feature for PWM VID specified by @ref idx.
 *
 * @param[in]   idx     PWM VID index.
 * @param[in]   bEnable Boolean indicating if we want to enable IPC feature.
 *                      LW_TRUE enables the IPC feature & LW_FALSE disables it.
 */
static void
s_voltTestUseIpcEnable
(
    LwU8   idx,
    LwBool bEnable
)
{
    LwU32 regVid0 = REG_RD32(CSB, LW_CPWR_THERM_VID0_PWM(idx));

    if (bEnable)
    {
        regVid0 = FLD_SET_DRF(_CPWR_THERM, _VID0_PWM, _USE_IPC, _ENABLE, regVid0);
    }
    else
    {
        regVid0 = FLD_SET_DRF(_CPWR_THERM, _VID0_PWM, _USE_IPC, _DISABLE, regVid0);
    }
    REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(idx), regVid0);
}

/*!
 * @brief  Enables/Disables fixed slew rate feature for PWM VID specified by
 *         @ref idx.
 *
 * @param[in]   idx     PWM VID index.
 * @param[in]   bEnable Boolean indicating if we want to enable fixed slew rate
 *                      feature. LW_TRUE enables the fixed slew rate feature
 *                      and LW_FALSE disables it.
 */
static void
s_voltTestConfigureFixedSlewRate
(
    LwU8   idx,
    LwBool bEnable
)
{
    LwU32 regVid0 = REG_RD32(CSB, LW_CPWR_THERM_VID0_PWM(idx));
    LwU32 regVid3 = REG_RD32(CSB, LW_CPWR_THERM_VID3_PWM(idx));

    if (bEnable)
    {
        regVid0 = FLD_SET_DRF(_CPWR_THERM, _VID0_PWM, _USE_FIXED_SLEWRATE,
                    _ENABLE, regVid0);
        regVid3 = FLD_SET_DRF_NUM(_CPWR_THERM, _VID3_PWM, _RISING,
                    VOLT_TEST_VID3_PWM_RISING, regVid3);
        regVid3 = FLD_SET_DRF_NUM(_CPWR_THERM, _VID3_PWM, _FALLING,
                    VOLT_TEST_VID3_PWM_FALLING, regVid3);
        regVid3 = FLD_SET_DRF_NUM(_CPWR_THERM, _VID3_PWM, _REPEAT,
                    VOLT_TEST_VID3_PWM_REPEAT, regVid3);
        regVid3 = FLD_SET_DRF_NUM(_CPWR_THERM, _VID3_PWM, _STEP,
                    VOLT_TEST_VID3_PWM_STEP, regVid3);
    }
    else
    {
        regVid0 = FLD_SET_DRF(_CPWR_THERM, _VID0_PWM, _USE_FIXED_SLEWRATE,
                    _DISABLE, regVid0);
    }
    REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(idx), regVid0);
    REG_WR32(CSB, LW_CPWR_THERM_VID3_PWM(idx), regVid3);
}

/*!
 * @brief  Trigger VID PWM settings for PWM VID specified by @ref idx.
 *
 * @param[in]   idx PWM VID index.
 */
static void
s_voltTestTriggerVidPwmSettings
(
    LwU8  idx
)
{
    LwU32 regTriggerMask = REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK);

    if (FLD_IDX_TEST_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK, _SETTING_NEW,
            idx, _PENDING, regTriggerMask))
    {
        regTriggerMask = FLD_IDX_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                            _SETTING_NEW, idx, _DONE, regTriggerMask);
        REG_WR32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, regTriggerMask);
    }

    regTriggerMask = FLD_IDX_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                        _SETTING_NEW, idx, _TRIGGER, regTriggerMask);
    REG_WR32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, regTriggerMask);

    // Wait until trigger is pending.
    while (FLD_IDX_TEST_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK, _SETTING_NEW,
            idx, _PENDING, REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK)))
    {
        // NOP
    }
}

/*!
 * @brief  Clear and disable interrupts for EDPp events.
 *
 * @param[in]   regIntrEnable Value of register in LW_CPWR_THERM_EVENT_INTR_ENABLE.
 */
static void
s_voltTestClearDisableInterruptsSlowdownForEDPpEvents
(
    LwU32 regIntrEnable,
    LwU32 regUseA
)
{
    LwU8  idx;

    s_voltTestClearEDPpInterrupts();

    // Clear and disable interrupts for EDPp events.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_EDPP)
    {
        regIntrEnable  = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ENABLE,
                            _CTRL, idx, _NO, regIntrEnable);
        regUseA        = FLD_IDX_SET_DRF(_CPWR_THERM, _USE_A, _CTRL, idx, _NO,
                            regUseA);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, regIntrEnable);
    REG_WR32(CSB, LW_CPWR_THERM_USE_A, regUseA);
}

/*!
 * @brief  Clear interrupts for EDPp events.
 */
static void
s_voltTestClearEDPpInterrupts()
{
    LwU8  idx;
    LwU32 regIntrPending = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING);

    // Clear and disable interrupts for EDPp events.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_EDPP)
    {
        regIntrPending = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_PENDING,
                            _STATE, idx, _CLEAR, regIntrPending);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING, regIntrPending);
}

/*!
 * @brief  Configure EDPp interrupts.
 */
static void
s_voltTestConfigureEDPpInterrupts
(
    LwU32 regIntrType,
    LwU32 regIntrOnTriggered,
    LwU32 regIntrOnNormal
)
{
    LwU8  idx;

    // Clear and disable interrupts for EDPp events.
    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_EDPP)
    {
        regIntrType        = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_TYPE,
                                _CTRL, idx, _LEVEL, regIntrType);
        regIntrOnTriggered = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ON_TRIGGERED,
                                _CTRL, idx, _ENABLED, regIntrOnTriggered);
        regIntrOnNormal    = FLD_IDX_SET_DRF(_CPWR_THERM, _EVENT_INTR_ON_NORMAL,
                                _CTRL, idx, _DISABLED, regIntrOnNormal);

    }
    FOR_EACH_INDEX_IN_MASK_END;

    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_TYPE, regIntrType);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED, regIntrOnTriggered);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_NORMAL, regIntrOnNormal);
}

/*!
 * @brief  Save/restore the init values of the registers in the reg cache.
 *
 * @param[in]   bInit     Bool to save/restore regcache.
 *
 * @return      FLCN_OK   regCache save/restore successful.
 */
static FLCN_STATUS
s_voltTestRegCacheInitRestore
(
    LwBool bInit
)
{
    if (pVoltRegCache == NULL)
    {
        memset(&voltRegCache, 0, sizeof(VOLT_TEST_REG_CACHE));
        pVoltRegCache = &voltRegCache;
    }

    if (bInit)
    {
        // Initialise the register cache.
        s_voltTestRegCacheInit();
    }
    else
    {
        // Restore the register cache.
        s_voltTestRegCacheRestore();
    }

    return FLCN_OK;
}

/*!
 * @brief  Save the init values of the registers in the reg cache.
 */
static void
s_voltTestRegCacheInit()
{
    LwU8  idx;

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        pVoltRegCache->regVid0[idx] = REG_RD32(CSB, LW_CPWR_THERM_VID0_PWM(idx));
        pVoltRegCache->regVid1[idx] = REG_RD32(CSB, LW_CPWR_THERM_VID1_PWM(idx));
        pVoltRegCache->regVid2[idx] = REG_RD32(CSB, LW_CPWR_THERM_VID2_PWM(idx));
        pVoltRegCache->regVid3[idx] = REG_RD32(CSB, LW_CPWR_THERM_VID3_PWM(idx));
        s_voltTestConfigureFixedSlewRate(idx, LW_FALSE);
    }

    pVoltRegCache->regIpcDebug    = REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG);
    pVoltRegCache->regIntrEnable  = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
    pVoltRegCache->regUseA        = REG_RD32(CSB, LW_CPWR_THERM_USE_A);

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        pVoltRegCache->regMonCtrl[idx] = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx));
    }

    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_EDPP)
    {
        pVoltRegCache->regEDPpEvents[idx - 29] = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(idx));
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pVoltRegCache->regIntrType        = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_TYPE);
    pVoltRegCache->regIntrOnTriggered = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED);
    pVoltRegCache->regIntrOnNormal    = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_NORMAL);
}

/*!
 * @brief  Restore the registers from the reg cache.
 */
static void
s_voltTestRegCacheRestore()
{
    LwU8 idx;

    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_TYPE, pVoltRegCache->regIntrType);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED, pVoltRegCache->regIntrOnTriggered);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_NORMAL, pVoltRegCache->regIntrOnNormal);

    FOR_EACH_INDEX_IN_MASK(32, idx, LW_CPWR_THERM_EVENT_MASK_EDPP)
    {
        REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS(idx), pVoltRegCache->regEDPpEvents[idx - 29]);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    for (idx = 0; idx < LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL(idx), pVoltRegCache->regMonCtrl[idx]);
    }

    REG_WR32(CSB, LW_CPWR_THERM_USE_A, pVoltRegCache->regUseA);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE, pVoltRegCache->regIntrEnable);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_DEBUG, pVoltRegCache->regIpcDebug);

    for (idx = 0; idx < LW_CPWR_THERM_VID0_PWM__SIZE_1; ++idx)
    {
        REG_WR32(CSB, LW_CPWR_THERM_VID3_PWM(idx), pVoltRegCache->regVid3[idx]);
        REG_WR32(CSB, LW_CPWR_THERM_VID2_PWM(idx), pVoltRegCache->regVid2[idx]);
        REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM(idx), pVoltRegCache->regVid1[idx]);
        REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(idx), pVoltRegCache->regVid0[idx]);

        // In the end trigger to restore original PWM settings.
        s_voltTestTriggerVidPwmSettings(idx);
    }
}

/*!
 * @brief  Enables/Disables EDPP slowdown.
 *
 * @param[in]   slowdownFactor  Slowdown factor.
 * @param[in]   bEnable         Boolean indicating if we want to configure slowdown
 *                              feature. LW_TRUE configures slowdown.
 *                              and LW_FALSE clears it.
 */
static void
s_voltTestConfigureEDPpSlowdown
(
    LwU32  slowdownFactor,
    LwBool bEnable
)
{
    LwU32 regEdppEvt = REG_RD32(CSB, LW_CPWR_THERM_EVT_EDPP_FONLY);

    if (bEnable)
    {
        regEdppEvt = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_EDPP_FONLY, _SLOW_FACTOR,
                    slowdownFactor, regEdppEvt);
    }
    else
    {
        regEdppEvt = FLD_SET_DRF(_CPWR_THERM, _EVT_EDPP_FONLY, _SLOW_FACTOR,
                    _INIT, regEdppEvt);
    }
    REG_WR32(CSB, LW_CPWR_THERM_EVT_EDPP_FONLY, regEdppEvt);
}

