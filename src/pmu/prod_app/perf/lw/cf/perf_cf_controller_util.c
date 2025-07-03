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
 * @file    perf_cf_controller_util.c
 * @copydoc perf_cf_controller_util.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller_util.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfCfControllerExelwteHelper(PERF_CF_CONTROLLER *pController);

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CONTROLLER_UTIL *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER_UTIL *)pBoardObjDesc;
    PERF_CF_CONTROLLER_UTIL    *pControllerUtil;
    LwBool                      bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                 status          = FLCN_OK;

    status = perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }
    pControllerUtil = (PERF_CF_CONTROLLER_UTIL *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pControllerUtil->clkDomIdx      != pDescController->clkDomIdx)      ||
            (pControllerUtil->clkTopologyIdx != pDescController->clkTopologyIdx) ||
            (pControllerUtil->pgTopologyIdx  != pDescController->pgTopologyIdx)  ||
            (pControllerUtil->bDoNotProgram  != pDescController->bDoNotProgram)  ||
            (pControllerUtil->bUtilThresholdScaleByVMCountSupported !=
             pDescController->bUtilThresholdScaleByVMCountSupported))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
        }
    }

    // Set member variables.
    pControllerUtil->target                                = pDescController->target;
    pControllerUtil->jumpThreshold                         = pDescController->jumpThreshold;
    pControllerUtil->gainInc                               = pDescController->gainInc;
    pControllerUtil->gainDec                               = pDescController->gainDec;
    pControllerUtil->clkDomIdx                             = pDescController->clkDomIdx;
    pControllerUtil->clkTopologyIdx                        = pDescController->clkTopologyIdx;
    pControllerUtil->hysteresisCountInc                    = pDescController->hysteresisCountInc;
    pControllerUtil->hysteresisCountDec                    = pDescController->hysteresisCountDec;
    pControllerUtil->pgTopologyIdx                         = pDescController->pgTopologyIdx;
    pControllerUtil->bDoNotProgram                         = pDescController->bDoNotProgram;
    pControllerUtil->bUtilThresholdScaleByVMCountSupported = pDescController->bUtilThresholdScaleByVMCountSupported;

    // Topology index is required for utilization controller.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_TOPOLOGY_IDX_NONE ==
            pControllerUtil->super.topologyIdx)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if ((pControllerUtil->target == 0) ||
        (pControllerUtil->target > LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if (pControllerUtil->jumpThreshold > LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if (pControllerUtil->gainInc == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if (pControllerUtil->gainDec == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if (!BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY, pControllerUtil->clkTopologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if (pControllerUtil->hysteresisCountInc == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if (pControllerUtil->hysteresisCountDec == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

    if ((LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_UTIL_PG_TOPOLOGY_IDX_NONE !=
            pControllerUtil->pgTopologyIdx) &&
        !BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY, pControllerUtil->pgTopologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit;
    }

perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_UTIL
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_UTIL_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_CONTROLLER_UTIL_GET_STATUS *)pPayload;
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfControllerIfaceModel10GetStatus_UTIL_exit;
    }

    pStatus->pct                    = pControllerUtil->pct;
    pStatus->lwrrentkHz             = pControllerUtil->lwrrentkHz;
    pStatus->targetkHz              = pControllerUtil->targetkHz;
    pStatus->avgTargetkHz           = pControllerUtil->avgTargetkHz;
    pStatus->hysteresisCountLwrr    = pControllerUtil->hysteresisCountLwrr;
    pStatus->limitIdx               = pControllerUtil->limitIdx;
    pStatus->bIncLast               = pControllerUtil->bIncLast;

perfCfControllerIfaceModel10GetStatus_UTIL_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_UTIL
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pController;
    LwU64                   clkFreqkHz;
    LwU64                   mulGHz2kHz      = 1000000;
    LwU64                   tmp;
    LwU64                   oneFxp24;
    LwU64                   pgPct;
    PERF_CF_CONTROLLERS    *pControllers    = PERF_CF_GET_CONTROLLERS();
    FLCN_STATUS             status          = FLCN_OK;

    status = perfCfControllerExelwte_SUPER(pController);
    if (status != FLCN_OK)
    {
        if (status == FLCN_ERR_STATE_RESET_NEEDED)
        {
            pControllerUtil->pct                    = 0;
            pControllerUtil->lwrrentkHz             = 0;
            pControllerUtil->targetkHz              = 0;
            pControllerUtil->avgTargetkHz           = 0;
            pControllerUtil->hysteresisCountLwrr    = 0;
            pControllerUtil->bIncLast               = LW_FALSE;

            // State reset done. Skip this cycle below.
            status = FLCN_OK;
        }

        goto perfCfControllerExelwte_UTIL_exit;
    }

    //
    // Reading clock frequency and colwert from FXP40.24 in GHz to U32 in kHz.
    // FXP40.24 can handle max 1PHz * 1000000 >> 24. U32 can handle max 4THz.
    //

    LwU64_ALIGN32_UNPACK(&tmp,
        &pController->pMultData->topologysStatus.topologys[pControllerUtil->clkTopologyIdx].topology.reading);
    lwU64Mul(&tmp, &tmp, &mulGHz2kHz);

    // Divide kHz in FXP40.24 by 1 in FXP40.24 would result in kHz.
    oneFxp24 = 1;
    lw64Lsl(&oneFxp24, &oneFxp24, 24);

    // Divide by (1 - PG%) instead of 1 to account for clock being gated.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_UTIL_PG_TOPOLOGY_IDX_NONE !=
            pControllerUtil->pgTopologyIdx)
    {
        LwU64_ALIGN32_UNPACK(&pgPct,
            &pController->pMultData->topologysStatus.topologys[pControllerUtil->pgTopologyIdx].topology.reading);

        //
        // We dont' want to divide by 0 or divide by a negative (very large
        // unsigned) number. If MSCG is close to 100%, then we have 0%
        // utilization and current frequency won't matter much.
        //
        if (lwU64CmpGT(&oneFxp24, &pgPct))
        {
            lw64Sub(&oneFxp24, &oneFxp24, &pgPct);
        }
    }

    lwU64Div(&tmp, &tmp, &oneFxp24);
    pControllerUtil->lwrrentkHz = (LwU32)tmp;

    // If current clock frequency cannot fit into 32 bits, use 32 bits max.
    lw64Lsr(&tmp, &tmp, 32);
    if (tmp != 0)
    {
        pControllerUtil->lwrrentkHz = LW_U32_MAX;
    }

    // Reading utilization and colwert from FXP40.24 to FXP20.12. Max utilization is 1.
    LwU64_ALIGN32_UNPACK(&tmp,
        &pController->pMultData->topologysStatus.topologys[pController->topologyIdx].topology.reading);
    lw64Lsr(&tmp, &tmp, 12);
    pControllerUtil->pct = (LwUFXP20_12)tmp;

    //
    // If feature of scaling the utilization threshold in proportion with number of VM slots
    // that are active in vGPU's scheduler is *supported* then:
    // Target freq (before gain) = lwrrentkHz * pct * VMSlotsNumber / target
    //
    // If the feature is *not supported* then
    // Target freq (before gain) = lwrrentkHz * pct / target.
    //
    // 64 bits can handle LwU32 * FXP20.12 / FXP20.12.
    //
    clkFreqkHz = pControllerUtil->lwrrentkHz;
    tmp = pControllerUtil->pct;
    lwU64Mul(&clkFreqkHz, &clkFreqkHz, &tmp);
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_THRESHOLD_SCALE_BY_VM_COUNT) &&
        (pControllerUtil->bUtilThresholdScaleByVMCountSupported)))
    {
        tmp = pControllers->maxVGpuVMCount;
        lwU64Mul(&clkFreqkHz, &clkFreqkHz, &tmp);
    }
    tmp = pControllerUtil->target;
    lwU64Div(&clkFreqkHz, &clkFreqkHz, &tmp);
    pControllerUtil->targetkHz = (LwU32)clkFreqkHz;

    // If target clock frequency cannot fit into 32 bits, target 32 bits max.
    lw64Lsr(&clkFreqkHz, &clkFreqkHz, 32);
    if (clkFreqkHz != 0)
    {
        pControllerUtil->targetkHz = LW_U32_MAX;
    }

    // Utilization Controller 1X specific processing
    if (BOARDOBJ_GET_TYPE(pController) == LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL)
    {
        status = s_perfCfControllerExelwteHelper(pController);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfCfControllerExelwte_UTIL_exit;
        }
    }

perfCfControllerExelwte_UTIL_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
FLCN_STATUS
perfCfControllerSetMultData_UTIL
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pController;
    FLCN_STATUS status = FLCN_OK;

    // Set the clock frequency topology the UTIL controller needs.
    boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask),
        pControllerUtil->clkTopologyIdx);

    // Set the power gating % topology the UTIL controller needs.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_UTIL_PG_TOPOLOGY_IDX_NONE !=
            pControllerUtil->pgTopologyIdx)
    {
        boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask),
            pControllerUtil->pgTopologyIdx);
    }

    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
FLCN_STATUS
perfCfControllerLoad_UTIL
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_UTIL    *pControllerUtil = (PERF_CF_CONTROLLER_UTIL *)pController;
    FLCN_STATUS                 status          = FLCN_OK;

    // Validate clock domain index. All boardobjgrps have been constructed at load.
    if (!BOARDOBJGRP_IS_VALID(CLK_DOMAIN, pControllerUtil->clkDomIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerLoad_UTIL_exit;
    }

    pControllerUtil->limitIdx = perfCfControllersGetMinLimitIdxFromClkDomIdx(
        PERF_CF_GET_CONTROLLERS(), pControllerUtil->clkDomIdx);
    if (PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID == pControllerUtil->limitIdx)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto perfCfControllerLoad_UTIL_exit;
    }

perfCfControllerLoad_UTIL_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
FLCN_STATUS
s_perfCfControllerExelwteHelper
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_UTIL *pControllerUtil =
        (PERF_CF_CONTROLLER_UTIL *)pController;
    LwBool                  bInc;
    LwU64                   clkFreqkHz;
    LwU64                   tmp;
    LwU8                    hysteresisCount;
    VPSTATE                *pVpstate;
    VPSTATE_3X_CLOCK_ENTRY  clockEntry;
    PERF_CF_CONTROLLERS    *pControllers    = PERF_CF_GET_CONTROLLERS();
    FLCN_STATUS             status          = FLCN_OK;

    //
    // If feature of scaling the utilization threshold in proportion with number of VM slots
    // that are active in vGPU's scheduler is *supported* then the comparsion needs to be with
    // the scaled threshold.
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_THRESHOLD_SCALE_BY_VM_COUNT) &&
        (pControllerUtil->bUtilThresholdScaleByVMCountSupported)))
    {
        bInc = pControllerUtil->pct > (pControllerUtil->target / pControllers->maxVGpuVMCount);
    }
    else
    {
        bInc = pControllerUtil->pct > pControllerUtil->target;
    }

    // Do we want to increase clocks or decrease?
    if (bInc)
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
                // Skip Vpstate type check. PERF_CF depends on PS3.5+.
                //if (LW2080_CTRL_PERF_VPSTATE_TYPE_3X != BOARDOBJ_GET_TYPE(&(pVpstate->super)))
                //{
                //    PMU_BREAKPOINT();
                //    status = FLCN_ERR_ILWALID_STATE;
                //    goto perfCfControllerExelwte_UTIL_exit;
                //}

                // It is OK if the clock domain is not specified for 3D Boost.
                status = vpstate3xClkDomainGet((VPSTATE_3X *)pVpstate, pControllerUtil->clkDomIdx, &clockEntry);
                if (status == FLCN_OK)
                {
                    pControllerUtil->targetkHz = LW_MAX(
                        pControllerUtil->targetkHz, clockEntry.targetFreqMHz * 1000);
                }
                status = FLCN_OK;
            }
        }
    }
    else
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

    if (1 == pControllerUtil->hysteresisCountLwrr)
    {
        // Initialize average to first sample.
        pControllerUtil->avgTargetkHz = pControllerUtil->targetkHz;
    }
    else
    {
        if (!bInc && (pControllerUtil->avgTargetkHz < pControllerUtil->targetkHz))
        {
            // Re-position average to high when decreasing.
            pControllerUtil->avgTargetkHz = pControllerUtil->targetkHz;
        }
        else
        {
            //
            // Rolling average = (avgTargetkHz * (hysteresisCount - 1) + targetkHz) / hysteresisCount.
            // 64 bits can handle (LwU32 * LwU8 + LwU32) / LwU8.
            // Average will always fit into 32 bits. hysteresisCount cannot be 0.
            //
            clkFreqkHz = pControllerUtil->avgTargetkHz;
            tmp = LW_MIN(pControllerUtil->hysteresisCountLwrr, hysteresisCount) - 1;
            lwU64Mul(&clkFreqkHz, &clkFreqkHz, &tmp);
            lw64Add32(&clkFreqkHz, pControllerUtil->targetkHz);
            tmp = LW_MIN(pControllerUtil->hysteresisCountLwrr, hysteresisCount);
            lwU64Div(&clkFreqkHz, &clkFreqkHz, &tmp);
            pControllerUtil->avgTargetkHz = (LwU32)clkFreqkHz;
        }
    }

    // Set limit only when hysteresis is met.
    if (pControllerUtil->hysteresisCountLwrr >= hysteresisCount)
    {
        pController->limitFreqkHz[pControllerUtil->limitIdx] = pControllerUtil->avgTargetkHz;
    }

    return status;
}
