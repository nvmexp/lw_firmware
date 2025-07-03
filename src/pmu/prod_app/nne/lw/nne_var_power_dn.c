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
 * @file    nne_var_power_dn.c
 *
 * @copydoc nne_var_power_dn.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @memberof NNE_VAR_POWER_DN
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneVarGrpIfaceModel10ObjSet_POWER_DN
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_VAR_POWER_DN      *pDescVarPowerDn =
        (RM_PMU_NNE_VAR_POWER_DN *)pBoardObjDesc;
    NNE_VAR_POWER_DN             *pVarPowerDn;
    LW2080_CTRL_NNE_NNE_VAR_INPUT input;
    LwU16                         mapArrIdx;
    FLCN_STATUS                   status;

    // Sanity check that VOLT_RAIL index is less than the supported max.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDescVarPowerDn->id.voltRailIdx < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_DN)
            ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarGrpIfaceModel10ObjSet_POWER_DN_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneVarGrpIfaceModel10ObjSet_POWER_DN_exit);

    pVarPowerDn = (NNE_VAR_POWER_DN *)*ppBoardObj;

    //
    // Until voltage scaling is implemented in @ref
    // nneVarLwF32Get_POWER_DN(), can only support 1.0V for now (i.e
    // no scaling required).
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDescVarPowerDn->voltageuV == 1000000) ?
            FLCN_OK : FLCN_ERR_ILWALID_STATE),
        nneVarGrpIfaceModel10ObjSet_POWER_DN_exit);

    // Set member variables.
    pVarPowerDn->id        = pDescVarPowerDn->id;
    pVarPowerDn->voltageuV = pDescVarPowerDn->voltageuV;

    // Insert this object into the NNE_VARS_VAR_ID_MAP.
    input.type = LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN;
    input.data.powerDn.id = pVarPowerDn->id;
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarsVarIdMapArrIdxGet_POWER_DN(&input, &mapArrIdx),
        nneVarGrpIfaceModel10ObjSet_POWER_DN_exit);
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_SET_IDX(status, NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
        POWER_DN, mapArrIdx, BOARDOBJ_GET_GRP_IDX(&pVarPowerDn->super.super),
        nneVarGrpIfaceModel10ObjSet_POWER_DN_exit);

nneVarGrpIfaceModel10ObjSet_POWER_DN_exit:
    return status;
}

/*!
 * @copydoc NneVarInputIDMatch
 */
FLCN_STATUS
nneVarInputIDMatch_POWER_DN
(
    NNE_VAR                         *pVar,
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInput,
    LwBool                          *pBMatch
)
{
    NNE_VAR_POWER_DN *pVarPowerDn = (NNE_VAR_POWER_DN *)pVar;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if ((pVarPowerDn  == NULL) ||
        (pInput  == NULL) ||
        (pBMatch == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarInputIDMatch_POWER_DN_exit;
    }

    *pBMatch = (pVarPowerDn->id.voltRailIdx == pInput->data.powerDn.id.voltRailIdx);

nneVarInputIDMatch_POWER_DN_exit:
    return status;
}

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 *
 * @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN implementation.
 *
 * @note This implementation must be kept in sync with @ref
 * NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_DN.
 */
FLCN_STATUS
nneVarsVarIdMapArrIdxGet_POWER_DN
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT  *pInput,
    LwU16                          *pMapArrIdx
)
{
    LW2080_CTRL_NNE_VAR_ID_POWER_DN *pPowerDnId;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (((pInput != NULL) && (pMapArrIdx != NULL) &&
                (pInput->type == LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN)) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_POWER_DN_exit);
    pPowerDnId = &pInput->data.powerDn.id;

    //
    // Sanity check that the requested VOLT_RAIL index is less than the
    // supported max.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPowerDnId->voltRailIdx < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_DN) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_POWER_DN_exit);
    *pMapArrIdx = pPowerDnId->voltRailIdx;

nneVarsVarIdMapArrIdxGet_POWER_DN_exit:
    return status;
}

/*!
 * @copydoc NneVarLwF32Get
 */
FLCN_STATUS
nneVarLwF32Get_POWER_DN
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
        goto nneVarLwF32Get_POWER_DN_exit;
    }

    //
    // CRPTODO - Do scaling of input voltage from 1.0V to target
    // voltage.  It needs a NNE_VAR_POWER_DN pointer, which this
    // interface does not receive.
    //
    *pVarValue = lwF32ColwertFromU32(pInput->data.powerDn.powermW);

nneVarLwF32Get_POWER_DN_exit:
    return status;
}

/* ----- Private Functions ------------------------------- */
