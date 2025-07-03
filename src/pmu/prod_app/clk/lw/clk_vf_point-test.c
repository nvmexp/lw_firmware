/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk_vf_point-test.c
 * @brief   Unit tests for logic in clk_vf_point.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobj-mock.h"
#include "pmu_objclk.h"
#include "clk/clk_vf_point.h"

/* ------------------------ Test Suite Declaration -------------------------- */
static void *testCaseSetup(void)
{
    boardObjConstructMockInit();
}

UT_SUITE_DEFINE(CLK_VF_POINT,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for VF points")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testCaseSetup))

/* ------------------------ clkVfPointGrpIfaceModel10ObjSet_SUPER() --------------------- */
UT_CASE_DEFINE(CLK_VF_POINT, Construct_BaseClassFailure,
                UT_CASE_SET_DESCRIPTION("Error constructing the base class.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT, Construct_BaseClassFailure)
{
    BOARDOBJGRP boardObjGrp = { 0 };
    CLK_VF_POINT vfPoint = { 0 };
    BOARDOBJ *pBoardObj = (BOARDOBJ*)&vfPoint;
    RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET boardObjDesc = { 0 };

    boardObjConstructMockAddEntry(0, FLCN_ERR_ILLEGAL_OPERATION);

    UT_ASSERT_EQUAL_UINT(
        clkVfPointGrpIfaceModel10ObjSet_SUPER(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_VF_POINT),
            (RM_PMU_BOARDOBJ*)&boardObjDesc),
        FLCN_ERR_ILLEGAL_OPERATION
    );
}

UT_CASE_DEFINE(CLK_VF_POINT, Construct_Success,
                UT_CASE_SET_DESCRIPTION("Success constructing base VF point.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT, Construct_Success)
{
    BOARDOBJGRP boardObjGrp = { 0 };
    CLK_VF_POINT vfPoint = { 0 };
    BOARDOBJ *pBoardObj = (BOARDOBJ*)&vfPoint;
    RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET boardObjDesc = { 0 };

    boardObjConstructMockAddEntry(0, FLCN_OK);

    UT_ASSERT_EQUAL_UINT(
        clkVfPointGrpIfaceModel10ObjSet_SUPER(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_VF_POINT),
            (RM_PMU_BOARDOBJ*)&boardObjDesc),
        FLCN_OK
    );
}
