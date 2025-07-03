/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file        harness.c
 * @brief       Test harness.
 */

// Standard includes.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// LIBLWRISCV includes.
#include <liblwriscv/print.h>

// Local includes.
#include "hal.h"
#include "harness.h"
#include "suites.h"


/*!
 * @brief Reports internal errors to the user in a consistent format.
 *
 * @param[in] message  A null-terminated string describing the error.
 */
#define HARNESS_INTERNAL_ERROR(message) \
    (printf("\n*** Internal Error: %s (%s)! ***\n\n", (message), __func__))


// Internal state for the current test suite.
static struct HARNESS_SUITE_STATE
{
    //
    // Whether a test suite is lwrrently running. The remaining fields in this
    // structure are only valid when bActive is true.
    //
    bool bActive;

    //
    // The number of tests run by the current test suite so far (including the
    // lwrrently-exelwting test, if one exists).
    //
    uint16_t testCount;

    //
    // The number of tests in the current test suite that have passed so far
    // (not including the lwrrently-exelwting test, if one exists).
    //
    uint16_t passCount;

} s_suiteState;

// Internal state for the current test case.
static struct HARNESS_TEST_STATE
{
    //
    // Whether a test case is lwrrently running. The remaining fields in this
    // structure are only valid when bActive is true.
    //
    bool bActive;

    //
    // A bitfield representing which exceptions (if any) the current test case
    // is expected to trigger.
    //
    HARNESS_EXCEPTION_MASK expectedExceptions;

    //
    // A bitfield representing which exceptions (if any) the current test case
    // has actually triggered.
    //
    HARNESS_EXCEPTION_MASK receivedExceptions;

} s_testState;

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Initializes the testing environment and exelwtes all test suites.
 */
void
harnessMain(void)
{   
    void (* const * pLwrrentSuite)(void) = NULL;
    
    // Report start of testing.
    puts("\nBeginning tests...\n\n");

    // Run setup.
    suiteSetup();

    // No point proceeding if setup fails.
    if(!harnessSuitePassed())
    {
        printf("Setup failed! Aborting tests...\n\n");
        goto abort;
    }

    // Execute test suites.
    for (pLwrrentSuite = g_suiteList; *pLwrrentSuite != NULL; pLwrrentSuite++)
    {
        (*pLwrrentSuite)();
    }

abort:
    // Perform clean-up operations.
    suiteTeardown();

    // Draw user's attention to potential stale state.
    if(!harnessSuitePassed())
    {
        printf("Clean-up failed - be wary of stale state!\n\n");
    }

    // Report end of testing.
    printf("Tests complete. See above for details.\n");
}

/*!
 * @brief RISC-V exception-handler.
 *
 * @param[in] mepc    Address of the instruction that triggered the exception.
 * @param[in] mcause  Code indicating the event that caused the exception.
 * @param[in] mtval   Additional exception-specific information.
 *
 * @return
 *    true  if the wrapper function should return to $ra instead of $mepc.
 *    false if the wrapper function should return to $mepc normally.
 *
 * @note Called by exceptionWrapper in trap.S. Should not be called directly.
 */
bool
harnessExceptionHandler
(
    uint64_t mepc,
    uint64_t mcause,
    uint64_t mtval
)
{   
    // Default to returning to $mepc.
    bool bReturn = false;
    
    // Handle exception based on type.
    switch (halGetExceptionType(mcause))
    {
        // Instruction-access-fault.
        case HAL_EXCEPTION_TYPE_IACC:
            {
                // Record exception for current test.
                harnessRecordException(HARNESS_EXCEPTION_MASK_IACC);

                // Tell the wrapper function to return to $ra instead of $mepc.
                bReturn = true;
            }
            break;
        
        // Load-access-fault.
        case HAL_EXCEPTION_TYPE_LACC:
            {
                // Record exception for current test.
                harnessRecordException(HARNESS_EXCEPTION_MASK_LACC);
            }
            break;

        // System call.
        case HAL_EXCEPTION_TYPE_CALL:
            {
                //
                // Only supported system call is utilXToM(), so switch our
                // return mode to m-mode.
                //
                HAL_SET_RETURN_MODE(MACHINE);
            }
            break;

        // Other.
        default:
            {
                // Record an unexpected exception.
                harnessRecordException(HARNESS_EXCEPTION_MASK_UNEX);

                // Report the details of the exception to the user.
                printf("\tException! mepc: %llu, mcause: %llu, mtval: %llu.\n",
                    mepc, mcause, mtval);
            }
            break;
    }

    // Skip the offending instruction.
    halSkipInstruction();

    // Return to the new $mepc (or $ra if overridden).
    return bReturn;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Marks the start of a new test suite.
 *
 * @param[in] pSuiteName  A null-terminated string describing the test suite.
 */
void
harnessBeginSuite
(
    const char *pSuiteName
)
{
    // Sanity checks.
    if (s_suiteState.bActive || pSuiteName == NULL)
    {
        HARNESS_INTERNAL_ERROR("Invalid function call");
        return;
    }

    // Record start of test suite.
    printf("=== Test Suite: %s ===\n", pSuiteName);

    // Reset internal state.
    s_suiteState.bActive = true;
    s_suiteState.passCount = 0;
    s_suiteState.testCount = 0;
}

/*!
 * @brief Marks the end of the current test suite.
 */
void
harnessEndSuite(void)
{
    // Sanity checks.
    if (!s_suiteState.bActive || s_testState.bActive)
    {
        HARNESS_INTERNAL_ERROR("Invalid function call");
        return;
    }

    // Record end of test suite and summarize results.
    printf("=== Finished: %u / %u ===\n\n", s_suiteState.passCount,
        s_suiteState.testCount);

    // Ilwalidate internal state.
    s_suiteState.bActive = false;
}

/*!
 * @brief Marks the start of a new test case.
 *
 * @param[in] pTestName  A null-terminated string describing the test case.
 */
void
harnessBeginTest
(
    const char *pTestName
)
{
    // Sanity checks.
    if (!s_suiteState.bActive || s_testState.bActive || pTestName == NULL)
    {
        HARNESS_INTERNAL_ERROR("Invalid function call");
        return;
    }

    // Record start of test case.
    printf("%s:\t", pTestName);

    // Reset internal state.
    s_testState.bActive = true;
    s_testState.expectedExceptions = HARNESS_EXCEPTION_MASK_NONE;
    s_testState.receivedExceptions = HARNESS_EXCEPTION_MASK_NONE;

    // Update test count.
    s_suiteState.testCount++;
}

/*!
 * @brief Marks the end of the current test case.
 *
 * @param[in] bResult  Whether the test has passed, ignoring exceptions.
 *
 * @note Exceptions are checked separately from bResult.
 */
void
harnessEndTest
(
    bool bResult
)
{
    // Sanity checks.
    if(!s_testState.bActive)
    {
        HARNESS_INTERNAL_ERROR("Invalid function call");
        return;
    }

    // Determine whether test passed.
    const bool bPassed = bResult &&
        (s_testState.expectedExceptions == s_testState.receivedExceptions);

    // Record end of test case and summarize result.
    puts(bPassed ? "PASS.\n" : "FAIL.\n");

    // Ilwalidate internal state.
    s_testState.bActive = false;

    // Update pass count.
    s_suiteState.passCount = (uint16_t)(s_suiteState.passCount + bPassed);
}

/*!
 * @brief Specifies which exceptions the current test is expected to trigger.
 *
 * @param[in] exceptionMask  A bitfield of expected exception types.
 *
 * @note A test that does not call this function expects no exceptions.
 */
void
harnessExpectException
(
    HARNESS_EXCEPTION_MASK exceptionMask
)
{
    // Sanity check.
    if (!s_testState.bActive)
    {
        HARNESS_INTERNAL_ERROR("Invalid function call");
        return;
    }

    // Update internal state.
    s_testState.expectedExceptions = exceptionMask;
}

/*!
 * @brief Records that the specified exceptions were triggered by the current test.
 *
 * @param[in] exceptionMask  A bitfield of the triggered exception types.
 *
 * @note Calls to this function are strictly additive (exceptions are never
 *       cleared once set).
 */
void
harnessRecordException
(
    HARNESS_EXCEPTION_MASK exceptionMask
)
{
    // Sanity check.
    if (!s_testState.bActive)
    {
        HARNESS_INTERNAL_ERROR("Exception encountered when not running test");
        return;
    }

    // Update internal state.
    s_testState.receivedExceptions |= exceptionMask;
}

/*!
 * @brief Checks whether the most recent test-suite passed fully.
 *
 * @return
 *    true  if all test-cases in the most recent test-suite passed.
 *    false if at least one test-case from the most recent test-suite failed.
 *
 * @note Result is undefined if no prior test-suite exists.
 *
 */
bool
harnessSuitePassed(void)
{
    // Sanity check.
    if (s_suiteState.bActive)
    {
        HARNESS_INTERNAL_ERROR("Invalid function call");
        return false;
    }

    // Check whether all test-cases passed.
    return (s_suiteState.passCount == s_suiteState.testCount);
}
