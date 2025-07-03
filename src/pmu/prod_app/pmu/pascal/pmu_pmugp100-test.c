/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_cmdmgmt-test.c
 * @brief   Unit tests for logic in task_cmdmgmt.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmu_objpmu.h"
#include "pmu_objpmu-mock.h"

#include "dev_pwr_csb.h"

/* ------------------------ Mocked Functions -------------------------------- */
/* ------------------------ Externs/Prototypes ------------------------------ */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_OBJPMU_GP100,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests portion of Unit Main of Module PMU Core")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_OBJPMU_GP100, PmuFbFlush_GP100Success,
               UT_CASE_SET_DESCRIPTION("Tests the FB flush function for  successful case")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("TODO")
               UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU_GP100, PmuFbFlush_GP100Fail,
               UT_CASE_SET_DESCRIPTION("Tests the FB flush function when the flush does not go through")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("TODO")
               UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Initializes state for PmuFbFlush_GP100SuccessPreTest
 *
 * @details Initializes the following:
 *              OBJPMU mocking state via @ref pmuMockInit
 *              LW_CPWR_FBIF_CTL register
 */
static void
PmuFbFlush_GP100SuccessPreTest(void)
{
    pmuMockInit();

    REG_WR32(CSB, LW_CPWR_FBIF_CTL, 0U);
}

UT_CASE_RUN(PMU_OBJPMU_GP100, PmuFbFlush_GP100Success)
{
    FLCN_STATUS status;

    PmuFbFlush_GP100SuccessPreTest();

    status = pmuFbFlush_GP100();

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief   Initializes state for PmuFbFlush_GP100Fail
 *
 * @details Initializes the following:
 *              OBJPMU mocking state via @ref pmuMockInit
 *                  Including a failing return value from @ref pmuPollReg32Ns
 *              LW_CPWR_FBIF_CTL register
 */
static void
PmuFbFlush_GP100FailPreTest(void)
{
    pmuMockInit();
    pmuPollReg32Ns_MOCK_fake.return_val = LW_FALSE;

    REG_WR32(CSB, LW_CPWR_FBIF_CTL, 0U);
}

UT_CASE_RUN(PMU_OBJPMU_GP100, PmuFbFlush_GP100Fail)
{
    FLCN_STATUS status;

    PmuFbFlush_GP100FailPreTest();

    status = pmuFbFlush_GP100();

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}
