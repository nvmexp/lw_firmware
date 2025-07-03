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
 * @file    pmu_ssp-test.c
 * @brief   Unit tests for logic in pmu_ssp.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "lwtypes.h"
#include "lwuproc.h"
#include "test-macros.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Prototypes -------------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_SSP,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests Core PMU module")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_SSP, PmuSspHalt,
                UT_CASE_SET_DESCRIPTION("Ensures the PMU halts when a stack canary failure is detected")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Ensures the PMU halts when a stack canary failure is detected
 */
UT_CASE_RUN(PMU_SSP, PmuSspHalt)
{
    __stack_chk_fail();

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
}
/* ------------------------ Static Functions -------------------------------- */
