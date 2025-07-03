/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "../debug/elf.h"
#include "dev_falcon_v4.h"
#include "dev_falcon_v4_addendum.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "dev_riscv_pri.h"
#include "lw_gsp_riscv_address_map.h"
#include "extintr.h"
#include "gdma.h"
#include "interrupts.h"
#include "kernel.h"
#include "mmu.h"
#include "paging.h"
#include "profiler.h"
#include "riscv-isa.h"
#include "task.h"
#include "timer.h"
#include "trap.h"
#include "ilwerted_pagetable.h"

static void kernel_patch_phdrs(elf64_header *elf, LwU64 elfPhysical);

/*!
 * @file init.c
 * @brief This module is responsible for configuring and booting the kernel.
 *   Control comes from the bootstrap (@see kernel_bootstap called from @see libos_start)
 *
 *   This is the first code that exelwtes with the MPU enabled.
 */

__attribute__((aligned(32))) LwU8 LIBOS_SECTION_DMEM_PINNED(kernel_stack)[LIBOS_KERNEL_STACK_SIZE] = {0};

/*!
 * @brief This is the main kernel entrypoint.
 *
 * \param phdr_boot_virtual_to_physical_delta
 *   This is equivalent to kernel_va - kernel_pa.
 *   It is used by the paging subsystem to program later MPU entries.
 *
 * \return Does not return.
 */
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_init(
  libos_bootstrap_args_t *args,               // a0 [bootloader]
  LwS64 phdr_boot_virtual_to_physical_delta)  // a1 [from bootstrap.c]
{
    static LwBool first_boot = LW_TRUE;
    elf64_header *elf = (elf64_header *)LIBOS_PHDR_BOOT_VA;
    LwU64 elfPhysical = LIBOS_PHDR_BOOT_VA + phdr_boot_virtual_to_physical_delta;

    // Patch the ELF PHDRs for large va-support
    kernel_patch_phdrs(elf, elfPhysical);

#if LIBOS_CONFIG_GDMA_SUPPORT
    kernel_gdma_load();
#endif

    // Copy resident kernel into IMEM/DMEM
    kernel_mmu_load_kernel(args, phdr_boot_virtual_to_physical_delta);

    // Configure user-mode access to cycle and timer counters
    csr_set(
        LW_RISCV_CSR_XCOUNTEREN,
        REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_TM, 1u) | REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_CY, 1u));

    if (first_boot)
    {
        kernel_trap_init();

        kernel_profiler_init();

        kernel_task_init();

        kernel_timer_init();
    }

#if LIBOS_CONFIG_TCM_PAGING
    kernel_paging_load(phdr_boot_virtual_to_physical_delta);
#endif

    kernel_mmu_load();

    // Load trap handler.  Enabling interrupts must be done after trap handler.
    kernel_trap_load();

    kernel_timer_load();

    kernel_external_interrupt_load();

    // Prime the MPU with all the cached mappings for the init task
    //
    // The init task is responsible for setting up and maintaining the
    // ilwerted page table as well as loading images.
    //
    if (first_boot)
    {
        kernel_bootstrap_pagetables(elf, elfPhysical);
        first_boot = LW_FALSE;

        // Init task entry point takes the elf physical address as an argument
        resume_task->state.registers[RISCV_A0] = elfPhysical;
        resume_task->state.registers[RISCV_A1] = LW_RISCV_AMAP_FBGPA_START;
        resume_task->state.registers[RISCV_A2] = LW_RISCV_AMAP_PRIV_START;
        resume_task->state.registers[RISCV_A3] = LW_RISCV_AMAP_SYSGPA_START;
    }
    else
    {
        // Remap the pagetables previously registered on the first boot
        kernel_remap_pagetables();
    }

    csr_write(LW_RISCV_CSR_XSTATUS, user_xstatus);

    // First boot: Start the init task,
    // Second boot: Return to the task that initiated libosSuspendProcessor
    kernel_raw_return_to_task(resume_task);
}

extern libos_phdr_patch_t phdr_patch_va_begin[];
LIBOS_DECLARE_LINKER_SYMBOL_AS_INTEGER(phdr_patch_va_count);
LIBOS_REFERENCE_LINKER_SYMBOL_AS_INTEGER(phdr_patch_va_count);

#if LIBOS_CONFIG_ENABLE_APERTURE_PATCHING_HACK
// Patch the physical address and virtual address of aperture mappings
// @todo: Remove the hack when per-family HALs are ready
static LIBOS_SECTION_TEXT_COLD void kernel_patch_aperture(elf64_header *elf, LwU64 paOld, LwU64 paNew)
{
    elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);
    const LwU64 paDiff = paOld ^ paNew;
    for (LwU16 i = 0; i < elf->phnum; i++)
    {
        elf64_phdr *phdr = (elf64_phdr *)(((LwU8 *)phdrs) + i * elf->phentsize);
        if ((phdr->paddr & 0xffff000000000000ull) == paOld)
        {
            if (phdr->vaddr == phdr->paddr)
                phdr->vaddr ^= paDiff;
            phdr->paddr ^= paDiff;
        }
    }
}
#endif // LIBOS_CONFIG_ENABLE_APERTURE_PATCHING_HACK

/*
    Thoughts
        - Need multiple pte's for each region vs. single MPU entry
        - Desire init process to use DMA after init for task reset
        - Desire init process to not direct use kernel DMA engines at bootstrap :(

    Physical DMA:
        - Use in physical mode? How do we know what IMEM/DMEM is available?
          Could have init process:
            - Disable scheduler
            - DMA kernel into IMEM/DMEM
            - Register IMEM and DMEM masks to kernel during PTE init.
        - What does physical mode look like?

    Deferred DMA:
        - Use DMA registers in the init process before "starting up"
          the DMA subsystem.  Makes a certain sense as pagetables are missing.
        - Pre-populate works for DMEM pages (!)

    Bootstrapped pagetables:
        - Create bootstrapped PTE's for init process
        - Needs many PTEs to cover the paged buffer
*/

static LIBOS_SECTION_TEXT_COLD void kernel_patch_phdrs(elf64_header *elf, LwU64 elfPhysical)
{
    elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);

    // Apply phdr_patch_va dataset to PHDRs
    // @note: This is done for memory regions declared as FIXED_VA.
    //        We ensure the patched PHDRs have no relocations (size = 0)
    for (LwU64 patch_index = 0; patch_index != phdr_patch_va_count; patch_index++)
    {
        elf64_phdr *phdr =
            (elf64_phdr *)(((LwU8 *)phdrs) + phdr_patch_va_begin[patch_index].index * elf->phentsize);
        phdr->vaddr = phdr_patch_va_begin[patch_index].va;
        phdr->memsz = phdr_patch_va_begin[patch_index].size;
    }

#if LIBOS_CONFIG_ENABLE_APERTURE_PATCHING_HACK
    // @todo: Remove the hack when per-family HALs are ready
    kernel_patch_aperture(elf, 0x6180000000000000ull, LW_RISCV_AMAP_FBGPA_START);
    kernel_patch_aperture(elf, 0x8180000000000000ull, LW_RISCV_AMAP_SYSGPA_START);
#endif // LIBOS_CONFIG_ENABLE_APERTURE_PATCHING_HACK
}
