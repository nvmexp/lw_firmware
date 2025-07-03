/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

.global  _IV0_routine_entryC
.global  _IV1_routine_entryC
.global  _EXC_routine_entryC
.global  _EV_preinit_routine_entryC
.global  __restore_ctx
.global  _pxLwrrentTCB
.global  _uxCriticalNesting

/******************************************************************************
 *
 * Task Context Save Macro
 *
 * This macro is responsible for saving the current task context using a
 * combination of 'push' instructions to save context to the current stack as
 * well as load/store instructions to save context the current task's TCB. The
 * macro may be called from either interrupt vector service routine as well as
 * the exception vector routine.
 *
 * Any changes to this macro (number of DWords pushed onto the stack)
 * must be reflected in the defind of OS_DEBUG_ISR_TASK_STACK_USAGE.
 *
 *****************************************************************************/
#define _macro_context_save           \
    pushm a15;                        \
    mv    a15  `(_uxCriticalNesting); \
    ldd.w a15  a15  0;                \
    rspr  a10  CSW;                   \
    push  a10;                        \
    push  a15;                        \
    mv    a15  `(_pxLwrrentTCB);      \
    ldd.w a15  a15  0;                \
    rspr  a14  SP;                    \
    std.w a15  a14

/******************************************************************************
 *
 * Task Context Restore Macro
 *
 * Macro for perform the exact opposite operations as those done by
 * _macro_context_save. It is of critial importance that these macros stay in-
 * sync and exactly mirror each other.
 *
 *****************************************************************************/
#define _macro_context_restore        \
    mv    a15  `(_pxLwrrentTCB);      \
    ldd.w a15  a15  0;                \
    ldd.w a14  a15  0;                \
    wspr  SP   a14;                   \
    pop   a10;                        \
    mv    a14  `(_uxCriticalNesting); \
    std.w a14  a10;                   \
    pop   a10;                        \
    wspr  CSW  a10;                   \
    popm  a15  0

/******************************************************************************
 *
 * Interrupt Vector 0 Service Routine
 *
 * Called by the Falcon hardware when an interrupt is asserted on Interrupt
 * Vector 0. Responsible for saving the current task context, servicing the
 * interrupt, and restoring a task context (which may be a different task as
 * a result of servicing the interrupt).
 *
 *****************************************************************************/
.global  _IV0_routine
_IV0_routine:

    // save the current task context (register-file, SP, etc ...)
    _macro_context_save;

    // Switch to the ISR stack
    mv    a14  `(__isr_stack_end);
    wspr  SP   a14;

    // ilwoke the c-portion of the interrupt-service routine
    call _IV0_routine_entryC;

__restore_ctx:
    _macro_context_restore;
    reti;


/******************************************************************************
 *
 * Interrupt Vector 1 Service Routine
 *
 * Called by the Falcon hardware when an interrupt is asserted on Interrupt
 * Vector 1. Performs the same actions as performed by the IV0 handler.
 *
 *****************************************************************************/
.global  _IV1_routine
_IV1_routine:
    // save the current task context (register-file, SP, etc ...)
    _macro_context_save;

    // Switch to the ISR stack
    mv    a14  `(__isr_stack_end);
    wspr  SP   a14;

    // ilwoke the c-portion of the interrupt-service routine
    call _IV1_routine_entryC;

    //
    // Finally, restore context and return from the interrupt. Jump instead
    // of using the restore macro directly to reduce codesize.
    //
    jmp  __restore_ctx;


/******************************************************************************
 *
 * Exception Vector Service Routine
 *
 * Called by the Falcon hardware when an exception is raised. Also performs the
 * same actions as performed by the IV0/1 handlers.
 *
 *****************************************************************************/
.global  _EV_routine
_EV_routine:
    // clear E bit in CSW
    sclrb  24;

    // save the current task context (register-file, SP, etc ...)
    _macro_context_save;

    // Switch to the ESR stack
    mv    a14  `(__esr_stack_end);
    wspr  SP   a14;

    // ilwoke the c-portion of the exception-vector
    call _EV_routine_entryC;

    //
    // Finally, restore context and return from the exception. Jump instead
    // of using the restore macro directly to reduce codesize.
    //
    jmp  __restore_ctx;


/******************************************************************************
 *
 * Early Exception Vector Service Routine
 *
 * Called by the Falcon hardware when an exception is raised.
 * Installed by pre-init code to handle early debugging exceptions.
 *
 *****************************************************************************/
.section .imem_init,"ax"
.global  _EV_preinit_routine
_EV_preinit_routine:
    // clear E bit in CSW
    sclrb  24;

    // save all GPRs before the handler uses them
    pushm  a15;

    // ilwoke the c-portion of the exception-vector
    call  _EV_preinit_routine_entryC;

    // restore all GPRs and return
    popm  a15  0;
    reti;

