#include "libos-config.h"
#include "manifest_defines.h"

.section .hot.text
.global KernelTrapUlwectored

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

.global KernelTrapSyscall
.global KernelContinueSyscall
KernelTrapSyscall:
        jal     gp, KernelContextSave

KernelContinueSyscall:                                                  /* LIBOS_SYSCALL_INSTRUCTION branches here for syscall dispatch */
        li      t1, ASM_SYSCALL_LIMIT
        bge     t0, t1, KernelTaskPanic
        slli    t4, t0, 2
        lui     t1, %hi(ucall_dispatch_table)
        add     t4, t4, t1
        lw      t4, %lo(ucall_dispatch_table)(t4)

        /* Auto advance mepc to point to instruction after UCALL */
        addi t6, t6, 4
        sd t6, OFFSETOF_TASK_XEPC(tp)

        /* Dispatch to syscall */
        jalr    ra, t4

        /* Return control back to scheduler */
        j    KernelSyscallReturn
