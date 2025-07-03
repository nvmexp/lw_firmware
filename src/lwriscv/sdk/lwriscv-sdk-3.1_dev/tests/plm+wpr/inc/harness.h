/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_HARNESS_H
#define PLM_WPR_HARNESS_H

/*!
 * @file        harness.h
 * @brief       Test harness.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>


// Bit flags for recognized exception types.
typedef enum HARNESS_EXCEPTION_MASK
{
    HARNESS_EXCEPTION_MASK_NONE = (0 << 0), // No exception.
    HARNESS_EXCEPTION_MASK_UNEX = (1 << 0), // Unexpected exception (reserved).

    HARNESS_EXCEPTION_MASK_IACC = (1 << 1), // Instruction-acess-fault.
    HARNESS_EXCEPTION_MASK_LACC = (1 << 2), // Load-access-fault.
}HARNESS_EXCEPTION_MASK;


/*
 * @brief Initializes the testing environment and exelwtes all test suites.
 */
void harnessMain(void);

/*!
 * @brief Marks the start of a new test suite.
 *
 * @param[in] pSuiteName  A null-terminated string describing the test suite.
 */
void harnessBeginSuite(const char *pSuiteName);

/*!
 * @brief Marks the end of the current test suite.
 */
void harnessEndSuite(void);

/*!
 * @brief Marks the start of a new test case.
 *
 * @param[in] pTestName  A null-terminated string describing the test case.
 */
void harnessBeginTest(const char *pTestName);

/*!
 * @brief Marks the end of the current test case.
 *
 * @param[in] bResult  Whether the test has passed, ignoring exceptions.
 *
 * @note Exceptions are checked separately from bResult.
 */
void harnessEndTest(bool bResult);

/*!
 * @brief Specifies which exceptions the current test is expected to trigger.
 *
 * @param[in] exceptionMask  A bitfield of expected exception types.
 *
 * @note A test that does not call this function expects no exceptions.
 */
void harnessExpectException(HARNESS_EXCEPTION_MASK exceptionMask);

/*!
 * @brief Records that the specified exceptions were triggered by the current test.
 *
 * @param[in] exceptionMask  A bitfield of the triggered exception types.
 *
 * @note Calls to this function are strictly additive (exceptions are never
 *       cleared once set) and are made automatically by the test harness when
 *       exceptions are caught. Tests do not need to call this function
 *       directly unless they wish to simulate an exception condition.
 */
void harnessRecordException(HARNESS_EXCEPTION_MASK exceptionMask);

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
bool harnessSuitePassed(void);

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Switches to M-mode from any other mode.
 */
static inline void
harnessXToM(void)
{
    // Trigger system-call exception (handler will return here in M-mode).
    __asm__ volatile ("ecall");
}

#endif // PLM_WPR_HARNESS_H
