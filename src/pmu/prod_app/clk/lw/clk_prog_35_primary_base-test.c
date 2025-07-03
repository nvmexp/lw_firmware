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
 * @file    clk_prog_35_base-test.c
 * @brief   Unit tests for logic in clk_prog_35_base.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objclk.h"
#include "clk/clk_prog_35_primary.h"
#include "clk/clk_vf_point_35-mock.h"

extern OBJCLK Clk;

static void *testCaseSetup(void)
{
    clkVfPoint35CacheMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_PROG_35_PRIMARY_BASE,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for base ClK Prog Primary 3.5")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testCaseSetup))

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, LoadIlwalidLwrveIndex,
                UT_CASE_SET_DESCRIPTION("Invalid lwrve index passed in.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, LoadIlwalidLwrveIndex)
{
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };

    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryLoad(
            &clkProg,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, LoadFirstIndexIlwalid,
                UT_CASE_SET_DESCRIPTION("The first VF index is not valid.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, LoadFirstIndexIlwalid)
{
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2] = { 0 };

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxFirst = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;

    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryLoad(
            &clkProg,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, LoadLastIndexIlwalid,
                UT_CASE_SET_DESCRIPTION("The last VF index is not valid.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, LoadLastIndexIlwalid)
{
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2] = { 0 };

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxLast = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;

    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryLoad(
            &clkProg,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, LoadNullVfPoint,
                UT_CASE_SET_DESCRIPTION("Bad index to VF point.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, LoadNullVfPoint)
{
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2] = { 0 };

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxFirst = 0;
    clkProg.primary.pVfEntries[0].vfPointIdxLast = 0;


    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryLoad(
            &clkProg,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, CacheIlwalidLwrveIndex,
                UT_CASE_SET_DESCRIPTION("Invalid lwrve index.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, CacheIlwalidLwrveIndex)
{
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    CLK_VF_POINT_35 vfPt = { 0 };

    UT_ASSERT_EQUAL_INT(
        clkProg35PrimaryCache(
            &clkProg,
            &clkDomain,
            0,
            LW_FALSE,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID,
            &vfPt
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, CacheIlwalidVFIndex,
                UT_CASE_SET_DESCRIPTION("Invalid VF point index.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, CacheIlwalidVFIndex)
{
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2];
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    CLK_VF_POINT_35 vfPt = { 0 };

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxFirst = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;
    clkProg.primary.pVfEntries[0].vfPointIdxLast  = 0;
    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryCache(
            &clkProg,
            &clkDomain,
            0,
            LW_FALSE,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            &vfPt
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );

    clkProg.primary.pVfEntries[0].vfPointIdxFirst = 0;
    clkProg.primary.pVfEntries[0].vfPointIdxLast  = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;
    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryCache(
            &clkProg,
            &clkDomain,
            0,
            LW_FALSE,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            &vfPt
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

static void initializeVfPointsGrp(BOARDOBJGRP_E255 *pBoardObjGrp, BOARDOBJ **pObjects)
{
    pBoardObjGrp->super.type         = LW2080_CTRL_BOARDOBJGRP_TYPE_E255;
    pBoardObjGrp->super.classId      = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_VF_POINT;
    pBoardObjGrp->super.objSlots     = LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS;
    pBoardObjGrp->super.ovlIdxDmem   = 0;
    pBoardObjGrp->super.bConstructed = LW_TRUE;
    pBoardObjGrp->super.ppObjects    = pObjects;

    boardObjGrpMaskInit_E255(&pBoardObjGrp->objMask);
    for (int i = 0; i < 255; ++i)
    {
        if (pObjects[i] != NULL)
        {
            boardObjGrpMaskBitSet(&pBoardObjGrp->objMask, i);
        }
    }
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, CacheFailureGettingVfPoint,
                UT_CASE_SET_DESCRIPTION("Error returned obtaining a VF point.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, CacheFailureGettingVfPoint)
{
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2];
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    CLK_VF_POINT_35 vfPoints[2] = { 0 };
    CLK_VF_POINT_35 vfPt = { 0 };
    BOARDOBJ *pObjects[255];

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxFirst = 0;
    clkProg.primary.pVfEntries[0].vfPointIdxLast  = 0;

    pObjects[1] = (BOARDOBJ*)&vfPoints[1];
    initializeVfPointsGrp(&Clk.vfPoints.super, pObjects);

    UT_ASSERT_EQUAL_PTR(
        CLK_VF_POINT_GET_BY_LWRVE_ID(
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            0
        ),
        NULL
    );
    UT_ASSERT_EQUAL_PTR(
        CLK_VF_POINT_GET_BY_LWRVE_ID(
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            1
        ),
        &vfPoints[1]
    );
    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryCache(
            &clkProg,
            &clkDomain,
            0,
            LW_FALSE,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            &vfPt
        ),
        FLCN_ERR_ILWALID_INDEX
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, CacheFailureCachingVfPoint,
                UT_CASE_SET_DESCRIPTION("Error returned caching a VF point.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, CacheFailureCachingVfPoint)
{
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2];
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    CLK_VF_POINT_35 vfPoints[2] = { 0 };
    CLK_VF_POINT_35 vfPt = { 0 };
    BOARDOBJ *pObjects[255];

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxFirst = 0;
    clkProg.primary.pVfEntries[0].vfPointIdxLast  = 0;

    pObjects[0] = (BOARDOBJ*)&vfPoints[0];
    pObjects[1] = (BOARDOBJ*)&vfPoints[1];
    initializeVfPointsGrp(&Clk.vfPoints.super, pObjects);

    clkVfPoint35CacheMockAddEntry(0, FLCN_ERR_LOCK_NOT_AVAILABLE);

    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryCache(
            &clkProg,
            &clkDomain,
            0,
            LW_FALSE,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            &vfPt
        ),
        FLCN_ERR_LOCK_NOT_AVAILABLE
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_BASE, CacheSuccessful,
                UT_CASE_SET_DESCRIPTION("Successfully cached.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_BASE, CacheSuccessful)
{
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[2];
    CLK_PROG_35_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    CLK_VF_POINT_35 vfPoints[4] = { 0 };
    CLK_VF_POINT_35 vfPt = { 0 };
    BOARDOBJ *pObjects[255];

    clkProg.primary.pVfEntries = vfEntries;
    clkProg.primary.pVfEntries[0].vfPointIdxFirst = 0;
    clkProg.primary.pVfEntries[0].vfPointIdxLast  = 3;

    for (int i = 0; i < 4; i++)
    {
        pObjects[i] = (BOARDOBJ*)&vfPoints[i];
        clkVfPoint35CacheMockAddEntry(i, FLCN_OK);
    }
    initializeVfPointsGrp(&Clk.vfPoints.super, pObjects);

    UT_ASSERT_EQUAL_UINT(
        clkProg35PrimaryCache(
            &clkProg,
            &clkDomain,
            0,
            LW_FALSE,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            &vfPt
        ),
        FLCN_OK
    );
}
