/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file watchdog_tu10x-test.c
 *
 * @details    This file contains all the unit tests for the Unit Watchdog
 *             Timer. The tests utilize the register mocking framework to ensure
 *             the correct values are being "written" to the hardware watchdog
 *             registers.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"
#include <ut/ut.h>
#include "ut/ut_suite.h"
#include "ut/ut_case.h"
#include "ut/ut_assert.h"
#include "ut/ut_fake_mem.h"

/* ------------------------ Application Includes ---------------------------- */
#include "csb.h"
#include "dev_pwr_csb.h"
#include "pmu_bar0.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifpmu.h"

/* ------------------------ Local Variables --------------------------------- */
/*!
 * @brief      Fake @ref RM_PMU_CMD_LINE_ARGS so that the Unit Watchdog Timer
 *             can callwlate the proper timeout values.
 */
RM_PMU_CMD_LINE_ARGS PmuInitArgs;

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief      Macro to generate the pre-test function prototype.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The pre-test function prototype.
 */
#define PRE_TEST_METHOD(name) \
    static void PRE_TEST_NAME(name)()

/*!
 * @brief      Macro to generate the pre-test function name.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The pre-test function name.
 */
#define PRE_TEST_NAME(name) \
    s_##name##PreTest

/*!
 * @brief      Macro to generate the post-test function prototype.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The post-test function prototype.
 */
#define POST_TEST_METHOD(name) \
    static void POST_TEST_NAME(name)()

/*!
 * @brief      Macro to generate the post-test function name.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The post-test function name.
 */
#define POST_TEST_NAME(name) \
    s_##name##PostTest

/* -------------------------------------------------------- */
/*!
 * @brief      Definition of the Unit Watchdog Timer test suite for libUT.
 */
UT_SUITE_DEFINE(WATCHDOGTIMER,
                UT_SUITE_SET_COMPONENT("Unit Watchdog Timer")
                UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Watchdog Timer")
                UT_SUITE_SET_OWNER("vrazdan")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))


/*!
 * @brief      Test the method to stop the hardware watchdog timer register.
 */
UT_CASE_DEFINE(WATCHDOGTIMER, stop_Valid,
  UT_CASE_SET_DESCRIPTION("Ensure timerWatchdogStop_TU10X programs the watchdog to stop.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for stopping the watchdog test.
 *
 * @details    Before the test can run, the watchdog control register must be
 *             mocked using @ref UTF_IO_MOCK so that the code under test does
 *             not actually program the hardware register.
 *
 *             Additionally, the value 0x3 is written to the mocked register so
 *             that later when the code under test "writes" to the register, it
 *             is possible to prove that only the bit that was supposed to be
 *             set was set.
 */
PRE_TEST_METHOD(stop_Valid)
{
    UTF_IO_MOCK(LW_CPWR_FALCON_WDTMRCTL, LW_CPWR_FALCON_WDTMRCTL, NULL, NULL);

    REG_WR32(CSB, LW_CPWR_FALCON_WDTMRCTL, 0x3U);
}

/*!
 * @brief      Test that the Unit Watchdog Timer correctly stops the watchdog.
 *
 * @details    The test shall ilwoke the Unit Watchdog Timer method to stop the
 *             watchdog timer. The test shall verify the watchdog was stopped by
 *             verifying that the register LW_CPWR_FALCON_WDTMRCTL has the
 *             bitfield LW_CPWR_FALCON_WDTMRCTL_WDTMREN set to
 *             LW_CPWR_FALCON_WDTMRCTL_WDTMREN_DISABLE.
 */
UT_CASE_RUN(WATCHDOGTIMER, stop_Valid)
{
    LwU32 writtenRegisterValue = 0U;

    PRE_TEST_NAME(stop_Valid)();

    // Have to extern to mute compiler warning about undefined methods.
    extern void timerWatchdogStop_TU10X(void);
    timerWatchdogStop_TU10X();

    writtenRegisterValue = REG_RD32(CSB, LW_CPWR_FALCON_WDTMRCTL);
    UT_ASSERT_EQUAL_UINT(writtenRegisterValue, 0x2U);
}

/* -------------------------------------------------------- */
/*!
 * @brief      Test the method to pet the hardware watchdog timer register.
 */
UT_CASE_DEFINE(WATCHDOGTIMER, pet_Valid,
  UT_CASE_SET_DESCRIPTION("Ensure timerWatchdogPet_TU10X programs the watchdog with the correct time.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for petting the watchdog test.
 *
 * @details    Before the test can run, the watchdog timer value register must
 *             be mocked using @ref UTF_IO_MOCK so that the code under test does
 *             not actually program the hardware register.
 *
 *             Additionally, the value 0x0 is written to the mocked register so
 *             that later when the code under test "writes" to the register, it
 *             is possible to prove that the correct value was written, and not
 *             that the value was already set.
 *
 *             Finally, the code under test uses a macro which assumes the
 *             global variable @ref PmuInitArgs is present and has the proper
 *             values programmed. The pre-test setup ensures that the CPU
 *             frequency member of the structure is set to 540 MHz as that is
 *             the frequency that shall  be used in the production system. This
 *             way, the test may evaluate that the correct value is programmed
 *             to the register.
 */
PRE_TEST_METHOD(pet_Valid)
{
    UTF_IO_MOCK(LW_CPWR_FALCON_WDTMRVAL, LW_CPWR_FALCON_WDTMRVAL, NULL, NULL);

    REG_WR32(CSB, LW_CPWR_FALCON_WDTMRVAL, 0x0U);

    PmuInitArgs.cpuFreqHz = 540000000U;
}

/*!
 * @brief      Post-test teardown after petting the watchdog test.
 *
 * @details    The @ref PmuInitArgs structure member cpuFreqHz is initialized
 *             globally to zero, but the pre-test sets it to 540 MHz. In order
 *             to make sure that each test operates in a clean, isolated
 *             fashion, the post-test method sets the cpuFreqHz back to 0. This
 *             way, each test may run with the assumption that all global
 *             variables and state have their initial values when the pre-test
 *             begins.
 */
POST_TEST_METHOD(pet_Valid)
{
    PmuInitArgs.cpuFreqHz = 0U;
}

/*!
 * @brief      Test that the Unit Watchdog Timer successfully pets the watchdog
 *             timer.
 *
 * @details    The test shall ilwoke the Unit Watchdog Timer method that pets
 *             the watchdog timer. The test shall verify that the code under
 *             test ran correctly by comparing the value that was written to the
 *             LW_CPWR_FALCON_WDTMRVAL register matches the expected time,
 *             callwlated using the PMU clock frequency. The test does not use
 *             the same macro that the code under test uses, to ensure that all
 *             the flags and values are set correctly during compilation of the
 *             unit test itself.
 */
UT_CASE_RUN(WATCHDOGTIMER, pet_Valid)
{
    LwU32 writtenRegisterValue = 0U;
    LwU32 watchdogTimeProgramValue = 0U;

    PRE_TEST_NAME(pet_Valid)();

    // Have to extern to mute compiler warning about undefined methods.
    extern void timerWatchdogPet_TU10X();
    timerWatchdogPet_TU10X();

    POST_TEST_NAME(pet_Valid)();

    writtenRegisterValue = REG_RD32(CSB, LW_CPWR_FALCON_WDTMRVAL);

    //
    // NOTE- This is hardcoded to match the math done in task_watchdog.h
    //
    watchdogTimeProgramValue = 5U * (1000U / 30U) * (540000000U / 33333U);

    UT_ASSERT_EQUAL_UINT(writtenRegisterValue, watchdogTimeProgramValue);
}

/* -------------------------------------------------------- */
/*!
 * @brief      Test the method to initialize the hardware watchdog timer register.
 */
UT_CASE_DEFINE(WATCHDOGTIMER, init_Valid,
  UT_CASE_SET_DESCRIPTION("Ensure timerWatchdogInit_TU10X programs the watchdog with the correct time and clock source.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("todo")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for initializing the watchdog test.
 *
 * @details    Before the test can run, the watchdog timer value register and
 *             watchdog control register must be mocked using @ref UTF_IO_MOCK
 *             so that the code under test does not actually program the
 *             hardware registers.
 *
 *             The value 0x0 is written to the mocked watchdog timer value
 *             register so that later when the code under test "writes" to the
 *             register, it is possible to prove that the correct value was
 *             written, and not that the value was already set.
 *
 *             The value 0x2 is written to the mocked watchdog timer control
 *             register so that later when the code under test "writes" to the
 *             register, it is possible to prove that the correct value was
 *             written, and not that the value was already set, as the correct
 *             value is the 2-bit bit ilwerse of 0x2 (0x1).
 *
 *             Finally, the code under test uses a macro which assumes the
 *             global variable @ref PmuInitArgs is present and has the proper
 *             values programmed. The pre-test setup ensures that the CPU
 *             frequency member of the structure is set to 540 MHz as that is
 *             the frequency that shall be used in the production system. This
 *             way, the test may evaluate that the correct value is programmed
 *             to the register.
 */
PRE_TEST_METHOD(init_Valid)
{
    UTF_IO_MOCK(LW_CPWR_FALCON_WDTMRVAL, LW_CPWR_FALCON_WDTMRVAL, NULL, NULL);
    UTF_IO_MOCK(LW_CPWR_FALCON_WDTMRCTL, LW_CPWR_FALCON_WDTMRCTL, NULL, NULL);

    // set to 2 so that init sets bit 1 to 1 and bit 2 to 0
    REG_WR32(CSB, LW_CPWR_FALCON_WDTMRCTL, 0x2U);
    REG_WR32(CSB, LW_CPWR_FALCON_WDTMRVAL, 0x0U);

    PmuInitArgs.cpuFreqHz = 540000000U;
}

/*!
 * @brief      Post-test teardown after initializing the watchdog test.
 *
 * @details    The @ref PmuInitArgs structure member cpuFreqHz is initialized
 *             globally to zero, but the pre-test sets it to 540 MHz. In order
 *             to make sure that each test operates in a clean, isolated
 *             fashion, the post-test method sets the cpuFreqHz back to 0. This
 *             way, each test may run with the assumption that all global
 *             variables and state have their initial values when the pre-test
 *             begins.
 */
POST_TEST_METHOD(init_Valid)
{
    PmuInitArgs.cpuFreqHz = 0U;
}

/*!
 * @brief      Test that the Unit Watchdog Timer successfully initializes the
 *             watchdog timer and control registers.
 *
 * @details    The test shall ilwoke the Unit Watchdog Timer method that
 *             initializes the watchdog timer. The test shall verify that the
 *             code under test ran correctly by comparing the value that was
 *             written to the LW_CPWR_FALCON_WDTMRVAL register matches the
 *             expected time, callwlated using the PMU clock frequency. The test
 *             does not use the same macro that the code under test uses, to
 *             ensure that all the flags and values are set correctly during
 *             compilation of the unit test itself.
 *
 *             The test shall verify that the code under test also programmed
 *             the LW_CPWR_FALCON_WDTMRCTL register correctly, making sure that
 *             LW_CPWR_FALCON_WDTMRCTL_WDTMREN bit field is set to _ENABLE and
 *             LW_CPWR_FALCON_WDTMRCTL_WDTMR_SRC_MODE bit field is set to
 *             _ENGCLK.
 */
UT_CASE_RUN(WATCHDOGTIMER, init_Valid)
{
    LwU32 wdtmrval = 0U;
    LwU32 wdtmrctl = 0U;
    LwU32 watchdogTimeProgramValue = 0U;

    PRE_TEST_NAME(init_Valid)();

    // Have to extern to mute compiler warning about undefined methods.
    extern void timerWatchdogInit_TU10X();
    timerWatchdogInit_TU10X();

    POST_TEST_NAME(init_Valid)();

    wdtmrval = REG_RD32(CSB, LW_CPWR_FALCON_WDTMRVAL);
    wdtmrctl = REG_RD32(CSB, LW_CPWR_FALCON_WDTMRCTL);

    //
    // NOTE- This is hardcoded to match the math done in task_watchdog.h
    //
    watchdogTimeProgramValue = 5U * (1000U / 30U) * (540000000U / 33333U);

    UT_ASSERT_EQUAL_UINT(wdtmrval, watchdogTimeProgramValue);
    UT_ASSERT_EQUAL_UINT(wdtmrctl, 0x1U);
}
