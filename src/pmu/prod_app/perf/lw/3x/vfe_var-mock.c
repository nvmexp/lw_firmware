/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var-mock.c
 * @brief   Mock implementations of vfe_var public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var.h"
#include "perf/3x/vfe_var-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @copydoc VFE_VAR_EVAL_BY_IDX_FROM_CACHE_MOCK_CONFIG
 */
VFE_VAR_EVAL_BY_IDX_FROM_CACHE_MOCK_CONFIG vfeVarEvalByIdxFromCache_MOCK_CONFIG =
{
    .status             = FLCN_OK,
    .bOverrideResult    = LW_FALSE,
    .result             = 0.0f
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of vfeVarEvalByIdxFromCache.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref vfeVarEvalByIdxFromCache_MOCK_CONFIG. See @ref VfeVarEvalByIdx(fname)
 *          for original interface.
 *
 * @param[in]   pContext    IGNORED. Must nto be NULL.
 * @param[in]   vfeVarIdx   IGNORED.
 * @param[out]  pResult     Pointer to result field that is populated with
 *                          vfeVarEvalByIdxFromCache_MOCK_CONFIG.result if
 *                          vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult is set
 *                          and vfeVarEvalByIdxFromCache_MOCK_CONFIG.status is FLCN_OK.
 *
 * @return      vfeVarEvalByIdxFromCache_MOCK_CONFIG.status.
 * @return      FLCN_ERR_ILWALID_ARGUMENT if pResult or pContext are NULL.
 */
FLCN_STATUS
vfeVarEvalByIdxFromCache_MOCK
(
    VFE_CONTEXT    *pContext,
    LwU8            vfeVarIdx,
    LwF32          *pResult
)
{
    if ((pContext == NULL) ||
        (pResult  == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if ((vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult) &&
        (vfeVarEvalByIdxFromCache_MOCK_CONFIG.status == FLCN_OK))
    {
        *pResult = vfeVarEvalByIdxFromCache_MOCK_CONFIG.result;
    }

    return vfeVarEvalByIdxFromCache_MOCK_CONFIG.status;
}

/* ------------------------ Private Functions ------------------------------- */
