/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>

#include <dev_gsp.h>
#include <dev_fb.h>

#include <lwriscv/peregrine.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/manifest_gh100.h>

#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/shutdown.h>
#include <liblwriscv/mpu.h>
#include <liblwriscv/libc.h>

#include <riscvifriscv.h>
#include <rmRiscvUcode.h>
#include <mmu/mmucmn.h>


#include "misc.h"
#include "partitions.h"
#include "rpc.h"

#ifdef CC_GSPRM_BUILD
#include <bootstrap.h>
#include <gsp_fw_wpr_meta.h>
#include <elf.h>
#endif
#include "cc_rmproxy.h"

#define MPU_ATTR_RWX    REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SW, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SX, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UW, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UX, 1ULL)

#define MPU_ATTR_RX     REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SX, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UX, 1ULL)

#define MPU_ATTR_RW     REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SW, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UW, 1ULL)

#define MPU_ATTR_CACHE  REF_NUM64(LW_RISCV_CSR_SMPUATTR_CACHEABLE, 1u) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_COHERENT, 1u)

#define MPU_ATTR_WPR2   REF_NUM64(LW_RISCV_CSR_SMPUATTR_WPR, 2u)

/* Linker-defined symbols. See ldscript. */
extern LwU32 __imem_rm_start[];
extern LwU32 __imem_rm_size[];
extern LwU32 __imem_rm_section_start[];
extern LwU32 __imem_rm_section_end[];
extern LwU32 __dmem_rm_start[];
extern LwU32 __dmem_rm_size[];
extern LwU32 __dmem_rm_section_start[];
extern LwU32 __dmem_rm_section_end[];
extern LwU32 __dmem_print_start[];
extern LwU32 __dmem_print_size[];

static MpuContext partition_rm_mpu_ctx;

#define LW_MWPR_DEFAULT_ALIGNMENT           (0X1000)

#ifdef CC_GSPRM_BUILD
static LwBool libosElfGetBootEntry(elf64_header *elf, LwU64 *entry_offset)
{
    LwU64 entry_va = elf->entry;
    LwU32 i;
    for (i = 0; i < elf->phnum; i++)
    {
        elf64_phdr *phdr = (elf64_phdr *)((LwU8 *)elf + elf->phoff + elf->phentsize * i);
        if (entry_va >= phdr->vaddr && entry_va < (phdr->vaddr + phdr->memsz))
        {
            *entry_offset = entry_va - phdr->vaddr + phdr->offset;
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}
#endif

#ifdef CC_GSPRM_BUILD
// Setup and enable MPU with required identity mappings for bootstrap.
// We use the highest entries, which LibOS can override and then scrub.
static void init_gsprm_mpu(LwU64 dataBase, LwU32 dataSize)
{
    MpuContext *pCtx = &partition_rm_mpu_ctx;

    LwU32 mpuEntry = REF_VAL64(LW_RISCV_CSR_SMPUCTL_ENTRY_COUNT,
                               csr_read(LW_RISCV_CSR_SMPUCTL));

    // identity for sub-WPR data region (includes the entry point)
    if (dataSize > 0)
    {
        MpuHandle MpuSubWprHandle;
        mpuReserveEntry(pCtx, --mpuEntry, &MpuSubWprHandle);
        mpuWriteEntry(pCtx, MpuSubWprHandle, dataBase, dataBase, dataSize,
                      MPU_ATTR_CACHE |
                      MPU_ATTR_RWX   |
                      MPU_ATTR_WPR2);
        mpuEnableEntry(pCtx, MpuSubWprHandle);
    }

    // identity for imem region
    {
        MpuHandle MpuImemHandle;
        mpuReserveEntry(pCtx, --mpuEntry, &MpuImemHandle);
        mpuWriteEntry(pCtx, MpuImemHandle, LW_RISCV_AMAP_IMEM_START,
                      LW_RISCV_AMAP_IMEM_START, LW_RISCV_AMAP_IMEM_SIZE, MPU_ATTR_RWX);
        mpuEnableEntry(pCtx, MpuImemHandle);
    }

    // identity for dmem region
    {
        MpuHandle MpuDmemHandle;
        mpuReserveEntry(pCtx, --mpuEntry, &MpuDmemHandle);
        mpuWriteEntry(pCtx, MpuDmemHandle, LW_RISCV_AMAP_DMEM_START,
                      LW_RISCV_AMAP_DMEM_START, LW_RISCV_AMAP_DMEM_SIZE, MPU_ATTR_RW);
        mpuEnableEntry(pCtx, MpuDmemHandle);
    }

    // identity for INT I/O region (needed for prints)
    {
        MpuHandle MpuIntioHandle;
        mpuReserveEntry(pCtx, --mpuEntry, &MpuIntioHandle);
        mpuWriteEntry(pCtx, MpuIntioHandle, LW_RISCV_AMAP_INTIO_START,
                      LW_RISCV_AMAP_INTIO_START, LW_RISCV_AMAP_INTIO_SIZE, MPU_ATTR_RW);
        mpuEnableEntry(pCtx, MpuIntioHandle);
    }

    // identity for PRIV region (libos bootstrap needs to access some regs)
    {
        MpuHandle MpuPrivHandle;
        mpuReserveEntry(pCtx, --mpuEntry, &MpuPrivHandle);
        mpuWriteEntry(pCtx, MpuPrivHandle, LW_RISCV_AMAP_PRIV_START,
                      LW_RISCV_AMAP_PRIV_START, LW_RISCV_AMAP_PRIV_SIZE, MPU_ATTR_RW);
        mpuEnableEntry(pCtx, MpuPrivHandle);
    }
}
#else // CC_GSPRM_BUILD
static void init_rm_proxy_mpu(LwU64 codeBase, LwU32 codeSize, LwU64 dataBase, LwU32 dataSize)
{
    MpuContext *pCtx = &partition_rm_mpu_ctx;
    LwU32 entryIdx = 0;

    // code
    {
        MpuHandle handle;

        mpuReserveEntry(pCtx, entryIdx++, &handle);
        mpuWriteEntry(pCtx, handle, RMPROXY_CODE_VA_BASE, codeBase, codeSize,
                      MPU_ATTR_CACHE | MPU_ATTR_RX | MPU_ATTR_WPR2);
        mpuEnableEntry(pCtx, handle);
    }

    // data
    {
        MpuHandle handle;

        mpuReserveEntry(pCtx, entryIdx++, &handle);
        mpuWriteEntry(pCtx, handle, RMPROXY_DATA_VA_BASE, dataBase, dataSize,
                      MPU_ATTR_CACHE | MPU_ATTR_RW | MPU_ATTR_WPR2);
        mpuEnableEntry(pCtx, handle);
    }

    // Identity : imem and rest of address space
    // code, need write so proxy can clobber entry point
    {
        MpuHandle handle;

        mpuReserveEntry(pCtx, entryIdx++, &handle);
        mpuWriteEntry(pCtx, handle, LW_RISCV_AMAP_IMEM_START,
                      LW_RISCV_AMAP_IMEM_START, LW_RISCV_AMAP_IMEM_SIZE,
                      MPU_ATTR_RWX); // MK TODO: permission fix
        mpuEnableEntry(pCtx, handle);
    }

    // Rest of address space, use last entry
    {
        MpuHandle handle;
        LwU32 mpuLastEntry = (LwU32)REF_VAL64(LW_RISCV_CSR_SMPUCTL_ENTRY_COUNT,
                                   csr_read(LW_RISCV_CSR_SMPUCTL)) - 1;

        mpuReserveEntry(pCtx, mpuLastEntry, &handle);
        mpuWriteEntry(pCtx, handle, LW_RISCV_AMAP_DMEM_START,
                      LW_RISCV_AMAP_DMEM_START,
                      LW_RISCV_AMAP_EXTMEM3_END - LW_RISCV_AMAP_DMEM_START,
                      MPU_ATTR_CACHE | MPU_ATTR_RW);
        mpuEnableEntry(pCtx, handle);
    }
}
#endif // CC_GSPRM_BUILD

// C "entry" to partition, has stack and liblwriscv
GCC_ATTR_NORETURN void partition_rm_main(void)
{
    // The GSP-RM partition only uses the data sub-WPR for now, but proxy uses both
    LwU64 codeBase = (LwU64)REF_VAL(LW_PFB_PRI_MMU_FALCON_GSP_CFGA_ADDR_LO,
                                    priRead(LW_PFB_PRI_MMU_FALCON_GSP_CFGA(GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR2)));
    LwU64 codeEnd  = (LwU64)REF_VAL(LW_PFB_PRI_MMU_FALCON_GSP_CFGB_ADDR_HI,
                                    priRead(LW_PFB_PRI_MMU_FALCON_GSP_CFGB(GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR2)));

    LwU64 dataBase = (LwU64)REF_VAL(LW_PFB_PRI_MMU_FALCON_GSP_CFGA_ADDR_LO,
                                    priRead(LW_PFB_PRI_MMU_FALCON_GSP_CFGA(GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR2)));
#ifdef CC_GSPRM_BUILD
    LwU64 dataEnd  = (LwU64)REF_VAL(LW_PFB_PRI_MMU_FALCON_GSP_CFGB_ADDR_HI,
                                    priRead(LW_PFB_PRI_MMU_FALCON_GSP_CFGB(GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR2)));
#else
    LwU64 dataEnd  = (LwU64)REF_VAL(LW_PFB_PRI_MMU_WPR2_ADDR_HI_VAL,
                                    priRead(LW_PFB_PRI_MMU_WPR2_ADDR_HI)); // Assume data spans rest of WPR2
#endif

    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        ccPanic();
    }

    // Sanity checks on WPR
    if (dataBase >= dataEnd)
    {
        ccPanic();
    }

    // recompute bases
    codeBase   = codeBase << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;
    codeEnd    = codeEnd  << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;
    dataBase <<= LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;
    dataEnd  <<= LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;

    dataEnd  += LW_MWPR_DEFAULT_ALIGNMENT;
    codeEnd  += LW_MWPR_DEFAULT_ALIGNMENT;

    // Colwert FB offsets to risc-v PA
    codeBase += LW_RISCV_AMAP_FBGPA_START;
    codeEnd  += LW_RISCV_AMAP_FBGPA_START;
    dataBase += LW_RISCV_AMAP_FBGPA_START;
    dataEnd  += LW_RISCV_AMAP_FBGPA_START;

    mpuInit(&partition_rm_mpu_ctx);

#ifdef CC_GSPRM_BUILD
    init_gsprm_mpu(dataBase, (LwU32)(dataEnd - dataBase));
#else // CC_GSPRM_BUILD
    init_rm_proxy_mpu(codeBase, (LwU32)(codeEnd - codeBase), dataBase,
                      (LwU32)(dataEnd - dataBase));
#endif // CC_GSPRM_BUILD

    mpuEnable();

#ifdef CC_GSPRM_BUILD
    GspFwWprMeta *pWprMeta = (GspFwWprMeta *)dataBase;

    if ((pWprMeta->magic != GSP_FW_WPR_META_MAGIC) ||
        (pWprMeta->revision != GSP_FW_WPR_META_REVISION) ||
        (pWprMeta->verified != GSP_FW_WPR_META_VERIFIED))
    {
        ccPanic();
    }

    LwU64 partition_rpc_request_pa = (LwUPtr)rpcGetRequest();
    LwU64 partition_rpc_reply_pa   = (LwUPtr)rpcGetReply();

    // Patch the GspFwWprMeta with the partition RPC addresses
    // Verify that the request and reply structs are in the same MPU page
    pWprMeta->partitionRpcAddr = partition_rpc_request_pa & ~LW_RISCV_CSR_MPU_PAGE_MASK;
    if (pWprMeta->partitionRpcAddr != (partition_rpc_reply_pa & ~LW_RISCV_CSR_MPU_PAGE_MASK))
    {
        ccPanic();
    }

    pWprMeta->partitionRpcRequestOffset = partition_rpc_request_pa & LW_RISCV_CSR_MPU_PAGE_MASK;
    pWprMeta->partitionRpcReplyOffset   = partition_rpc_reply_pa & LW_RISCV_CSR_MPU_PAGE_MASK;

    LwU64 entryOffset;
    LwU64 elfPa = pWprMeta->gspFwOffset + LW_RISCV_AMAP_FBGPA_START;
    if (!libosElfGetBootEntry((elf64_header *)elfPa, &entryOffset))
    {
        ccPanic();
    }

    kernel_bootstrap_t bootstrap = (kernel_bootstrap_t)(elfPa + entryOffset);

    // Entry is expected to be within the sub-WPR
    if (((LwU64)bootstrap < dataBase) || ((LwU64)bootstrap >= dataEnd))
    {
        ccPanic();
    }

    // Fill in the bootstrap arguments for LibOS. We don't bother telling it
    // about our stack (at the end of dmem_init) since we set it up fresh on every
    // entry.
    RM_GSP_RM_CMD *pBootArgs = &rpcGetRequest()->rm;

    pBootArgs->imem_base_pa = (LwUPtr) __imem_rm_start;
    pBootArgs->imem_size    = (LwU32)  (((LwUPtr)__imem_rm_size) & 0xFFFFFFFFUL);
    pBootArgs->imem_bs_size = (LwU32)  (((LwUPtr)__imem_rm_section_end - (LwUPtr)__imem_rm_section_start) & 0xFFFFFFFFUL);
    pBootArgs->dmem_base_pa = (LwUPtr) __dmem_rm_start;
    pBootArgs->dmem_size    = (LwU32)  (((LwUPtr)__dmem_rm_size) & 0xFFFFFFFFUL);
    pBootArgs->dmem_bs_size = (LwU32)  (((LwUPtr)__dmem_rm_section_end - (LwUPtr)__dmem_rm_section_start) & 0xFFFFFFFFUL);

    bootstrap(pBootArgs);
#else
    // fb-based entry point pointer
    typedef GCC_ATTR_NORETURN void (*RmProxyBootstrap)(void *pRequest, void* pReply);

    RmProxyBootstrap bootstrap = (RmProxyBootstrap)RMPROXY_CODE_VA_BASE;

    bootstrap(rpcGetRequest(), rpcGetReply());
#endif
}


void partition_rm_trap(void)
{
    ccPanic();
}
