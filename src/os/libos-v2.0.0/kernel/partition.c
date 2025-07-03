/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "gdma.h"
#include "partition.h"
#include "mmu.h"
#include "suspend.h"

#if !LIBOS_CONFIG_GDMA_SUPPORT
#error The partition TCM save/restore implementation requires GDMA support
#endif

#define LIBOS_MAX_PARTITION_BOOTSTRAP_IMEM_SIZE 4096U
#define LIBOS_MAX_PARTITION_BOOTSTRAP_DMEM_SIZE 256U

static struct {
    LwU64 imem_base_pa;
    LwU64 dmem_base_pa;
    LwU32 imem_size;
    LwU32 dmem_size;
    LwU8 imem[LIBOS_MAX_PARTITION_BOOTSTRAP_IMEM_SIZE];
    LwU8 dmem[LIBOS_MAX_PARTITION_BOOTSTRAP_DMEM_SIZE];
} bootstrap_tcm;

void LIBOS_SECTION_TEXT_COLD kernel_partition_save_bootstrap_tcm(
    const libos_bootstrap_args_t *boot_args)
{
    bootstrap_tcm.imem_base_pa = boot_args->imem_base_pa;
    bootstrap_tcm.imem_size    = boot_args->imem_bs_size;
    bootstrap_tcm.dmem_base_pa = boot_args->dmem_base_pa;
    bootstrap_tcm.dmem_size    = boot_args->dmem_bs_size;

    // Make sure we can fit in the backing buffer
    if ((bootstrap_tcm.dmem_size > LIBOS_MAX_PARTITION_BOOTSTRAP_DMEM_SIZE) ||
        (bootstrap_tcm.imem_size > LIBOS_MAX_PARTITION_BOOTSTRAP_IMEM_SIZE))
    {
        panic_kernel();
    }

    // Assume we have at least two GDMA channels and they can both be used
    kernel_gdma_xfer_reg(bootstrap_tcm.imem_base_pa,
                         cached_phdr_boot_virtual_to_physical_delta + (LwU64)bootstrap_tcm.imem,
                         bootstrap_tcm.imem_size, 0, 0);
    kernel_gdma_xfer_reg(bootstrap_tcm.dmem_base_pa,
                         cached_phdr_boot_virtual_to_physical_delta + (LwU64)bootstrap_tcm.dmem,
                         bootstrap_tcm.dmem_size, 1, 0);
    kernel_gdma_flush();
}

void LIBOS_SECTION_TEXT_COLD kernel_partition_restore_bootstrap_tcm(void)
{
    // Assume we have at least two GDMA channels and they can both be used
    kernel_gdma_xfer_reg(cached_phdr_boot_virtual_to_physical_delta + (LwU64)bootstrap_tcm.imem,
                         bootstrap_tcm.imem_base_pa,
                         bootstrap_tcm.imem_size, 0, 0);
    kernel_gdma_xfer_reg(cached_phdr_boot_virtual_to_physical_delta + (LwU64)bootstrap_tcm.dmem,
                         bootstrap_tcm.dmem_base_pa,
                         bootstrap_tcm.dmem_size, 1, 0);
    kernel_gdma_flush();
}

static LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void sbi_partition_switch(
    LwU64 arg0,
    LwU64 arg1,
    LwU64 arg2,
    LwU64 arg3,
    LwU64 from_partition,
    LwU64 to_partition
)
{
    register LwU64 sbi_extension_id __asm__("a7") = 0x090001EB; // LWPU vendor-specific SBI extension
    register LwU64 sbi_function_id  __asm__("a6") = 0;
    register LwU64 a5 __asm__( "a5" ) = to_partition;
    register LwU64 a4 __asm__( "a4" ) = from_partition;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a0 __asm__( "a0" ) = arg0;

    __asm__ volatile ("ecall"
                  : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4), "+r"(a5), "+r"(sbi_function_id), "+r"(sbi_extension_id)
                  :
                  : "ra", "s0", "s1", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    __builtin_unreachable();
}

void LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD kernel_syscall_partition_switch(
    LwU64 arg0,
    LwU64 arg1,
    LwU64 arg2,
    LwU64 arg3,
    LwU64 from_partition,
    LwU64 to_partition
)
{
    kernel_prepare_for_suspend();
    sbi_partition_switch(arg0, arg1, arg2, arg3, from_partition, to_partition);
}
