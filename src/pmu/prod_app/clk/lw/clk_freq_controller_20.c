/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_freq_controller_20.c
 *
 * @brief Module managing all state related to the CLK_FREQ_CONTROLLER V 2.0.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "clk/clk_freq_controller.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
static CLK_FREQ_CONTROLLERS_20 FreqControllers20
    GCC_ATTRIB_SECTION("dmem_clkControllers", "FreqControllers20");
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))

/* ------------------------ Static Function Prototypes --------------------- */
static LwBool s_clkFreqControllerSamplePoisonedHwFs_20(CLK_FREQ_CONTROLLER_20 *pFreqController20)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisonedHwFs_20");
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
static LwBool s_clkFreqControllerSamplePoisonedBA_20(CLK_FREQ_CONTROLLER_20 *pFreqController20)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisonedBA_20");
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
static LwBool s_clkFreqControllerSamplePoisonedDroopy_20(CLK_FREQ_CONTROLLER_20 *pFreqController20)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "s_clkFreqControllerSamplePoisonedDroopy_20");
#endif

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Initialize clock controllers 2.0
 *
 * @return FLCN_OK
 *      Init Success.
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *      If memory is already allocated for clk controllers.
 */
FLCN_STATUS
clkFreqControllersInit_20()
{
    FLCN_STATUS status = FLCN_OK;

    if (CLK_CLK_FREQ_CONTROLLERS_GET() != NULL)
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        PMU_BREAKPOINT();
        goto clkFreqControllersInit_20_exit;
    }

    (CLK_CLK_FREQ_CONTROLLERS_GET()) = &FreqControllers20.super;

    // Set version to 2.0
    (CLK_CLK_FREQ_CONTROLLERS_GET())->version = LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_VERSION_20;

clkFreqControllersInit_20_exit:
    return status;
}

/*!
 * Update cached voltage delta with the value which is actually
 * applied by the voltage code.
 */
void
clkFreqControllersFinalVoltDeltauVUpdate(void)
{
    LwS32 voltDeltauV = 0;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // PP-TODO : We need array of final volt Delta to store per rail
        // volt delta values.
        //
        voltDeltauV = perfChangeSeqChangeLastVoltOffsetuVGet(0,
                        LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Update the cache
    (CLK_CLK_FREQ_CONTROLLERS_GET())->finalVoltDeltauV = voltDeltauV;
}

/*!
 * @ref OsTimerOsCallback
 * OS_TMR callback for frequency controller.
 */
FLCN_STATUS
clkFreqControllerOsCallback_20
(
    OS_TMR_CALLBACK *pCallback
)
{
    CLK_FREQ_CONTROLLER    *pFreqController;
    LwS32                   voltDeltauV    = LW_S32_MIN;
    LwBoardObjIdx           idx;
    LwBool                  bDisabledAll   = LW_TRUE;
    FLCN_STATUS             status         = FLCN_OK;
    OSTASK_OVL_DESC         ovlDescList[]  = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkFreqCountedAvg)
#endif
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    };

    LwBool  bSkipGatedClksControl = LW_FALSE;
    LwU32   msGatingCount;
    LwBool  bMscgEnabled =
        PMUCFG_FEATURE_ENABLED(PMU_PG_MS) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    //
    // Update the final voltage delta with the actually applied value
    // by the voltage code.
    //
    clkFreqControllersFinalVoltDeltauVUpdate();

    if (bMscgEnabled)
    {
        //
        // Get the latest copy of the MSCG entry count and compare it against
        // the cached copy. If the gating counts don't match, it means that
        // there were MSCG cycles since last sampled and we should skip
        // evaluating the non-sysclk controllers in such a case since MSCG
        // gates all the core clocks (except SYSCLK)at the root and frequency
        // control is not possible in such a case.
        //
        // Note the use of critical section since we want to check the current
        // gating count and update the local copy without getting preempted by
        // PG task
        //
        appTaskCriticalEnter();
        {
            msGatingCount = PG_STAT_ENTRY_COUNT_GET(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
            if ((CLK_CLK_FREQ_CONTROLLERS_GET())->msGatingCount != msGatingCount)
            {
                (CLK_CLK_FREQ_CONTROLLERS_GET())->msGatingCount = msGatingCount;
                bSkipGatedClksControl = LW_TRUE;
            }
        }
        appTaskCriticalExit();
    }

    // Skip evaluation if all frequency controllers are disabled.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pFreqController, idx, NULL)
    {
        if (pFreqController->disableClientsMask != 0)
        {
            continue;
        }

        //
        // Skip evaluating the controller if the clock driven by this
        // controller has been gated since the last sample.
        //
        if (bSkipGatedClksControl &&
            (pFreqController->clkDomain & MS_GET_CG_MASK()))
        {
            continue;
        }

        // At least one controller is enabled.
        bDisabledAll = LW_FALSE;
        break;
    }
    BOARDOBJGRP_ITERATOR_END;

    // PP-TODO : Optimization to skip sending reset request to CHSEQ.
    if (bDisabledAll)
    {
        voltDeltauV = 0;
    }
    else
    {
        // If at-least one frequency controller is enable, run the evaluation.

        // Attach required overlays.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pFreqController, idx, NULL)
            {
                // Evaluate controllers.
                PMU_ASSERT_OK_OR_GOTO(status,
                    clkFreqControllerEval(pFreqController),
                    _clkFreqControllerOsCallback_V20_detach);

                // Get max voltage delta across all the enabled controllers.
                voltDeltauV = LW_MAX(voltDeltauV,
                                     clkFreqControllerVoltDeltauVGet(pFreqController));
            }
            BOARDOBJGRP_ITERATOR_END;

_clkFreqControllerOsCallback_V20_detach:
            lwosNOP();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            goto _clkFreqControllerOsCallback_V20_exit;
        }
    }

    // Drop redundant request when CLFC offset is zero.
    if ((!bDisabledAll) &&
        (voltDeltauV == 0))
    {
        goto _clkFreqControllerOsCallback_V20_exit;
    }

    OSTASK_OVL_DESC perfChangeOvlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(perfChangeOvlDescList);
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET voltOffset;

        memset(&voltOffset, 0,
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET));

        // Update rail mask.
        LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(voltOffset.voltRailsMask.super), 0);

        // Update the controller mask.
        voltOffset.rails[0].offsetMask = LWBIT32(LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC);

        // Update volt delta.
        voltOffset.rails[0].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC] =
            voltDeltauV;

        // Force trigger perf change
        voltOffset.bForceChange = LW_TRUE;

        // Apply new delta to perf change sequencer.
        status = perfChangeSeqQueueVoltOffset(&voltOffset);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(perfChangeOvlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkFreqControllerOsCallback_V20_exit;
    }

_clkFreqControllerOsCallback_V20_exit:
    return status;
}

/*!
 * Load frequency 2.0 controllers.
 * Schedules the callback to evaluate the controllers.
 *
 * @param[in] bLoad   LW_TRUE if load ALL the controllers,
 *                    LW_FALSE otherwise
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqControllersLoad_20
(
   RM_PMU_CLK_LOAD *pClkLoad
)
{
    LwBool bLoad;
    LwBool bMscgEnabled;

    bLoad = FLD_TEST_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK, _FREQ_CONTROLLER_CALLBACK,
                _YES, (pClkLoad->actionMask));

    bMscgEnabled = PMUCFG_FEATURE_ENABLED(PMU_PG_MS) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    if (bLoad)
    {
        //
        // We are loading the controllers. Cache the latest count of the MSCG
        // clock gating count if MSCG is supported
        //
        if (bMscgEnabled)
        {
            (CLK_CLK_FREQ_CONTROLLERS_GET())->msGatingCount =
            PG_STAT_ENTRY_COUNT_GET(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
        }
    }
    return LW_OK;

}

/*!
 * Evaluates the frequency controller 2.0 based on the type.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerEval_20
(
    CLK_FREQ_CONTROLLER_20 *pFreqController
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(&pFreqController->super))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            status = clkFreqControllerEval_PI_20(
                (CLK_FREQ_CONTROLLER_20_PI *)pFreqController);
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerEval_20_exit;
    }

clkFreqControllerEval_20_exit:
    return status;
}

/*!
 * Reset the frequency controller 2.0
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerReset_20
(
    CLK_FREQ_CONTROLLER_20 *pFreqController
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pFreqController))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            status = clkFreqControllerReset_PI_20(
                (CLK_FREQ_CONTROLLER_20_PI *)pFreqController);
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerReset_20_exit;
    }

clkFreqControllerReset_20_exit:
    return status;
}

/*!
 * Disable/Enable the frequency controller 2.0 for a given clock domain
 *
 * @param[in]   ctrlId
 *      Index of the frequency controller
 * @param[in]   clientId
 *      Client ID @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_<xyz>.
 * @param[in]   bDisable
 *      Whether to disable the controller. True-Disable, False-Enable.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Unsupported clock domain passed.
 * @return Other
 *      Status returned from the functions called within.
 * @return FLCN_OK
 *      Successfully disabled/enabled the controller.
 */
FLCN_STATUS
clkFreqControllerDisable_20
(
    LwU32  ctrlId,
    LwU8   clientId,
    LwBool bDisable
)
{
    FLCN_STATUS status = FLCN_OK;
    CLK_FREQ_CONTROLLER *pFreqController =
        BOARDOBJGRP_OBJ_GET(CLK_FREQ_CONTROLLER, ctrlId);

    if (pFreqController == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkFreqControllerDisable_20_exit;
    }

    // Update the disable mask
    if (bDisable)
    {
        pFreqController->disableClientsMask |= BIT(clientId);
    }
    else
    {
        pFreqController->disableClientsMask &= ~BIT(clientId);
    }

    // Reset
    status = clkFreqControllerReset(pFreqController);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerDisable_20_exit;
    }

clkFreqControllerDisable_20_exit:
    return status;
}

/*!
 * CLK_FREQ_CONTROLLER_20 super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkFreqControllerGrpIfaceModel10ObjSet_20_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{

    RM_PMU_CLK_CLK_FREQ_CONTROLLER_20_BOARDOBJ_SET *pSet = NULL;
    CLK_FREQ_CONTROLLER_20 *pFreqController20 = NULL;
    FLCN_STATUS             status;

    //
    // Call into CLK_FREQ_CONTROLLER super-constructor, which will do actual memory
    // allocation.
    //
    status = clkFreqControllerGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerGrpIfaceModel10ObjSet_20_SUPER_exit;
    }
    pSet = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_20_BOARDOBJ_SET *)pBoardObjDesc;
    pFreqController20 = (CLK_FREQ_CONTROLLER_20 *)*ppBoardObj;

    // Copy the CLK_FREQ_CONTROLLER-specific data.
    pFreqController20->freqMode             = pSet->freqMode;
    pFreqController20->clkDomainIdx         = pSet->clkDomainIdx;
    pFreqController20->voltControllerIdx    = pSet->voltControllerIdx;
    pFreqController20->voltErrorThresholduV = pSet->voltErrorThresholduV;
    pFreqController20->voltOffsetMinuV      = pSet->voltOffsetMinuV;
    pFreqController20->voltOffsetMaxuV      = pSet->voltOffsetMaxuV;

    // Only if BA feature is enabled.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
    pFreqController20->thermMonIdx0 = pSet->thermMonIdx0;
    pFreqController20->threshold0   = pSet->threshold0;
#endif

    // Only if droopy feature is enabled.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
    pFreqController20->thermMonIdx1 = pSet->thermMonIdx1;
    pFreqController20->threshold1   = pSet->threshold1;
#endif

    // HW slowdown threshold
    pFreqController20->hwSlowdownThreshold = pSet->hwSlowdownThreshold;

clkFreqControllerGrpIfaceModel10ObjSet_20_SUPER_exit:
    return status;
}

/*!
 * CLK_FREQ_CONTROLLER_20 super-class query
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkFreqControllerIfaceModel10GetStatus_20_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJ_GET_STATUS *pGetStatus;
    CLK_FREQ_CONTROLLER_20 *pFreqController20;
    FLCN_STATUS             status;

    status = clkFreqControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerIfaceModel10GetStatus_SUPER_exit;
    }

    pGetStatus = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJ_GET_STATUS *)pPayload;
    pFreqController20 = (CLK_FREQ_CONTROLLER_20 *)pBoardObj;

    // Get the CLK_FREQ_CONTROLLER query parameters if any
    (void)pFreqController20;
    (void)pGetStatus;

clkFreqControllerIfaceModel10GetStatus_SUPER_exit:
    return status;
}

FLCN_STATUS
clkFreqControllerSamplePoisoned_20_SUPER
(
    CLK_FREQ_CONTROLLER_20 *pFreqController20,
    LwBool                 *pbPoisoned
)
{
    LwBool bPoisonedHwfs   = LW_FALSE;
    LwBool bPoisonedBA     = LW_FALSE;
    LwBool bPoisonedDroopy = LW_FALSE;

    // Check if sample is poisoned because of HW failsafe events (excluding BA).
    bPoisonedHwfs = s_clkFreqControllerSamplePoisonedHwFs_20(pFreqController20);

    // Check if sample is poisoned because of BA slowdown.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
    bPoisonedBA = s_clkFreqControllerSamplePoisonedBA_20(pFreqController20);
#endif

    // Check if sample is poisoned because of droopy.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
    bPoisonedDroopy = s_clkFreqControllerSamplePoisonedDroopy_20(pFreqController20);
#endif

    *pbPoisoned = (bPoisonedHwfs || bPoisonedBA || bPoisonedDroopy);

    return FLCN_OK;
}

/* ------------------------- Private Functions ----------------------------- */
/*!
 * Check if the frequency controller sample is poisoned because of
 * hardware failsafe events engaged (excluding BA).
 *
 * The sample is poisoned if any kind of thermal slowdowns are engaged
 * for more than the percentage limit.
 * The thermal violation timer tracks the total time thermal slowdowns
 * are engaged since boot so we take the difference between two samples
 * to get the enagaged time for controller sampling period, colwert into
 * percentage and check with the percentage limit.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisonedHwFs_20
(
    CLK_FREQ_CONTROLLER_20 *pFreqController
)
{
    FLCN_TIMESTAMP  timeNowns;
    FLCN_TIMESTAMP  elapsedTimens;
    LwU32           slowdownTimeNowns;
    LwU32           elapsedSlowdownTimens;
    LwU8            pctVal;
    LwBool          bPoison = LW_FALSE;

    appTaskCriticalEnter();
    {
        // Get current timer values.
        osPTimerTimeNsLwrrentGet(&timeNowns);
        (void)thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                                            &slowdownTimeNowns);
    }
    appTaskCriticalExit();

    // Compute elapsed time.
    lw64Sub(&elapsedTimens.data, &timeNowns.data,
        &pFreqController->prevHwfsEvalTimeNs.data);

    elapsedSlowdownTimens = slowdownTimeNowns -
        pFreqController->prevHwfsEngageTimeNs;

    // cache the current time for the next iteration
    pFreqController->prevHwfsEvalTimeNs.data  = timeNowns.data;

    // cache the current slowdown time for the next iteration
    pFreqController->prevHwfsEngageTimeNs     = slowdownTimeNowns;

    // Colwert time to percentage.
    elapsedSlowdownTimens *= 100;

    PMU_HALT_COND(elapsedTimens.parts.lo != 0);
    pctVal = LW_UNSIGNED_ROUNDED_DIV(elapsedSlowdownTimens,
        elapsedTimens.parts.lo);

    //
    // Poison the sample if slowdown engaged for more than the percentage
    // time limit.
    //
    if (pctVal > pFreqController->hwSlowdownThreshold)
    {
        bPoison = LW_TRUE;
    }

    return bPoison;
}

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED)
/*!
 * Check if the frequency controller sample is poisoned because of
 * BA EDPp engaged.
 *
 * The sample is poisoned if BA was enagaged for more time than the
 * minimum limit @ref baPctMin.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisonedBA_20
(
    CLK_FREQ_CONTROLLER_20 *pFreqController
)
{
    LwUFXP52_12     num_UFXP52_12;
    LwUFXP52_12     quotient_UFXP52_12;
    LwUFXP4_12      baPct;
    FLCN_TIMESTAMP  lwrrBaEvalTimeNs;
    FLCN_TIMESTAMP  elapsedBaEvalTimeNs;
    FLCN_TIMESTAMP  lwrrBaEngageTimeNs;
    FLCN_TIMESTAMP  elapsedBaEngageTimeNs;
    THRM_MON       *pThrmMon;
    FLCN_STATUS     status  = FLCN_OK;
    LwBool          bPoison = LW_FALSE;

    OSTASK_OVL_DESC ovlDescListTherm[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListTherm);
    {
        // Return early if Thermal Monitor Index is invalid.
        if (LW2080_CTRL_THERMAL_THERM_MONITOR_IDX_ILWALID ==
            pFreqController->thermMonIdx0)
        {
            goto s_clkFreqControllerSamplePoisonedBA_20_exit;
        }

        // Get Thermal Monitor object corresponding to index.
        pThrmMon = THRM_MON_GET(pFreqController->thermMonIdx0);
        if (pThrmMon == NULL)
        {
            PMU_BREAKPOINT();
            goto s_clkFreqControllerSamplePoisonedBA_20_exit;
        }

        // Get current timer values.
        status = thrmMonTimer64Get(pThrmMon, &lwrrBaEvalTimeNs, &lwrrBaEngageTimeNs);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkFreqControllerSamplePoisonedBA_20_exit;
        }

        // Compute elapsed time values.
        lw64Sub(&elapsedBaEvalTimeNs.data, &lwrrBaEvalTimeNs.data,
                &pFreqController->prevBaEvalTimeNs.data);
        lw64Sub(&elapsedBaEngageTimeNs.data, &lwrrBaEngageTimeNs.data,
                &pFreqController->prevBaEngageTimeNs.data);

        //
        // BA engage time should not be higher than the evaluation time and
        // evaluation time shouldn't be Zero.
        //
        if (lwU64CmpGT(&elapsedBaEngageTimeNs.data, &elapsedBaEvalTimeNs.data) ||
            (elapsedBaEvalTimeNs.data == LW_TYPES_FXP_ZERO))
        {
            PMU_BREAKPOINT();
            goto s_clkFreqControllerSamplePoisonedBA_20_exit;
        }

        // Cache current time values for next iteration.
        pFreqController->prevBaEvalTimeNs   = lwrrBaEvalTimeNs;
        pFreqController->prevBaEngageTimeNs = lwrrBaEngageTimeNs;

        //
        // Compute percentage time(as ratio in LwUFXP4_12) BA was engaged.
        // Note that imem overlays required for 64-bit math below are already
        // attached in _clkFreqControllerOsCallback function and this function
        // is eventually called from there.
        //
        num_UFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, elapsedBaEngageTimeNs.data);
        lwU64DivRounded(&quotient_UFXP52_12, &num_UFXP52_12, &elapsedBaEvalTimeNs.data);
        baPct = LwU32_LO16(LwU64_LO32(quotient_UFXP52_12));

        // Poison the sample if BA slowdown engaged more than the min limit.
        if (baPct > pFreqController->threshold0)
        {
            bPoison = LW_TRUE;
        }

s_clkFreqControllerSamplePoisonedBA_20_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListTherm);

    return bPoison;
}
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED)
/*!
 * Check if the frequency controller sample is poisoned because of
 * droopy VR engaged.
 *
 * The sample is poisoned if droopy was enagaged for more time than the
 * minimum limit @ref droopyPctMin.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
static LwBool
s_clkFreqControllerSamplePoisonedDroopy_20
(
    CLK_FREQ_CONTROLLER_20 *pFreqController
)
{
    LwUFXP52_12     num_UFXP52_12;
    LwUFXP52_12     quotient_UFXP52_12;
    LwUFXP4_12      droopyPct;
    FLCN_TIMESTAMP  lwrrDroopyEvalTimeNs;
    FLCN_TIMESTAMP  elapsedDroopyEvalTimeNs;
    FLCN_TIMESTAMP  lwrrDroopyEngageTimeNs;
    FLCN_TIMESTAMP  elapsedDroopyEngageTimeNs;
    THRM_MON       *pThrmMon;
    FLCN_STATUS     status  = FLCN_OK;
    LwBool          bPoison = LW_FALSE;

    OSTASK_OVL_DESC ovlDescListTherm[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListTherm);
    {
        // Return early if Thermal Monitor Index is invalid.
        if (LW2080_CTRL_THERMAL_THERM_MONITOR_IDX_ILWALID ==
            pFreqController->thermMonIdx1)
        {
            goto s_clkFreqControllerSamplePoisoned_DROOPY_20_exit;
        }

        // Get Thermal Monitor object corresponding to index.
        pThrmMon = THRM_MON_GET(pFreqController->thermMonIdx1);
        if (pThrmMon == NULL)
        {
            PMU_BREAKPOINT();
            goto s_clkFreqControllerSamplePoisoned_DROOPY_20_exit;
        }

        // Get current timer values.
        status = thrmMonTimer64Get(pThrmMon, &lwrrDroopyEvalTimeNs, &lwrrDroopyEngageTimeNs);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkFreqControllerSamplePoisoned_DROOPY_20_exit;
        }

        // Compute elapsed time values.
        lw64Sub(&elapsedDroopyEvalTimeNs.data, &lwrrDroopyEvalTimeNs.data,
                &pFreqController->prevDroopyEvalTimeNs.data);
        lw64Sub(&elapsedDroopyEngageTimeNs.data, &lwrrDroopyEngageTimeNs.data,
                &pFreqController->prevDroopyEngageTimeNs.data);

        //
        // Droopy engage time should not be higher than the evaluation time and
        // evaluation time shouldn't be Zero.
        //
        if (lwU64CmpGT(&elapsedDroopyEngageTimeNs.data, &elapsedDroopyEvalTimeNs.data) ||
            (elapsedDroopyEvalTimeNs.data == LW_TYPES_FXP_ZERO))
        {
            PMU_BREAKPOINT();
            goto s_clkFreqControllerSamplePoisoned_DROOPY_20_exit;
        }

        // Cache current time values for next iteration.
        pFreqController->prevDroopyEvalTimeNs   = lwrrDroopyEvalTimeNs;
        pFreqController->prevDroopyEngageTimeNs = lwrrDroopyEngageTimeNs;

        //
        // Compute percentage time(as ratio in LwUFXP4_12) droopy was engaged.
        // Note that imem overlays required for 64-bit math below are already
        // attached in _clkFreqControllerOsCallback function and this function
        // is eventually called from there.
        //
        num_UFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, elapsedDroopyEngageTimeNs.data);
        lwU64DivRounded(&quotient_UFXP52_12, &num_UFXP52_12, &elapsedDroopyEvalTimeNs.data);
        droopyPct = LwU32_LO16(LwU64_LO32(quotient_UFXP52_12));

        // Poison the sample if droopy engaged more than the min limit.
        if (droopyPct > pFreqController->threshold1)
        {
            bPoison = LW_TRUE;
        }

s_clkFreqControllerSamplePoisoned_DROOPY_20_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListTherm);

    return bPoison;
}
#endif
