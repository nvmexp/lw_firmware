// _LWRM_COPYRIGHT_BEGIN_
//
// Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
// information contained herein is proprietary and confidential to LWPU
// Corporation.  Any use, reproduction, or disclosure without the written
// permission of LWPU Corporation is prohibited.
//
// _LWRM_COPYRIGHT_END_
//

#include <libos-config.h>
#include <manifest_defines.h>
#include <peregrine-headers.h>
#include <libos_xcsr.h>


.global KernelContextSave
.global KernelContextRestore
.global KernelSchedulerSyscallFromKernel

/**
  * @brief: Interrupt enable management
  *     Requirement:
  *        - KernelSchedulerSyscallFromKernel should not happen inside a spinlock
  *        - KernelContextSave/KernelContextRestore must not tamper with XIE
  *      
  *     Scenario. Thread enters scheduler and returns to different thread
  *         \-> Trap hardware disables interrupts 
  *         \-> Trap hardware saves SIE into SPIE.
  *         \-> KernelContextRestore saves xstatus (MPIE is the actual enable)
  *         \-> KernelContextRestore restores xstatus
  *         \-> KernelContextRestore calls eret restoring SPIE->SIE
  *
  *     Scenario. System call from kernel server (supervisor)
  *         \-> KernelSchedulerSyscallFromKernel disables interrupts 
  *             Saves old xstatus
  *         \-> Code ensures XIE was set (forbidden to call from inside interrupts)
  *         \-> Copies XIE into XPIE so KernelContextRestore is happy
  *         \-> Saves modified xstatus
  *         \-> KernelContextRestore restores xstatus
  *         \-> KernelContextRestore calls eret restoring SPIE->SIE
  *
  */

/**
  * @brief: KernelSchedulerLock management.
  *
  * Technically:
  *   - KernelContextRestore should release the spinlock
  *   - KernelContextSave should aquire the lock
  *
  * But:
  *   - We can't release the lock in these two scenarios as 
  *     we're still on the scheduler stack.
  *   - Releasing it would restoring a garbage xstatus.
  * 
  * However:
  *   - In SMP another core may wish to hold off the scheduler
  *     on this core. E.g. for a migrate.  
  *
  */

/**
 * @brief Declare the scheduler stack
 */
.section .hot.data.KernelSchedulerStack
.global KernelSchedulerStack
.global KernelSchedulerStackTop
.align 8
KernelSchedulerStack:
   .fill LIBOS_CONFIG_KERNEL_STACK_SIZE, 1, 0
KernelSchedulerStackTop:

/**
 * @brief Syscalls into the scheduler run on this stack
 * 
 */

.section .text.context

#if !LIBOS_TINY
/*!
 * @brief Switches to the scheduler stack.
 * 
 * Use for operations making blocking root
 * partition calls in the init path.
 */

.global KernelSchedulerSwitchToStack
KernelSchedulerSwitchToStack:
#if LIBOS_DEBUG
        // Set ourselves as owner of scheduler lock
        csrr     tp, sscratch
        la       gp, KernelSchedulerLock
        sd       x0, 0(gp)                      // sentinel
        sd       tp, 8(gp)                      // @todo: offsetof(ownerScratch)
#endif

        la sp, KernelSchedulerStackTop
        jr a7

/*!
 * @brief Analogous to "ECALL" for kernel tasks running
 *        in supervisor mode.
 * 
 * On RISC-V ECALL is defined as raising the call 
 * up to the next privelege level.  Thus we need
 * to explicitly avoid making this call for s-mode.
 *
 * This routine is used by libport.c and friends in place of ECALL.
 *
 * @params[in] t0 - Address of the syscall function to run
 * @params[in] gp - Return address 
 * @params[in] a0-a7 t0-t2 - Syscall arguments
 *
 */

KernelSchedulerSyscallFromKernel:
        // Save return address to xepc to free up
        // a register.
        csrw      sepc,  gp

        /**
          * KernelSchedulerLock acquire.
          * 
          * Disable interrupts and store previous state.
          */

#if LIBOS_CONFIG_RISCV_S_MODE
        csrrci    gp, sstatus, 2 /*REF_DEF64(LW_RISCV_CSR_SSTATUS_SIE, _ENABLE)*/
#else
        csrrci    gp, mstatus, 8 /*REF_DEF64(LW_RISCV_CSR_MSTATUS_MIE, _ENABLE)*/
#endif

        /**
          * Ilwariant: syscall interface cannot be called inside a spinlock
          */
#if LIBOS_DEBUG
#if LIBOS_CONFIG_RISCV_S_MODE
        andi      tp, gp, 2     /*REF_DEF64(LW_RISCV_CSR_SSTATUS_SIE, _ENABLE)*/
#else
        andi      tp, gp, 8     /*REF_DEF64(LW_RISCV_CSR_MSTATUS_MIE, _ENABLE)*/
#endif
        beq       tp, x0, KernelPanic
#endif

        /**
          *  Copy SIE->SPIE.  We know SIE is true from ilwariant above.
          */
#if LIBOS_CONFIG_RISCV_S_MODE
        ori       gp, gp,  32          /*REF_DEF64(LW_RISCV_CSR_SSTATUS_SPIE, _ENABLE)*/
#else
        ori       gp, gp,  128         /*REF_DEF64(LW_RISCV_CSR_MSTATUS_MPIE, _ENABLE)*/
#endif

        // Get the task context pointer
#if LIBOS_CONFIG_RISCV_S_MODE
        csrr     tp, sscratch
#else
        csrr     tp, mscratch
#endif

        // Save the xstatus register (for interrupt enable)
        sd        gp, OFFSETOF_TASK_XSTATUS(tp)

        /**
           While we are technically holding the scheduler lock,
           it is released as part of exiting the scheduler (and loading the next task).

           Place a sentinel value in xstatus to force KernelSpinlockRelease to crash
           if the kernel attempts to release this spinlock inside the scheduler.
        */

#if LIBOS_DEBUG
        // Set ourselves as owner of scheduler lock
        la       gp, KernelSchedulerLock
        sd       x0, 0(gp)                      // sentinel
        sd       tp, 8(gp)                      // @todo: offsetof(ownerScratch)
#endif

        // Save the return address
        csrr     gp, sepc
        sd       gp, OFFSETOF_TASK_XEPC(tp)

        // Save general purpose registers
        sd       t0, OFFSETOF_TASK_REGISTERS+5*8(tp)
        sd       t1, OFFSETOF_TASK_REGISTERS+6*8(tp)
        sd       t2, OFFSETOF_TASK_REGISTERS+7*8(tp)
        sd       t3, OFFSETOF_TASK_REGISTERS+28*8(tp)
        sd       t4, OFFSETOF_TASK_REGISTERS+29*8(tp)
        sd       t5, OFFSETOF_TASK_REGISTERS+30*8(tp)
        sd       t6, OFFSETOF_TASK_REGISTERS+31*8(tp)
        sd       a0, OFFSETOF_TASK_REGISTERS+10*8(tp)
        sd       a1, OFFSETOF_TASK_REGISTERS+11*8(tp)
        sd       a2, OFFSETOF_TASK_REGISTERS+12*8(tp)
        sd       a3, OFFSETOF_TASK_REGISTERS+13*8(tp)
        sd       a4, OFFSETOF_TASK_REGISTERS+14*8(tp)
        sd       a5, OFFSETOF_TASK_REGISTERS+15*8(tp)
        sd       a6, OFFSETOF_TASK_REGISTERS+16*8(tp)
        sd       a7, OFFSETOF_TASK_REGISTERS+17*8(tp)
        sd       s0, OFFSETOF_TASK_REGISTERS+8*8(tp)
        sd       s1, OFFSETOF_TASK_REGISTERS+9*8(tp)
        sd       s2, OFFSETOF_TASK_REGISTERS+18*8(tp)
        sd       s3, OFFSETOF_TASK_REGISTERS+19*8(tp)
        sd       s4, OFFSETOF_TASK_REGISTERS+20*8(tp)
        sd       s5, OFFSETOF_TASK_REGISTERS+21*8(tp)
        sd       s6, OFFSETOF_TASK_REGISTERS+22*8(tp)
        sd       s7, OFFSETOF_TASK_REGISTERS+23*8(tp)
        sd       s8, OFFSETOF_TASK_REGISTERS+24*8(tp)
        sd       s9, OFFSETOF_TASK_REGISTERS+25*8(tp)
        sd       s10, OFFSETOF_TASK_REGISTERS+26*8(tp)
        sd       s11, OFFSETOF_TASK_REGISTERS+27*8(tp)
        sd       ra, OFFSETOF_TASK_REGISTERS+1*8(tp)
        sd       sp, OFFSETOF_TASK_REGISTERS+2*8(tp)

        // Enable kernel stack
        la       sp, KernelSchedulerStackTop

        // Run the kernel function
        jalr     ra, t0

        // Restore the task context
        jal      KernelTaskReturn
#endif

/*!
 * @brief Save all registers to the task context area.
 *        Enter the scheduler lock, and switch to the scheduler stack.
 *
 * @params[in] gp - Return address 
 * @params[in] a0-a7 t0-t2 - Syscall arguments
 *
 */
KernelContextSave:
        // Load thread state
#if LIBOS_CONFIG_RISCV_S_MODE
        csrr     tp, sscratch
#else
        csrr     tp, mscratch
#endif

        // Save general purpose registers
        sd       t0, OFFSETOF_TASK_REGISTERS+5*8(tp)
        sd       t1, OFFSETOF_TASK_REGISTERS+6*8(tp)
        sd       t2, OFFSETOF_TASK_REGISTERS+7*8(tp)
        sd       t3, OFFSETOF_TASK_REGISTERS+28*8(tp)
        sd       t4, OFFSETOF_TASK_REGISTERS+29*8(tp)
        sd       t5, OFFSETOF_TASK_REGISTERS+30*8(tp)
        sd       t6, OFFSETOF_TASK_REGISTERS+31*8(tp)
        sd       a0, OFFSETOF_TASK_REGISTERS+10*8(tp)
        sd       a1, OFFSETOF_TASK_REGISTERS+11*8(tp)
        sd       a2, OFFSETOF_TASK_REGISTERS+12*8(tp)
        sd       a3, OFFSETOF_TASK_REGISTERS+13*8(tp)
        sd       a4, OFFSETOF_TASK_REGISTERS+14*8(tp)
        sd       a5, OFFSETOF_TASK_REGISTERS+15*8(tp)
        sd       a6, OFFSETOF_TASK_REGISTERS+16*8(tp)
        sd       a7, OFFSETOF_TASK_REGISTERS+17*8(tp)
        sd       s0, OFFSETOF_TASK_REGISTERS+8*8(tp)
        sd       s1, OFFSETOF_TASK_REGISTERS+9*8(tp)
        sd       s2, OFFSETOF_TASK_REGISTERS+18*8(tp)
        sd       s3, OFFSETOF_TASK_REGISTERS+19*8(tp)
        sd       s4, OFFSETOF_TASK_REGISTERS+20*8(tp)
        sd       s5, OFFSETOF_TASK_REGISTERS+21*8(tp)
        sd       s6, OFFSETOF_TASK_REGISTERS+22*8(tp)
        sd       s7, OFFSETOF_TASK_REGISTERS+23*8(tp)
        sd       s8, OFFSETOF_TASK_REGISTERS+24*8(tp)
        sd       s9, OFFSETOF_TASK_REGISTERS+25*8(tp)
        sd       s10, OFFSETOF_TASK_REGISTERS+26*8(tp)
        sd       s11, OFFSETOF_TASK_REGISTERS+27*8(tp)
        sd       ra, OFFSETOF_TASK_REGISTERS+1*8(tp)
        sd       sp, OFFSETOF_TASK_REGISTERS+2*8(tp)

        // Save the xstatus register (for interrupt enable)
#if !LIBOS_TINY
#if LIBOS_CONFIG_RISCV_S_MODE
        csrr     t4, sstatus
#else
        csrr     t4, mstatus
#endif
        sd       t4, OFFSETOF_TASK_XSTATUS(tp)  

#if LIBOS_DEBUG
        // Set ourselves as owner of scheduler lock
        la       t4, KernelSchedulerLock
        sd       x0, 0(t4)                      // sentinel
        sd       tp, 8(t4)                      // @todo: offsetof(ownerScratch)
#endif
#endif

#if LIBOS_CONFIG_RISCV_S_MODE
        // Save faulting address (page fault)
        csrr    t4, sbadaddr
        sd      t4, OFFSETOF_TASK_XBADADDR(tp)

        // Save halting PC address  (will be written to field later)
        csrr    t6,  sepc
        sd      t6,  OFFSETOF_TASK_XEPC(tp)

        // Dispatch into trap handler
        csrr    t4, scause
#else
        // Save faulting address (page fault)
        csrr    t4, mbadaddr
        sd      t4, OFFSETOF_TASK_XBADADDR(tp)

        // Save halting PC address  (will be written to field later)
        csrr    t6, mepc
        sd      t6, OFFSETOF_TASK_XEPC(tp)

        // Dispatch into trap handler
        csrr    t4, mcause
#endif
        // Save faulting reason
        sd      t4, OFFSETOF_TASK_XCAUSE(tp)

        // Enable kernel stack
        la      sp, KernelSchedulerStackTop

        // Return to caller
        jalr    x0, gp


KernelContextRestore:
        mv      tp, a0

        // Update kernel scratch location
#if LIBOS_CONFIG_RISCV_S_MODE
        csrw  sscratch, tp
#else
        csrw  mscratch, tp
#endif

        // Restore SSTATUS (both for SPP to allow supervisor tasks *and*
        // to allow interrupts disabled)
#if !LIBOS_TINY
        ld      a0, OFFSETOF_TASK_XSTATUS(tp)
#if LIBOS_CONFIG_RISCV_S_MODE
        csrw    sstatus, a0
#else
        csrw    mstatus, a0
#endif
#endif

        // Release the lock
#if LIBOS_DEBUG && !LIBOS_TINY
        la       t4, KernelSchedulerLock
        sd       x0, 0(t4)
        sd       x0, 8(t4)
#endif

        // Load the new registers
        ld      s0, OFFSETOF_TASK_REGISTERS+8*8(tp)
        ld      s1, OFFSETOF_TASK_REGISTERS+9*8(tp)
        ld      s2, OFFSETOF_TASK_REGISTERS+18*8(tp)
        ld      s3, OFFSETOF_TASK_REGISTERS+19*8(tp)
        ld      s4, OFFSETOF_TASK_REGISTERS+20*8(tp)
        ld      s5, OFFSETOF_TASK_REGISTERS+21*8(tp)
        ld      s6, OFFSETOF_TASK_REGISTERS+22*8(tp)
        ld      s7, OFFSETOF_TASK_REGISTERS+23*8(tp)
        ld      s8, OFFSETOF_TASK_REGISTERS+24*8(tp)
        ld      s9, OFFSETOF_TASK_REGISTERS+25*8(tp)
        ld      s10, OFFSETOF_TASK_REGISTERS+26*8(tp)
        ld      s11, OFFSETOF_TASK_REGISTERS+27*8(tp)

        ld      ra, OFFSETOF_TASK_REGISTERS+1*8(tp)
        ld      sp, OFFSETOF_TASK_REGISTERS+2*8(tp)

        ld      t0, OFFSETOF_TASK_XEPC(tp)

#if LIBOS_CONFIG_RISCV_S_MODE
        csrw    sepc, t0
#else
        csrw    mepc, t0
#endif

        ld      t0, OFFSETOF_TASK_REGISTERS+5*8(tp)
        ld      t1, OFFSETOF_TASK_REGISTERS+6*8(tp)
        ld      t2, OFFSETOF_TASK_REGISTERS+7*8(tp)
        ld      t3, OFFSETOF_TASK_REGISTERS+28*8(tp)
        ld      t4, OFFSETOF_TASK_REGISTERS+29*8(tp)
        ld      t5, OFFSETOF_TASK_REGISTERS+30*8(tp)
        ld      t6, OFFSETOF_TASK_REGISTERS+31*8(tp)
        ld      a0, OFFSETOF_TASK_REGISTERS+10*8(tp)
        ld      a1, OFFSETOF_TASK_REGISTERS+11*8(tp)
        ld      a2, OFFSETOF_TASK_REGISTERS+12*8(tp)
        ld      a3, OFFSETOF_TASK_REGISTERS+13*8(tp)
        ld      a4, OFFSETOF_TASK_REGISTERS+14*8(tp)
        ld      a5, OFFSETOF_TASK_REGISTERS+15*8(tp)
        ld      a6, OFFSETOF_TASK_REGISTERS+16*8(tp)
        ld      a7, OFFSETOF_TASK_REGISTERS+17*8(tp)

        // Restore TP
        ld      tp, OFFSETOF_TASK_REGISTERS+4*8(tp)

#if LIBOS_CONFIG_RISCV_S_MODE
        sret
#else
        mret
#endif
