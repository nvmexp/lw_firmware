/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwosdebug.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lwosdebug.h"
#include "linkersymbols.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * RM visible (read-only) structure containing RTOS's main debug entry point.
 *  - RTOS updates this structure on each relevant state change
 *  - RM only inspects this structure when debug information is required
 * Race conditions are expected, and RM should read data when falcon is halted.
 */
RM_RTOS_DEBUG_ENTRY_POINT   OsDebugEntryPoint = { 0 };

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Implementation --------------------------------- */
/*!
 * Detects ISR stack overflow by checking if stack TOT contains predefined value.
 */
void
osDebugISRStackOverflowDetect()
{
    if (*_isr_stack_start != RM_FALC_STACK_FILL)
    {
        OS_HALT();
    }
}

/*!
 * Detects ESR stack overflow by checking if stack TOT contains predefined value.
 */
void
osDebugESRStackOverflowDetect()
{
#if UPROC_FALCON
    if (*_esr_stack_start != RM_FALC_STACK_FILL)
    {
        OS_HALT();
    }
#else
    OS_HALT();
#endif
}

/*!
 * Initialize the ISR stack by filling it with predefined value. This helps us
 * monitor and check for ISR stack usage and overflows.
 */
void
osDebugISRStackInit(void)
{
#if UPROC_FALCON
    LwU16 isrStackSize = (LwU16)
        (((LwU8 *)_isr_stack_end - (LwU8 *)_isr_stack_start)/sizeof(LwUPtr));

    // Ignore OsDebugEntryPoint for RISCV build since RM is not ready to use it
    // Expose ISR stack offset and size to RM.
    OsDebugEntryPoint.pIsrStack    = RM_FALC_PTR_TO_OFS(_isr_stack_start);
    OsDebugEntryPoint.isrStackSize = isrStackSize;

    //
    // No need to sanity check pointers since these are declared in the linker
    // script and we can assume that they should already be valid.
    //
    osDebugStackPreFill(_isr_stack_start, isrStackSize);
#else
    OS_HALT();
#endif
}

/*!
 * Initialize the ESR stack by filling it with predefined value. This helps us
 * monitor and check for ISR stack usage and overflows.
 */
void
osDebugESRStackInit(void)
{
#if UPROC_FALCON
    //unsigned portSHORT esrStackSize = (unsigned portSHORT)
    //    (((LwU8 *)_esr_stack_end - (LwU8 *)_esr_stack_start)/sizeof(LwUPtr));

    // Expose ESR stack offset and size to RM.
    //OsDebugEntryPoint.pEsrStack    = RM_FALC_PTR_TO_OFS(_esr_stack_start);
    //OsDebugEntryPoint.esrStackSize = esrStackSize;

    //
    // No need to sanity check pointers since these are declared in the linker
    // script and we can assume that they should already be valid.
    // Note: The hardcoded value of 1 below is only temporary during a series
    // of changes that will change the prefill function to only fill the stack
    // top (lowest address). This temporarily allows us to use the existing
    // function to do that.
    //

    osDebugStackPreFill(_esr_stack_start, 1);
#else
    OS_HALT();
#endif
}

/*!
 * Pre-fills entire stack depth with predetermined value that helps monitor
 * stack usage and overflows.
 *
 * @param[in]   pxStack       Pointer to start of stack (lowest memory address)
 *                            Note that stack grows downwards to lower memory
 *                            addresses.
 * @param[in]   usStackDepth  Stack depth
 * 
 * @note: Stack prefilling is REQUIRED for auto, even if the hardware supports
 *        stack overflow detection.
 */
void
osDebugStackPreFill
(
    LwUPtr *pxStack,
    LwU16   usStackDepth
)
{
    LwU32 i;
    for (i = 0; i < usStackDepth; i++)
    {
        pxStack[i] = RM_FALC_STACK_FILL;
    }
}
