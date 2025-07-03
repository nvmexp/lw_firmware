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
 * @file    changeseq_35_daemon-test.c
 * @brief   Unit tests for logic in CHANGESEQ daemon.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objclk.h"
#include "perf/35/changeseq_35_daemon.h"
#include "perf/changeseq_daemon-mock.h"
#include "perf/changeseqscriptstep_bif-mock.h"
#include "perf/changeseqscriptstep_change-mock.h"
#include "perf/changeseqscriptstep_lpwr-mock.h"
#include "perf/changeseqscriptstep_pstate-mock.h"
#include "perf/changeseqscriptstep_volt-mock.h"
#include "perf/35/changeseqscriptstep_clk-mock.h"
#include "perf/35/changeseqscriptstep_clkmon-mock.h"

extern OBJCLK Clk;
static LwU8 SemaphoreCount = 0;

/* ------------------------ Function Prototypes ----------------------------- */
// static void *testCaseSetup(void);
// static void initializeChangeSequencer(CHANGE_SEQ_35 *pChangeSeq);
// static void initializeScript(CHANGE_SEQ_SCRIPT *pRequest);
// static void initializeChangeRequest(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pRequest);

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
#define VFPOINTS_CACHE_COUNTER                                              45U

/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief The test group for the P-states 3.5 change sequencer.
 *
 * @defgroup CHANGESEQ_35_DAEMON_TEST_SUITE
 */
// UT_SUITE_DEFINE(PMU_CHANGESEQ_35_DAEMON,
//                 UT_SUITE_SET_COMPONENT("Change Sequencer 3.5 Daemon")
//                 UT_SUITE_SET_DESCRIPTION("Unit test for the P-states 3.5 daemon")
//                 UT_SUITE_SET_OWNER("Jeff Hutchins")
//                 UT_SUITE_SETUP_HOOK(testCaseSetup))

/*!
 * @brief Initialize the change sequencer script where the VF Points cache
 * has been updated.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ_35_DAEMON.CacheCounterNotEqual
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_35_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Modify the current change's VF point cache counter to not equal that
 * of the CHANGESEQ.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptInit_35(). Expect the function to
 * return @ref FLCN_WARN_NOTHING_TO_DO.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ_35_DAEMON, CacheCounterNotEqual,
//                 UT_CASE_SET_DESCRIPTION("Stale VF data present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ_35_DAEMON, CacheCounterNotEqual)
// {
//     CHANGE_SEQ_35 changeSeq;
//     CHANGE_SEQ_SCRIPT script;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;

//     initializeChangeRequest(&lwrrentChange);
//     initializeChangeRequest(&lastChange);

//     lwrrentChange.vfPointsCacheCounter--;

//     UT_ASSERT_EQUAL_UINT(
//         perfDaemonChangeSeqScriptInit_35(
//             (CHANGE_SEQ*)&changeSeq,
//             &script,
//             &lwrrentChange,
//             &lastChange
//         ),
//         FLCN_WARN_NOTHING_TO_DO
//     );
// }

/*!
 * @brief Test the case where the change does not contain a P-state change.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ_35_DAEMON.NoPstateChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_35_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the current and last change requests to have the same P-state
 * index.
 * @li Configure the step mock functions to return FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptInit_35(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify perfDaemonChangeSeqScriptBuildStep_PSTATE() has not been called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ_35_DAEMON, NoPstateChange,
//                 UT_CASE_SET_DESCRIPTION("No P-state change")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ_35_DAEMON, NoPstateChange)
// {
//     CHANGE_SEQ_35 changeSeq;
//     CHANGE_SEQ_SCRIPT script;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;

//     boardObjGrpMaskInit_E32(&changeSeq.super.script.clkDomainsActiveMask);
//     boardObjGrpMaskInit_E32(&changeSeq.super.clkDomainsInclusionMask);
//     boardObjGrpMaskInit_E32(&changeSeq.super.clkDomainsExclusionMask);
//     boardObjGrpMaskInit_E32(&Clk.domains.progDomainsMask);

//     initializeChangeRequest(&lwrrentChange);
//     lwrrentChange.pstateIndex = 1;
//     initializeChangeRequest(&lastChange);
//     lastChange.pstateIndex = 1;

//     perfDaemonChangeSeqFlush_CHANGE_MOCK_fake.return_val = FLCN_OK;
//     perfDaemonChangeSeqValidateChange_MOCK_fake.return_val = FLCN_OK;
//     perfDaemonChangeSeqScriptBuildStep_CHANGEMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeqScriptBuildStep_VOLTMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeq35ScriptBuildStep_CLK_MONMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeqScriptBuildStep_CHANGEMockAddEntry(1, FLCN_OK);
//     perfDaemonChangeSeqScriptStepInsert_MOCK_fake.return_val = FLCN_OK;

//     UT_ASSERT_EQUAL_UINT(
//         perfDaemonChangeSeqScriptInit_35(
//             (CHANGE_SEQ*)&changeSeq,
//             &script,
//             &lwrrentChange,
//             &lastChange
//         ),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqScriptBuildStep_PSTATEMockNumCalls(), 0);
// }

/*!
 * @brief Test the case where the change contains a P-state change.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ_35_DAEMON.PstateChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_35_DAEMON_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the current and last change requests to have different P-state
 * index.
 * @li Configure the step mock functions to return FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptInit_35(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify perfDaemonChangeSeqScriptBuildStep_PSTATE() has been called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ_35_DAEMON, PstateChange,
//                 UT_CASE_SET_DESCRIPTION("No P-state change")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ_35_DAEMON, PstateChange)
// {
//     CHANGE_SEQ_35 changeSeq;
//     CHANGE_SEQ_SCRIPT script;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;

//     boardObjGrpMaskInit_E32(&changeSeq.super.script.clkDomainsActiveMask);
//     boardObjGrpMaskInit_E32(&changeSeq.super.clkDomainsInclusionMask);
//     boardObjGrpMaskInit_E32(&changeSeq.super.clkDomainsExclusionMask);
//     boardObjGrpMaskInit_E32(&Clk.domains.progDomainsMask);

//     initializeChangeRequest(&lwrrentChange);
//     lwrrentChange.pstateIndex = 1;
//     initializeChangeRequest(&lastChange);
//     lastChange.pstateIndex = 1;

//     perfDaemonChangeSeqFlush_CHANGE_MOCK_fake.return_val = FLCN_OK;
//     perfDaemonChangeSeqValidateChange_MOCK_fake.return_val = FLCN_OK;
//     perfDaemonChangeSeqScriptBuildStep_CHANGEMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeqScriptBuildStep_VOLTMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeq35ScriptBuildStep_CLK_MONMockAddEntry(0, FLCN_OK);
//     perfDaemonChangeSeqScriptBuildStep_CHANGEMockAddEntry(1, FLCN_OK);
//     perfDaemonChangeSeqScriptStepInsert_MOCK_fake.return_val = FLCN_OK;

//     UT_ASSERT_EQUAL_UINT(
//         perfDaemonChangeSeqScriptInit_35(
//             (CHANGE_SEQ*)&changeSeq,
//             &script,
//             &lwrrentChange,
//             &lastChange
//         ),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(perfDaemonChangeSeqScriptBuildStep_PSTATEMockNumCalls(), 0);
// }

/* ------------------------ Private Functions ------------------------------- */
// static void *testCaseSetup(void)
// {
//     RESET_FAKE(perfDaemonChangeSeqFlush_CHANGE_MOCK);
//     RESET_FAKE(perfDaemonChangeSeqValidateChange_MOCK);
//     perfDaemonChangeSeqScriptBuildStep_CHANGEMockInit();
//     perfDaemonChangeSeqScriptBuildStep_PSTATEMockInit();
//     perfDaemonChangeSeqScriptBuildStep_LPWRMockInit();
//     perfDaemonChangeSeqScriptBuildStep_BIFMockInit();
//     perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockInit();
//     perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockInit();
//     perfDaemonChangeSeq35ScriptBuildStep_CLK_MONMockInit();
//     perfDaemonChangeSeqScriptBuildStep_VOLTMockInit();
//     RESET_FAKE(perfDaemonChangeSeqScriptStepInsert_MOCK);
//     FFF_RESET_HISTORY();

//     Clk.vfPoints.vfPointsCacheCounter = VFPOINTS_CACHE_COUNTER;

//     return NULL;
// }

// static void initializeChangeSequencer(CHANGE_SEQ_35 *pChangeSeq)
// {
//     memset(pChangeSeq, 0x00, sizeof(*pChangeSeq));
// }

// static void initializeScript(CHANGE_SEQ_SCRIPT *pRequest)
// {
//     memset(pRequest, 0x00, sizeof(*pRequest));
// }

// static void initializeChangeRequest(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pRequest)
// {
//     memset(pRequest, 0x00, sizeof(*pRequest));
//     pRequest->vfPointsCacheCounter = VFPOINTS_CACHE_COUNTER;
// }

void osSemaphoreRWTakeRD(PSEMAPHORE_RW pSem)
{
    UT_ASSERT_EQUAL_UINT8(SemaphoreCount, 0);
    SemaphoreCount++;
}

void osSemaphoreRWGiveRD(PSEMAPHORE_RW pSem)
{
    UT_ASSERT_EQUAL_UINT8(SemaphoreCount, 1);
    SemaphoreCount--;
}
