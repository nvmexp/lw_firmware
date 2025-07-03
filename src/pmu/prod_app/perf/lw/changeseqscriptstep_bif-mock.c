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
 * @file    changeseqscriptstep_bif-mock.c
 * @brief   Mock implementations of changeseqscriptstep_bif public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseqscriptstep_bif.h"
#include "perf/changeseqscriptstep_bif-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_BIF_MOCK_MAX_ENTRIES       (4U)
#define PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_BIF_MOCK_MAX_ENTRIES     (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of perfDaemonChangeSeqScriptBuildStep_BIF().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_BIF_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_BIF_MOCK_CONFIG;

/*!
 * Configuration data for the mocked version of perfDaemonChangeSeqScriptExelwteStep_BIF().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_BIF_MOCK_MAX_ENTRIES];
} PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_BIF_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_BIF_MOCK_CONFIG perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG;
static PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_BIF_MOCK_CONFIG perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptBuildStep_BIF().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeqScriptBuildStep_BIFMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_BIF_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeqScriptBuildStep_BIFMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_BUILD_STEP_BIF_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Initializes the mock configuration data for perfDaemonChangeSeqScriptExelwteStep_BIF().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfDaemonChangeSeqScriptExelwteStep_BIFMockInit(void)
{
    memset(&perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG, 0x00, sizeof(perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG));
    for (LwU8 i = 0; i < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_BIF_MOCK_MAX_ENTRIES; ++i)
    {
        perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfDaemonChangeSeqScriptExelwtedStep_BIFMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_DAEMON_CHANGE_SEQ_SCRIPT_EXELWTE_STEP_BIF_MOCK_MAX_ENTRIES);
    perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptBuildStep_BIF
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pScript         Pointer to the change sequencer script
 * @param[in]   pSuper          Pointer to the base step class
 * @param[in]   lwrStepIndex    Current step
 *
 * @return perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_BIF_MOCK
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LwU8 entry = perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptBuildStepBif_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief MOCK implementation of perfDaemonChangeSeqScriptExelwteStep_BIF
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq      Pointer to the change sequencer
 * @param[in]   pStepSuper      Pointer to the base step class
 *
 * @return perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.status
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_BIF_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LwU8 entry = perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.numCalls;
    ++perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.numCalls;

    return perfDaemonChangeSeqScriptExelwteStepBif_MOCK_CONFIG.entries[entry].status;

}
