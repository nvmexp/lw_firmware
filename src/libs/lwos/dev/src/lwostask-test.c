/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file lwostask-test.c
 *
 * @details    This file contains all the unit tests for the lwostask functions
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
#include "lwostask.h"
#include "lwostask-mock.h"
#include "lwrtos-mock.h"
#include "dmemovl-mock.h"
#include "linkersymbols.h"

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------ Unit-Under-Test --------------------------------- */

/* ------------------------ Local Variables --------------------------------- */
static struct _lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state {
    FLCN_STATUS return_val;
    RM_RTOS_TCB_PVT lwTcbExt;
} lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state;

static struct _lwrtosTaskCreate_MOCK_fake_lwstom_state {
    FLCN_STATUS return_val;
    LwrtosTaskHandle handle;
} lwrtosTaskCreate_MOCK_fake_lwstom_state;

/* ------------------------ Mocked Functions -------------------------------- */
FLCN_STATUS lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom(LwrtosTaskHandle taskHandle, void **ppLwTcbExt)
{
    *ppLwTcbExt = &(lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.lwTcbExt);
    return lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.return_val;
}

FLCN_STATUS 
lwrtosTaskCreate_MOCK_fake_lwstom
(
    LwrtosTaskHandle                 *pTaskHandle,
    const LwrtosTaskCreateParameters *pParams
)
{
    *pTaskHandle = lwrtosTaskCreate_MOCK_fake_lwstom_state.handle;
    return lwrtosTaskCreate_MOCK_fake_lwstom_state.return_val;
}

/* -------------------------------------------------------------------------- */

UT_SUITE_DEFINE(LWOSTASK,
    UT_SUITE_SET_COMPONENT("LWOS Task Management")
    UT_SUITE_SET_DESCRIPTION("Unit tests for the lwostask functions")
    UT_SUITE_SET_OWNER("ashenfield")
    UT_SUITE_SETUP_HOOK(NULL)
    UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(LWOSTASK, lwrtosHookTickAdjustGet,
    UT_CASE_SET_DESCRIPTION("test the lwrtosHookTickAdjustGet function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_lwrtosHookTickAdjustGet]"))

/*!
 * @brief      Test the lwrtosHookTickAdjustGet function
 *
 * @details    The test shall ilwoke lwrtosHookTickAdjustGet and assert that it returns 0.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOSTASK, lwrtosHookTickAdjustGet)
{
    //In the fully-resident case, this function should always return 0
    UT_ASSERT_EQUAL_UINT(lwrtosHookTickAdjustGet(), 0);
}

UT_CASE_DEFINE(LWOSTASK, lwosTaskSetupCanary,
    UT_CASE_SET_DESCRIPTION("test the lwosTaskSetupCanary function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_lwosTaskSetupCanary]"))

/*!
 * @brief      Test the lwosTaskSetupCanary function
 *
 * @details    The test shall ilwoke lwosTaskSetupCanary and assert that 
 *             the current task's stackCanary has been set to __stack_chk_guard.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOSTASK, lwosTaskSetupCanary)
{
    pPrivTcbLwrrent = &(lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.lwTcbExt);
    lwosTaskSetupCanary();
    UT_ASSERT_EQUAL_UINT(pPrivTcbLwrrent->stackCanary, (LwU32)LW_PTR_TO_LWUPTR(__stack_chk_guard));
}

UT_CASE_DEFINE(LWOSTASK, osTaskLoad,
    UT_CASE_SET_DESCRIPTION("test the osTaskLoad function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_osTaskLoad]"))

/*!
 * @brief      Test the osTaskLoad function
 *
 * @details    The test shall ilwoke osTaskLoad and assert that 
 *             it returns successfully and sets xLoadTimeTicks and
 *             the stack canary to valid values.
 *             The test will also call osTaskLoad with a NULL pointer
 *             for the pxLoadTimeTicks argument and assert that osTaskLoad
 *             has succeeded and set the stack canary to a valid value.
 *
 * This depends on the lwrtosTaskGetLwTcbExt MOCK from Unit Lwrtos.
 */
UT_CASE_RUN(LWOSTASK, osTaskLoad)
{
    LwU32            xLoadTimeTicks = 1234;
    LwrtosTaskHandle xTask = 1;
    const LwU32 canary = 0x12345678;

    lwrtosTaskGetLwTcbExt_MOCK_fake.lwstom_fake = lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom;
    lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.lwTcbExt.stackCanary = canary;
    lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.return_val = FLCN_OK;
    LwS32 ret = osTaskLoad(&xTask, &xLoadTimeTicks);
    UT_ASSERT_EQUAL_INT(ret, 0);
    UT_ASSERT_EQUAL_UINT(xLoadTimeTicks, 0);
    //we expect this function to set __stack_chk_guard to the task's canary value 
    UT_ASSERT_EQUAL_UINT(canary, (LwU32)LW_PTR_TO_LWUPTR(__stack_chk_guard));

    ret = osTaskLoad(&xTask, NULL);
    UT_ASSERT_EQUAL_INT(ret, 0);
    UT_ASSERT_EQUAL_UINT(xLoadTimeTicks, 0);
    UT_ASSERT_EQUAL_UINT(canary, (LwU32)LW_PTR_TO_LWUPTR(__stack_chk_guard));
}

UT_CASE_DEFINE(LWOSTASK, osTaskLoad_negative,
    UT_CASE_SET_DESCRIPTION("negative testing for the osTaskLoad function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_osTaskLoad_negative]"))

/*!
 * @brief      Negative test the osTaskLoad function
 *
 * @details    The test shall setup the lwrtosTaskGetLwTcbExt MOCK to return an error
 *             and ilwoke osTaskLoad and assert that it returns an error code and halts.
 *
 * This depends on the lwrtosTaskGetLwTcbExt MOCK from Unit Lwrtos.
 */
UT_CASE_RUN(LWOSTASK, osTaskLoad_negative)
{
    LwU32            xLoadTimeTicks = 1234;
    LwrtosTaskHandle xTask = 1;
    const LwU32 canary = 0x12345678;

    lwrtosTaskGetLwTcbExt_MOCK_fake.lwstom_fake = lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom;
    lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.return_val = FLCN_ERROR;
    LwS32 ret = osTaskLoad(&xTask, &xLoadTimeTicks);
    UT_ASSERT_NOT_EQUAL_INT(ret, 0);
    //we expect osTaskLoad to halt when lwrtosTaskGetLwTcbExt returns error
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
}

UT_CASE_DEFINE(LWOSTASK, osTaskIdGet,
    UT_CASE_SET_DESCRIPTION("Test the osTaskIdGet function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_osTaskIdGet]"))

/*!
 * @brief      Test the osTaskIdGet function
 *
 * @details    The test shall ilwoke osTaskIdGet both in the case when
 *             pPrivTcbLwrrent is valid and when it is NULL. osTaskIdGet
 *             should halt in the latter case.
 *
 * No external unit dependencies.
 */
UT_CASE_RUN(LWOSTASK, osTaskIdGet)
{
    pPrivTcbLwrrent = &(lwrtosTaskGetLwTcbExt_MOCK_fake_lwstom_state.lwTcbExt);
    LwU8 taskId = osTaskIdGet_IMPL();
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 0U);

    pPrivTcbLwrrent = NULL;
    taskId = osTaskIdGet_IMPL();
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
}

UT_CASE_DEFINE(LWOSTASK, lwosTaskCreate,
    UT_CASE_SET_DESCRIPTION("Test the lwosTaskCreate function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_lwosTaskCreate]"))

void dummyTaskFn(void *arg)
{
    return;
}

/*!
 * @brief      Test the lwosTaskCreate function
 *
 * @details    The test shall ilwoke lwosTaskCreate and assert that
 *             it returns successfully and allocates a stack and a handle.
 *
 * Depends on the lwrtosTaskCreate MOCK from Unit Lwrtos.
 */
UT_CASE_RUN(LWOSTASK, lwosTaskCreate)
{
    LwrtosTaskHandle              handle                    = NULL;
    RM_RTOS_TCB_PVT               tcbPvt                    = {0};
    const LwU8                    taskID                    = 1;
    const LwU16                   usStackDepth              = 0x0100;
    const LwU32                   uxPriority                = 4;
    const LwU8                    privLevelExt              = 1;
    const LwU8                    privLevelCsb              = 2;
    const LwU8                    ovlCntImemLS              = 3;
    const LwU8                    ovlCntImemHS              = 4;
    const LwU8                    ovlIdxImem                = 5;
    const LwU8                    ovlCntDmem                = 6;
    const LwU8                    ovlIdxStack               = 7;
    const LwU8                    ovlIdxDmem                = 8;
    const LwBool                  bEnableImemOnDemandPaging = LW_FALSE;


    lwrtosTaskCreate_MOCK_fake.lwstom_fake = lwrtosTaskCreate_MOCK_fake_lwstom;
    lwrtosTaskCreate_MOCK_fake_lwstom_state.handle = (LwrtosTaskHandle) 0x1234;
    lwrtosTaskCreate_MOCK_fake_lwstom_state.return_val = FLCN_OK;

    lwosDmemovlMockSetFailAlloc(LW_FALSE);

    FLCN_STATUS status = lwosTaskCreate_IMPL(
        &handle,
        &tcbPvt,
        &dummyTaskFn,
        taskID,
        usStackDepth,
        uxPriority,
        privLevelExt,
        privLevelCsb,
        ovlCntImemLS,
        ovlCntImemHS,
        ovlIdxImem,
        ovlCntDmem,
        ovlIdxStack,
        ovlIdxDmem,
        bEnableImemOnDemandPaging);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(handle, lwrtosTaskCreate_MOCK_fake_lwstom_state.handle);
    UT_ASSERT_NOT_EQUAL_PTR(tcbPvt.pStack, NULL);
}

UT_CASE_DEFINE(LWOSTASK, lwosTaskCreate_negative,
    UT_CASE_SET_DESCRIPTION("Negative test for the lwosTaskCreate function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_lwosTaskCreate_negative]"))

/*!
 * @brief      Negative test the lwosTaskCreate function
 *
 * @details    The test shall ilwoke lwosTaskCreate in the case when
 *             its call to malloc will fail, and in the case when its
 *             call to lwrtosTaskCreate will fail. Assert that it returns
 *             an error code in both cases.
 *
 * Depends on Mocks from Unit Malloc and Unit Lwrtos.
 */
UT_CASE_RUN(LWOSTASK, lwosTaskCreate_negative)
{
    LwrtosTaskHandle              handle                    = NULL;
    RM_RTOS_TCB_PVT               tcbPvt                    = {0};
    const LwU8                    taskID                    = 1;
    const LwU16                   usStackDepth              = 0x0100;
    const LwU32                   uxPriority                = 4;
    const LwU8                    privLevelExt              = 1;
    const LwU8                    privLevelCsb              = 2;
    const LwU8                    ovlCntImemLS              = 3;
    const LwU8                    ovlCntImemHS              = 4;
    const LwU8                    ovlIdxImem                = 5;
    const LwU8                    ovlCntDmem                = 6;
    const LwU8                    ovlIdxStack               = 7;
    const LwU8                    ovlIdxDmem                = 8;
    const LwBool                  bEnableImemOnDemandPaging = LW_FALSE;

    // make the malloc fail
    lwosDmemovlMockSetFailAlloc(LW_TRUE);
    lwrtosTaskCreate_MOCK_fake.lwstom_fake = lwrtosTaskCreate_MOCK_fake_lwstom;
    lwrtosTaskCreate_MOCK_fake_lwstom_state.handle = (LwrtosTaskHandle) 0x1234;
    lwrtosTaskCreate_MOCK_fake_lwstom_state.return_val = FLCN_OK;

    FLCN_STATUS status = lwosTaskCreate_IMPL(
        &handle,
        &tcbPvt,
        &dummyTaskFn,
        taskID,
        usStackDepth,
        uxPriority,
        privLevelExt,
        privLevelCsb,
        ovlCntImemLS,
        ovlCntImemHS,
        ovlIdxImem,
        ovlCntDmem,
        ovlIdxStack,
        ovlIdxDmem,
        bEnableImemOnDemandPaging);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM);

    // make the lwrtosTaskCreate call fail
    lwosDmemovlMockSetFailAlloc(LW_FALSE);
    lwrtosTaskCreate_MOCK_fake_lwstom_state.return_val = FLCN_ERROR;

    status = lwosTaskCreate_IMPL(
        &handle,
        &tcbPvt,
        &dummyTaskFn,
        taskID,
        usStackDepth,
        uxPriority,
        privLevelExt,
        privLevelCsb,
        ovlCntImemLS,
        ovlCntImemHS,
        ovlIdxImem,
        ovlCntDmem,
        ovlIdxStack,
        ovlIdxDmem,
        bEnableImemOnDemandPaging);

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);

    // now call it with a NULL handle arg
    lwosDmemovlMockSetFailAlloc(LW_FALSE);
    lwrtosTaskCreate_MOCK_fake_lwstom_state.return_val = FLCN_OK;

    status = lwosTaskCreate_IMPL(
        NULL,
        &tcbPvt,
        &dummyTaskFn,
        taskID,
        usStackDepth,
        uxPriority,
        privLevelExt,
        privLevelCsb,
        ovlCntImemLS,
        ovlCntImemHS,
        ovlIdxImem,
        ovlCntDmem,
        ovlIdxStack,
        ovlIdxDmem,
        bEnableImemOnDemandPaging);

    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(LWOSTASK, vApplicationIdleTaskCreate,
    UT_CASE_SET_DESCRIPTION("Test the vApplicationIdleTaskCreate function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_vApplicationIdleTaskCreate]"))

/*!
 * @brief      Test the vApplicationIdleTaskCreate function
 *
 * @details    The test shall ilwoke vApplicationIdleTaskCreate both in the
 *             successful case, and in the case when lwosTaskCreate returns an error.
 *             Assert that we halt in the latter case.
 *
 * No external dependencies.
 */
UT_CASE_RUN(LWOSTASK, vApplicationIdleTaskCreate)
{
    lwosTaskMockInit();
    vApplicationIdleTaskCreate(&dummyTaskFn, 0);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 0U);

    lwosTaskMockInit();
    LwosTaskMockConfig.lwosTaskCreateConfig.errorCodes[0] = FLCN_ERROR;
    vApplicationIdleTaskCreate(&dummyTaskFn, 0);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
}


UT_CASE_DEFINE(LWOSTASK, lwosInit,
    UT_CASE_SET_DESCRIPTION("Test the lwosInit function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_lwosInit]"))

/*!
 * @brief      Test the lwosInit function
 *
 * @details    The test shall ilwoke lwosInit both in the
 *             successful case, and in the case when 
 *             lwrtosTaskSchedulerInitialize returns an error.
 *             Assert that we halt in the latter case.
 *
 * This depends on the lwrtosTaskSchedulerInitialize Mock from unit Lwrtos.
 */
UT_CASE_RUN(LWOSTASK, lwosInit)
{
    FLCN_STATUS status;

    lwrtosTaskSchedulerInitialize_MOCK_fake.return_val = FLCN_OK;
    status = lwosInit_IMPL();
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    lwrtosTaskSchedulerInitialize_MOCK_fake.return_val = FLCN_ERROR;
    status = lwosInit_IMPL();
    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

UT_CASE_DEFINE(LWOSTASK, lwosResVAddrToPhysMemOffsetMap,
    UT_CASE_SET_DESCRIPTION("Test the lwosResVAddrToPhysMemOffsetMap function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTASK_lwosResVAddrToPhysMemOffsetMap]"))

/*!
 * @brief      Test the lwosResVAddrToPhysMemOffsetMap function
 *
 * @details    The test shall ilwoke lwosResVAddrToPhysMemOffsetMap both in the
 *             successful case, and in the case when the requested address is past
 *             _end_physical_dmem. Assert that an error is returned in the latter case.
 *
 * No external dependencies.
 */
UT_CASE_RUN(LWOSTASK, lwosResVAddrToPhysMemOffsetMap)
{
    LwU32 *pVAddr = (LwU32 *)0x5;
    LwU32 offset;

    FLCN_STATUS status = lwosResVAddrToPhysMemOffsetMap(pVAddr, &offset);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(offset, (LwU32)pVAddr);

    //now try an address which is past _end_physical_dmem
    pVAddr = (LwU32 *)_end_physical_dmem;
    status = lwosResVAddrToPhysMemOffsetMap(pVAddr, &offset);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE);
}
