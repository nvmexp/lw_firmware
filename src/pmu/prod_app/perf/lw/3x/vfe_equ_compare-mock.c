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
 * @file    vfe_equ_compare-mock.c
 * @brief   Mock implementations of vfe_equ_compare public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_equ_compare.h"
#include "perf/3x/vfe_equ_compare-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @copydoc VFE_EQU_EVAL_NODE_COMPARE_MOCK_CONFIG
 */
VFE_EQU_EVAL_NODE_COMPARE_MOCK_CONFIG vfeEquEvalNode_COMPARE_MOCK_CONFIG = vfeEquEvalNode_COMPARE_MOCK_CONFIG_DEFAULT;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of vfeEquEvalNode_COMPARE
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref vfeEquEvalNode_COMPARE_MOCK. See @ref VfeEquEvalNode()
 *          for original interface.
 *
 * @param[in]   pContext    IGNORED. Must not be NULL.
 * @param[in]   pVfeEqu     IGNORED. Must not be NULL.
 * @param[out]  pResult     Pointer to result field that is populated with
 *                          vfeEquEvalNode_COMPARE_MOCK_CONFIG.result if
 *                          vfeEquEvalNode_COMPARE_MOCK_CONFIG.bOverrideResult is set
 *                          and vfeEquEvalNode_COMPARE_MOCK_CONFIG.status is FLCN_OK.
 *
 * @return      vfeEquEvalNode_COMPARE_MOCK_CONFIG.status.
 * @return      FLCN_ERR_ILWALID_ARGUMENT if any argument is NULL
 */
FLCN_STATUS
vfeEquEvalNode_COMPARE_MOCK
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

    if (vfeEquEvalNode_COMPARE_MOCK_CONFIG.bCallEvalList)
    {
        return vfeEquEvalList_IMPL(pContext,
                ((VFE_EQU_COMPARE *)pVfeEqu)->equIdxTrue,
                pResult);
    }

    if ((vfeEquEvalNode_COMPARE_MOCK_CONFIG.bOverrideResult) &&
        (vfeEquEvalNode_COMPARE_MOCK_CONFIG.status == FLCN_OK))
    {
        *pResult = vfeEquEvalNode_COMPARE_MOCK_CONFIG.result;
    }

    return vfeEquEvalNode_COMPARE_MOCK_CONFIG.status;
}

/* ------------------------ Private Functions ------------------------------- */
