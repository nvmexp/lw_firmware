/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * The assembler doesn't understand include paths, so depending on which Falcon
 * we're building for, set the appropriate STACKCFG register address. The hope
 * is that these won't change often enough (maybe ever) to cause a problem.
 */
#ifdef PMU_CSB_ACCESS
    #define STACKCFG 0x138    // #define LW_CPWR_FALCON_STACKCFG  0x00000138
#elif  DPU_CSB_ACCESS
    #define STACKCFG 0x627138 // #define LW_PDISP_FALCON_STACKCFG 0x00627138
#elif  SEC2_CSB_ACCESS
    #define STACKCFG 0x4e00   // #define LW_CSEC_FALCON_STACKCFG  0x00004e00
#elif  GSPLITE_CSB_ACCESS
    #define STACKCFG 0x4e00   // #define LW_CGSP_FALCON_STACKCFG  0x00004e00
#endif

/*
 * All handlers are responsible for saving the current task context, servicing
 * the interrupt, and restoring a task context (which may be a different task
 * as a result of servicing the interrupt: context switch).
 */

.global _IV0_routine_entryC
.global _IV1_routine_entryC
.global _EXC_routine_entryC
.global _EV_preinit_routine_entryC
.global __restore_ctx
.global _pxLwrrentTCB
.global _uxCriticalNesting


/******************************************************************************
 *
 * Initialize constant values used in context save/restore sequences.
 *
 * This helper must not affect CSW. Lwrrently this is true since neither:
 * call / mvi / ret have effect on CSW.
 *
 * Register use:
 *
 *  @reg[out]   a0      ZERO
 *  @reg[out]   a1      STACKCFG
 *  @reg[out]   a2      &uxCriticalNesting
 *
 *****************************************************************************/
 _handler_constants_init:
    mvi     a0      0;
    mvi     a1      STACKCFG;
    mvi     a2      `(_uxCriticalNesting);
    ret;


/******************************************************************************
 *
 * Task Context Save Helper
 *
 * This routine is responsible for saving the current task context using a
 * combination of 'push' instructions to save context to the current stack as
 * well as load/store instructions to save context into the current task's TCB.
 * This routine may be called from either interrupt vector service routine as
 * well as the exception vector routine.
 *
 * Any changes to this macro (number of DWords pushed onto the stack)
 * must be reflected in the define of OS_DEBUG_ISR_TASK_STACK_USAGE.
 *
 * Register use:
 *
 * @reg     a0      ZERO
 * @reg     a1      STACKCFG
 * @reg     a2      &uxCriticalNesting
 * @reg     a4      &uxHandlerCacheStackData
 *
 * @reg[in] a10     Handler specific stack end address (stack empty)
 * @reg[in] a11     Handler specific stack start address (stack full)
 * @reg[in] a14     Denotes if context is to be saved in local array
 *
 *****************************************************************************/
_handler_context_save:
    //
    // Removing the return address from the stack since control will return
    // to the different stack.
    //
    pop     a9;

    //
    // Initialize constant values used in context save.
    //
    call    _handler_constants_init;

    //
    // Save CSW and the "critical nesting" count on the stack of the caller.     
    //
    rspr    a8      CSW;
    push    a8;
    ldd.w   a8      a2      0;
    push    a8;

    //
    // Save SP into the TCB of the interrupted task.
    //
    mv      a8     `(_pxLwrrentTCB);
    ldd.w   a8      a8      0;
    rspr    a7      SP;
    std.w   a8      a7;

    //
    // Switch to the custom stack:
    // a) clear the stack limit to prevent exception while updating the SP
    // b) switch to the handler specific stack
    // c) update the stack limit for the handler specific stack
    //
    stxb    a1      0       a0;         // blocking store to STACKCFG
    wspr    SP      a10;                // SP update
    asr.w   a11     0x2;                // colwert to dword
    setb    a11     0x1f;               // enable exception
    stxb    a1      0       a11;        // blocking store to STACKCFG

    //
    // Restoring the return address on the new stack.
    //
    push    a9;
    ret;


/******************************************************************************
 *
 * Used externally to ilwoke first context switch when starting the scheduler.
 *
 *****************************************************************************/
__restore_ctx:
    //
    // Initialize constant values used in context restore.
    //
    call    _handler_constants_init;
    jmp     __restore_ctx_merged;


/******************************************************************************
 *
 * Interrupt Vector 0 Service Routine
 *
 * Called by the Falcon hardware when an interrupt is asserted on Interrupt
 * Vector 0. The IV0 ISR handles all interrupts besides Timer Tick.
 *
 *****************************************************************************/
.global _IV0_routine
_IV0_routine:
    //
    // Save all GPRs before handler uses them.
    //
    pushm   a15;

    //
    // Setup the parameters for the _handler_context_save().
    //
    mv      a10     `(__isr_stack_end);
    mv      a11     `(__isr_stack_start);
    call    _handler_context_save;

    //
    // Ilwoke the C-portion of the interrupt-service routine 0.
    //
    call    _IV0_routine_entryC;

__restore_ctx_full:
    //
    // WARNING: Here we exploit a GCC implementation knowledge that interfaces
    //          are preserving a0..a8 registers if they are being used, so we
    //          do not have to call _handler_constants_init to set-up required
    //          constants in a0..a2 registers required by the restore sequence.
    //

__restore_ctx_merged:
    //
    // Restore to the target stack:
    // a) clear the stack limit to prevent exception while updating the SP
    // b) switch to the target stack
    // c) update the stack limit for the target stack
    //
    stxb    a1      0       a0;         // blocking store to STACKCFG
    mv      a8      `(_pxLwrrentTCB);
    ldd.w   a8      a8      0;          // fetch TCB pointer
    ldd.w   a7      a8      0;          // fetch stored SP
    wspr    SP      a7;
    ldd.w   a7      a8      8;          // fetch stored stack limit
    asr.w   a7      2;                  // colwert to dword
    setb    a7      0x1f;               // enable exception
    stxb    a1      0       a7;         // blocking store to STACKCFG

    //
    // Restore the "critical nesting" count and CSW from the target stack.
    //
    pop     a8;
    std.w   a2      a8;
    pop     a8;
    wspr    CSW     a8;

    //
    // Restore all GPRs and return to caller, on context switch to the new task. 
    //
    popm    a15     0;
    reti;


/******************************************************************************
 *
 * Interrupt Vector 1 Service Routine
 *
 * Called by the Falcon hardware when an interrupt is asserted on Interrupt
 * Vector 1. The IV1 ISR handles only Timer Tick interrupt.
 *
 *****************************************************************************/
.global _IV1_routine
_IV1_routine:
    //
    // Save all GPRs before handler uses them.
    //
    pushm   a15;

    //
    // Setup the parameters for the _handler_context_save().
    //
    mv      a10     `(__isr_stack_end);
    mv      a11     `(__isr_stack_start);
    call    _handler_context_save;

    //
    // Ilwoke the C-portion of the interrupt-service routine 1.
    //
    call    _IV1_routine_entryC;

    jmp     __restore_ctx_full;


/******************************************************************************
 *
 * Exception Vector Service Routine
 *
 * Called by the Falcon hardware when an exception is raised.
 *
 *****************************************************************************/
.global _EV_routine
_EV_routine:
    //
    // Clear E bit in CSW to prevent exception nesting.
    // Without this nested exceptions will be hard to debug.
    //
    sclrb   24;

    //
    // Save all GPRs before handler uses them.
    //
    pushm   a15;

    //
    // Setup the parameters for the _handler_context_save().
    //
    mv      a10     `(__esr_stack_end);
    mv      a11     `(__esr_stack_start);
    call    _handler_context_save;

    //
    // Ilwoke the C-portion of the exception-vector.
    //
    call    _EV_routine_entryC;

    jmp     __restore_ctx_full;


/******************************************************************************
 *
 * Early Exception Vector Service Routine
 *
 * Called by the Falcon hardware when an exception is raised.
 * Installed by pre-init code to handle early debugging exceptions.
 *
 *****************************************************************************/
.section .imem_init,"ax"
.global _EV_preinit_routine
_EV_preinit_routine:
    //
    // Clear E bit in CSW to prevent exception nesting.
    // Without this nested exceptions will be hard to debug.
    //
    sclrb   24;

    //
    // Save all GPRs before handler uses them.
    //
    pushm   a15;

    //
    // Ilwoke the C-portion of the exception-vector.
    //
    call    _EV_preinit_routine_entryC;

    //
    // Restore all GPRs and return
    popm    a15     0;
    reti;

