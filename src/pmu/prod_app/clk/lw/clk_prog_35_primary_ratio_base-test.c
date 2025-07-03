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
 * @file    clk_prog_35_primary_ratio_base-test.c
 * @brief   Unit tests for logic in clk_prog_35_primary_ratio_base.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "perf/changeseq-mock.h"
#include "boardobj/boardobj-mock.h"
#include "pmu_objclk.h"
#include "clk/clk_prog_35_primary_ratio.h"

extern OBJCLK Clk;
extern const RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET_UNION UT_ClockProg[1];

static void *testSetup(void)
{
    memset(&Clk, 0x00, sizeof(Clk));
    callocMockInit();
    boardObjConstructMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_PROG_35_PRIMARY_RATIO,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for ratio-based primary clocks")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

/* ------------------------ clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO() -------------- */
UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForBoardObj,
                UT_CASE_SET_DESCRIPTION("Cannot allocate memory for an object.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForBoardObj)
{
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ *pBoardObj = NULL;
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    callocMockAddEntry(0, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForVfEntries,
                UT_CASE_SET_DESCRIPTION("Cannot allocate memory for an object.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForVfEntries)
{
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 2);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkProg);
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForVoltDelta,
                UT_CASE_SET_DESCRIPTION("Cannot allocate memory for an object.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForVoltDelta)
{
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[4];
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, vfEntries, NULL);
    callocMockAddEntry(2, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 3);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkProg);
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForSecondaryEntries,
                UT_CASE_SET_DESCRIPTION("Successfully construct an object.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, FirstConstructNoFreeMemoryForSecondaryEntries)
{
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[4];
    LwS32 voltDelta[4];
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, vfEntries, NULL);
    callocMockAddEntry(2, voltDelta, NULL);
    callocMockAddEntry(3, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 4);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkProg);
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, FirstConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Successfully construct an object.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, FirstConstructSuccess)
{
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[4];
    LwS32 voltDelta[4];
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY secondaryEntries[4];
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, vfEntries, NULL);
    callocMockAddEntry(2, voltDelta, NULL);
    callocMockAddEntry(3, secondaryEntries, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT8(callocMockNumCalls(), 4);
    UT_ASSERT_EQUAL_PTR(pBoardObj, &clkProg);

    // CLK_PROG/BOARDOBJ
    UT_ASSERT_EQUAL_UINT(clkProg.super.super.super.super.super.super.type, LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO);
    UT_ASSERT_EQUAL_UINT(clkProg.super.super.super.super.super.super.grpIdx, 0);
    // CLK_PROG_35_PRIMARY
    // CLK_PROG_3X
    UT_ASSERT_EQUAL_UINT(clkProg.super.super.super.source, desc.super.super.source);
    UT_ASSERT_EQUAL_UINT(clkProg.super.super.super.freqMaxMHz, desc.super.super.freqMaxMHz);
    UT_ASSERT_EQUAL_UINT(clkProg.super.super.super.sourceData.pll.pllIdx, desc.super.super.sourceData.pll.pllIdx);
    UT_ASSERT_EQUAL_UINT(clkProg.super.super.super.sourceData.pll.freqStepSizeMHz, desc.super.super.sourceData.pll.freqStepSizeMHz);
    // CLK_PROG_3X_PRIMARY
    UT_ASSERT_EQUAL_UINT(clkProg.super.primary.bOCOVEnabled, desc.super.primary.bOCOVEnabled);
    for (LwU8 i = 0; i < 4; ++i)
    {
        UT_ASSERT_EQUAL_UINT(clkProg.super.primary.pVfEntries[i].vfeIdx, desc.super.primary.vfEntries[i].vfeIdx);
        UT_ASSERT_EQUAL_UINT(clkProg.super.primary.pVfEntries[i].cpmMaxFreqOffsetVfeIdx, desc.super.primary.vfEntries[i].cpmMaxFreqOffsetVfeIdx);
        UT_ASSERT_EQUAL_UINT(clkProg.super.primary.pVfEntries[i].vfPointIdxFirst, desc.super.primary.vfEntries[i].vfPointIdxFirst);
        UT_ASSERT_EQUAL_UINT(clkProg.super.primary.pVfEntries[i].vfPointIdxLast, desc.super.primary.vfEntries[i].vfPointIdxLast);
    }
    UT_ASSERT_EQUAL_UINT(clkProg.super.primary.freqDelta.type, desc.super.primary.deltas.freqDelta.type);
    UT_ASSERT_EQUAL_UINT(clkProg.super.primary.freqDelta.data.staticOffset.deltakHz, desc.super.primary.deltas.freqDelta.data.staticOffset.deltakHz);
    for (LwU8 i = 0; i < 4; ++i)
    {
        UT_ASSERT_EQUAL_INT(clkProg.super.primary.pVoltDeltauV[i], desc.super.primary.deltas.voltDeltauV[i]);
    }
    UT_ASSERT_EQUAL_UINT(clkProg.super.primary.sourceData.nafll.baseVFSmoothVoltuV, desc.super.primary.sourceData.nafll.baseVFSmoothVoltuV);
    UT_ASSERT_EQUAL_UINT(clkProg.super.primary.sourceData.nafll.maxVFRampRate, desc.super.primary.sourceData.nafll.maxVFRampRate);
    UT_ASSERT_EQUAL_UINT(clkProg.super.primary.sourceData.nafll.maxFreqStepSizeMHz, desc.super.primary.sourceData.nafll.maxFreqStepSizeMHz);
    // CLK_PROG_35_PRIMARY_RATIO
    for (LwU8 i = 0; i < 4; ++i)
    {
        UT_ASSERT_EQUAL_UINT(clkProg.ratio.pSecondaryEntries[i].clkDomIdx, desc.ratio.secondaryEntries[i].clkDomIdx);
        UT_ASSERT_EQUAL_UINT(clkProg.ratio.pSecondaryEntries[i].ratio, desc.ratio.secondaryEntries[i].ratio);
    }
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, SubsequentConstructIlwalidType,
                UT_CASE_SET_DESCRIPTION("Set values with invalid type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, SubsequentConstructIlwalidType)
{
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[4];
    LwS32 voltDelta[4];
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY secondaryEntries[4];
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, vfEntries, NULL);
    callocMockAddEntry(2, voltDelta, NULL);
    callocMockAddEntry(3, secondaryEntries, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_OK
    );
    desc.super.super.super.super.type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED;
    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_ERROR
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, SubsequentConstructIlwalidIndex,
                UT_CASE_SET_DESCRIPTION("Set values with invalid index.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, SubsequentConstructIlwalidIndex)
{
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[4];
    LwS32 voltDelta[4];
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY secondaryEntries[4];
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, vfEntries, NULL);
    callocMockAddEntry(2, voltDelta, NULL);
    callocMockAddEntry(3, secondaryEntries, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_OK
    );
    desc.super.super.super.super.grpIdx += 2;
    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_PROG_35_PRIMARY_RATIO, SubsequentConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Successfully update clock prog.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_35_PRIMARY_RATIO, SubsequentConstructSuccess)
{
    const LwU8 newRatios[] = { 94, 98, 89, 85 };
    CLK_PROG_35_PRIMARY_RATIO clkProg = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY vfEntries[4];
    LwS32 voltDelta[4];
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY secondaryEntries[4];
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET desc =
        UT_ClockProg[0].v35PrimaryRatio;

    Clk.progs.vfEntryCount = 4;
    Clk.progs.secondaryEntryCount = 4;
    callocMockAddEntry(0, &clkProg, NULL);
    callocMockAddEntry(1, vfEntries, NULL);
    callocMockAddEntry(2, voltDelta, NULL);
    callocMockAddEntry(3, secondaryEntries, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_OK
    );

    desc.ratio.secondaryEntries[0].ratio = newRatios[0];
    desc.ratio.secondaryEntries[1].ratio = newRatios[1];
    desc.ratio.secondaryEntries[2].ratio = newRatios[2];
    desc.ratio.secondaryEntries[3].ratio = newRatios[3];

    UT_ASSERT_EQUAL_UINT(
        clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_PROG_35_PRIMARY_RATIO),
            (RM_PMU_BOARDOBJ *)&desc
        ),
        FLCN_OK
    );

    UT_ASSERT_EQUAL_UINT(clkProg.ratio.pSecondaryEntries[0].ratio, newRatios[0]);
    UT_ASSERT_EQUAL_UINT(clkProg.ratio.pSecondaryEntries[1].ratio, newRatios[1]);
    UT_ASSERT_EQUAL_UINT(clkProg.ratio.pSecondaryEntries[2].ratio, newRatios[2]);
    UT_ASSERT_EQUAL_UINT(clkProg.ratio.pSecondaryEntries[3].ratio, newRatios[3]);
}
