/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq-test.c
 * @brief   Unit tests for logic in CHANGESEQ.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objclk.h"
#include "clk/clk_domain.h"
#include "clk/clk_clockmon-mock.h"
#include "perf/changeseq.h"

extern OBJCLK Clk;

/* ------------------------ Function Prototypes ----------------------------- */
static void *buildTestCaseSetup(void);
static void initializeClockDomains(CLK_DOMAIN *pClkDomains, LwU8 count);
static void initializeClockDomainBoardObjPtrs(BOARDOBJ **ppBoardObj, CLK_DOMAIN *pClkDomains, LwU8 count);
static void initializeObjClk(BOARDOBJ **ppBoardObj);
static void initializeChangeSeq(CHANGE_SEQ *pChangeSeq);
static void initializeChangeSeqScript(CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED *pStepLwrr);
static void initializeClkMonListItem(LW2080_CTRL_CLK_DOMAIN_CLK_MON_LIST_ITEM *pClkMonListItem, LwU32 clkApiDomain, LwU32 clkFreqMHz, LwUFXP20_12 lowThresholdPercent, LwUFXP20_12 highThresholdPercent);

/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief The test group for the Clock Monitor step.
 *
 * @defgroup CHANGESEQSCRIPTSTEPBUILD_CLKMON_TEST_SUITE
 */
UT_SUITE_DEFINE(CHANGESEQSCRIPTSTEPBUILD_CLKMON,
                UT_SUITE_SET_COMPONENT("Change Sequencer Script CLK_MON Step")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(buildTestCaseSetup))

/*!
 * @brief Test the case where an error is returned while evaluating the
 * clock monitor thresholds.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQSCRIPTSTEPBUILD_CLKMON.ErrorEvaluatingThreshold
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQSCRIPTSTEPBUILD_CLKMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the mock to @ref clkClockMonsThresholdEvaluate() to return a
 * non-FLCN_OK value.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeq35ScriptBuildStep_CLK_MON(). Expect the
 * function to return the value returned by the mocked version of
 * @ref clkClockMonsThresholdEvaluate().
 */
UT_CASE_DEFINE(CHANGESEQSCRIPTSTEPBUILD_CLKMON, ErrorEvaluatingThreshold,
                UT_CASE_SET_DESCRIPTION("Error returned while evaluating the threshold.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQSCRIPTSTEPBUILD_CLKMON, ErrorEvaluatingThreshold)
{
    const FLCN_STATUS expectedStatus = FLCN_ERR_ILWALID_ARGUMENT;

    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script = { 0 };
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange = { 0 };
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange = { 0 };

    initializeChangeSeq(&changeSeq);
    clkClockMonsThresholdEvaluateMockAddEntry(0, expectedStatus);

    UT_ASSERT_EQUAL_UINT(
        perfDaemonChangeSeq35ScriptBuildStep_CLK_MON_IMPL(
            &changeSeq,
            &script,
            &lwrrentChange,
            &lastChange),
        expectedStatus);
}

/*!
 * @brief Test the case where the clock domain sanity check failed.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQSCRIPTSTEPBUILD_CLKMON.FailClockDomainSanityCheck
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQSCRIPTSTEPBUILD_CLKMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the mock to @ref clkClockMonsThresholdEvaluate() to return
 * FLCN_OK.
 * @li Configure the clock domains to have a single clock domain. Configure the
 * clock domains to have a domain of LW2080_CTRL_CLK_DOMAIN_UNDEFINED, except
 * have one have a different clock domain value
 * (e.g. LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK).
 *
 * Test Exelwtion:
 * Call @ref perfDaemonChangeSeq35ScriptBuildStep_CLK_MON(). Expect the function
 * to return FLCN_ERR_ILWALID_INDEX.
 */
// UT_CASE_DEFINE(CHANGESEQSCRIPTSTEPBUILD_CLKMON, FailClockDomainSanityCheck,
//                 UT_CASE_SET_DESCRIPTION("Fail the clock domain sanity check.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(CHANGESEQSCRIPTSTEPBUILD_CLKMON, FailClockDomainSanityCheck)
// {
//     const LwU8 numberOfClockDomains = 4;

//     CHANGE_SEQ changeSeq;
//     CHANGE_SEQ_SCRIPT script = { 0 };
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange = { 0 };
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange = { 0 };
//     CLK_DOMAIN clkDomains[numberOfClockDomains];
//     BOARDOBJ *pClkDomains[32];

//     initializeChangeSeq(&changeSeq);
//     initializeClockDomains(clkDomains, numberOfClockDomains);
//     clkDomains[numberOfClockDomains / 2].apiDomain = LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK;
//     initializeClockDomainBoardObjPtrs(pClkDomains, clkDomains, numberOfClockDomains);
//     initializeObjClk((BOARDOBJ**)&pClkDomains);
//     clkClockMonsThresholdEvaluateMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(
//         perfDaemonChangeSeq35ScriptBuildStep_CLK_MON_IMPL(
//             &changeSeq,
//             &script,
//             &lwrrentChange,
//             &lastChange),
//         FLCN_ERR_ILWALID_INDEX);
// }

/* ------------------------ Private Functions ------------------------------- */
static void *buildTestCaseSetup(void)
{
    clkClockMonsThresholdEvaluateMockInit();
    return NULL;
}

static void initializeClockDomains(CLK_DOMAIN *pClkDomains, LwU8 count)
{
    UT_ASSERT_TRUE(count <= 32);
    for (LwU8 i = 0; i < count; ++i)
    {
        pClkDomains[i].domain = clkWhich_GpcClk + i;
        pClkDomains[i].apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK + i;
        pClkDomains[i].perfDomainGrpIdx = 0;
    }
}

static void initializeClockDomainBoardObjPtrs(BOARDOBJ **ppBoardObj, CLK_DOMAIN *pClkDomains, LwU8 count)
{
    UT_ASSERT_TRUE(count <= 32);
    for (LwU8 i = 0; i < count; ++i)
    {
        ppBoardObj[i] = (BOARDOBJ*)&pClkDomains[i];
    }
}

static void initializeObjClk(BOARDOBJ **ppBoardObj)
{
    memset(&Clk, 0x00, sizeof(Clk));

    Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
    Clk.domains.super.super.objSlots = 32;
    Clk.domains.super.super.ovlIdxDmem = 3;
    Clk.domains.super.super.bConstructed = LW_TRUE;
    Clk.domains.super.super.ppObjects = ppBoardObj;
    boardObjGrpMaskInit_E32(&Clk.domains.super.objMask);
    for (LwU8 i = 0; i < 32; ++i)
    {
        if (ppBoardObj[i] != NULL)
        {
            boardObjGrpMaskBitSet(&Clk.domains.super.objMask, i);
        }
    }
}

static void initializeChangeSeq(CHANGE_SEQ *pChangeSeq)
{
    memset(pChangeSeq, 0x00, sizeof(*pChangeSeq));
}

static void initializeChangeSeqScript(CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED *pStepLwrr)
{
    memset(pScript, 0x00, sizeof(*pScript));
    memset(pStepLwrr, 0x00, sizeof(*pStepLwrr));

    boardObjGrpMaskInit_E32(&pScript->clkDomainsActiveMask);
    boardObjGrpMaskBitSet(&pScript->clkDomainsActiveMask, 0);
    boardObjGrpMaskBitSet(&pScript->clkDomainsActiveMask, 1);
    boardObjGrpMaskBitSet(&pScript->clkDomainsActiveMask, 3);
    boardObjGrpMaskBitSet(&pScript->clkDomainsActiveMask, 4);

    pScript->pStepLwrr = pStepLwrr;

    pScript->pStepLwrr->data.clkMon.super.stepId = LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_CLK_MON;
    pScript->pStepLwrr->data.clkMon.clkMonList.numDomains = 4;
    for (LwU8 i = 0; i < pScript->pStepLwrr->data.clkMon.clkMonList.numDomains; ++i)
    {
        initializeClkMonListItem(
            &pScript->pStepLwrr->data.clkMon.clkMonList.clkDomains[i],
            LW2080_CTRL_CLK_DOMAIN_GPCCLK + i,
            1000,
            LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 2),
            LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 5));
    }
}

static void initializeClkMonListItem
(
    LW2080_CTRL_CLK_DOMAIN_CLK_MON_LIST_ITEM *pClkMonListItem,
    LwU32 clkApiDomain,
    LwU32 clkFreqMHz,
    LwUFXP20_12 lowThresholdPercent,
    LwUFXP20_12 highThresholdPercent
)
{
    pClkMonListItem->clkApiDomain = clkApiDomain;
    pClkMonListItem->clkFreqMHz = clkFreqMHz;
    pClkMonListItem->lowThresholdPercent = lowThresholdPercent;
    pClkMonListItem->highThresholdPercent = highThresholdPercent;
}
