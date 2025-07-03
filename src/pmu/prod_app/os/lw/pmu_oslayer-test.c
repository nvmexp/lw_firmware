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
 * @file    pmu_oslayer-test.c
 * @brief   Unit tests for logic in pmu_oslayer.c
 */

/* ------------------------ Mocked Function Prototypes ---------------------- */
/* ------------------------ Unit-Under-Test --------------------------------- */
/* ------------------------ Includes ---------------------------------------- */
#include "lwtypes.h"
#include "test-macros.h"
#include "lwrtos.h"
/* ------------------------ Mocked Functions -------------------------------- */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_OSLAYER,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests PMU OS hooks")
                UT_SUITE_SET_OWNER("jorgeo"))

UT_CASE_DEFINE(PMU_OSLAYER, OsHookErrorHalts,
                UT_CASE_SET_DESCRIPTION("Ensure lwrtosHookError halts the PMU")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Ensure lwrtosHookError halts the PMU
 *
 * @details Ensures that lwrtosHookError halts the PMU when called.
 */
UT_CASE_RUN(PMU_OSLAYER, OsHookErrorHalts)
{
    // Values not actually used for anything lwrrently by lwrtosHookError 
    void *pxTask;
    LwS32 xErrorCode;

    lwrtosHookError(pxTask, xErrorCode);

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
}
