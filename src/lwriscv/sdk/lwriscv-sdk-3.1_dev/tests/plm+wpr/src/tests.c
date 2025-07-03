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
 * @file        tests.c.
 * @brief       Test functions.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/fence.h>
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/print.h>

// Local includes.
#include "config.h"
#include "engine.h"
#include "hal.h"
#include "harness.h"
#include "targets.h"
#include "tests.h"
#include "util.h"


// Prototypes for local helper functions. See definitions for details.
static void _testSetConfiguration(TEST_MEMORY_MODE memMode,
    TEST_OPERATING_MODE opMode, HAL_PRIVILEGE_LEVEL lwrrentLevel,
    HAL_PRIVILEGE_LEVEL otherLevel, HAL_REGION_ID wprIdFetch,
    HAL_REGION_ID wprIdLoad, HAL_REGION_ID wprIdMpu,
    HAL_REGION_ID wprIdDma, HAL_REGION_ID wprIdGdma);


/*
 * @brief Initializes common resources required by all test-cases.
 */
void
testInit(void)
{   
    // Treat as a test-case for reporting purposes.
    harnessBeginTest("INIT");
    
    //
    // Set maximum privileges to ensure we have access to all required
    // resources during setup. Note that S- and U-mode privileges are kept at
    // zero as MSPM.S/UPLM haven't been configured to allow other settings yet.
    // We similarly force bare/non-WPR memory access as the MPU and WPR have
    // also not been configured yet.
    //
    _testSetConfiguration(BARE, MACHINE, LEVEL3, LEVEL0, NONWPR, NONWPR,
        NONWPR, NONWPR, NONWPR);

    // Enable all privilege-levels for all modes (manifest only does m-mode).
    halEnableAllPrivLevels();

    // Always use the current operating mode's translation/protection rules.
    halResetAddressingMode();

    // Enable the MPU entry we're about to program below (and only that entry).
    halInitMPU(CONFIG_TEST_MPU_COUNT);

    // Set up an identity mapping in the MPU for virtual-mode tests to use.
    halCreateIdentityMapping(CONFIG_TEST_MPU_INDEX);

    // Enable FBIF for physical requests and configure to prevent hanging.
    halConfigureFBIF();

    // Verify that our chosen FB address is usable before enabling WPR.
    uint32_t fbExpected, fbActual;
    if(!targetPrepareAccess(WPR, LOAD, &fbExpected) ||
       !targetAttemptAccess(WPR, LOAD, &fbActual)   ||
       (fbActual != fbExpected))
    {
        harnessEndTest(false);

        puts("Likely bad FB address chosen for WPR tests!\n");
        return;
    }

    // Initialize the test targets (enable WPR, configure DMA, etc.).
    if (!targetInitializeAll())
    {
        harnessEndTest(false);

        puts("Unable to configure one or more test targets!\n");
        return;
    }

    // Ensure above configuration settings have fully taken effect.
    riscvLwfenceRWIO();

    // Setup completed.
    harnessEndTest(true);
}

/*
 * @brief Verifies the exelwtion environment for the test-cases.
 */
void
testElwironment(void)
{
    // Treat as a test-case for reporting purposes.
    harnessBeginTest("ENVIRONMENT");

    //
    // Set maximum privileges to ensure we have access to everything we need to
    // check. Also set bare/non-WPR as we don't know yet whether MPU/WPR have
    // been successfully configured.
    //
    _testSetConfiguration(BARE, MACHINE, LEVEL3, LEVEL0, NONWPR, NONWPR,
        NONWPR, NONWPR, NONWPR);

    // Verify access to DMA registers (e.g. DMATRFCMD).
    if (!HAL_HAS_DEVICEMAP_ACCESS(MMODE,    DMA, READ)  ||
        !HAL_HAS_DEVICEMAP_ACCESS(MMODE,    DMA, WRITE) ||
        !HAL_HAS_DEVICEMAP_ACCESS(SUBMMODE, DMA, READ)  ||
        !HAL_HAS_DEVICEMAP_ACCESS(SUBMMODE, DMA, WRITE))
    {
        harnessEndTest(false);

        puts("Lacking access to DMA registers!\n");
        return;
    }

    // Verify access to FBIF registers (e.g. CTL/CTL2).
    if (!HAL_HAS_DEVICEMAP_ACCESS(MMODE,    FBIF, READ)  ||
        !HAL_HAS_DEVICEMAP_ACCESS(MMODE,    FBIF, WRITE) ||
        !HAL_HAS_DEVICEMAP_ACCESS(SUBMMODE, FBIF, READ)  ||
        !HAL_HAS_DEVICEMAP_ACCESS(SUBMMODE, FBIF, WRITE))
    {
        harnessEndTest(false);

        puts("Lacking access to FBIF registers!\n");
        return;
    }

    // Verify access to MMODE registers (e.g. FBIF_REGIONCFG).
    if (!HAL_HAS_DEVICEMAP_ACCESS(MMODE,    MMODE, READ)  ||
        !HAL_HAS_DEVICEMAP_ACCESS(MMODE,    MMODE, WRITE) ||
        !HAL_HAS_DEVICEMAP_ACCESS(SUBMMODE, MMODE, READ)  ||
        !HAL_HAS_DEVICEMAP_ACCESS(SUBMMODE, MMODE, WRITE))
    {
        harnessEndTest(false);

        puts("Lacking access to MMODE registers!\n");
        return;
    }

    // Verify that all privilege levels are permitted.
    if(!halVerifyPrivLevels())
    {
        harnessEndTest(false);

        puts("Lacking one or more privilege-levels in MSPM!\n");
        return;
    }

    // Verify that MPRV is disabled.
    if (!halVerifyAddressingMode())
    {
        harnessEndTest(false);

        puts("Lacking normal address translation/protection (MPRV)!\n");
        return;
    }

    // Verify that the MPU has been configured.
    if (!halVerifyMPUEntry(CONFIG_TEST_MPU_INDEX))
    {
        harnessEndTest(false);

        puts("MPU has not been configured!\n");
        return;
    }

    // Verify FBIF configuration.
    if (!halVerifyFBIF())
    {
        harnessEndTest(false);

        puts("Physical FBIF requests are not enabled!\n");
        return;
    }

    // Verify NACK_AS_ACK setting.
    if (!halVerifyNackMode())
    {
        puts("Warning: NACK_AS_ACK is disabled - negative tests may hang!\n");
        // Continue - not fatal.
    }

    // Verify WPR setup.
    if(!halVerifyWPR())
    {
        harnessEndTest(false);

        puts("WPR is not configured!\n");
        return;
    }

    // Verify IO-PMP settings.
    if (!halVerifyIOPMP())
    {
        harnessEndTest(false);

        puts("IO-PMP should be disabled!\n");
        return;
    }

    // Verification successful.
    harnessEndTest(true);
}

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Tears down the exelwtion environment.
 */
void
testTeardown(void)
{
    // Treat as a test-case for reporting purposes.
    harnessBeginTest("TEARDOWN");

    //
    // Track success/failure for later. We can't return early in this function
    // as we want to complete as much teardown as possible regardless of any
    // failures we encounter.
    //
    bool bResult = true;

    //
    // Set maximum privileges to ensure we have access to everything we need to
    // touch (L0 for S-/U-mode in case setup failed to configure MSPM). Also
    // ensure bare memory access as the MPU state is unknown, and disable WPR
    // as we'll be tearing it down.
    //
    _testSetConfiguration(BARE, MACHINE, LEVEL3, LEVEL0, NONWPR, NONWPR,
        NONWPR, NONWPR, NONWPR);

    // Disable WPR1 and the host engine's subWPR.
    halDisableWPR();

    // Wait for disablement to go through.
    riscvLwfenceRWIO();

    // Verify subWPR disablement.
    if (halVerifyWPR())
    {
        bResult = false;
    }

    // Unlock all target PLMs.
    if (
#if ENGINE_TEST_CSB
        !targetSetPLM(CSB, PLM0 | PLM1 | PLM2 | PLM3) ||
#endif // ENGINE_TEST_CSB
        !targetSetPLM(PRI, PLM0 | PLM1 | PLM2 | PLM3))
    {
        bResult = false;
    }

    // Clear target contents to avoid polluting future test runs.
    if (
#if ENGINE_TEST_CSB
        !targetClearAccess(CSB) ||
#endif // ENGINE_TEST_CSB
        !targetClearAccess(PRI) ||
        !targetClearAccess(WPR))
    {
        bResult = false;
    }

    // Teardown complete.
    harnessEndTest(bResult);
}

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
void
testSingleAccess
(
    const char *pTestName,
    TARGET_ACCESS_TYPE accessType,
    TARGET_TYPE targetType,
    TEST_MEMORY_MODE memMode,
    TEST_OPERATING_MODE opMode,
    HAL_PRIVILEGE_LEVEL lwrrentLevel,
    HAL_PRIVILEGE_LEVEL otherLevel,
    HAL_PRIVILEGE_MASK targetPlm,
    HAL_REGION_ID wprIdFetch,
    HAL_REGION_ID wprIdLoad,
    HAL_REGION_ID wprIdMpu,
    HAL_REGION_ID wprIdDma,
    HAL_REGION_ID wprIdGdma,
    TEST_ACCESS_RESULT expectedResult
)
{
    
    uint32_t expectedValue, actualValue;

    // Mark start of test-case.
    harnessBeginTest(pTestName);

    //
    // Temporarily change to level 3 to ensure we can configure the target PLM.
    // Also set the WPR ID before preparing the target (required for WPR,
    // ignored for PRI/CSB). Remaining settings are "don't care".
    //
    _testSetConfiguration(BARE, MACHINE, LEVEL3, LEVEL3, WPR1, WPR1, WPR1,
        WPR1, WPR1);

    // Update the PLM of the target.
    if (!targetSetPLM(targetType, targetPlm))
    {
        harnessEndTest(false);

        puts("Unable to configure target PLM!\n");
        return;
    }

    // Prepare the target for access.
    if(!targetPrepareAccess(targetType, accessType, &expectedValue))
    {
        harnessEndTest(false);

        puts("Unable to prepare target!\n");
        return;
    }

    // Switch to the actual requested configuration for the test.
    _testSetConfiguration(memMode, opMode, lwrrentLevel, otherLevel,
        wprIdFetch, wprIdLoad, wprIdMpu, wprIdDma, wprIdGdma);

    // Attempt to access the target.
    if(!targetAttemptAccess(targetType, accessType, &actualValue))
    {
        harnessEndTest(false);

        puts("Unable to attempt access!\n");
        return;
    }

    // Check positive results.
    if (expectedResult == GRANTED)
    {
        // No exceptions expected in the positive case.
        harnessExpectException(HARNESS_EXCEPTION_MASK_NONE);

        // Values should match.
        harnessEndTest(actualValue == expectedValue);
    }

    // Negative results are dependent on access-type.
    else switch(accessType)
    {
        case LOAD:
            // Expect load-access-fault and value mismatch.
            harnessExpectException(HARNESS_EXCEPTION_MASK_LACC);
            harnessEndTest(actualValue != expectedValue);
            break;

        case FETCH:
            // Expect instruction-access-fault (value irrelevant for fetch).
            harnessExpectException(HARNESS_EXCEPTION_MASK_IACC);
            harnessEndTest(true);
            break;

        case DMA:
#if ENGINE_TEST_GDMA
        case GDMA:
#endif // ENGINE_TEST_GDMA
            // No exception for (G)DMA but values should not match.
            harnessExpectException(HARNESS_EXCEPTION_MASK_NONE);
            harnessEndTest(actualValue != expectedValue);
            break;

        default:
            // Unrecognized access type (always fail).
            harnessEndTest(false);
            printf("Unrecognized access type (%d)!\n", accessType);
    }
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Colwenience function to set overall core configuration at once.
 *
 * @param[in] memMode        The memory mode to switch to.
 *
 * @param[in] opMode         The operating mode to switch to.
 *
 * @param[in] lwrrentLevel   The external privilege-level to assign to the
 *                           current (new) operating mode.
 *
 * @param[in] otherLevel     The external privilege-level to assign to all
 *                           other operating modes.
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
 */
static void
_testSetConfiguration
(
    TEST_MEMORY_MODE memMode,
    TEST_OPERATING_MODE opMode,
    HAL_PRIVILEGE_LEVEL lwrrentLevel,
    HAL_PRIVILEGE_LEVEL otherLevel,
    HAL_REGION_ID wprIdFetch,
    HAL_REGION_ID wprIdLoad,
    HAL_REGION_ID wprIdMpu,
    HAL_REGION_ID wprIdDma,
    HAL_REGION_ID wprIdGdma
)
{
    // Switch to M-mode as it will be needed to configure other parameters.
    harnessXToM();

    // Set the desired memory mode.
    if (memMode == BARE)
    {
        halDisableMPU();
    }

    else
    {
        halEnableMPU();
    }

    // Set the WPR ID for bare-mode accesses.
    halSetBareWPRID(wprIdFetch, wprIdLoad);

    // Set the WPR ID for virtual-mode accesses.
    halSetVirtualWPRID(CONFIG_TEST_MPU_INDEX, wprIdMpu);

    // Set the WPR ID for DMA transfers.
    halSetDMAWPRID(CONFIG_ACCESS_DMA_INDEX, wprIdDma);

    // Set the WPR ID for GDMA transfers.
    targetSetGDMAWPRID(wprIdGdma);

    // Set the external privilege-levels and operating mode.
    switch (opMode)
    {
        case MACHINE:
            halSetPrivLevels(lwrrentLevel, otherLevel, otherLevel);
            // Already in m-mode.
            break;

        case SUPERVISOR:
            halSetPrivLevels(otherLevel, lwrrentLevel, otherLevel);
            halMToS();
            break;

        case USER:
            halSetPrivLevels(otherLevel, otherLevel, lwrrentLevel);
            halMToU();
            break;
    }

    // Ensure settings go through fully.
    riscvLwfenceRWIO();
}
