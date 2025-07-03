/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"

#include "dev_falcon_v4.h"
#include "dev_riscv_pri.h"
#include "dev_fb.h"
#include "dma.h"
#include "ilwerted_pagetable.h"
#include "mmu.h"
#include "lw_gsp_riscv_address_map.h"
#include "paging.h"
#include "partition.h"

static const LwU64 scratch_va = 0x100000000ULL;
LwU64 cached_phdr_boot_virtual_to_physical_delta;
LwU32 mpu_entry_count;
libos_wpr_info_t LIBOS_SECTION_DMEM_PINNED(wpr);

// @todo Should be indexed on kind.
LwU64 LIBOS_SECTION_DMEM_PINNED(mpu_attributes_base)[2] = {
    // MPU_ATTRIBUTES_COHERENT
    (REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) |
     REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u)),

    // MPU_ATTRIBUTES_INCOHERENT
    (REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u)),
};

static inline __attribute__((always_inline)) LwU64 va_to_pa(const void *ptr)
{
    return cached_phdr_boot_virtual_to_physical_delta + (LwU64)ptr;
}

static inline __attribute__((always_inline)) void kernel_mmu_program_scratch_mpu_entry(
    LwU64 pa,
    LwU64 size,
    LwBool write)
{
    //! @note This VA is used for scratch mappings for access to
    //!       non-resident portions of the kernel (paged).
    LwU64 scratch_attr = REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
                         REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u);

    if (write)
        scratch_attr |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1u);
    else
        scratch_attr |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u);

    kernel_mmu_program_mpu_entry(LIBOS_MPU_INDEX_SCRATCH, scratch_va, pa, size, scratch_attr);
}

#if LIBOS_CONFIG_WPR
static void LIBOS_SECTION_TEXT_COLD kernel_mmu_get_wpr_info(LwU64 code_pa)
{
#if LIBOS_CONFIG_WPR != 2
#error This implementation lwrrently assumes WPR2
#endif
    // Map PRIV briefly to read the WPR bounds
    kernel_mmu_program_scratch_mpu_entry(LW_RISCV_AMAP_PRIV_START,
                                         LW_RISCV_AMAP_PRIV_SIZE,
                                         LW_FALSE);

    LwU64 wpr2Base = *((LwU32 *)(scratch_va + LW_PFB_PRI_MMU_WPR2_ADDR_LO));
    LwU64 wpr2End  = *((LwU32 *)(scratch_va + LW_PFB_PRI_MMU_WPR2_ADDR_HI));

    kernel_mmu_clear_mpu_entry(LIBOS_MPU_INDEX_SCRATCH);

    wpr2Base = REF_VAL64(LW_PFB_PRI_MMU_WPR2_ADDR_LO_VAL, wpr2Base);
    wpr2End  = REF_VAL64(LW_PFB_PRI_MMU_WPR2_ADDR_HI_VAL, wpr2End);
    wpr2Base <<= LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;
    wpr2End  <<= LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;
    wpr2End  += 0x1FFFFu;    // _ADDR_HI is the address of the last 128KB block in WPR
    wpr2Base += LW_RISCV_AMAP_FBGPA_START;
    wpr2End  += LW_RISCV_AMAP_FBGPA_START;

    if ((code_pa >= wpr2Base) && (code_pa < wpr2End))
    {
        wpr.start = wpr2Base;
        wpr.end   = wpr2End;
    }
}
#endif

void LIBOS_SECTION_TEXT_COLD kernel_mmu_load(void)
{
#if LIBOS_CONFIG_TCM_PAGING
    //! Scratch mapping of the entire FB/SYSMEM with least privileges (WPRID=0)
    kernel_mmu_program_scratch_mpu_entry(
        LW_RISCV_AMAP_FBGPA_START,
        (LW_RISCV_AMAP_SYSGPA_START - LW_RISCV_AMAP_FBGPA_START) + LW_RISCV_AMAP_SYSGPA_SIZE,
        LW_FALSE);

    // Configure DMA engine - only needed if TCM paging is enabled
    kernel_dma_init();

    // Remove scratch mapping
    kernel_mmu_clear_mpu_entry(LIBOS_MPU_INDEX_SCRATCH);
#endif
}

/*!
 *
 * @brief Loads the kernel into tightly coupled memory
 *    and sets up access to PEREGRINE PRI registers
 *
 * \param phdr_boot_virtual_to_physical_delta
 *   This value when added to a kernel VA provides the PA
 *   (within FB).  @see kernel_bootstrap
 */
void LIBOS_SECTION_TEXT_COLD kernel_mmu_load_kernel(
    const libos_bootstrap_args_t *boot_args,
    LwS64 phdr_boot_virtual_to_physical_delta)
{
    cached_phdr_boot_virtual_to_physical_delta = phdr_boot_virtual_to_physical_delta;
#if LIBOS_CONFIG_RISCV_S_MODE
    //! Visible MPU entries in S-mode can be restricted by SK in LWRISCV 2.0+
    mpu_entry_count = REF_VAL64(LW_RISCV_CSR_SMPUCTL_ENTRY_COUNT,
                                csr_read(LW_RISCV_CSR_SMPUCTL));
#else
    mpu_entry_count = (1u << ((1 ? LW_RISCV_CSR_MMPUIDX_INDEX) -
                              (0 ? LW_RISCV_CSR_MMPUIDX_INDEX) + 1u));
#endif

#if LIBOS_CONFIG_WPR
    //! The kernel may reside within a write-protect region protected by hardware
    //! We extract the value that the secure-bootloader left us with.
    kernel_mmu_get_wpr_info(va_to_pa(code_paged_begin));
#endif

    LwU64 imem_base_pa, imem_size;
    LwU64 dmem_base_pa, dmem_size;

    //! Create a temporary identity mapping to the DMEM page containing the boot
    //! arguments, if the structure is in DTCM. Since on TU10x and GA100,
    //! LW_RISCV_AMAP_DMEM_START happens to be the same as scratch_va, we use the
    //! scratch mapping index so that they can never collide.
    if (((LwU64)boot_args >= LW_RISCV_AMAP_DMEM_START) &&
        ((LwU64)boot_args <  LW_RISCV_AMAP_DMEM_END))
    {
        kernel_mmu_program_mpu_entry(LIBOS_MPU_INDEX_SCRATCH,
                                     (LwU64)boot_args & LIBOS_MPU_PAGEMASK,
                                     (LwU64)boot_args & LIBOS_MPU_PAGEMASK,
                                     LIBOS_MPU_PAGESIZE,
                                     REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));
    }

    imem_base_pa = boot_args->imem_base_pa;
    imem_size    = boot_args->imem_size;
    dmem_base_pa = boot_args->dmem_base_pa;
    dmem_size    = boot_args->dmem_size;

#if LIBOS_CONFIG_PARTITION
    //! Save off the partition bootstrapping TCM, so that it can be clobbered.
    kernel_partition_save_bootstrap_tcm(boot_args);
#endif

    //! Do we fit in the imem allocated to us at boot?
    //! The kernel weighs in at about 5kb without compressed isa.
    if (LIBOS_KERNEL_PINNED_ITCM_SIZE <= imem_size)
    {
        //! Map the tcm resident portion of the kernel
        kernel_mmu_program_scratch_mpu_entry(va_to_pa(code_paged_begin),
                                             LIBOS_KERNEL_PINNED_ITCM_SIZE,
                                             LW_FALSE);

        //! Map the IMEM page for kernel as R/W so we can fill it
        kernel_mmu_program_mpu_entry(LIBOS_MPU_INDEX_KERNEL_PINNED_ITCM,
                                     (LwU64)code_paged_begin,
                                     imem_base_pa,
                                     LIBOS_KERNEL_PINNED_ITCM_SIZE,
                                     REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) |
                                        REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));

        //! Copy the kernel
        LwU64 *src = (LwU64 *)scratch_va;
        LwU64 *dst = (LwU64 *)code_paged_begin;

        for (LwU64 i = 0ULL; i < LIBOS_KERNEL_PINNED_ITCM_SIZE / sizeof(LwU64); i++)
        {
            dst[i] = src[i];
        }

        //! Switch permissions to RX for IMEM page
        csr_write(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XX, 1ULL));
    }
    else
    {
        panic_kernel();
    }

    //! We use a single page of DMEM for kernel data structures (scheduler, interrupt sequencer,
    //! trap stack, etc)
    if (LIBOS_KERNEL_PINNED_DTCM_SIZE <= dmem_size)
    {
        //! Create a scratch mapping to the data we're going to copy
        kernel_mmu_program_scratch_mpu_entry(va_to_pa(data_paged_begin),
                                             LIBOS_KERNEL_PINNED_DTCM_SIZE,
                                             LW_FALSE);

        //! Create the final DTCM mapping for the data
        //  @caution Do not write the enable bit (as this would cut off access to our own stack)
        csr_write(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_KERNEL_PINNED_DTCM);
        csr_write(LW_RISCV_CSR_XMPURNG, LIBOS_KERNEL_PINNED_DTCM_SIZE);
        csr_write(LW_RISCV_CSR_XMPUPA, dmem_base_pa);
        csr_write(
            LW_RISCV_CSR_XMPUATTR,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));
        csr_write(LW_RISCV_CSR_XMPUVA, (LwU64)data_paged_begin);

        //! Copy the kernel data (and live stack)
        {
            register LwU64 t1 __asm__("t1") = (LwU64)data_paged_begin;
            register LwU64 t2 __asm__("t2") = scratch_va;
            register LwU64 t3 __asm__("t3") = LIBOS_KERNEL_PINNED_DTCM_SIZE;
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
        panic_kernel();
    }

    //!  Enable PRI for just the Peregrine and Falcon registers
    kernel_mmu_program_mpu_entry(LIBOS_MPU_INDEX_EXTIO_PRI,
                                 PEREGRINE_EXTIO_PRI_BASE_VA,
                                 LW_RISCV_AMAP_PRIV_START + LW_FALCON_GSP_BASE,
                                 0x2000ULL,
                                 REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) |
                                    REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL));

    //!  Clear scratch mapping and any mappings leftover by the bootstrapper
    kernel_mmu_clear_mpu();
}

LIBOS_SECTION_IMEM_PINNED void kernel_mmu_clear_mpu(void)
{
    for (LwU64 i = LIBOS_MPU_INDEX_SCRUB_START; i < mpu_entry_count; i++)
    {
        kernel_mmu_clear_mpu_entry(i);
    }
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
LIBOS_SECTION_TEXT_COLD void kernel_mmu_prepare_for_processor_suspend(void)
{
#if LIBOS_CONFIG_TCM_PAGING
    //! Trigger eviction of all pages
    for (LwU32 i = 0u; i < MAX_PAGEABLE_TCM_PAGES; i++)
    {
        kernel_paging_writeback_and_ilwalidate(&tcm_pages[i]);
    }
#endif

    //! The kernel may reside within a write-protect region protected by hardware
    //! We extract the value that the secure-bootloader left us with.

    // This is hardcoded to zero in sepkern. Eventually there will be sldstattr/sfetchattr.
    // LwU64 wprId = REF_VAL64(LW_RISCV_CSR_UACCESSATTR_WPR, csr_read(LW_RISCV_CSR_UACCESSATTR));

    //! Create a writeable scratch mapping to the cached memory location
    // @note We use LIBOS_MPU_INDEX_SCRATCH, since we've already written DMEM back
    // @note and we know the kernel is halting.  It's important that the MPU index be
    // @note no higher, otherwise the scratch_va address may be caught be a user mapping (!).
    kernel_mmu_program_scratch_mpu_entry(va_to_pa(data_paged_begin),
                                         LIBOS_KERNEL_PINNED_DTCM_SIZE,
                                         LW_TRUE);

    // @note The writeback is done in assembly since we're writing back the
    // @note stack to DMEM.  We need to ensure no stack accesses happen until
    // @note we complete the copy and disable the KERNEL_DMEM MPU mapping
    {
        register LwU64 t1 __asm__("t1") = scratch_va;
        register LwU64 t2 __asm__("t2") = (LwU64)data_paged_begin;
        register LwU64 t3 __asm__("t3") = LIBOS_KERNEL_PINNED_DTCM_SIZE;
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
              [ MPU_INDEX_KERNEL_DMEM ] "i"(LIBOS_MPU_INDEX_KERNEL_PINNED_DTCM)
            : "memory", "t0");
    }

#if LIBOS_CONFIG_PARTITION
    //! We are in the cold path and everything has been written back.
    //! Disable the TCM mappings because the TCM is about to be overwritten.
    kernel_mmu_clear_mpu_entry(LIBOS_MPU_INDEX_KERNEL_PINNED_ITCM);
    kernel_mmu_clear_mpu_entry(LIBOS_MPU_INDEX_KERNEL_PINNED_DTCM);

    //! Extend the cached code MPU entry over the paged code in FB, to cover
    //! "pinned" functions that will be called in the remaining suspend path.
    //! @note The code_cached range precedes the code_paged range.
    csr_write(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_INDEX_CACHE_CODE);
    LwU64 code_cached_begin = csr_read(LW_RISCV_CSR_XMPUVA) & ~1ULL;
    csr_write(LW_RISCV_CSR_XMPURNG, ((LwU64)code_paged_begin - code_cached_begin) +
                                     LIBOS_KERNEL_PINNED_ITCM_SIZE);

    //! Scrub the rest of the MPU
    kernel_mmu_clear_mpu();

    //! Restore the partition bootstrapping TCM so that we can be booted again
    kernel_partition_restore_bootstrap_tcm();
#endif
}

LIBOS_SECTION_TEXT_COLD void kernel_mmu_program_mpu_entry(LwU64 idx, LwU64 va, LwU64 pa, LwU64 size, LwU64 attr)
{
#if LIBOS_CONFIG_WPR
    if (WPR_OVERLAP(pa, size))
        attr |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, LIBOS_CONFIG_WPR);
#endif

    csr_write(LW_RISCV_CSR_XMPUIDX, idx);
    csr_write(LW_RISCV_CSR_XMPUATTR, attr);
    csr_write(LW_RISCV_CSR_XMPUPA, pa);
    csr_write(LW_RISCV_CSR_XMPURNG, size);
    csr_write(LW_RISCV_CSR_XMPUVA, va | 1ULL /* VALID */);
}

LIBOS_SECTION_TEXT_COLD void kernel_mmu_mark_mpu_entry_ro(LwU64 idx)
{
    csr_write(LW_RISCV_CSR_XMPUIDX, idx);
    csr_write(
        LW_RISCV_CSR_XMPUATTR,
        csr_read(LW_RISCV_CSR_XMPUATTR) &
            ~(REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL)));
}

LIBOS_SECTION_TEXT_COLD void kernel_mmu_clear_mpu_entry(LwU64 idx)
{
    csr_write(LW_RISCV_CSR_XMPUIDX, idx);
    csr_write(LW_RISCV_CSR_XMPUVA, 0ULL);
}

LIBOS_SECTION_TEXT_COLD void kernel_mmu_map_pagetable(LwU64 va, LwU64 pa, LwU64 size)
{
    LwU64 mpu_attr = mpu_attributes_base[MPU_ATTRIBUTES_COHERENT] |
                     REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1ULL);

#ifdef LIBOS_MEMORY_MPU_INDEX_MASK
    // Pagetable entries must be writeable by the kernel, but only if we store
    // MPU indices in them
    mpu_attr |= REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL);
#endif

    kernel_mmu_program_mpu_entry(LIBOS_MPU_INDEX_PAGETABLE, va, pa, size, mpu_attr);
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_codefault()
{
    LwU64 xbadaddr_page = mpu_page_addr(self->state.xbadaddr);
    libos_pagetable_entry_t *memory = kernel_resolve_address(self->pasid, xbadaddr_page);
    if (kernel_mmu_page_in(memory, xbadaddr_page, self->state.xcause, LW_FALSE) !=
        LIBOS_ILWALID_TCM_OFFSET)
    {
        kernel_return();
    }

    kernel_task_panic();
}

static LIBOS_SECTION_TEXT_COLD void
kernel_load_access_fault(libos_pagetable_entry_t *memory)
{
    // Always reset the error code to 0 for xcauses that clients may
    // look at this field for.
    self->state.lwriscv.priv_err_info = 0;

#if LWRISCV_VERSION >= LWRISCV_2_2
    // A LACC fault generated due to an IO error will update the
    // RISCV_PRIV_ERR_* registers with additional info on LWRISCV 2.2+.
    // Other LACC faults require no additional handling.
    if (!LIBOS_MEMORY_IS_MMIO(memory))
        return;

    LwU32 intr = riscv_read(LW_PRISCV_RISCV_PRIV_ERR_STAT);
    if ((intr & REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID, _TRUE)) &&
        !(intr & REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_WRITE, _TRUE)))
    {
        // Only pull out the error code if the address matches xbadaddr, to
        // prevent contaminating the info with another error's context.
        // The PRIV_ERR_ADDR is a PA, whereas xbadaddr is a VA, so we need
        // to use the translation we've already resolved.
        LwU64 err_addr_pa = ((LwU64)riscv_read(LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI) << 32) |
                            riscv_read(LW_PRISCV_RISCV_PRIV_ERR_ADDR);
        LwU64 xbadaddr_pa = memory->physical_address +
                            (self->state.xbadaddr - memory->virtual_address);
        if (err_addr_pa == xbadaddr_pa)
        {
            self->state.lwriscv.priv_err_info = riscv_read(LW_PRISCV_RISCV_PRIV_ERR_INFO);

            // Clear the error status to allow capturing of later errors
            riscv_write(LW_PRISCV_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
        }
    }
#endif
}

static LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void
kernel_datafault_cold(libos_pagetable_entry_t *memory)
{
    if (self->state.xcause == LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT)
        kernel_load_access_fault(memory);

    kernel_task_panic();
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_datafault()
{
    LwU64 xbadaddr_page;
    libos_pagetable_entry_t *memory;

#if LWRISCV_VERSION == LWRISCV_1_0
    /*
        WAR Tu10x Hardware bug:
            xtval will contain either a VA or a PA depending on
            whether it was an MPU hit.  As a result the register
            value cannot safely be used.

        lwbugs/200410506
    */

    // Safe to de-reference as we already know this wasn't an instruction fault.
    LwU32 instruction = *(LwU32 *)self->state.xepc;

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

    if (self->state.xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT)
    {
        immediate_shift_20 = to_int32((instruction & 0xFE000000u) | ((instruction & 0xF80u) << 13));
        LIBOS_ASSERT((instruction & 0x7F) == RISCV_OPCODE_SD);
    }
    else // (self->state.xcause == CSR_XCAUSE_MEMORY_FAULT_LOAD_ACCESS)
    {
        immediate_shift_20 = (LwS32)instruction;
        LIBOS_ASSERT((instruction & 0x7F) == RISCV_OPCODE_LD);
    }

    // WAR: Override the hardware xtval based on decoded instruction
    // Shifting right by 20 has the effect of sign extending the immediate
    self->state.xbadaddr = (self->state.registers[address_base] + (immediate_shift_20 / 0x100000));
#endif
    xbadaddr_page = mpu_page_addr(self->state.xbadaddr);
    memory = kernel_resolve_address(self->pasid, xbadaddr_page);

    // Handle MPU/page faults (hot path)
    if (kernel_mmu_page_in(memory, xbadaddr_page, self->state.xcause, LW_FALSE) !=
        LIBOS_ILWALID_TCM_OFFSET)
    {
        kernel_return();
    }

    // Handle everything else (cold path)
    kernel_datafault_cold(memory);
}

static LIBOS_SECTION_IMEM_PINNED void
kernel_mmu_map_region(LwU32 mpu_index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attr)
{
    csr_write(LW_RISCV_CSR_XMPUIDX, mpu_index);
    csr_write(LW_RISCV_CSR_XMPUATTR, attr);
    csr_write(LW_RISCV_CSR_XMPUVA, va | 1ULL);
    csr_write(LW_RISCV_CSR_XMPURNG, size);
    csr_write(LW_RISCV_CSR_XMPUPA, pa);
}

LIBOS_SECTION_IMEM_PINNED LwU64
kernel_mmu_page_in(libos_pagetable_entry_t *memory, LwU64 xbadaddr, LwU64 xcause, LwBool skip_read)
{
    LwU64 mpu_attributes;
    LwU32 mpu_index;

    // @todo: Hack, we should really send LACC_FAULT and LPAGE_FAULT to different
    //        locations on GA102 as the LACC_FAULT suggests aa MEMERR directly.
    #if LWRISCV_VERSION < LWRISCV_2_0
      LwBool was_write = xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT;
    #else
      LwBool was_write = xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT ||
                         xcause == LW_RISCV_CSR_XCAUSE_EXCODE_SPAGE_FAULT;
    #endif

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
#if LIBOS_CONFIG_TCM_PAGING
        if (LIBOS_MEMORY_IS_CACHED(memory))
#else
        // When TCM paging is disabled, we direct map its backing FB as cached instead
        if (LIBOS_MEMORY_IS_CACHED(memory) || LIBOS_MEMORY_IS_PAGED_TCM(memory))
#endif
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

#if LIBOS_CONFIG_TCM_PAGING
    if (!LIBOS_MEMORY_IS_PAGED_TCM(memory))
#endif
    {
        //
        //  BUG WAR: GA100 and TU10x return LACC faults when the MPU entry is valid
        //           for the requested access. This happens on PRI timeouts, ECC
        //           errors and other scenarios.  Later hardware adds a fault bit
        //           to distinguish.
        //
#if LWRISCV_VERSION < LWRISCV_2_0
        // Where was this cached entry last resident?
        LwU32 last_index = memory->attributes_and_size & LIBOS_MEMORY_MPU_INDEX_MASK;

        // Was this entry already in the MPU?
        csr_write(LW_RISCV_CSR_XMPUIDX, last_index);

        // @note We already verified access above, this is guaranteed to be
        //       a MEMERR in disguise.  Pass it up as a failure.
        if (csr_read(LW_RISCV_CSR_XMPUVA) == (memory->virtual_address | 1ULL))
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

        // If the target memory region overlaps WPR, we potentially need to
        // create up to 3 MPU mappings.
        LwU64 pa = memory->physical_address;
        LwU64 va = memory->virtual_address;
        LwU64 map_size = memory->attributes_and_size & ~LIBOS_MEMORY_ATTRIBUTE_MASK;

        // Pick a victim MPU entry(/entries)
        const LwU32 mpu_cached_entries_count = mpu_entry_count - LIBOS_MPU_INDEX_CACHED_FIRST;
        mpu_index = LIBOS_MPU_INDEX_CACHED_FIRST + (csr_read(LW_RISCV_CSR_CYCLE) % mpu_cached_entries_count);

#if LIBOS_CONFIG_WPR
        //  1. The region before WPR, with no WPR attribute set
        if (pa < wpr.start)
        {
            LwU64 submap_size = (wpr.start > (pa + map_size)) ? map_size
                                                              : wpr.start - pa;
            kernel_mmu_map_region(mpu_index++, va, pa, submap_size, mpu_attributes);
            pa += submap_size;
            va += submap_size;
            map_size -= submap_size;
            if (mpu_index == (LIBOS_MPU_INDEX_CACHED_FIRST + mpu_cached_entries_count))
                mpu_index = LIBOS_MPU_INDEX_CACHED_FIRST;
        }

        //  2. The region coinciding with WPR, with the WPR attribute set
        if ((map_size > 0) && (pa < wpr.end))
        {
            LwU64 submap_size = ((wpr.end + 1) > (pa + map_size)) ? map_size
                                                                  : (wpr.end + 1) - pa;
            kernel_mmu_map_region(mpu_index++, va, pa, submap_size,
                                  mpu_attributes | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, LIBOS_CONFIG_WPR));
            pa += submap_size;
            va += submap_size;
            map_size -= submap_size;
            if (mpu_index == (LIBOS_MPU_INDEX_CACHED_FIRST + mpu_cached_entries_count))
                mpu_index = LIBOS_MPU_INDEX_CACHED_FIRST;
        }
#elif LWRISCV_VERSION < LWRISCV_2_0
        // Update the last MPU index
        memory->attributes_and_size =
            (memory->attributes_and_size & ~LIBOS_MEMORY_MPU_INDEX_MASK) | mpu_index;
#endif

        //  3. The region after WPR, with no WPR attribute set
        if ((map_size > 0) && (pa > wpr.end))
        {
            kernel_mmu_map_region(mpu_index, va, pa, map_size, mpu_attributes);
        }

        // XXX SL TODO: this return value needs to be distinct from LIBOS_ILWALID_TCM_OFFSET since
        // this isn't an error case
        return 0;
    }

#if LIBOS_CONFIG_TCM_PAGING
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
        resident_page *page = container_of(i, resident_page, header);

        if ((page->fb_offset & PAGE_MASK) == translated_address)
        {
            mpu_index = (LwU32)to_uint64(page - &tcm_pages[0]);
            csr_write(LW_RISCV_CSR_XMPUIDX, widen(LIBOS_MPU_INDEX_PAGED_TCM_FIRST + mpu_index));
            csr_write(LW_RISCV_CSR_XMPUATTR, mpu_attributes);
            // XXX CB TODO: removing this causes test failures see kernel_mmu_clear_mpu() above
            csr_write(LW_RISCV_CSR_XMPUVA, xbadaddr | 1ULL);
            if (was_write)
            {
                page->fb_offset |= PAGE_DIRTY;
            }
            if (mpu_index < LIBOS_MPU_INDEX_PAGED_DTCM_COUNT)
            {
                return (LIBOS_TCM_INDEX_PAGED_DTCM_FIRST + (mpu_index - DMEM_PAGE_BASE_INDEX)) << LIBOS_MPU_PAGESHIFT;
            }
            else
            {
                return (LIBOS_TCM_INDEX_PAGED_ITCM_FIRST + (mpu_index - IMEM_PAGE_BASE_INDEX)) << LIBOS_MPU_PAGESHIFT;
            }
        }
    }

    // Pick a page to evict (poorly)
    LwU32 rand_index = (LwU32)csr_read(LW_RISCV_CSR_CYCLE);
    LwU32 offset;
    LwU64 pa;
    LwBool is_exec;

    // Adjust chosen index to reference TCM entry in correct pool
    if (LIBOS_MEMORY_IS_EXELWTE(memory))
    {
        mpu_index = IMEM_PAGE_BASE_INDEX + (rand_index % LIBOS_MPU_INDEX_PAGED_ITCM_COUNT);
        offset    = (LIBOS_TCM_INDEX_PAGED_ITCM_FIRST + (mpu_index - IMEM_PAGE_BASE_INDEX)) << LIBOS_MPU_PAGESHIFT;
        pa        = LW_RISCV_AMAP_IMEM_START | widen(offset);
        is_exec   = LW_TRUE;
    }
    else
    {
        mpu_index = DMEM_PAGE_BASE_INDEX + (rand_index % LIBOS_MPU_INDEX_PAGED_DTCM_COUNT);
        offset    = (LIBOS_TCM_INDEX_PAGED_DTCM_FIRST + (mpu_index - DMEM_PAGE_BASE_INDEX)) << LIBOS_MPU_PAGESHIFT;
        pa        = LW_RISCV_AMAP_DMEM_START | widen(offset);
        is_exec   = LW_FALSE;
    }

    // Remove this page from the tracking structure if needed
    if ((tcm_pages[mpu_index].fb_offset & PAGE_RESIDENT) != 0ULL)
    {
        kernel_paging_writeback(&tcm_pages[mpu_index]);
        resident_page_remove(&tcm_pages[mpu_index].header);
    }

    // Point the donor MPU entry at the bad VA and the dmem page
    csr_write(LW_RISCV_CSR_XMPUIDX, widen(LIBOS_MPU_INDEX_PAGED_TCM_FIRST + mpu_index));
    csr_write(LW_RISCV_CSR_XMPUATTR, mpu_attributes);
    csr_write(LW_RISCV_CSR_XMPUPA, pa);
    csr_write(LW_RISCV_CSR_XMPURNG, LIBOS_MPU_PAGESIZE);
    csr_write(LW_RISCV_CSR_XMPUVA, xbadaddr | 1ULL);

    if (!skip_read)
    {
        kernel_dma_start(
            translated_address, offset, LIBOS_MPU_PAGESIZE, LIBOS_DMAIDX_PHYS_VID_FN0, is_exec ? LIBOS_DMAFLAG_IMEM : 0);
    }

    // Perform the tracking structure updates while the DMA operation is finishing up
    // (renders this code essentially free)
    tcm_pages[mpu_index].fb_offset = translated_address | PAGE_RESIDENT | (was_write ? PAGE_DIRTY : 0ULL);
    resident_page_insert(resident_page_bucket(translated_address), &tcm_pages[mpu_index].header);

    kernel_dma_flush();

    return offset;
#endif // LIBOS_CONFIG_TCM_PAGING
}
