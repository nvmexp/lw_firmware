/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stddef.h>
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "lw_gsp_riscv_address_map.h"
#include "lwtypes.h"
#include "lwmisc.h"
#include "dev_gsp.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_fb.h"
#include "libos_init_args.h"
#include "config.h"
#include "lib/loader.h"
#if LIBOS_BOOTLOADER_CONFIG_SSP
#include "rand.h"
#include "ssp.h"
#endif // LIBOS_BOOTLOADER_CONFIG_SSP
#include "dev_top_addendum.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "kernel/bootstrap.h"
#include "bootloader.h"
#include "dma.h"
#include "gsp_fw_wpr_meta.h"

// Use to verify address before adding LW_RISCV_AMAP_xyz_START.
#define OFFSET_WITHIN_APERTURE(addr_offset, aperture) ((addr_offset) < ((LW_RISCV_AMAP_##aperture##_END) - (LW_RISCV_AMAP_##aperture##_START)))

static inline LwU32 bar0Read(LwU32 priAddr)
{
    return *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + priAddr);
}

static LwBool isValidPrivSecFuse(void)
{
    // Production check only.
    if (FLD_TEST_DRF(_PGSP, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, bar0Read(LW_PGSP_SCP_CTL_STAT)))
    {
        if (FLD_TEST_DRF(_FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, bar0Read(LW_FUSE_OPT_PRIV_SEC_EN)))
        {
            // Error: Priv sec is not enabled
            return LW_FALSE;
        }
    }
    return LW_TRUE;
}

#if LIBOS_BOOTLOADER_CONFIG_SIGNED_BOOT && !LIBOS_BOOTLOADER_CONFIG_USES_BOOTER
static LwBool isUnsignedUcodeAllowed(void)
{
    if (!FLD_TEST_DRF(_PGSP, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, bar0Read(LW_PGSP_SCP_CTL_STAT)))
    {
        // Unsigned ucode is allowed for debug boards
        return LW_TRUE;
    }

    if (FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_19_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _ENABLE,
        bar0Read(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_19_PRIV_LEVEL_MASK)))
    {
        if (bar0Read(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_19) != 0)
        {
            return LW_FALSE;
        }
    }

    return FLD_TEST_DRF(_PTOP, _SCRATCH0, _HYPERVISOR_WRITE_PROTECT, _ALLOW_UNSIGNED,
        bar0Read(LW_PTOP_SCRATCH0));
}
#endif // LIBOS_BOOTLOADER_CONFIG_SIGNED_BOOT && !LIBOS_BOOTLOADER_CONFIG_USES_BOOTER

#if LIBOS_BOOTLOADER_CONFIG_USE_INIT_ARGS
static LibosMemoryRegionInitArgument *initArgsFindFirst
(
    LibosMemoryRegionInitArgument *initArgs,
    LwU64 id8
)
{
    LwU64 initArgsAddr = (LwU64) initArgs;

    // The init args should be within a 4K page.
    LwU64 nextPageStart = (initArgsAddr + 4096) & (~4095);
    int entries = (nextPageStart - initArgsAddr) / sizeof(LibosMemoryRegionInitArgument);
    int i;
    for (i = 0; i < entries; i++)
    {
        if (initArgs[i].id8 == 0)
        {
            // Reached the end of the array.
            return NULL;
        }
        else if (initArgs[i].id8 == id8)
        {
            return initArgs + i;
        }
    }

    // The init args do not stop at 4K page boundary.
    halt();
}
#endif

#if LIBOS_BOOTLOADER_CONFIG_COPY_UCODE
LwU64 pageByAddress(LwU64 * * * pde2, LwU64 va)
{
    LwU64 entriesLog2 = LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2 - 3;
    LwU64 pageMask = LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE - 1;
    LwU64 idxMask = (LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE  / sizeof(LwU64)) - 1;
    LwU64 tmpOff;

    if (va >= ((LwU64)LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE << (3 * entriesLog2)))
        halt();

    tmpOff = (LwU64)pde2[(va >> (2 * entriesLog2 + LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2)) & idxMask];
    if (!OFFSET_WITHIN_APERTURE(tmpOff, SYSGPA))
        halt();

    LwU64 * *  pde1 = (LwU64 * *)(tmpOff + LW_RISCV_AMAP_SYSGPA_START);

    tmpOff = (LwU64)pde1[(va >> (1 * entriesLog2 + LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2)) & idxMask];
    if (!OFFSET_WITHIN_APERTURE(tmpOff, SYSGPA))
        halt();

    LwU64 * pde0 = (LwU64 *)(tmpOff + LW_RISCV_AMAP_SYSGPA_START);

    LwU64 pte = (LwU64)pde0[(va >> (0 * entriesLog2 + LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2)) & idxMask];
    if (!OFFSET_WITHIN_APERTURE(pte, SYSGPA))
        halt();

    // Recompute offset.
    // Returns offset in aperture (without adding LW_RISCV_AMAP_SYSGPA_START).
    return     pte + (va & pageMask);
}
#endif

static inline void csr_write(LwU64 csr, LwU64 value)
{
    __asm__ volatile("csrw %0, %1" ::"i"(csr), "r"(value));
}

static inline void csr_set(LwU64 csr, LwU64 value)
{
    __asm__ volatile("csrs %0, %1" ::"i"(csr), "r"(value));
}

static inline void csr_clear(LwU64 csr, LwU64 value)
{
    __asm__ volatile("csrc %0, %1" ::"i"(csr), "r"(value));
}

#if LIBOS_BOOTLOADER_CONFIG_COPY_UCODE
void mpu_enable_identity()
{
    // Enable the MPU
#if LIBOS_BOOTLOADER_CONFIG_RISCV_S_MODE
    // Program MPU entry 0 as an identity map
    csr_write(LW_RISCV_CSR_SMPUIDX, 0);
    csr_write(LW_RISCV_CSR_SMPURNG, 0xFFFFFFFFFFFFF000ULL);
    csr_write(LW_RISCV_CSR_SMPUPA, 0);
    csr_write(
        LW_RISCV_CSR_SMPUATTR,
        REF_NUM64(LW_RISCV_CSR_SMPUATTR_CACHEABLE, 1u) |    /* cache all accesses in L1D$ */
        REF_NUM64(LW_RISCV_CSR_SMPUATTR_COHERENT, 1u) |     /* snoop CPU cache */
        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SW, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SX, 1ULL)
    );
    csr_write(LW_RISCV_CSR_SMPUVA, 1 /* address=0 with enable bit set */);
    csr_set(LW_RISCV_CSR_SATP, REF_DEF64(LW_RISCV_CSR_SATP_MODE, _LWMPU));
#else
    // Program MPU entry 0 as an identity map
    csr_write(LW_RISCV_CSR_MMPUIDX, 0);
    csr_write(LW_RISCV_CSR_MMPURNG, 0xFFFFFFFFFFFFF000ULL);
    csr_write(LW_RISCV_CSR_MMPUPA, 0);
    csr_write(
        LW_RISCV_CSR_MMPUATTR,
        REF_NUM64(LW_RISCV_CSR_MMPUATTR_CACHEABLE, 1u) |    /* cache all accesses in L1D$ */
        REF_NUM64(LW_RISCV_CSR_MMPUATTR_COHERENT, 1u) |     /* snoop CPU cache */
        REF_NUM64(LW_RISCV_CSR_MMPUATTR_MR, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_MMPUATTR_MW, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_MMPUATTR_MX, 1ULL)
    );
    csr_write(LW_RISCV_CSR_MMPUVA, 1 /* address=0 with enable bit set */);
    csr_set(LW_RISCV_CSR_MSTATUS, REF_DEF64(LW_RISCV_CSR_MSTATUS_VM, _LWMPU));
#endif
}

void mpu_disable_identity()
{
#if LIBOS_BOOTLOADER_CONFIG_RISCV_S_MODE
    csr_clear(LW_RISCV_CSR_SATP, REF_DEF64(LW_RISCV_CSR_SATP_MODE, _LWMPU));
#else
    csr_clear(LW_RISCV_CSR_MSTATUS, REF_DEF64(LW_RISCV_CSR_MSTATUS_VM, _LWMPU));
#endif
}
#endif // LIBOS_BOOTLOADER_CONFIG_COPY_UCODE

__attribute__((noreturn)) void main()
{
#if LIBOS_BOOTLOADER_CONFIG_RISCV_S_MODE
    // Halt on any interrupt.
    __asm(
        "1:auipc t0, %%pcrel_hi(halt);"
        "addi t0, t0, %%pcrel_lo(1b);"
        "csrw stvec, t0;"
        ::: "t0"
    );
#endif // LIBOS_BOOTLOADER_CONFIG_RISCV_S_MODE

#if LIBOS_BOOTLOADER_CONFIG_SSP
    // The main function is noreturn so it's safe to set the stack canary inside it.
    SETUP_STACK_CANARY(rand64());
#endif // LIBOS_BOOTLOADER_CONFIG_SSP

    if (!isValidPrivSecFuse())
    {
        halt();
    }

#if LIBOS_BOOTLOADER_CONFIG_SIGNED_BOOT && !LIBOS_BOOTLOADER_CONFIG_USES_BOOTER
    if (!isUnsignedUcodeAllowed())
    {
        halt();
    }
#endif // LIBOS_BOOTLOADER_CONFIG_SIGNED_BOOT && !LIBOS_BOOTLOADER_CONFIG_USES_BOOTER

#if LIBOS_BOOTLOADER_CONFIG_USES_BOOTER
    // Halt if booter binary did not successfully complete.
    if (!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_DONE,
        bar0Read(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14)))
    {
        halt();
    }
#endif // LIBOS_BOOTLOADER_CONFIG_USES_BOOTER

#if LIBOS_BOOTLOADER_CONFIG_COPY_UCODE
    mpu_enable_identity();
#endif

    dma_init();

#if LIBOS_BOOTLOADER_CONFIG_USE_INIT_ARGS
    LwU32 mailbox0 = bar0Read(LW_PGSP_FALCON_MAILBOX0);
    LwU32 mailbox1 = bar0Read(LW_PGSP_FALCON_MAILBOX1);
    LwU64 mailbox  = (((LwU64) mailbox1) << 32) | mailbox0;

    if (!OFFSET_WITHIN_APERTURE(mailbox, SYSGPA))
    {
        // The init args should be in SYSCOH.
        halt();
    }

    LibosMemoryRegionInitArgument * initArgs =
        (LibosMemoryRegionInitArgument *) (mailbox + LW_RISCV_AMAP_SYSGPA_START);

    // GspFwWprMeta
    LibosMemoryRegionInitArgument * metadata = initArgsFindFirst(initArgs, LwU32_BUILD('W', 'P', 'R', 'M'));

    if ((metadata == NULL) ||
        (metadata->loc != LIBOS_MEMORY_REGION_LOC_FB) ||
        (metadata->kind != LIBOS_MEMORY_REGION_CONTIGUOUS))
    {
        halt();
    }

    GspFwWprMeta *pWprMeta = (GspFwWprMeta *)(metadata->pa + LW_RISCV_AMAP_FBGPA_START);
    LwU64 gspFwWprStart = pWprMeta->gspFwWprStart;
    LwU64 gspFwWprEnd = pWprMeta->gspFwWprEnd;
#else
    //
    // Notes that should have been in the hw manual:
    //
    // Bits 0x1F in LW_PFB_PRI_MMU_WPR2_ADDR_LO_VAL and _HI_VAL are wired to 0.
    // The resulting address is always 128KB (0x20000) aligned.
    //
    // _HI_VAL points to the start of the last 128KB block in the WPR, so we
    // need to add 0x20000 to get the last address of WPR (+1).
    //
    LwU32 data = bar0Read(LW_PFB_PRI_MMU_WPR2_ADDR_LO);
    LwU64 gspFwWprStart = ((LwU64)DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, data) <<
            LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);

    data = bar0Read(LW_PFB_PRI_MMU_WPR2_ADDR_HI);
    LwU64 gspFwWprEnd = ((LwU64)DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, data) <<
            LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT) + 0x20000;

    GspFwWprMeta *pWprMeta = (GspFwWprMeta *)(gspFwWprStart + LW_RISCV_AMAP_FBGPA_START);

    //
    // Booter checks the WPR meta data, so we should never get here if something
    // is bad.  But double check out of paranoia.
    //
    if ((pWprMeta->verified != GSP_FW_WPR_META_VERIFIED) ||
        (pWprMeta->revision != GSP_FW_WPR_META_REVISION) ||
        (pWprMeta->magic != GSP_FW_WPR_META_MAGIC)       ||
        !OFFSET_WITHIN_APERTURE(gspFwWprStart, FBGPA)    ||
        !OFFSET_WITHIN_APERTURE(gspFwWprEnd, FBGPA))
    {
        halt();
    }
#endif

    // Confirm that bootloader and GSP-RM are both within WPR
    if ((pWprMeta->bootBinOffset < gspFwWprStart)                            ||
        (pWprMeta->bootBinOffset + pWprMeta->sizeOfBootloader > gspFwWprEnd) ||
        (pWprMeta->gspFwOffset < pWprMeta->gspFwWprStart)                    ||
        (pWprMeta->gspFwOffset + pWprMeta->sizeOfRadix3Elf > gspFwWprEnd))
    {
        halt();
    }

    // Confirm we are lwrrently running inside the boot bin area, but only if
    // we are running from FB.  The check won't work if we are running from DMEM.
    LwU64 mailwA = (LwU64)main;

    if (mailwA > LW_RISCV_AMAP_FBGPA_START &&
        mailwA < LW_RISCV_AMAP_FBGPA_END)
    {
        // Running from FB.
        LwU64 mainPA = mailwA - LW_RISCV_AMAP_FBGPA_START;
        if (mainPA < pWprMeta->bootBinOffset ||
            mainPA > pWprMeta->bootBinOffset + pWprMeta->sizeOfBootloader)
        {
            halt();
        }
    }

    // Boot count
    LwBool bFirstBoot = (pWprMeta->bootCount == 0);
    pWprMeta->bootCount++;

    // GSP firmware runspace
    LwU64 runspacePa, runspaceOffset, runspaceSize;
    runspaceOffset = pWprMeta->gspFwOffset;
    runspaceSize = pWprMeta->sizeOfRadix3Elf;
    runspacePa = runspaceOffset + LW_RISCV_AMAP_FBGPA_START;

#if LIBOS_BOOTLOADER_CONFIG_COPY_UCODE
    // TODO - Move this code to BOOTER
    // Optional source image to copy into the runspace below
    if (bFirstBoot && (pWprMeta->sysmemAddrOfRadix3Elf != 0))
    {
        // Ensure the image is radix encoded an in sysmem
        if (!OFFSET_WITHIN_APERTURE(pWprMeta->sysmemAddrOfRadix3Elf, SYSGPA))
        {
            halt();
        }
        LwU64 imagePa = pWprMeta->sysmemAddrOfRadix3Elf + LW_RISCV_AMAP_SYSGPA_START;

        // Copy the image into the runspace
        for (LwU64 offset = 0; offset < pWprMeta->sizeOfRadix3Elf; offset += LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE)
        {
            // Translate the address
            LwU64 sourceOffset = pageByAddress((LwU64 ***)imagePa /* radix3 table */, offset);

            // Read data into TCM
            dma_copy(sourceOffset, 8192 /* skip SK/stack */, 4096, DMAIDX_PHYS_SYS_COH_FN0, DMAFLAG_READ);

            dma_flush();

            // Write data from TCM
            dma_copy(runspaceOffset + offset, 8192 /* skip SK/stack */, 4096, DMAIDX_PHYS_VID_FN0, DMAFLAG_WRITE);

            dma_flush();
        }

        __asm(
            "fence rw,rw;" ::: "memory"
        );
    }
#endif

    LwU64 entryOffset;
    if (!libosElfGetBootEntry((elf64_header *)runspacePa, &entryOffset))
    {
        halt();
    }

    // Make sure entryOffset is within the firmware.
    if (entryOffset >= runspaceSize)
    {
        halt();
    }

    kernel_bootstrap_t bootstrap = (kernel_bootstrap_t) (runspacePa + entryOffset);

#if LIBOS_BOOTLOADER_CONFIG_COPY_UCODE
    mpu_disable_identity();
#endif

    libos_bootstrap_args_t boot_args;
    boot_args.imem_base_pa = LW_RISCV_AMAP_IMEM_START + LIBOS_BOOTLOADER_CONFIG_IMEM_OFFSET;
    boot_args.imem_size    = LIBOS_BOOTLOADER_CONFIG_IMEM_SIZE;
    boot_args.imem_bs_size = 0;
    boot_args.dmem_base_pa = LW_RISCV_AMAP_DMEM_START + LIBOS_BOOTLOADER_CONFIG_DMEM_OFFSET;
    boot_args.dmem_size    = LIBOS_BOOTLOADER_CONFIG_DMEM_SIZE;
    boot_args.dmem_bs_size = 0;

    bootstrap(&boot_args);

    halt();
}

__attribute__((noreturn, aligned(4))) void halt(void)
{
    for (;;)
    {
#if LIBOS_BOOTLOADER_CONFIG_LWRISCV_SEPKERN
        // TU10X and GA100 have no way to shutdown the core.
        register LwU64 sbi_extension_id __asm__("a7") = 8;
        register LwU64 sbi_function_id  __asm__("a6") = 0;
        __asm__ __volatile__("ecall"  :: "r"(sbi_extension_id), "r"(sbi_function_id));
#endif // LIBOS_BOOTLOADER_CONFIG_LWRISCV_SEPKERN
    }
}
