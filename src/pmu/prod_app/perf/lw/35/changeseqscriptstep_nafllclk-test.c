/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep_nafllclk-test.c
 * @brief   Unit tests for logic in LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "perf/changeseq.h"
#include "clk/pmu_clknafll_regime_10-mock.h"

/* ------------------------ Function Prototypes ----------------------------- */
static void *testCaseSetup(void);
static void initializeStep(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pStep);
static void addClockDomain(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pStep, LwU32 clkDomain, LwU8 clkProgSource);

/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief The test group for the NAFLL clocks step.
 *
 * @defgroup CHANGESEQSCRIPTSTEP_NAFLLCLKS_TEST_SUITE
 */
UT_SUITE_DEFINE(CHANGESEQSCRIPTSTEP_NAFLLCLKS,
                UT_SUITE_SET_COMPONENT("Change Sequencer Script Step (NAFLL CLKS)")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("Jeff Hutchins"))

/*!
 * @brief Test the case where one of the clock domains does not have an NAFLL
 * clock source.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQSCRIPTSTEP_NAFLLCLKS.IlwalidSource
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQSCRIPTSTEP_NAFLLCLKS_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure all but one clock domain to have a NAFLL source. For one of
 * them, set the source to be a PLL.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_IMPL().
 * Expect the return value to be FLCN_ERR_NOT_SUPPORTED.
 */
UT_CASE_DEFINE(CHANGESEQSCRIPTSTEP_NAFLLCLKS, IlwalidSource,
                UT_CASE_SET_DESCRIPTION("An invalid CLK_PROG source was found")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQSCRIPTSTEP_NAFLLCLKS, IlwalidSource)
{
    CHANGE_SEQ changeSeq = { 0 };
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS step = { 0 };

    initializeStep(&step);
    addClockDomain(&step, LW2080_CTRL_CLK_DOMAIN_GPCCLK, LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL);
    addClockDomain(&step, LW2080_CTRL_CLK_DOMAIN_XBARCLK, LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL);
    addClockDomain(&step, LW2080_CTRL_CLK_DOMAIN_SYSCLK, LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL);

    UT_ASSERT_EQUAL_UINT(
        perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_IMPL(
            &changeSeq,
            (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER*)&step
        ),
        FLCN_ERR_NOT_SUPPORTED
    );
}

/*!
 * @brief Test the case where an error was encountered during programming.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQSCRIPTSTEP_NAFLLCLKS.ErrorProgramming
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQSCRIPTSTEP_NAFLLCLKS_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure all but one clock domain to have a NAFLL source.
 * @li Configure the mocked version of @ref clkAdcsProgram() to return a
 * non-FLCN_OK value.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_IMPL().
 * Expect the return value to be the value returned from the mocked version
 * of clkAdcsProgram().
 */
UT_CASE_DEFINE(CHANGESEQSCRIPTSTEP_NAFLLCLKS, ErrorProgramming,
                UT_CASE_SET_DESCRIPTION("An error programming oclwrred")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQSCRIPTSTEP_NAFLLCLKS, ErrorProgramming)
{
    CHANGE_SEQ changeSeq = { 0 };
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS step = { 0 };

    initializeStep(&step);
    addClockDomain(&step, LW2080_CTRL_CLK_DOMAIN_GPCCLK, LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL);
    clkNafllGrpProgramMockAddEntry(0, FLCN_ERR_ILWALID_FUNCTION);

    UT_ASSERT_EQUAL_UINT(
        perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_IMPL(
            &changeSeq,
            (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER*)&step
        ),
        FLCN_ERR_ILWALID_FUNCTION
    );
}

/* ------------------------ Private Functions ------------------------------- */
static void *testCaseSetup(void)
{
    clkNafllGrpProgramMockInit();
    return NULL;
}

static void initializeStep(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pStep)
{
    pStep->clkList.numDomains = 0;
    for (LwU8 i = 0; i < LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS; ++i)
    {
        pStep->clkList.clkDomains[i].clkDomain = LW2080_CTRL_CLK_DOMAIN_UNDEFINED;
        pStep->clkList.clkDomains[i].source    = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID;
    }
}

static void addClockDomain(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pStep, LwU32 clkDomain, LwU8 clkProgSource)
{
    UT_ASSERT_TRUE(pStep->clkList.numDomains < LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS);
    pStep->clkList.numDomains;

    pStep->clkList.clkDomains[pStep->clkList.numDomains].clkDomain = clkDomain;
    pStep->clkList.clkDomains[pStep->clkList.numDomains].source    = clkProgSource;
    ++pStep->clkList.numDomains;
}
