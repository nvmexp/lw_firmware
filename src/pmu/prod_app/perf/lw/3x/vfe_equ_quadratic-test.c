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
 * @file    vfe_equ_quadratic-test.c
 * @brief   Unit tests for logic in VFE_EQU_QUADRATIC.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_equ-mock.h"
#include "perf/3x/vfe_var-mock.h"

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief Helper union to quickly colwert between a float and its byte representation (IEEE 754).
 */
typedef union LW_VFE_UINT_FLOAT
{
    LwU32 uintRep;  //!< Read/write this value as a unsigned dword.
    LwF32 floatRep; //!< Read/write this value as a single precision float.
} LW_VFE_UINT_FLOAT;

/*!
 * @brief   Structure describing a set of inputs to a quadratic equation and its expected result.
 *
 * @details result = c2*var*var + c1*var + c0.
 */
typedef struct VFE_EQU_QUADRATIC_INPUT_OUTPUT
{
    LW_VFE_UINT_FLOAT c2;       //!< Second degree coefficient.
    LW_VFE_UINT_FLOAT c1;       //!< First degree coefficient.
    LW_VFE_UINT_FLOAT c0;       //!< Zeroth degree coefficient.
    LW_VFE_UINT_FLOAT var;      //!< Independent variable.
    LW_VFE_UINT_FLOAT result;   //!< Expected result of evaluation.
} VFE_EQU_QUADRATIC_INPUT_OUTPUT;

/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of positive test conditions for PMU_VFE_EQA_QuadraticTest.
 *
 * @details Expectation is that (c2*var*var + c1*var + c0) == result.
 */
static VFE_EQU_QUADRATIC_INPUT_OUTPUT vfeEquQuadraticTestResults[] =
{
    /* 0000 */  { .c2.floatRep = 0.0f,      .c1.floatRep = 0.0f,        .c0.floatRep = 0.0f,        .var.floatRep = 0.0f,   .result.floatRep = 0.0f         },
    /* 0001 */  { .c2.floatRep = 1.0f,      .c1.floatRep = 2.0f,        .c0.floatRep = 3.0f,        .var.floatRep = 0.0f,   .result.floatRep = 3.0f         },
    /* 0002 */  { .c2.floatRep = 1.5f,      .c1.floatRep = 2.5f,        .c0.floatRep = 3.5f,        .var.floatRep = 10.0f,  .result.floatRep = 178.5f       },
    /* 0003 */  { .c2.floatRep = 0.5f,      .c1.floatRep = 0.0f,        .c0.floatRep = 0.0f,        .var.floatRep = 5.0f,   .result.floatRep = 12.50f       },
    /* 0004 */  { .c2.floatRep = 0.0f,      .c1.floatRep = 0.0f,        .c0.floatRep = 3.14159f,    .var.floatRep = 1.2f,   .result.uintRep  = 0x40490fd0   },
    /* 0005 */  { .c2.floatRep = 0.0f,      .c1.floatRep = 3.14159f,    .c0.floatRep = 0.0f,        .var.floatRep = 1.2f,   .result.uintRep  = 0x4071462d   },
    /* 0006 */  { .c2.floatRep = 0.0f,      .c1.floatRep = 3.14159f,    .c0.floatRep = 3.14159f,    .var.floatRep = 1.2f,   .result.uintRep  = 0x40dd2afe   },
    /* 0007 */  { .c2.floatRep = 3.14159f,  .c1.floatRep = 0.0f,        .c0.floatRep = 0.0f,        .var.floatRep = 1.2f,   .result.uintRep  = 0x4090c3b5   },
    /* 0008 */  { .c2.floatRep = 3.14159f,  .c1.floatRep = 0.0f,        .c0.floatRep = 3.14159f,    .var.floatRep = 1.2f,   .result.uintRep  = 0x40f54b9d   },
    /* 0009 */  { .c2.floatRep = 3.14159f,  .c1.floatRep = 3.14159f,    .c0.floatRep = 0.0f,        .var.floatRep = 1.2f,   .result.uintRep  = 0x4104b366   },
    /* 0010 */  { .c2.floatRep = 3.14159f,  .c1.floatRep = 3.14159f,    .c0.floatRep = 3.14159f,    .var.floatRep = 1.2f,   .result.uintRep  = 0x4136f75a   },
};

/* ------------------------ Defines and Macros ------------------------------ */

/*!
 * @brief   Number of positive test cases for PMU_VFE_EQA_QuadraticTest.
 */
#define NUM_VFE_EQU_QUADRATIC_TEST_CASES    \
    (sizeof(vfeEquQuadraticTestResults) / sizeof(VFE_EQU_QUADRATIC_INPUT_OUTPUT))

/*!
 * @brief       Expect a certain LW_VFE_UINT_FLOAT result of the evaluation for a given test
 *              case, lwrrentTestIdx.
 *
 * @details     In the case of a failed expectation, print out details of the failing test case.
 *
 * @param[in]   result          Result of quadratic polynomial evaluation as a LW_VFE_UINT_FLOAT.
 * @param[in]   lwrrentTestIdx  Current test ordinal as some unsigned integer (index into
 *                              vfeEquQuadraticTestResults[]).
 */
#define EXPECT_VFE_EQU_QUADRATIC_RESULT(result, lwrrentTestIdx)                                               \
    do                                                                                                        \
    {                                                                                                         \
        if ((result).uintRep != vfeEquQuadraticTestResults[(lwrrentTestIdx)].result.uintRep)                  \
        {                                                                                                     \
            utf_printf("%s(%d) : testcase = %04d c2 = 0x%08x c1 = 0x%08x c0 = 0x%08x "                        \
                       "var = 0x%08x expectedResult = 0x%08x actualResult = 0x%08x\n",                        \
                __FUNCTION__, __LINE__,                                                                       \
                (lwrrentTestIdx),                                                                             \
                vfeEquQuadraticTestResults[(lwrrentTestIdx)].c2.uintRep,                                      \
                vfeEquQuadraticTestResults[(lwrrentTestIdx)].c1.uintRep,                                      \
                vfeEquQuadraticTestResults[(lwrrentTestIdx)].c0.uintRep,                                      \
                vfeEquQuadraticTestResults[(lwrrentTestIdx)].var.uintRep,                                     \
                vfeEquQuadraticTestResults[(lwrrentTestIdx)].result.uintRep,                                  \
                (result).uintRep);                                                                            \
        }                                                                                                     \
        UT_ASSERT_EQUAL_UINT((result).uintRep, vfeEquQuadraticTestResults[(lwrrentTestIdx)].result.uintRep);  \
    }                                                                                                         \
    while (LW_FALSE)

/*!
 * @brief The test group for the VFE quadratic equation.
 *
 * @defgroup VFE_EQU_QUADRATIC_TEST_SUITE
 */
UT_SUITE_DEFINE(PMU_VFE_EQU_QUADRATIC,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO"))

/* ----------------------- vfeEquEvalNode_QUADRATIC() ----------------------- */
/*!
 * @brief   Unit test for vfeEquEvalNode_QUADRATIC's core logic.
 *
 * Iterates over testcases described in @ref vfeEquQuadraticTestResults to
 * simply exercise the evaluation of quadratic polynomials. The SUPER behaviour
 * of vfeEquEvalNode is not tested here.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_QUADRATIC.EvalNodeLogic
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_QUADRATIC_TEST_SUITE
 *
 * Test Setup:
 * @li Set up the mock function for @ref vfeVarEvalByIdxFromCache() and
 * @ref vfeEvalNode() to return FLCN_OK.
 *
 * Test Exelwtion:
 * @li Loop over the data sets found in @ref vfeEquQuadraticTestResults. Verify
 * @ref vfeEquEvalNode_QUADRATIC() returns the expected values based on the
 * inputs provided.
 */
UT_CASE_DEFINE(PMU_VFE_EQU_QUADRATIC, EvalNodeLogic,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU_QUADRATIC, EvalNodeLogic)
{
    VFE_EQU_QUADRATIC   vfeEquQuadratic = { 0 };
    VFE_CONTEXT         vfeContext      = { 0 };
    LW_VFE_UINT_FLOAT   result          = { 0 };
    LwU32               lwrrentTestIdx  = 0;
    FLCN_STATUS         status          = FLCN_ERROR;

    // Configure vfeVarEvalByIdxFromCache for positive testing
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status             = FLCN_OK;
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult    = LW_TRUE;

    // Configure vfeEvalNode_SUPER for positive testing
    vfeEvalNode_SUPER_MOCK_CONFIG.status            = FLCN_OK;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult   = LW_FALSE;

    // Evaluate each set of test inputs
    for (lwrrentTestIdx = 0; lwrrentTestIdx < NUM_VFE_EQU_QUADRATIC_TEST_CASES; lwrrentTestIdx++)
    {
        // Zero out inputs
        memset(&vfeEquQuadratic, 0x0, sizeof(vfeEquQuadratic));
        memset(&vfeContext,      0x0, sizeof(vfeContext));
        memset(&result,          0x0, sizeof(result));

        // Setup coefficients
        vfeEquQuadratic.coeffs[2] = vfeEquQuadraticTestResults[lwrrentTestIdx].c2.floatRep;
        vfeEquQuadratic.coeffs[1] = vfeEquQuadraticTestResults[lwrrentTestIdx].c1.floatRep;
        vfeEquQuadratic.coeffs[0] = vfeEquQuadraticTestResults[lwrrentTestIdx].c0.floatRep;

        // Change result of vfeVarEvalByIdxFromCache to the independant variable
        vfeVarEvalByIdxFromCache_MOCK_CONFIG.result =
             vfeEquQuadraticTestResults[lwrrentTestIdx].var.floatRep;

        // Execute unit under test
        status = vfeEquEvalNode_QUADRATIC(&vfeContext, &vfeEquQuadratic.super, &result.floatRep);

        // Check outputs
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        EXPECT_VFE_EQU_QUADRATIC_RESULT(result, lwrrentTestIdx);
    }
}

UT_CASE_DEFINE(PMU_VFE_EQU_QUADRATIC, EvalNodeErrorPropagation,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Unit test for vfeEquEvalNode_QUADRATIC's error propagation.
 *
 * Simply Checks to see that the error propagation of dependent unit failures
 * behave as expected.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU_QUADRATIC.EvalNodeErrorPropagation
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * Test Setup:
 * @li Configure the VFE quadratic equation.
 * @li Set 1: Configure @ref vfeVarEvalByIdxFromCache() mock to return an error.
 * @li Set 2: configure @ref vfeEvalNode_SUPER() mock to return an error.
 *
 * Test Exelwtion:
 * @li Loop over the data sets, using the conditions specified. Expect the
 * return value specified by the data set. Reset the mock funtion after
 * exelwting a loop.
 */
UT_CASE_RUN(PMU_VFE_EQU_QUADRATIC, EvalNodeErrorPropagation)
{
    VFE_EQU_QUADRATIC   vfeEquQuadratic = { 0 };
    VFE_CONTEXT         vfeContext      = { 0 };
    LW_VFE_UINT_FLOAT   result          = { 0 };
    FLCN_STATUS         status          = FLCN_ERROR;

    // Setup coefficients
    vfeEquQuadratic.coeffs[2] = vfeEquQuadraticTestResults[0].c2.floatRep;
    vfeEquQuadratic.coeffs[1] = vfeEquQuadraticTestResults[0].c1.floatRep;
    vfeEquQuadratic.coeffs[0] = vfeEquQuadraticTestResults[0].c0.floatRep;

    // Change result of vfeVarEvalByIdxFromCache to the independant variable
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.result =
         vfeEquQuadraticTestResults[0].var.floatRep;

    // Ensure error Propagation works as expected for unit dependencies
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status             = FLCN_ERR_ILLEGAL_OPERATION;
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.bOverrideResult    = LW_FALSE;
    status = vfeEquEvalNode_QUADRATIC(&vfeContext, &vfeEquQuadratic.super, &result.floatRep);
    UT_ASSERT_EQUAL_UINT(status, vfeVarEvalByIdxFromCache_MOCK_CONFIG.status);
    vfeVarEvalByIdxFromCache_MOCK_CONFIG.status = FLCN_OK;

    vfeEvalNode_SUPER_MOCK_CONFIG.status            = FLCN_ERR_FEATURE_NOT_ENABLED;
    vfeEvalNode_SUPER_MOCK_CONFIG.bOverrideResult   = LW_FALSE;
    status = vfeEquEvalNode_QUADRATIC(&vfeContext, &vfeEquQuadratic.super, &result.floatRep);
    UT_ASSERT_EQUAL_UINT(status, vfeEvalNode_SUPER_MOCK_CONFIG.status);
    vfeEvalNode_SUPER_MOCK_CONFIG.status = FLCN_OK;
}

/* ------------------------ End of File ------------------------------------- */
