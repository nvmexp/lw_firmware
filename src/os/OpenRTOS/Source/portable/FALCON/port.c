/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes ------------------------------- */
#include <stdlib.h>
#include <falcon-intrinsics.h>

/* ------------------------- OpenRTOS Includes ----------------------------- */
#include "OpenRTOS.h"
#include "OpenRTOSConfig.h"
#include "OpenRTOSFalcon.h"
#include "task.h"
#include "falcon.h"
#include "portmacro.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */

/*!
 * Each task maintains its own interrupt status in the critical nesting
 * variable.
 */
#define portNO_CRITICAL_SECTION_NESTING 0

/* ------------------------- Global Variables ------------------------------ */
volatile unsigned portBASE_TYPE uxCriticalNesting = 9999;

/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Externs --------------------------------------- */
extern void IV0_routine(void);
extern void IV1_routine(void);
extern void EV_routine(void);
extern void EV_preinit_routine(void);

/* ------------------------- Function Prototypes --------------------------- */

portSTACK_TYPE *
pxPortInitialiseStack
(
    portSTACK_TYPE *pxTopOfStack,
    pdTASK_CODE     pxCode,
    void           *pvParameters
)
#ifdef TASK_RESTART
__attribute__((section (".imem_resident._pxPortInitialiseStack")));
#else
__attribute__((section (".imem_init._pxPortInitialiseStack")));
#endif

void vPortInitialize(void)
   __attribute__((section (".imem_init._vPortInitialize")));

static void _vPortInstallExceptionHandlers(void)
   __attribute__((section (".imem_init._vPortInstallExceptionHandlers")));
static void _vPortInstallPreInitExceptionHandler(void)
   __attribute__((section (".imem_init._vPortInstallPreInitExceptionHandler")));

/* ------------------------- Implementation ------------------------------- */
void
vPortInitialize( void )
{
    _vPortInstallPreInitExceptionHandler();
}

portSTACK_TYPE *
pxPortInitialiseStack
(
    portSTACK_TYPE *pxTopOfStack,
    pdTASK_CODE     pxCode,
    void           *pvParameters
)
{
    // set return address (PC) to the start of the task
    *pxTopOfStack = (portSTACK_TYPE)pxCode;

    //
    // Adjust the stack pointer to skip a0-a15 (don't care about the data except
    // for a10=*pvParameters)
    //
    pxTopOfStack -= 17;

    //
    // When the task starts is will expect to find the function parameter in
    // a10.
    //
    pxTopOfStack[6] = (portSTACK_TYPE) pvParameters; // a10

    //
    // Set the CSW to restore upon the first task swich (interrupts enabled :
    // PIEx set)
    //
    *pxTopOfStack-- = (portSTACK_TYPE)0x300000;
    *pxTopOfStack   = portNO_CRITICAL_SECTION_NESTING;
    return pxTopOfStack;
}

#ifndef HW_STACK_LIMIT_MONITORING
void
vPortStackOverflowDetect
(
    void *pvTcb
)
{
    tskTCB *pTcb = (tskTCB*) pvTcb;
    portSTACK_TYPE  stackTop;

    // For now we do not track this for IDLE task (NJ-TODO).
    if (pTcb != NULL)
    {
        //
        // Find the base of the stack address.  This depends on whether the
        // stack grows from high memory to low or visa versa. In our case,
        // stack grows downwards from higher memory to lower. Once found, if
        // the stack base is still at the value we originially initialized the
        // stack memory to, we assume no overflow has happened. However, if it
        // has been modified, then during the last exelwtion of this task, the
        // task's stack grew up to its last byte and this is our barrier to
        // detect overflow. Halt to catch this condition. The only false
        // negatives possible with this implementation is if some uninitialized
        // local variable was put on the stack at the stack base, or if some
        // local variable had the value of tskSTACK_FILL_WORD and happened to
        // be placed at the stack base. We will, however, not have any false
        // positives. If we are able to detect that a task has used up at least
        // 100% of its allocated stack, halt.
        //
        stackTop = *( pTcb->pxStackBaseAddress );

        if ( stackTop != tskSTACK_FILL_WORD )
        {
            falc_halt();
        }
    }
}
#endif

void
vPortEnterCritical(void)
{
    falc_sclrb_i(CSW_FIELD_IE0);
    falc_sclrb_i(CSW_FIELD_IE1);
    uxCriticalNesting++;
}

void
vPortExitCritical(void)
{
    uxCriticalNesting--;
    if(uxCriticalNesting == 0)
    {
        falc_ssetb_i(CSW_FIELD_IE0);
        falc_ssetb_i(CSW_FIELD_IE1);
    }
}

/*!
 * This function cannot be in the .init because code behind
 * loadLwrrentTaskOverlay would be potentially overwritten by the Dma transfer
 */
portBASE_TYPE
xPortStartScheduler(void)
{
    // Install the handler routines for the interrupt and exception vectors.
    _vPortInstallExceptionHandlers();

    // load task overlay if needed
    (void)xPortLoadTask(NULL, NULL);

    // Set the falcon privilege level for current task.
    portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK();

    //
    // Start the first task (jump directly to the IV0_handler epilogue to avoid
    // duplicated code (and save a few bytes)
    //
    asm volatile (" jmp __restore_ctx; ");

    // Should not get here!
    return 0;
}


int xPortLoadTask( xTaskHandle xTask, unsigned int *pxLoadTimeTicks )
{
extern signed int osTaskLoad(xTaskHandle pxTask, unsigned int *pxLoadTimeTicks);

    return osTaskLoad( xTask, pxLoadTimeTicks );
}

static void
_vPortInstallPreInitExceptionHandler(void)
{
   // register the handler function for EV
   falc_wspr(EV, EV_preinit_routine);
}

static void
_vPortInstallExceptionHandlers(void)
{
   // register the handler function for EV
   falc_wspr(EV, EV_routine);

   // register the handler function for IV0
   falc_wspr(IV0, IV0_routine);

   // register the handler function for IV1
   falc_wspr(IV1, IV1_routine);
}
