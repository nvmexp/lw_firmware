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
 * @file    vfe_equ_compare-test.c
 * @brief   Unit tests for logic in VFE_EQU_COMPARE.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_equ-mock.h"
#include "perf/3x/vfe_var-mock.h"

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to a compare equation and its expected result.
 */
typedef struct VFE_EQU_COMPARE_INPUT_OUTPUT
{
    LwU8    comparison;     //!< LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_<xyz>.
    LwF32   var;            //!< Independent variable.
    LwF32   criteria;       //!< Comparison against independent variable.
    LwF32   equResultTrue;  //!< Result of true equation.
    LwF32   equResultFalse; //!< Result of false equation.
    LwF32   result;         //!< Expected result of evaluation.
} VFE_EQU_COMPARE_INPUT_OUTPUT;

/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of positive test conditions for EvalNodeLogic.
 */
static VFE_EQU_COMPARE_INPUT_OUTPUT vfeEquCompareTestResults[] =
{
    /* 0000 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL,      .var = 1.0f, .criteria = 1.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 10.0f  },
    /* 0001 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL,      .var = 1.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 20.0f  },

    /* 0002 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ, .var = 1.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 20.0f  },
    /* 0003 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ, .var = 2.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 10.0f  },
    /* 0004 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ, .var = 3.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 10.0f  },

    /* 0005 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER,    .var = 1.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 20.0f  },
    /* 0006 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER,    .var = 2.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 20.0f  },
    /* 0007 */  { .comparison = LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER,    .var = 3.0f, .criteria = 2.0f, .equResultTrue = 10.0f, .equResultFalse = 20.0f, .result = 10.0f  },
};

/*!
 * @brief The test group for the VFE compare equation.
 *
 * @defgroup VFE_EQU_COMPARE_TEST_SUITE
 */
UT_SUITE_DEFINE(PMU_VFE_EQU_COMPARE,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO"))

/* ---------------------- vfeEquEvalNode_COMPARE_IMPL() --------------------- */
/*!
 * @brief Unit test for vfeEquEvalNode_COMPARE's core logic.
 *
 * Iterates over testcases described in @ref vfeEquCompareTestResults to simply
 * exercise the evaluation of compare equations. The SUPER behaviour of
 * vfeEquEvalNode is not tested here.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_COMPARE.EvalNodeLogic
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_COMPARE_TEST_SUITE
 *
 * Test Setup:
 * @li Set #1: Compare var (1.0) == criteria (1.0), expect true result
 * @li Set #2: Compare var (1.0) == criteria (2.0), expect false result
 * @li Set #3: Compare var (1.0) >= criteria (2.0), expect false result
 * @li Set #4: Compare var (2.0) >= criteria (2.0), expect true result
 * @li Set #5: Compare var (3.0) >= criteria (2.0), expect true result
 * @li Set #6: Compare var (1.0) > criteria (2.0), expect false result
 * @li Set #7: Compare var (2.0) > criteria (2.0), expect false result
 * @li Set #8: Compare var (3.0) > criteria (2.0), expect true result
 *
 * Test Exelwtion:
 * @li Loop over the data sets, using the comparison function, variable value,
 * and criteria value as parameters. Expect the result specified in the data
 * set.
 */
UT_CASE_DEFINE(PMU_VFE_EQU_COMPARE, EvalNodeLogic,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU_COMPARE, EvalNodeLogic)
{
    VFE_EQU_COMPARE vfeEquCompare   = { 0 };
    VFE_CONTEXT     vfeContext      = { 0 };
    LwF32           result          = 0;
    LwU32           lwrrentTestIdx  = 0;
    FLCN_STATUS     status          = FLCN_ERROR;

    // Configure vfeVarEvalByIdxFromCache for positive testing
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status             = FLCN_OK;
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult    = LW_TRUE;

    // Configure vfeEquEvalList for positive testing
    vfeEquEvalList_MOCK_CONFIG.status           = FLCN_OK;
    vfeEquEvalList_MOCK_CONFIG.bOverrideResult  = LW_TRUE;

    // Configure vfeEvalNode_SUPER for positive testing
    vfeEvalNode_SUPER_MOCK_CONFIG.status            = FLCN_OK;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult   = LW_FALSE;

    // Evaluate each set of test inputs
    for (lwrrentTestIdx = 0; lwrrentTestIdx < LW_ARRAY_ELEMENTS(vfeEquCompareTestResults); lwrrentTestIdx++)
    {
        // Zero out inputs
        memset(&vfeEquCompare, 0x0, sizeof(vfeEquCompare));
        memset(&vfeContext,    0x0, sizeof(vfeContext));
        memset(&result,        0x0, sizeof(result));

        // Setup equation
        vfeEquCompare.funcId        = vfeEquCompareTestResults[lwrrentTestIdx].comparison;
        vfeEquCompare.equIdxTrue    = 0;
        vfeEquCompare.equIdxFalse   = 1;
        vfeEquCompare.criteria      = vfeEquCompareTestResults[lwrrentTestIdx].criteria;

        // Change result of vfeVarEvalByIdxFromCache to the independant variable
        vfeVarEvalByIdxFromCache_MOCK_CONFIG.result =
             vfeEquCompareTestResults[lwrrentTestIdx].var;

        // Change result of vfeEquEvalList to based on input equation index
        vfeEquEvalList_MOCK_CONFIG.result[vfeEquCompare.equIdxTrue]     = vfeEquCompareTestResults[lwrrentTestIdx].equResultTrue;
        vfeEquEvalList_MOCK_CONFIG.result[vfeEquCompare.equIdxFalse]    = vfeEquCompareTestResults[lwrrentTestIdx].equResultFalse;

        // Execute unit under test
        status = vfeEquEvalNode_COMPARE_IMPL(&vfeContext, &vfeEquCompare.super, &result);

        // Check outputs
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        if (lwF32CmpNE(result, vfeEquCompareTestResults[lwrrentTestIdx].result))
        {
            utf_printf("%s(%d) : testcase %04d failed\n",
                __FUNCTION__, __LINE__, lwrrentTestIdx);
        }
        UT_ASSERT_TRUE(lwF32CmpEQ(result, vfeEquCompareTestResults[lwrrentTestIdx].result));
    }
}

/*!
 * @brief Unit test for vfeEquEvalNode_COMPARE's error propagation.
 *
 * Simply Checks to see that the error propagation of dependent unit failures
 * behave as expected.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_COMPARE.EvalNodeErrorPropagation
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_COMPARE_TEST_SUITE
 *
 * Test Setup:
 * @li Configure the VFE comparison equation.
 * @li Set 1: Configure variable evaluation to return an error.
 * @li Set 2: Configure the equation evaluation list to return an error.
 * @li Set 3: Configure the node evaluation to return an error.
 *
 * Test Exelwtion:
 * @li Loop over the data sets, using the conditions specified. Expect the
 * return value specified by the data set.
 */
UT_CASE_DEFINE(PMU_VFE_EQU_COMPARE, EvalNodeErrorPropagation,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU_COMPARE, EvalNodeErrorPropagation)
{
    VFE_EQU_COMPARE vfeEquCompare   = { 0 };
    VFE_CONTEXT     vfeContext      = { 0 };
    LwF32           result          = 0;
    FLCN_STATUS     status          = FLCN_ERROR;

    // Setup equation
    vfeEquCompare.funcId        = vfeEquCompareTestResults[0].comparison;
    vfeEquCompare.equIdxTrue    = 0;
    vfeEquCompare.equIdxFalse   = 1;

    // Change result of vfeEquEvalList to based on input equation index
    vfeEquEvalList_MOCK_CONFIG.result[vfeEquCompare.equIdxTrue]     = vfeEquCompareTestResults[0].equResultTrue;
    vfeEquEvalList_MOCK_CONFIG.result[vfeEquCompare.equIdxFalse]    = vfeEquCompareTestResults[0].equResultFalse;

    // Ensure error propagation works as expected for unit dependencies
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status             = FLCN_ERR_MUTEX_ACQUIRED;
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult    = LW_FALSE;
    status = vfeEquEvalNode_COMPARE_IMPL(&vfeContext, &vfeEquCompare.super, &result);
    UT_ASSERT_EQUAL_UINT(status, vfeVarEvalByIdxFromCache_MOCK_CONFIG.status);
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status = FLCN_OK;

    vfeEquEvalList_MOCK_CONFIG.status           = FLCN_ERR_ILLEGAL_OPERATION;
    vfeEquEvalList_MOCK_CONFIG.bOverrideResult  = LW_FALSE;
    status = vfeEquEvalNode_COMPARE_IMPL(&vfeContext, &vfeEquCompare.super, &result);
    UT_ASSERT_EQUAL_UINT(status, vfeEquEvalList_MOCK_CONFIG.status);
    vfeEquEvalList_MOCK_CONFIG.status = FLCN_OK;

    vfeEvalNode_SUPER_MOCK_CONFIG.status            = FLCN_ERR_FEATURE_NOT_ENABLED;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult   = LW_FALSE;
    status = vfeEquEvalNode_COMPARE_IMPL(&vfeContext, &vfeEquCompare.super, &result);
    UT_ASSERT_EQUAL_UINT(status, vfeEvalNode_SUPER_MOCK_CONFIG.status);
    vfeEvalNode_SUPER_MOCK_CONFIG.status = FLCN_OK;
}

/*!
 * @brief Unit test for vfeEquEvalNode_COMPARE's invalid function error handling.
 *
 * Simply Checks to see that the error propagation of dependent unit failures
 * behave as expected.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_COMPARE.EvalNodeIlwalidFunction
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_COMPARE_TEST_SUITE
 *
 * Test Setup:
 * @li Configure the equation to have an invalid function ID.
 *
 * Test Exelwtion:
 * Call @ref vfeEquEvalNode_COMPARE_IMPL. Expect @ref FLCN_ERR_ILWALID_STATE to
 * be returned.
 */
UT_CASE_DEFINE(PMU_VFE_EQU_COMPARE, EvalNodeIlwalidFunction,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU_COMPARE, EvalNodeIlwalidFunction)
{
    VFE_EQU_COMPARE vfeEquCompare   = { 0 };
    VFE_CONTEXT     vfeContext      = { 0 };
    LwF32           result          = 0;
    FLCN_STATUS     status          = FLCN_ERROR;

    // Setup equation
    vfeEquCompare.funcId        = 0xFF; // Invalid function
    vfeEquCompare.equIdxTrue    = 0;
    vfeEquCompare.equIdxFalse   = 1;

    // Change result of vfeEquEvalList to based on input equation index
    vfeEquEvalList_MOCK_CONFIG.result[vfeEquCompare.equIdxTrue]     = vfeEquCompareTestResults[0].equResultTrue;
    vfeEquEvalList_MOCK_CONFIG.result[vfeEquCompare.equIdxFalse]    = vfeEquCompareTestResults[0].equResultFalse;

    // Setup dependencies to not do anything
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status             = FLCN_OK;
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult    = LW_FALSE;

    vfeEquEvalList_MOCK_CONFIG.status                       = FLCN_OK;
    vfeEquEvalList_MOCK_CONFIG.bOverrideResult              = LW_FALSE;

    vfeEvalNode_SUPER_MOCK_CONFIG.status                    = FLCN_OK;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult           = LW_FALSE;

    // Call unit
    status = vfeEquEvalNode_COMPARE_IMPL(&vfeContext, &vfeEquCompare.super, &result);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);
}

/* ------------------------ End of File ------------------------------------- */
