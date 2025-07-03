/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include "kernel/libos_defines.h"

#include "debug/elf.h"
#include "include/libos_assert.h"
#include "include/libos_elf_acl.h"
#include "include/libos_ilwerted_pagetable.h"
#include "libos_init_daemon.h"

#include "libos.h"
#include "task_init.h"
#include "libos_log.h"

#ifdef GSPRM_ASAN_ENABLE
#include "libos_asan.h"

typedef struct {
    LwU64 va_start;
    LwU64 va_end;
    libos_pasid_t pasid;
} shadow_range;

static shadow_range *shadow_ranges;
static LwU64 shadow_ranges_n;
#endif  // GSPRM_ASAN_ENABLE

//
//  @todo Document that PTE's are programmed in lib_init_pagetables_initialize
//        and while entries can be modified after that point, they cannot
//        be removed or added.  This allows a staticly known probe-count
//        and guarantees deterministic and bounded PTE lookup post-init.
//
// Robin hash:
//  https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/

static libos_phdr_acl_t *lib_init_phdr_acl(const elf64_header *elf);

LwU64 heap_physical_base = 0;
LwU64 heap_physical_size = 0;

extern LwU64 *log_init_buffer_page0;
extern LwU64 *log_init_buffer_lwr_page;
extern LwU64 log_init_buffer_size;
extern LwU64 log_init_pte_count;
extern LwU64 log_init_buffer_pagetable[LOG_INIT_MAX_PTE];

/**
 * @brief Pre-maps the init log
 *
 * This relies on the requires TASK_INIT_FB identity mapping
 * in order to function.  The users will later call construct
 * log to wire in the final VA for the log, allowing TASK_INIT_FB
 * to be disabled at runtime.
 */
void lib_init_premap_log(LibosMemoryRegionInitArgument *init_args, LwU64 sysGpaStartAddr)
{
    // Early initialize the log to point to the TASK_INIT_FB aperture
    // This will be swapped out for the restricted log mapping later.
    if (init_args && init_args->id8 == LOG_INIT_BUFFER_ID8)
    {
        LwU64 size = init_args->size;
        if (size > LOG_INIT_MAX_PTE * 0x1000)
        {
            // Just make it work.  Init arg validation prints an error later.
            size = LOG_INIT_MAX_PTE * 0x1000;
        }
        log_init_buffer_page0 = log_init_buffer_lwr_page = (LwU64 *)(init_args->pa + sysGpaStartAddr);
        log_init_buffer_size                             = size / sizeof(LwU64);

        /* Create a page table when needed. */
        if (LOG_INIT_ALLOW_LARGE_BUFFER && (size > 0x1000))
        {
            LwU64 i;
            log_init_pte_count = (size + 0xFFF)/ 0x1000;
            for (i = 0; i < log_init_pte_count; i++)
                log_init_buffer_pagetable[i] = log_init_buffer_page0[i + 1] + sysGpaStartAddr;
        }
    }
}

/**
 * @brief lib_init_map_log
 *
 * Special version of construct_log_init to use after lib_init_premap_log.
 * The logging structure was already initialized using the identity mapping
 * in lib_init_premap_log.  Adjust it to use the restricted log mapping.
 */
void lib_init_map_log(void)
{
    log_init_buffer_page0 = log_init_buffer_lwr_page = (LwU64 *)log_init_buffer_instance;

    if (log_init_pte_count > 0)
    {
        LwU64 offset = (LwU64)log_init_buffer_page0 - log_init_buffer_pagetable[0];
        LwU64 i;
        for (i = 0; i < log_init_pte_count; i++)
            log_init_buffer_pagetable[i] += offset;
    }
}

/**
 * @brief Allocates a block of memory from the init-heap.
 *
 * This heap is used to back any BSS or copy-on-load data sections,
 * as well as page tables, and may be made available for other tasks
 * through a port-based allocation API.
 *
 * @param elf - target ELF binary
 *
 * @return Required PTE entries in ilwerted pagetable
 */
void *lib_init_heap_alloc(LwU64 length)
{
    // heap_physical_base starts naturally aligned, and all
    // lengths are rounded up to ensure alignment is maintained
    length = (length + (LIBOS_MPU_PAGESIZE - 1)) & LIBOS_MPU_PAGEMASK;

    // Check for sufficient free space
    if (length > heap_physical_size)
        return 0;

    heap_physical_size -= length;

    return (void *)(heap_physical_base + heap_physical_size);
}

/**
 * @brief Returns the free space in the initial heap
 *
 */
LwU64 lib_init_heap_get_free(void) { return heap_physical_size; }

static LwU64 lib_init_mapping_granularity_for_va(LwU64 virtual_address)
{
    //
    // Which granularity are we using for this search?
    // This will determine how many PTE's are used to cover the allocation
    //
    if (virtual_address < LIBOS_PAGETABLE_MAX_ADDRESS_GRANULARITY_SMALL)
        return LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL;
    else
        return LIBOS_PAGETABLE_SEARCH_GRANULARITY_HUGE;
}

/**
 * @brief Callwlates the pagetable size required for all ELF PHDRs
 *
 * @param elf - target ELF binary
 *
 * @return Required PTE entries in ilwerted pagetable
 */
static LwU64 lib_init_pagetable_entry_count(const elf64_header *elf)
{
    LwU64 total_entries = 0;

    // Look at the main ELF header
    elf64_phdr *phdrs           = (elf64_phdr *)((LwU8 *)elf + elf->phoff);

    for (LwU64 i = 0; i < elf->phnum; i++)
    {
        elf64_phdr *phdr = (elf64_phdr *)((LwU8 *)phdrs + elf->phentsize * i);

        LwU64 size        = phdr->memsz;
        LwU64 granularity = lib_init_mapping_granularity_for_va(phdr->vaddr);
        LwU64 entries     = (size + granularity - 1) / granularity;

        total_entries += entries;
    }
    return total_entries;
}

/**
 * @brief Accesses the LIBOS access control table for the PHDRs in this ELF image
 *
 * @param elf - target ELF binary
 */
static libos_phdr_acl_t *lib_init_phdr_acl(const elf64_header *elf)
{
    elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);

    LIBOS_VALIDATE(LIBOS_MANIFEST_PHDR_ACLS < elf->phnum)

    elf64_phdr *i = (elf64_phdr *)((LwU8 *)phdrs + elf->phentsize * LIBOS_MANIFEST_PHDR_ACLS);

    LIBOS_VALIDATE(i->type == LIBOS_PT_MANIFEST_ACLS);

    return (libos_phdr_acl_t *)((LwU8 *)elf + i->offset);
}

/**
 * @brief Accesses the LIBOS access control table for the init argument manifest
 *
 * @param elf - target ELF binary
 */
libos_manifest_init_arg_memory_region_t *
lib_init_phdr_init_arg_memory_regions(const elf64_header *elf, LwU64 *count)
{
    elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);

    LIBOS_VALIDATE(LIBOS_MANIFEST_INIT_ARG_MEMORY_REGION < elf->phnum)

    elf64_phdr *i = (elf64_phdr *)((LwU8 *)phdrs + elf->phentsize * LIBOS_MANIFEST_INIT_ARG_MEMORY_REGION);

    LIBOS_VALIDATE(i->type == LIBOS_PT_MANIFEST_INIT_ARG_MEMORY_REGION);

    *count = i->filesz / sizeof(libos_manifest_init_arg_memory_region_t);

    return (libos_manifest_init_arg_memory_region_t *)((LwU8 *)elf + i->offset);
}

static libos_pagetable_entry_t *pagetable      = 0;
static LwU64           pagetable_mask = 0;

/**
 * @brief Creates the pagetable entries for the underlying region.
 *        Kernel panic will occur if the VA has already been mapped for that pasid.
 *
 *        In the future, this could be replaced by a set routine to allow remapping
 *        or unmapping.  This would require tombstone value support and some logic
 *        to stop at the max-displacement.
 *
 *
 * @param pasid                 Process address space identifier for mapping
 * @param virtual_address       Start VA of the mapping. Addresses below 4G must be 4k aligned,
 * above must be 2**48 bytes aligned
 * @param size                  Size of mapping - must be 4kb aligned.
 * @param attributes            Page attributes mask (@see LIBOS_MEMORY_ATTRIBUTE* /
 * LIBOS_MEMORY_KIND*)
 * @param physical_address      Target physical address in either FB, SYSMEM, IMEM, or DMEM \in
 * format accepted by MPU.
 *
 * @return Does not return.
 */
void lib_init_pagetable_insert(
    libos_pasid_t pasid, LwU64 virtual_address, LwU64 size, LwU64 attributes, LwU64 physical_address)
{
    if (size == 0)
        return;

    //
    // Which granularity are we using for this search?
    // This will determine how many PTE's are used to cover the allocation
    //
    LwU64 step_size = lib_init_mapping_granularity_for_va(virtual_address);

    // Ensure the size is at least 4kb granular
    LIBOS_VALIDATE((size & (LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL - 1)) == 0);

    // Ensure the physical address is 4kb granular
    LIBOS_VALIDATE((physical_address & (LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL - 1)) == 0);

    // Verify that the virtual_address of the allocation starts on one of the steps
    LIBOS_VALIDATE((virtual_address & (step_size - 1)) == 0);

    LwU64 mapping_end = virtual_address + size;

    for (LwU64 pageva = virtual_address; pageva < mapping_end; pageva += step_size)
    {
        LwU64          hash      = murmur(pasid | pageva);
        libos_pagetable_entry_t candidate = {pageva | pasid, virtual_address, physical_address, attributes | size};
        LwU64          candidate_displacement = 0;
        LwU32          slot                   = hash & pagetable_mask;

        // @bug: We should be checking the displacement distance.  Technically this isn't reachable,
        //       however we're
        while (LW_TRUE)
        {
            // Slot empty? Fill it.
            if (!pagetable[slot].key_virtual_address)
            {
                pagetable[slot] = candidate;
                break;
            }

            // Ensure the element wasn't already present
            // @todo We could change this to a set routine if we search first before attempting
            // insert
            LIBOS_VALIDATE(pagetable[slot].virtual_address != (pageva | pasid));

            // How far is the item below us from its ideal slot?
            LwU64 hash = murmur(pagetable[slot].virtual_address);
            LwU64 displacement;

            if (hash < slot)
                displacement = slot - hash;
            else
                displacement = hash - slot;

            // Robin Hood: If the item under the cursor is "closer" than the item
            //             we're holding -- swap.
            if (displacement < candidate_displacement)
            {
                libos_pagetable_entry_t temp    = candidate;
                candidate              = pagetable[slot];
                candidate_displacement = displacement;
                pagetable[slot]        = temp;
            }

            // Update the displacement
            candidate_displacement++;

            // Move to the next slot
            slot = (slot + 1) & pagetable_mask;
        }
    }
}

static void lib_init_pagetable_populate_phdr(LwU64 elfPhysical, elf64_phdr *phdr, libos_phdr_acl_t *acl)
{
    // Populate the virtual address from the PHDR
    LwU64 virtual_address = phdr->vaddr;
    LwU64 attributes      = phdr->flags & LIBOS_MEMORY_ATTRIBUTE_KIND;
    if (phdr->flags & PF_X)
        attributes |= LIBOS_MEMORY_ATTRIBUTE_EXELWTE_MASK;

    LwU64 physical_address;
    if (phdr->type == LIBOS_PT_PHYSICAL)
    {
        physical_address = phdr->paddr;
#if 0
        LOG_INIT(
            LOG_LEVEL_INFO, "    LIBOS_PT_PHYSICAL VA=%016x memsz=%016x filesz=%08x PA=%016x\n", phdr->vaddr,
            phdr->memsz, phdr->filesz, physical_address);
#endif
    }

    else if (phdr->type == PT_LOAD)
    {
        if (phdr->memsz != phdr->filesz)
        {
            physical_address = (LwU64)lib_init_heap_alloc(phdr->memsz);

            LIBOS_VALIDATE(physical_address);
#if 0
            LOG_INIT(
                LOG_LEVEL_INFO, "    PT_LOAD           VA=%016x memsz=%016x filesz=%08x PA=%016x\n",
                phdr->vaddr, phdr->memsz, phdr->filesz, physical_address);
#endif

            if (phdr->filesz > 0)
            {
                // VA == PA, so we can use elfPhysical directly
                LIBOS_VALIDATE(
                    libosDmaCopy(phdr->vaddr, elfPhysical + phdr->offset, phdr->filesz) ==
                    LIBOS_OK);

                libosDmaFlush();
            }

            // Fill staging buffer with zeroes
            for (LwU64 i = 0; i < 4096; i += 8)
                *(LwU64 *)(TASK_INIT_DMEM_64KB + i) = 0;

            // Write zeroes
            for (LwU32 offset = 0; offset < phdr->filesz; offset += 4096)
            {
                LIBOS_VALIDATE(
                    libosDmaCopy(virtual_address + offset, TASK_INIT_DMEM_64KB, 4096) ==
                    LIBOS_OK);
            }
        }
        else
        {
            // Normal sections inherit a direct mapping
            physical_address = elfPhysical + phdr->offset;
#if 0
            LOG_INIT(
                LOG_LEVEL_INFO, "    PT_LOAD           VA=%016x memsz=%016x filesz=%08x PA=%016x\n",
                phdr->vaddr, phdr->memsz, phdr->filesz, physical_address);
#endif
        }
    }
    else
    {
#if 0
        LOG_INIT(
            LOG_LEVEL_INFO, "    NO_LOAD           VA=%016x memsz=%016x filesz=%08x PA=%016x\n", phdr->vaddr,
            phdr->memsz, phdr->filesz, elfPhysical + phdr->offset);
#endif
        return;
    }

    // @note Walk security table to determine which address spaces it is visible in
    for (LwU16 i = 0; i != acl_read_only(acl)->count; i++)
    {
        lib_init_pagetable_insert(
            acl_read_only(acl)->pasid[i], virtual_address, phdr->memsz, attributes, physical_address);

#ifdef GSPRM_ASAN_ENABLE
        if (phdr->type == PT_LOAD && !(phdr->flags & PF_X))
            lib_init_shadow_ranges_register(acl_read_only(acl)->pasid[i], virtual_address, phdr->memsz);
#endif  // GSPRM_ASAN_ENABLE
    }

    for (LwU16 i = 0; i != acl_read_write(acl)->count; i++)
    {
        lib_init_pagetable_insert(
            acl_read_write(acl)->pasid[i], virtual_address, phdr->memsz,
            attributes | LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK, physical_address);

#ifdef GSPRM_ASAN_ENABLE
        if (phdr->type == PT_LOAD && !(phdr->flags & PF_X))
            lib_init_shadow_ranges_register(acl_read_write(acl)->pasid[i], virtual_address, phdr->memsz);
#endif  // GSPRM_ASAN_ENABLE
    }
}

void lib_init_pagetable_initialize(elf64_header *elf, LwU64 elfPhysical)
{
    LOG_INIT(LOG_LEVEL_INFO, "LIBOS Loading...\n");
    LOG_INIT(LOG_LEVEL_INFO, "   ELF @ %016x\n", elfPhysical);

    // Look at the main ELF header
    elf64_phdr *phdrs     = (elf64_phdr *)((LwU8 *)elf + elf->phoff);

    // Find the free memory available for copied section allocation
    LwU64 *header = (LwU64 *)(elfPhysical - 16);

    // Check for heap header after final ELF image
    if (header[0] != LIBOS_MAGIC)
    {
        LOG_INIT(LOG_LEVEL_INFO, "Loader expected to place LIBOS_MAGIC/heap-size after final elf.\n");
        libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
    }

    // Read header
    heap_physical_size = header[1];
    heap_physical_base = elfPhysical - heap_physical_size;
    LOG_INIT(LOG_LEVEL_INFO, "    Heap: base 0x%llx 0x%llx bytes\n", heap_physical_base, heap_physical_size);

    // Estimate PTE size
    LwU64 pte_entries = lib_init_pagetable_entry_count(elf);

#ifdef GSPRM_ASAN_ENABLE
    // ASAN worst case number of entries is one shadow entry for every "real" entry
    pte_entries *= 2;  // worst case is double (very conservative)
#endif  // GSPRM_ASAN_ENABLE

    // Round the size up to the nearest power of 2 (required for lookup)
    LwU64 entries = 1;
    while (entries < pte_entries)
        entries *= 2;

    // Ensure the load factor is at most 66%
    if ((pte_entries * 3 / 2) > entries)
        entries *= 2;

    // Validate the memory layout
    if (entries * sizeof(libos_pagetable_entry_t) > LIBOS_MAX_PAGETABLE_SIZE)
    {
        LOG_INIT(LOG_LEVEL_INFO, "LIBOS: Insufficient kernel address space to map pagetables.\n");
        libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
    }

    // Allocate the pagetable
    pagetable      = lib_init_heap_alloc(entries * sizeof(libos_pagetable_entry_t));
    pagetable_mask = entries - 1;
    if (!pagetable)
    {
        LOG_INIT(
            LOG_LEVEL_INFO, "Out of memory, unable to allocate memory for pagetables (reqd: %d bytes)\n",
            entries * sizeof(libos_pagetable_entry_t));
        libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
    }
    LOG_INIT(
        LOG_LEVEL_INFO, "    Pagetables allocated 0x%08X length %d bytes\n", pagetable,
        entries * sizeof(libos_pagetable_entry_t));

    // Initialize the pagetable to an empty state
    for (LwU64 i = 0; i < entries; i++)
        pagetable[i].key_virtual_address = 0;

    // @todo We need the kernel to extend the mapping of data to include this section
    //       We also need to ensure the kernel code section came first and thus does NOT overlap

    // How do we know the size of this section?
    //  Option 1. The init message?
    //  Option 2. Terrible - require the header to size it.
    //  Core question, how would the open source folks know the required minimum size?

    libos_phdr_acl_t *acl_table = lib_init_phdr_acl(elf);

#ifdef GSPRM_ASAN_ENABLE
    shadow_ranges_n = elf->phnum;
    shadow_ranges = lib_init_heap_alloc(shadow_ranges_n * sizeof(shadow_range));
    if (!shadow_ranges)
    {
        LOG_INIT(
            LOG_LEVEL_INFO, "Out of memory, unable to allocate memory for shadow ranges metadata (reqd: %d bytes)\n",
            shadow_ranges_n * sizeof(shadow_range));
        libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
    }
    LOG_INIT(
        LOG_LEVEL_INFO, "    Shadow ranges metadata allocated 0x%08X length %d bytes\n", shadow_ranges,
        shadow_ranges_n * sizeof(shadow_range));

    for (LwU64 i = 0; i < shadow_ranges_n; i++)
    {
        shadow_ranges[i].va_start = 0;
        shadow_ranges[i].va_end = 0;
    }
#endif  // GSPRM_ASAN_ENABLE

    // lib_init_pagetable_populate_phdr needs a registered DMA buffer
    LIBOS_VALIDATE(libosDmaRegisterTcm(TASK_INIT_DMEM_64KB, 4096) == LIBOS_OK);

    for (LwU64 i = 0; i < elf->phnum; i++)
    {
        elf64_phdr *phdr = (elf64_phdr *)((LwU8 *)phdrs + elf->phentsize * i);
        lib_init_pagetable_populate_phdr(elfPhysical, phdr, &acl_table[i]);
    }

    // Flush all previous DMA's before the pages become available
    libosDmaFlush();

    // Cache flush in case we're virtually tagged
    //libosTaskIlwalidateData(pagetable, entries*sizeof(libos_memory_t));

    // @todo probe count
    libosInitRegisterPagetables(pagetable, entries, entries);

    LOG_INIT(LOG_LEVEL_INFO, "    Software MMU enabled.\n\n");
}

void lib_init_pagetable_map(elf64_header *elf, LwU32 phdr_index, LwU64 physical_address, LwU64 physical_size)
{
    elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);
    elf64_phdr *phdr  = (elf64_phdr *)((LwU8 *)phdrs + elf->phentsize * phdr_index);

#if 0
    LOG_INIT(
        LOG_LEVEL_INFO,
        "    lib_init_pagetable_map(elf:%016x [%016x], phdr:%d [%016x], PA=%016x, size=%016x) %016x %016x\n",
        elf, *(LwU64 *)elf, phdr_index, phdr, physical_address, physical_size, phdr->memsz, phdr->vaddr);
#endif

    // Ensure that the passed buffer isn't larger than our VA reservation
    if (physical_size > phdr->memsz)
    {
        LOG_INIT(LOG_LEVEL_INFO, "        ERROR: init argument larger than region\n");
        return;
    }

    LwU64 attributes = phdr->flags & LIBOS_MEMORY_ATTRIBUTE_KIND;
    if (phdr->flags & PF_X)
        attributes |= LIBOS_MEMORY_ATTRIBUTE_EXELWTE_MASK;

    libos_phdr_acl_t *acl = lib_init_phdr_acl(elf) + phdr_index;

    for (LwU16 i = 0; i != acl_read_only(acl)->count; i++)
    {
        lib_init_pagetable_insert(
            acl_read_only(acl)->pasid[i], phdr->vaddr, physical_size, attributes, physical_address);
#ifdef GSPRM_ASAN_ENABLE
        lib_init_shadow_ranges_register(acl_read_only(acl)->pasid[i], phdr->vaddr, physical_size);
#endif  // GSPRM_ASAN_ENABLE
    }

    for (LwU16 i = 0; i != acl_read_write(acl)->count; i++)
    {
        lib_init_pagetable_insert(
            acl_read_write(acl)->pasid[i], phdr->vaddr, physical_size,
            attributes | LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK, physical_address);
#ifdef GSPRM_ASAN_ENABLE
        lib_init_shadow_ranges_register(acl_read_write(acl)->pasid[i], phdr->vaddr, physical_size);
#endif  // GSPRM_ASAN_ENABLE
    }
}

#ifdef GSPRM_ASAN_ENABLE
LwU64 lib_init_shadow_ranges_get_size_estimate(libos_pasid_t pasid)
{
    LwU64 estimate = 0;

    for (LwU64 i = 0; i < shadow_ranges_n; i++)
    {
        if (shadow_ranges[i].va_start == 0 && shadow_ranges[i].va_end == 0)
            continue;

        if (shadow_ranges[i].pasid != pasid)
            continue;

        estimate += shadow_ranges[i].va_end - shadow_ranges[i].va_start;
    }

    return estimate;
}

void lib_init_shadow_ranges_register_phdr(libos_pasid_t pasid, elf64_header *elf, LwU32 phdr_index, LwU64 size)
{
    elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);
    elf64_phdr *phdr  = (elf64_phdr *)((LwU8 *)phdrs + elf->phentsize * phdr_index);
    lib_init_shadow_ranges_register(pasid, phdr->vaddr, size);
}

void lib_init_shadow_ranges_register(libos_pasid_t pasid, LwU64 virtual_address, LwU64 memsz)
{
    // No shadow necessary for zero-sized regions
    if (memsz == 0) {
        return;
    }

    LIBOS_VALIDATE(shadow_ranges);
    LIBOS_VALIDATE(shadow_ranges_n > 0);

    // Compute corresponding shadow
    LwU64 shadow_size = ((LwU64) memsz + (LIBOS_ASAN_SHADOW_GRAN - 1)) / LIBOS_ASAN_SHADOW_GRAN;
    LwU64 va_start = (LwU64) LIBOS_ASAN_MEM_TO_SHADOW(virtual_address);
    LwU64 va_end = va_start + shadow_size;

    // Align shadow VAs to required alignment for pagetables
    va_start = va_start & ~(lib_init_mapping_granularity_for_va(va_start) - 1);
    va_end = (va_end + (LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL - 1)) & ~(LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL - 1);

    // Record shadow range required for future mapping and allocation
    shadow_range *lwr = 0;
    shadow_range *empty = 0;
    shadow_range *cand_for_start = 0;
    shadow_range *cand_for_end = 0;
    for (LwU64 i = 0; i < shadow_ranges_n; i++)
    {
        lwr = &shadow_ranges[i];
        if (lwr->va_start == 0 && lwr->va_end == 0)
        {
            if (!empty)
                empty = lwr;

            continue;
        }

        if (pasid == lwr->pasid && va_start >= lwr->va_start && va_start < lwr->va_end)
        {
            if (cand_for_start)
            {
                LOG_INIT(LOG_LEVEL_INFO,
                         "More than one matching shadow range candidate (cand_for_start) for pasid %d non-shadow VA=%016x\n",
                         pasid, virtual_address);
                libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
            }
            cand_for_start = lwr;
        }

        if (pasid == lwr->pasid && va_end > lwr->va_start && va_end <= lwr->va_end)
        {
            if (cand_for_end)
            {
                LOG_INIT(LOG_LEVEL_INFO,
                         "More than one matching shadow range candidate (cand_for_end) for pasid %d non-shadow VA=%016x\n",
                         pasid, virtual_address);
                libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
            }
            cand_for_end = lwr;
        }
    }

    if (cand_for_start && cand_for_end)
    {
        if (cand_for_start != cand_for_end)
        {
            LOG_INIT(LOG_LEVEL_INFO,
                     "Bridging shadow ranges (cand_for_start and cand_for_end) for pasid %d non-shadow VA=%016x\n",
                     pasid, virtual_address);
            cand_for_start->va_end = cand_for_end->va_end;
            cand_for_end->va_start = 0;
            cand_for_end->va_end = 0;
        }
        // Otherwise, nothing needs to be done (completely contained within existing range)
    }
    else if (cand_for_start)
    {
        cand_for_start->va_end = va_end;
    }
    else if (cand_for_end)
    {
        cand_for_end->va_start = va_start;
    }
    else
    {
        if (!empty)
        {
            LOG_INIT(LOG_LEVEL_INFO,
                     "No empty shadow range available for pasid %d non-shadow VA=%016x\n",
                     pasid, virtual_address);
            libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
        }

        empty->pasid = pasid;
        empty->va_start = va_start;
        empty->va_end = va_end;
    }
}

void lib_init_shadow_ranges_allocate_and_map(libos_pasid_t pasid)
{
    LOG_INIT(LOG_LEVEL_INFO, "Allocating and mapping shadow for pasid %d ...\n", pasid);

    for (LwU64 i = 0; i < shadow_ranges_n; i++)
    {
        if (shadow_ranges[i].va_start == 0 && shadow_ranges[i].va_end == 0)
            continue;

        if (shadow_ranges[i].pasid != pasid)
            continue;

        LwU64 size = shadow_ranges[i].va_end - shadow_ranges[i].va_start;

        LOG_INIT(LOG_LEVEL_INFO, "    Shadow range [%d] (pasid %d) %016x - %016x (size %d bytes)\n",
                 i, pasid, shadow_ranges[i].va_start, shadow_ranges[i].va_end, size);

        // Allocate backing for shadow memory
        LwU64 *backing = (LwU64 *) lib_init_heap_alloc(size);
        if (!backing)
        {
            LOG_INIT(LOG_LEVEL_INFO,
                     "Out of memory, unable to allocate backing memory for shadow (reqd: %d bytes)\n",
                     size);
            libosTaskPanic(LIBOS_TASK_PANIC_REASON_CONDITION_FAILED);
        }

        // Zero shadow memory
        for (LwU64 j = 0; j < size / sizeof(LwU64); j++)
            backing[j] = 0;

        // Map shadow memory in pagetable
        lib_init_pagetable_insert(pasid, shadow_ranges[i].va_start, size,
                LIBOS_MEMORY_KIND_PAGED_TCM | LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK, (LwU64) backing);
    }

    // libosTaskIlwalidateData(pagetable, (pagetable_mask + 1) * sizeof(libos_pagetable_entry_t));
    LOG_INIT(LOG_LEVEL_INFO, "    Shadow for pasid %d mapped.\n", pasid);
}
#endif
