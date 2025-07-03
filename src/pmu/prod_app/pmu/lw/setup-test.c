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
 * @file    setup-test.c
 * @brief   Unit tests for logic in setup.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"
#include "flcnifcmn.h"
#include "pmu/pmuifpmu.h"
#include "lwuproc.h"
#include "main_init_api.h"

#include "lwrtos-mock.h"
#include "lwoscmdmgmt-mock.h"
#include "lwostask-mock.h"
#include "pmu_objpmu-mock.h"
#include "pmu_objfb-mock.h"
#include "pmu_objic-mock.h"
#include "pmu_objtimer-mock.h"
#include "pmu_objclk-mock.h"
#include "therm/objtherm-mock.h"
#include "pmu_objperf-mock.h"
#include "perf/perf_daemon-mock.h"
#include "volt/objvolt-mock.h"
#include "main-mock.h"
#include "task_cmdmgmt-mock.h"
#include "task_perf-mock.h"
#include "task_perf_daemon-mock.h"
#include "task_therm-mock.h"
#include "task_watchdog-mock.h"
#include "ic/pmu_icgkxxx-mock.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_SETUP,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests PMU Core setup")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_SETUP, PmuSetupNullPointer,
                UT_CASE_SET_DESCRIPTION("Tests initPmuApp called with a NULL pointer")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_SETUP, PmuSetupSuccess,
                UT_CASE_SET_DESCRIPTION("Tests initPmuApp on success")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_SETUP, PmuSetupCalleeFailures,
                UT_CASE_SET_DESCRIPTION("Tests initPmuApp when its callees fail")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Tests initPmuApp called with a NULL pointer
 */
UT_CASE_RUN(PMU_SETUP, PmuSetupNullPointer)
{
    initPmuApp(NULL);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
}

/*!
 * @brief   Initializes state for PmuSetupSuccess
 *
 * @details Initializes the following;
 *              LWOS task mocking
 *              OBJPMU mocking
 *              OBJIC mocking
 *              Timer mocking
 *              OBJCLK mocking
 *              OBJTHERM mocking
 *              OBJPERF mocking
 *              Perf Daemon mocking
 *              OBJVOLT mocking
 *              Main mocking
 *              CmdMgmt Task mocking
 *              Perf Task mocking
 *              Perf Daemon Task mocking
 *              Therm Task mocking
 *              Watchdog Task mocking
 *              LWOS CmdMgmt mocking
 *              OBJIC GKXXX mocking
 *              LWRTOS mocking
 */
void
PmuSetupSuccessPreTest(void)
{
    lwosTaskMockInit();
    pmuMockInit();
    icMockInit();
    timerMockInit();
    clkMockInit();
    thermMockInit();
    perfMockInit();
    perfDaemonMockInit();
    voltMockInit();
    mainMockInit();
    cmdmgmtTaskMockInit();
    perfTaskMockInit();
    perfDaemonTaskMockInit();
    thermTaskMockInit();
    watchdogMockInit();
    lwosCmdmgmtMockInit();
    icgkxxxMockInit();
    lwrtosMockInit();
}

/*!
 * @brief   Tests initPmuApp when its callees fail
 */
UT_CASE_RUN(PMU_SETUP, PmuSetupSuccess)
{
    PmuSetupSuccessPreTest();

    initPmuApp(&(RM_PMU_CMD_LINE_ARGS){0U});

    UT_ASSERT_EQUAL_UINT(lwrtosTaskSchedulerStart_MOCK_fake.call_count, 1U);
}


void
PmuSetupCalleeFailuresPreTest(void)
{
    lwosTaskMockInit();
    pmuMockInit();
    fbMockInit();
    icMockInit();
    timerMockInit();
    clkMockInit();
    thermMockInit();
    perfMockInit();
    perfDaemonMockInit();
    voltMockInit();
    mainMockInit();
    cmdmgmtTaskMockInit();
    perfTaskMockInit();
    perfDaemonTaskMockInit();
    thermTaskMockInit();
    watchdogMockInit();
    lwosCmdmgmtMockInit();
    icgkxxxMockInit();
    lwrtosMockInit();
}

/*!
 * @brief   Tests initPmuApp when its callees fail
 */
UT_CASE_RUN(PMU_SETUP, PmuSetupCalleeFailures)
{
#define PMU_SETUP_CALLEE_CASE(func) \
    &func##_MOCK_fake.return_val
    static FLCN_STATUS *pReturlwals[] =
    {
        PMU_SETUP_CALLEE_CASE(lwosInit),
        PMU_SETUP_CALLEE_CASE(constructPmu),
        PMU_SETUP_CALLEE_CASE(constructFb),
        PMU_SETUP_CALLEE_CASE(constructIc),
        PMU_SETUP_CALLEE_CASE(constructTimer),
        PMU_SETUP_CALLEE_CASE(constructClk),
        PMU_SETUP_CALLEE_CASE(constructTherm),
        PMU_SETUP_CALLEE_CASE(constructPerf),
        PMU_SETUP_CALLEE_CASE(constructPerfDaemon),
        PMU_SETUP_CALLEE_CASE(constructVolt),
        PMU_SETUP_CALLEE_CASE(idlePreInitTask),
        PMU_SETUP_CALLEE_CASE(cmdmgmtPreInitTask),
        PMU_SETUP_CALLEE_CASE(perfPreInitTask),
        PMU_SETUP_CALLEE_CASE(perfDaemonPreInitTask),
        PMU_SETUP_CALLEE_CASE(thermPreInitTask),
        PMU_SETUP_CALLEE_CASE(watchdogPreInitTask),
        PMU_SETUP_CALLEE_CASE(osCmdmgmtPreInit),
        PMU_SETUP_CALLEE_CASE(watchdogPreInit),
    };

    LwLength i;
    LwU32 oldHaltCount = 0U;

    PmuSetupCalleeFailuresPreTest();

    for (i = 0U; i < LW_ARRAY_ELEMENTS(pReturlwals); i++)
    {
        LwU32 newHaltCount;

        *pReturlwals[i] = FLCN_ERROR;

        initPmuApp(&(RM_PMU_CMD_LINE_ARGS){0U});

        newHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();

        // Ensure we halted and did not start the scheduler.
        UT_ASSERT_EQUAL_UINT(newHaltCount, oldHaltCount + 1U);
        UT_ASSERT_EQUAL_UINT(lwrtosTaskSchedulerStart_MOCK_fake.call_count, 0U);

        oldHaltCount = newHaltCount;

        *pReturlwals[i] = FLCN_OK;
    }
}

/* ------------------------ Mocked Functions --------------------------------- */
/*!
 * @brief   Local definition of this function for now. There is no generalized
 *          way to _MOCK HALs, so just defining this locally until that's done
 *          (or until this function is compiled into UTF)
 */
LwU32
pmuGetOsTickIntrMask_GMXXX(void)
{
    return 0U;
}
