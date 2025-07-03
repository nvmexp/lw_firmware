/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "trap.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "dev_riscv_pri.h"
#include "dma.h"
#include "extintr.h"
#include "gdma.h"
#include "include/libos_syscalls.h"
#include "interrupts.h"
#include "ilwerted_pagetable.h"
#include "kernel.h"
#include "kernel_copy.h"
#include "mmu.h"
#include "lwtypes.h"
#include "paging.h"
#include "partition.h"
#include "port.h"
#include "profiler.h"
#include "riscv-isa.h"
#include "task.h"
#include "timer.h"

// @brief Main interrupt/trap dispatch table
//
//  Hardware mcause
//            63                4            0
//      ---------------------------------------
//      | Interrupt? |         Int # / Trap # |
//      ---------------------------------------
//
//  The software table shifts the interrupt bit to bit 5 (DISPATCH_INTERRUPT).
//  There are less than 32 traps, and less than 16 interrupts.
//
//  These are stored as 32-bit addresses as guaranteed by mcmodel=medlow.
//

#define DISPATCH_INTERRUPT 32

LIBOS_PTR32(void) LIBOS_SECTION_DMEM_PINNED(dispatch_table)[48];

// @todo Hardware manuals should define _LAST_INTERRUPT and _LAST_TRAP
_Static_assert(LW_RISCV_LAST_EXCEPTION < DISPATCH_INTERRUPT, "Trap table overrun");
_Static_assert(LW_RISCV_LAST_INTERRUPT <
        (sizeof(dispatch_table) / sizeof(*dispatch_table) - DISPATCH_INTERRUPT),
    "Trap table overrun");

// @brief Main syscall dispatch table
LIBOS_PTR32(void) LIBOS_SECTION_DMEM_PINNED(ucall_dispatch_table)[LIBOS_SYSCALL_LIMIT_POW2];

void LIBOS_SECTION_TEXT_COLD kernel_trap_init()
{
    // Panic kernel on unrecognized trap or interupt
    for (LwU32 i = 0; i < sizeof(dispatch_table) / sizeof(*dispatch_table); i++)
        dispatch_table[i] = LIBOS_PTR32_NARROW(panic_kernel);

    // Panic task on unknown UCALL
    for (LwU32 i = 0; i < LIBOS_SYSCALL_LIMIT_POW2; i++)
        ucall_dispatch_table[i] = LIBOS_PTR32_NARROW(kernel_task_panic);

    // Syscalls
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_MEMORY_READ] =
        LIBOS_PTR32_NARROW(kernel_syscall_task_memory_read);
    ucall_dispatch_table[LIBOS_SYSCALL_PROCESSOR_SUSPEND] =
        LIBOS_PTR32_NARROW(kernel_syscall_processor_suspend);
    ucall_dispatch_table[LIBOS_SYSCALL_DCACHE_ILWALIDATE] =
        LIBOS_PTR32_NARROW(kernel_syscall_ilwalidate_cache);
    ucall_dispatch_table[LIBOS_SYSCALL_MEMORY_QUERY] =
        LIBOS_PTR32_NARROW(kernel_syscall_memory_query);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_STATE_QUERY] =
        LIBOS_PTR32_NARROW(kernel_syscall_task_query);
    ucall_dispatch_table[LIBOS_SYSCALL_SHUTTLE_RESET] =
        LIBOS_PTR32_NARROW(kernel_syscall_shuttle_reset);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT] =
        LIBOS_PTR32_NARROW(kernel_syscall_port_send_recv);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_YIELD] =
        LIBOS_PTR32_NARROW(kernel_syscall_task_yield);
    ucall_dispatch_table[LIBOS_SYSCALL_INIT_REGISTER_PAGETABLES] =
        LIBOS_PTR32_NARROW(kernel_syscall_init_register_pagetables);
    ucall_dispatch_table[LIBOS_SYSCALL_PROFILER_ENABLE] =
        LIBOS_PTR32_NARROW(kernel_syscall_profiler_enable);
    ucall_dispatch_table[LIBOS_SYSCALL_INTERRUPT_ARM_EXTERNAL] =
        LIBOS_PTR32_NARROW(kernel_syscall_interrupt_arm_external);
    ucall_dispatch_table[LIBOS_SYSCALL_TIMER_GET_NS] =
        LIBOS_PTR32_NARROW(kernel_syscall_timer_get_ns);
    ucall_dispatch_table[LIBOS_SYSCALL_DCACHE_ILWALIDATE_ALL] =
        LIBOS_PTR32_NARROW(kernel_syscall_ilwalidate_cache_all);
    ucall_dispatch_table[LIBOS_SYSCALL_PROFILER_GET_COUNTER_MASK] =
        LIBOS_PTR32_NARROW(kernel_syscall_profiler_get_counter_mask);
#if LIBOS_CONFIG_GDMA_SUPPORT
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_DMA_REGISTER_TCM] =
        LIBOS_PTR32_NARROW(kernel_syscall_gdma_register_tcm);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_DMA_COPY] =
        LIBOS_PTR32_NARROW(kernel_syscall_gdma_copy);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_DMA_FLUSH] =
        LIBOS_PTR32_NARROW(kernel_syscall_gdma_flush);
#else
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_DMA_REGISTER_TCM] =
        LIBOS_PTR32_NARROW(kernel_syscall_dma_register_tcm);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_DMA_COPY] =
        LIBOS_PTR32_NARROW(kernel_syscall_dma_copy);
    ucall_dispatch_table[LIBOS_SYSCALL_TASK_DMA_FLUSH] =
        LIBOS_PTR32_NARROW(kernel_syscall_dma_flush);
#endif
#if LIBOS_CONFIG_PARTITION
    ucall_dispatch_table[LIBOS_SYSCALL_PARTITION_SWITCH] =
        LIBOS_PTR32_NARROW(kernel_syscall_partition_switch);
#endif

    // LIBOS_SYSCALL_TASK_EXIT needs no explicit dispatch as we want kernel_task_panic for now
    // LIBOS_SYSCALL_TASK_PANIC needs no explicit dispatch as we want kernel_task_panic anyway

    // Instruction faults
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_IACC_FAULT] = LIBOS_PTR32_NARROW(kernel_codefault);
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_IAMA]       = LIBOS_PTR32_NARROW(kernel_task_panic);
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_ILL]        = LIBOS_PTR32_NARROW(kernel_task_panic);
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_BKPT]       = LIBOS_PTR32_NARROW(kernel_task_panic);

    // Data faults
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT] = LIBOS_PTR32_NARROW(kernel_datafault);
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT] = LIBOS_PTR32_NARROW(kernel_datafault);
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_LAMA]       = LIBOS_PTR32_NARROW(kernel_task_panic);
    dispatch_table[LW_RISCV_CSR_XCAUSE_EXCODE_SAMA]       = LIBOS_PTR32_NARROW(kernel_task_panic);

    // Timer interrupt
    dispatch_table[DISPATCH_INTERRUPT | LW_RISCV_CSR_XCAUSE_EXCODE_X_TINT] = LIBOS_PTR32_NARROW(kernel_timer_interrupt);

#ifdef LW_RISCV_CSR_SCAUSE_EXCODE_IPAGE_FAULT
    dispatch_table[LW_RISCV_CSR_SCAUSE_EXCODE_IPAGE_FAULT] = LIBOS_PTR32_NARROW(kernel_codefault);
    dispatch_table[LW_RISCV_CSR_SCAUSE_EXCODE_LPAGE_FAULT] = LIBOS_PTR32_NARROW(kernel_datafault);
    dispatch_table[LW_RISCV_CSR_SCAUSE_EXCODE_SPAGE_FAULT] = LIBOS_PTR32_NARROW(kernel_datafault);
#endif

    // External interrupt
    dispatch_table[DISPATCH_INTERRUPT | LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT] =
        LIBOS_PTR32_NARROW(kernel_external_interrupt);
#ifdef LW_RISCV_CSR_SCAUSE_EXCODE_S_EINT
    dispatch_table[DISPATCH_INTERRUPT | LW_RISCV_CSR_SCAUSE_EXCODE_S_EINT] =
        LIBOS_PTR32_NARROW(kernel_external_interrupt);
#endif
}

void LIBOS_SECTION_TEXT_COLD kernel_trap_load()
{
    // Setup trap handler
    csr_write(LW_RISCV_CSR_XTVEC, (LwU64)kernel_trap);
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN __attribute__((naked)) void kernel_trap(void)
{
    __asm(
        /*
            Restore the kernel task pointer (tp)
              @note: Usermode thread pointer is kernel controlled,
                     and thus not saved on context save.  userspace
                     wish to change this register *must* call the kernel_va
                     to update the context.

              xscratch:
                - lwrrently running user task
                - null while in kernel
        */
        "mv         tp, x0; "
#if LIBOS_CONFIG_RISCV_S_MODE
        "csrrwi     tp, sscratch, 0;"
#else
        "csrrwi     tp, mscratch, 0;"
#endif

        /*
          Were we in the kernel when we trapped
            @note: We don't need to save registers since the kernel will never
                   directly resume.  Instead, we resume from a stored continuation.
        */
        "beq tp, x0, .kernel_trap_dispatch;"

        /* Save general purpose registers */
        "sd t0,    %c[x0_offset]+5*8(tp);"
        "sd t1,    %c[x0_offset]+6*8(tp);"
        "sd t2,    %c[x0_offset]+7*8(tp);"
        "sd t3,    %c[x0_offset]+28*8(tp);"
        "sd t4,    %c[x0_offset]+29*8(tp);"
        "sd t5,    %c[x0_offset]+30*8(tp);"
        "sd t6,    %c[x0_offset]+31*8(tp);"
        "sd a0,    %c[x0_offset]+10*8(tp);"
        "sd a1,    %c[x0_offset]+11*8(tp);"
        "sd a2,    %c[x0_offset]+12*8(tp);"
        "sd a3,    %c[x0_offset]+13*8(tp);"
        "sd a4,    %c[x0_offset]+14*8(tp);"
        "sd a5,    %c[x0_offset]+15*8(tp);"
        "sd a6,    %c[x0_offset]+16*8(tp);"
        "sd a7,    %c[x0_offset]+17*8(tp);"
        "sd s0,    %c[x0_offset]+8*8(tp);"
        "sd s1,    %c[x0_offset]+9*8(tp);"
        "sd s2,    %c[x0_offset]+18*8(tp);"
        "sd s3,    %c[x0_offset]+19*8(tp);"
        "sd s4,    %c[x0_offset]+20*8(tp);"
        "sd s5,    %c[x0_offset]+21*8(tp);"
        "sd s6,    %c[x0_offset]+22*8(tp);"
        "sd s7,    %c[x0_offset]+23*8(tp);"
        "sd s8,    %c[x0_offset]+24*8(tp);"
        "sd s9,    %c[x0_offset]+25*8(tp);"
        "sd s10,   %c[x0_offset]+26*8(tp);"
        "sd s11,   %c[x0_offset]+27*8(tp);"
        "sd ra,    %c[x0_offset]+1*8(tp);"
        "sd sp,    %c[x0_offset]+2*8(tp);"

#if LIBOS_CONFIG_RISCV_S_MODE
        /* Save faulting address (page fault)  */
        "csrr t4,  sbadaddr;"
        "sd t4,    %c[xbadaddr_offset](tp);"

        /* Save halting PC address  (will be written to field later) */
        "csrr t6,  sepc;"

        /* Dispatch into trap handler */
        "csrr    t4, scause;"
#else
        /* Save faulting address (page fault)  */
        "csrr t4,  mbadaddr;"
        "sd t4,    %c[xbadaddr_offset](tp);"

        /* Save halting PC address  (will be written to field later) */
        "csrr t6,  mepc;"

        /* Dispatch into trap handler */
        "csrr    t4, mcause;"
#endif

        /* Save faulting reason */
        "sd t4,    %c[xcause_offset](tp);"

        /* Enable kernel stack */
        "la sp,    kernel_stack+%c[stack_size];"

        /* Fastpath to UCALL dispatch */
        "li      t1, %c[csr_xcause_excode_ucall];"
        "beq     t4, t1, .ucall_dispatch;"

        /* Not a UCALL? mepc should point to faulting instruction */
        "sd t6, %c[xepc_offset](tp);"

        /* Callwlate dispatch table index */
        "slli    t1, t4, 2;"

        /* Shift the interrupt ids to start at index 32 */
        "srli    t4, t4, 63-7;" /* move top bit to DISPATCH_INTERRUPT */
        "add     t4, t4, t1;"

        /* Lookup in dispatch table */
        "dispatch_table_reloc: ;"
        "auipc   t2, %%pcrel_hi(dispatch_table);"
        "add     t4, t4, t2;"
        "lw      t4, %%pcrel_lo(dispatch_table_reloc)(t4);"

        /* Dispatch */
        "jalr    x0, t4;"

        /* UCALL dispatch */
        ".ucall_dispatch:;"
        "li      t1, %c[syscall_limit_pow2];"
        "bge     t0, t1, kernel_task_panic;"
        "slli    t4, t0, 2;"
        "ucall_dispatch_table_reloc: ;"
        "auipc   t1, %%pcrel_hi(ucall_dispatch_table); " // TODO: Shrink me
        "add     t4, t4, t1;"
        "lw      t4, %%pcrel_lo(ucall_dispatch_table_reloc)(t4);"

        /* Auto advance mepc to point to instruction after UCALL */
        "addi t6, t6, 4; "
        "sd t6, %c[xepc_offset](tp);"

        /* Dispatch to syscall */
        "jalr    ra, t4;"

        /* Syscalls are allowed to return */
        "j kernel_return; "

        /* Kernel traps are continuation based and do not require register saves */
        ".kernel_trap_dispatch:;"

        /* Restore kernel stack */
        "la sp,    kernel_stack+%c[stack_size];"

        /*
           Restore mstatus to safe default for this task
           (the nested kernel trap replaced previous privelege)
        */
        "la        t4,      user_xstatus;"
        "ld        t4,      0(t4);"
#if LIBOS_CONFIG_RISCV_S_MODE
        "csrw      sstatus, t4;"
#else
        "csrw      mstatus, t4;"
#endif

        /*
           Lwrrently, the only valid kernel trap handler is for the kernel copy
        */
        "la        ra, panic_kernel;"
        "jal       x0, kernel_handle_copy_fault;" ::[csr_xcause_excode_ucall] "i"(
            LW_RISCV_CSR_XCAUSE_EXCODE_UCALL),
        [ syscall_limit_pow2 ] "i"(LIBOS_SYSCALL_LIMIT_POW2), [ stack_size ] "i"(LIBOS_KERNEL_STACK_SIZE),
        [ x0_offset ] "i"(offsetof(libos_task_t, state.registers)),
        [ xbadaddr_offset ] "i"(offsetof(libos_task_t, state.xbadaddr)),
        [ xcause_offset ] "i"(offsetof(libos_task_t, state.xcause)),
        [ xepc_offset ] "i"(offsetof(libos_task_t, state.xepc)));
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN __attribute__((naked)) void
                          kernel_raw_return_to_task(libos_task_t *return_to)
{
    __asm(
        "mv tp,    a0;"

        /* Update kernel scratch location */
#if LIBOS_CONFIG_RISCV_S_MODE
        "csrrw  x0,sscratch, tp;"
#else
        "csrrw  x0,mscratch, tp;"
#endif

        /* Load the new registers */
        "ld s0,    %c[x0_offset]+8*8(tp);"
        "ld s1,    %c[x0_offset]+9*8(tp);"
        "ld s2,    %c[x0_offset]+18*8(tp);"
        "ld s3,    %c[x0_offset]+19*8(tp);"
        "ld s4,    %c[x0_offset]+20*8(tp);"
        "ld s5,    %c[x0_offset]+21*8(tp);"
        "ld s6,    %c[x0_offset]+22*8(tp);"
        "ld s7,    %c[x0_offset]+23*8(tp);"
        "ld s8,    %c[x0_offset]+24*8(tp);"
        "ld s9,    %c[x0_offset]+25*8(tp);"
        "ld s10,   %c[x0_offset]+26*8(tp);"
        "ld s11,   %c[x0_offset]+27*8(tp);"

        "ld ra,    %c[x0_offset]+1*8(tp);"
        "ld sp,    %c[x0_offset]+2*8(tp);"

        "ld t0,    %c[xepc_offset](tp);"
#if LIBOS_CONFIG_RISCV_S_MODE
        "csrw sepc, t0;"
#else
        "csrw mepc, t0;"
#endif

        "ld t0,    %c[x0_offset]+5*8(tp);"
        "ld t1,    %c[x0_offset]+6*8(tp);"
        "ld t2,    %c[x0_offset]+7*8(tp);"
        "ld t3,    %c[x0_offset]+28*8(tp);"
        "ld t4,    %c[x0_offset]+29*8(tp);"
        "ld t5,    %c[x0_offset]+30*8(tp);"
        "ld t6,    %c[x0_offset]+31*8(tp);"
        "ld a0,    %c[x0_offset]+10*8(tp);"
        "ld a1,    %c[x0_offset]+11*8(tp);"
        "ld a2,    %c[x0_offset]+12*8(tp);"
        "ld a3,    %c[x0_offset]+13*8(tp);"
        "ld a4,    %c[x0_offset]+14*8(tp);"
        "ld a5,    %c[x0_offset]+15*8(tp);"
        "ld a6,    %c[x0_offset]+16*8(tp);"
        "ld a7,    %c[x0_offset]+17*8(tp);"

        /* Restore TP */
        "ld tp,    %c[x0_offset]+4*8(tp)  ;"
#if LIBOS_CONFIG_RISCV_S_MODE
        "sret;"
#else
        "mret;"
#endif
        :: [x0_offset] "i"(offsetof(libos_task_t, state.registers)),
        [ xcause_offset ] "i"(offsetof(libos_task_t, state.xcause)),
        [ xepc_offset ] "i"(offsetof(libos_task_t, state.xepc)));
    __builtin_unreachable();
}
