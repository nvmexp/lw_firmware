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
 * @file    clk_vf_point_35_freq-test.c
 * @brief   Unit tests for logic in clk_vf_point_35_freq.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objclk.h"
#include "clk/clk_vf_point_35_freq.h"
#include "clk/clk_vf_point_35-mock.h"

extern OBJCLK Clk;

static void *testCaseSetup(void)
{
    memset(&Clk, 0x00, sizeof(Clk));
    Clk.domains.bEnforceVfSmoothening = LW_TRUE;
    Clk.domains.bEnforceVfMonotonicity = LW_TRUE;
    Clk.progs.secondaryEntryCount = 2;

    clkVfPointConstruct_35MockInit();
    //boardObjDynamicCastMockInit();
}

UT_SUITE_DEFINE(CLK_VF_POINT_35_FREQ,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for VF points 3.5 frequency")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testCaseSetup))


/* ------------------------ clkVfPoint35Smoothing_FREQ() -------------------- */
UT_CASE_DEFINE(CLK_VF_POINT_35_FREQ, Smoothing_NullTuple,
                UT_CASE_SET_DESCRIPTION("Base tuple is not present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35_FREQ, Smoothing_NullTuple)
{
    CLK_VF_POINT_35 vfPoint = { 0 };
    CLK_PROG_35_PRIMARY progPrimary = { 0 };

    vfPoint.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_UNKNOWN;

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35Smoothing_FREQ(&vfPoint, NULL, NULL, &progPrimary, 0),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

/* TODO: Failing after we switched from tu_10a to tu10x. Unit owner should fix this.

UT_CASE_DEFINE(CLK_VF_POINT_35_FREQ, Smoothing_NotSupported,
                UT_CASE_SET_DESCRIPTION("Smoothing is not supported.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35_FREQ, Smoothing_NotSupported)
{
    //
    // We will never hit the FLCN_ERR_NOT_SUPPORTED case in automotive, as
    // PMU_CLK_CLK_DOMAIN_VF_SMOOTHENING_SUPPORTED feature is not enabled
    // on automotive. If it does, assert that FLCN_ERR_NOT_SUPPORTED is returned.
    //
    CLK_VF_POINT_35_FREQ vfPoint = { 0 };
    CLK_PROG_35_PRIMARY progPrimary = { 0 };

    vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ;
    vfPoint.baseVFTuple.voltageuV = 100;
    progPrimary.super.super.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
    progPrimary.primary.sourceData.nafll.maxVFRampRate = 1000;
    progPrimary.primary.sourceData.nafll.baseVFSmoothVoltuV = 100;

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35Smoothing_FREQ(&vfPoint, NULL, NULL, &progPrimary, 0),
        FLCN_OK
    );
}
*/
UT_CASE_DEFINE(CLK_VF_POINT_35_FREQ, Smoothing_Success,
                UT_CASE_SET_DESCRIPTION("Success.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35_FREQ, Smoothing_Success)
{
    CLK_VF_POINT_35_FREQ vfPoint = { 0 };
    CLK_PROG_35_PRIMARY progPrimary = { 0 };

    vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ;
    vfPoint.baseVFTuple.voltageuV = 100;
    progPrimary.super.super.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL;
    progPrimary.primary.sourceData.nafll.maxVFRampRate = 1000;
    progPrimary.primary.sourceData.nafll.baseVFSmoothVoltuV = 100;

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35Smoothing_FREQ(&vfPoint, NULL, NULL, &progPrimary, 0),
        FLCN_OK
    );
}

UT_CASE_DEFINE(CLK_VF_POINT_35_FREQ, Construct_BaseClassFailure,
                UT_CASE_SET_DESCRIPTION("Failure constructing base class.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35_FREQ, Construct_BaseClassFailure)
{
    // TODO-aherring: make sure that this works with the changes to an interface
    BOARDOBJGRP_IFACE_MODEL_10  model10  = { 0 };
    CLK_VF_POINT_35_FREQ vfPoint = { 0 };
    BOARDOBJ *pBoardObj;
    RM_PMU_CLK_CLK_VF_POINT_35_FREQ_BOARDOBJ_SET desc = { 0 };

    clkVfPointConstruct_35MockAddEntry(0, FLCN_ERR_ILLEGAL_OPERATION, NULL);
    UT_ASSERT_EQUAL_UINT(
        clkVfPointGrpIfaceModel10ObjSet_35_FREQ(&model10, &pBoardObj, sizeof(vfPoint), &desc),
        FLCN_ERR_ILLEGAL_OPERATION
    );
}

// UT_CASE_DEFINE(CLK_VF_POINT_35_FREQ, Construct_BadCast,
//                 UT_CASE_SET_DESCRIPTION("Handle a bad dynamic cast.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(CLK_VF_POINT_35_FREQ, Construct_BadCast)
// {
//     // TODO-aherring: make sure that this works with the changes to an interface
//     BOARDOBJGRP_IFACE_MODEL_10  model10  = { 0 };
//     CLK_VF_POINT_35_FREQ vfPoint = { 0 };
//     BOARDOBJ *pBoardObj;
//     RM_PMU_CLK_CLK_VF_POINT_35_FREQ_BOARDOBJ_SET desc = { 0 };

//     clkVfPointConstruct_35MockAddEntry(0, FLCN_OK, (BOARDOBJ*)&vfPoint);
//     boardObjDynamicCastMockAddEntry(0, NULL);
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_FREQ(
//             &model10,
//             &pBoardObj,
//             sizeof(vfPoint),
//             (RM_PMU_BOARDOBJ*)&desc),
//         FLCN_ERR_ILWALID_CAST
//     );
// }

// UT_CASE_DEFINE(CLK_VF_POINT_35_FREQ, Construct_Success,
//                 UT_CASE_SET_DESCRIPTION("Success constructing.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(CLK_VF_POINT_35_FREQ, Construct_Success)
// {
//     // TODO-aherring: make sure that this works with the changes to an interface
//     BOARDOBJGRP_IFACE_MODEL_10  model10  = { 0 };
//     CLK_VF_POINT_35_FREQ vfPoint = { 0 };
//     BOARDOBJ *pBoardObj;
//     RM_PMU_CLK_CLK_VF_POINT_35_FREQ_BOARDOBJ_SET desc = { 0 };

//     desc.freqMHz = 1100;
//     desc.voltDeltauV = 100;

//     clkVfPointConstruct_35MockAddEntry(0, FLCN_OK, (BOARDOBJ*)&vfPoint);
//     vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ;
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint);
//     clkVfPointConstruct_35MockAddEntry(0, FLCN_OK, &vfPoint);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_FREQ(&model10, &pBoardObj, sizeof(vfPoint), &desc),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(vfPoint.voltDeltauV, 100);
//     for (LwU8 i = 0; i < Clk.progs.secondaryEntryCount + 1U; ++i)
//     {
//         UT_ASSERT_EQUAL_UINT(vfPoint.baseVFTuple.freqTuple[i].freqMHz, 1100);
//     }
//     for (LwU8 j = Clk.progs.secondaryEntryCount + 1U; j < LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE; ++j)
//     {
//         UT_ASSERT_EQUAL_UINT(vfPoint.baseVFTuple.freqTuple[j].freqMHz, 0);
//     }
// }
