/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var-mock.h
 * @brief   Data required for configuring mock vfe_var interfaces.
 */

#ifndef VFE_VAR_MOCK_H
#define VFE_VAR_MOCK_H
/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/*!
 * Configuration variables for @ref vfeVarEvalByIdxFromCache_MOCK.
 */
typedef struct VFE_VAR_EVAL_BY_IDX_FROM_CACHE_MOCK_CONFIG
{
    /*!
     * Mock returned status in case of no other errors.
     */
    FLCN_STATUS status;

    /*!
     * Set if *pResult should be overridden
     * (param to @ref vfeVarEvalByIdxFromCache_MOCK).
     */
    LwBool      bOverrideResult;

    /*!
     * Mock value of *pResult
     * (param to @ref vfeVarEvalByIdxFromCache_MOCK).
     */
    LwF32       result;
} VFE_VAR_EVAL_BY_IDX_FROM_CACHE_MOCK_CONFIG;

/* ------------------------ External Definitions ---------------------------- */
extern VFE_VAR_EVAL_BY_IDX_FROM_CACHE_MOCK_CONFIG vfeVarEvalByIdxFromCache_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_MOCK_H
