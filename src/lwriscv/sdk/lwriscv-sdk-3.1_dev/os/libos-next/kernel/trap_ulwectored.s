#include "libos-config.h"
#include "manifest_defines.h"

.section .text.trap
.global KernelTrapLoad

/*
   Compute the dispatch table index

   @note: scause has the interrupt/trap flag as the sign bit.

   @note: tp and gp are read-only to user-space and thus
         free for clobbering in the trap handler. These
         values are not saved in the later context save.

   @brief Main interrupt/trap dispatch table

    Hardware mcause
              63                4            0
        ---------------------------------------
        | Interrupt? |         Int # / Trap # |
        ---------------------------------------

    The software table shifts the interrupt bit to bit 5 (DISPATCH_INTERRUPT).
    There are less than 32 traps, and less than 16 interrupts.

    These are stored as 32-bit addresses as guaranteed by mcmodel=medlow.

*/

.align 4 // stvec must be 4 byte aligned
KernelTrapUlwectored:
        csrr    tp, scause      /* I=bit(63) and reason=bits(4..0) */
        slli    gp, tp, 2       /* gp = reason * 4 */
        srli    tp, tp, 63-7    /* tp = I*128 */
        add     tp, tp, gp      /* tp = 4 * (I*32 + reason) */

        /* Perform table lookup */
.ulwectored_dispatch_auipc:
        auipc   gp, %pcrel_hi(KernelTrapUlwectoredTable)
        add     gp, gp, tp
        lw      gp, %pcrel_lo(.ulwectored_dispatch_auipc)(gp)

        /* softmmu.s requires tp to be 4 for dispatch */
        li      tp, 4

        /* Dispatch */
        jalr    x0, gp

.global KernelTrapInstructionAccess
.global KernelTrapInstructionIllegal
.global KernelTrapInstructionBreakpoint
.global KernelTrapLoadMisaligned
.global KernelTrapLoadAccess
.global KernelTrapStoreMisaligned
.global KernelTrapStoreAccess
.global KernelTrapInstructionAccess
.global KernelTrapInstructionMisaligned
KernelTrapInstructionMisaligned:
KernelTrapInstructionIllegal:
KernelTrapInstructionBreakpoint:
KernelTrapLoadMisaligned:
KernelTrapLoadAccess:
KernelTrapStoreMisaligned:
KernelTrapStoreAccess:
KernelTrapInstructionAccess:
        jal gp, KernelContextSave
        jal x0, KernelTaskPanic

.global KernelTrapTimer
KernelTrapTimer:
        jal gp, KernelContextSave
        jal x0, KernelSchedulerInterrupt

.global KernelTrapInterrupt
KernelTrapInterrupt:
        jal gp, KernelContextSave
        jal x0, KernelTaskPanic

KernelTrapLoad:
        la      a0, KernelTrapUlwectored
        csrw    stvec, a0
        jalr    x0, ra
