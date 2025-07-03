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
 * @file    nne_var_power_total.c
 *
 * @copydoc nne_var_power_dn.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * Sanity check that max number of @ref
 * LW2080_CTRL_NNE_VAR_ID_POWER_TOTAL_VOLT_RAIL_NAME_MAX can fit within
 * space allocated within @ref NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_TOTAL.
 */
ct_assert(LW2080_CTRL_NNE_VAR_ID_POWER_TOTAL_VOLT_RAIL_NAME_MAX <= NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_TOTAL);

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @memberof NNE_VAR_POWER_TOTAL
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneVarGrpIfaceModel10ObjSet_POWER_TOTAL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_VAR_POWER_TOTAL   *pDescVarPowerTotal =
        (RM_PMU_NNE_VAR_POWER_TOTAL *)pBoardObjDesc;
    NNE_VAR_POWER_TOTAL          *pVarPowerTotal;
    LW2080_CTRL_NNE_NNE_VAR_INPUT input;
    LwU16                         mapArrIdx;
    FLCN_STATUS                   status;

    // Sanity check that VOLT_RAIL index is less than the supported max.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDescVarPowerTotal->id.voltRailName < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_TOTAL)
            ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarGrpIfaceModel10ObjSet_POWER_TOTAL_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneVarGrpIfaceModel10ObjSet_POWER_TOTAL_exit);

    pVarPowerTotal = (NNE_VAR_POWER_TOTAL *)*ppBoardObj;

    // Set member variables.
    pVarPowerTotal->id = pDescVarPowerTotal->id;

    // Insert this object into the NNE_VARS_VAR_ID_MAP.
    input.type = LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL;
    input.data.powerTotal.id = pVarPowerTotal->id;
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarsVarIdMapArrIdxGet_POWER_TOTAL(&input, &mapArrIdx),
        nneVarGrpIfaceModel10ObjSet_POWER_TOTAL_exit);
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_SET_IDX(status, NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
        POWER_TOTAL, mapArrIdx, BOARDOBJ_GET_GRP_IDX(&pVarPowerTotal->super.super),
        nneVarGrpIfaceModel10ObjSet_POWER_TOTAL_exit);

nneVarGrpIfaceModel10ObjSet_POWER_TOTAL_exit:
    return status;
}

/*!
 * @copydoc NneVarInputIDMatch
 */
FLCN_STATUS
nneVarInputIDMatch_POWER_TOTAL
(
    NNE_VAR                         *pVar,
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInput,
    LwBool                          *pBMatch
)
{
    NNE_VAR_POWER_TOTAL  *pVarPowerTotal = (NNE_VAR_POWER_TOTAL *)pVar;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if ((pVarPowerTotal  == NULL) ||
        (pInput  == NULL) ||
        (pBMatch == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarInputIDMatch_POWER_TOTAL_exit;
    }

    *pBMatch = (pVarPowerTotal->id.voltRailName == pInput->data.powerTotal.id.voltRailName);

nneVarInputIDMatch_POWER_TOTAL_exit:
    return status;
}

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 *
 * @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL implementation.
 *
 * @note This implementation must be kept in sync with @ref
 * NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_TOTAL.
 */
FLCN_STATUS
nneVarsVarIdMapArrIdxGet_POWER_TOTAL
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT  *pInput,
    LwU16                          *pMapArrIdx
)
{
    LW2080_CTRL_NNE_VAR_ID_POWER_TOTAL *pPowerTotalId;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (((pInput != NULL) && (pMapArrIdx != NULL) &&
                (pInput->type == LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL)) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_POWER_TOTAL_exit);
    pPowerTotalId = &pInput->data.powerTotal.id;

    //
    // Sanity check that the requested VOLT_RAIL index is less than the
    // supported max.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPowerTotalId->voltRailName < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_TOTAL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_POWER_TOTAL_exit);
    *pMapArrIdx = pPowerTotalId->voltRailName;

nneVarsVarIdMapArrIdxGet_POWER_TOTAL_exit:
    return status;
}

/*!
 * @copydoc NneVarLwF32Get
 */
FLCN_STATUS
nneVarLwF32Get_POWER_TOTAL
(
    const LW2080_CTRL_NNE_NNE_VAR_INPUT    *pInput,
    LwF32                                  *pVarValue
)
{
    FLCN_STATUS status      = FLCN_OK;

    if ((pInput    == NULL) ||
        (pVarValue == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarLwF32Get_POWER_TOTAL_exit;
    }

    *pVarValue = lwF32ColwertFromU32(pInput->data.powerTotal.powermW);

nneVarLwF32Get_POWER_TOTAL_exit:
    return status;
}

/* ----- Private Functions ------------------------------- */
