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
 * @file    clk_domain_3x_fixed-test.c
 * @brief   Unit tests for logic in clk_domain_3x_fixed.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj-mock.h"
#include "perf/changeseq-mock.h" // for CALLOC_MOCK_EXPECTED_VALUE
#include "pmu_objclk.h"
#include "clk/clk_domain_3x_fixed.h"

// external variables that need to get their own header
extern const RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET_UNION UT_ClockDomains[10];

static void* testSetup(void)
{
    callocMockInit();
    boardObjConstructMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_DOMAIN_3X_FIXED,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for fixed clock domains")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

/* ------------------------- clkDomainGrpIfaceModel10ObjSet_3X_FIXED() ------------------ */
UT_CASE_DEFINE(CLK_DOMAIN_3X_FIXED, FirstConstructNoFreeMem,
                UT_CASE_SET_DESCRIPTION("Encounter no free memory during construction.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_3X_FIXED, FirstConstructNoFreeMem)
{
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET desc = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[2].v3xFixed;

    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_PTR(pBoardObj, NULL);
}

UT_CASE_DEFINE(CLK_DOMAIN_3X_FIXED, FirstConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Success initially constructing clock domain.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_3X_FIXED, FirstConstructSuccess)
{
    const LwU8 clkDomIdx = 2;
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_3X_FIXED clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, &clkDomain, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v3xFixed;

    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );

    // BOARDOBJ
    // Once the class data member is properly implemented, uncomment the following assert.
    //UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.class, LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.type, LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.grpIdx, clkDomIdx);
    // BOARDOBJ_VTABLE
    UT_ASSERT_EQUAL_PTR(clkDomain.super.super.super.pVirtualTable, NULL);
    // CLK_DOMAIN
    UT_ASSERT_EQUAL_UINT32(clkDomain.super.super.domain, desc.super.super.domain);
    UT_ASSERT_EQUAL_UINT32(clkDomain.super.super.apiDomain, desc.super.super.apiDomain);
    UT_ASSERT_EQUAL_UINT8(clkDomain.super.super.perfDomainGrpIdx, desc.super.super.perfDomainGrpIdx);
    // CLK_DOMAIN_3X
    UT_ASSERT_EQUAL_UINT8(clkDomain.super.bNoiseAwareCapable, desc.super.bNoiseAwareCapable);
    // CLK_DOMAIN_3X_FIXED
    UT_ASSERT_EQUAL_UINT32(clkDomain.freqMHz, desc.freqMHz);
}

UT_CASE_DEFINE(CLK_DOMAIN_3X_FIXED, SubsequentConstructIlwalidType,
                UT_CASE_SET_DESCRIPTION("Attempting to update a different type")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_3X_FIXED, SubsequentConstructIlwalidType)
{
    const LwU8 clkDomIdx = 2;
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_3X_FIXED clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, &clkDomain, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v3xFixed;

    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    desc.super.super.super.type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY;
    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERROR
    );
}

UT_CASE_DEFINE(CLK_DOMAIN_3X_FIXED, SubsequentConstructIlwalidElement,
                UT_CASE_SET_DESCRIPTION("Attempting to update with an invalid index")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_3X_FIXED, SubsequentConstructIlwalidElement)
{
    const LwU8 clkDomIdx = 2;
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_3X_FIXED clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, &clkDomain, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v3xFixed;

    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    desc.super.super.super.grpIdx = clkDomIdx + 1;
    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_DOMAIN_3X_FIXED, SubsequentConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Successful update to clock domain.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_3X_FIXED, SubsequentConstructSuccess)
{
    const LwU8 clkDomIdx = 2;
    const LwU32 newFreqMHz = 5005;
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_3X_FIXED clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, &clkDomain, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v3xFixed;

    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    desc.freqMHz = newFreqMHz;
    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT(clkDomain.freqMHz, newFreqMHz);
}

UT_CASE_DEFINE(CLK_DOMAIN_3X_FIXED, IsFixed,
                UT_CASE_SET_DESCRIPTION("Tests if the clock is fixed.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_3X_FIXED, IsFixed)
{
    const LwU8 clkDomIdx = 2;
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_3X_FIXED clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, &clkDomain, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v3xFixed;

    UT_ASSERT_EQUAL_UINT(
        clkDomainGrpIfaceModel10ObjSet_3X_FIXED(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_3X_FIXED),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    UT_ASSERT_TRUE(clkDomainIsFixed_IMPL((CLK_DOMAIN *)&clkDomain));
}
