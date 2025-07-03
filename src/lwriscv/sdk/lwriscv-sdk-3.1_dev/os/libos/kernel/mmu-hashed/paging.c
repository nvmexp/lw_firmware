/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"

#include "lwmisc.h"
#include "lwtypes.h"
#include "libelf.h"
#include "dma.h"
#include "include/libos_init_daemon.h"
#include "include/libos.h"
#include "pagetable.h"
#include "memory.h"
#include "libriscv.h"
#include "task.h"

typedef struct resident_page_header
{
    struct resident_page_header *next, *prev;
} resident_page_header;

typedef struct resident_page
{
    resident_page_header header;
    LwU64 fb_offset;
} resident_page;
LwU64 cached_phdr_boot_virtual_to_physical_delta;

// fb_offset flags
#define PAGE_DIRTY    1ULL
#define PAGE_RESIDENT 2ULL
#define PAGE_MASK     0xFFFFFFFFFFFFF000ULL

static resident_page_header LIBOS_SECTION_DMEM_PINNED(page_by_pa)[HASH_BUCKETS];

LIBOS_SECTION_IMEM_PINNED static resident_page_header *resident_page_bucket(LwU64 address)
{
    return &page_by_pa[(address % 1023u) & (HASH_BUCKETS - 1u)];
}

LIBOS_SECTION_IMEM_PINNED static void
resident_page_insert(resident_page_header *root, resident_page_header *value)
{
    value->prev       = root;
    value->next       = root->next;
    value->prev->next = value;
    value->next->prev = value;
}

LIBOS_SECTION_IMEM_PINNED static void resident_page_remove(resident_page_header *value)
{
    value->prev->next = value->next;
    value->next->prev = value->prev;
}

static resident_page LIBOS_SECTION_DMEM_PINNED(tcm_pages)[PAGEABLE_TCM_PAGES] = {0};

// @todo Should be indexed on kind.
LwU64 LIBOS_SECTION_DMEM_PINNED(mpu_attributes_base)[2] = {
    // MPU_ATTRIBUTES_COHERENT
    (REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) |
     REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, 0u)),

    // MPU_ATTRIBUTES_INCOHERENT
    (REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) |
     REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, 0u)),
};

/*!
 *
 * @brief Loads the kernel into tightly coupled memory
 *    and sets up access to PEREGRINE PRI registers
 *
 * \param PHDRBootVirtualToPhysicalDelta
 *   This value when added to a kernel VA provides the PA
 *   (within FB).  @see KernelBootstrap
 */
void LIBOS_SECTION_TEXT_COLD KernelPagingLoad(LwS64 PHDRBootVirtualToPhysicalDelta)
{
    //! @note This VA is used for scratch mappings for access to
    //!       non-resident portions of the kernel (paged).
    const LwU64 scratch_va = 0x100000000ULL;

    cached_phdr_boot_virtual_to_physical_delta = PHDRBootVirtualToPhysicalDelta;

    //! The kernel may reside within a write-protect region protected by hardware
    //! We extract the value that the secure-bootloader left us with.

    // This is hardcoded to zero in sepkern. Eventually there will be sldstattr/sfetchattr.
    // LwU64 wprId = REF_VAL64(LW_RISCV_CSR_UACCESSATTR_WPR, KernelCsrRead(LW_RISCV_CSR_UACCESSATTR));

    //! Compute the size of the kernel.
    LwU64 code_paged_size = (LwU64)code_paged_end - (LwU64)code_paged_begin,
          data_paged_size = (LwU64)data_paged_end - (LwU64)data_paged_begin;

    //! Fill in the WPRID into the templates for COHERENT and INCOHERENT paged mappings
    //mpu_attributes_base[MPU_ATTRIBUTES_COHERENT] |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, wprId);
    //mpu_attributes_base[MPU_ATTRIBUTES_INCOHERENT] |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, wprId);

    //! For now, we statically reserve two pages ~ 8kb IMEM page for the kernel
    //  The kernel weighs in at about 5kb without compressed isa
    // @note Won't be a problem on GA11x+ beyond due to compressed isa + 1kb pages
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    if ((LwU64)code_paged_size <= (LIBOS_TCM_INDEX_ITCM_FIRST - 1) * LIBOS_CONFIG_MPU_PAGESIZE)
#else
    if ((LwU64)code_paged_size <= LIBOS_TCM_INDEX_ITCM_FIRST * LIBOS_CONFIG_MPU_PAGESIZE)
#endif
    {
        //! Map the tcm resident portion of the kernel
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_TCM_FIRST);
        KernelCsrWrite(LW_RISCV_CSR_XMPURNG, (LwU64)code_paged_size);
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, (LwU64)PHDRBootVirtualToPhysicalDelta + (LwU64)code_paged_begin);
        KernelCsrWrite(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) |
                REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) // | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, wprId)
        );
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, scratch_va | 1);

        //! Map the IMEM page for kernel as R/W so we can fill it
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_KERNEL_IMEM);
        KernelCsrWrite(LW_RISCV_CSR_XMPURNG, code_paged_size);
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_IMEM_START + 0x1000);
#else
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_IMEM_START);
#endif
        KernelCsrWrite(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, (LwU64)code_paged_begin | 1ULL);

        //! Copy the kernel
        LwU64 *src = (LwU64 *)scratch_va;
        LwU64 *dst = (LwU64 *)code_paged_begin;

        for (LwU64 i = 0ULL; i < code_paged_size / sizeof(LwU64); i++)
        {
            dst[i] = src[i];
        }

        //! Switch permissions to RX for IMEM page
        KernelCsrWrite(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XX, 1ULL));
    }
    else
    {
        KernelPanic();
    }

    //! We use a single page of DMEM for kernel data structures (scheduler, interrupt sequencer,
    //! trap stack, etc)
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    if ((LwU64)data_paged_size <= (LIBOS_TCM_INDEX_DTCM_FIRST - 1) * LIBOS_CONFIG_MPU_PAGESIZE)
#else
    if ((LwU64)data_paged_size <= LIBOS_TCM_INDEX_DTCM_FIRST * LIBOS_CONFIG_MPU_PAGESIZE)
#endif
    {
        //! Create a scratch mapping to the data we're going to copy
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_TCM_FIRST);
        KernelCsrWrite(LW_RISCV_CSR_XMPURNG, (LwU64)data_paged_size);
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, (LwU64)PHDRBootVirtualToPhysicalDelta + (LwU64)data_paged_begin);
        KernelCsrWrite(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) |
                REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, /* wprId = */ 0));
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, scratch_va | 1);

        //! Create the final DTCM mapping for the data
        //  @caution Do not write the enable bit (as this would cut off access to our own stack)
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_KERNEL_DMEM);
        KernelCsrWrite(LW_RISCV_CSR_XMPURNG, data_paged_size);
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_DMEM_START + 0x1000);
#else
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_DMEM_START);
#endif
        KernelCsrWrite(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, (LwU64)data_paged_begin);

        //! Copy the kernel data (and live stack)
        {
            register LwU64 t1 __asm__("t1") = (LwU64)data_paged_begin;
            register LwU64 t2 __asm__("t2") = scratch_va;
            register LwU64 t3 __asm__("t3") = data_paged_size;
            __asm__ __volatile__(
                // Enable the dmem overlay (this overlays the stack)
                "csrsi           %c[RISCV_CSR_XMPUVA], 1;"

                // Start copying data from the scratch page to the DMEM page
                ".load_loop:;"
                "beq t3, x0, .load_loop_end;"
                "ld t0, 0(t2);"
                "sd t0, 0(t1);"
                "addi t1, t1,  8;"
                "addi t2, t2,  8;"
                "addi t3, t3, -8;"
                "j .load_loop;"
                ".load_loop_end:;"
                // Copy is complete and stack is restored
                : "+r"(t1), "+r"(t2), "+r"(t3)
                : [ RISCV_CSR_XMPUVA ] "i"(LW_RISCV_CSR_XMPUVA)
                : "memory", "t0");
        }
    }
    else
    {
        KernelPanic();
    }

    //!  Clear scratch mapping
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_TCM_FIRST);
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0);
}


void LIBOS_SECTION_TEXT_COLD kernel_paging_init_hw(LwS64 PHDRBootVirtualToPhysicalDelta)
{
    // Configure DMA engine
    kernel_dma_init();

    // Pre-initialize the page size for the TCM paging entries
    for (LwU32 i = 0; i < LIBOS_MPU_INDEX_SCRUB_COUNT; i++)
    {
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, Widen(LIBOS_MPU_INDEX_SCRUB_START + i));
        KernelCsrWrite(LW_RISCV_CSR_XMPURNG, LIBOS_CONFIG_MPU_PAGESIZE);
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0ULL);
    }
}

void LIBOS_SECTION_TEXT_COLD KernelPagingLoad(LwS64 PHDRBootVirtualToPhysicalDelta)
{
    // Initialize physically tagged cache
    for (LwU32 i = 0u; i < HASH_BUCKETS; i++)
    {
        page_by_pa[i].next = &page_by_pa[i];
        page_by_pa[i].prev = &page_by_pa[i];
    }

    //! Scratch mapping of the entire FB/SYSMEM with least privileges (WPRID=0)
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_TCM_FIRST);
    KernelCsrWrite(
        LW_RISCV_CSR_XMPURNG, (LW_RISCV_AMAP_SYSGPA_START - LW_RISCV_AMAP_FBGPA_START) + LW_RISCV_AMAP_SYSGPA_SIZE);
    KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_FBGPA_START);
    KernelCsrWrite(
        LW_RISCV_CSR_XMPUATTR, REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
                                   REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) |
                                   REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, LW_RISCV_AMAP_FBGPA_START | 1);

    // Bring up the hardware
    // This will wipe out the scratch mapping
    kernel_paging_init_hw(PHDRBootVirtualToPhysicalDelta);
}

LIBOS_SECTION_IMEM_PINNED void kernel_paging_clear_mpu(void)
{
    for (LwU64 i = 0ULL; i < LIBOS_MPU_INDEX_SCRUB_COUNT; i++)
    {
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_SCRUB_START + i);
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0ULL);
    }
}

static void LIBOS_SECTION_IMEM_PINNED kernel_paging_writeback(resident_page *page)
{
    if ((page->fb_offset & PAGE_DIRTY) == 0ULL)
    {
        return;
    }

    LwU32 page_index = (LwU32)CastUInt64(page - &tcm_pages[0u]);
    LwU64 phys_addr  = page->fb_offset & PAGE_MASK;

    // Clear the dirty bit and mark the page as read-only
    page->fb_offset &= ~PAGE_DIRTY;
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, Widen(LIBOS_MPU_INDEX_DTCM_FIRST + page_index));
    KernelCsrWrite(
        LW_RISCV_CSR_XMPUATTR,
        KernelCsrRead(LW_RISCV_CSR_XMPUATTR) &
            ~(REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL)));

    // @note We're guaranteed that this is not exelwtable memory
    // libos.py doesn't allow writeable code sections
    LwU64 offset = Widen(LIBOS_TCM_INDEX_DTCM_FIRST + page_index - DMEM_PAGE_BASE_INDEX) << 12;
    kernel_dma_start(phys_addr, offset, 4096, LIBOS_DMAIDX_PHYS_VID_FN0, LIBOS_DMAFLAG_WRITE);

    kernel_syscall_dma_flush();
}

static LIBOS_SECTION_IMEM_PINNED void kernel_paging_writeback_and_ilwalidate(resident_page *page)
{
    if ((page->fb_offset & PAGE_RESIDENT) == 0u)
    {
        return;
    }

    kernel_paging_writeback(page);

    // Mark page as invalid
    KernelCsrWrite(
        LW_RISCV_CSR_XMPUIDX, Widen(LIBOS_MPU_INDEX_TCM_FIRST + (LwU32)CastUInt64(page - &tcm_pages[0u])));
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0ULL);

    // De-associate DMEM from page
    resident_page_remove(&page->header);
    page->fb_offset &= ~PAGE_RESIDENT;
}

/*!
 *
 * @brief Writes back all dirty pages back to FB.
 *
 *   Guarantees that no dirty data (either kernel global variables or task data)
 *   is in volatile storage (TCM).
 *
 *   ROUTINE ASSUMES KERNEL TERMINATION IMMEDIATELY AFTER.
 *
 *   Intended for use in Processor Suspend Algorithm:
 *      - Kernel writes back
 *         1. All dirty data pages
 *            @note Guaranteed to be no dirty code pages by construction of libos.py
 *         2. Kernel DMEM page
 *      - Kernel raises interrupt and then HALTS
 *      - Kernel is restarted later and resets the stack
 *
 */
LIBOS_SECTION_TEXT_COLD void KernelPagingPrepareForPartitionSwitch(void)
{
    //! Trigger eviction of all pages
    for (LwU32 i = 0u; i < PAGEABLE_TCM_PAGES; i++)
    {
        kernel_paging_writeback_and_ilwalidate(&tcm_pages[i]);
    }

    //! Use the standard scratch area for kernel writeback
    const LwU64 scratch_va = 0x100000000ULL;

    //! The kernel may reside within a write-protect region protected by hardware
    //! We extract the value that the secure-bootloader left us with.

    // This is hardcoded to zero in sepkern. Eventually there will be sldstattr/sfetchattr.
    // LwU64 wprId = REF_VAL64(LW_RISCV_CSR_UACCESSATTR_WPR, KernelCsrRead(LW_RISCV_CSR_UACCESSATTR));

    //! Compute the size of the kernel DMEM region
    LwU64 data_paged_size = (LwU64)data_paged_end - (LwU64)data_paged_begin;

    //! Create a writeable scratch mapping to the cached memory location
    // @note We use LIBOS_MPU_INDEX_TCM_FIRST, since we've already written DMEM back
    // @note and we know the kernel is halting.  It's important that the MPU index be
    // @note no higher, otherwise the scratch_va address may be caught be a user mapping (!).
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_TCM_FIRST);
    KernelCsrWrite(LW_RISCV_CSR_XMPURNG, (LwU64)data_paged_size);
    KernelCsrWrite(
        LW_RISCV_CSR_XMPUPA, (LwU64)cached_phdr_boot_virtual_to_physical_delta + (LwU64)data_paged_begin);
    KernelCsrWrite(
        LW_RISCV_CSR_XMPUATTR,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) // | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, wprId)
    );
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, scratch_va | 1);

    // @note The writeback is done in assembly since we're writing back the
    // @note stack to DMEM.  We need to ensure no stack accesses happen until
    // @note we complete the copy and disable the KERNEL_DMEM MPU mapping
    {
        register LwU64 t1 __asm__("t1") = scratch_va;
        register LwU64 t2 __asm__("t2") = (LwU64)data_paged_begin;
        register LwU64 t3 __asm__("t3") = data_paged_size;
        __asm__ __volatile__(
            //! Start copying data from the scratch page to the DMEM page
            ".load_loop2:;"
            "beq t3, x0, .load_loop_end2;"
            "ld t0, 0(t2);"
            "sd t0, 0(t1);"
            "addi t1, t1,  8;"
            "addi t2, t2,  8;"
            "addi t3, t3, -8;"
            "j .load_loop2;"
            ".load_loop_end2:;"

            //! Disable the KERNEL_DMEM mapping (access will fall back to the later cached mapping)
            "csrwi           %c[RISCV_CSR_XMPUIDX], %c[MPU_INDEX_KERNEL_DMEM];"
            "csrsi           %c[RISCV_CSR_XMPUVA], 1;"

            // Copy is complete and stack is restored
            : "+r"(t1), "+r"(t2), "+r"(t3)
            : [ RISCV_CSR_XMPUVA ] "i"(LW_RISCV_CSR_XMPUVA), [ RISCV_CSR_XMPUIDX ] "i"(LW_RISCV_CSR_XMPUIDX),
              [ MPU_INDEX_KERNEL_DMEM ] "i"(LIBOS_MPU_INDEX_KERNEL_DMEM)
            : "memory", "t0");
    }
}

/*!
 *
 * @brief pages in a cached or TCM paged.
 *
 */
LIBOS_SECTION_IMEM_PINNED LwU64
PagingPageIn(Pasid pasid, LwU64 xbadaddr, LwU64 xcause, LwBool skip_read)
{
    LwU64 mpu_attributes;
    LwU32 offset;
    LwU32 mpu_index;

    // @todo: Hack, we should really send LACC_FAULT and LPAGE_FAULT to different
    //        locations on GA102 as the LACC_FAULT suggests aa MEMERR directly.
    #if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
      LwBool was_write = xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT;
    #else
      LwBool was_write = xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT ||
                         xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SPAGE_FAULT;
    #endif
    xbadaddr = xbadaddr & ~(LIBOS_CONFIG_MPU_PAGESIZE - 1ULL);

    libos_pagetable_entry_t *memory = kernel_resolve_address(pasid, xbadaddr);

    // DMA-only regions should not be ilwolved in paging transactions
    // @note: The null_descriptor (translation fail) is marked as a zero size DMA transaction.
    if (LIBOS_MEMORY_IS_DMA(memory))
    {
        return LIBOS_ILWALID_TCM_OFFSET;
    }

    /*
        Verify that we have the appropriate write permissions
    */
    if (was_write && !(memory->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK))
        return LIBOS_ILWALID_TCM_OFFSET;


    if (LIBOS_MEMORY_IS_MMIO(memory))
    {
        mpu_attributes = mpu_attributes_base[MPU_ATTRIBUTES_INCOHERENT];
    }
    else
    {
        mpu_attributes = mpu_attributes_base[MPU_ATTRIBUTES_COHERENT];
        if (LIBOS_MEMORY_IS_CACHED(memory))
        {
            mpu_attributes |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1ULL);
        }
    }

    if (LIBOS_MEMORY_IS_EXELWTE(memory))
    {
        mpu_attributes |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_UX, 1ULL);
    }
    else
    {
        mpu_attributes |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL);
        mpu_attributes |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL);
    }

    if (!LIBOS_MEMORY_IS_PAGED_TCM(memory))
    {
        LwU32 mpu_index;

        //
        //  BUG WAR: GA100 and TU10x return LACC faults when the MPU entry is valid
        //           for the requested access. This happens on PRI timeouts, ECC
        //           errors and other scenarios.  Later hardware adds a fault bit
        //           to distinguish.
        //
#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
        // Where was this cached entry last resident?
        LwU32 last_index = memory->attributes_and_size & LIBOS_MEMORY_MPU_INDEX_MASK;

        // Was this entry already in the MPU?
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, last_index);

        // @note We already verified access above, this is guaranteed to be
        //       a MEMERR in disguise.  Pass it up as a failure.
        if (KernelCsrRead(LW_RISCV_CSR_XMPUVA) == (memory->virtual_address | 1ULL))
            return LIBOS_ILWALID_TCM_OFFSET;
#else
    //
    //  LACC/SACC are reserved for other memory errors (pri timeout, memerr, pmp permissions)
    //  This implies the MPU entry is valid.
    //
    if (xcause == LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT ||
        xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT)
        return LIBOS_ILWALID_TCM_OFFSET;
#endif
        // Pick a victim MPU entry
        mpu_index =
            LIBOS_MPU_INDEX_CACHED_FIRST + (KernelCsrRead(LW_RISCV_CSR_CYCLE) % LIBOS_MPU_INDEX_CACHED_COUNT);

#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
        // Update the last MPU index
        memory->attributes_and_size =
            (memory->attributes_and_size & ~LIBOS_MEMORY_MPU_INDEX_MASK) | mpu_index;
#endif

        // Switch the MPU index
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, mpu_index);
        KernelCsrWrite(LW_RISCV_CSR_XMPUATTR, mpu_attributes);
        KernelCsrWrite(LW_RISCV_CSR_XMPUVA, memory->virtual_address | 1ULL);
        KernelCsrWrite(LW_RISCV_CSR_XMPURNG, memory->attributes_and_size & ~LIBOS_MEMORY_ATTRIBUTE_MASK);
        KernelCsrWrite(LW_RISCV_CSR_XMPUPA, memory->physical_address);

        // XXX SL TODO: this return value needs to be distinct from LIBOS_ILWALID_TCM_OFFSET since
        // this isn't an error case
        return 0;
    }

    if (!was_write)
    {
        mpu_attributes &=
            ~(REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL));
    }

    LwU64 translated_address = xbadaddr - memory->virtual_address + memory->physical_address;

    // Locate the page in the hash-table (to see if it was mapped as read-only)
    resident_page_header *bucket = resident_page_bucket(translated_address);

    for (resident_page_header *i = bucket->next; i != bucket; i = i->next)
    {
        resident_page *page = CONTAINER_OF(i, resident_page, header);

        if ((page->fb_offset & PAGE_MASK) == translated_address)
        {
            mpu_index = (LwU32)CastUInt64(page - &tcm_pages[0]);
            KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, Widen(LIBOS_MPU_INDEX_TCM_FIRST + mpu_index));
            KernelCsrWrite(LW_RISCV_CSR_XMPUATTR, mpu_attributes);
            // XXX CB TODO: removing this causes test failures see kernel_paging_clear_mpu() above
            KernelCsrWrite(LW_RISCV_CSR_XMPUVA, xbadaddr | 1ULL);
            if (was_write)
            {
                page->fb_offset |= PAGE_DIRTY;
            }
            if (mpu_index < LIBOS_MPU_INDEX_DTCM_COUNT)
            {
                return (LIBOS_TCM_INDEX_DTCM_FIRST + (mpu_index - DMEM_PAGE_BASE_INDEX)) << 12u;
            }
            else
            {
                return (LIBOS_TCM_INDEX_ITCM_FIRST + (mpu_index - IMEM_PAGE_BASE_INDEX)) << 12u;
            }
        }
    }

    // Pick a page to evict (poorly)
    LwU32 rand_index = (LwU32)KernelCsrRead(LW_RISCV_CSR_CYCLE);
    LwU64 pa;
    LwBool is_exec;

    // Adjust chosen index to reference TCM entry in correct pool
    if (LIBOS_MEMORY_IS_EXELWTE(memory))
    {
        mpu_index = IMEM_PAGE_BASE_INDEX + (rand_index % LIBOS_MPU_INDEX_ITCM_COUNT);
        offset    = (LIBOS_TCM_INDEX_ITCM_FIRST + (mpu_index - IMEM_PAGE_BASE_INDEX)) << 12u;
        pa        = LW_RISCV_AMAP_ITCM_BASE | Widen(offset);
        is_exec   = LW_TRUE;
    }
    else
    {
        mpu_index = DMEM_PAGE_BASE_INDEX + (rand_index % LIBOS_MPU_INDEX_DTCM_COUNT);
        offset    = (LIBOS_TCM_INDEX_DTCM_FIRST + (mpu_index - DMEM_PAGE_BASE_INDEX)) << 12u;
        pa        = LW_RISCV_AMAP_DTCM_BASE | Widen(offset);
        is_exec   = LW_FALSE;
    }

    // Remove this page from the tracking structure if needed
    if ((tcm_pages[mpu_index].fb_offset & PAGE_RESIDENT) != 0ULL)
    {
        kernel_paging_writeback(&tcm_pages[mpu_index]);
        resident_page_remove(&tcm_pages[mpu_index].header);
    }

    // Point the donor MPU entry at the bad VA and the dmem page
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, Widen(LIBOS_MPU_INDEX_TCM_FIRST + mpu_index));
    KernelCsrWrite(LW_RISCV_CSR_XMPUATTR, mpu_attributes);
    KernelCsrWrite(LW_RISCV_CSR_XMPUPA, pa);
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, xbadaddr | 1ULL);

    if (!skip_read)
    {
        kernel_dma_start(
            translated_address, offset, 4096, LIBOS_DMAIDX_PHYS_VID_FN0, is_exec ? LIBOS_DMAFLAG_IMEM : 0);
    }

    // Perform the tracking structure updates while the DMA operation is finishing up
    // (renders this code essentially free)
    tcm_pages[mpu_index].fb_offset = translated_address | PAGE_RESIDENT | (was_write ? PAGE_DIRTY : 0ULL);
    resident_page_insert(resident_page_bucket(translated_address), &tcm_pages[mpu_index].header);

    kernel_syscall_dma_flush();

    return offset;
}


LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
KernelValidateMemoryAccessOrReturn(LwU64 buffer, LwU64 buffer_size, LwBool write)
{
    return validate_memory_pasid_access_or_return(buffer, buffer_size, write, KernelSchedulerGetTask()->pasid);
}

LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
validate_memory_pasid_access_or_return(LwU64 buffer, LwU64 buffer_size, LwBool write, Pasid pasid)
{
    libos_pagetable_entry_t *region = kernel_resolve_address(pasid, buffer & ~(LIBOS_CONFIG_MPU_PAGESIZE - 1));

    /*
        Verify that we have the appropriate write permissions
    */
    if (write && !(region->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK))
        KernelSyscallReturn(LibosErrorAccess);

    /*
        kernelResolve_address guarantees that "buffer" is within the region.
        Therefore subtracting off region start cannot overflow
    */
    LwU64 offset = (LwU64)buffer - region->virtual_address;

    /*
        Callwlate the remaining size available for copy starting at buffer.
        Again, this cannot overflow as the offset was guaranteed to be within
        the buffer (this comes from a trusted source)
    */
    LwU64 size_from_offset = (region->attributes_and_size & ~LIBOS_MEMORY_ATTRIBUTE_MASK) - offset;

    /*
        Compare against the requested buffer size
    */
    if (buffer_size > size_from_offset)
    {
        KernelSyscallReturn(LibosErrorAccess);
    }

    return region;
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_codefault()
{
    if (PagingPageIn(KernelSchedulerGetTask()->pasid, KernelSchedulerGetTask()->state.xbadaddr, KernelSchedulerGetTask()->state.xcause, LW_FALSE) !=
        LIBOS_ILWALID_TCM_OFFSET)
    {
        KernelTaskReturn();
    }

    KernelTaskPanic();
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_datafault()
{
#if LIBOS_LWRISCV_1_0
#   define RISCV_OPCODE_LD 0x03
#   define RISCV_OPCODE_SD 0x23
    /*
        WAR Tu10x Hardware bug:
            xtval will contain either a VA or a PA depending on
            whether it was an MPU hit.  As a result the register
            value cannot safely be used.

        lwbugs/200410506
    */

    // Safe to de-reference as we already know this wasn't an instruction fault.
    LwU32 instruction = *(LwU32 *)KernelSchedulerGetTask()->state.xepc;

    /*
        31         25  24..20   19..15  14..12   11..7       6..0
        --------------------------------------------------------------
        |  Imm[11:5] | rs2    |  rs1  | funct3 | imm[4:0]  | opcode  |
        --------------------------------------------------------------
                                                            STORE

        31         20  19..15  14..12   11..7      6..0
        -----------------------------------------------------
        |  Imm[11:0]  |  rs1  | funct3 |   rd   |   opcode  |
        -----------------------------------------------------
                                                    LOAD
    */
    LwU32 address_base = (instruction >> 15) & 0x1Fu;

    LwS32 immediate_shift_20; // Top 12 bits will be the signed immediate

    if (KernelSchedulerGetTask()->state.xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT)
    {
        immediate_shift_20 = CastInt32((instruction & 0xFE000000u) | ((instruction & 0xF80u) << 13));
        LibosAssert((instruction & 0x7F) == RISCV_OPCODE_SD);
    }
    else // (KernelSchedulerGetTask()->state.xcause == CSR_XCAUSE_MEMORY_FAULT_LOAD_ACCESS)
    {
        immediate_shift_20 = (LwS32)instruction;
        LibosAssert((instruction & 0x7F) == RISCV_OPCODE_LD);
    }

    // WAR: Override the hardware xtval based on decoded instruction
    // Shifting right by 20 has the effect of sign extending the immediate
    KernelSchedulerGetTask()->state.xbadaddr = (KernelSchedulerGetTask()->state.registers[address_base] + (immediate_shift_20 / 0x100000));
#endif
    // Handle DMA page page fault
    if (PagingPageIn(KernelSchedulerGetTask()->pasid, KernelSchedulerGetTask()->state.xbadaddr, KernelSchedulerGetTask()->state.xcause, LW_FALSE) !=
        LIBOS_ILWALID_TCM_OFFSET)
    {
        KernelTaskReturn();
    }

    KernelTaskPanic();
}
