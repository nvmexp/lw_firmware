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
#include "clk/clk_domain_35_primary.h"

// external variables that need to get their own header
extern OBJCLK Clk;
extern const RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET_UNION UT_ClockDomains[10];

static void* testSetup(void)
{
    callocMockInit();
    boardObjConstructMockInit();
    boardObjGrpMaskImportMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_DOMAIN_35_PRIMARY,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for secondary clock domains")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

/* ------------------------- clkDomainGrpIfaceModel10ObjSet_3X_FIXED() ------------------ */
UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, FirstConstructNoFreeMemForBoardObj,
                UT_CASE_SET_DESCRIPTION("Encounter no free memory during construction.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, FirstConstructNoFreeMemForBoardObj)
{
    const LwU8 clkDomIdx = 0;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, FirstConstructNoFreeMemForPORVoltageDeltas,
                UT_CASE_SET_DESCRIPTION("Encounter no free memory during construction.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, FirstConstructNoFreeMemForPORVoltageDeltas)
{
    const LwU8 clkDomIdx = 0;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelts[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, voltDelts, NULL);
    callocMockAddEntry(2, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 3);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkDomain);
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, FirstConstructNoFreeMemForVoltageDeltas,
                UT_CASE_SET_DESCRIPTION("Encounter no free memory during construction.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, FirstConstructNoFreeMemForVoltageDeltas)
{
    const LwU8 clkDomIdx = 0;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelta[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, NULL, NULL);
    callocMockAddEntry(2, porDelta, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 2);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkDomain);
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, FirstConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Encounter no free memory during construction.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, FirstConstructSuccess)
{
    const LwU8 clkDomIdx = 1;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelta[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, voltDelta, NULL);
    callocMockAddEntry(2, porDelta, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 3);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkDomain);

    // BOARDOBJ
    // Once the class data member is properly implemented, uncomment the following assert.
    //UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.super.super.class, LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.super.super.type, LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.super.super.grpIdx, clkDomIdx);
    // BOARDOBJ_VTABLE
    UT_ASSERT_NOT_EQUAL_PTR(clkDomain.super.super.super.super.super.pVirtualTable, NULL);
    // CLK_DOMAIN
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.domain, desc.super.super.super.super.domain);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.apiDomain, desc.super.super.super.super.apiDomain);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.super.perfDomainGrpIdx, desc.super.super.super.super.perfDomainGrpIdx);
    // CLK_DOMAIN_3X
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.super.bNoiseAwareCapable, desc.super.super.super.bNoiseAwareCapable);
    // CLK_DOMAIN_3X_PROG
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.clkProgIdxFirst, desc.super.super.clkProgIdxFirst);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.clkProgIdxLast, desc.super.super.clkProgIdxLast);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.bForceNoiseUnawareOrdering, desc.super.super.bForceNoiseUnawareOrdering);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.factoryDelta.type, desc.super.super.factoryDelta.type);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.factoryDelta.data.staticOffset.deltakHz, desc.super.super.factoryDelta.data.staticOffset.deltakHz);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.freqDeltaMinMHz, desc.super.super.freqDeltaMinMHz);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.freqDeltaMaxMHz, desc.super.super.freqDeltaMaxMHz);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.deltas.freqDelta.type, desc.super.super.deltas.freqDelta.type);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.super.deltas.freqDelta.data.staticOffset.deltakHz, desc.super.super.deltas.freqDelta.data.staticOffset.deltakHz);
    for (LwU8 i = 0; i < Clk.domains.voltRailsMax; ++i)
    {
        UT_ASSERT_EQUAL_UINT(clkDomain.super.super.deltas.pVoltDeltauV[i], desc.super.super.deltas.voltDeltauV[i]);
    }
    // CLK_DOMAIN_35_PROG
    UT_ASSERT_EQUAL_UINT(clkDomain.super.preVoltOrderingIndex, desc.super.preVoltOrderingIndex);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.postVoltOrderingIndex, desc.super.postVoltOrderingIndex);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkPos, desc.super.clkPos);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkVFLwrveCount, desc.super.clkVFLwrveCount);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkMonInfo.lowThresholdVfeIdx, desc.super.clkMonInfo.lowThresholdVfeIdx);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkMonInfo.highThresholdVfeIdx, desc.super.clkMonInfo.highThresholdVfeIdx);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkMonCtrl.flags, desc.super.clkMonCtrl.flags);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkMonCtrl.lowThresholdOverride, desc.super.clkMonCtrl.lowThresholdOverride);
    UT_ASSERT_EQUAL_UINT(clkDomain.super.clkMonCtrl.highThresholdOverride, desc.super.clkMonCtrl.highThresholdOverride);
    for (LwU8 i = 0; i < Clk.domains.voltRailsMax; ++i)
    {
        UT_ASSERT_EQUAL_UINT(clkDomain.super.pPorVoltDeltauV[i], desc.super.porVoltDeltauV[i]);
    }
    // CLK_DOMAIN_3X_PRIMARY
    UT_ASSERT_EQUAL_UINT(clkDomain.primary.secondaryIdxsMask.super.pData[0], desc.primary.secondaryIdxsMask.super.pData[0]);
    // CLK_DOMAIN_35_PRIMARY
    UT_ASSERT_EQUAL_UINT(clkDomain.primarySecondaryDomainsGrpMask.super.pData[0], desc.primarySecondaryDomainsGrpMask.super.pData[0]);
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, SubsequentConstructIlwalidType,
                UT_CASE_SET_DESCRIPTION("Attempting to update a different type")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, SubsequentConstructIlwalidType)
{
    const LwU8 clkDomIdx = 0;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelta[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, voltDelta, NULL);
    callocMockAddEntry(2, porDelta, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    desc.super.super.super.super.super.type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED;
    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERROR
    );
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, SubsequentConstructIlwalidElement,
                UT_CASE_SET_DESCRIPTION("Attempting to update with an invalid index")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, SubsequentConstructIlwalidElement)
{
    const LwU8 clkDomIdx = 0;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelta[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, voltDelta, NULL);
    callocMockAddEntry(2, porDelta, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    desc.super.super.super.super.super.grpIdx = clkDomIdx + 10;
    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, SubsequentConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Successful update to clock domain.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, SubsequentConstructSuccess)
{
    const LwU8 clkDomIdx = 0;
    const LwU32 newMask = 0x12345678;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelta[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, voltDelta, NULL);
    callocMockAddEntry(2, porDelta, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    desc.primarySecondaryDomainsGrpMask.super.pData[0] = newMask;
    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT(clkDomain.primarySecondaryDomainsGrpMask.super.pData[0], newMask);
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PRIMARY, IsFixed,
                UT_CASE_SET_DESCRIPTION("Tests if the clock is not fixed.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PRIMARY, IsFixed)
{
    const LwU8 clkDomIdx = 0;
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET desc = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomain = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LwS32       voltDelta[2];
    LwS32       porDelta[2];

    Clk.domains.voltRailsMax = 2;
    callocMockAddEntry(0, &clkDomain, NULL);
    callocMockAddEntry(1, voltDelta, NULL);
    callocMockAddEntry(2, porDelta, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    desc = UT_ClockDomains[clkDomIdx].v35Primary;

    UT_ASSERT_EQUAL_INT(
        clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_DOMAIN_35_PRIMARY),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_OK
    );
    UT_ASSERT_FALSE(clkDomainIsFixed_IMPL((CLK_DOMAIN *)&clkDomain));
}
