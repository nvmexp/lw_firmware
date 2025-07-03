/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ap_ctrl.c
 * @brief  Adaptive Power controller file
 *
 * AP_CTRL object cache the controller specific characteristics of Adaptive
 * algorithm. It handles controller specific operations.
 *
 * Refer - "Design: Adaptive Power Feature" @pmuifap.h for the details.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objap.h"
#include "pmu_objlpwr.h"
#include "pmu_objgcx.h"
#include "dbgprintf.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
static LwU8  s_apCtrlPredictBestIdleFilterX(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlPredictBestIdleFilterX");
static LwS32 s_apCtrlCalcPowerSavingUs(LwU8, LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlCalcPowerSavingUs");
static LwU8  s_apCtrlCalcResidency(LwU8, LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlCalcResidency");
static void  s_apCtrlScheduleCallback(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlScheduleCallback");
static void  s_apCtrlDeScheduleCallback(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlDeScheduleCallback");
static void  s_apCtrlIdleThresholdMaxSet(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlIdleThresholdMaxSet");
static void  s_apCtrlIdleThresholdRestore(LwU8 ctrlId)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlIdleThresholdRestore");
static void  s_apCtrlIncrementHistCntrSw(LwU8, LwU32)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlIncrementHistCntrSw");
static LwU8  s_apCtrlParentPgCtrlIdGet(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlParentPgCtrlIdGet");
static LwU32 s_apCtrlResidencyOverheadGet(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlResidencyOverheadGet");
static LwU32 s_apCtrlPowerOverheadGet(LwU8)
              GCC_ATTRIB_SECTION("imem_libAp", "s_apCtrlPowerOverheadGet");

/* ------------------------ Macros ----------------------------------------- */

// Macro colwerts BinIdx to HCycles. Hcycles refer to 100 pwrclk cycles.
#define AP_BIN_IDX_TO_HCYCLES(binIdx, bin0Cycles)               \
                                               (((bin0Cycles) << (binIdx))/100)

// Macro colwerts BinIdx to pwrclk cycles
#define AP_BIN_IDX_TO_CYCLES(binIdx, bin0Cycles)     ((bin0Cycles) << (binIdx))

/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief Process init and enable APCTRL command
 *
 * @param[in]   pPgCmd  Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_CTRL_INIT_AND_ENABLE
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_RPC_ILWALID_INPUT  Invalid input
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay.
 * @return      FLCN_ERR_NOT_SUPPORTED      Parent is not supported
 */
FLCN_STATUS
apCtrlInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_CTRL_INIT_AND_ENABLE *pParams
)
{
    LwU32                ctrlId       = pParams->ctrlId;
    AP_CTRL             *pApCtrl      = AP_GET_CTRL(ctrlId);
    RM_PMU_AP_CTRL_STAT *pApCtrlStats = NULL;
    FLCN_STATUS          status       = FLCN_OK;
    LwU8                 idleMaskIndex;

    //
    // Skip the initialization if ApCtrl is already supported. This might
    // be a duplicate call.
    //
    if (AP_IS_CTRL_SUPPORTED(ctrlId))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto apCtrlInit_exit;
    }

    //
    // Allocate space for ApCtrl. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pApCtrl == NULL)
    {
        pApCtrl = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, AP_CTRL);
        if (pApCtrl == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto apCtrlInit_exit;
        }

        Ap.pApCtrl[ctrlId] = pApCtrl;
    }

    //
    // Allocate space for ApCtrl stats. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    pApCtrlStats = pApCtrl->pStat;
    if (pApCtrlStats == NULL)
    {
        pApCtrlStats = lwosCallocResidentType(1, RM_PMU_AP_CTRL_STAT);
        if (pApCtrlStats == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto apCtrlInit_exit;
        }

        pApCtrl->pStat = pApCtrlStats;
    }

    //
    // Allocate space for SW bin WAR. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_AP_WAR_INCREMENT_HIST_CNTR) &&
        pParams->bSwHistEnabled)
    {
        if (pApCtrl->pSwBins == NULL)
        {
            pApCtrl->pSwBins = lwosCallocResidentType(RM_PMU_AP_CFG_HISTOGRAM_BIN_N, LwS16);
            if (pApCtrl->pSwBins == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;

                goto apCtrlInit_exit;
            }
        }
    }

    // Initialize disable reason mask
    pApCtrl->disableReasonMask = AP_CTRL_DISABLE_MASK(RM);
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        pApCtrl->disableReasonMask |= AP_CTRL_DISABLE_MASK(PERF);
    }

    // Initialize AP Ctrl parameters
    pApCtrl->idleThresholdMinUs = pParams->idleThresholdMinUs;
    pApCtrl->idleThresholdMaxUs = pParams->idleThresholdMaxUs;
    pApCtrl->powerBreakEvenUs   = pParams->breakEvenResidentTimeUs +
                                  s_apCtrlPowerOverheadGet(ctrlId);
    pApCtrl->cyclesPerSampleMax = pParams->cyclesPerSampleMax;
    pApCtrl->minResidency       = pParams->minResidency;

    // Temporary. Delete in next CL
    pApCtrl->minTargetSavingUs  = AP_CTRL_MINIMUM_TARGET_SAVING_DEFAULT_US;

    // ApCtrl Specific Initialization
    if ((PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR)) &&
        (ctrlId == RM_PMU_AP_CTRL_ID_GR))
    {
        if (pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_PG))
        {
            LwU32 *pIdleMask =
                GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG)->idleMask;

            pApCtrl->idleMask[0] = pIdleMask[0];
            pApCtrl->idleMask[1] = pIdleMask[1];
            pApCtrl->idleMask[2] = pIdleMask[2];
        }
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;

            goto apCtrlInit_exit;
        }

        pApCtrl->callbackId = LPWR_CALLBACK_ID_AP_GR;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)) &&
             (ctrlId == RM_PMU_AP_CTRL_ID_DI_DEEP_L1))
    {
        // Initialize Idle Mask
        pgApDiIdleMaskInit_HAL(&Pg);

        pApCtrl->callbackId = LPWR_CALLBACK_ID_AP_DI;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG)) &&
             (ctrlId == RM_PMU_AP_CTRL_ID_MSCG))
    {
        if (pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
        {
            LwU32 *pIdleMask =
                GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG)->idleMask;

            pApCtrl->idleMask[0] = pIdleMask[0];
            pApCtrl->idleMask[1] = pIdleMask[1];
            pApCtrl->idleMask[2] = pIdleMask[2];
        }
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;

            goto apCtrlInit_exit;
        }

        pApCtrl->callbackId = LPWR_CALLBACK_ID_AP_MSCG;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;

        goto apCtrlInit_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
    {
        Pg.pCallback->callbackCtrl[pApCtrl->callbackId].baseMultiplier = pParams->baseMultiplier;
    }

    //
    // Config AP Idle counter
    // 1) Set Idle Mask
    // 2) Set supplemental idle counters mode of Histogram to AUTO_IDLE.
    //
    idleMaskIndex = apCtrlGetIdleMaskIndex(ctrlId);
    pgApConfigIdleMask_HAL(&Pg, idleMaskIndex, pApCtrl->idleMask);
    pgApConfigCntrMode_HAL(&Pg, idleMaskIndex, PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);

    // Update support bit for ApCtrl
    Ap.supportedMask |= BIT(ctrlId);

apCtrlInit_exit:

    return status;
}

/*!
 * @brief Enable ApCtrl
 *
 * Enabling ApCtrl includes -
 * 1) Kick ApCtrl
 * 2) Schedule callback
 *
 * @param [IN]  ctrlId      AP Ctrl Id
 * @param [IN]  reasonMask  Enable reason AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_xyz
 *
 * @return      FLCN_OK     On success
 */
FLCN_STATUS
apCtrlEnable
(
    LwU8  ctrlId,
    LwU32 reasonMask
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);

    // Return early if ApCtrl is already enabled
    if (pApCtrl->disableReasonMask == 0)
    {
        goto apCtrlEnable_exit;
    }

    pApCtrl->disableReasonMask &= (~reasonMask);

    if (pApCtrl->disableReasonMask == 0)
    {
        // Update AP Ctrl parameters and restart histogram logging
        apCtrlKick(ctrlId, AP_CTRL_SKIP_COUNT_DEFAULT);

        // Schedule Callback
        s_apCtrlScheduleCallback(ctrlId);
    }

apCtrlEnable_exit:

    return FLCN_OK;
}

/*!
 * @brief Disable ApCtrl
 *
 * Disabling ApCtrl includes -
 * 1) De-schedule callback
 * 2) Stop Histogram
 * 3) Restore idle threshold
 *
 * @param [IN]  ctrlId      AP Ctrl Id
 * @param [IN]  reasonMask  Disable reason AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_xyz
 *
 * @return      FLCN_OK     On success
 */
FLCN_STATUS
apCtrlDisable
(
    LwU8  ctrlId, 
    LwU32 reasonMask
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);

    if (pApCtrl->disableReasonMask == 0)
    {
        // De-schedule callback
        s_apCtrlDeScheduleCallback(ctrlId);

        // Stop Histogram
        apCtrlHistogramStop_HAL(&Ap, ctrlId);

        // Restore threshold
        s_apCtrlIdleThresholdRestore(ctrlId);

        // ApCtrl is no longer active
        pApCtrl->pStat->bActive = LW_FALSE;
    }

    // Update disable reason mask
    pApCtrl->disableReasonMask |= reasonMask;

    return FLCN_OK;
}

/*!
 * @brief Kick off ApCtrl
 *
 * To be called pre/post a disruptive event. An event is be considered
 * disruptive if it:
 *      1. Changes the graphics activity pattern
 *      2. The histogram settings need to be adjusted
 *
 * @param [in]  ctrlId      AP Ctrl Id
 *
 * @return      FLCN_OK     On success
 */
FLCN_STATUS
apCtrlKick
(
    LwU8  ctrlId,
    LwU32 skipCount
)
{
    AP_CTRL *pApCtrl;
    LwU32    pwrClkMHz = AP_GET_PWRCLK_MHZ();
    LwS32    bin0Index;

    pApCtrl = AP_GET_CTRL(ctrlId);

    // Re-callwlate all parameters in AP
    pApCtrl->idleThresholdMinCycles = (pApCtrl->idleThresholdMinUs * pwrClkMHz);
    pApCtrl->idleThresholdMaxCycles = (pApCtrl->idleThresholdMaxUs * pwrClkMHz);
    pApCtrl->powerBreakEvenHCycles  = (pApCtrl->powerBreakEvenUs * pwrClkMHz) / 100;

    // Temporary. Delete in next CL
    pApCtrl->minTargetSavingHCycles = (pApCtrl->minTargetSavingUs * pwrClkMHz) / 100;

    //
    // Skip count shouldn't be less than default value.
    // Refer doc @AP_CTRL_SKIP_COUNT_DEFAULT
    //
    if (skipCount < AP_CTRL_SKIP_COUNT_DEFAULT)
    {
        skipCount = AP_CTRL_SKIP_COUNT_DEFAULT;
    }

    // Initialize APCTRL Statistics
    pApCtrl->pStat->bActive           = LW_FALSE;
    pApCtrl->pStat->idleFilterX       = RM_PMU_AP_CFG_HISTOGRAM_BIN_N;
    pApCtrl->pStat->prevPowerSavingUs = 0;
    pApCtrl->pStat->skipCount         = skipCount;

    // Initialize threshold to max
    s_apCtrlIdleThresholdMaxSet(ctrlId);

    for (bin0Index = RM_PMU_AP_CFG_HISTOGRAM_BIN_N - 1; bin0Index > 0; bin0Index--)
    {
        //
        // Continue if bin0Cycles are less than minimum supported Idle
        // filter.
        //
        // Collecting activity pattern for time periods less than
        // idleThresholdMinCycles is not giving us any useful information
        // AP will never select and set Idle thresholds below
        // idleThresholdMinCycles. Thus, adjust histogram bins in such way
        // that bin0 will collect idle periods greater than or equal to
        // idleThresholdMinCycles.
        //
        // Shift window saturates at 15. Means we can't set Bin0Cycles
        // greater than BIT(15). Thus we start the loop with 15.
        //
        if ((BIT(bin0Index) <= pApCtrl->idleThresholdMinCycles) ||
            (bin0Index == 0))
        {
            break;
        }
    }

    pApCtrl->bin0Cycles = BIT(bin0Index);

    // Set _SHIFTWINDOW of histogram HW and start logging.
    apCtrlHistogramResetAndStart_HAL(&Ap, ctrlId, bin0Index);

    return FLCN_OK;
}

/*!
 * @brief Adaptive Power Controller Exelwtion
 *
 * This function -
 * 1) Samples histogram
 * 2) Predicts the best idle filter setting
 * 3) Update new idle threshold
 *
 * @param [in]  ctrlId             AP Ctrl Id
 *
 * @return      FLCN_STATUS        FLCN_OK if success.
 */
FLCN_STATUS
apCtrlExelwte
(
    LwU8 ctrlId
)
{
    LwU8                 binIdx;
    AP_CTRL             *pApCtrl;
    RM_PMU_AP_CTRL_STAT *pApCtrlStats;
    LwBool               bNextActive = LW_FALSE;

    if (!AP_IS_CTRL_ENABLED(ctrlId))
    {
        return FLCN_ERROR;
    }

    pApCtrl      = AP_GET_CTRL(ctrlId);
    pApCtrlStats = pApCtrl->pStat;

    // Read Histogram
    apCtrlHistogramRead_HAL(&Ap, ctrlId);

    // Skip this iteration if skipCount is set. Reset the threshold.
    if (pApCtrlStats->skipCount > 0)
    {
        pApCtrlStats->skipCount--;

        goto apCtrlExelwte_exit;
    }

    //
    // Return early if histogram bins are empty
    //
    // Next prediction is possible only when histogram have non-zero data.
    // All histogram bins are empty means GPU (or targeted portion of GPU)
    // is completely busy. We should reset the idle threshold.
    //
    for (binIdx = 0; binIdx < RM_PMU_AP_CFG_HISTOGRAM_BIN_N; binIdx++)
    {
        if (pApCtrlStats->bin[binIdx] != 0)
        {
            break;
        }
    }
    if (binIdx == RM_PMU_AP_CFG_HISTOGRAM_BIN_N)
    {
        goto apCtrlExelwte_exit;
    }

    //
    // Callwlate the power saving and residency of the previous idle filter as
    // a metric on the activity pattern consistency. Here we check the power
    // saving and residency of the previous idle filter for current activity
    // pattern. Aim is to evaluate the correctness of last prediction. Last
    // valid prediction is treated as bad decision if we didn't meet the
    // targeted requirements.
    //
    // Previous idle filter is cached in pApCtrlStats->idleFilterX.
    //
    pApCtrlStats->prevPowerSavingUs = s_apCtrlCalcPowerSavingUs(ctrlId, pApCtrlStats->idleFilterX);
    pApCtrlStats->prevResidency     = s_apCtrlCalcResidency(ctrlId, pApCtrlStats->idleFilterX);

    //
    // Predict the optimal filter for iteration based on current idle pattern
    // captured by histogram.
    //
    pApCtrlStats->idleFilterX       = s_apCtrlPredictBestIdleFilterX(ctrlId);

    //
    // Check whether previous prediction met the requirements.
    // Keep AP deactivated if previous prediction failed.
    //
    if ((pApCtrlStats->prevPowerSavingUs <= 0) ||
        (pApCtrlStats->prevResidency < pApCtrl->minResidency))
    {
        //
        // Increment bad decision count as previous prediction failed to
        // meet the targeted requirements
        //
        if (pApCtrlStats->bActive)
        {
            pApCtrlStats->badDecisionCount++;
        }
    }
    //
    // Previous prediction passed. Now check the validity of next prediction.
    // Keep AP active only if next prediction is valid.
    //
    else if (pApCtrlStats->idleFilterX < RM_PMU_AP_CFG_HISTOGRAM_BIN_N)
    {
        pApCtrl->idleThresholdCycles =
            AP_BIN_IDX_TO_CYCLES(pApCtrlStats->idleFilterX, pApCtrl->bin0Cycles);

        // Round up the idle threshold to minimum permittable limit
        if (pApCtrl->idleThresholdCycles < pApCtrl->idleThresholdMinCycles)
        {
            pApCtrl->idleThresholdCycles = pApCtrl->idleThresholdMinCycles;
        }

        bNextActive = LW_TRUE;
    }

apCtrlExelwte_exit:

    //
    // Update threshold if we have valid prediction otherwise reset the
    // threshold
    //
    if (bNextActive)
    {
        // Issue lazy updates for idle threshold.
        apCtrlIdleThresholdSet(ctrlId, LW_FALSE);

        pApCtrlStats->thresholdCounter[pApCtrlStats->idleFilterX]++;
    }
    else
    {
        // AP is not active. Set threshold to maximum value.
        s_apCtrlIdleThresholdMaxSet(ctrlId);

        pApCtrlStats->defaultThresholdCounter++;
    }

    pApCtrlStats->bActive = bNextActive;

    return FLCN_OK;
}

/*!
 * @brief Set idle threshold for ApCtrl
 *
 * Threshold update is disruptive operation. It wakes PgCtrl. Waking PgCtrl
 * have impact on power saving. To optimize the power saving ApCtrl is not
 * queuing the wake-up. Threshold update is delayed till next natural wake-up.
 * We have facility to override this behavior by forcing immediate threshold
 * update feature.
 *
 * @param [in]  ctrlId                      AP Ctrl Id
 * @param [in]  bImmediateThresholdUpdate   Force immediate threshold update
 */
void
apCtrlIdleThresholdSet
(
    LwU8   apCtrlId,
    LwBool bImmediateThresholdUpdate
)
{

    if ((PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR)   &&
         apCtrlId == RM_PMU_AP_CTRL_ID_GR) ||
        (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
         apCtrlId == RM_PMU_AP_CTRL_ID_MSCG))
    {
        LwU8              pgCtrlId;
        OBJPGSTATE       *pPgState;
        PG_CTRL_THRESHOLD thresholdCycles;

        pgCtrlId = s_apCtrlParentPgCtrlIdGet(apCtrlId);

        if (pgIsCtrlSupported(pgCtrlId))
        {
            pPgState = GET_OBJPGSTATE(pgCtrlId);

            //
            // Trigger threshold update if -
            // 1) Immediate threshold update feature is enabled     OR
            // 2) Parent PgCtrl is in FULL Power mode
            //
            // In other word - in absence of "immediate threshold update",
            // update can be triggered when PgCtrl is in Full Power mode.
            //
            if (bImmediateThresholdUpdate ||
                PG_IS_FULL_POWER(pgCtrlId))
            {
                // Update the threshold and clear the pending action bit.
                thresholdCycles.idle = AP_CTRL_PREDICTED_THRESHOLD_CYCLES_GET(apCtrlId);
                thresholdCycles.ppu  = 0;

                pgCtrlThresholdsUpdate(pgCtrlId, &thresholdCycles);

                PG_CLEAR_PENDING_STATE(pPgState, AP_THRESHOLD_UPDATE);
            }
            else
            {
                //
                // PgCtrl is not in Full Power mode. Set pending action bit to
                // delay the threshold update.
                //
                PG_SET_PENDING_STATE(pPgState, AP_THRESHOLD_UPDATE);
            }
        }
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1) &&
             apCtrlId == RM_PMU_AP_CTRL_ID_DI_DEEP_L1)
    {
        LwU32 deepL1ThresholdUs =
                AP_CTRL_PREDICTED_THRESHOLD_CYCLES_GET(RM_PMU_AP_CTRL_ID_DI_DEEP_L1) / AP_GET_PWRCLK_MHZ();

        pgApDiIdleThresholdUsSet_HAL(&Pg , deepL1ThresholdUs);
    }
}

/*!
 * @brief Handle power source event  (AC vs DC) for ApCtrl
 *
 * This API updates maximum allowable cycles per sample based on power event.
 * It triggers the kick. Kick will discard last data and restart the histogram.
 *
 * @param [in]  ctrlId             AP Ctrl Id
 * @param [in]  bPwrSrcDc          Set when GPU running on battery
 *
 * @return FLCN_OK      On success
 */
FLCN_STATUS
apCtrlEventHandlerPwrSrc
(
    LwU8   ctrlId,
    LwBool bPwrSrcDc
)
{
    AP_CTRL *pApCtrl;
    LwU32    newEntryIndex = 0;
    LwU32    lwrEntryIndex = 0;

    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X)) &&
        ((ctrlId == RM_PMU_AP_CTRL_ID_GR) ||
         (ctrlId == RM_PMU_AP_CTRL_ID_MSCG)))
    {
        pApCtrl = AP_GET_CTRL(ctrlId);

        // Find new AP table try based on power source state
        if (ctrlId == RM_PMU_AP_CTRL_ID_GR)
        {
            newEntryIndex = bPwrSrcDc ? Lpwr.pVbiosData->gr.adaptiveGrIndexDc :
                                        Lpwr.pVbiosData->gr.adaptiveGrIndexAc;

            lwrEntryIndex = Lpwr.pVbiosData->gr.adaptiveGrIndexLwrrent;
            Lpwr.pVbiosData->gr.adaptiveGrIndexLwrrent = newEntryIndex;
        }
        else if (ctrlId == RM_PMU_AP_CTRL_ID_MSCG)
        {
            newEntryIndex = bPwrSrcDc ? Lpwr.pVbiosData->ms.adaptiveMscgIndexDc :
                                        Lpwr.pVbiosData->ms.adaptiveMscgIndexAc;

            lwrEntryIndex = Lpwr.pVbiosData->ms.adaptiveMscgIndexLwrrent;
            Lpwr.pVbiosData->ms.adaptiveMscgIndexLwrrent = newEntryIndex;
        }

        // Avoid duplicate updates
        if (newEntryIndex != lwrEntryIndex)
        {
            pApCtrl->idleThresholdMinUs = Lpwr.pVbiosData->ap.entry[newEntryIndex].idleThresholdMinUs;
            pApCtrl->idleThresholdMaxUs = Lpwr.pVbiosData->ap.entry[newEntryIndex].idleThresholdMaxUs;
            pApCtrl->powerBreakEvenUs   = Lpwr.pVbiosData->ap.entry[newEntryIndex].breakEvenResidentTimeUs +
                                          s_apCtrlPowerOverheadGet(ctrlId);
            pApCtrl->cyclesPerSampleMax = Lpwr.pVbiosData->ap.entry[newEntryIndex].cyclesPerSampleMax;
            pApCtrl->minResidency       = Lpwr.pVbiosData->ap.entry[newEntryIndex].minResidency;

            apCtrlKick(ctrlId, AP_CTRL_SKIP_COUNT_DEFAULT);
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Increment Histogram Counter
 *
 * It increments HW counters to accommodate time for which PWRCLK was gated.
 * There are two ways to increment idle counter -
 * 1) SW WAR that simulates behavior of incrementing counter
 * 2) HW support to directly increment counter
 *
 * PREREQUISITES:
 * This function should be called when Counter mode is set to FORCE_IDLE.
 *
 * @param [in]  ctrlId             AP Ctrl Id
 * @param [in]  incrementUs        HW counter Increment in us
 */
void
apCtrlIncrementHistCntr
(
    LwU8  ctrlId,
    LwU32 incrementUs
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);
    LwU32    incrementPwrClkCycles;

    // Return early if ApCtrl not supported OR increment is Zero
    if ((!AP_IS_CTRL_SUPPORTED(ctrlId)) ||
        (incrementUs <= 0))
    {
        return;
    }

    incrementPwrClkCycles = incrementUs * AP_GET_PWRCLK_MHZ();

    if ((PMUCFG_FEATURE_ENABLED(PMU_AP_WAR_INCREMENT_HIST_CNTR)) &&
        (pApCtrl->pSwBins != NULL))
    {
        s_apCtrlIncrementHistCntrSw(ctrlId, incrementPwrClkCycles);
    }
    else
    {
        apCtrlIncrementHistCntrHw_HAL(&Ap, ctrlId, incrementPwrClkCycles);
    }
}

/*!
 * @brief Merge SW Histogram Bins to HW histograms
 *
 * This function merges SW Histogram Bins to HW histograms and generates final
 * Histogram buckets. At end function resets SW Bins.
 *
 * @param [in]  ctrlId      AP Ctrl Id
 */
void
apCtrlMergeSwHistBin
(
    LwU8 ctrlId
)
{
    AP_CTRL *pApCtrl;
    LwU32    binIdx;
    LwS32    bilwal;

    pApCtrl = AP_GET_CTRL(ctrlId);

    // Return early if SW Histogram Bins are not supported
    if ((!AP_IS_CTRL_SUPPORTED(ctrlId)) ||
        (pApCtrl->pSwBins == NULL))
    {
        return;
    }

    for (binIdx = 0; binIdx < RM_PMU_AP_CFG_HISTOGRAM_BIN_N; binIdx++)
    {
        bilwal = pApCtrl->pStat->bin[binIdx] + pApCtrl->pSwBins[binIdx];

        //
        // Histogram Bin value must be between 0 and
        // AP_MAX_VALUE_STORED_BY_HISTOGRAM_BIN
        //
        if ((bilwal >= 0) &&
            (bilwal <= AP_MAX_VALUE_STORED_BY_HISTOGRAM_BIN))
        {
            pApCtrl->pStat->bin[binIdx] = bilwal;
        }

        // Reset SW Bins
        pApCtrl->pSwBins[binIdx] = 0;
    }
}

/*!
 * @brief Get Histogram Index
 *
 * @param [IN]  ctrlId      AP Ctrl Id
 *
 * @return      LwU8        Histogram Index
 */
LwU8
apCtrlGetHistogramIndex
(
    LwU8 ctrlId
)
{
    LwU8 histogramIndex = RM_PMU_AP_HISTOGRAM_IDX_GR;

    switch (ctrlId)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR)
        case RM_PMU_AP_CTRL_ID_GR:
        {
             break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR)

#if PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)
        case RM_PMU_AP_CTRL_ID_DI_DEEP_L1:
        {
            histogramIndex = RM_PMU_AP_HISTOGRAM_IDX_DI_DEEP_L1;
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)

#if PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG)
        case RM_PMU_AP_CTRL_ID_MSCG:
        {
            histogramIndex = RM_PMU_AP_HISTOGRAM_IDX_MSCG;
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG)

        default:
        {
            // Invalid AP Ctrl Id
            PMU_HALT();
        }
    }

    return histogramIndex;
}

/*!
 * @brief Get Index of Idle Supplementary Mask
 *
 * @param [in]  ctrlId      AP Ctrl Id
 *
 * @return      LwU8        Index of Idle Supplementary Mask
 */
LwU8
apCtrlGetIdleMaskIndex
(
    LwU8 ctrlId
)
{
    LwU8 idleMaskIndex = RM_PMU_AP_IDLE_MASK_GR;

    switch (ctrlId)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR)
        case RM_PMU_AP_CTRL_ID_GR:
        {
             break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR)

#if PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)
        case RM_PMU_AP_CTRL_ID_DI_DEEP_L1:
        {
            idleMaskIndex = RM_PMU_AP_IDLE_MASK_DI_DEEP_L1;
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)

#if PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG)
        case RM_PMU_AP_CTRL_ID_MSCG:
        {
            idleMaskIndex = RM_PMU_AP_IDLE_MASK_MSCG;
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG)

        default:
        {
            // Invalid AP Ctrl Id
            PMU_HALT();
        }
    }

    return idleMaskIndex;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Get Parent controller id
 *
 * @param [in]  ctrlId      AP Ctrl Id
 *
 * @return      Parent controller ID
 */
static LwU8
s_apCtrlParentPgCtrlIdGet
(
    LwU8 ctrlId
)
{
    LwU8 pgCtrlId = RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE;

    if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR) &&
        ctrlId == RM_PMU_AP_CTRL_ID_GR)
    {
        pgCtrlId = RM_PMU_LPWR_CTRL_ID_GR_PG;
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
        ctrlId == RM_PMU_AP_CTRL_ID_MSCG)
    {
        pgCtrlId = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
    }

    return pgCtrlId;
}

/*!
 * @brief Predict best idle filter index
 *
 * Predict the idle filter index with -
 * 1) Maximum allowed idle filter
 * 2) Power features cycles less than "max allowable cycles"
 * 3) Best possible net power savings
 * 4) Residency is greater than minimum targeted residency
 * 5) Minimum allowed idle filter
 *
 * Following formula gives us Net Power Saving:
 * Net power saving = "idle period" - "idle filter" - "break even time".
 *
 * "Break even time" is the minimum time that the engine needs to be power-gated
 * to over come the overhead power lost.
 *
 * @param [in]  ctrlId      AP Ctrl Id
 *
 * @return      LwU8        best idle filter index
 */
static LwU8
s_apCtrlPredictBestIdleFilterX
(
    LwU8 ctrlId
)
{
    AP_CTRL *pApCtrl                = AP_GET_CTRL(ctrlId);
    LwS32    binIdx;
    LwU8     bestIdleFilterX        = RM_PMU_AP_CFG_HISTOGRAM_BIN_N;
    LwS32    powerSavingHCycles     = 0;
    LwS32    bestPowerSavingHCycles = 0;
    LwU32    sum                    = 0;
    LwU32    prevIdleFilterHCycles;
    LwU32    lwrrentIdleFilterHCycles;
    LwU32    lwrrentIdleFilterCycles;

    // Idle filters are aligned to the histogram bins
    for (binIdx = RM_PMU_AP_CFG_HISTOGRAM_BIN_N - 1; binIdx >= 0; binIdx--)
    {
        //
        // For loop is moving from Higher Histogram BinIdx to Lower Histogram
        // BinIdx. Thus, prevIdleFilterBinIdx = LwrrentIdleFilterBinIdx + 1.
        //
        prevIdleFilterHCycles =
            AP_BIN_IDX_TO_HCYCLES(binIdx + 1, pApCtrl->bin0Cycles);

        lwrrentIdleFilterHCycles =
            AP_BIN_IDX_TO_HCYCLES(binIdx, pApCtrl->bin0Cycles);

        // Increment power saving from Previous BinIdx to Current BinIdx
        powerSavingHCycles += (sum * (prevIdleFilterHCycles -
                                      lwrrentIdleFilterHCycles));

        // Each power-gate inherits the overhead power lost
        powerSavingHCycles -= (pApCtrl->pStat->bin[binIdx] *
                               pApCtrl->powerBreakEvenHCycles);

        //
        // Increment total number of Idle Period
        //
        // If we set lwrrentBinIdx as Idle filter then approximately we will
        // see "sum" number of Power Feature(say ELPG) cycles.
        //
        sum += pApCtrl->pStat->bin[binIdx];

        lwrrentIdleFilterCycles =
            AP_BIN_IDX_TO_CYCLES(binIdx, pApCtrl->bin0Cycles);

        //
        // Requirement 1: Maximum allowed idle filter
        //
        // Check for next lower threshold if current idle filter is greater
        // than maximum idle filter.
        //
        if (lwrrentIdleFilterCycles > pApCtrl->idleThresholdMaxCycles)
        {
            continue;
        }

        //
        // Requirement 2: Maximum allowed power feature cycles
        //
        // We are allowing at max "cyclesPerSampleMax" cycles of power feature.
        // i.e. Our expectation is to have maximum "cyclesPerSampleMax" power
        // feature cycles. Thus, if total number of predicted Power Feature
        // Cycles are exceeding "cyclesPerSampleMax" then return callwlated
        // bestIdleFilterX as best idle threshold for given activity pattern.
        //
        // Stop immediately on violation of this requirement.
        //
        if (sum > pApCtrl->cyclesPerSampleMax)
        {
            break;
        }

        //
        // Requirement 3: Best net power saving
        // Requirement 4: Minimum targeted residency
        //
        // Check if power saving of current idle filter is better than previous
        // one and it meets the requirement of minimum targeted residency
        //
        if ((powerSavingHCycles > 0)                      &&
            (powerSavingHCycles > bestPowerSavingHCycles) &&
            (s_apCtrlCalcResidency(ctrlId, binIdx) >= pApCtrl->minResidency))
        {
            bestPowerSavingHCycles = powerSavingHCycles;
            bestIdleFilterX        = binIdx;
        }

        //
        // Requirement 5: Minimum allowed idle filter
        //
        // Stop if current idle filter is less than or equal to minimum idle
        // filter.
        //
        if (lwrrentIdleFilterCycles <= pApCtrl->idleThresholdMinCycles)
        {
            break;
        }
    }

    return bestIdleFilterX;
}

/*!
 * @brief Callwlate net power saving for given idle filter index.
 *
 * Following formula gives us Net Power Saving:
 * Net power saving = "idle period" - "idle filter" - "break even time".
 *
 * Say target idle filter is "i" and bin0Cycles=1. Thus, Idle threshold
 * corresponding to idle filter "i" will be BIT(i). "i"th bin aclwmulates
 * idle periods from BIT[i] to BIT[i+1]-1 pwrclk cycles. Consider worst case
 * where each idle period in bin "i" represent (BIT[i]+1) cycles. Thus, setting
 * BIT[i] as idle threshold will raise PG_ON interrupt for all idle periods in
 * bin[i] but every time ELPG sequence will get aborted. Performing partial
 * ELPG cycles will generate extra overhead/perf hit.
 *
 * @param [in]  ctrlId          AP Ctrl Id
 * @param [in]  targetFilterX   Target idle filter
 *
 * @return      LwU32           Net power savings in usec
 */
static LwS32
s_apCtrlCalcPowerSavingUs
(
    LwU8 ctrlId,
    LwU8 targetFilterX
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);
    LwS32    powerSavingHCycles = 0;
    LwS32    powerSavingUs      = 0;
    LwU32    binIdx;
    LwU32    targetFilterHCycles;
    LwU32    lwrrentFilterHCycles;

    if (targetFilterX >= RM_PMU_AP_CFG_HISTOGRAM_BIN_N)
    {
        goto s_apCtrlCalcPowerSavingUs_exit;
    }

    targetFilterHCycles = AP_BIN_IDX_TO_HCYCLES(targetFilterX,
                                                 pApCtrl->bin0Cycles);

    // Only consider idle filters greater than or equal to target idle filter.
    for (binIdx = targetFilterX; binIdx < RM_PMU_AP_CFG_HISTOGRAM_BIN_N; binIdx++)
    {
        lwrrentFilterHCycles = AP_BIN_IDX_TO_HCYCLES(binIdx,
                                                     pApCtrl->bin0Cycles);

        // Net power saving = "idle period" - "idle filter" - "break even time".
        powerSavingHCycles += (pApCtrl->pStat->bin[binIdx] *
                               (lwrrentFilterHCycles - targetFilterHCycles - pApCtrl->powerBreakEvenHCycles));
    }

    //
    // Power saving usec = power saving cycles / pwrClkMhz
    // Power saving usec = (power saving Hcycles * 100) / pwrClkMhz
    //
    powerSavingUs = (powerSavingHCycles * 100) / AP_GET_PWRCLK_MHZ();

s_apCtrlCalcPowerSavingUs_exit:

    return powerSavingUs;
}

/*!
 * @brief Callwlate parent feature residency for given idle filter index.
 *
 * Following formula gives us Net Power Saving:
 * Residency = Predicted resident time / Sampling period
 *
 * Predicted resident time = Idle slot width -
 *                           Idle threshold  -
 *                           Entry latency   -
 *                           Exit latency
 *
 * Idle slot width is callwlated from histogram bins
 *
 * @param [in]  ctrlId          AP Ctrl Id
 * @param [in]  targetFilterX   Target idle filter
 *
 * @return      LwU8            Predicted residency
 */
static LwU8
s_apCtrlCalcResidency
(
    LwU8 ctrlId,
    LwU8 targetFilterX
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);
    LwU32    totalResidentHcycles = 0;
    LwS32    lwrrentResidentHcycles;
    LwS32    targetFilterHCycles;
    LwS32    lwrrentFilterHCycles;
    LwU32    binIdx;
    LwU8     residency = 0;

    // Residency callwlations are not possible without centralised callback.
    if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
    {
        goto s_apCtrlCalcResidency_exit;
    }

    if (targetFilterX >= RM_PMU_AP_CFG_HISTOGRAM_BIN_N)
    {
        goto s_apCtrlCalcResidency_exit;
    }

    targetFilterHCycles = AP_BIN_IDX_TO_HCYCLES(targetFilterX,
                                                 pApCtrl->bin0Cycles);

    // Only consider idle filters greater than or equal to target idle filter.
    for (binIdx = targetFilterX; binIdx < RM_PMU_AP_CFG_HISTOGRAM_BIN_N; binIdx++)
    {
        lwrrentFilterHCycles = AP_BIN_IDX_TO_HCYCLES(binIdx,
                                                     pApCtrl->bin0Cycles);
        //
        // Resident time for current binIdx = Idle_filter_cycles[Current BinIdx]  -
        //                                    Idle_filter_cycles[targeted BinIdx] -
        //                                    Entry_and_exit_latyency
        //
        lwrrentResidentHcycles = lwrrentFilterHCycles -
                                 targetFilterHCycles  -
                                 s_apCtrlResidencyOverheadGet(ctrlId);

        //
        // Update total resident time only if we get +ve residency in
        // current cycle
        //
        if (lwrrentResidentHcycles > 0)
        {
            totalResidentHcycles += (pApCtrl->pStat->bin[binIdx] * lwrrentResidentHcycles);
        }
    }

    //
    // Residency
    // = ((Resident time in usec) / (Callback time in usec)) * 100
    // = ((Resident time in cycles / pwrClkMhz) / (Callback time in usec)) * 100
    // = ((Resident time in Hcycles / pwrClkMhz) / (Callback time in usec)) * 100 * 100
    // = ((Resident time in Hcycles / pwrClkMhz) / (Callback time in usec / 10000))
    //
    residency = (totalResidentHcycles / AP_GET_PWRCLK_MHZ()) /
                (lpwrCallbackCtrlGetPeriodUs(pApCtrl->callbackId) / 10000);

s_apCtrlCalcResidency_exit:

    return residency;
}

/*!
 * @brief Schedules callback for APCTRL
 *
 * @param[in]   ctrlId      AP Ctrl Id
 *
 * @return      FLCN_OK     On success
 */
static void
s_apCtrlScheduleCallback
(
    LwU8 ctrlId
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
    {
        lpwrCallbackCtrlSchedule(pApCtrl->callbackId);
    }
}

/*!
 * @brief DeSchedules callback for APCTRL
 *
 * @param[in]   ctrlId      AP Ctrl Id
 *
 * @return      FLCN_OK     On success
 */
static void
s_apCtrlDeScheduleCallback
(
    LwU8 ctrlId
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
    {
        lpwrCallbackCtrlDeschedule(pApCtrl->callbackId);
    }
}

/*!
 * @brief Reset Idle Threshold
 *
 * @param [in]  ctrlId      AP Ctrl Id
 *
 * @return      FLCN_OK     On success
 */
static void
s_apCtrlIdleThresholdMaxSet
(
    LwU8 ctrlId
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);

    // Update idle threshold cache
    pApCtrl->idleThresholdCycles = pApCtrl->idleThresholdMaxCycles;

    // Issue lazy updates for idle threshold.
    apCtrlIdleThresholdSet(ctrlId, LW_FALSE);
}

/*!
 * @brief Restore Idle Threshold
 *
 * @param [in]  ctrlId      AP Ctrl Id
 *
 * @return      FLCN_OK     On success
 */
static void
s_apCtrlIdleThresholdRestore
(
    LwU8 ctrlId
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);

    // Set max value if there is no restore value specified for ApCtrl
    pApCtrl->idleThresholdCycles = pApCtrl->idleThresholdMaxCycles;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        LwU32 pwrClkMHz = AP_GET_PWRCLK_MHZ();

        if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR) &&
            ctrlId == RM_PMU_AP_CTRL_ID_GR)
        {
            pApCtrl->idleThresholdCycles =
                Lpwr.pVbiosData->gr.idleThresholdUs * pwrClkMHz;
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1) &&
                 ctrlId == RM_PMU_AP_CTRL_ID_DI_DEEP_L1)
        {
            pApCtrl->idleThresholdCycles =
                Lpwr.pVbiosData->di.idleThresholdUs * pwrClkMHz;
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
                 ctrlId == RM_PMU_AP_CTRL_ID_MSCG)
        {
            pApCtrl->idleThresholdCycles =
                Lpwr.pVbiosData->ms.idleThresholdUs * pwrClkMHz;
        }
    }

    // Immediately flush the idle threshold cache
    apCtrlIdleThresholdSet(ctrlId, LW_TRUE);
}

/*!
 * @brief Increment Histogram Counter
 *
 * Helper function increments HW counters to accommodate time for which PWRCLK
 * was gated. In assistance with apCtrlMergeSwHistBin(), effectively this
 * function simulates behavior as if we are incrementing HW counters.
 *
 * This function forcefully resets Histogram HW counters. Its destructive
 * operation. Say,
 *  Total idle cycles                            = T
 *  Idle cycles for which PWRCLK was Gated       = G
 *  Idle cycles counted by Histogram counters(H) = T-G.
 *
 * SW doesn't have exact information about H cycles (and thus about BinIdx(T-G))
 * as SW doesn't know the exact point at which Histogram Counter is getting
 * reset. Thus, instead of waiting for traffic to come and reset HW counters
 * SW explicitly issues the "RESET".
 *
 * Refer details at "SW correction for HW Histogram Bins" in pmuifap.h.
 *
 * PREREQUISITES:
 * 1) This function should be called when Counter mode is set to FORCE_IDLE.
 * 2) Client needs to set counter mode to AUTO_IDLE after exelwting this function.
 *
 * @param [in]  ctrlId                  AP Ctrl Id
 * @param [in]  ncrementPwrClkCycles    HW counter Increment in PWRCLK cycles
 */
static void
s_apCtrlIncrementHistCntrSw
(
    LwU8  ctrlId,
    LwU32 incrementPwrClkCycles
)
{
    AP_CTRL *pApCtrl = AP_GET_CTRL(ctrlId);
    LwU32    lwrrentIdleCycles;
    LwU32    totalIdleCycles;
    LwU8     histogramIdx;
    LwU8     binIdx = RM_PMU_AP_CFG_HISTOGRAM_BIN_N - 1;
    LwU8     lwrrentBinIdx;
    LwU8     binIdxWithCorrection;

    histogramIdx = apCtrlGetHistogramIndex(ctrlId);

    // Step1: Get Current Idle Cycles callwlated by Histogram
    lwrrentIdleCycles = REG_RD32(CSB, LW_CPWR_PMU_HISTOGRAM_COUNT(histogramIdx));

    //
    // Step2: Reset counter by setting counter mode to FORCE_BUSY.
    //        Histogram will update HW Bins with Current Idle Cycles.
    //
    // This is destructive step. It's not restoring counter mode. We have
    // assumption that GC5 will restore counter mode to AUTO_IDLE.
    //
    pgApConfigCntrMode_HAL(&Pg, apCtrlGetIdleMaskIndex(ctrlId),
                                PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY);

    //
    // Step3: Callwlate Total Idle Cycles with increment
    //        Total Idle Cycles = Current Idle Cycles + Increment
    //
    totalIdleCycles = lwrrentIdleCycles + incrementPwrClkCycles;

    // Step4: Find BinIdx corresponding to Total Idle Cycles
    while ((binIdx > 0) &&
           (AP_BIN_IDX_TO_CYCLES(binIdx, pApCtrl->bin0Cycles) > totalIdleCycles))
    {
        binIdx--;
    }
    binIdxWithCorrection = binIdx;

    // Step5: Get BinIdx Corresponding to lwrrentIdleCycles
    while ((binIdx > 0) &&
           (AP_BIN_IDX_TO_CYCLES(binIdx, pApCtrl->bin0Cycles) > lwrrentIdleCycles))
    {
        binIdx--;
    }
    lwrrentBinIdx = binIdx;

    //
    // Step6: Apply correction if binIdxWithCorrection is different from
    //        lwrrentBinIdx.
    //
    // Correction is -
    // 1) Histogram has unnecessary incremented Bin corresponding to
    //    lwrrentBinIdx. Thus decrement it by one.
    // 2) Increment Bin corresponding to binIdxWithCorrection
    //
    if (lwrrentBinIdx != binIdxWithCorrection)
    {
        pApCtrl->pSwBins[lwrrentBinIdx]--;
        pApCtrl->pSwBins[binIdxWithCorrection]++;
    }
}

/*!
 * @brief Get the latency overhead for residency callwlations
 *
 * @param [in]  ctrlId      AP Ctrl Id
 */
static LwU32
s_apCtrlResidencyOverheadGet
(
    LwU8 ctrlId
)
{
    LwU32 residencyOverheadUs = 0;

    switch (ctrlId)
    {
        case RM_PMU_AP_CTRL_ID_MSCG:
        {
            residencyOverheadUs = AP_LATENCY_OVERHEAD_FOR_RESIDENCY_MSCG_US;
            break;
        }
        default:
        {
            residencyOverheadUs = AP_LATENCY_OVERHEAD_FOR_RESIDENCY_DEFAULT_US;
            break;
        }
    }

    return residencyOverheadUs;
}

/*!
 * @brief Get the latency overhead for power callwlations
 *
 * @param [in]  ctrlId      AP Ctrl Id
 */
static LwU32
s_apCtrlPowerOverheadGet
(
    LwU8 ctrlId
)
{
    LwU32 powerOverheadUs = 0;

    switch (ctrlId)
    {
        case RM_PMU_AP_CTRL_ID_MSCG:
        {
            powerOverheadUs = AP_LATENCY_OVERHEAD_FOR_POWER_MSCG_US;
            break;
        }
        default:
        {
            powerOverheadUs = AP_LATENCY_OVERHEAD_FOR_POWER_DEFAULT_US;
            break;
        }
    }

    return powerOverheadUs;
}
