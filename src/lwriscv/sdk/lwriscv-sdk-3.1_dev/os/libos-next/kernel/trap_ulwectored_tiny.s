#include "libos-config.h"
#include "manifest_defines.h"
#include "peregrine-headers.h"

.section .hot.text
.global KernelTrapLoad

/*
   Compute the dispatch table index

   @note: scause has the interrupt/trap flag as the sign bit.

   @note: tp and gp are read-only to user-space and thus
         free for clobbering in the trap handler. These
         values are not saved in the later context save.
*/

#define INTERRUPT 0x8000000000000000

.align 4                        /* Lower 2 bits of stvec must be zero */
KernelTrapUlwectored:
        csrr    tp, scause      /* I=bit(63) and reason=bits(4..0) */
        li      gp, LW_RISCV_CSR_MCAUSE_EXCODE_UCALL
        beq     gp, tp, KernelTrapSyscall
        li      gp, INTERRUPT+LW_RISCV_CSR_MCAUSE_EXCODE_S_TINT
        beq     gp, tp, KernelTrapTimer
        li      gp, INTERRUPT+LW_RISCV_CSR_MCAUSE_EXCODE_S_EINT
        beq     gp, tp, KernelTrapInterrupt
        j       KernelPanic

.global KernelTrapUnhandledTask
KernelTrapUnhandledTask:
        jal     gp, KernelContextSave
        jal     x0, KernelTaskPanic

.global KernelTrapTimer
KernelTrapTimer:
        jal     gp, KernelContextSave
        jal     x0, KernelSchedulerInterrupt

.global KernelTrapInterrupt
KernelTrapInterrupt:
        jal     gp, KernelContextSave
        jal     x0, KernelTaskPanic

KernelTrapLoad:
        la      a0, KernelTrapUlwectored
        csrw    stvec, a0
        jalr    x0, ra
