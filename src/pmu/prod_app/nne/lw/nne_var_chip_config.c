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
 * @file    nne_var_chip_config.c
 * @brief   NNE_VAR_CHIP_CONFIG class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Sanity check that max number of @ref
 * LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE can fit within
 * space allocated within @ref NNE_VARS_VAR_ID_MAP.
 */
ct_assert(LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_NUM <= NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_CHIP_CONFIG);

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @memberof NNE_VAR_CHIP_CONFIG
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_VAR_CHIP_CONFIG   *pDescVar =
        (RM_PMU_NNE_VAR_CHIP_CONFIG *)pBoardObjDesc;
    NNE_VAR_CHIP_CONFIG          *pNneVar;
    LW2080_CTRL_NNE_NNE_VAR_INPUT input;
    LwU16                         mapArrIdx;
    FLCN_STATUS                   status;

    // Sanity check that the CLK_DOMAIN index is less than the supported max.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDescVar->varConfigId.configType <
                LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_NUM) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG_exit);

    pNneVar = (NNE_VAR_CHIP_CONFIG *)*ppBoardObj;

    // Set member variables.
    pNneVar->varConfigId = pDescVar->varConfigId;

    // Insert this object into the NNE_VARS_VAR_ID_MAP.
    input.type = LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG;
    input.data.config.configId = pNneVar->varConfigId;
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarsVarIdMapArrIdxGet_CHIP_CONFIG(&input, &mapArrIdx),
        nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG_exit);
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_SET_IDX(status, NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
        CHIP_CONFIG, mapArrIdx, BOARDOBJ_GET_GRP_IDX(&pNneVar->super.super),
        nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG_exit);

nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG_exit:
    return status;
}

/*!
 * @copydoc NneVarInputIDMatch
 */
FLCN_STATUS
nneVarInputIDMatch_CHIP_CONFIG
(
    NNE_VAR                         *pVar,
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInput,
    LwBool                          *pBMatch
)
{
    NNE_VAR_CHIP_CONFIG   *pVarConfig = (NNE_VAR_CHIP_CONFIG *)pVar;
    FLCN_STATUS            status     = FLCN_OK;

    // Sanity checking.
    if ((pVarConfig == NULL) ||
        (pInput     == NULL) ||
        (pBMatch    == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarInputIDMatch_CHIP_CONFIG_exit;
    }

    *pBMatch = (pVarConfig->varConfigId.configType == pInput->data.config.configId.configType);

nneVarInputIDMatch_CHIP_CONFIG_exit:
    return status;
}

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 *
 * @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG implementation.
 *
 * @note This implementation must be kept in sync with @ref
 * NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_CHIP_CONFIG.
 */
FLCN_STATUS
nneVarsVarIdMapArrIdxGet_CHIP_CONFIG
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT  *pInput,
    LwU16                          *pMapArrIdx
)
{
    LW2080_CTRL_NNE_VAR_ID_CHIP_CONFIG *pConfigId;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (((pInput != NULL) && (pMapArrIdx != NULL) &&
                (pInput->type == LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG)) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_CHIP_CONFIG_exit);
    pConfigId = &pInput->data.config.configId;

    //
    // Sanity check that the requested CONFIG_TYPE is less than the
    // supported max.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pConfigId->configType < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_CHIP_CONFIG) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_CHIP_CONFIG_exit);
    *pMapArrIdx = pConfigId->configType;

nneVarsVarIdMapArrIdxGet_CHIP_CONFIG_exit:
    return status;
}

/*!
 * @copydoc NneVarLwF32Get
 */
FLCN_STATUS
nneVarLwF32Get_CHIP_CONFIG
(
    const LW2080_CTRL_NNE_NNE_VAR_INPUT    *pInput,
    LwF32                                  *pVarValue
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if ((pInput    == NULL) ||
        (pVarValue == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarLwF32Get_CHIP_CONFIG_exit;
    }

    *pVarValue = lwF32ColwertFromU32(pInput->data.config.config);

nneVarLwF32Get_CHIP_CONFIG_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
