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
 * @file    nne_var_freq.c
 * @brief   NNE_VAR_FREQ class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Maximum CLK_DOMAIN index which the NNE_VAR_FREQ class can support.
 * This value is tuned to the maximum CLK_DOMAIN index we expect to
 * need and can be increased if needed (with qualifying the DMEM
 * increases).
 *
 * For now, limiting to only support CLK_DOMAIN idx < 3:
 * 0 - GPCCLK
 * 1 - XBARCLK
 * 2 - DRAMCLK
 *
 * @note Must be kept in sync with @ref NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_FREQ
 */
#define NNE_VAR_FREQ_CLK_DOMAIN_IDX_MAX                                        3
ct_assert(NNE_VAR_FREQ_CLK_DOMAIN_IDX_MAX * 2 <= NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_FREQ);

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @memberof NNE_VAR_FREQ
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneVarGrpIfaceModel10ObjSet_FREQ
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_VAR_FREQ          *pDescVar =
        (RM_PMU_NNE_VAR_FREQ *)pBoardObjDesc;
    NNE_VAR_FREQ                 *pNneVar;
    LW2080_CTRL_NNE_NNE_VAR_INPUT input;
    LwU16                         mapArrIdx;
    FLCN_STATUS                   status;

    // Sanity check that the CLK_DOMAIN index is less than the supported max.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDescVar->varFreqId.clkDomainIdx < NNE_VAR_FREQ_CLK_DOMAIN_IDX_MAX) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarGrpIfaceModel10ObjSet_FREQ_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneVarGrpIfaceModel10ObjSet_FREQ_exit);

    pNneVar = (NNE_VAR_FREQ *)*ppBoardObj;

    // Set member variables.
    pNneVar->varFreqId = pDescVar->varFreqId;

    // Insert this object into the NNE_VARS_VAR_ID_MAP.
    input.type = LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ;
    input.data.freq.freqId = pNneVar->varFreqId;
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarsVarIdMapArrIdxGet_FREQ(&input, &mapArrIdx),
        nneVarGrpIfaceModel10ObjSet_FREQ_exit);
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_SET_IDX(status, NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
        FREQ, mapArrIdx, BOARDOBJ_GET_GRP_IDX(&pNneVar->super.super),
        nneVarGrpIfaceModel10ObjSet_FREQ_exit);

nneVarGrpIfaceModel10ObjSet_FREQ_exit:
    return status;
}

/*!
 * @copydoc NneVarInputIDMatch
 */
FLCN_STATUS
nneVarInputIDMatch_FREQ
(
    NNE_VAR                         *pVar,
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInput,
    LwBool                          *pBMatch
)
{
    NNE_VAR_FREQ   *pVarFreq = (NNE_VAR_FREQ *)pVar;
    FLCN_STATUS     status   = FLCN_OK;

    // Sanity checking.
    if ((pVarFreq == NULL) ||
        (pInput   == NULL) ||
        (pBMatch  == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarInputIDMatch_FREQ_exit;
    }

    *pBMatch = (pVarFreq->varFreqId.clkDomainIdx == pInput->data.freq.freqId.clkDomainIdx) &&
               (pVarFreq->varFreqId.bAbsolute    == pInput->data.freq.freqId.bAbsolute);

nneVarInputIDMatch_FREQ_exit:
    return status;
}

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 *
 * @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ implementation.
 *
 * @note This implementation must be kept in sync with @ref
 * NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_FREQ and @ref NNE_VAR_FREQ_CLK_DOMAIN_IDX_MAX
 */
FLCN_STATUS
nneVarsVarIdMapArrIdxGet_FREQ
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT  *pInput,
    LwU16                          *pMapArrIdx
)
{
    LW2080_CTRL_NNE_VAR_ID_FREQ *pFreqId;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (((pInput != NULL) && (pMapArrIdx != NULL) &&
                (pInput->type == LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ)) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_FREQ_exit);
    pFreqId = &pInput->data.freq.freqId;

    // Sanity check that the requested CLK_DOMAIN index is less than the supported max.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pFreqId->clkDomainIdx < NNE_VAR_FREQ_CLK_DOMAIN_IDX_MAX) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapArrIdxGet_FREQ_exit);
    *pMapArrIdx = pFreqId->clkDomainIdx + (pFreqId->bAbsolute ? 0 : NNE_VAR_FREQ_CLK_DOMAIN_IDX_MAX);

nneVarsVarIdMapArrIdxGet_FREQ_exit:
    return status;
}

/*!
 * @copydoc NneVarLwF32Get
 */
FLCN_STATUS
nneVarLwF32Get_FREQ
(
    const LW2080_CTRL_NNE_NNE_VAR_INPUT    *pInput,
    LwF32                                  *pVarValue
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pInput    == NULL) ||
        (pVarValue == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarLwF32Get_FREQ_exit;
    }

    *pVarValue =  lwF32ColwertFromS32(pInput->data.freq.freqMhz);

nneVarLwF32Get_FREQ_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
