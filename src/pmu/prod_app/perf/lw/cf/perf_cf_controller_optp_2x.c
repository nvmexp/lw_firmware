/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_optp_2x.c
 * @copydoc perf_cf_controller_optp_2x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller_optp_2x.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CONTROLLER_OPTP_2X *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER_OPTP_2X *)pBoardObjDesc;
    PERF_CF_CONTROLLER_OPTP_2X *pControllerOptp2x;
    LwBool                      bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                 status          = FLCN_OK;

    status = perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit;
    }
    pControllerOptp2x = (PERF_CF_CONTROLLER_OPTP_2X *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pControllerOptp2x->freqFloorkHz      != pDescController->freqFloorkHz)      ||
            (pControllerOptp2x->grClkTopologyIdx  != pDescController->grClkTopologyIdx)  ||
            (pControllerOptp2x->vidClkTopologyIdx != pDescController->vidClkTopologyIdx) ||
            (pControllerOptp2x->pgTopologyIdx     != pDescController->pgTopologyIdx))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit;
        }
    }

    // Set member variables.
    pControllerOptp2x->freqFloorkHz      = pDescController->freqFloorkHz;
    pControllerOptp2x->highThreshold     = pDescController->highThreshold;
    pControllerOptp2x->lowThreshold      = pDescController->lowThreshold;
    pControllerOptp2x->grClkTopologyIdx  = pDescController->grClkTopologyIdx;
    pControllerOptp2x->vidClkTopologyIdx = pDescController->vidClkTopologyIdx;
    pControllerOptp2x->pgTopologyIdx     = pDescController->pgTopologyIdx;

    // OPTP 2.x controller does not use the common topology index.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_TOPOLOGY_IDX_NONE !=
            pControllerOptp2x->super.topologyIdx)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit;
    }

    if (!BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY,
            pControllerOptp2x->grClkTopologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit;
    }

    if (!BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY,
            pControllerOptp2x->vidClkTopologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit;
    }

    if ((LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_OPTP_2X_PG_TOPOLOGY_IDX_NONE !=
            pControllerOptp2x->pgTopologyIdx) &&
        !BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY, pControllerOptp2x->pgTopologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit;
    }

perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_OPTP_2X
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_OPTP_2X_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_CONTROLLER_OPTP_2X_GET_STATUS *)pPayload;
    PERF_CF_CONTROLLER_OPTP_2X *pControllerOptp2x =
        (PERF_CF_CONTROLLER_OPTP_2X *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfControllerIfaceModel10GetStatus_OPTP_2X_exit;
    }

    pStatus->perfTarget = pControllerOptp2x->perfTarget;
    pStatus->grkHz      = pControllerOptp2x->grkHz;
    pStatus->vidkHz     = pControllerOptp2x->vidkHz;

perfCfControllerIfaceModel10GetStatus_OPTP_2X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_OPTP_2X
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLERS *pControllers =
        PERF_CF_GET_CONTROLLERS();
    PERF_CF_CONTROLLER_OPTP_2X *pControllerOptp2x =
        (PERF_CF_CONTROLLER_OPTP_2X *)pController;
    LwU64       clkFreqkHz;
    LwU64       mulGHz2kHz  = 1000000;
    LwU64       tmp;
    LwU64       oneFxp24;
    LwU64       pgPct;
    FLCN_STATUS status      = FLCN_OK;

    status = perfCfControllerExelwte_SUPER(pController);
    if (status != FLCN_OK)
    {
        if (status == FLCN_ERR_STATE_RESET_NEEDED)
        {
            // No state to reset.
            status = FLCN_OK;
        }
        goto perfCfControllerExelwte_OPTP_2X_exit;
    }

    //
    // Reading GR clock frequency and colwert from FXP40.24 in GHz to U32 in kHz.
    // FXP40.24 can handle max 1PHz * 1000000 >> 24. U32 can handle max 4THz.
    //

    LwU64_ALIGN32_UNPACK(&tmp,
        &pController->pMultData->topologysStatus.topologys[pControllerOptp2x->grClkTopologyIdx].topology.reading);
    lwU64Mul(&tmp, &tmp, &mulGHz2kHz);

    // Divide kHz in FXP40.24 by 1 in FXP40.24 would result in kHz.
    oneFxp24 = 1;
    lw64Lsl(&oneFxp24, &oneFxp24, 24);

    // Divide by (1 - PG%) instead of 1 to account for clock being gated.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_OPTP_2X_PG_TOPOLOGY_IDX_NONE !=
            pControllerOptp2x->pgTopologyIdx)
    {
        LwU64_ALIGN32_UNPACK(&pgPct,
            &pController->pMultData->topologysStatus.topologys[pControllerOptp2x->pgTopologyIdx].topology.reading);

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
    pControllerOptp2x->grkHz = (LwU32)tmp;

    // If current clock frequency cannot fit into 32 bits, use 32 bits max.
    lw64Lsr(&tmp, &tmp, 32);
    if (tmp != 0)
    {
        pControllerOptp2x->grkHz = LW_U32_MAX;
    }

    //
    // Reading VID clock frequency and colwert from FXP40.24 in GHz to U32 in kHz.
    // FXP40.24 can handle max 1PHz * 1000000 >> 24. U32 can handle max 4THz.
    //

    LwU64_ALIGN32_UNPACK(&tmp,
        &pController->pMultData->topologysStatus.topologys[pControllerOptp2x->vidClkTopologyIdx].topology.reading);
    lwU64Mul(&tmp, &tmp, &mulGHz2kHz);

    lw64Lsr(&tmp, &tmp, 24);
    pControllerOptp2x->vidkHz = (LwU32)tmp;

    // Apply simple OPTP 2.0 algorithm to max of graphics and video clocks.

    pControllerOptp2x->perfTarget = pControllers->optpPerfRatio;
    clkFreqkHz = LW_MAX(pControllerOptp2x->grkHz, pControllerOptp2x->vidkHz);

    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_OPTP_PERF_RATIO_INACTIVE ==
            pControllerOptp2x->perfTarget)
    {
        // OPTP is not active.
        pController->limitFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MAX)] =
            pController->limitFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MAX)] =
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;
    }
    else if ((pControllerOptp2x->perfTarget > pControllerOptp2x->highThreshold) ||
             (pControllerOptp2x->perfTarget < pControllerOptp2x->lowThreshold) ||
             (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST ==
                pController->limitFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MAX)]))
    {
        //
        // Target freq = current freq * OPTP perf target.
        // Pre-scaled clock freq up to 16 GHz can fit into 32 bits after
        // scaling to max OPTP perf ratio (256).
        //
        tmp = pControllerOptp2x->perfTarget;
        lwU64Mul(&clkFreqkHz, &clkFreqkHz, &tmp);
        lw64Lsr(&clkFreqkHz, &clkFreqkHz, 24);
        pController->limitFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MAX)] =
            pController->limitFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MAX)] =
            LW_MAX((LwU32)clkFreqkHz, pControllerOptp2x->freqFloorkHz);
    }

perfCfControllerExelwte_OPTP_2X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
FLCN_STATUS
perfCfControllerSetMultData_OPTP_2X
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    PERF_CF_CONTROLLER_OPTP_2X *pControllerOptp2x =
        (PERF_CF_CONTROLLER_OPTP_2X *)pController;
    FLCN_STATUS status = FLCN_OK;

    // Set the clock frequency topologys the OPTP 2.x controller needs.
    boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask),
        pControllerOptp2x->grClkTopologyIdx);
    boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask),
        pControllerOptp2x->vidClkTopologyIdx);

    // Set the power gating % topology the OPTP 2.x controller needs.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_OPTP_2X_PG_TOPOLOGY_IDX_NONE !=
            pControllerOptp2x->pgTopologyIdx)
    {
        boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask),
            pControllerOptp2x->pgTopologyIdx);
    }

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
