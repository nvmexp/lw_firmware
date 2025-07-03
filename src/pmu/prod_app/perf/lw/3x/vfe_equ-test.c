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
 * @file    vfe_equ-test.c
 * @brief   Unit tests for logic in VFE_EQU.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_equ_compare-mock.h"

/* ------------------------ Globals ---------------------------------------- */
OBJPERF     Perf        = { 0 };
VFE         VfeTemp     = { 0 };
VFE_EQUS    VfeEqusTemp = { 0 };

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to an equation base class
 *          evaluation and its expected result.
 */
typedef struct VFE_EQU_INPUT_OUTPUT
{
    LwF32 outRangeMin;  //!< Min bound of output.
    LwF32 outRangeMax;  //!< Max bound of output.
    LwF32 inputResult;  //!< Result passed as input to vfeEquEvalNode_SUPER.
    LwF32 outputResult; //!< Result of vfeEquEvalNode_SUPER.
} VFE_EQU_INPUT_OUTPUT;

/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of positive test conditions for EvalNodeLogic.
 */
static VFE_EQU_INPUT_OUTPUT vfeEquTestCases[] =
{
    /* 0000 */  { .outRangeMin = 0.0f, .outRangeMax = 0.0f,  .inputResult = 0.0f,  .outputResult = 0.0f },
    /* 0001 */  { .outRangeMin = 5.0f, .outRangeMax = 15.0f, .inputResult = 1.0f,  .outputResult = 5.0f },
    /* 0002 */  { .outRangeMin = 5.0f, .outRangeMax = 15.0f, .inputResult = 6.0f,  .outputResult = 6.0f },
    /* 0003 */  { .outRangeMin = 5.0f, .outRangeMax = 15.0f, .inputResult = 14.0f, .outputResult = 14.0f },
    /* 0004 */  { .outRangeMin = 5.0f, .outRangeMax = 15.0f, .inputResult = 16.0f, .outputResult = 15.0f },
};

/* ------------------------ Defines and Macros ------------------------------ */

/*!
 * @brief The test group for the VFE equations.
 *
 * @defgroup VFE_EQU_TEST_SUITE
 */
UT_SUITE_DEFINE(PMU_VFE_EQU,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO"))

/* ------------------------ Test Suite Implementation ----------------------- */
/*!
 * @brief Unit test for vfeEquEvalNode_SUPER's core logic.
 *
 * Iterates over testcases described in @ref vfeEquTestResults to simply
 * exercise the evaluation of compare equations.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU.EvalNodeLogic
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_TEST_SUITE
 *
 * Test Setup:
 * @li Create several data sets to test the value ranges.
 * @li Set #1: Min = 0.0, Max = 0.0, Input = 0.0, Output = 0.0
 * @li Set #2: Min = 5.0, Max = 15.0, Input = 1.0, Output = 5.0
 * @li Set #3: Min = 5.0, Max = 15.0, Input = 6.0, Output = 6.0
 * @li Set #4: Min = 5.0, Max = 15.0, Input = 14.0, Output = 14.0
 * @li Set #3: Min = 5.0, Max = 15.0, Input = 16.0, Output = 15.0
 *
 * Test Exelwtion:
 * @li Loop over the data sets, verifying the output falls in the range of
 * [Min, Max].
 */
UT_CASE_DEFINE(PMU_VFE_EQU, EvalNodeLogic,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU, EvalNodeLogic)
{
    VFE_CONTEXT     vfeContext      = { 0 };
    VFE_EQU         vfeEqu          = { 0 };
    LwF32           cachedResult    = 0;
    LwF32           result          = 0;
    LwU32           lwrrentTestIdx  = 0;
    FLCN_STATUS     status          = FLCN_ERROR;

    // Setup context
    boardObjGrpMaskInit_E255(&vfeContext.maskCacheValidEqus);
    vfeContext.pOutCacheEqus = &cachedResult;

    // Setup equation
    vfeEqu.super.grpIdx = 0;

    // Evaluate each set of test inputs
    for (lwrrentTestIdx = 0; lwrrentTestIdx < LW_ARRAY_ELEMENTS(vfeEquTestCases); lwrrentTestIdx++)
    {
        // Reset context
        vfeEquBoardObjMaskInit(&vfeContext.maskCacheValidEqus);
        vfeContext.pOutCacheEqus[vfeEqu.super.grpIdx] = 0.0f;

        // Setup equation
        vfeEqu.outRangeMin = vfeEquTestCases[lwrrentTestIdx].outRangeMin;
        vfeEqu.outRangeMax = vfeEquTestCases[lwrrentTestIdx].outRangeMax;

        // Setup result
        result = vfeEquTestCases[lwrrentTestIdx].inputResult;

        // Execute unit under test
        status = vfeEquEvalNode_SUPER_IMPL(&vfeContext, &vfeEqu, &result);

        // Expected sane return value
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

        // Expected evaluation result
        UT_ASSERT_TRUE(lwF32CmpEQ(result, vfeEquTestCases[lwrrentTestIdx].outputResult));

        // Expected cached result
        UT_ASSERT_TRUE(boardObjGrpMaskBitGet(&vfeContext.maskCacheValidEqus, vfeEqu.super.grpIdx));
        UT_ASSERT_EQUAL_UINT(vfeContext.pOutCacheEqus[vfeEqu.super.grpIdx], vfeEquTestCases[lwrrentTestIdx].outputResult);
    }
}

/*!
 * @brief Unit test for vfeEquEvalList's relwrsion depth logic.
 *
 * Tests to see that relwrsive calls do not exceed VFE_EQU_MAX_DEPTH.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU.EvalListRelwrsionDepth
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_TEST_SUITE
 *
 * Test Setup:
 * @li Create a comparison equation that calls itself when the comparison is
 * true.
 *
 * Test Exelwtion:
 * @li Call @ref vfeEquEvalList_IMPL() with a comparison that will evaluate to
 * true. Verify the function returns FLCN_ERR_RELWRSION_LIMIT_EXCEEDED.
 */
UT_CASE_DEFINE(PMU_VFE_EQU, EvalListRelwrsionDepth,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU, EvalListRelwrsionDepth)
{
    VFE_CONTEXT     vfeContext      = { 0 };
    VFE_EQU_COMPARE vfeEquCompare   = { 0 };
    LwF32           result          = 0;
    FLCN_STATUS     status          = FLCN_ERROR;
    BOARDOBJ       *objects[1];

    // Setup a VFE_EQU_COMPARE object that relwrsively compares against itself...
    vfeEquCompare.super.super.type      = LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE;
    vfeEquCompare.super.super.grpIdx    = 0;
    vfeEquCompare.super.equIdxNext      = PMU_PERF_VFE_EQU_INDEX_ILWALID;
    vfeEquCompare.equIdxTrue            = BOARDOBJ_GET_GRP_IDX_8BIT(&vfeEquCompare.super.super);

    // Setup OBJPERF with a valid VFE member
    Perf.pVfe = &VfeTemp;

    // Add VFE_EQUSE object to VFE_EQU boardobjgrp
    VfeTemp.pEqus = &VfeEqusTemp;

    // Setup VFE_EQUS
    VfeEqusTemp.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E255;
    VfeEqusTemp.super.super.objSlots    = 1;
    VfeEqusTemp.super.super.ppObjects   = objects;
    vfeEquBoardObjMaskInit(&VfeEqusTemp.super.objMask);
    boardObjGrpMaskBitSet(&VfeEqusTemp.super.objMask, BOARDOBJ_GET_GRP_IDX(&vfeEquCompare.super.super));

    // Insert vfe equation into group
    objects[0] = &vfeEquCompare.super.super;

    // Setup context to look at correct equations
    vfeContext.pVfeEqus = VfeTemp.pEqus;

    // Setup mocked function(s)
    vfeEquEvalNode_COMPARE_MOCK_CONFIG.bCallEvalList = LW_TRUE;

    // Execute unit under test
    status = vfeEquEvalList_IMPL(&vfeContext,
                BOARDOBJ_GET_GRP_IDX_8BIT(&vfeEquCompare.super.super),
                &result);

    // Expected return value
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_RELWRSION_LIMIT_EXCEEDED);

    // Teardown
    vfeEquEvalNode_COMPARE_MOCK_CONFIG_RESET(vfeEquEvalNode_COMPARE_MOCK_CONFIG);
    Perf.pVfe = NULL;
}

/*!
 * @brief Unit test for vfeEquEvalList's linked equation adding logic.
 *
 * Tests to see that linked equations are added together correctly.
 *
 * @verbatim
 * Test Case ID   : PMU_VFE_EQU.EvalListRelwrsionDepth
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VFE_EQU_TEST_SUITE
 *
 * Test Setup:
 * @li Create a list of two equations.
 * @li Mock the equation of each node to return 1.0.
 *
 * Test Exelwtion:
 * @li Call @ref vfeEquEvalList_IMPL(). Verify the function returns 2.0 (1.0
 * times the number of equations in the list).
 */
UT_CASE_DEFINE(PMU_VFE_EQU, EvalListAdd,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_VFE_EQU, EvalListAdd)
{
    VFE_CONTEXT     vfeContext      = { 0 };
    VFE_EQU_COMPARE vfeEquCompare   = { 0 };
    VFE_EQU_COMPARE vfeEquCompare2  = { 0 };
    LwF32           result          = 0;
    FLCN_STATUS     status          = FLCN_ERROR;
    BOARDOBJ       *objects[2];

    // Setup equations
    vfeEquCompare.super.super.type      = LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE;
    vfeEquCompare.super.super.grpIdx    = 0;
    vfeEquCompare.super.equIdxNext      = PMU_PERF_VFE_EQU_INDEX_ILWALID;

    vfeEquCompare2.super.super.type      = LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE;
    vfeEquCompare2.super.super.grpIdx    = 1;
    vfeEquCompare2.super.equIdxNext      = BOARDOBJ_GET_GRP_IDX_8BIT(&vfeEquCompare.super.super);

    // Setup OBJPERF with a valid VFE member
    Perf.pVfe = &VfeTemp;

    // Add VFE_EQUSE object to VFE_EQU boardobjgrp
    VfeTemp.pEqus = &VfeEqusTemp;

    // Setup VFE_EQUS
    VfeEqusTemp.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E255;
    VfeEqusTemp.super.super.objSlots    = 2;
    VfeEqusTemp.super.super.ppObjects   = objects;
    vfeEquBoardObjMaskInit(&VfeEqusTemp.super.objMask);
    boardObjGrpMaskBitSet(&VfeEqusTemp.super.objMask, BOARDOBJ_GET_GRP_IDX(&vfeEquCompare.super.super));
    boardObjGrpMaskBitSet(&VfeEqusTemp.super.objMask, BOARDOBJ_GET_GRP_IDX(&vfeEquCompare2.super.super));

    // Insert vfe equation into group
    objects[0] = &vfeEquCompare.super.super;
    objects[1] = &vfeEquCompare2.super.super;

    // Setup context to look at correct equations
    vfeContext.pVfeEqus = VfeTemp.pEqus;

    // Setup mocked function(s)
    vfeEquEvalNode_COMPARE_MOCK_CONFIG.status           = FLCN_OK;
    vfeEquEvalNode_COMPARE_MOCK_CONFIG.bOverrideResult  = LW_TRUE;
    vfeEquEvalNode_COMPARE_MOCK_CONFIG.result           = 1.0f;

    // Execute unit under test
    status = vfeEquEvalList_IMPL(&vfeContext,
                BOARDOBJ_GET_GRP_IDX_8BIT(&vfeEquCompare2.super.super),
                &result);

    // Expected return values
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_TRUE(lwF32CmpEQ(result, 2.0f));

    // Teardown
    vfeEquEvalNode_COMPARE_MOCK_CONFIG_RESET(vfeEquEvalNode_COMPARE_MOCK_CONFIG);
    Perf.pVfe = NULL;
}

/* ------------------------ End of File ------------------------------------- */
