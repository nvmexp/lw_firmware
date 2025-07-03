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
 * @file    vfe_equ-mock.h
 * @brief   Data required for configuring mock vfe_equ interfaces.
 */

#ifndef VFE_EQU_MOCK_H
#define VFE_EQU_MOCK_H
/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define VFE_EQU_EVAL_LIST_INPUTS_MAX    (16)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration variables for @ref vfeEquEvalNode_SUPER_MOCK.
 */
typedef struct VFE_EQU_EVAL_NODE_SUPER_MOCK_CONFIG
{
    /*!
     * Mock returned status in case of no other errors.
     */
    FLCN_STATUS status;

    /*!
     * Set if *pResult should be overridden
     * (param in @ref vfeEquEvalNode_SUPER_IMPL).
     */
    LwBool      bOverrideResult;

    /*!
     * Mock value of *pResult
     * (param in @ref vfeEquEvalNode_SUPER_IMPL).
     */
    LwF32       result;
} VFE_EQU_EVAL_NODE_SUPER_MOCK_CONFIG;

/*!
 * Configuration variables for @ref vfeEquEvalList_MOCK.
 */
typedef struct VFE_EQU_EVAL_LIST_MOCK_CONFIG
{
    /*!
     * Mock returned status in case of no other errors.
     */
    FLCN_STATUS status;

    /*!
     * Set if *pResult should be overridden
     * (param in @ref vfeEquEvalNode_SUPER_IMPL).
     */
    LwBool      bOverrideResult;

    /*!
     * Mock value of *pResult with result[vfeEquIdx]
     * (params in @ref vfeEquEvalNode_SUPER_IMPL).
     */
    LwF32       result[VFE_EQU_EVAL_LIST_INPUTS_MAX];
} VFE_EQU_EVAL_LIST_MOCK_CONFIG;

/*!
 * Configuration variables for @ref vfeEquEvaluate_MOCK.
 */
typedef struct VFE_EQU_EVALUATE_MOCK_CONFIG
{
    /*!
     * Mock value of *pResult with result.
     */
    LwU32       result;

    /*!
     * Mock returned status in case of no other errors.
     */
    FLCN_STATUS status;
} VFE_EQU_EVALUATE_MOCK_CONFIG;

/* ------------------------ External Definitions ---------------------------- */
extern VFE_EQU_EVAL_NODE_SUPER_MOCK_CONFIG  vfeEvalNode_SUPER_MOCK_CONFIG;
extern VFE_EQU_EVAL_LIST_MOCK_CONFIG        vfeEquEvalList_MOCK_CONFIG;
extern VFE_EQU_EVALUATE_MOCK_CONFIG         vfeEquEvaluate_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_EQU_MOCK_H
