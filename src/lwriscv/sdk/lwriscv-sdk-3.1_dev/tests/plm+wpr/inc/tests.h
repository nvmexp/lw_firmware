/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_TESTS_H
#define PLM_WPR_TESTS_H

/*!
 * @file        tests.h
 * @brief       Test functions.
 */

// Local includes.
#include "targets.h"
#include "util.h"


//
// Enum for specifying the expected result of an access attempt. Note that
// short names are used here to improve the readbility of calls to
// testSingleAccess().
//
typedef enum TEST_ACCESS_RESULT
{
    DENIED,     // Access should be denied.
    GRANTED,    // Access should be granted.
}TEST_ACCESS_RESULT;

//
// Enum for specifying the internal memory mode for a test. Note that short
// names are used here to improve the readability of calls to
// testSingleAccess().
//
typedef enum TEST_MEMORY_MODE
{
    BARE,
    MPU,
}TEST_MEMORY_MODE;

//
// Enum for specifying the internal operating mode for a test. Note that
// short names are used here to improve the readability of calls to
// testSingleAccess().
//
typedef enum TEST_OPERATING_MODE
{
    MACHINE,        // M-mode.
    SUPERVISOR,     // S-mode.
    USER,           // U-mode.
}TEST_OPERATING_MODE;

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Initializes common resources required by all test-cases.
 */
void testInit(void);

/*
 * @brief Verifies the exelwtion environment for the test-cases.
 */
void testElwironment(void);

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Tears down the exelwtion environment.
 */
void testTeardown(void);

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Tests access to the specified target under the specified conditions.
 *
 * @param[in] pTestName      A null-terminated string describing the test-case.
 *                           Should generally contain the test-case ID number.
 *
 * @param[in] accessType     The method by which the target should be accessed.
 *
 * @param[in] targetType     Which target to attempt access to.
 *
 * @param[in] memMode        The memory mode to use when accessing the target.
 *
 * @param[in] opMode         The operating mode to use when accessing the
 *                           target.
 *
 * @param[in] lwrrentLevel   The external privilege-level to assign to the
 *                           current operating mode when accessing the target.
 *
 * @param[in] otherLevel     The external privilege-level to assign to all
 *                           other operating modes when accessing the target.
 *                           
 * @param[in] targetPlm      How the target's PLM should be configured.
 *
 * @param[in] wprIdFetch     The WPR ID to assign to bare-mode instruction
 *                           fetches.
 *
 * @param[in] wprIdLoad      The WPR ID to assign to bare-mode loads/stores.
 *
 * @param[in] wprIdMpu       The WPR ID to assign to virtual-mode accesses.
 *
 * @param[in] wprIdDma       The WPR ID to assign to DMA transfers.
 *
 * @param[in] wprIdGdma      The WPR ID to assign to GDMA transfers.
 *
 * @param[in] expectedResult Whether the access attempt is expected to succeed
 *                           or fail.
 */
void testSingleAccess(const char *pTestName, TARGET_ACCESS_TYPE accessType,
    TARGET_TYPE targetType, TEST_MEMORY_MODE memMode,
    TEST_OPERATING_MODE opMode, HAL_PRIVILEGE_LEVEL lwrrentLevel,
    HAL_PRIVILEGE_LEVEL otherLevel, HAL_PRIVILEGE_MASK targetPlm,
    HAL_REGION_ID wprIdFetch, HAL_REGION_ID wprIdLoad,
    HAL_REGION_ID wprIdMpu, HAL_REGION_ID wprIdDma,
    HAL_REGION_ID wprIdGdma, TEST_ACCESS_RESULT expectedResult);

#endif // PLM_WPR_TESTS_H
