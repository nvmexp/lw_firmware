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
 * @file    nne_var_pm.c
 * @brief   NNE_VAR_PM class implementation.
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
 * @memberof NNE_VAR_PM
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneVarGrpIfaceModel10ObjSet_PM
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_VAR_PM            *pDescVar =
        (RM_PMU_NNE_VAR_PM *)pBoardObjDesc;
    NNE_VAR_PM                   *pNneVar;
    LW2080_CTRL_NNE_NNE_VAR_INPUT input;
    LwU16                         mapArrIdx;
    FLCN_STATUS                   status;

    // Sanity check that BA PM signal index is less than the supported max.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDescVar->varPmId.baIdx < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_PM)
            ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarGrpIfaceModel10ObjSet_PM_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneVarGrpIfaceModel10ObjSet_PM_exit);

    pNneVar = (NNE_VAR_PM *)*ppBoardObj;

    // Set member variables.
    pNneVar->varPmId = pDescVar->varPmId;
    pNneVar->secNorm = lwF32MapFromU32(&pDescVar->secNorm);

    // Insert this object into the NNE_VARS_VAR_ID_MAP.
    input.type = LW2080_CTRL_NNE_NNE_VAR_TYPE_PM;
    input.data.pm.pmId = pNneVar->varPmId;
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarsVarIdMapArrIdxGet_PM(&input, &mapArrIdx),
        nneVarGrpIfaceModel10ObjSet_PM_exit);
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_SET_IDX(status, NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
        PM, mapArrIdx, BOARDOBJ_GET_GRP_IDX(&pNneVar->super.super),
        nneVarGrpIfaceModel10ObjSet_PM_exit);

nneVarGrpIfaceModel10ObjSet_PM_exit:
    return status;
}

/*!
 * @copydoc NneVarInputIDMatch
 */
FLCN_STATUS
nneVarInputIDMatch_PM
(
    NNE_VAR                         *pVar,
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInput,
    LwBool                          *pBMatch
)
{
    NNE_VAR_PM   *pVarPm = (NNE_VAR_PM *)pVar;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if ((pVarPm  == NULL) ||
        (pInput  == NULL) ||
        (pBMatch == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarInputIDMatch_PM_exit;
    }

    *pBMatch = (pVarPm->varPmId.baIdx == pInput->data.pm.pmId.baIdx);

nneVarInputIDMatch_PM_exit:
    return status;
}

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 *
 * @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_PM implementation.
 *
 * @note This implementation must be kept in sync with @ref
 * NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_PM and @ref NNE_VAR_PM_CLK_DOMAIN_IDX_MAX
 */
FLCN_STATUS
nneVarsVarIdMapArrIdxGet_PM
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT  *pInput,
    LwU16                          *pMapArrIdx
)
{
    LW2080_CTRL_NNE_VAR_ID_PM *pPmId;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (((pInput != NULL) && (pMapArrIdx != NULL) &&
                (pInput->type == LW2080_CTRL_NNE_NNE_VAR_TYPE_PM)) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_PM_exit);
    pPmId = &pInput->data.pm.pmId;

    //
    // Final sanity check that the result won't hit a buffer over-run,
    // then copy out the MAP_ARR index.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPmId->baIdx < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_PM) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_PM_exit);
    *pMapArrIdx = pPmId->baIdx;

nneVarsVarIdMapArrIdxGet_PM_exit:
    return status;
}

/*!
 * @copydoc NneVarLwF32Get
 */
FLCN_STATUS
nneVarLwF32Get_PM
(
    const LW2080_CTRL_NNE_NNE_VAR_INPUT    *pInput,
    LwF32                                  *pVarValue
)
{
    LwU64       value;
    FLCN_STATUS status      = FLCN_OK;

    if ((pInput    == NULL) ||
        (pVarValue == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarLwF32Get_PM_exit;
    }

    LwU64_ALIGN32_UNPACK(&value, &pInput->data.pm.pmCount);
    *pVarValue = lwF32ColwertFromU64(value);

nneVarLwF32Get_PM_exit:
    return status;
}

/*!
 * @copydoc NneVarInputNormalizationGet
 */
FLCN_STATUS
nneVarInputNormalizationGet_PM
(
    const NNE_VAR *pVar,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput,
    LwF32 *pNormalization
)
{
    FLCN_STATUS status;
    const NNE_VAR_PM *const pVarPm = (NNE_VAR_PM *)pVar;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pVar != NULL) &&
        (pInput != NULL) && 
        (pNormalization != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneVarInputNormalizationGet_PM_exit);

    if (lwF32CmpEQ(pVarPm->secNorm, 0.0F))
    {
        *pNormalization = 1.0F;
    }
    else
    {
        LwF32 value;
        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarLwF32Get_PM(pInput, &value),
            nneVarInputNormalizationGet_PM_exit);

        *pNormalization = lwF32Div(value, pVarPm->secNorm);
    }

nneVarInputNormalizationGet_PM_exit:
    return status;
}

/*!
 * @copydoc NneVarInputNormalize
 */
FLCN_STATUS
nneVarInputNormalize_PM
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput,
    LwF32 norm 
)
{
    FLCN_STATUS status;
    LwF32 value;
    LwU64 scaledData;

    PMU_ASSERT_OK_OR_GOTO(status,
        pInput != NULL ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneVarInputNormalize_PM_exit);

    // Get the original value
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarLwF32Get_PM(pInput, &value),
        nneVarInputNormalize_PM_exit);

    // Normalize
    value = lwF32Div(value, norm);

    // Colwert the F32 value into a LwU64
    scaledData = lwF32ColwertToU64(value);

    // Update the input.
    LwU64_ALIGN32_PACK(&pInput->data.pm.pmCount, &scaledData);

nneVarInputNormalize_PM_exit:
    return status;
}

/* ----- Private Functions ------------------------------- */
