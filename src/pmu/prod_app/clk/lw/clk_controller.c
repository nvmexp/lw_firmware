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
 * @file clk_controller.c
 *
 * @brief Module managing all state related to the CLK_CONTROLLER structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "clk/clk_controller.h"
#include "lwostimer.h"
#include "lwostmrcallback.h"

/* ------------------------ Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBClkController;
#endif

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
// Controller callback interfaces
static OsTimerCallback   (s_clkControllerCallback)
    GCC_ATTRIB_SECTION("imem_libClkController", "s_clkControllerCallback")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc (s_clkControllerOsCallback)
    GCC_ATTRIB_SECTION("imem_libClkController", "s_clkControllerOsCallback")
    GCC_ATTRIB_USED();
static FLCN_STATUS s_clkClkControllersQueueVoltOffset (LwBool bOverrideVoltOffset, LwBool bForced)
    GCC_ATTRIB_SECTION("imem_libClkController", "s_clkClkControllersQueueVoltOffset");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Load all clock controllers.
 * Schedules the callback to evaluate the controllers.
 *
 * @param[in] bLoad   LW_TRUE if load ALL the controllers,
 *                    LW_FALSE otherwise
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkClkControllersLoad
(
   RM_PMU_CLK_LOAD *pClkLoad
)
{
    FLCN_STATUS status                = FLCN_OK;
    LwU32       samplingPeriodms      = 0;
    LwU8        lowSamplingMultiplier = 0;

    OSTASK_OVL_DESC ovlDescListFreq[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
    };
    OSTASK_OVL_DESC ovlDescListVolt[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVoltController)
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListFreq);
        {
            clkFreqControllersLoad(pClkLoad);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListFreq);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListVolt);
        {
            samplingPeriodms = CLK_VOLT_CONTROLLER_SAMPLING_PERIOD_GET();
            lowSamplingMultiplier = CLK_VOLT_CONTROLLER_LOW_SAMPLING_MULTIPLIER_GET();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListVolt);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListFreq);
        {
            samplingPeriodms = CLK_FREQ_CONTROLLER_SAMPLING_PERIOD_GET();
            lowSamplingMultiplier = CLK_FREQ_CONTROLLER_LOW_SAMPLING_MULTIPLIER_GET();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListFreq);
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto clkClkControllersLoad_exit;
    }



#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
            if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBClkController))
            {
                osTmrCallbackCreate(&OsCBClkController,                      // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
                    OVL_INDEX_IMEM(libClkController),                        // ovlImem
                    s_clkControllerOsCallback,                               // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                   // queueHandle
                    samplingPeriodms * 1000,                                 // periodNormalus
                    samplingPeriodms * lowSamplingMultiplier * 1000,         // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                    // taskId
                osTmrCallbackSchedule(&OsCBClkController);
            }
            else
            {
                //Update callback period
                osTmrCallbackUpdate(&OsCBClkController,
                    samplingPeriodms * 1000,
                    samplingPeriodms * lowSamplingMultiplier * 1000,
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,
                    OS_TIMER_UPDATE_USE_BASE_LWRRENT);
            }
#else
            osTimerScheduleCallback(
                &PerfOsTimer,                                             // pOsTimer
                PERF_OS_TIMER_ENTRY_CLK_VOLT_CONTROLLER_CALLBACK,         // index
                lwrtosGET_TICK_COUNT(),                                   // ticksPrev
                samplingPeriodms * 1000,                                  // usDelay
                DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC), // flags
                s_clkControllerCallback,                                  // pCallback
                NULL,                                                     // pParam
                OVL_INDEX_IMEM(libClkController));                        // ovlIdx
#endif // (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))

clkClkControllersLoad_exit:
    return status;
}

/*!
 * Process CLK_CONTROLLERS ilwalidate request
 *
 * @param[in] bOverrideVoltOffset  LW_TRUE     Override the voltage offset
 *                                 LW_FALSE    Accumulate the voltage offset
 */
FLCN_STATUS
perfClkControllersIlwalidate(LwBool bOverrideVoltOffset)
{
    LwBoardObjIdx   railIdx;
    VOLT_RAIL      *pRail = NULL;
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList [] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVoltController)
    };

    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))
    {
        // Skip processing ilwalidate request if volt controller is disabled
        return FLCN_OK;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if ((CLK_VOLT_CONTROLLERS_GET())->super.super.objSlots == 0U)
        {
            // Skip processing ilwalidate request if volt controller is disabled
            status = FLCN_OK;
            goto perfClkControllersIlwalidate_exit;
        }

        //
        // Trigger perf change without changing the total voltage offset
        // This is required to ensure that final Vtarget = Varb + Vctrloffset
        // stays within the updated voltage range bounds
        //
        BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, railIdx, NULL)
        {
            CLK_VOLT_CONTROLLERS_SAMPLE_MAX_VOLT_OFFSET_UV_RESET(railIdx);
        }
        BOARDOBJGRP_ITERATOR_END;

        // Always force trigger perf change queue on ilwalidation.
        status = s_clkClkControllersQueueVoltOffset(bOverrideVoltOffset, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfClkControllersIlwalidate_exit;
        }

perfClkControllersIlwalidate_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------ Private Functions ------------------------------ */
/*!
 * @ref OsTimerOsCallback
 * OS_TMR callback for clk controllers.
 */
static FLCN_STATUS
s_clkControllerOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC clkVoltOvlDescList[]  = {
        OSTASK_OVL_DESC_DEFINE_LIB_AVG
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVoltController)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVoltController)
    };

    // Attach required overlays.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(clkVoltOvlDescList);
        {
            (void)clkVoltControllerOsCallback(pCallback);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(clkVoltOvlDescList);
    }

    // Do not need to force trigger perf inject if offset is zero.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_clkClkControllersQueueVoltOffset(LW_FALSE, LW_FALSE),
        s_clkControllerOsCallback_exit);

s_clkControllerOsCallback_exit:
    return status;
}

/*!
 * @ref OsTimerCallback
 * OS_TIMER callback for voltage controller.
 */
static void
s_clkControllerCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_clkControllerOsCallback(NULL);
}


/*!
 * Helper interface to queue the new voltage offset values to change sequencer
 *
 * @param[in] bOverrideVoltOffset  LW_TRUE     Override the voltage offset
 *                                 LW_FALSE    Accumulate the voltage offset
 * @param[in] bForced              LW_TRUE     Force trigger perf change queue
 *                                 LW_FALSE    Otherwise
 */
static FLCN_STATUS
s_clkClkControllersQueueVoltOffset
(
    LwBool bOverrideVoltOffset,
    LwBool bForced
)
{
    LwBool          bChangeRequired         = LW_FALSE;
    FLCN_STATUS     status                  = FLCN_OK;
    OSTASK_OVL_DESC perfChangeOvlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(perfChangeOvlDescList);
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET voltOffset;
        LwU8                                                 railIdx;

        memset(&voltOffset, 0,
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET));

        // Init the offset override request.
        voltOffset.bOverrideVoltOffset = bOverrideVoltOffset;
        if (bOverrideVoltOffset)
        {
            bChangeRequired = LW_TRUE;
        }

        for (railIdx = 0; railIdx < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; railIdx++)
        {
            if (VOLT_RAIL_INDEX_IS_VALID(railIdx))
            {
                if ((bForced) ||
                    ((CLK_VOLT_CONTROLLERS_GET())->sampleMaxVoltOffsetuV[railIdx] != RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED))
                {
                    // Update rail mask
                    LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(voltOffset.voltRailsMask.super), railIdx);

                    // Update the controller mask.
                    voltOffset.rails[railIdx].offsetMask = LWBIT32(LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLVC);

                    // Update volt offset
                    voltOffset.rails[railIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLVC] =
                        (CLK_VOLT_CONTROLLERS_GET())->sampleMaxVoltOffsetuV[railIdx];

                    // Request change
                    bChangeRequired = LW_TRUE;
                }
            }
        }

        if (bChangeRequired)
        {
            // Force trigger perf change
            voltOffset.bForceChange = LW_TRUE;

            // Apply new delta to perf change sequencer.
            status = perfChangeSeqQueueVoltOffset(&voltOffset);
        }

    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(perfChangeOvlDescList);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkClkControllersQueueVoltOffset_exit;
    }

s_clkClkControllersQueueVoltOffset_exit:
    return status;
}
