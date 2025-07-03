/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objclk.c
 * @brief  Container-object for PMU CLK routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "pmu_objpmu.h"
#include "task_therm.h"
#include "task_pmgr.h"
#include "clk/clk_domain_pmumon.h"

#include "config/g_clk_private.h"
#if(CLK_MOCK_FUNCTIONS_GENERATED)
#include "config/g_clk_mock.c"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
 * @brief Main structure for all CLK data.
 */
OBJCLK Clk;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Construct the CLK object.
 * @details  This sets up the HAL interface used by the
 * CLK module as well as clearing other SW structures to 0.
 *
 * @return  FLCN_OK
 */
FLCN_STATUS
constructClk_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;

    (void)memset(&Clk, 0, sizeof(OBJCLK));

    // Construct/initialize counted average frequency feature, if enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
    {
        clkFreqCountedAvgConstruct_HAL(&Clk);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAINS_PMUMON))
    {
        status = clkClkDomainsPmumonConstruct();
        if (status != FLCN_OK)
        {
            goto constructClk_exit;
        }
    }

constructClk_exit:
    return status;
}

/*!
 * @brief Pre-Init the CLK object.
 *
 * @details This does any first-time HW loading.
 */
void
clkPreInit_IMPL(void)
{
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_AND_2X_SUPPORTED)
    // Initialize 1x distribution clock domain mask.
    Clk.clkDist1xDomMask = clkDist1xDomMaskGet_HAL(&Clk);
#endif // PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_AND_2X_SUPPORTED)

    // Pre-Init Clock Counter feature, if enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR))
    {
        clkCntrPreInit();
    }

    // Pre-Init ADC Devices feature, if enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES))
    {
        clkAdcPreInit();
    }

    // Pre-Init FREQ_DOMAIN group, if feature is enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    {
        clkFreqDomainPreInit();
    }
}

/*!
 * @brief Post-Init the CLK object.
 *
 * @return FLCN_OK On success
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkFreqControllersInit();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPostInit_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_AVG))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_AVG
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = swCounterAvgInit();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPostInit_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLpwr)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkLpwrPostInit();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPostInit_exit;
        }
    }

clkPostInit_exit:
    return status;
}

/*!
 * @brief Helper interface to ilwalidate the CLK SW state (VF lwrve, NAFLL LUT etc.)
 * @details  This interface will be called from VFE ilwalidate and various CLK set controls
 * which could alter the CLK SW state.
 *
 * @param[in]   bVFEEvalRequired  Boolean to suggest if VFE evaluation is required
 *
 * @return FLCN_OK On success
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkIlwalidate
(
    LwBool bVFEEvalRequired
)
{
    FLCN_STATUS status = FLCN_OK;

    status = preClkIlwalidate(bVFEEvalRequired);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkIlwalidate_exit;
    }

    status = postClkIlwalidate();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkIlwalidate_exit;
    }

clkIlwalidate_exit:
    return status;
}

/*!
 * @brief Helper interface to ilwalidate the CLK VF lwrve.
 *
 * @details This interface will be called from VFE ilwalidate and various CLK set controls
 * which could alter the CLK SW state.
 *
 * @param[in]   bVFEEvalRequired  Boolean to suggest if VFE evaluation is required
 *
 * @pre Caller is also expected to call @ref postClkIlwalidate to update dependencies.
 *
 * @return FLCN_OK On success
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
preClkIlwalidate
(
    LwBool bVFEEvalRequired
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVfIlwalidation)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
#endif
        };

        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_OFFSET_VF_CACHE))
        {
            status = clkDomainsOffsetVFCache();
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto preClkIlwalidate_exit;
            }
        }

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkDomainsCache(bVFEEvalRequired);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto preClkIlwalidate_exit;
        }
    }

    // Compute ADC code correction offset prior to VFE change
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_EXTENDED))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkAdcsComputeCodeOffset(bVFEEvalRequired, LW_TRUE, NULL);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto preClkIlwalidate_exit;
        }
    }

    // Re-compute pstates clock tuples while we hold perf write semaphore
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_INFRA))
    {
        status = perfPstatesCache();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto preClkIlwalidate_exit;
        }
    }

preClkIlwalidate_exit:
    return status;
}

/*!
 * @brief Helper interface to ilwalidate the CLK SW state dependent on VF lwrve.
 * @details  They are  updating NAFLL LUT, PSTATE frequency tuple -> arbiter ilwalidation.
 *
 * This interface will be called from VFE ilwalidate and various CLK set controls
 * which could alter the CLK SW state.
 *
 * @pre Caller is expected to call @ref preClkIlwalidate before calling this interface.
 *
 * @return FLCN_OK On success
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
postClkIlwalidate(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_EXTENDED))
            {
                // 1. Program the ADC correction code offset
                status = clkAdcsProgramCodeOffset(LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                }
            }

            // 2. Update the LUT for all the NAFLL devices
            status = clkNafllUpdateLutForAll();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto postClkIlwalidate_exit;
        }

        // 3. Ilwalidate the PSTATE frequency tuple.
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_INFRA))
        {
            status = perfPstatesIlwalidate(LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto postClkIlwalidate_exit;
            }
        }

        //
        // 4. Notify other tasks of the VF lwrve ilwalidation.
        //    This should always be the last step to ensure perf
        //    task has completed all of its ilwalidation
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_PERF_VF_ILWALIDATION_NOTIFY))
        {
            // Notify therm
            DISP2THERM_CMD disp2Therm = {{ 0 }};

            disp2Therm.therm.hdr.unitId = RM_PMU_UNIT_THERM;
            disp2Therm.therm.hdr.eventType = THERM_EVENT_ID_PERF_VF_ILWALIDATION_NOTIFY;

            status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, THERM), &disp2Therm,
                                       sizeof(disp2Therm), 0);
        }
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PERF_VF_ILWALIDATION_NOTIFY))
        {
            // Notify pmgr
            DISPATCH_PMGR disp2Pmgr = {{ 0 }};

            disp2Pmgr.hdr.unitId = RM_PMU_UNIT_PMGR;
            disp2Pmgr.hdr.eventType = PMGR_EVENT_ID_PERF_VF_ILWALIDATION_NOTIFY;

            status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, PMGR), &disp2Pmgr,
                                       sizeof(disp2Pmgr), 0);
        }
    }

postClkIlwalidate_exit:
    return status;
}

/*!
 * @copydoc ClkFreqDeltaAdjust()
 */
LwU16
clkFreqDeltaAdjust
(
    LwU16 freqMHz,
    LW2080_CTRL_CLK_FREQ_DELTA *pFreqDelta
)
{
    LwS16 freqMHzAdj = 0;

    switch (LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pFreqDelta))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC:
        {
            LwS32 freqDeltaKHz =
                LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pFreqDelta);
            LwS16 freqDeltaMHz = (LwS16)(freqDeltaKHz / 1000);
            //
            // LwS16 range is [-32767, 32768]. Max POR frequency (in MHz) has
            // been well below 10000 MHz, so it should be safe to force cast
            // frequency value from LwU16 to LwS16
            //
            freqMHzAdj         = ((LwS16)freqMHz) + freqDeltaMHz;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_PERCENT:
        {
            LwSFXP4_12 percentOffset =
               LW2080_CTRL_CLK_FREQ_DELTA_GET_PCT(pFreqDelta);
            LwSFXP20_12 freqDeltaFxp = freqMHz * percentOffset;
            LwS32 freqDeltaMHz =
                LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(20, 12, freqDeltaFxp);
            //
            // Given the POR frequency and delta range (< 10000 MHz) it should
            // be safe to cast:
            //  1. freqMHz LwU16=>LwS16
            //  2. freqDeltaMHz LwS32=>LwS16
            //
            freqMHzAdj = (LwS16)freqMHz + (LwS16)freqDeltaMHz;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

    return (LwU16)LW_MAX(0, freqMHzAdj);
}

/*!
 * @copydoc ClkFreqDeltaAdjust()
 */
LwS16
clkOffsetVFFreqDeltaAdjust
(
    LwS16 freqMHz,
    LW2080_CTRL_CLK_FREQ_DELTA *pFreqDelta
)
{
    LwS16 freqMHzAdj = 0;

    switch (LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pFreqDelta))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC:
        {
            LwS32 freqDeltaKHz =
                LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pFreqDelta);
            LwS16 freqDeltaMHz = (LwS16)(freqDeltaKHz / 1000);
            //
            // LwS16 range is [-32767, 32768]. Max POR frequency (in MHz) has
            // been well below 10000 MHz, so it should be safe to force cast
            // frequency value from LwU16 to LwS16
            //
            freqMHzAdj         = freqMHz + freqDeltaMHz;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_PERCENT:
        {
            PMU_BREAKPOINT();
            // What to do in the percentage case?
            // LwSFXP4_12 percentOffset =
            //    LW2080_CTRL_CLK_FREQ_DELTA_GET_PCT(pFreqDelta);
            // LwSFXP20_12 freqDeltaFxp = freqMHz * percentOffset;
            // LwS32 freqDeltaMHz =
            //     LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(20, 12, freqDeltaFxp);
            // //
            // // Given the POR frequency and delta range (< 10000 MHz) it should
            // // be safe to cast:
            // //  1. freqMHz LwU16=>LwS16
            // //  2. freqDeltaMHz LwS32=>LwS16
            // //
            // freqMHzAdj = (LwS16)freqMHz + (LwS16)freqDeltaMHz;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

    return freqMHzAdj;
}

/*!
 * @copydoc ClkVoltDeltaAdjust()
 */
LwU32
clkVoltDeltaAdjust
(
    LwU32 voltageuV,
    LwS32 voltDeltauV
)
{
    LwS32 voltageuVAdj = ((LwS32)voltageuV) + voltDeltauV;
    return (LwU32)LW_MAX(0, voltageuVAdj);
}

/*!
 * @copydoc ClkVoltDeltaAdjust()
 */
LwS32
clkOffsetVFVoltDeltaAdjust
(
    LwS32 voltageuV,
    LwS32 voltDeltauV
)
{
    LwS32 voltageuVAdj = voltageuV + voltDeltauV;
    return voltageuVAdj;
}

/*!
 * @copydoc ClkFreqDeltaIlwert()
 */
void clkFreqDeltaIlwert
(
    LW2080_CTRL_CLK_FREQ_DELTA *pFreqDeltaIlw
)
{
    switch (LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pFreqDeltaIlw))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC:
        {
            pFreqDeltaIlw->data.staticOffset.deltakHz *= -1;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }
}
