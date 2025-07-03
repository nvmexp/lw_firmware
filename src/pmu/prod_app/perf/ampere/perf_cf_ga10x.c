/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objvbios.h"
#include "vbios/vbios_image.h"
#include "perf/vbios/pstates_vbios_6x.h"
#include "shlib/syscall.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_perf_private.h"
#include "dev_fbpa.h"
#include "dev_fbfalcon_pri.h"
#include "dev_fuse.h"
#include "pmu_objfuse.h"
#include "pmu_objfb.h"
#include "pmu_objlpwr.h"
#include "pmu_bar0.h"
#include "perf/cf/perf_cf.h"
#include "pmu_fbflcn_if.h"
#include "lib/lib_sandbag.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static LwU32 s_perfCfControllerMemTuneMonitorMclkFreqkHzGet(CLK_CNTR_AVG_FREQ_START *pClkCntrSample)
        GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerMemTuneMonitorMclkFreqkHzGet")
        GCC_ATTRIB_USED();

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * Index for memory clock used by MEM TUNE controller
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MCLK_FREQ_IDX                    2U
/*!
 * Wait for PMU_MCLK_WAIT_FOR_FBFLCN_US to allow enough time for
 * in-progress FNFLCN R-M-W on the shared registers to complete.
 *
 * @ref PMU_MCLK_WAIT_FOR_FBFLCN_US
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_WAIT_FOR_FBFLCN_US          185000U

/*!
 * @brief Memory Tuning Controller interface to take the necessary
 *        actions on PMU exception events.
 *
 * @return Explicitly marked void as there is no action PMU could take
 *         except trusting that the register read / write will never fail.
 */
void
perfCfMemTuneControllerPmuExceptionHandler_GA10X(void)
{
    LwU32 regVal;

    LwU32 val = fuseRead(LW_FUSE_SPARE_BIT_1);

    // Validate the memory tuning controller high secure fuse.
    if (val == LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        // Init default
        LwU32   head = 0U;
        LwU32   tail = 0U;

        // 1. Notify FBFLCN about PMU going down.
        head = FLD_SET_DRF_NUM(_PMU_FBFLCN, _HEAD, _SEQ, 1U, head);
        head = FLD_SET_DRF_NUM(_PMU_FBFLCN, _HEAD, _CMD, PMU_FBFLCN_CMD_ID_PMU_HALT_NOTIFY, head);
        head = FLD_SET_DRF(_PMU_FBFLCN, _HEAD, _CYA, _YES, head);
        head = FLD_SET_DRF_NUM(_PMU_FBFLCN, _HEAD, _DATA16, 1U, head);
        tail = FLD_SET_DRF_NUM(_PMU_FBFLCN, _TAIL, _DATA32, 0U, tail);

        REG_WR32(BAR0, LW_PFBFALCON_CMD_QUEUE_TAIL(FBFLCN_CMD_QUEUE_IDX_PMU_HALT_NOTIFY), tail);
        // HEAD must be updated last since WR triggers an interrupt to the FBFLCN.
        REG_WR32(BAR0, LW_PFBFALCON_CMD_QUEUE_HEAD(FBFLCN_CMD_QUEUE_IDX_PMU_HALT_NOTIFY), head);

        // 2. PMU to wait for 185 msec to allow any in-flight command to FBFLCN to complete.
        OS_PTIMER_SPIN_WAIT_US(PERF_CF_CONTROLLER_MEM_TUNE_WAIT_FOR_FBFLCN_US);

        //
        // Program the memory tuning settings to throttle the performance as per
        // mining POR. Because the memory tuning settings ownership is shared between
        // PMU and FBFLCN, ensure the programmed value are intact while accounting for
        // any in-progress memory clock request.
        //
        do
        {
            //
            // 3.a. Program the tFAW to POR agreed value to throttle the
            // memory performance.
            //
            regVal = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
            regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG3, _FAW, LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL, regVal);
            REG_WR32(BAR0, LW_PFB_FBPA_CONFIG3, regVal);

            // 3.b. PMU to wait for 185 msec to allow any in-flight command to FBFLCN to complete.
            OS_PTIMER_SPIN_WAIT_US(PERF_CF_CONTROLLER_MEM_TUNE_WAIT_FOR_FBFLCN_US);

            // 3.c. Read back tFAW to confirm the programed value persist.
            regVal  = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);

        } while (DRF_VAL(_PFB, _FBPA_CONFIG3, _FAW, regVal) != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL);
    }

    return;
}

/*!
 * @brief  Memory Tuning Controller interface to validate the controller
 *         timestamps in ISR context, used for external and timer ISRs.
 *
 *         We need a call to this HAL in external ISR handling code (and not
 *         just for timer ISRs) since on RISCV external ISRs are higher
 *         priority than timer interrupts right now, so an external
 *         interrupt storm could completely block timer interrupts.
 *         This is a quirk of how RISCV with the current SafeRTOS8.4 port
 *         handles interrupts today, and might change in the future!
 *
 *         We need a call to this HAL in the timer ISR handling code (and not
 *         just for external ISRs) since the perfCf task can get stuck in
 *         a software-induced deadlock from e.g. queue operations backpressure
 *         (note this doesn't apply to e.g. infinite loops in critical
 *         sections, we can't do anything about these); and we have no
 *         guarantees that external interrupts would ever fire in that case.
 *
 * @return Explicitly marked void since the right course of action is to
 *         crash immediately if the controller timer checks fail.
 */
void
perfCfMemTuneWatchdogTimerValidate_GA10X(void)
{
    LwU32 regVal = fuseRead(LW_FUSE_SPARE_BIT_1);

    if (FLD_TEST_DRF(_FUSE, _SPARE_BIT_1, _DATA, _ENABLE, regVal))
    {
        FLCN_TIMESTAMP lwrrTimestamp;

        osPTimerTimeNsLwrrentGet(&lwrrTimestamp);

        if (memTuneControllerTimeStamp.data == 0U)
        {
            //
            // Mem tune controller didn't get the chance to run yet.
            // Set initial timer value for it.
            //
            osPTimerTimeNsLwrrentGet(&memTuneControllerTimeStamp);
        }
        else if ((lwrrTimestamp.data - memTuneControllerTimeStamp.data) >
                 (PERF_CF_CONTROLLER_MEM_TUNE_ISR_WATCHDOG_TIMER_THRESHOLD_US() * 1000UL))
        {
            dbgPrintf(LEVEL_ALWAYS, "Timed out waiting on memtune controller!\n");
            PMU_HALT();
        }

        if (memTuneControllerMonitorTimeStamp.data == 0U)
        {
            //
            // Mem tune controller monitor didn't get the chance to run yet.
            // Set initial timer value for it.
            //
            osPTimerTimeNsLwrrentGet(&memTuneControllerMonitorTimeStamp);
        }
        else if ((lwrrTimestamp.data - memTuneControllerMonitorTimeStamp.data) >
                 (PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_ISR_WATCHDOG_TIMER_THRESHOLD_US() * 1000UL))
        {
            dbgPrintf(LEVEL_ALWAYS, "Timed out waiting on memtune controller monitor!\n");
            PMU_HALT();
        }
    }
}

/*!
 * @brief Memory Tuning Controller interface to check whether static
 *        always on override is enabled.
 *
 * @return LW_FALSE If SKU has mining fuse blown and belongs to allow list.
 * @return LW_FALSE If SKU has mining fuse NOT blown.
 * @return LW_TRUE  If SKU has mining fuse blown and does not belongs to allow list.
 */
LwBool
perfCfMemTuneControllerStaticAlwaysOnOverrideEnabled_GA10X(void)
{
    LwU32 val = fuseRead(LW_FUSE_SPARE_BIT_1);

    // Validate the memory tuning controller high secure fuse.
    if (val == LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        return sandbagIsRequested();
    }

    // For SKUs that does not have mining throttle enabled, disable always ON throttle.
    return LW_FALSE;
}

/*!
 * @brief Perf CF Policy patch.
 *
 * @return FLCN_OK  Patch successfully applied.
 * @return other    Other errors coming from function calls.
 */
FLCN_STATUS
perfCfPolicysPatch_GA10X(void)
{
    LwU32       val    = fuseRead(LW_FUSE_SPARE_BIT_1);
    FLCN_STATUS status = FLCN_OK;

    //
    // We are using the fuse spare bit to indicate whether to
    // throttle the performance for mining applications. This
    // is ORed with other mechanism to enable the throttling
    // so ONLY enable if requested.
    // DO NOT DISABLE OTHERWISE IT WILL OVERRIDE THE POR SETTINGS.
    //
    if (val == LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        status = perfCfPolicysActivate(PERF_CF_GET_POLICYS(),
                    LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_MEM_TUNE,
                    LW_TRUE);
    }

    return status;
}

/*!
 * @brief Perf CF Policy sanity check.
 *
 * @return FLCN_OK  Successfully passed sanity checks.
 * @return other    Sanity check failed.
 */
FLCN_STATUS
perfCfPolicysSanityCheck_GA10X(void)
{
    PERF_CF_POLICYS           *pPolicys         = PERF_CF_GET_POLICYS();
    PERF_CF_POLICY            *pPolicy          = NULL;
    PERF_CF_POLICY_CTRL_MASK  *pPolicyCtrlMask  = NULL;
    BOARDOBJGRPMASK_E32        mask;
    LwU32                      val              = fuseRead(LW_FUSE_SPARE_BIT_1);
    FLCN_STATUS                status           = FLCN_ERR_ILWALID_STATE;

    //
    // Sanity check are required ONLY if mining throttling
    // is enabled via fuse bit.
    //
    if (val != LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        status = FLCN_OK;
        goto perfCfPolicysSanityCheck_GA10X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicys != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    pPolicy = BOARDOBJGRP_OBJ_GET(PERF_CF_POLICY, 6);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicy != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicy->super.type == LW2080_CTRL_PERF_PERF_CF_POLICY_TYPE_CTRL_MASK) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicy->label == LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_MEM_TUNE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicy->priority == LW_U8_MAX) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicy->bActivate == LW_TRUE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    // Init to zero.
    boardObjGrpMaskInit_E32(&mask);

    // Ilwert to build compatible mask.
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskIlw(&mask),
        perfCfPolicysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskIsEqual(&pPolicy->compatibleMask, &mask)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    pPolicyCtrlMask = (PERF_CF_POLICY_CTRL_MASK *)pPolicy;
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPolicyCtrlMask != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPolicyCtrlMask->maskControllers, 8)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPolicysSanityCheck_GA10X_exit);

    // If all validation passed, update the status.
    status = FLCN_OK;

perfCfPolicysSanityCheck_GA10X_exit:
    return status;
}

/*!
 * @brief Perf CF Controller sanity check.
 *
 * @return FLCN_OK  Successfully passed sanity checks.
 * @return other    Sanity check failed.
 */
FLCN_STATUS
perfCfControllersSanityCheck_GA10X(void)
{
    PERF_CF_CONTROLLERS          *pControllers       = PERF_CF_GET_CONTROLLERS();
    PERF_CF_CONTROLLER           *pController        = NULL;
    PERF_CF_CONTROLLER_MEM_TUNE  *pControllerMemTune = NULL;
    LwU32                         val         = fuseRead(LW_FUSE_SPARE_BIT_1);
    FLCN_STATUS                   status      = FLCN_ERR_ILWALID_STATE;

    //
    // Sanity check are required ONLY if mining throttling
    // is enabled via fuse bit.
    //
    if (val != LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        status = FLCN_OK;
        goto perfCfControllersSanityCheck_GA10X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllers != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllers->baseSamplingPeriodus == 200000) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    pController = BOARDOBJGRP_OBJ_GET(PERF_CF_CONTROLLER, 8);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController->super.type == LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController->samplingMultiplier == 5) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController->topologyIdx == 10) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    pControllerMemTune = (PERF_CF_CONTROLLER_MEM_TUNE *)pController;
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->bFilterEnabled == LW_TRUE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->bLhrEnhancementEnabled == perfCfControllerMemTuneIsLhrEnhancementFuseEnabled()) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->pmSensorIdx == 0) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->timerTopologyIdx == 19) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->hysteresisCountInc == 5) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->hysteresisCountDec == 15) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.numTargets == 4) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[0].numSignals == 2) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[0].signal[0].index == 208) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[0].signal[1].index == 209) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[1].numSignals == 2) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[1].signal[0].index == 210) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[1].signal[1].index == 211) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[2].numSignals == 2) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[2].signal[0].index == 212) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[2].signal[1].index == 213) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].numSignals == 4) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].signal[0].index == 210) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].signal[1].index == 211) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].signal[2].index == 212) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].signal[3].index == 213) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[0].target.high == 1802) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[0].target.low == 901) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[1].target.high == 0) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[1].target.low == 0) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[2].target.high == 4096) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[2].target.low == 4096) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].target.high == 983) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune->targets.target[3].target.low == 573) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllersSanityCheck_GA10X_exit);


    if (perfCfControllerMemTuneIsLhrEnhancementFuseEnabled())
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            ((pControllerMemTune->porMaxMclkkHz52_12 == sysPerfMemTuneControllerGetVbiosMaxMclkkhz()) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
            perfCfControllersSanityCheck_GA10X_exit);
    }

    // If all validation passed, update the status.
    status = FLCN_OK;

perfCfControllersSanityCheck_GA10X_exit:
    return status;
}


/*!
 * @brief Perf CF Topology sanity check.
 *
 * @return FLCN_OK  Successfully passed sanity checks.
 * @return other    Sanity check failed.
 */
FLCN_STATUS
perfCfTopologysSanityCheck_GA10X(void)
{
    PERF_CF_TOPOLOGYS               *pTopologys          = PERF_CF_GET_TOPOLOGYS();
    PERF_CF_TOPOLOGY                *pTopology           = NULL;
    PERF_CF_TOPOLOGY_SENSED         *pTopologySensed     = NULL;
    PERF_CF_TOPOLOGY_SENSED_BASE    *pTopologySensedBase = NULL;
    LwU32            val      = fuseRead(LW_FUSE_SPARE_BIT_1);
    FLCN_STATUS      status   = FLCN_ERR_ILWALID_STATE;

    //
    // Sanity check are required ONLY if mining throttling
    // is enabled via fuse bit.
    //
    if (val != LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        status = FLCN_OK;
        goto perfCfTopologysSanityCheck_GA10X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopologys != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    // Validate the PTIMER topology
    pTopology = BOARDOBJGRP_OBJ_GET(PERF_CF_TOPOLOGY, 19);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology->super.type == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology->gpumonTag == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_NONE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology->bNotAvailable == LW_FALSE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    pTopologySensed = (PERF_CF_TOPOLOGY_SENSED *)pTopology;
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopologySensed != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopologySensed->sensorIdx == 11) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    // Validate the MCLK topology
    pTopology = BOARDOBJGRP_OBJ_GET(PERF_CF_TOPOLOGY, 10);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology->super.type == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology->gpumonTag == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_NONE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopology->bNotAvailable == LW_FALSE) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    pTopologySensedBase = (PERF_CF_TOPOLOGY_SENSED_BASE *)pTopology;
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopologySensedBase != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopologySensedBase->sensorIdx == 13) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTopologySensedBase->baseSensorIdx == 11) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfTopologysSanityCheck_GA10X_exit);

    // If all validation passed, update the status.
    status = FLCN_OK;

perfCfTopologysSanityCheck_GA10X_exit:
    return status;
}

/*!
 * @brief Perf CF PM sensors sanity check.
 *
 * @return FLCN_OK  Successfully passed sanity checks.
 * @return other    Sanity check failed.
 */
FLCN_STATUS
perfCfPmSensorsSanityCheck_GA10X(void)
{
    PERF_CF_PM_SENSORS *pPmSensors  = PERF_CF_PM_GET_SENSORS();
    PERF_CF_PM_SENSOR  *pPmSensor   = NULL;
    LwU32               val         = fuseRead(LW_FUSE_SPARE_BIT_1);
    FLCN_STATUS         status      = FLCN_ERR_ILWALID_STATE;

    //
    // Sanity check are required ONLY if mining throttling
    // is enabled via fuse bit.
    //
    if (val != LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        status = FLCN_OK;
        goto perfCfPmSensorsSanityCheck_GA10X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPmSensors != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    pPmSensor = BOARDOBJGRP_OBJ_GET(PERF_CF_PM_SENSOR, 0);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPmSensor != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPmSensor->super.type == LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_V10) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPmSensor->signalsSupportedMask, 208)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPmSensor->signalsSupportedMask, 209)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPmSensor->signalsSupportedMask, 210)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPmSensor->signalsSupportedMask, 211)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPmSensor->signalsSupportedMask, 212)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitGet(&pPmSensor->signalsSupportedMask, 213)) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfPmSensorsSanityCheck_GA10X_exit);

    // If all validation passed, update the status.
    status = FLCN_OK;

perfCfPmSensorsSanityCheck_GA10X_exit:
    return status;
}

/*!
 * @brief Public interface to check whether the memory tuning controller
 *        is activated via PMU fuse bit.
 *
 * @return TRUE     Controller is active
 * @return FALSE    Controller is inactive
 */
LwBool
perfCfControllerMemTuneIsActive_GA10X(void)
{
    PERF_CF_POLICYS *pPolicys = PERF_CF_GET_POLICYS();
    LwU32            val      = fuseRead(LW_FUSE_SPARE_BIT_1);
    LwBool           bActive  = LW_FALSE;

    bActive = (val == LW_FUSE_SPARE_BIT_1_DATA_ENABLE);

    // Temporary code to enable sanity check if enabled via VBIOS.
    if (pPolicys != NULL)
    {
        PERF_CF_POLICY *pPolicy = NULL;

        pPolicy = BOARDOBJGRP_OBJ_GET(PERF_CF_POLICY, 6);
        if ((pPolicy != NULL) &&
            (pPolicy->bActivate))
        {
            // ONLY SET TO TRUE. NEVER SET TO FALSE.
            bActive = LW_TRUE;
        }
    }

    return bActive;
}

/*!
 * The WAR is disabled on P8 (405 MHz) and P8 (810 MHz)
 * Given that clock counters could have slight variation in their
 * reading around the target 810 MHz frequency and we know the next
 * higher MCLK is 2000 MHz or above, adding some margin to rule out
 * the frequency noise.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MCLK_FREQ_KHZ_FLOOR     1000000U

/*!
 * @brief Perf CF Memory Tuning Controller Monitor sanity checks to
 *        ensure the controller is operating as expeted.
 *
 * @return FLCN_OK  Successfully passed sanity checks.
 * @return other    Sanity check failed.
 */
FLCN_STATUS
perfCfControllerMemTuneMonitorSanityCheck_GA10X
(
    LwU8                     tFAWLwrr,
    LwU8                     tFAWPrev,
    CLK_CNTR_AVG_FREQ_START *pClkCntrSample
)
{
    LwU16       trrdValHW;
    LwBool      bCtrlEngageHW;
    LwU32       mclkFreqkHz;
    FLCN_STATUS status;

    // Disable MS Feature before issuing first access.
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
    {
        lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
    }

    // We never expect the interface to be called before first Pstate change.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((perfChangeSeqChangeLastPstateIndexGet() != LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        perfCfControllerMemTuneMonitorSanityCheck_exit);

    // Sanity check tFAW value
    if ((((tFAWLwrr < LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ENGAGE_TFAW_VAL) ||
          (tFAWLwrr > LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ENGAGE_TFAW_VAL_MAX)) &&
         (tFAWLwrr != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)) ||
        (((tFAWPrev < LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ENGAGE_TFAW_VAL) ||
          (tFAWPrev > LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ENGAGE_TFAW_VAL_MAX)) &&
         (tFAWPrev != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfCfControllerMemTuneMonitorSanityCheck_exit;
    }

    // Compute average MCLK frequency.
    mclkFreqkHz = s_perfCfControllerMemTuneMonitorMclkFreqkHzGet(pClkCntrSample);

    if (mclkFreqkHz >= PERF_CF_CONTROLLER_MEM_TUNE_MCLK_FREQ_KHZ_FLOOR)
    {
        // Read the HW programmed tRRD value
        trrdValHW = clkFbflcnTrrdGet_HAL(&Clk);

        // Check whether the WAR is engaged by comparing the engage value of tRRD.
        bCtrlEngageHW = ((trrdValHW == tFAWLwrr) || (trrdValHW == tFAWPrev));

        if ((bCtrlEngageHW != (tFAWLwrr != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)) &&
            (bCtrlEngageHW != (tFAWPrev != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)))
        {
            //
            // Upon mis-match check once again to confirm
            // we did not collide with inprogress MCLK switch.
            //
            mclkFreqkHz = s_perfCfControllerMemTuneMonitorMclkFreqkHzGet(pClkCntrSample);

            if (mclkFreqkHz >= PERF_CF_CONTROLLER_MEM_TUNE_MCLK_FREQ_KHZ_FLOOR)
            {
                status = FLCN_ERR_MISMATCHED_TARGET;
                goto perfCfControllerMemTuneMonitorSanityCheck_exit;
            }
        }
        else
        {
            status = FLCN_OK;
            goto perfCfControllerMemTuneMonitorSanityCheck_exit;
        }
    }

    //
    // If MCLK is below the floor or somehow we collided with
    // in-progress MCLK switch, mark it NOT QUERIED which will
    // inform the parent to ignore this case.
    //
    status = FLCN_WARN_NOT_QUERIED;

perfCfControllerMemTuneMonitorSanityCheck_exit:

    // Enable MS Group before exit.
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
    {
        lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
    }

    return status;
}

/*!
 * @brief Perf CF Memory Tuning Controller parses and 
 *        selwrely stores POR Mclk
 *
 * @return FLCN_OK  Successfully passed parsing vbios.
 * @return other    Vbios parsing failed.
 */
FLCN_STATUS
perfCfControllerMemTuneParseVbiosPorMclk_GA10X()
{
    FLCN_STATUS status;
    const PSTATES_VBIOS_6X_HEADER *pHeader;
    const PSTATES_VBIOS_6X_ENTRY_CLOCK *pClkEntry;
    LwU8 pstateIdx;
    LwU32 mclkFreqkHz;
    LwU64 mclkFreqkHz64;
    LwLength entryStride;

    // Get the version and size of the table
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosImageDataGet(
            &Vbios,
            LW_GFW_DIRT_PERFORMANCE_TABLE,
            (const void **)&pHeader,
            0U,
            2U),
        perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);

    //
    // Ensure the version matches expectations and that the size is greater than
    // the minimum expected.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pHeader->version == PSTATES_VBIOS_6X_TABLE_VERSION) &&
        (pHeader->headerSize >= PSTATES_VBIOS_6X_HEADER_SIZE_0A),
        FLCN_ERR_ILWALID_STATE,
        perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);

    // Get the entire header
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosImageDataGet(
            &Vbios,
            LW_GFW_DIRT_PERFORMANCE_TABLE,
            (const void **)&pHeader,
            0U,
            pHeader->headerSize),
        perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);

    // Check the rest of the object sizes against the minimums
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pHeader->baseEntrySize >= PSTATES_VBIOS_6X_BASE_ENTRY_SIZE_2) &&
        (pHeader->clockEntrySize >= PSTATES_VBIOS_6X_ENTRY_CLOCK_SIZE_06),
        FLCN_ERR_ILWALID_STATE,
        perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);

    entryStride = pHeader->baseEntrySize +
        pHeader->clockEntrySize * pHeader->clockEntryCount;
    for (pstateIdx = 0U; pstateIdx < pHeader->baseEntryCount; pstateIdx++)
    {
        const PSTATES_VBIOS_6X_ENTRY *pEntry;
        const LwLength entryOffset = pHeader->headerSize + entryStride * pstateIdx;

        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosImageDataGet(
                &Vbios,
                LW_GFW_DIRT_PERFORMANCE_TABLE,
                (const void **)&pEntry,
                entryOffset,
                pHeader->baseEntrySize),
            perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);

        if (pEntry->pstateLevel == PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P0)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                vbiosImageDataGet(
                    &Vbios,
                    LW_GFW_DIRT_PERFORMANCE_TABLE,
                    (const void **)&pClkEntry,
                    entryOffset + pHeader->baseEntrySize +
                    PERF_CF_CONTROLLER_MEM_TUNE_MCLK_FREQ_IDX *
                    pHeader->clockEntrySize,
                    pHeader->clockEntrySize),
                perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);

            mclkFreqkHz = REF_VAL(
                    PSTATES_VBIOS_6X_CLOCK_PROG_PARAM1_MAX_FREQ_MHZ,
                    pClkEntry->param1) * 1000;

            // POR Max MCLK Freq = POR MCLK Freq + 7% OC Margin
            mclkFreqkHz += ((mclkFreqkHz * 7) / 100);
            mclkFreqkHz64 = mclkFreqkHz;
            // Left shift by 12 to colwert 64.00 -> 52.24
            lw64Lsl(&mclkFreqkHz64, &mclkFreqkHz64, 12);

            PMU_ASSERT_OK_OR_GOTO(status,
                sysPerfMemTuneControllerSetVbiosMaxMclkkhz(mclkFreqkHz64),
                perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit);
        }
    }

perfCfControllerMemTuneParseVbiosPorMclk_GA10X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Wait for 5 us to diff the clock counter readings to callwlate
 * the current MCLK value.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MCLK_CALC_DELAY_US                       5U

/*!
 * @brief Helper interface to callwlate MCLK frequency.
 */
static LwU32
s_perfCfControllerMemTuneMonitorMclkFreqkHzGet
(
    CLK_CNTR_AVG_FREQ_START *pClkCntrSample
)
{
    LwU32 mclkFreqkHz;

    // 1. Get the initial sample reasing.
    mclkFreqkHz = clkCntrAvgFreqKHz(LW2080_CTRL_CLK_DOMAIN_MCLK,
                    LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED, pClkCntrSample);

    // 2. Wait for 5 us
    OS_PTIMER_SPIN_WAIT_US(PERF_CF_CONTROLLER_MEM_TUNE_MCLK_CALC_DELAY_US);

    // 3. Diff the counter to callwlate the MCLK frequency.
    mclkFreqkHz = clkCntrAvgFreqKHz(LW2080_CTRL_CLK_DOMAIN_MCLK,
                    LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED, pClkCntrSample);

    return mclkFreqkHz;
}

