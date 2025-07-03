/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel_copy.h"
#include "../include/libos.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "kernel.h"
#include "libos_defines.h"
#include "libos_syscalls.h"
#include "memory.h"
#include "mmu.h"
#include "paging.h"
#include "ilwerted_pagetable.h"
#include "riscv-isa.h"
#include "task.h"
#include <sdk/lwpu/inc/lwtypes.h>

extern LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN LIBOS_NAKED void
kernel_port_copy_and_continue_load(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, libos_task_t *copy_context);
extern LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN LIBOS_NAKED void
kernel_port_copy_and_continue_store(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, libos_task_t *copy_context);

/*
    @note Performs a copy in the kernel

        Page faults will occur during the copy while in the kernel.
        These are handled through a coninuation.

        As a result: This function does not return and calls kernel_continuation.
*/
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN LIBOS_NAKED void kernel_port_copy_and_continue(
    LwU64 destination, // a0
    LwU64 source,      // a1
    LwU64 size)        // a2
{
    __asm__ __volatile__(
        // Save thread pointer so that kernel_trap_in_kernel
        // can restore it after a page fault
        "mv a4, tp;"

        ".loop:;"
        "beq a2, x0, .end_loop;"

        "kernel_port_copy_and_continue_load:"
        "lb a3, 0(a1);"

        "kernel_port_copy_and_continue_store:"
        "sb a3, 0(a0);"
        "addi a0, a0, 1;"
        "addi a1, a1, 1;"
        "addi a2, a2, -1;"
        "j .loop;"
        ".end_loop:;"
        "j kernel_continuation;"
        :
        :
        [ stack_size ] "i"(LIBOS_KERNEL_STACK_SIZE), [ xepc_offset ] "i"(offsetof(libos_task_t, state.xepc)),
        [ ioctl_sendrecv_status_a0 ] "i"(offsetof(libos_task_t, state.registers[LIBOS_REG_IOCTL_STATUS_A0])),
        [ reg_a0 ] "i"(offsetof(libos_task_t, state.registers[RISCV_A0])),
        [ reg_a1 ] "i"(offsetof(libos_task_t, state.registers[RISCV_A1])),
        [ reg_t1 ] "i"(offsetof(libos_task_t, state.registers[RISCV_T1]))
        : "memory", "t0", "t1", "a0", "a1", "a2", "a3");
}

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN
kernel_handle_copy_fault(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, libos_task_t *copy_context)
{
    LwU64 xepc = csr_read(LW_RISCV_CSR_XEPC);
    LwU64 xcause = csr_read(LW_RISCV_CSR_XCAUSE);

    // @todo: MEMERR containment should report the port copy failure
    //        to userspace.
#if LWRISCV_VERSION < LWRISCV_2_0
    if (xcause != LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT &&
        xcause != LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT)
        panic_kernel();
#else
    if (xcause != LW_RISCV_CSR_XCAUSE_EXCODE_LPAGE_FAULT &&
        xcause != LW_RISCV_CSR_XCAUSE_EXCODE_SPAGE_FAULT)
        panic_kernel();
#endif

    LwBool load = (xepc == (LwU64)kernel_port_copy_and_continue_load);
    LwBool store = (xepc == (LwU64)kernel_port_copy_and_continue_store);

    if (load || store)
    {
        LwU64 addr_page;
        libos_pagetable_entry_t *memory;

        self = copy_context;

        if (load)
        {
            addr_page = mpu_page_addr(a1);
            memory = kernel_resolve_address(self->kernel_read_pasid, addr_page);
        }
        else
        {
            addr_page = mpu_page_addr(a0);
            memory = kernel_resolve_address(self->kernel_write_pasid, addr_page);
        }

        if (kernel_mmu_page_in(memory, addr_page, xcause, LW_FALSE) != LIBOS_ILWALID_TCM_OFFSET)
        {
            if (load)
                kernel_port_copy_and_continue_load(a0, a1, a2, a3, copy_context);
            else
                kernel_port_copy_and_continue_store(a0, a1, a2, a3, copy_context);
        }
    }

    panic_kernel();    
}
