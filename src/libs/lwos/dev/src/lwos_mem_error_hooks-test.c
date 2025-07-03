/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file lwos_mem_error_hooks-test.c
 *
 * @details    This file contains all the unit tests for the memory error hook functions
 *
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"
#include <ut/ut.h>
#include "ut/ut_suite.h"
#include "ut/ut_case.h"
#include "ut/ut_assert.h"

/* ------------------------ Application includes ---------------------------- */
#include "lwrtos.h"
#include "lwoslayer.h"
#include "lwrtosHooks.h"

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */

/* ------------------------ Mocked Function Prototypes ---------------------- */

/* ------------------------ Unit-Under-Test --------------------------------- */

/* ------------------------ Local Variables --------------------------------- */

/* -------------------------------------------------------------------------- */

UT_SUITE_DEFINE(LWOS_MEM_ERROR_HOOKS,
    UT_SUITE_SET_COMPONENT("LWOS Memory Error Hooks")
    UT_SUITE_SET_DESCRIPTION("Unit tests for the Memory Error hook functions")
    UT_SUITE_SET_OWNER("ashenfield")
    UT_SUITE_SETUP_HOOK(NULL)
    UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(LWOS_MEM_ERROR_HOOKS, lwrtosHookImemMiss,
    UT_CASE_SET_DESCRIPTION("test the lwrtosHookImemMiss function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOS_MEM_ERROR_HOOKS_lwrtosHookImemMiss]"))

/*!
 * @brief      Test the lwrtosHookImemMiss function
 *
 * @details    Call lwrtosHookImemMiss, and assert that it halts
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOS_MEM_ERROR_HOOKS, lwrtosHookImemMiss)
{
    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();
    lwrtosHookImemMiss();
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(LWOS_MEM_ERROR_HOOKS, lwrtosHookDmemMiss,
    UT_CASE_SET_DESCRIPTION("test the lwrtosHookDmemMiss function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOS_MEM_ERROR_HOOKS_lwrtosHookDmemMiss]"))

/*!
 * @brief      Test the lwrtosHookDmemMiss function
 *
 * @details    Call lwrtosHookDmemMiss, and assert that it halts
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOS_MEM_ERROR_HOOKS, lwrtosHookDmemMiss)
{
    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();
    lwrtosHookDmemMiss();
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}
