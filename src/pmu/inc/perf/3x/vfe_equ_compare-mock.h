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
 * @file    vfe_equ_compare-mock.h
 * @brief   Data required for configuring mock vfe_equ_compare interfaces.
 */

#ifndef VFE_EQU_COMPARE_MOCK_H
#define VFE_EQU_COMPARE_MOCK_H
/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define vfeEquEvalNode_COMPARE_MOCK_CONFIG_DEFAULT  \
{                                                   \
    .status             = FLCN_OK,                  \
    .bCallEvalList      = LW_FALSE,                 \
    .bOverrideResult    = LW_FALSE,                 \
    .result             = 0.0f                      \
}

#define vfeEquEvalNode_COMPARE_MOCK_CONFIG_RESET(config)    \
do                                                          \
{                                                           \
    config.status           = FLCN_OK;                      \
    config.bCallEvalList    = LW_FALSE;                     \
    config.bOverrideResult  = LW_FALSE;                     \
    config.result           = 0.0f;                         \
}                                                           \
while (LW_FALSE)
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration variables for @ref vfeEquEvalNode_COMPARE_MOCK.
 */
typedef struct VFE_EQU_EVAL_NODE_COMPARE_MOCK_CONFIG
{
    /*!
     * Mock returned status
     */
    FLCN_STATUS status;

    /*!
     * Set if you want to relwrsively call @ref vfeEquEvalList_IMPL on the input
     * ((VFE_EQU_COMPARE *)pVfeEqu)->equIdxTrue. If set, only vfeEquEvalList_IMPL
     * will be run and status+result will not be overriden.
     */
    LwBool      bCallEvalList;

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
} VFE_EQU_EVAL_NODE_COMPARE_MOCK_CONFIG;

/* ------------------------ External Definitions ---------------------------- */
extern VFE_EQU_EVAL_NODE_COMPARE_MOCK_CONFIG vfeEquEvalNode_COMPARE_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_EQU_MOCK_H
