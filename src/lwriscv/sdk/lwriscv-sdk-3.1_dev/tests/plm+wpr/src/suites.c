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
 * @file        suites.c
 * @brief       Test suites.
 */

// Standard includes.
#include <stdbool.h>

// Local includes.
#include "engine.h"
#include "harness.h"
#include "suites.h"
#include "tests.h"


/*
 * @brief Meta test-cases for setting up and verifying the test environment.
 */
void
suiteSetup(void)
{
    // Treat as a test-suite for reporting purposes.
    harnessBeginSuite("SETUP");

    // Initialize common resources required by all test-cases.
    testInit();

    // Verify the exelwtion environment (above settings and more).
    testElwironment();
    
    // Report results.
    harnessEndSuite();
}

/*
 * @brief Meta test-cases for cleaning up the test environment.
 */
void
suiteTeardown(void)
{   
    // Treat as a test-suite for reporting purposes.
    harnessBeginSuite("TEARDOWN");

    // Tear down the exelwtion environment.
    testTeardown();

    // Report results.
    harnessEndSuite();
}

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Cases testing PLM enforcement when accessing registers over PRI/CSB.
 */
static void
suiteDirectRegisterAccess(void)
{
    // Mark start of test-suite.
    harnessBeginSuite("DIRECT REGISTER ACCESS");

    // Execute all test-cases.
    testSingleAccess( "SEC-TC-A1", LOAD, PRI, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess( "SEC-TC-A2", LOAD, PRI, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess( "SEC-TC-A3", LOAD, PRI, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
#if ENGINE_TEST_CSB
    testSingleAccess( "SEC-TC-A4", LOAD, CSB, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess( "SEC-TC-A5", LOAD, CSB, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess( "SEC-TC-A6", LOAD, CSB, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
#endif // ENGINE_TEST_CSB
    testSingleAccess( "SEC-TC-A7", LOAD, PRI, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess( "SEC-TC-A8", LOAD, PRI, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess( "SEC-TC-A9", LOAD, PRI, BARE,       USER, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
#if ENGINE_TEST_CSB
    testSingleAccess("SEC-TC-A10", LOAD, CSB, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-A11", LOAD, CSB, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-A12", LOAD, CSB, BARE,       USER, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
#endif // ENGINE_TEST_CSB

    // Report results.
    harnessEndSuite();
}

/*
 * @brief Cases testing PLM enforcement when accessing registers via GDMA.
 */
static void
suiteGDMARegisterAccess(void)
{
    // Mark start of test-suite.
    harnessBeginSuite("GDMA REGISTER ACCESS");

#if ENGINE_TEST_GDMA
    // Execute all test-cases.
    testSingleAccess("SEC-TC-B1", GDMA, PRI, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-B2", GDMA, PRI, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-B3", GDMA, PRI, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-B4", GDMA, PRI, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-B5", GDMA, PRI, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-B6", GDMA, PRI, BARE,       USER, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
#endif // ENGINE_TEST_GDMA

    // Report results.
    harnessEndSuite();
}

/*
 * @brief Cases testing PLM/ID enforcement when fetching instructions from WPR.
 */
static void
suiteWPRInstructionFetch(void)
{
    // Mark start of test-suite.
    harnessBeginSuite("WPR INSTRUCTION FETCH");

    // Execute all test-cases.
    testSingleAccess("SEC-TC-C1", FETCH, WPR, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3,   WPR1, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-C2", FETCH, WPR, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3,   WPR1, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-C3", FETCH, WPR, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3,   WPR1, NONWPR, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-C4", FETCH, WPR, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3,   WPR1, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-C5", FETCH, WPR, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3,   WPR1, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-C6", FETCH, WPR, BARE,       USER, LEVEL3, LEVEL0,               PLM3,   WPR1, NONWPR, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-C7", FETCH, WPR,  MPU,    MACHINE, LEVEL2, LEVEL0,        PLM2 | PLM3, NONWPR,   WPR1,   WPR1,   WPR1,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-C8", FETCH, WPR,  MPU, SUPERVISOR, LEVEL2, LEVEL0,        PLM2 | PLM3,   WPR1,   WPR1, NONWPR,   WPR1,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-C9", FETCH, WPR,  MPU,       USER, LEVEL2, LEVEL0,        PLM2 | PLM3, NONWPR, NONWPR,   WPR1, NONWPR, NONWPR, GRANTED);

    // Report results.
    harnessEndSuite();
}

/*
 * @brief Cases testing PLM/ID enforcement when loading data from WPR.
 */
static void
suiteWPRLoadStore(void)
{
    // Mark start of test-suite.
    harnessBeginSuite("WPR LOAD/STORE");

    // Execute all test-cases.
    testSingleAccess("SEC-TC-D1", LOAD, WPR, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR,   WPR1, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-D2", LOAD, WPR, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR,   WPR1, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-D3", LOAD, WPR, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR,   WPR1, NONWPR, NONWPR, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-D4", LOAD, WPR, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3, NONWPR,   WPR1, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-D5", LOAD, WPR, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3, NONWPR,   WPR1, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-D6", LOAD, WPR, BARE,       USER, LEVEL3, LEVEL0,               PLM3, NONWPR,   WPR1, NONWPR, NONWPR, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-D7", LOAD, WPR,  MPU,    MACHINE, LEVEL1, LEVEL0,        PLM1 | PLM3,   WPR1, NONWPR,   WPR1,   WPR1,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-D8", LOAD, WPR,  MPU, SUPERVISOR, LEVEL1, LEVEL0,        PLM1 | PLM3,   WPR1,   WPR1, NONWPR,   WPR1,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-D9", LOAD, WPR,  MPU,       USER, LEVEL1, LEVEL0,        PLM1 | PLM3, NONWPR, NONWPR,   WPR1, NONWPR, NONWPR, GRANTED);

    // Report results.
    harnessEndSuite();
}

/*
 * @brief Cases testing PLM/ID enforcement when DMA-ing data from WPR.
 */
static void
suiteWPRDMA(void)
{
    // Mark start of test-suite.
    harnessBeginSuite("WPR DMA");

    // Execute all test-cases.
    testSingleAccess("SEC-TC-E1", DMA, WPR, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR,   WPR1, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-E2", DMA, WPR, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR,   WPR1, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-E3", DMA, WPR, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR,   WPR1, NONWPR,  DENIED);
    testSingleAccess("SEC-TC-E4", DMA, WPR, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR,   WPR1, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-E5", DMA, WPR, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR,   WPR1, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-E6", DMA, WPR, BARE,       USER, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR,   WPR1, NONWPR, GRANTED);
    testSingleAccess("SEC-TC-E7", DMA, WPR, BARE,    MACHINE, LEVEL2, LEVEL1,        PLM2 | PLM3,   WPR1,   WPR1,   WPR1, NONWPR,   WPR1,  DENIED);

    // Report results.
    harnessEndSuite();
}

/*
 * @brief Cases testing PLM/ID enforcement when GDMA-ing data from WPR.
 */
static void
suiteWPRGDMA(void)
{
    // Mark start of test-suite.
    harnessBeginSuite("WPR GDMA");

#if ENGINE_TEST_GDMA
    // Execute all test-cases.
    testSingleAccess("SEC-TC-F1", GDMA, WPR, BARE,    MACHINE, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-F2", GDMA, WPR, BARE, SUPERVISOR, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-F3", GDMA, WPR, BARE,       USER, LEVEL0, LEVEL3, PLM1 | PLM2 | PLM3, NONWPR, NONWPR, NONWPR, NONWPR,   WPR1,  DENIED);
    testSingleAccess("SEC-TC-F4", GDMA, WPR, BARE,    MACHINE, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR,   WPR1, GRANTED);
    testSingleAccess("SEC-TC-F5", GDMA, WPR, BARE, SUPERVISOR, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR,   WPR1, GRANTED);
    testSingleAccess("SEC-TC-F6", GDMA, WPR, BARE,       USER, LEVEL3, LEVEL0,               PLM3, NONWPR, NONWPR, NONWPR, NONWPR,   WPR1, GRANTED);
    testSingleAccess("SEC-TC-F7", GDMA, WPR, BARE,    MACHINE, LEVEL1, LEVEL2,        PLM1 | PLM3,   WPR1,   WPR1,   WPR1,   WPR1, NONWPR,  DENIED);
#endif // ENGINE_TEST_GDMA

    // Report results.
    harnessEndSuite();
}

///////////////////////////////////////////////////////////////////////////////

//
// A list of all available test-suites. Used by the test harness to automate
// exelwtion.
//
// Note that the setup and teardown suites are intentionally omitted here as
// they require special handling and so are called directly by the test
// harness instead.
//
void(* const g_suiteList[])(void) =
{
    // PLM test-suites.
    suiteDirectRegisterAccess,
    suiteGDMARegisterAccess,

    // WPR test-suites.
    suiteWPRInstructionFetch,
    suiteWPRLoadStore,
    suiteWPRDMA,
    suiteWPRGDMA,

    // Add new test-suites here as needed.

    // End-of-list sentinel.
    NULL,
};
