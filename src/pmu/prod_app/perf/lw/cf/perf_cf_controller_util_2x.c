/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_util_2x.c
 * @copydoc perf_cf_controller_util_2x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller_util_2x.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfCfControllerApplyOutputFilter(PERF_CF_CONTROLLER *pController, LwBool bInc);
static FLCN_STATUS s_perfCfControllerApplyGainFilter(PERF_CF_CONTROLLER *pController, LwBool bInc);
static FLCN_STATUS s_perfCfControllerApplyHysteresisFilter(PERF_CF_CONTROLLER *pController, LwBool bInc);
static FLCN_STATUS s_perfCfControllerApplyIIRFilter(PERF_CF_CONTROLLER *pController, LwBool bInc);

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CONTROLLER_UTIL_2X   *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER_UTIL_2X *)pBoardObjDesc;
    PERF_CF_CONTROLLER_UTIL_2X          *pControllerUtil2x;
    FLCN_STATUS status;

    status = perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X_exit;
    }
    pControllerUtil2x = (PERF_CF_CONTROLLER_UTIL_2X *)*ppBoardObj;

    // Set member variables.
    pControllerUtil2x->IIRCountInc = pDescController->IIRCountInc;
    pControllerUtil2x->IIRCountDec = pDescController->IIRCountDec;

    // sanity check
    if (pControllerUtil2x->IIRCountInc == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X_exit;
    }

    if (pControllerUtil2x->IIRCountDec == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X_exit;
    }

perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_2X_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_UTIL_2X
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    FLCN_STATUS status = FLCN_OK;

    status = perfCfControllerIfaceModel10GetStatus_UTIL(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerIfaceModel10GetStatus_UTIL_2X_exit;
    }

perfCfControllerIfaceModel10GetStatus_UTIL_2X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_UTIL_2X
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pController;    
    FLCN_STATUS              status;
    LwBool                   bInc;

    // SUPER
    status = perfCfControllerExelwte_UTIL(pController);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerExelwte_UTIL_2X_exit;
    }

    // Check if clocks should be increased or decreased
    bInc = pControllerUtil->lwrrentkHz < pControllerUtil->targetkHz;

    status = s_perfCfControllerApplyOutputFilter(pController, bInc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerExelwte_UTIL_2X_exit;
    }

perfCfControllerExelwte_UTIL_2X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
FLCN_STATUS
perfCfControllerSetMultData_UTIL_2X
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    FLCN_STATUS status;

    status = perfCfControllerSetMultData_UTIL(pController, pMultData);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerSetMultData_UTIL_2X_exit;
    }

perfCfControllerSetMultData_UTIL_2X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
FLCN_STATUS
perfCfControllerLoad_UTIL_2X
(
    PERF_CF_CONTROLLER *pController
)
{
    FLCN_STATUS status;

    status = perfCfControllerLoad_UTIL(pController);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerLoad_UTIL_2X_exit;
    }

perfCfControllerLoad_UTIL_2X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * Helper function to filter output of utilization controller target
 */
FLCN_STATUS
s_perfCfControllerApplyOutputFilter
(
    PERF_CF_CONTROLLER *pController,
    LwBool              bInc
)
{
    FLCN_STATUS status = FLCN_OK;

    status = s_perfCfControllerApplyGainFilter(pController, bInc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfCfControllerApplyOutputFilter_exit;
    }

    status = s_perfCfControllerApplyHysteresisFilter(pController, bInc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfCfControllerApplyOutputFilter_exit;
    }

    status = s_perfCfControllerApplyIIRFilter(pController, bInc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfCfControllerApplyOutputFilter_exit;
    }

s_perfCfControllerApplyOutputFilter_exit:
    return status;
}

/*!
 * Helper function applies Gain Filter to output of utilization controller target
 */
FLCN_STATUS
s_perfCfControllerApplyGainFilter
(
    PERF_CF_CONTROLLER *pController,
    LwBool              bInc
)
{
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pController;
    FLCN_STATUS             status = FLCN_OK;
    LwU64                   tmp;
    LwU64                   clkFreqkHz;
    VPSTATE                *pVpstate;
    VPSTATE_3X_CLOCK_ENTRY  clockEntry;

    if (bInc) // Increase clocks
    {
        //
        // Target freq (after gain) = lwrrentkHz + (targetkHz - lwrrentkHz) * gainInc >> 12.
        // 64 bits can handle LwU32 * FXP20.12 >> 12.
        //
        clkFreqkHz = pControllerUtil->targetkHz - pControllerUtil->lwrrentkHz;
        tmp = pControllerUtil->gainInc;
        lwU64Mul(&clkFreqkHz, &clkFreqkHz, &tmp);
        lw64Lsr(&clkFreqkHz, &clkFreqkHz, 12);
        pControllerUtil->targetkHz = pControllerUtil->lwrrentkHz + (LwU32)clkFreqkHz;

        //
        // If delta clock frequency cannot fit into 32 bits, or current + delta
        // overflows 32 bits, target 32 bits max.
        //
        lw64Lsr(&clkFreqkHz, &clkFreqkHz, 32);
        if ((clkFreqkHz != 0) ||
            (pControllerUtil->targetkHz < pControllerUtil->lwrrentkHz))
        {
            pControllerUtil->targetkHz = LW_U32_MAX;
        }

        // Target at least 3D Boost when utilization exceeds jump threshold.
        if ((pControllerUtil->jumpThreshold != 0) &&
            (pControllerUtil->pct > pControllerUtil->jumpThreshold))
        {
            // It is OK if 3D Boost is not found.
            pVpstate = vpstateGetBySemanticIdx(RM_PMU_PERF_VPSTATE_IDX_3D_BOOST);
            if (pVpstate != NULL)
            {
                // It is OK if the clock domain is not specified for 3D Boost.
                status = vpstate3xClkDomainGet((VPSTATE_3X *)pVpstate, pControllerUtil->clkDomIdx, &clockEntry);
                if (status == FLCN_OK)
                {
                    pControllerUtil->targetkHz = LW_MAX(
                        pControllerUtil->targetkHz, clockEntry.targetFreqMHz * 1000);
                }
                // explictly override status to FLCN_OK
                status = FLCN_OK;
            }
        }                
    }
    else // Decrease clocks        
    {
        //
        // Target freq (after gain) = lwrrentkHz - (lwrrentkHz - targetkHz) * gainDec >> 12.
        // 64 bits can handle LwU32 * FXP20.12 >> 12.
        //
        clkFreqkHz = pControllerUtil->lwrrentkHz - pControllerUtil->targetkHz;
        tmp = pControllerUtil->gainDec;
        lwU64Mul(&clkFreqkHz, &clkFreqkHz, &tmp);
        lw64Lsr(&clkFreqkHz, &clkFreqkHz, 12);
        pControllerUtil->targetkHz = pControllerUtil->lwrrentkHz - (LwU32)clkFreqkHz;

        // If delta clock frequency is larger than current, target 0.
        if ((LwU32)clkFreqkHz > pControllerUtil->lwrrentkHz)
        {
            pControllerUtil->targetkHz = 0;
        }

        // If delta clock frequency cannot fit into 32 bits, target 0.
        lw64Lsr(&clkFreqkHz, &clkFreqkHz, 32);
        if (clkFreqkHz != 0)
        {
            pControllerUtil->targetkHz = 0;
        }
    }

    return FLCN_OK;
}

/*!
 * Helper function applies Hysteresis Filter to output of utilization controller target
 */
FLCN_STATUS
s_perfCfControllerApplyHysteresisFilter
(
    PERF_CF_CONTROLLER *pController,
    LwBool              bInc
)
{
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pController;    
    LwU8 hysteresisCount;

    // Select the correct hysteresis count.
    hysteresisCount = bInc ? pControllerUtil->hysteresisCountInc : pControllerUtil->hysteresisCountDec;

    // Increment when util is consistently above/below target, otherwise this is the first sample.
    if (!pControllerUtil->bIncLast == !bInc)
    {
        if (pControllerUtil->hysteresisCountLwrr < LW_U8_MAX)
        {
            pControllerUtil->hysteresisCountLwrr++;
        }
    }
    else
    {
        pControllerUtil->hysteresisCountLwrr = 1;
        pControllerUtil->bIncLast = bInc;
    }

    // Set limit only when hysteresis is met.
    if (pControllerUtil->hysteresisCountLwrr >= hysteresisCount)
    {
        pControllerUtil->avgTargetkHz = pControllerUtil->targetkHz;
        pController->limitFreqkHz[pControllerUtil->limitIdx] = pControllerUtil->avgTargetkHz;
    }

    return FLCN_OK;
}

/*!
 * Helper function applies IIR Filter to output of utilization controller target
 */
FLCN_STATUS
s_perfCfControllerApplyIIRFilter
(
    PERF_CF_CONTROLLER *pController,
    LwBool              bInc
)
{
    // TBD - return OK
    return FLCN_OK;
}

