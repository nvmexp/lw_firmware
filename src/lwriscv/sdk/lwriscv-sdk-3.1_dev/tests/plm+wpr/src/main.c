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
 * @file        main.c.
 * @brief       Application entry-point.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// LIBLWRISCV includes.
#include <liblwriscv/print.h>

// Local includes.
#include "hal.h"
#include "harness.h"
#include "engine.h"
#include "util.h"


// Identifying number written to mailbox for debugging purposes ("SECTESTS").
#define APP_MAGIC_NUMBER 0x5EC7E575U

// Amount of memory to reserve for print output (in bytes, multiples of eight).
#define PRINT_BUFFER_SIZE 0x1000U


// printInit() expects a 16-bit buffer size.
_Static_assert(PRINT_BUFFER_SIZE <= UINT16_MAX,
    "Size specified for print buffer exceeds expected data-type (uint16_t)!");


// Prototypes for local helper functions. See definitions for details.
static bool _preInit(void);


// End (highest) address of the program stack in DMEM.
extern const void * const _liblwriscv_stack_bottom;


/*
 * @brief Application entry-point.
 *
 * @return
 *    0 if tests exelwted successfully.
 *    1 if an error oclwrred.
 *
 * @note Return value only indicates whether the tests were exelwted, not
 *       whether they passed or failed.
 */
int
main(void)
{
    // Track return value.
    int status = 0;
    
    //
    // Write identifying number to mailbox first. This helps demonstrate that
    // our code actually ran, even if all later prints fail.
    //
    halWriteMailbox0(APP_MAGIC_NUMBER);

    //
    // Execute early initialization steps that need to take place before any
    // test-specific setup can occur.
    //
    if (!_preInit())
    {
        status = 1;
        goto finish;
    }

    // Print startup message with platform and version information.
    printf("Running PLM and WPR tests on %s.\nThis binary was built on "
        __DATE__" at "__TIME__" for engine "UTIL_STRINGIZE(ENGINE_PREFIX)".\n",
        halGetPlatformName());
    
    // Execute the tests (results are reported via prints).
    harnessMain();

finish:
    // Infinite loop to allow for inspecting state before we shut down.
    while (1)
        ;

    // Finished.
    return status;
}

///////////////////////////////////////////////////////////////////////////////

/*
 * @brief Performs general initialization required by the application.
 *
 * @return
 *    true  if pre-initialization was successful.
 *    false if an error oclwrred.
 *
 * @note Only performs general setup (mostly steps intended to ease debugging).
 *       Test-specific setup is handled by testInit() instead.
 */
static bool
_preInit(void)
{   
    // Override ICD privileges to more intuitive settings.
    halConfigureICD();

    // Disable interrupt/exception delegation.
    halDisableDelegation();

    // Disable all interrupt sources.
    halDisableInterrupts();

    // Turn on tracebuffer for all operating modes.
    halEnableTracebuffer();

    // Unlock IMEM and DMEM access.
    halUnlockTcm();

    // Release priv-lockdown.
    halReleaseLockdown();

    // Compute the ending address of DMEM.
    const uintptr_t dmemEndPa = HAL_GET_END_ADDRESS(DMEM);
    
    // Place the print buffer at this ending address.
    const uintptr_t printBufferPa = dmemEndPa - PRINT_BUFFER_SIZE;

    // Check for underflow or bleeding into other DMEM contents.
    if (printBufferPa <= (uintptr_t)_liblwriscv_stack_bottom ||
        printBufferPa > dmemEndPa)
        return false;

    // Initialize the print driver.
    if (!printInit((void*)printBufferPa, PRINT_BUFFER_SIZE, 0, 0))
        return false;

    // Pre-initialization complete.
    return true;
}
