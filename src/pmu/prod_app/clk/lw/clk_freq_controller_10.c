/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_freq_controller_10.c
 *
 * @brief Module managing all state related to the CLK_FREQ_CONTROLLER structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "clk/clk_freq_controller.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
static CLK_FREQ_CONTROLLERS_10 FreqControllers10
    GCC_ATTRIB_SECTION("dmem_clkControllers", "FreqControllers10");
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
static OS_TMR_CALLBACK OsCBClkFreqController10;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
/* ------------------------ Static Function Prototypes --------------------- */
FLCN_STATUS _clkFreqControllersVoltageAboveNoiseUnawareVmin(LwBool *pbAboveVmin, LwS32 *pVoltMaxDelta)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "_clkFreqControllersVoltageAboveNoiseUnawareVmin");
FLCN_STATUS _clkFreqControllersFreqCapApply(LwBool bAboveNoiseUnawareVmin)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "_clkFreqControllersFreqCapApply");
FLCN_STATUS _clkFreqControllersVoltOffsetApply(LwS32 maxVoltDeltaNegativeuV)
    GCC_ATTRIB_SECTION("imem_libClkFreqController", "_clkFreqControllersVoltOffsetApply");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Initialize clock controllers 1.0
 *
 * @return FLCN_OK
 *      Init Success.
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *      If memory is already allocated for clk controllers.
 */
FLCN_STATUS
clkFreqControllersInit_10()
{
    FLCN_STATUS status = FLCN_OK;

    if (CLK_CLK_FREQ_CONTROLLERS_GET() != NULL)
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        PMU_BREAKPOINT();
        goto clkFreqControllersInit_10_exit;
    }

    (CLK_CLK_FREQ_CONTROLLERS_GET()) = &FreqControllers10.super;

    // Set version to 1.0
    (CLK_CLK_FREQ_CONTROLLERS_GET())->version = LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_VERSION_10;

clkFreqControllersInit_10_exit:
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

    // Get the applied voltage delta
    if (FLCN_OK != voltPolicyOffsetGet((CLK_CLK_FREQ_CONTROLLERS_GET())->voltPolicyIdx,
                                      &voltDeltauV))
    {
        PMU_BREAKPOINT();
        return;
    }

    // Update the cache
    (CLK_CLK_FREQ_CONTROLLERS_GET())->finalVoltDeltauV = voltDeltauV;
}

/*!
 * Apply and cache the final voltage delta which is the max value from all the
 * frequency controllers.
 *
 * @param[in] voltDeltauV   Final voltage delta in uV.
 *
 * @return Status returned from functions called within.
 */
FLCN_STATUS
clkFreqControllersFinalVoltDeltauVSet
(
    LwS32 voltDeltauV
)
{
    FLCN_STATUS status = FLCN_OK;

    // Apply the final voltage delta
    status = voltPolicyOffsetVoltage((CLK_CLK_FREQ_CONTROLLERS_GET())->voltPolicyIdx,
                                     voltDeltauV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // Cache the final voltage delta
    (CLK_CLK_FREQ_CONTROLLERS_GET())->finalVoltDeltauV = voltDeltauV;

    return status;
}

/*!
 * Load/ Unload ALL frequency controllers.
 *
 * @param[in] bLoad   LW_TRUE if load ALL the controllers,
 *                    LW_FALSE otherwise
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqControllersLoad_WRAPPER
(
   LwBool bLoad
)
{
    RM_PMU_CLK_LOAD clkLoad = {0};
    FLCN_STATUS     status  = FLCN_OK;

    // Short-circuit if the frequency controllers are disabled at TOT.
    if ((CLK_CLK_FREQ_CONTROLLERS_GET())->super.super.objSlots == 0)
    {
        goto clkFreqControllersLoad_WRAPPER_exit;
    }

    clkLoad.feature    = LW_RM_PMU_CLK_LOAD_FEATURE_FREQ_CONTROLLER;
    clkLoad.actionMask =  bLoad ?
        FLD_SET_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK, _FREQ_CONTROLLER_CALLBACK,
            _YES, clkLoad.actionMask) :
        FLD_SET_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK, _FREQ_CONTROLLER_CALLBACK,
            _NO, clkLoad.actionMask);

    clkLoad.payload.freqControllers10.clientId = LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_VF_SWITCH;

    status = clkFreqControllersLoad_10(&clkLoad);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllersLoad_WRAPPER_exit;
    }

clkFreqControllersLoad_WRAPPER_exit:
    return status;
}

/*!
 * @ref OsTimerOsCallback
 * OS_TMR callback for frequency controller.
 */
FLCN_STATUS
clkFreqControllerOsCallback_10
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS     status                 = FLCN_OK;
    LwS32           maxVoltDeltNegativeauV = LW_S32_MIN;
    LwBool          bAboveNoiseUnawareVmin = LW_FALSE;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkFreqCountedAvg)
#endif
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Skip callback if VF_SWITCH client has disabled the controllers. Force
        // call this callback from LOAD command (while enabling the controllers).
        //
        if (CLK_FREQ_CONTROLLER_IS_CONT_MODE_ENABLED)
        {
            CLK_FREQ_CONTROLLER *pFreqController;
            LwBoardObjIdx idx;

            // Reset the flag.
            (CLK_CLK_FREQ_CONTROLLERS_GET())->bCallbackSkipped = LW_FALSE;

            BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pFreqController, idx, NULL)
            {
                if ((pFreqController->disableClientsMask &
                     BIT(LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_VF_SWITCH)) != 0)
                {
                    (CLK_CLK_FREQ_CONTROLLERS_GET())->bCallbackSkipped = LW_TRUE;
                    break;
                }
            }
            BOARDOBJGRP_ITERATOR_END;

            //
            // Skip the callback if any of the controller is disabled during
            // VF switch.
            //
            if ((CLK_CLK_FREQ_CONTROLLERS_GET())->bCallbackSkipped)
            {
                goto clkFreqControllerOsCallback_10_exit;
            }
        }

        //
        // Update the final voltage delta with the actually applied value
        // by the voltage code
        //
        clkFreqControllersFinalVoltDeltauVUpdate();

        //
        // At the start of the each evaluation find if the set voltage is
        // above/below the noise unaware vmin. Note: we do this at the beginning of
        // each evaluation to take into account the voltage delta that would have
        // been applied in the previous cycle.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            _clkFreqControllersVoltageAboveNoiseUnawareVmin(
                &bAboveNoiseUnawareVmin, &maxVoltDeltNegativeauV),
            clkFreqControllerOsCallback_10_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            _clkFreqControllersFreqCapApply(bAboveNoiseUnawareVmin),
            clkFreqControllerOsCallback_10_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            _clkFreqControllersVoltOffsetApply(maxVoltDeltNegativeauV),
            clkFreqControllerOsCallback_10_exit);

clkFreqControllerOsCallback_10_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Load frequency 1.0 controllers.
 * Schedules the callback to evaluate the controllers.
 *
 * @param[in] bLoad   LW_TRUE if load ALL the controllers,
 *                    LW_FALSE otherwise
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqControllersLoad_10
(
   RM_PMU_CLK_LOAD *pClkLoad
)
{
    CLK_FREQ_CONTROLLER *pFreqController;
    LwBoardObjIdx idx    = 0;
    FLCN_STATUS   status = FLCN_OK;
    LwBool        bLoad;
    LwBool        bMscgEnabled;

    bLoad = FLD_TEST_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK, _FREQ_CONTROLLER_CALLBACK,
                _YES, (pClkLoad->actionMask));

    bMscgEnabled = PMUCFG_FEATURE_ENABLED(PMU_PG_MS) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    // Loop All: If bLoad is TRUE enable them if bDisable is not TRUE. Else disable them.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pFreqController, idx, NULL)
    {
        if (bLoad)
        {
            if (!pFreqController->bDisable)
            {
                status = clkFreqControllerDisable(idx,
                    pClkLoad->payload.freqControllers10.clientId, LW_FALSE);
            }
        }
        else
        {
            status = clkFreqControllerDisable(idx,
                pClkLoad->payload.freqControllers10.clientId, LW_TRUE);
        }
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkFreqControllersLoad_10_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (bLoad)
    {
        //
        // Skip re-scheduling the callback function if continuous mode is
        // enabled and the client ID is VF_SWITCH.
        //
        if (CLK_FREQ_CONTROLLER_IS_CONT_MODE_ENABLED &&
            (pClkLoad->payload.freqControllers10.clientId ==
             LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_VF_SWITCH))
        {
            // Force call the callback function if it was skipped during VF switch.
            if ((CLK_CLK_FREQ_CONTROLLERS_GET())->bCallbackSkipped)
            {
                clkFreqControllerOsCallback(NULL);
            }
        }
        else
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

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
            if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBClkFreqController10))
            {
                osTmrCallbackCreate(&OsCBClkFreqController10,               // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(libClkFreqController),                   // ovlImem
                    clkFreqControllerOsCallback,                            // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                  // queueHandle
                    (CLK_CLK_FREQ_CONTROLLERS_GET())->samplingPeriodms * 1000,          // periodNormalus
                    ((CLK_CLK_FREQ_CONTROLLERS_GET())->samplingPeriodms *
                    CLK_FREQ_CONTROLLER_LOW_SAMPLING_MULTIPLIER * 1000) ,   // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                   // taskId
                osTmrCallbackSchedule(&OsCBClkFreqController10);
            }
            else
            {
                //Update callback period
                osTmrCallbackUpdate(&OsCBClkFreqController10,
                                     (CLK_CLK_FREQ_CONTROLLERS_GET())->samplingPeriodms * 1000,
                                     (CLK_CLK_FREQ_CONTROLLERS_GET())->samplingPeriodms * CLK_FREQ_CONTROLLER_LOW_SAMPLING_MULTIPLIER * 1000,
                                     OS_TIMER_RELAXED_MODE_USE_NORMAL,
                                     OS_TIMER_UPDATE_USE_BASE_LWRRENT);
            }
#else
            osTimerScheduleCallback(
                &PerfOsTimer,                                             // pOsTimer
                PERF_OS_TIMER_ENTRY_CLK_FREQ_CONTROLLER_CALLBACK,         // index
                lwrtosTaskGetTickCount32(),                               // ticksPrev
                (CLK_CLK_FREQ_CONTROLLERS_GET())->samplingPeriodms * 1000,            // usDelay
                DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC), // flags
                clkFreqControllerCallback,                                // pCallback
                NULL,                                                     // pParam
                OVL_INDEX_IMEM(libClkFreqController));                    // ovlIdx
#endif
        }
    }

clkFreqControllersLoad_10_exit:
    return status;
}

/*!
 * Evaluates the frequency controller 1.0 based on the type.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerEval_10
(
    CLK_FREQ_CONTROLLER_10 *pFreqController10
)
{
    switch (BOARDOBJ_GET_TYPE(&pFreqController10->super))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI:
        {
            return clkFreqControllerEval_PI_10(
                (CLK_FREQ_CONTROLLER_10_PI *)pFreqController10);
        }
        default:
        {
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}


/*!
 * Reset the frequency controller 1.0
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerReset_10
(
    CLK_FREQ_CONTROLLER_10 *pFreqController10
)
{
    switch (BOARDOBJ_GET_TYPE(&pFreqController10->super))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI:
        {
            return clkFreqControllerReset_PI_10(
                (CLK_FREQ_CONTROLLER_10_PI *)pFreqController10);
        }
        default:
        {
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Disable/Enable the frequency controller 1.0 for a given clock domain
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
clkFreqControllerDisable_10
(
    LwU32  ctrlId,
    LwU8   clientId,
    LwBool bDisable
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32      prevMask;
    CLK_FREQ_CONTROLLER *pFreqController =
        BOARDOBJGRP_OBJ_GET(CLK_FREQ_CONTROLLER, ctrlId);
    LwBool bSkipReset = LW_FALSE;

    if (pFreqController == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkFreqControllerDisable_10_exit;
    }

    prevMask = pFreqController->disableClientsMask;

    // Update the disable mask
    if (bDisable)
    {
        pFreqController->disableClientsMask |= BIT(clientId);
    }
    else
    {
        pFreqController->disableClientsMask &= ~BIT(clientId);
    }

    //
    // Skip resetting the controller if continuous mode is enabled and
    // client ID is VF_SWITCH.
    //
    if (CLK_FREQ_CONTROLLER_IS_CONT_MODE_ENABLED &&
        (clientId == LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_VF_SWITCH))
    {
        bSkipReset = LW_TRUE;
    }

    // Reset controller if it's getting enabled now.
    if (!bSkipReset && (prevMask != 0) &&
        (pFreqController->disableClientsMask == 0))
    {
        status = clkFreqControllerReset(pFreqController);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkFreqControllerDisable_10_exit;
        }
    }

clkFreqControllerDisable_10_exit:
    return status;
}

/*!
 * CLK_FREQ_CONTROLLER_10 super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkFreqControllerGrpIfaceModel10ObjSet_10_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status = LW_OK;

    //
    // Call into CLK_FREQ_CONTROLLER super-constructor, which will do actual memory
    // allocation.
    //
    status = clkFreqControllerGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerGrpIfaceModel10ObjSet_10_SUPER_exit;
    }

clkFreqControllerGrpIfaceModel10ObjSet_10_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkFreqControllerIfaceModel10GetStatus_10_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    FLCN_STATUS status = FLCN_OK;

    status = clkFreqControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkFreqControllerIfaceModel10GetStatus_10_SUPER_exit;
    }

clkFreqControllerIfaceModel10GetStatus_10_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * For ALL supported voltage rails, check if the set voltage for all rails is
 * greater than their corresponding noise unaware Vmin(s).
 * If the set voltage(s) is not greater than the noise unaware Vmin(s) for any
 * of the rails return LW_FALSE.
 * If above noise unaware Vmin, this interface will also return the maximum
 * delta that can be applied.
 *
 * @param[out] pbAboveVmin   LW_TRUE if set voltage(s) is greater than the
 *                                  noise unaware Vmin for all the rails.
 *                           LW_FALSE otherwise
 * @param[out] pVoltDeltaMax The maximum negative delta that can be applied
 * @return FLCN_OK if checked successfully for all supported rails
 *         other errors propagated up the stack from individual functions
 */
FLCN_STATUS
_clkFreqControllersVoltageAboveNoiseUnawareVmin
(
   LwBool *pbAboveVmin,
   LwS32  *pVoltMaxDelta
)
{
    VOLT_RAIL    *pRail;
    LwU32         voltageuV     = 0;
    LwU32         voltageNoiseUnawareVminuV = 0;
    LwBoardObjIdx idx;
    FLCN_STATUS   status        = FLCN_OK;

    *pbAboveVmin      = LW_TRUE;
    *pVoltMaxDelta = LW_S32_MIN;

    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, idx, NULL)
    {
        status = voltRailGetVoltage(pRail, &voltageuV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllersVoltageAboveNoiseUnawareVmin_exit;
        }
        status = voltRailGetNoiseUnawareVmin(pRail, &voltageNoiseUnawareVminuV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllersVoltageAboveNoiseUnawareVmin_exit;
        }

        //
        // TODO Amit:  Apply voltage hysteresis here using the voltage margin
        // VFE. Otherwise when we control the voltage we will keep bouncing
        // above and below the noise unaware Vmin at the boundary conditions
        //

        //
        // Check if the set voltage voltageuV is less than or equal to the
        // voltageNoiseUnawareVminuV. If yes, it means that we are not above
        // the Vmin. Set *pbAboveVmin to LW_FALSE, pVoltDeltaMax to 0 and
        // break out early from the loop.
        //
        if (voltageuV == voltageNoiseUnawareVminuV)
        {
            *pbAboveVmin      = LW_FALSE;
            *pVoltMaxDelta = 0;
            break;
        }

        // The current set voltage includes the delta that would have been
        // applied by the previous callback of the frequency controller. We
        // don't want to include that when trying to determine how far we are
        // from the noise unaware Vmin. So subtract it here from the voltageuV
        //
        voltageuV -= (CLK_CLK_FREQ_CONTROLLERS_GET())->finalVoltDeltauV;

        //
        // Get the MAX of the delta from previous iteration and the current one.
        // Note: For the first rail, pVoltDeltaMax should be set to LW_S32_MIN
        // and (voltageNoiseUnawareVminuV - voltageuV) being a negative value
        // will always allow us to select the smallest max negative delta.
        //
        *pVoltMaxDelta = LW_MAX(*pVoltMaxDelta,
                                (LwS32)(voltageNoiseUnawareVminuV - voltageuV));
    }
    BOARDOBJGRP_ITERATOR_END;

_clkFreqControllersVoltageAboveNoiseUnawareVmin_exit:
    return status;
}

/*!
 * @brief Apply the frequency cap for all the controllers
 *
 * This routine takes a parameter that specifies which region we are in and
 * applies the frequency cap for that region if the lwrrently applied cap is
 * different from it. Also since the application of the cap touches a register
 * that is gated by MSCG - it has to explicitly disallow MSCG.
 *
 * @param[out] bAboveNoiseUnawareVmin  If we are in a region above the noise
 *                                     unaware Vmin
 *
 * @return FLCN_OK if caps applied successfully
 *         other errors propagated up the stack from individual functions
 */
FLCN_STATUS
_clkFreqControllersFreqCapApply
(
   LwBool bAboveNoiseUnawareVmin
)
{
    CLK_FREQ_CONTROLLER *pFreqController  = NULL;
    LwBoardObjIdx        idx;
    FLCN_STATUS          status           = FLCN_OK;
    LwBool               bMsGrpDisallowed = LW_FALSE;

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??
#endif

    // Loop on all the controllers for evaluation.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pFreqController, idx, NULL)
    {
        LwU16             freqOffsetMhz;

        // Skip if the controller is disabled.
        if (pFreqController->disableClientsMask != 0)
        {
            continue;
        }

        freqOffsetMhz = bAboveNoiseUnawareVmin ?
                            pFreqController->freqCapNoiseUnawareVminAbove :
                            pFreqController->freqCapNoiseUnawareVminBelow;

        lwosVarNOP1(freqOffsetMhz);

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V2X))
        CLK_NAFLL_DEVICE *pNafllDev = pFreqController->pNafllDev;

        // Program the new offset only if it's changing.
        if (freqOffsetMhz != clkNafll2xOffsetFreqMHzGet(pNafllDev))
        {
            //
            // Disable MS Group before issuing the first access that sets the
            // frequency cap. Setting the frequency cap ilwolves writing to
            // PTRIM register which cannot be done if the clock is gated
            //
            if (!bMsGrpDisallowed)
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
                lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
                bMsGrpDisallowed = LW_TRUE;
            }

            // Set the new frequency offset
            status = clkNafll2xOffsetFreqMHzSet(pNafllDev, freqOffsetMhz);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
            }
        }
#endif
    }
    BOARDOBJGRP_ITERATOR_END;

    // Re-enable MS Group if it was disabled during any of the iteration above
    if (bMsGrpDisallowed)
    {
        lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif

    return status;
}

/*!
 * @brief Evaluate and apply the voltage offset for all the controllers
 *
 * This routine evaluates the voltage offset that needs to be applied for the
 * frequency errors for each of the supported NAFLLs. It then takes the MAX of
 * the deltas from the ilwidual controllers and sends it to the volt object.
 *
 * The voltage offset can either be a positive or negative. When the overall
 * voltage offset is negative, we have to make sure that the offset doesn't take
 * the set voltage below the noise unaware Vmin. For this, the routine takes a
 * parameter that tells it how much of a negative offset it can apply.
 *
 * MSCG, since it gates the GPC, LTC, XBAR clocks skews the frequency errors
 * causing the controllers to saturate the max voltage delta. To avoid this, we
 * skip evaluating those controllers if there was a MSCG cycle since we last
 * sampled.
 *
 * @param[out] maxVoltDeltaNegativeuV  Maximum negative voltage delta that can
 *                                     be applied
 *
 * @return FLCN_OK if voltage offset was evaluated and applied successfully.
 *         other errors propagated up the stack from individual functions
 */
FLCN_STATUS
_clkFreqControllersVoltOffsetApply
(
   LwS32 maxVoltDeltaNegativeuV
)
{
    LwU32                msGatingCount;
    CLK_FREQ_CONTROLLER *pFreqController = NULL;
    FLCN_STATUS          status          = FLCN_OK;
    LwS32                voltDeltauV     = LW_S32_MIN;
    LwBoardObjIdx        idx;
    LwBool               bDisabledAll    = LW_TRUE;
    LwBool               bDisallowed     = LW_FALSE;
    LwBool               bSkipGatedClksControl = LW_FALSE;
    LwBool               bMscgEnabled;

    bMscgEnabled = PMUCFG_FEATURE_ENABLED(PMU_PG_MS) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    // Loop on all the controllers for evaluation.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pFreqController, idx, NULL)
    {
        LwS32  tmpVoltDeltauV;

        // Skip evaluating the controller if its disabled
        if (pFreqController->disableClientsMask != 0)
        {
            continue;
        }

        if (bMscgEnabled && !bDisallowed)
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
                    bSkipGatedClksControl             = LW_TRUE;
                }
            }
            appTaskCriticalExit();

            //
            // Disallow MSCG to prevent any further cycles from engaging during the
            // evaluation itself, otherwise the evaluation will result in skewed
            // results.
            // TODO-pankumar: We need to colwert this case to LPWR Grp API
            //
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
            pgCtrlDisallowExt(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
            bDisallowed = LW_TRUE;
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

        status = clkFreqControllerEval(pFreqController);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkFreqControllersVoltOffsetApply_exit;
        }

        // Get max voltage delta across all the enabled controllers.
        tmpVoltDeltauV = clkFreqControllerVoltDeltauVGet(pFreqController);
        voltDeltauV    = LW_MAX(tmpVoltDeltauV, voltDeltauV);

        // Atleast one controller is enabled.
        bDisabledAll = LW_FALSE;
    }
    BOARDOBJGRP_ITERATOR_END;

    // Apply a zero voltage delta if all the controllers are disabled.
    if (bDisabledAll)
    {
        voltDeltauV = 0;
    }
    else
    {
        // Adjust the per sample max delta with last applied final delta.
        voltDeltauV += clkFreqControllersFinalVoltDeltauVGet();

        //
        // Get the max voltage delta that will be allowed by the noise-unaware
        // Vmin(s) of the all the voltage rail(s).
        //
        voltDeltauV = LW_MAX(voltDeltauV, maxVoltDeltaNegativeuV);
    }

    // Store and apply the final voltage delta
    status = clkFreqControllersFinalVoltDeltauVSet(voltDeltauV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkFreqControllersVoltOffsetApply_exit;
    }

_clkFreqControllersVoltOffsetApply_exit:
    if (bMscgEnabled && bDisallowed)
    {
        pgCtrlAllowExt(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
    }
    return status;
}
