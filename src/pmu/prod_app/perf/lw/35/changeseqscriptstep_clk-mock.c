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
 * @file    changeseqscriptstep_clk-mock.c
 * @brief   Mock implementations of changeseqscriptstep_clkmon public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/35/changeseqscriptstep_clk.h"
#include "perf/35/changeseqscriptstep_clk-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES      (4U)
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES     (4U)
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES    (4U)
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES   (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of perfDaemomChangeSeqScriptBuildStep_PRE_VOLTCLK().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PRE_VOLT_CLK_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemomChangeSeqScriptBuildStep_PRE_VOLTCLK().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_POST_VOLT_CLK_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemomChangeSeqScriptExelwteStep_PRE_VOLTCLK().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemomChangeSeqScriptExelwteStep_PRE_VOLTCLK().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_VOLT_CLK_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PRE_VOLT_CLK_MOCK_CONFIG perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG;
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_POST_VOLT_CLK_MOCK_CONFIG perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG;
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_CONFIG perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG;
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_VOLT_CLK_MOCK_CONFIG perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptBuildStep_PRE_VOLT_CLK().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptBuildStep_POST_VOLT_CLK().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_CLK().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKSMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_CLK().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKSMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_VOLT_CLK_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_VOLT_CLK_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptBuildStep_PRE_VOLT_CLK
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pScript         Pointer to the change sequencer script
 * @param[in]   pSuper          Pointer to the base step class
 * @param[in]   lwrStepIndex    Current step
 *
 * @return perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_MOCK
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LwU8 entry = perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptBuildStep_POST_VOLT_CLK
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pScript         Pointer to the change sequencer script
 * @param[in]   pSuper          Pointer to the base step class
 * @param[in]   lwrStepIndex    Current step
 *
 * @return perfDaemonChangeSeqScriptBuildStepPreVoltClk_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_MOCK
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LwU8 entry = perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptBuildStepPostVoltClk_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pStepSuper      Pointer to the base step class
 *
 * @return perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LwU8 entry = perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptExelwteStepPreVoltClk_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pStepSuper      Pointer to the base step class
 *
 * @return perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LwU8 entry = perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptExelwteStepPostVoltClk_MOCK_CONFIG.entries[entry].status;
}
