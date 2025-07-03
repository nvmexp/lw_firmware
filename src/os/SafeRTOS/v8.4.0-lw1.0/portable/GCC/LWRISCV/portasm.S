/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "SafeRTOSConfig.h"
#include "riscv_csr.h"
#include "portasmmacro.h"
#include "portSyscall.h"

#define KERNEL_STACK_BUFFER 8

FUNC xPortStartScheduler
    jal     vPortSetupTimer

    // On next exception, we can start from bottom of kernel stack, discard all
    // Write init word at bottom of kernel stack
    la t0, pxPortKernelStackTop
    sd sp, 0(t0)

    j __PortRestoreContextAndEret
EFUNC xPortStartScheduler

/*
 * Context switch magic explanation.
 * We have two "modes" of context switch - requested (basically syscall)
 * and unexpected (exception, interrupt etc.).
 *
 * To boost performance for syscalls (possibly originating from kernel), we
 * may then ignore all Caller save registers. Which is almost half of them.
 *
 * On handler entry, we save only caller registers and tp/sp/gp.
 * On return we either read them from old context, or zero (latter faster).
 *
 * For unexpected events obviously we have to do full save/restore.
 *
 * WARNING: this will obviously work only if code will obey calling colwentions,
 * and syscall is wrapped in "non-inline" function.
 *
 * We must also remember whether we did full or partial ctx switch.
 * For that I've added "flag" to task context (offset 34).
 */
.balign 4
FUNC vPortTrapHandler
    // use gp as scratch because we'll use it as a temp register
    csrw    sscratch, gp

    //
    // First, set excption handler to crash in case we die saving context
    // (we can't recover from that) or during optimized syscalls
    // (which don't handle exception nesting)
    //
    la      gp, vPortInfiniteLoop
    csrw    stvec, gp

    csrr    gp, scause
    addi    gp, gp, -8
    beqz    gp, __PortCheckOptimizedSyscalls // SCAUSE == 8 -> ecall from U
    addi    gp, gp, -1
    beqz    gp, __PortCheckOptimizedSyscalls // SCAUSE == 9 -> ecall from S
    addi    gp, gp, -2
    beqz    gp, __PortCheckOptimizedSyscalls // SCAUSE == 11 -> ecall from M

    csrwi   LW_RISCV_CSR_SSCRATCH2, 0 // mark as NOT a syscall, since all syscall checks failed
    j       __PortSkipOptimizedSyscalls

__PortCheckOptimizedSyscalls:
    // We now know for sure this is a syscall.

    //
    // Check for horizontal ecalls (S-mode-to-S-mode) - we might need to do some magic to avoid underflowing tracebuffer
    // This is not restricted to SCAUSE == 9 (ecall from S) because that doesn't apply here (sepkern sets SCAUSE to 8)
    //
    csrr    gp, sstatus
    andi    gp, gp, portPRIVILEGED_NOINTR_SSTATUS // only the SPP set in sstatus

    beqz    gp, __PortSkipHorizontalEcallCheck // not a horizontal ecall, ignore

    //
    // Here we know we had a horizontal trap; this means that sepkern messed up the jump-ret order,
    // so insert extra jump to match sret. This way, stack-mode trace will not underflow!
    //

    // Save ra to avoid losing the caller's value!
    csrw LW_RISCV_CSR_SSCRATCH2, ra
    jal __PortPreventTraceUnderflow

__PortPreventTraceUnderflow:
    // Restore ra
    csrr ra, LW_RISCV_CSR_SSCRATCH2
__PortSkipHorizontalEcallCheck:

    //
    // Do optimized syscall checks.
    // LW_RISCV_CSR_SSCRATCH2 marks this as a syscall, in case we don't match any
    // optimized syscalls and fall-through to the "normal" syscalls.
    //
    csrwi   LW_RISCV_CSR_SSCRATCH2, 1

    // Check the exception nesting. Optimized syscalls only allow 0 nesting.
    ld      gp, xPortExceptionNesting
    bnez    gp, __PortEnterFromKernel

    //
    // By this point we are sure this is a syscall, and even optimized syscalls
    // will save and restore t* registers and other caller-saved registers
    // (as needed) at call-site (as a minimal "adherence" to a function ABI).
    // So it's fine to modify caller-saved registers like t* at this point!
    //
    li      gp, 1
    slli    gp, gp, 63 // highest-bit to match LW_SYSCALL_FLAG_SMODE

    //
    // Optimized syscalls that are called more frequently go first in the list.
    // gp has the "base" LW_SYSCALL_FLAG_SMODE offset, and then for each
    // optimized syscall t0 will be set to the full syscall ID we want to check.
    //

    // Disable interrupts (e.g. for entering critical section)
    optimizedSyscallCasePre gp, LW_SYSCALL_INTR_DISABLE
    li      t0, portSIE_ALL
    csrc    sie, t0
    li      a0, 0 // return FLCN_OK
    optimizedSyscallCasePost

    // Re-enable interrupts
    optimizedSyscallCasePre gp, LW_SYSCALL_INTR_ENABLE
    li      t0, portSIE_ALL
    csrs    sie, t0
    li      a0, 0 // return FLCN_OK
    optimizedSyscallCasePost

#ifndef PMU_RTOS
    // Raise task priv (called from sysPrivilegeRaise)
    // Setting the bits of portPRIVILEGED_SSTATUS in sstatus works to raise priv.
    // On PMU raising task priv without disabling interrupts first is not allowed.
    optimizedSyscallCasePre gp, LW_SYSCALL_SET_KERNEL
    li      t0, portPRIVILEGED_SSTATUS
    csrs    sstatus, t0
    li      a0, 0 // return FLCN_OK
    optimizedSyscallCasePost
#endif // PMU_RTOS

    // Raise task priv with intr disabled (called from sysPrivilegeRaiseIntrDisabled)
    optimizedSyscallCasePre gp, LW_SYSCALL_SET_KERNEL_INTR_DISABLED
    li      t0, sstatusInterruptCheckBits // clear SPIE bit first
    csrc    sstatus, t0
    li      t0, portPRIVILEGED_NOINTR_SSTATUS // then set priv bits
    csrs    sstatus, t0
    li      a0, 0 // return FLCN_OK
    optimizedSyscallCasePost

    // Lower task priv (called from sysPrivilegeLower)
    // See portcommon.h for why portPRIVILEGED_SSTATUS works as a mask here
    optimizedSyscallCasePre gp, LW_SYSCALL_SET_USER
    li      t0, portPRIVILEGED_SSTATUS
    csrc    sstatus, t0
    li      a0, 0 // return FLCN_OK
    optimizedSyscallCasePost

    // @see dmaMemTransfer
    optimizedSyscallCasePre gp, LW_SYSCALL_DMA_TRANSFER
    optimizedSyscallFuncContextSave
    jal     dmaMemTransferSyscallHandler_NoContext // args a0 through a4 should have already been set
    optimizedSyscallFuncContextRestore
    optimizedSyscallCasePost

    // @see dmaConfigureFbif
    optimizedSyscallCasePre gp, LW_SYSCALL_CONFIGURE_FBIF
    optimizedSyscallFuncContextSave
    jal     dmaConfigureFbif_NoContext // arg a0 through a3 should have already been set
    optimizedSyscallFuncContextRestore
    optimizedSyscallCasePost

    // @see mpuVirtToPhys
    optimizedSyscallCasePre gp, LW_SYSCALL_VIRT_TO_PHYS
    optimizedSyscallFuncContextSave
    jal     mpuVirtToPhysSyscallHandler_NoContext // arg a0 should have already been set
    optimizedSyscallFuncContextRestore
    optimizedSyscallCasePost

    // Failed all optimized syscall checks, go to __PortSkipOptimizedSyscalls
    j       __PortSkipOptimizedSyscalls

__PortOptimizedSyscallsDone: // label required by the macros above
    csrr    gp, sepc
    addi    gp, gp, 4
    csrw    sepc, gp
    li      t0, 0 // scrub t0 to clearing optimized syscall state (gp is cleared in __PortExitTrapHandler)
    j       __PortExitTrapHandler

__PortSkipOptimizedSyscalls: // label required by the macros above
    //
    // Proper context switch handling starts here.
    // First, figure out if we came from kernel or user, select proper location
    // for TCB, and store callee saved registers, then if needed save callers
    //

    // we can't increase nesting here (not enough registers)
    ld      gp, xPortExceptionNesting
    bnez    gp, __PortEnterFromKernel

__PortEnterFromTask:
    // In task mode, context is stored in pxLwrrentTCB
    ld      gp, pxLwrrentTCB
    // Save ilwerted sp
    not     sp, sp
    sd      sp, TCB_OFFSET_SP_MIRROR * REGBYTES(gp)
    not     sp, sp
    j       __PortSaveCallees

__PortEnterFromKernel:
    // Verity if kernel stack pointer makes sense or else...
    // We can recycle gp here
    la      gp, __stack_end
    bgeu    sp, gp, vPortInfiniteLoop // sp >= end of stack?
    la      gp, __stack_start
    addi    gp, gp, TCB_CTX_SIZE_WORDS * REGBYTES // we need enough space for context
    addi    gp, gp, KERNEL_STACK_BUFFER * REGBYTES // Add few words for fcn calls, TODO: precompute?
    bgeu    gp, sp, vPortInfiniteLoop // start of stack + context >= sp?

    // Reserve space for kernel context on kernel stack
    // We don't update SP yet so that it can be saved to the context
    addi    gp, sp, -TCB_CTX_SIZE_WORDS * REGBYTES

__PortSaveCallees:
    contextSaveCallee // At that point we can use s0-s11
    contextSaveRaA0

    // Load kernel-level TLS into tp
    la      tp, kernelTls
    // Mark partial context switch
    sd      zero, TCB_OFFSET_FLAGS * REGBYTES(gp)

#ifdef IS_SSP_ENABLED
#ifdef IS_KERNEL_SSP_SEPARATE
    //
    // Since kernel and tasks have separate canary values, replace the stack guard value with
    // the kernel guard value before calling kernel space C functions
    //
    ld      s0, __kernel_stack_chk_guard
    la      s1, __stack_chk_guard
    sd      s0, 0(s1)
#endif // IS_KERNEL_SSP_SEPARATE
#endif // IS_SSP_ENABLED

    // Increase exception nesting (now that we can as part of context is saved)
    la      s0, xPortExceptionNesting
    ld      s1, 0(s0)
    addi    s1, s1, 1
    sd      s1, 0(s0)

    // Check if we come from user - then switch to kernel stack
    ld      s0, xPortExceptionNesting
    li      s1, 1 // 1 == exception handler entered from task
    bgt     s0, s1, __PortUpdateSp

    // We came from user, switch to (fresh) kernel stack
    ld      sp, pxPortKernelStackTop // Load kernel stack if we entered kernel

    j       __PortParseSyscall

__PortUpdateSp:
    // We came from kernel, just push down SP
    mv      sp, gp

//__PortParseSyscallKernel:
    // Figure out if it's syscall or not, if it is - we can do partial switch
    csrr    s0, LW_RISCV_CSR_SSCRATCH2 // set at start of trap handler
    bnez    s0, vPortInfiniteLoop // crash because we don't allow syscalls from kernel!

    //
    // Here we know we're not handling a syscall!
    // Full context switch - save caller, do exception
    //
    contextSaveCaller
    j __PortFullContextSwitch

__PortParseSyscall:
    // Figure out if it's syscall or not, if it is - we can do partial switch
    csrr    s0, LW_RISCV_CSR_SSCRATCH2 // set at start of trap handler
    bnez    s0, __PortDoSyscall

    //
    // Here we know we're not handling a syscall!
    // Full context switch - save caller, do instrumentation callback, handle exception
    //
    contextSaveCaller

#if defined(INSTRUMENTATION) || defined(USTREAMER)
    // Handle instrumentation if enabled
    jal     lwrtosHookInstrumentationBegin
#endif // INSTRUMENTATION || USTREAMER

__PortFullContextSwitch:
    // Mark full context switch
    li      t0, 1
    sd      t0, TCB_OFFSET_FLAGS * REGBYTES(gp)

#ifdef ODP_ENABLED
    csrr    s0, scause
    addi    s0, s0, -12
    beqz    s0, __PortDoOdp // SCAUSE == 12 -> fetch page fault
    addi    s0, s0, -1
    beqz    s0, __PortDoOdp // SCAUSE == 13 -> load page fault
    addi    s0, s0, -2
    beqz    s0, __PortDoOdp // SCAUSE == 15 -> store page fault
#endif // ODP_ENABLED

__PortSpuriousOdp:
    csrr    t0, scause
    blt     t0, zero, __PortInterrupt // check if top bit set

__PortException:
    add     a0, gp, zero // ctx
    csrr    a1, scause
    csrr    a2, LW_RISCV_CSR_SCAUSE2
    csrr    a3, stval
    jal     vPortHandleException
    // restore STVEC here for "exception" case (as we can't nest in exception path)
    la      t0, vPortTrapHandler
    csrw    stvec, t0
    // Exception handler is expected to patch epc if needed
    j       __PortRestoreContextAndEret

__PortInterrupt:
    // it's now safe to restore exception handler
    la      t2, vPortTrapHandler
    csrw    stvec, t2
    li      t1, SCAUSE_SOFTWARE_INTERRUPT
    beq     t0, t1, __PortSoftwareInterrupt
    li      t1, SCAUSE_EXTERNAL_INTERRUPT
    beq     t0, t1, __PortExternalInterrupt

/* Interupt is timer interrupt */
__PortTimerInterrupt:
    jal     vPortHandleTimerIrq
    j       __PortRestoreContextAndEret

__PortSoftwareInterrupt:
    jal     vPortHandleSwIrq
    j       __PortRestoreContextAndEret

__PortExternalInterrupt:
    jal     vPortHandleExtIrq
    j       __PortRestoreContextAndEret

#ifdef ODP_ENABLED
__PortDoOdp:
    csrr    a0, stval
    jal     odpIsrHandler
    beqz    a0, __PortSpuriousOdp // If returned false, ODP wasn't serviced
    j       __PortRestoreContextAndEret
#endif // ODP_ENABLED

// No fallthrough to this point! Direct jumps only!
__PortDoSyscall:
    // When doing syscall (from user-mode), we also want to do an instr callback!
    // However, it's not safe to do it here since we could corrupt the a0-a7 registers.
    // We could save and restore them around the call, but that would be messy and inefficient.
    // Instead, we do the instrumentationBegin call inside lwrtosHookSyscall itself!

    // it's now safe to restore exception handler
    la      t0, vPortTrapHandler
    csrw    stvec, t0
    // mark partial context switch
    sd      zero, TCB_OFFSET_FLAGS * REGBYTES(gp)

    jal     lwrtosHookSyscall
    j       __PortRestoreContextAndEret


__PortRestoreContextAndEret:
    // Check if we're returning to kernel or to user
    ld      t0, xPortExceptionNesting
    // Sanity check - nesting == 0 should never happen here!
    beqz    t0, vPortInfiniteLoop
    // Decrease nesting
    addi    t0, t0, -1

    bnez     t0, __PortRestoreKernel // nesting > 0 -> return to kernel
    j __PortContextSwitchAndRestoreUser

__PortRestoreKernel:
    // Stack should at this point be top of kernel context
    add     gp, zero, sp

    ld      t0, TCB_OFFSET_FLAGS * REGBYTES(gp)
    beqz    t0, __PortPartialRestore
    j       __PortFullRestore

__PortContextSwitchAndRestoreUser:
    // Check if context switch was requested
    lbu     t0, IcCtxSwitchReq
    beqz    t0, __PortRestoreUser
    jal     vTaskSelectNextTask
    sb      zero, IcCtxSwitchReq, t0

__PortRestoreUser:
    // Restore context
    jal     lwrtosHookConfigMpuForTask
#if defined(ODP_ENABLED) && defined(DMA_SUSPENSION)
__PortPinStack:
    sb      zero, IcCtxSwitchReq, t0
    jal     lwrtosHookPinTaskStack
    lbu     t0, IcCtxSwitchReq
    beqz    t0, __PortRestoreTCB
    //
    // if we failed to pin task stack and need to context-switch,
    // try context-switching to a task until we're able to
    // pin its stack.
    //
    jal     vTaskSelectNextTask
    j       __PortPinStack
__PortRestoreTCB:
#endif // defined(ODP_ENABLED) && defined(DMA_SUSPENSION)
    ld      gp, pxLwrrentTCB

#if defined(INSTRUMENTATION) || defined(USTREAMER)
    jal     lwrtosHookInstrumentationEnd
#endif  // INSTRUMENTATION || USTREAMER

#ifdef IS_SSP_ENABLED
#ifdef IS_SSP_ENABLED_PER_TASK
    //
    // Load the per-task stack canary from the TCB and replace the
    // stack guard value with it
    //
    jal     lwrtosHookSetTaskStackCanary
#else
    //
    // If we want to use a single canary value for all tasks, simply replace
    // the stack guard value with the task guard value when restoring to user mode
    //
    ld      t0, __task_stack_chk_guard
    la      t1, __stack_chk_guard
    sd      t0, 0(t1)
#endif // IS_SSP_ENABLED_PER_TASK
#endif // IS_SSP_ENABLED

#ifdef DYNAMIC_FLCN_PRIV_LEVEL
    // Set the dynamic priv level to the value from this task's TCB
    jal     lwrtosHookPrivLevelSet
#endif // DYNAMIC_FLCN_PRIV_LEVEL

    ld      t0, TCB_OFFSET_FLAGS * REGBYTES(gp)
    beqz    t0, __PortPartialRestore

__PortFullRestore:
    contextRestoreCaller
    j       __PortFinishRestore

__PortPartialRestore:
    contextZeroCaller
    // Fallthrough

__PortFinishRestore:
    contextRestoreRaA0
    // That's latest we have register to decrease nesting, if this code crashes,
    // we're in trouble (s0-s11 are available)
    // Therefore setup stvec trap again for last restore
    la      s0, vPortInfiniteLoop
    csrw    stvec, s0

    // Decrease exception nesting
    la      s0, xPortExceptionNesting
    ld      s1, 0(s0)
    beqz    s1, vPortInfiniteLoop // sanity check, should be at least 1
    addi    s1, s1, -1
    sd      s1, 0(s0) // write exception nesting back

    contextRestoreCallee

    // No registers available to restore stvec, use sscratch and gp
    // This can't fail, unless CPU is broken
    csrw    sscratch, gp

__PortExitTrapHandler:
    la      gp, vPortTrapHandler
    csrw    stvec, gp
    csrr    gp, sscratch

    sret
EFUNC vPortTrapHandler

/*
 * Since there is a define to redefine vPortPanic as vPortDebug,
 * allow exception handler to use vPortPanic anyway.
 * This is needed as vPortDebug can cause more exceptions.
 */
.balign 4
.global vPortInfiniteLoop
vPortInfiniteLoop:
FUNC vPortPanic
    nop
    j       vPortPanic
EFUNC vPortPanic

/*
 * This is used to improve GDB stack unwinding for tasks.
 * When printing a stack trace GDB assumes the caller is whatever function
 * includes the instruction prior to the callee's return address. Since we give
 * vTaskExit as the return address for all tasks, prvTaskStart is shown as the
 * outermost frame in stack traces.
 */
.section .shared_code
FUNC prvTaskStart
    nop
.global vTaskExit
vTaskExit:
    j vTaskExitError
EFUNC prvTaskStart
