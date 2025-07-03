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
 * @file    changeseqscriptseq_pstate-mock.c
 * @brief   Mock implementations of changeseqscriptseq_pstate public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseqscriptstep_pstate.h"
#include "perf/changeseqscriptstep_pstate-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PSTATE_MOCK_MAX_ENTRIES        (4U)
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_PSTATE_MOCK_MAX_ENTRIES  (4U)
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_PSTATE_MOCK_MAX_ENTRIES (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of perfDaemonChangeSeqScriptBuildStep_PSTATE().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PSTATE_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PSTATE_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_PSTATE_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_PSTATE_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_PSTATE_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_PSTATE_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PSTATE_MOCK_CONFIG perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG;
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_PSTATE_MOCK_CONFIG perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG;
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_PSTATE_MOCK_CONFIG perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptBuildStep_PSTATE().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeqScriptBuildStep_PSTATEMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PSTATE_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeqScriptBuildStep_PSTATEMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_PSTATE_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Returns the number of times perfDaemonChangeSeqScriptBuildStep_PSTATE()
 * was called.
 *
 * @return the number time the mock function was called.
 */
LwU8 perfDaemonChangeSeqScriptBuildStep_PSTATEMockNumCalls(void)
{
    return perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.numCalls;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMUMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_PSTATE_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptBuildExelwtePrePstate_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATEMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_PRE_PSTATE_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMUMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_PSTATE_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptBuildExelwtePrePstate_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMUMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_POST_PSTATE_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptBuildStep_PSTATE
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pScript         Pointer to the change sequencer script
 * @param[in]   pSuper          Pointer to the base step class
 * @param[in]   lwrStepIndex    Current step
 *
 * @return perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_PSTATE_MOCK
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LwU8 entry = perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptBuildStepPstate_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pStepSuper      Pointer to the base step class
 *
 * @return perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LwU8 entry = perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptExelwteStepPrePstate_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pStepSuper      Pointer to the base step class
 *
 * @return perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LwU8 entry = perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptExelwteStepPostPstate_MOCK_CONFIG.entries[entry].status;
}
