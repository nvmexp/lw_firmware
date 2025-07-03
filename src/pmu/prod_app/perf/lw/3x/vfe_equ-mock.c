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
 * @file    vfe_equ-mock.c
 * @brief   Mock implementations of vfe_equ public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_equ-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @copydoc VFE_EQU_EVAL_NODE_SUPER_MOCK_CONFIG
 */
VFE_EQU_EVAL_NODE_SUPER_MOCK_CONFIG vfeEvalNode_SUPER_MOCK_CONFIG =
{
    .status             = FLCN_OK,
    .bOverrideResult    = LW_FALSE,
    .result             = 0.0f
};

/*!
 * @copydoc VFE_EQU_EVAL_LIST_MOCK_CONFIG
 */
VFE_EQU_EVAL_LIST_MOCK_CONFIG vfeEquEvalList_MOCK_CONFIG =
{
    .status             = FLCN_OK,
    .bOverrideResult    = LW_FALSE,
    .result             = { 0.0f }
};

/*!
 * @copydoc VFE_EQU_EVALUATE_MOCK_CONFIG
 */
VFE_EQU_EVALUATE_MOCK_CONFIG vfeEquEvaluate_MOCK_CONFIG =
{
    .status             = FLCN_OK,
    .result             = 0
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of vfeEquEvalNode_SUPER
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref vfeEvalNode_SUPER_MOCK_CONFIG. See @ref VfeEquEvalNode(fname)
 *          for original interface.
 *
 * @param[in]   pContext    IGNORED. Must not be NULL.
 * @param[in]   pVfeEqu     IGNORED. Must not be NULL.
 * @param[out]  pResult     Pointer to result field that is populated with
 *                          vfeEvalNode_SUPER_MOCK_CONFIG.result if
 *                          vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult is set
 *                          and vfeEvalNode_SUPER_MOCK_CONFIG.status is FLCN_OK.
 *
 * @return      vfeEvalNode_SUPER_MOCK_CONFIG.status.
 * @return      FLCN_ERR_ILWALID_ARGUMENT if any argument is NULL
 */
FLCN_STATUS
vfeEquEvalNode_SUPER_MOCK
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    if ((pContext == NULL)  ||
        (pVfeEqu  == NULL)  ||
        (pResult  == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if ((vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult) &&
        (vfeEvalNode_SUPER_MOCK_CONFIG.status == FLCN_OK))
    {
        *pResult = vfeEvalNode_SUPER_MOCK_CONFIG.result;
    }

    return vfeEvalNode_SUPER_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of vfeEquEvalList
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref vfeEquEvalList_MOCK_CONFIG. See @ref VfeEquEvalList(fname)
 *          for original interface.
 *
 * @param[in]   pContext    IGNORED. Must not be NULL.
 * @param[in]   vfeEquIdx   Must be less than VFE_EQU_EVAL_LIST_INPUTS_MAX.
 * @param[out]  pResult     Pointer to result field that is populated with
 *                          vfeEquEvalList_MOCK_CONFIG.result if
 *                          vfeEquEvalList_MOCK_CONFIG.bOverrideResult is set
 *                          and vfeEquEvalList_MOCK_CONFIG.status is FLCN_OK.
 *
 * @return      vfeEquEvalList_MOCK_CONFIG.status.
 * @return      FLCN_ERR_ILWALID_ARGUMENT if any argument is invalid.
 */
FLCN_STATUS
vfeEquEvalList_MOCK
(
    VFE_CONTEXT    *pContext,
    LwU8            vfeEquIdx,
    LwF32          *pResult
)
{
    if ((pContext  == NULL)                             ||
        (vfeEquIdx >=  VFE_EQU_EVAL_LIST_INPUTS_MAX)    ||
        (pResult   == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if ((vfeEquEvalList_MOCK_CONFIG.bOverrideResult) &&
        (vfeEquEvalList_MOCK_CONFIG.status == FLCN_OK))
    {
        *pResult = vfeEquEvalList_MOCK_CONFIG.result[vfeEquIdx];
    }

    return vfeEquEvalList_MOCK_CONFIG.status;
}

/*!
 * @brief Evaluates a VFE_EQU using an array of VFE_VAR values.
 *
 * @note If caller does not need to provide values for VFE_VAR this interface
 * should be called as vfeEquEvaluate(idx, NULL, 0, &result).
 *
 * @param[in]   vfeEquIdx   Equation list head node index.
 * @param[in]   pValues     Array of input VFE_VAR values.
 * @param[in]   valCount    Size of @ref pValues array.
 * @param[in]   outputType  Expected type of the result as
 *                          LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_<xyz>.
 * @param[out]  pResult     Buffer to hold evaluation result.
 *
 * @return FLCN_OK upon successful evaluation
 * @return error propagated from inner functions otherwise
 */
FLCN_STATUS
vfeEquEvaluate_MOCK
(
    LwVfeEquIdx                 vfeEquIdx,
    RM_PMU_PERF_VFE_VAR_VALUE  *pValues,
    LwU8                        valCount,
    LwU8                        outputType,
    RM_PMU_PERF_VFE_EQU_RESULT *pResult
)
{
    switch (outputType)
    {
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV:
        {
            pResult->voltDeltauV = vfeEquEvaluate_MOCK_CONFIG.result;
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_THRESH_PERCENT:
        {
            pResult->threshPercent = vfeEquEvaluate_MOCK_CONFIG.result;
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }
    return vfeEquEvaluate_MOCK_CONFIG.status;
}

/* ------------------------ Private Functions ------------------------------- */
