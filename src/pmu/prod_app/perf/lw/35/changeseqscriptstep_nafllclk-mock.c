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
 * @file    changeseqscriptstep_clkmon-mock.c
 * @brief   Mock implementations of changeseqscriptstep_clkmon public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/35/changeseqscriptstep_nafllclk.h"
#include "perf/35/changeseqscriptstep_nafllclk-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_NAFLLCLK_MOCK_MAX_ENTRIES    (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of perfDaemomChangeSeqScriptExelwteStep_NAFLLCLK().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_NAFLLCLK_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_NAFLLCLK_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_NAFLLCLK_MOCK_CONFIG perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptExelwteStep_CLKMON().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKSMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_NAFLLCLK_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptExelwteStepClkmon_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_NAFLLCLK_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS()
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptExelwteStepVolt_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pStepSuper      Pointer to the base step class
 *
 * @return perfDaemonChangeSeqScriptExelwteStepVolt_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LwU8 entry = perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptExelwteStepNafllclk_MOCK_CONFIG.entries[entry].status;
}
