/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"

#include "libelf.h"
#include "dma.h"
#include "include/libos_init_daemon.h"
#include "include/libos.h"
#include "pagetable.h"
#include "memory.h"
#include "paging.h"
#include "libriscv.h"
#include "task.h"

LwU32 LIBOS_SECTION_DMEM_PINNED(pagetable_mask);
libos_pagetable_entry_t *LIBOS_SECTION_DMEM_PINNED(pagetable);

extern LwU64 cached_phdr_boot_virtual_to_physical_delta;
extern Task init_task_instance;

#define LIBOS_BOOTSTRAP_PTE_COUNT 128

LIBOS_SECTION_TEXT_COLD void kernel_bootstrap_pagetables(LibosElf64Header *elf, LwU64 elfPhysical)
{
    static libos_pagetable_entry_t bootstrap_pagetable[LIBOS_BOOTSTRAP_PTE_COUNT] = {0};
    LwU64 pasid                                                          = 1; //init_task_instance.pasid;
    LibosElf64ProgramHeader *phdrs = (LibosElf64ProgramHeader *)((LwU8 *)elf + elf->phoff);

    pagetable      = bootstrap_pagetable;
    pagetable_mask = LIBOS_BOOTSTRAP_PTE_COUNT - 1;

    for (LwU16 i = 0; i != elf->phnum; i++)
    {
        LibosElf64ProgramHeader *phdr = (LibosElf64ProgramHeader *)(((LwU8 *)phdrs) + i * elf->phentsize);

        // Skip sections not marked for init
        if (!(phdr->flags & LIBOS_PHDR_FLAG_PREMAP_AT_INIT))
            continue;

        // Populate the virtual address from the PHDR
        LwU64 virtual_address = phdr->vaddr;
        LwU64 attributes      = phdr->flags & LIBOS_MEMORY_ATTRIBUTE_KIND;
        if (phdr->flags & PF_X)
            attributes |= LIBOS_MEMORY_ATTRIBUTE_EXELWTE_MASK;
        else
            attributes |= LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK;

        LwU64 physical_address;
        if (phdr->type == LIBOS_PT_PHYSICAL)
        {
            physical_address = phdr->paddr;
        }

        else if (phdr->type == PT_LOAD)
        {
            // Ensure pre-loaded sections were fully allocated
            if (phdr->memsz != phdr->filesz)
                KernelPanic();

            // Normal sections inherit a direct mapping
            physical_address = elfPhysical + phdr->offset;
        }
        else
            KernelPanic();

        LwU64 size      = phdr->memsz;
        LwU64 step_size = virtual_address < LIBOS_PAGETABLE_MAX_ADDRESS_GRANULARITY_SMALL
                              ? LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL
                              : LIBOS_PAGETABLE_SEARCH_GRANULARITY_HUGE;
        LwU64 mapping_end = virtual_address + size;

        for (LwU64 pageva = virtual_address; pageva < mapping_end; pageva += step_size)
        {
            LwU64 hash               = murmur(pasid | pageva);
            libos_pagetable_entry_t candidate = {pageva | pasid, virtual_address, physical_address, attributes | size};
            LwU32 slot               = hash & pagetable_mask;
            LwU32 first_slot         = slot;

            while (LW_TRUE)
            {
                // Slot empty? Fill it.
                if (!pagetable[slot].key_virtual_address)
                {
                    pagetable[slot] = candidate;
                    break;
                }

                // Move to the next slot
                slot = (slot + 1) & pagetable_mask;

                // Pagetable full
                if (slot == first_slot)
                    KernelPanic();
            }
        }
    }
}

#if LIBOS_FEATURE_PAGING
void LIBOS_SECTION_TEXT_COLD
kernel_syscall_init_register_pagetables(LwU64 a0, LwU64 pagetable_base, LwU64 entries, LwU64 probe_count)
{
    // @note Init calls this function on startup *once*
    static LwBool initialized = LW_FALSE;
    if (initialized)
        KernelTaskPanic();
    initialized = LW_TRUE;

    // The pagetable is a physical pointer, colwert to a virtual pointer relative to the kernel data
    // section
    pagetable      = (libos_pagetable_entry_t *)(pagetable_base - cached_phdr_boot_virtual_to_physical_delta);
    pagetable_mask = entries - 1;

    KernelTaskReturn();
}
#endif

/*!
 *
 * @brief Translates a virtual-address to the containing memory descriptor.
 *
 *   If no memory descriptor lies under the address, a null memory descriptor
 *   is returned.
 *
 *   This operation typically results in at most one cache miss
 *
 * \param virtual_address
 *   Virtual address to search for containing region.
 */

//
//  @note: We mark the null descriptor as DMA as only DMA mappings are allowed to be
//         less than a multiple of a full page.
//
static libos_pagetable_entry_t null_descriptor = {0, 0, 0, LIBOS_MEMORY_KIND_DMA};


LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *kernel_resolve_address(Pasid pasid, LwU64 virtual_address)
{
    LibosAssert(buffer & (LIBOS_CONFIG_MPU_PAGESIZE - 1) == 0 && "Address must be paged aligned.");

    LwU64 masked_va;

    // 64-bit VA's must use 2**48 bit page sizes
    if (virtual_address < LIBOS_PAGETABLE_MAX_ADDRESS_GRANULARITY_SMALL)
        masked_va = virtual_address;
    else
        masked_va = virtual_address & ~(LIBOS_PAGETABLE_SEARCH_GRANULARITY_HUGE - 1);

    LwU64 bucket = murmur(pasid | masked_va) & pagetable_mask;

    while (LW_TRUE)
    {
        // Does the bucket match?
        if ((masked_va | pasid) == pagetable[bucket].key_virtual_address)
        {
            LwU64 offset = virtual_address - pagetable[bucket].virtual_address;
            
            if (offset >= (pagetable[bucket].attributes_and_size &~ LIBOS_MEMORY_ATTRIBUTE_MASK))
                return &null_descriptor;

            return &pagetable[bucket];
        }

        // Stop on an empty bucket
        if (!pagetable[bucket].virtual_address)
        {
            return &null_descriptor;
        }

        // Compute the next slot with wrap
        bucket = (bucket + 1) & pagetable_mask;
    }
}
