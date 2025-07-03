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
 * @file    vfe_equ_minmax-test.c
 * @brief   Unit tests for logic in VFE_EQU_MINMAX.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_equ-mock.h"
#include "perf/3x/vfe_var-mock.h"

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to a minmax equation and its expected result.
 *
 * @details result = (bMax ? MAX(eval(equ1), eval(equ2)) : MIN(eval(equ1), eval(equ2))).
 */
typedef struct VFE_EQU_MINMAX_INPUT_OUTPUT
{
    LwBool  bMax;           //!< If true, expect MAX(equ0_result, equ1_result), else, MIN(...)
    LwF32   equ0_result;    //!< Result of first equation.
    LwF32   equ1_result;    //!< Result of second equation.
    LwF32   result;         //!< Expected result of evaluation.
} VFE_EQU_MINMAX_INPUT_OUTPUT;

/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of positive test conditions for PMU_VFE_EQU_MinMaxTest.
 *
 * @details Expectation is that result == (bMax ? MAX(eval(equ1), eval(equ2)) : MIN(eval(equ1), eval(equ2))).
 */
static VFE_EQU_MINMAX_INPUT_OUTPUT vfeEquMinMaxTestResults[] =
{
    /* 0000 */  { .bMax = LW_FALSE, .equ0_result = 0.0f,    .equ1_result = 0.0f,    .result = 0.0f  },
    /* 0001 */  { .bMax = LW_TRUE,  .equ0_result = 0.0f,    .equ1_result = 0.0f,    .result = 0.0f  },

    /* 0002 */  { .bMax = LW_FALSE, .equ0_result = 1.0f,    .equ1_result = 2.0f,    .result = 1.0f  },
    /* 0003 */  { .bMax = LW_TRUE,  .equ0_result = 1.0f,    .equ1_result = 2.0f,    .result = 2.0f  },

    /* 0004 */  { .bMax = LW_FALSE, .equ0_result = 2.0f,    .equ1_result = 1.0f,    .result = 1.0f  },
    /* 0005 */  { .bMax = LW_TRUE,  .equ0_result = 2.0f,    .equ1_result = 1.0f,    .result = 2.0f  },
};

/* ------------------------ Defines and Macros ------------------------------ */

/*!
 * @brief The test group for the VFE min/max equation.
 *
 * @defgroup VFE_EQU_MINMAX_TEST_SUITE
 */
UT_SUITE_DEFINE(PMU_VFE_EQU_MINMAX,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO"))

/* ----------------------- vfeEquEvalNode_MINMAX() -------------------------- */
/*!
 * @brief   Unit test for vfeEquEvalNode_MINMAX's core logic.
 *
 * Iterates over testcases described in @ref vfeEquMinMaxTestResults to simply
 * exercise the evaluation of minmax equations. The SUPER behaviour of
 * vfeEquEvalNode is not tested here.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_MINMAX.EvalNodeLogic
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_MINMAX_TEST_SUITE
 *
 * Test Setup:
 * @li Set #1: min(0.0, 0.0), expect 0.0
 * @li Set #2: max(0.0, 0.0), expect 0.0
 * @li Set #3: min(1.0, 2.0), expect 1.0
 * @li Set #4: max(1.0, 2.0), expect 2.0
 * @li Set #5: min(2.0, 1.0), expect 1.0
 * @li Set #6: max(2.0, 1.0), expect 2.0
 *
 * Test Exelwtion:
 * @li Call @ref vfeEquEvalNode_MINMAX() for each data set. Expect the result
 * as specified by the data set.
 */
UT_CASE_DEFINE(PMU_VFE_EQU_MINMAX, EvalNodeLogic,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU_MINMAX, EvalNodeLogic)
{
    VFE_EQU_MINMAX  vfeEquMinMax    = { 0 };
    VFE_CONTEXT     vfeContext      = { 0 };
    LwF32           result          = 0;
    LwU32           lwrrentTestIdx  = 0;
    FLCN_STATUS     status          = FLCN_ERROR;

    // Configure vfeEquEvalList for positive testing
    vfeEquEvalList_MOCK_CONFIG.status           = FLCN_OK;
    vfeEquEvalList_MOCK_CONFIG.bOverrideResult  = LW_TRUE;

    // Configure vfeEvalNode_SUPER for positive testing
    vfeEvalNode_SUPER_MOCK_CONFIG.status            = FLCN_OK;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult   = LW_FALSE;

    // Evaluate each set of test inputs
    for (lwrrentTestIdx = 0; lwrrentTestIdx < LW_ARRAY_ELEMENTS(vfeEquMinMaxTestResults); lwrrentTestIdx++)
    {
        // Zero out inputs
        memset(&vfeEquMinMax, 0x0, sizeof(vfeEquMinMax));
        memset(&vfeContext,   0x0, sizeof(vfeContext));
        memset(&result,       0x0, sizeof(result));

        // Setup equation
        vfeEquMinMax.bMax       = vfeEquMinMaxTestResults[lwrrentTestIdx].bMax;
        vfeEquMinMax.equIdx0    = 0;
        vfeEquMinMax.equIdx1    = 1;

        // Change result of vfeEquEvalList to based on input equation index
        vfeEquEvalList_MOCK_CONFIG.result[vfeEquMinMax.equIdx0] = vfeEquMinMaxTestResults[lwrrentTestIdx].equ0_result;
        vfeEquEvalList_MOCK_CONFIG.result[vfeEquMinMax.equIdx1] = vfeEquMinMaxTestResults[lwrrentTestIdx].equ1_result;

        // Execute unit under test
        status = vfeEquEvalNode_MINMAX(&vfeContext, &vfeEquMinMax.super, &result);

        // Check outputs
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        if (lwF32CmpNE(result, vfeEquMinMaxTestResults[lwrrentTestIdx].result))
        {
            utf_printf("%s(%d) : testcase %04d failed\n",
                __FUNCTION__, __LINE__, lwrrentTestIdx);
        }
        UT_ASSERT_TRUE(lwF32CmpEQ(result, vfeEquMinMaxTestResults[lwrrentTestIdx].result));
    }
}

/*!
 * @brief Unit test for vfeEquEvalNode_MINMAX's error propagation.
 *
 * Simply Checks to see that the error propagation of dependent unit failures
 * behave as expected.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_MINMAX.EvalNodeErrorPropagation
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_MINMAX_TEST_SUITE
 *
 * Test Setup:
 * @li Set 1: @ref vfeEquList() returns a failure
 * @li Set 2: @ref vfeEvalNode() returns a failure
 *
 * Test Exelwtion:
 * @li Call @ref vfeEquEvalNode_MINMAX() for each data set. Expect the errors
 * the mocked versions of the functions specified return as the return value.
 */
UT_CASE_DEFINE(PMU_VFE_EQU_MINMAX, EvalNodeErrorPropagation,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU_MINMAX, EvalNodeErrorPropagation)
{
    VFE_EQU_MINMAX  vfeEquMinMax    = { 0 };
    VFE_CONTEXT     vfeContext      = { 0 };
    LwF32           result          = 0;
    FLCN_STATUS     status          = FLCN_ERROR;

    // Setup equation
    vfeEquMinMax.bMax       = vfeEquMinMaxTestResults[0].bMax;
    vfeEquMinMax.equIdx0    = 0;
    vfeEquMinMax.equIdx1    = 1;

    // Change result of vfeEquEvalList to base don input equation index
    vfeEquEvalList_MOCK_CONFIG.result[vfeEquMinMax.equIdx0] = vfeEquMinMaxTestResults[0].equ0_result;
    vfeEquEvalList_MOCK_CONFIG.result[vfeEquMinMax.equIdx1] = vfeEquMinMaxTestResults[0].equ1_result;

    // Ensure error propagation works as expected for unit dependencies
    vfeEquEvalList_MOCK_CONFIG.status           = FLCN_ERR_ILLEGAL_OPERATION;
    vfeEquEvalList_MOCK_CONFIG.bOverrideResult  = LW_FALSE;
    status = vfeEquEvalNode_MINMAX(&vfeContext, &vfeEquMinMax.super, &result);
    UT_ASSERT_EQUAL_UINT(status, vfeEquEvalList_MOCK_CONFIG.status);
    vfeEquEvalList_MOCK_CONFIG.status = FLCN_OK;

    vfeEvalNode_SUPER_MOCK_CONFIG.status            = FLCN_ERR_FEATURE_NOT_ENABLED;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult   = LW_FALSE;
    status = vfeEquEvalNode_MINMAX(&vfeContext, &vfeEquMinMax.super, &result);
    UT_ASSERT_EQUAL_UINT(status, vfeEvalNode_SUPER_MOCK_CONFIG.status);
    vfeEvalNode_SUPER_MOCK_CONFIG.status = FLCN_OK;
}

/* ------------------------ End of File ------------------------------------- */
