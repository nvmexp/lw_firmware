/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All information contained
 * herein is proprietary and confidential to LWPU Corporation.  Any use, reproduction, or
 * disclosure without the written permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file     lwostmrcallback-test.c
 *
 * @details  This file contains all the unit tests for the Unit Timer Callback.
 *           The unit tests are bucketed into the items below:
 *           - Input validation tests: Provide invalid inputs to the APIs and test for expected
 *             behavior.
 *           - Basic requirement tests: Provide valid inputs to the APIs and test for expected
 *             functional behavior.
 *           - Fault injection tests: Different from input validation, some functions are mocked to
 *             return error or corrupt data structures in the middle of a process (which are
 *             probably otherwise impossible in real world cases), and test for expected behavior.
 *           - Use case targeting tests: Given that in `PMU-TU10A` there is only one use case of
 *             callback, we try to cover this in this section.
 *
 *           Test naming scheme:
 *           - <function_name>_<test_category>
 */

/* ------------------------ System Includes ----------------------------------------------------- */
#include <ut/ut.h>
#include "test-macros.h"

/* ------------------------ Application Includes ------------------------------------------------ */
#include "lwos_ovl.h"
#include "lwrtos-mock.h"
#include "lwoswatchdog-mock.h"
#include "lwostmrcallback-mock.h"
#include "dmemovl.h"

/* ------------------------ Defines ------------------------------------------------------------- */
#ifndef OS_CALLBACKS_SINGLE_MODE
#error Test with OS_TMR_CALLBACK_MODE_SLEEP not supported.
#endif

#if !(OS_CALLBACKS_WSTATS)
#error Test with OS_CALLBACKS_WSTATS being false not supported.
#endif

/*!
 * @brief    Some constants used by default
 */
#define DEFAULT_QUEUE_HANDLE_UTF 0
#define DEFAULT_VALID_PERIOD_US_UTF (3U * LWRTOS_TICK_PERIOD_US)
#define UPDATED_VALID_PERIOD_US_UTF (5U * LWRTOS_TICK_PERIOD_US)
#define DEFAULT_TASK_ID 0

/*!
 * @brief    Default user defined timer callback UTF instance.
 * @details  All other callback functions in this unit test suite set must call this callback.
 *           It updates the callback exelwtion flag.
 *           Knobs (restored to default in @ref restoreCallbackDefault):
 *           - @ref g_defaultCallbackFuncRet to adjust callback return value.
 *           - @ref g_defaultCallbackFuncPayload to adjust callback payload
 *             (by just fast forward OS time tick)
 */
static FLCN_STATUS defaultCallbackFunc_UTF(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief  Callback function that updates its period.
 */
static FLCN_STATUS osTmrCallbackFuncWithUpdate_UTF(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief  Callback function that cancels itself.
 */
static FLCN_STATUS osTmrCallbackFuncWithCancel_UTF(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief    UTF callback tracking info. Should not access directly.
 *
 * @details  Each pair consists of
 *           1. function pointer.
 *           2. if it's exelwted.
 *           3. callback structure that triggers it.
 *
 *           List of functions that controls this structure.
 *           - @ref EXPECT_CALLBACK_EXELWTED
 *           - @ref EXPECT_CALLBACK_NOT_EXELWTED
 *           - @ref callbackFuncsInfo
 *           - @ref restoreCallbackDefault
 */
typedef struct UTF_CALLBACK_FUNC_INFO
{
    OsTmrCallbackFuncPtr pFunc;
    LwBool bExelwted;
    OS_TMR_CALLBACK *pCallback;
} UTF_CALLBACK_FUNC_INFO;

UTF_CALLBACK_FUNC_INFO callbackFuncsInfo[] =
{
    {defaultCallbackFunc_UTF, LW_FALSE, NULL},
    {osTmrCallbackFuncWithUpdate_UTF, LW_FALSE, NULL},
    {osTmrCallbackFuncWithCancel_UTF, LW_FALSE, NULL}
};

/*!
 * @brief  Knobs for the test infrastructure in this file.
 */
OS_TMR_CALLBACK_TYPE g_callbackType_UTF           = OS_TMR_CALLBACK_TYPE_ONE_SHOT;
LwU32                g_callbackPeriodUs_UTF       = DEFAULT_VALID_PERIOD_US_UTF;
OsTmrCallbackFuncPtr g_callbackFncPtr_UTF         = defaultCallbackFunc_UTF;
FLCN_STATUS          g_defaultCallbackFuncRet     = FLCN_OK;
LwU32                g_defaultCallbackFuncPayload = 0;

extern OS_TMR_CALLBACK    **LwOsTmrCallbackMap; 
extern OS_TMR_CB_MAP_HANDLE LwOsTmrCallbackMapHandleMax; 
extern OS_TMR_CB_MAP_HANDLE cbMapCount;
/* ------------------------ Private static Functions -------------------------------------------- */
/*!
 * @brief    Check if the callback is exelwted.
 *
 * @details  Should only be used by macro `EXPECT_CALLBACK_EXELWTED` and `defaultCallbackFunc_UTF`.
 *           The function detects if the callback is registered. Return if the callback has been
 *           exelwted and clear the flag.
 *
 * @param[in]  Callback pointer.
 *
 * @return   UT_FAIL   if callback function does not exist.
 *           LW_FALSE  if callback exelwtion flag was not set.
 *           LW_TRUE   otherwise.
 */
static LwBool s_ifCallbackFnExelwted(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief    Mark the callback as exelwted.
 *
 * @details  Should only be used by macro `EXPECT_CALLBACK_EXELWTED` and `defaultCallbackFunc_UTF`.
 *
 * @param[in]  Callback pointer.
 *
 * @return   LW_FALSE if callback function does not exist.
 *           LW_FALSE if callback exelwtion flag was not cleared.
 *           LW_TRUE on success.
 */
static LwBool s_registerCallbackExelwtion(OS_TMR_CALLBACK *pCallback);

/* ------------------------ Static Functions ---------------------------------------------------- */
/*!
 * @brief  Macro to detect callback exelwtion given the procedure.
 */
#define EXPECT_CALLBACK_EXELWTED(pCallback, procedure)                                             \
    UT_ASSERT_EQUAL_UINT(s_ifCallbackFnExelwted(pCallback), LW_FALSE);                             \
    procedure;                                                                                     \
    UT_ASSERT_EQUAL_UINT(s_ifCallbackFnExelwted(pCallback), LW_TRUE)

/*!
 * @brief  Callback function should not be called after a given procedure.
 */
#define EXPECT_CALLBACK_NOT_EXELWTED(pCallback, procedure)                                         \
    UT_ASSERT_EQUAL_UINT(s_ifCallbackFnExelwted(pCallback), LW_FALSE);                             \
    procedure;                                                                                     \
    UT_ASSERT_EQUAL_UINT(s_ifCallbackFnExelwted(pCallback), LW_FALSE)

/*!
 * @brief  Restore default values of the knobs in the test file.
 */
static void restoreCallbackDefault();

/*!
 * @brief    Reset a given callback gracefully.
 * @details  Remove a callback from the global list if there and reset the structure.
 */
static void resetCallback_UTF(const OS_TMR_CALLBACK *pCallback);

/*!
 * @brief  Remove all callbacks from the list gracefully and reset OS tick count.
 */
static void resetOsTmrCallback_UTF();

/*!
 * @brief    Per test setup procedure.
 * @details  The goal of the function is to make sure the test condition is brought back to the
 *           initial state so each test is isolated.
 */
static void perTestSetupFn();

/*!
 * @brief  Per test tear down procedure.
 */
static void perTestTeardownFn();

/*!
 * @brief    Put the callback to the target state if possible.
 * @details  One exception is @ref OS_TMR_CALLBACK_STATE_EXELWTED, the function will put the
 *           callback through the `EXELWTED` state.
 */
static void putCallbackToTargetState_UTF(OS_TMR_CALLBACK *pCallback, OS_TMR_CALLBACK_STATE state);

/*!
 * @brief  Helper function for @ref putCallbackToTargetState_UTF , which helps with the relwrsive
 *         implementation to bring the callback to a certain state.
 */
static void s_putCallbackToTargetState_UTF(OS_TMR_CALLBACK *pCallback, OS_TMR_CALLBACK_STATE state);

/*!
 * @brief    Verify if two callbacks are pointing to each other in a given mode.
 *
 * @param[in]  pCallback_A  First callback.
 * @param[in]  pCallback_B  Second callback.
 */
static void
verifyCallbacksLinks
(
    OS_TMR_CALLBACK *pCallback_A,
    OS_TMR_CALLBACK *pCallback_B,
    OS_TMR_CALLBACK_MODE mode
);

static LwBool ifCallbackInDefault(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief    Verify individual callback integrity.
 * @details  Ilwoke UT_ASSERT if callback has something wrong.
 */
static void verifyCallbackIntegrity(OS_TMR_CALLBACK *pCallback);

static LwBool ifOsTmrCallbackModeValid();

static LwBool ifOsTmrCallbackInDefault();

/*!
 * @brief    Verify integrity of the global callback list OsTmrCallback.
 *
 * @details  OsTmrCallback should satisfy:
 *           1. All callbacks in the list are valid.
 *           2. All callbacks is in `SCHEDULED` state.
 *           3. Callback links are consistent.
 *           
 * @note     It should not be needed for functions that does not touch OsTmrCallback
 *           nor change the links in the callbacks.
 */
static void verifyOsTmrCallbackIntegrity();

/* ------------------------ Unit Tests ---------------------------------------------------------- */
UT_SUITE_DEFINE(LWOSTMRCALLBACK,
    UT_SUITE_SET_COMPONENT("Unit Timer Callback")
    UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Timer Callback")
    UT_SUITE_SET_OWNER("yitianc")
    UT_SUITE_SETUP_HOOK(perTestSetupFn)
    UT_SUITE_TEARDOWN_HOOK(perTestTeardownFn))

/* ------------------------ Input validation tests ---------------------------------------------- */
UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackCreate_inputValidation,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackCreate input validation")
    UT_CASE_SET_TYPE(INTERFACE)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackCreate_inputValidation]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackCreate_inputValidation)
{
    /*!
     * @brief    Invalid callback pointer.
     * @details  1. Pass `NULL` callback pointer to `osTmrCallbackCreate` with other valid inputs.
     *           2. Expect `FLCN_ERR_ILWALID_ARGUMENT` returned.
     */
    UT_ASSERT_EQUAL_UINT(
        osTmrCallbackCreate(NULL, OS_TMR_CALLBACK_TYPE_ONE_SHOT, OVL_INDEX_ILWALID,
            defaultCallbackFunc_UTF, DEFAULT_QUEUE_HANDLE_UTF, DEFAULT_VALID_PERIOD_US_UTF,
            DEFAULT_VALID_PERIOD_US_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL, DEFAULT_TASK_ID),
        FLCN_ERR_ILWALID_ARGUMENT);

    /*!
     * @brief    Invalid callback periods.
     * @details  1. No valid callback period is passed to `osTmrCallbackCreate`.
     *           2. Expect `FLCN_ERR_ILWALID_ARGUMENT` returned.
     *           3. Expect the callback to remain in default state.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        resetCallback_UTF(&tmrCallback_UTF);
        UT_ASSERT_EQUAL_UINT(
            osTmrCallbackCreate(&tmrCallback_UTF, OS_TMR_CALLBACK_TYPE_ONE_SHOT, OVL_INDEX_ILWALID,
                defaultCallbackFunc_UTF, DEFAULT_QUEUE_HANDLE_UTF,
                OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER, OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER,
                OS_TIMER_RELAXED_MODE_USE_NORMAL, DEFAULT_TASK_ID),
            FLCN_ERR_ILWALID_ARGUMENT);
        UT_ASSERT_EQUAL_UINT(ifCallbackInDefault(&tmrCallback_UTF), LW_TRUE);
    }

    /*!
     * @brief    Invalid callback state.
     * @details  1. Pass in callback with non `START` state.
     *           2. Expect `FLCN_ERR_ILLEGAL_OPERATION` returned.
     *           3. Expect no state change to the callback structure.
     *           4. Expect the callback structure integrity.
     *           5. Except `OsTmrCallback` integrity.
     */
    {
        LwU32 i = 0;
        OS_TMR_CALLBACK_STATE states[] = {
            OS_TMR_CALLBACK_STATE_CREATED,
            OS_TMR_CALLBACK_STATE_SCHEDULED,
            OS_TMR_CALLBACK_STATE_QUEUED,
            // @todo: OS_TMR_CALLBACK_STATE_EXELWTED,
            OS_TMR_CALLBACK_STATE_ERROR
        };
        for (i = 0; i < sizeof(states)/sizeof(OS_TMR_CALLBACK_STATE); i++)
        {
            OS_TMR_CALLBACK tmrCallback_UTF;
            putCallbackToTargetState_UTF(&tmrCallback_UTF, states[i]);

            UT_ASSERT_EQUAL_UINT(
                osTmrCallbackCreate(&tmrCallback_UTF,OS_TMR_CALLBACK_TYPE_ONE_SHOT,
                    OVL_INDEX_ILWALID, defaultCallbackFunc_UTF, DEFAULT_QUEUE_HANDLE_UTF,
                    g_callbackPeriodUs_UTF, g_callbackPeriodUs_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL, DEFAULT_TASK_ID),
                FLCN_ERR_ILLEGAL_OPERATION);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, states[i]);
            verifyCallbackIntegrity(&tmrCallback_UTF);
            verifyOsTmrCallbackIntegrity();

            resetOsTmrCallback_UTF();
        }
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackSchedule_inputValidation,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackSchedule input validation")
    UT_CASE_SET_TYPE(INTERFACE)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackSchedule_inputValidation]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackSchedule_inputValidation)
{
    /*!
     * @brief    Invalid callback pointer.
     * @details  1. Pass `NULL` callback pointer to `osTmrCallbackSchedule`.
     *           2. Expect `FLCN_ERR_ILWALID_ARGUMENT` returned.
     *           3. Except `OsTmrCallback` integrity.
     */
    UT_ASSERT_EQUAL_UINT(osTmrCallbackSchedule(NULL), FLCN_ERR_ILWALID_ARGUMENT);
    verifyOsTmrCallbackIntegrity();

    /*!
     * @brief    Invalid callback state.
     * @details  1. Pass in callback with non `CREATED` state.
     *           2. Expect `FLCN_ERR_ILLEGAL_OPERATION` returned.
     *           3. Expect no state change to the callback structure.
     *           4. Except the callback structure integrity.
     *           5. Except `OsTmrCallback` integrity.
     */
    {
        LwU32 i = 0;
        OS_TMR_CALLBACK_STATE states[] = {
            OS_TMR_CALLBACK_STATE_START,
            OS_TMR_CALLBACK_STATE_SCHEDULED,
            OS_TMR_CALLBACK_STATE_QUEUED,
            // @todo: OS_TMR_CALLBACK_STATE_EXELWTED,
            OS_TMR_CALLBACK_STATE_ERROR
        };
        for (i = 0; i < sizeof(states)/sizeof(OS_TMR_CALLBACK_STATE); i++)
        {
            OS_TMR_CALLBACK tmrCallback_UTF;
            putCallbackToTargetState_UTF(&tmrCallback_UTF, states[i]);

            UT_ASSERT_EQUAL_UINT(osTmrCallbackSchedule(&tmrCallback_UTF),
                FLCN_ERR_ILLEGAL_OPERATION);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, states[i]);
            verifyCallbackIntegrity(&tmrCallback_UTF);
            verifyOsTmrCallbackIntegrity();

            resetOsTmrCallback_UTF();
        }
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackUpdate_inputValidation,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackUpdate input validation")
    UT_CASE_SET_TYPE(INTERFACE)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackUpdate_inputValidation]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackUpdate_inputValidation)
{
    /*!
     * @brief    Invalid callback pointer.
     * @details  1. Pass `NULL` callback pointer to `osTmrCallbackUpdate` with other valid inputs.
     *           2. Expect `FLCN_ERR_ILWALID_ARGUMENT` returned.
     */
    UT_ASSERT_EQUAL_UINT(osTmrCallbackUpdate(NULL, DEFAULT_VALID_PERIOD_US_UTF,
        DEFAULT_VALID_PERIOD_US_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL, OS_TIMER_UPDATE_USE_BASE_LWRRENT),
        FLCN_ERR_ILWALID_ARGUMENT);

    /*!
     * @brief    Invalid callback periods.
     * @details  1. No valid callback period is passed to `osTmrCallbackUpdate`.
     *           2. Expect `FLCN_ERR_ILWALID_ARGUMENT` returned.
     *           3. Expect no change to the callback structure. Here in the test, remain in the
     *              default state.
     *           4. Except `OsTmrCallback` integrity.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_START);
        UT_ASSERT_EQUAL_UINT(
            osTmrCallbackUpdate(&tmrCallback_UTF, OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER,
                OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER, OS_TIMER_RELAXED_MODE_USE_NORMAL, OS_TIMER_UPDATE_USE_BASE_LWRRENT),
            FLCN_ERR_ILWALID_ARGUMENT);
        UT_ASSERT_EQUAL_UINT(ifCallbackInDefault(&tmrCallback_UTF), LW_TRUE);
        verifyOsTmrCallbackIntegrity();
    }

    /*!
     * @brief    Invalid callback state.
     * @details  1. Pass in callback with `START` or `ERROR`.
     *           2. Expect `FLCN_ERR_ILLEGAL_OPERATION` returned.
     *           3. Expect no state change to the callback structure.
     *           4. Except `OsTmrCallback` integrity.
     */
    {
        LwU32 i = 0;
        OS_TMR_CALLBACK_STATE states[] = {
            OS_TMR_CALLBACK_STATE_START,
            OS_TMR_CALLBACK_STATE_ERROR
        };
        FLCN_STATUS status[] = {
            FLCN_ERR_ILLEGAL_OPERATION,
            FLCN_ERR_ILWALID_STATE
        };
        for (i = 0; i < sizeof(states)/sizeof(OS_TMR_CALLBACK_STATE); i++)
        {
            OS_TMR_CALLBACK tmrCallback_UTF;
            putCallbackToTargetState_UTF(&tmrCallback_UTF, states[i]);

            UT_ASSERT_EQUAL_UINT(osTmrCallbackUpdate(&tmrCallback_UTF, g_callbackPeriodUs_UTF,
                g_callbackPeriodUs_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL, OS_TIMER_UPDATE_USE_BASE_LWRRENT), status[i]);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, states[i]);
            verifyCallbackIntegrity(&tmrCallback_UTF);
            verifyOsTmrCallbackIntegrity();

            resetOsTmrCallback_UTF();
        }
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackExelwte_inputValidation,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackExelwte input validation")
    UT_CASE_SET_TYPE(INTERFACE)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackExelwte_inputValidation]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackExelwte_inputValidation)
{
    /*!
     * @brief    Invalid callback pointer.
     * @details  1. Pass `NULL` callback pointer to `osTmrCallbackExelwte`.
     *           2. Expect `FLCN_ERR_ILWALID_ARGUMENT` returned.
     */
    UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(NULL), FLCN_ERR_ILWALID_ARGUMENT);

    /*!
     * @brief    Invalid callback state.
     * @details  1. Pass in callback with `START` or `ERROR`.
     *           2. Expect `FLCN_ERR_ILLEGAL_OPERATION` returned.
     *           3. Expect no state change to the callback structure.
     *           4. Except `OsTmrCallback` integrity.
     */
    {
        LwU32 i = 0;
        OS_TMR_CALLBACK_STATE states[] = {
            OS_TMR_CALLBACK_STATE_START,
            OS_TMR_CALLBACK_STATE_CREATED,
            OS_TMR_CALLBACK_STATE_SCHEDULED,
            // @todo: OS_TMR_CALLBACK_STATE_EXELWTED,
            OS_TMR_CALLBACK_STATE_ERROR
        };
        for (i = 0; i < sizeof(states)/sizeof(OS_TMR_CALLBACK_STATE); i++)
        {
            OS_TMR_CALLBACK tmrCallback_UTF;
            putCallbackToTargetState_UTF(&tmrCallback_UTF, states[i]);

            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF),
                FLCN_ERR_ILLEGAL_OPERATION);

            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, states[i]);
            verifyCallbackIntegrity(&tmrCallback_UTF);
            verifyOsTmrCallbackIntegrity();

            resetOsTmrCallback_UTF();
        }
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackCancel_inputValidation,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackCancel input validation")
    UT_CASE_SET_TYPE(INTERFACE)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackCancel_inputValidation]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackCancel_inputValidation)
{
    /*!
     * @brief    Invalid callback pointer.
     * @details  1. Pass `NULL` callback pointer to `osTmrCallbackCancel`.
     *           2. Expect `LW_FALSE` returned.
     */
    UT_ASSERT_EQUAL_UINT(LW_FALSE, osTmrCallbackCancel(NULL));

    /*!
     * @brief    Invalid callback state.
     * @details  1. Pass in callback with `START`, `CREATED` or `ERROR`.
     *           2. Expect `LW_FALSE` returned.
     *           3. Expect no state change to the callback structure.
     */
    {
        LwU32 i = 0;
        OS_TMR_CALLBACK_STATE states[] = {
            OS_TMR_CALLBACK_STATE_START,
            OS_TMR_CALLBACK_STATE_CREATED,
            OS_TMR_CALLBACK_STATE_ERROR
        };
        for (i = 0; i < sizeof(states)/sizeof(OS_TMR_CALLBACK_STATE); i++)
        {
            OS_TMR_CALLBACK tmrCallback_UTF;
            putCallbackToTargetState_UTF(&tmrCallback_UTF, states[i]);

            UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel(&tmrCallback_UTF), LW_FALSE);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, states[i]);

            resetOsTmrCallback_UTF();
        }
    }
}

/* ------------------------ Basic requirement tests --------------------------------------------- */
UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackSchedule_basicRequirements,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackSchedule test for requirements")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS(
        "SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackSchedule_basicRequirements]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackSchedule_basicRequirements)
{
    /*!
     * @brief    Ability to abort cancellation.
     * @details  Try to operate between `QUEUED` and `EXELWTED` state.
     *           1. Create a callback and bring it to `QUEUED` state.
     *           2  Cancel the callback:
     *              Expect pending cancellation flag to be set.
     *           3. Schedule the callback:
     *              Expect pending cancellation flag to be cleared.
     *              Expect the callback can still be exelwted.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel(&tmrCallback_UTF), LW_TRUE);
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.bCancelPending, LW_TRUE);
        UT_ASSERT_EQUAL_UINT(osTmrCallbackSchedule(&tmrCallback_UTF), FLCN_OK);
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.bCancelPending, LW_FALSE);
        EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));

        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Schedule multiple callbacks.
     * @details  Schedule callbacks that can cause different queue insertion operations. Expect the
     *           queue of the callbacks to have the right order at any given time.
     */
    {
        LwU32 i = 0;
        OS_TMR_CALLBACK tmrCallbacks_UTF[] = {{0}, {0}, {0}};
        LwU32 callbackPeriods_Tick[]       = { 3,   1,   2};
        for (i = 0; i < sizeof(tmrCallbacks_UTF)/sizeof(OS_TMR_CALLBACK); i++)
        {
            g_callbackPeriodUs_UTF = callbackPeriods_Tick[i] * LWRTOS_TICK_PERIOD_US;
            putCallbackToTargetState_UTF(&(tmrCallbacks_UTF[i]), OS_TMR_CALLBACK_STATE_SCHEDULED);
            // @todo verify order in the queue
        }
        resetOsTmrCallback_UTF();
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackUpdate_basicRequirements,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackUpdate test for requirements")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackUpdate_basicRequirements]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackUpdate_basicRequirements)
{
    /*!
     * @brief    Update scheduled callback
     * @details  1. Bring the callback to `SCHEDULED` state.
     *           2. Fast forward system ticks by 10 ticks.
     *           3. Update the callback.
     *              Expect the callback next trigger time updated accordingly.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;

        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_SCHEDULED);
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.modeData[OsTmrCallback.mode].ticksNext,
            lwrtosTaskGetTickCount32() + LWRTOS_TIME_US_TO_TICKS(g_callbackPeriodUs_UTF));

        lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue += 2U;
        UT_ASSERT(lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue <
            LWRTOS_TIME_US_TO_TICKS(g_callbackPeriodUs_UTF));

        UT_ASSERT_EQUAL_UINT(
            osTmrCallbackUpdate(&tmrCallback_UTF, g_callbackPeriodUs_UTF, g_callbackPeriodUs_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL,
            OS_TIMER_UPDATE_USE_BASE_LWRRENT), FLCN_OK);
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.modeData[OsTmrCallback.mode].ticksNext,
            lwrtosTaskGetTickCount32() + LWRTOS_TIME_US_TO_TICKS(g_callbackPeriodUs_UTF));

        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Update queued callback.
     * @details  1. Bring the callback to `QUEUED` state.
     *           2. Update the callback.
     *              Expect the callback next trigger time updated accordingly.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;

        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);

        UT_ASSERT_EQUAL_UINT(
            osTmrCallbackUpdate(&tmrCallback_UTF, g_callbackPeriodUs_UTF, g_callbackPeriodUs_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL,
            OS_TIMER_UPDATE_USE_BASE_LWRRENT), FLCN_OK);
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.modeData[OsTmrCallback.mode].ticksNext,
            lwrtosTaskGetTickCount32() + LWRTOS_TIME_US_TO_TICKS(g_callbackPeriodUs_UTF));

        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Update exelwted callback.
     * @details  1. Using a callback of `RELWRRENT` type and callback function with update in it.
     *           2. Bring the callback to `QUEUED` state and start exelwtion.
     *              Expect no next tick update during exelwtion (see
     *                  @ref osTmrCallbackFuncWithUpdate_UTF).
     *              Expect next tick update after exelwtion.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        g_callbackType_UTF = OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP;
        g_callbackFncPtr_UTF = osTmrCallbackFuncWithUpdate_UTF;

        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.modeData[OsTmrCallback.mode].ticksNext,
            lwrtosTaskGetTickCount32() + LWRTOS_TIME_US_TO_TICKS(UPDATED_VALID_PERIOD_US_UTF));

        resetOsTmrCallback_UTF();
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackExelwte_basicRequirements,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackExelwte requirements test")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS(
        "SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackExelwte_basicRequirements]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackExelwte_basicRequirements)
{
    /*!
     * @brief    Execute different types of callback.
     * @details  1. Put the callback through `EXELWTED` state.
     *           2. Expect the callback to be called.
     *           3. Expect the callback state afterwards is correct based on the callback type.
     * @todo  Add ticksNext check.
     */
    OS_TMR_CALLBACK_TYPE type;
    for (type = OS_TMR_CALLBACK_TYPE_ONE_SHOT; type < OS_TMR_CALLBACK_TYPE_COUNT; type++)
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        g_callbackType_UTF = type;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));
        if (type == OS_TMR_CALLBACK_TYPE_ONE_SHOT)
        {
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_CREATED);
        }
        else
        {
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_SCHEDULED);
        }
        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Execute callback with pending cancellation.
     * @details  Expect @ref osTmrCallbackExelwte return `FLCN_OK`.
     *           Expect callback function not exelwted.
     *           Expect relwrrent callback to not be reschedule.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        g_callbackType_UTF = OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel(&tmrCallback_UTF), LW_TRUE);
        EXPECT_CALLBACK_NOT_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_CREATED);
        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Cancel callback during exelwtion
     * @details  Expect @ref osTmrCallbackExelwte return `FLCN_OK`.
     *           Expect callback function exelwted.
     *           Expect relwrrent callback to not be reschedule.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        g_callbackType_UTF = OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP;
        g_callbackFncPtr_UTF = osTmrCallbackFuncWithCancel_UTF;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_CREATED);
        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Execute callback that is not due.
     * @details  Update callback after `QUEUED` state.
     *           Expect @ref osTmrCallbackExelwte return `FLCN_OK`.
     *           Expect callback function not exelwted.
     *           Expect callback to be put back to the queue with original schedule.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        UT_ASSERT_EQUAL_UINT(
            osTmrCallbackUpdate(&tmrCallback_UTF, g_callbackPeriodUs_UTF, g_callbackPeriodUs_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL,
            OS_TIMER_UPDATE_USE_BASE_LWRRENT), FLCN_OK);
        EXPECT_CALLBACK_NOT_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_SCHEDULED);
        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    Callback function return ERROR during exelwtion.
     * @details  Expect same ERROR status to be forwarded through @ref osTmrCallbackExelwte.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        g_callbackFncPtr_UTF = osTmrCallbackFuncWithCancel_UTF;
        g_defaultCallbackFuncRet = FLCN_ERR_DMA_NACK;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_ERR_DMA_NACK));
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_EXELWTED);
        resetOsTmrCallback_UTF();
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackCancel_basicRequirements,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackCancel requirements test")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS(
        "SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackCancel_basicRequirements]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackCancel_basicRequirements)
{
    /*!
     * @brief    Callback is canceled twice.
     * @details  Expect the second cancel operation to fail.
     */
    OS_TMR_CALLBACK tmrCallback_UTF;
    putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
    UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel(&tmrCallback_UTF), LW_TRUE);
    UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.bCancelPending, LW_TRUE);
    UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel(&tmrCallback_UTF), LW_FALSE);
}

// @todo Other cases that can be covered.
// @todo Cancel queued callback
// @todo callback not in the list to be canceled

/* ------------------------ Fault injection tests ----------------------------------------------- */
UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackCreate_faultInjection,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackCreate fault injection test")
    UT_CASE_SET_TYPE(FAULT_INJECTION)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackCreate_faultInjection]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackCreate_faultInjection)
{
    /*!
     * @brief    @ref osTmrCallbackUpdate return `ERROR`
     * @details  1. Mock `osTmrCallbackUpdate` to return `ERROR` when called by
     *              @ref osTmrCallbackCreate.
     *           2. Expect `osTmrCallbackCreate` to return FLCN_ERR_ILLEGAL_OPERATION
     *              Expect the created callback in `ERROR` state.
     */
    osTmrCallbackUpdate_MOCK_CONFIG.bOverride = LW_TRUE;
    osTmrCallbackUpdate_MOCK_CONFIG.status = FLCN_ERR_ILLEGAL_OPERATION;

    OS_TMR_CALLBACK tmrCallback_UTF;
    resetCallback_UTF(&tmrCallback_UTF);
    UT_ASSERT_EQUAL_UINT(
        osTmrCallbackCreate(
            &tmrCallback_UTF, OS_TMR_CALLBACK_TYPE_ONE_SHOT, OVL_INDEX_ILWALID, 
            defaultCallbackFunc_UTF, DEFAULT_QUEUE_HANDLE_UTF, DEFAULT_VALID_PERIOD_US_UTF,
            DEFAULT_VALID_PERIOD_US_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL, DEFAULT_TASK_ID),
        FLCN_ERR_ILLEGAL_OPERATION);
    UT_ASSERT_EQUAL_UINT(OS_TMR_CALLBACK_STATE_ERROR, tmrCallback_UTF.state);

    osTmrCallbackUpdate_MOCK_CONFIG.bOverride = LW_FALSE;
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackExelwte_faultInjection,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackExelwte fault injection test")
    UT_CASE_SET_TYPE(FAULT_INJECTION)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackExelwte_faultInjection]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackExelwte_faultInjection)
{
    /*!
     * @brief    Callback type corruption.
     * @details  Expect the callback state to become `ERROR`.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        tmrCallback_UTF.type = OS_TMR_CALLBACK_TYPE_COUNT;
        UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_ERR_ILWALID_STATE);
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_ERROR);
        resetOsTmrCallback_UTF();
    }

    /*!
     * @brief    @ref osTmrCallbackSchedule_MOCK returns error during re-scheduling.
     * @details  Expect the callback state to become `ERROR`.
     */
    {
        OS_TMR_CALLBACK tmrCallback_UTF;
        g_callbackType_UTF = OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED;
        putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_QUEUED);
        osTmrCallbackSchedule_MOCK_CONFIG.bOverride = LW_TRUE;
        osTmrCallbackSchedule_MOCK_CONFIG.status = FLCN_ERR_TIMEOUT;
        EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
                UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF),
                    FLCN_ERR_TIMEOUT));
        UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_ERROR);
        resetOsTmrCallback_UTF();
    }
}

UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackCancel_faultInjection,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackCancel fault injection test")
    UT_CASE_SET_TYPE(INTERFACE)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackCancel_faultInjection]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackCancel_faultInjection)
{
    /*!
     * @brief    Invalid callback state.
     * @details  Expect the cancel operation to fail.
     *           Expect the callback state to become `ERROR`.
     */
    OS_TMR_CALLBACK tmrCallback_UTF;
    tmrCallback_UTF.state = OS_TMR_CALLBACK_STATE_ERROR;
    UT_ASSERT_EQUAL_UINT(LW_FALSE, osTmrCallbackCancel(&tmrCallback_UTF));
    UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.state, OS_TMR_CALLBACK_STATE_ERROR);
}

/* ------------------------ Use case targeting tests -------------------------------------------- */
/*!
 * @brief    Test that verifies statistics mechanism with `RELWRRENT` type `SKIP_MISSED`.
 * @details  Steps:
 *           1. Setup delay and drift for each callback instance.
 *           2. Increase the OS time tick and check the statistics.
 *           3. Expect the statistics being the same as expectation.
 *
 * @note  Current use cases in TU10A pmu profile (2/25/2020):
 *        Only for `task_perf`, with details being:
 *        1. In function @ref perfVfeLoad , a callback is created:
 *           - callback: `OsCBVfeTimer.super`
 *           - type: @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED
 *           - callback function: @ref s_vfeTimerOsCallback
 *           - periods: `(LwU32)(PERF_GET_VFE(&Perf)->pollingPeriodms) * (LwU32)1000U`
 *        2. In file `task_perf.c`, callback notification is waited for and handled:
 *           - In macro @ref LWOS_TASK_LOOP for `task_perf`, @ref osTmrCallbackExelwte is called.
 *           - Other notifications are handled in @ref s_perfEventHandle.
 */
UT_CASE_DEFINE(LWOSTMRCALLBACK, osTmrCallbackExelwte_skipMissedStatistics,
    UT_CASE_SET_DESCRIPTION("osTmrCallbackExelwte statistics verification with skip missed type")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS(
        "SWVS-FUNC-NAME-REF[LWOSTMRCALLBACK_osTmrCallbackExelwte_skipMissedStatistics]"))

UT_CASE_RUN(LWOSTMRCALLBACK, osTmrCallbackExelwte_skipMissedStatistics)
{
    LwU32 i = 0;
    const LwU32 numExec = 5;
    OS_TMR_CALLBACK tmrCallback_UTF;

    // Test setup: drift for each callback instance.
    LwU32 drifts[]                 = {0,  2,  7,  0,  0};
    // Test setup: payload for each callback instance.
    LwU32 payload[]                = {2,  3,  1,  0,  0};
    //
    // Manually callwlated statistics for the callback instances based solely on the setup arrays
    // above.
    // @note  At the moment, only `expectedExpTimes` is compared against an extra formula. But we
    //        can extend it to other statistics and other types in the future.
    //
    LwU32 expectedExpTimes[]       = {3,  6, 12, 21, 24, 27};
    LwU32 ticksTimeExecMin[]       = {2,  2,  1,  1,  1};
    LwU32 ticksTimeExecMax[]       = {2,  3,  3,  3,  3};
    LwU32 totalTicksTimeExelwted[] = {2,  5,  6,  7,  8};
    LwU32 totalTicksTimeDrifted[]  = {0,  2,  9,  9,  9};

    LwU32 scheduledTime = 0;
    LwU32 expectedExpTime = 3;

    // Put callback to scheduled state to start with.
    g_callbackType_UTF = OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED;
    g_callbackPeriodUs_UTF = 3 * LWRTOS_TICK_PERIOD_US;
    putCallbackToTargetState_UTF(&tmrCallback_UTF, OS_TMR_CALLBACK_STATE_SCHEDULED);

    while (i < numExec)
    {
        lwrtosHookTimerTick(lwrtosTaskGetTickCount32());
        utf_printf("tick: %d, exp: %d, drift: %d, delay: %d, state: %d\n",
            lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue,
            tmrCallback_UTF.modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksNext,
            drifts[i],
            payload[i],
            tmrCallback_UTF.state);

        //
        // When the callback is not expired, @ref osTmrCallbackExelwte should return
        // @ref FLCN_ERR_ILLEGAL_OPERATION.
        // Otherwise:
        // 1. Forward OS time tick by the amount of drift.
        // 2. Callback should be able to be exelwted with expiration time updated.
        // 3. Check the statistics after.
        //
        if (lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue < expectedExpTime)
        {
            EXPECT_CALLBACK_NOT_EXELWTED(&tmrCallback_UTF,
                UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF),
                    FLCN_ERR_ILLEGAL_OPERATION));
        }
        else
        {
            LwU32 time = 0;
            lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue += drifts[i];
            time = lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue;
            g_defaultCallbackFuncPayload = payload[i];
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksNext,
                expectedExpTimes[i]);

            EXPECT_CALLBACK_EXELWTED(&tmrCallback_UTF,
                UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(&tmrCallback_UTF), FLCN_OK));

            expectedExpTime += ((drifts[i] + payload[i]) / 3) * 3 + 3;
            UT_ASSERT_EQUAL_UINT(expectedExpTime, expectedExpTimes[i+1]);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksNext,
                expectedExpTimes[i+1]);

            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.stats.ticksTimeExecMin, ticksTimeExecMin[i]);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.stats.ticksTimeExecMax, ticksTimeExecMax[i]);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.stats.totalTicksTimeExelwted,
                totalTicksTimeExelwted[i]);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.stats.totalTicksTimeDrifted,
                totalTicksTimeDrifted[i]);
            UT_ASSERT_EQUAL_UINT(tmrCallback_UTF.stats.countExec, i+1);

            UT_ASSERT_EQUAL_UINT(time + payload[i],
                lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue);
            i++;
        }
        lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue++;
    }

    resetOsTmrCallback_UTF();
}

/* ------------------------ Static function implementations ------------------------------------- */
// @todo Maybe combine the callback functions
static FLCN_STATUS defaultCallbackFunc_UTF(OS_TMR_CALLBACK *pCallback)
{
    // check if previous callback exelwtion is cleared
    UT_ASSERT_EQUAL_UINT(s_ifCallbackFnExelwted(pCallback), LW_FALSE);
    UT_ASSERT_EQUAL_UINT(s_registerCallbackExelwtion(pCallback), LW_TRUE);
    lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue += g_defaultCallbackFuncPayload;
    utf_printf("Default callback function performed.\n");
    return g_defaultCallbackFuncRet;
}

static FLCN_STATUS osTmrCallbackFuncWithUpdate_UTF(OS_TMR_CALLBACK *pCallback)
{
    UT_ASSERT_EQUAL_UINT(
        osTmrCallbackUpdate(pCallback, UPDATED_VALID_PERIOD_US_UTF, UPDATED_VALID_PERIOD_US_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL,
            OS_TIMER_UPDATE_USE_BASE_LWRRENT), FLCN_OK);
    UT_ASSERT_NOT_EQUAL_UINT(pCallback->modeData[OsTmrCallback.mode].ticksNext,
            lwrtosTaskGetTickCount32() + LWRTOS_TIME_US_TO_TICKS(UPDATED_VALID_PERIOD_US_UTF));
    utf_printf("Callback function with period update performed.\n");
    return defaultCallbackFunc_UTF(pCallback);
}

static FLCN_STATUS osTmrCallbackFuncWithCancel_UTF(OS_TMR_CALLBACK *pCallback)
{
    UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel(pCallback), LW_TRUE);
    utf_printf("Callback function with cancel performed.\n");
    return defaultCallbackFunc_UTF(pCallback);
}

static void restoreCallbackDefault()
{
    LwU32 i = 0;
    g_callbackFncPtr_UTF = defaultCallbackFunc_UTF;
    g_callbackType_UTF = OS_TMR_CALLBACK_TYPE_ONE_SHOT;
    g_callbackPeriodUs_UTF = 3U * LWRTOS_TICK_PERIOD_US;
    g_defaultCallbackFuncRet = FLCN_OK;
    g_defaultCallbackFuncPayload = 0;
    for (i = 0; i < sizeof(callbackFuncsInfo)/sizeof(UTF_CALLBACK_FUNC_INFO); i++)
    {
        callbackFuncsInfo[i].bExelwted = LW_FALSE;
        callbackFuncsInfo[i].pCallback = NULL;
    }
}

static void resetCallback_UTF(const OS_TMR_CALLBACK *pCallback)
{
    if (pCallback != NULL)
    {
        UT_ASSERT_EQUAL_UINT(osTmrCallbackCancel_MOCK_CONFIG.bOverride,
            LW_FALSE);
        osTmrCallbackCancel(pCallback);
        memset(pCallback, 0, sizeof(OS_TMR_CALLBACK));
        UT_ASSERT_EQUAL_UINT(ifCallbackInDefault(pCallback), LW_TRUE);
    }
}

static void resetOsTmrCallback_UTF()
{
    restoreCallbackDefault();
    while (OsTmrCallback.headHandle[OS_TMR_CALLBACK_MODE_NORMAL] != CB_MAP_HANDLE_NULL)
    {
        utf_printf("Removing callback...\n");
        resetCallback_UTF(LwOsTmrCallbackMap[OsTmrCallback.headHandle[OS_TMR_CALLBACK_MODE_NORMAL]]);
    }
    memset(&OsTmrCallback, 0, sizeof(OS_TMR_CALLBACK_STRUCT));
    OsTmrCallback.mode = OS_TMR_CALLBACK_MODE_NORMAL;
    lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue = 0;
    utf_printf("Callback list cleared.\n");
}

static void perTestSetupFn()
{
    UT_ASSERT_EQUAL_UINT(ifOsTmrCallbackInDefault(), LW_TRUE);

    // enable mocking for used external function
    lwrtosTaskGetTickCount32_MOCK_CONFIG.bOverride = LW_TRUE;
    lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue = 0;
    lwrtosQueueSendFromISRWithStatus_MOCK_CONFIG.bOverride = LW_TRUE;
    lwrtosQueueSendFromISRWithStatus_MOCK_CONFIG.status = FLCN_OK;
    lwosWatchdogAddItemFromISR_MOCK_CONFIG.bOverride = LW_TRUE;
    lwosWatchdogAddItemFromISR_MOCK_CONFIG.status = FLCN_OK;

    // disable mocking for all functions under test
    osTmrCallbackCreate_MOCK_CONFIG.bOverride = LW_FALSE;
    osTmrCallbackSchedule_MOCK_CONFIG.bOverride = LW_FALSE;
    osTmrCallbackUpdate_MOCK_CONFIG.bOverride = LW_FALSE;
    osTmrCallbackExelwte_MOCK_CONFIG.bOverride = LW_FALSE;
    osTmrCallbackCancel_MOCK_CONFIG.bOverride = LW_FALSE;

    resetOsTmrCallback_UTF();

    UT_ASSERT_EQUAL_UINT(OsTmrCallback.mode, OS_TMR_CALLBACK_MODE_NORMAL);
    UT_ASSERT_EQUAL_UINT(OsTmrCallback.headHandle[OS_TMR_CALLBACK_MODE_NORMAL],
        CB_MAP_HANDLE_NULL);

    LwOsTmrCallbackMapHandleMax = 30;
    // Create space for the map
    if (LwOsTmrCallbackMap != NULL)
    {
        LwOsTmrCallbackMap = (OS_TMR_CALLBACK **)lwosCallocResident(LwOsTmrCallbackMapHandleMax * sizeof(OS_TMR_CALLBACK *));
    }
    // Assign NULL to index 0 and increase map count by 1
    LwOsTmrCallbackMap[cbMapCount++] = NULL;

    utf_printf("Successfully performed setup.\n");
}

static void perTestTeardownFn()
{
    // disable mocking for used external function
    lwrtosTaskGetTickCount32_MOCK_CONFIG.bOverride = LW_FALSE;
    lwrtosQueueSendFromISRWithStatus_MOCK_CONFIG.bOverride = LW_FALSE;
    lwosWatchdogAddItemFromISR_MOCK_CONFIG.bOverride = LW_FALSE;

    for (LwU8 i = 0; i < LwOsTmrCallbackMapHandleMax; i++)
    {
        LwOsTmrCallbackMap[i] = NULL;
    }
    cbMapCount = 0;

    utf_printf("Successfully performed teardown.\n");
}

// @todo  Implement this.
static void verifyCallbackIntegrity(OS_TMR_CALLBACK *pCallback)
{
    UT_ASSERT_NOT_EQUAL_UINT(pCallback, NULL);
    switch (pCallback->state)
    {
        case OS_TMR_CALLBACK_STATE_START:
        case OS_TMR_CALLBACK_STATE_ERROR:
            break;
        case OS_TMR_CALLBACK_STATE_CREATED:
            // valid fields
            break;
    }
    return LW_TRUE;
}

static LwBool ifOsTmrCallbackModeValid()
{
    return (OsTmrCallback.mode < OS_TMR_CALLBACK_MODE_COUNT) ? LW_TRUE : LW_FALSE;
}

static LwBool ifCallbackInDefault(OS_TMR_CALLBACK *pCallback)
{
    UT_ASSERT_NOT_EQUAL_UINT(pCallback, NULL);
    return (pCallback->state == OS_TMR_CALLBACK_STATE_START) ? LW_TRUE : LW_FALSE;
}

static LwBool ifOsTmrCallbackInDefault()
{
    LwU32 i = 0;

    for (i = 0; i < OS_TMR_CALLBACK_MODE_COUNT; i++)
    {
        if (OsTmrCallback.headHandle[i] != CB_MAP_HANDLE_NULL)
        {
            return LW_FALSE;
        }
    }

    return ifOsTmrCallbackModeValid();
}

static void putCallbackToTargetState_UTF(OS_TMR_CALLBACK *pCallback, OS_TMR_CALLBACK_STATE state)
{
    UT_ASSERT_NOT_EQUAL_UINT(pCallback, NULL);
    resetCallback_UTF(pCallback);
    s_putCallbackToTargetState_UTF(pCallback, state);
    // @todo may skip
    verifyCallbackIntegrity(pCallback);
}

static void s_putCallbackToTargetState_UTF(OS_TMR_CALLBACK *pCallback, OS_TMR_CALLBACK_STATE state)
{

    switch (state)
    {
        case OS_TMR_CALLBACK_STATE_START:
            resetCallback_UTF(pCallback);
            break;
        case OS_TMR_CALLBACK_STATE_CREATED:
            s_putCallbackToTargetState_UTF(pCallback, OS_TMR_CALLBACK_STATE_START);
            UT_ASSERT_EQUAL_UINT(osTmrCallbackCreate(pCallback, g_callbackType_UTF,
                OVL_INDEX_ILWALID, g_callbackFncPtr_UTF, DEFAULT_QUEUE_HANDLE_UTF,
                g_callbackPeriodUs_UTF, g_callbackPeriodUs_UTF, OS_TIMER_RELAXED_MODE_USE_NORMAL, DEFAULT_TASK_ID), FLCN_OK);
            break;
        case OS_TMR_CALLBACK_STATE_SCHEDULED:
            s_putCallbackToTargetState_UTF(pCallback, OS_TMR_CALLBACK_STATE_CREATED);
            UT_ASSERT_EQUAL_UINT(osTmrCallbackSchedule(pCallback), FLCN_OK);
            break;
        case OS_TMR_CALLBACK_STATE_QUEUED:
            s_putCallbackToTargetState_UTF(pCallback, OS_TMR_CALLBACK_STATE_SCHEDULED);
            lwrtosTaskGetTickCount32_MOCK_CONFIG.returlwalue =
                pCallback->modeData[OsTmrCallback.mode].ticksNext;
            lwrtosHookTimerTick(lwrtosTaskGetTickCount32());
            break;
        case OS_TMR_CALLBACK_STATE_EXELWTED:
            s_putCallbackToTargetState_UTF(pCallback, OS_TMR_CALLBACK_STATE_QUEUED);
            UT_ASSERT_EQUAL_UINT(osTmrCallbackExelwte(pCallback), FLCN_OK);
            break;
        default:
            s_putCallbackToTargetState_UTF(pCallback, OS_TMR_CALLBACK_STATE_START);
            pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
            break;
    }
}

static void
verifyCallbacksLinks
(
    OS_TMR_CALLBACK *pCallback_A,
    OS_TMR_CALLBACK *pCallback_B,
    OS_TMR_CALLBACK_MODE mode
)
{
    OS_TMR_CALLBACK *pCallback_A_pNext =
        (pCallback_A == NULL) ? pCallback_B : LwOsTmrCallbackMap[pCallback_A->modeData[mode].nextHandle];
    OS_TMR_CALLBACK *pCallback_B_pPrev =
        (pCallback_B == NULL) ? pCallback_A : LwOsTmrCallbackMap[pCallback_B->modeData[mode].prevHandle];
    UT_ASSERT_EQUAL_UINT(pCallback_A_pNext, pCallback_B);
    UT_ASSERT_EQUAL_UINT(pCallback_B_pPrev, pCallback_A);
}

static void verifyOsTmrCallbackIntegrity()
{
    if (ifOsTmrCallbackInDefault() == LW_FALSE)
    {
        OS_TMR_CALLBACK_MODE mode = 0;
        for (mode = 0; mode < OS_TMR_CALLBACK_MODE_COUNT; mode++)
        {
            OS_TMR_CALLBACK *pCallback_prev = NULL;
            OS_TMR_CALLBACK *pCallback_lwrr = LwOsTmrCallbackMap[OsTmrCallback.headHandle[mode]];
            verifyCallbacksLinks(pCallback_prev, pCallback_lwrr, mode);
            while (pCallback_lwrr != NULL)
            {
                verifyCallbackIntegrity(pCallback_lwrr);
                verifyCallbacksLinks(pCallback_prev, pCallback_lwrr, mode);
                UT_ASSERT_EQUAL_UINT(pCallback_lwrr->state, OS_TMR_CALLBACK_STATE_SCHEDULED);
                pCallback_prev = pCallback_lwrr;
                pCallback_lwrr = LwOsTmrCallbackMap[pCallback_lwrr->modeData[mode].nextHandle];
            }
            verifyCallbacksLinks(pCallback_prev, pCallback_lwrr, mode);
        }
    }
}

/* ------------------------ Private static function implementations ----------------------------- */
static LwBool s_ifCallbackFnExelwted(OS_TMR_CALLBACK *pCallback)
{
    LwU32 i = 0;
    LwBool bExelwted = LW_FALSE;

    UT_ASSERT_NOT_EQUAL_UINT(pCallback, NULL);
    UT_ASSERT_NOT_EQUAL_UINT(pCallback->pTmrCallbackFunc, NULL);

    for (i = 0; i < sizeof(callbackFuncsInfo)/sizeof(UTF_CALLBACK_FUNC_INFO); i++)
    {
        if (pCallback->pTmrCallbackFunc == callbackFuncsInfo[i].pFunc)
        {
            bExelwted = callbackFuncsInfo[i].bExelwted;
            callbackFuncsInfo[i].bExelwted = LW_FALSE;
            callbackFuncsInfo[i].pCallback = NULL;
            return bExelwted;
        }
    }
    // @todo add error reporting here.
    utf_printf("Callback function does not exist.\n");
    return LW_FALSE;
}

static LwBool s_registerCallbackExelwtion(OS_TMR_CALLBACK *pCallback)
{
    LwU32 i = 0;
    LwBool bExelwted = LW_FALSE;

    UT_ASSERT_NOT_EQUAL_UINT(pCallback, NULL);
    UT_ASSERT_NOT_EQUAL_UINT(pCallback->pTmrCallbackFunc, NULL);

    for (i = 0; i < sizeof(callbackFuncsInfo)/sizeof(UTF_CALLBACK_FUNC_INFO); i++)
    {
        if (pCallback->pTmrCallbackFunc == callbackFuncsInfo[i].pFunc)
        {
            UT_ASSERT_EQUAL_UINT(callbackFuncsInfo[i].bExelwted, LW_FALSE);
            callbackFuncsInfo[i].bExelwted = LW_TRUE;
            callbackFuncsInfo[i].pCallback = pCallback;
            return LW_TRUE;
        }
    }
    // @todo add error reporting here.
    utf_printf("Callback function does not exist.\n");
    return LW_FALSE;
}
