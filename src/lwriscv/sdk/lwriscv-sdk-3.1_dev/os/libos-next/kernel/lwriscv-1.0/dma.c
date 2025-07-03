/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dma.h"
#include "kernel.h"
#include "paging.h"
#include "libriscv.h"
#include "../include/libos.h"
#include <dev_gsp.h>

/*!
 *  The LWRISCV 1.x DMA engine is only supported on LIBOS for GSP.
 *  This was done to simplify porting to support the new RISCV local registers (LW_PRGNLCL_*)
 */


/*!
 * @brief Initialize dma engine.
 */
void LIBOS_SECTION_TEXT_COLD kernel_dma_init(void)
{
    LwU32 reg;

    //
    // Configure DMA engine
    //
    reg = REF_DEF(LW_PGSP_FALCON_DMACTL_REQUIRE_CTX, _FALSE);
    KernelMmioWrite(LW_PGSP_FALCON_DMACTL, reg);

    //
    // Pre-program the DMA engine for physical FB transactions
    // (to minimize FB fault time).
    //
    reg =
        (REF_DEF(LW_PGSP_FBIF_CTL_ENABLE, _TRUE) | REF_DEF(LW_PGSP_FBIF_CTL_ALLOW_PHYS_NO_CTX, _ALLOW));
    fbif_write(LW_PGSP_FBIF_CTL, reg);

    // setup vidmem aperture access
    reg =
        (REF_DEF(LW_PGSP_FBIF_TRANSCFG_TARGET, _LOCAL_FB) |
         REF_DEF(LW_PGSP_FBIF_TRANSCFG_MEM_TYPE, _PHYSICAL) |
         REF_DEF(LW_PGSP_FBIF_TRANSCFG_ENGINE_ID_FLAG, _BAR2_FN0) |
         REF_DEF(LW_PGSP_FBIF_TRANSCFG_L2C_WR, _L2_EVICT_NORMAL) |
         REF_DEF(LW_PGSP_FBIF_TRANSCFG_L2C_RD, _L2_EVICT_NORMAL));
    fbif_write(LW_PGSP_FBIF_TRANSCFG(LIBOS_DMAIDX_PHYS_VID_FN0), reg);

    // setup coherent sysmem aperture access
    reg =
        (REF_DEF(LW_PGSP_FBIF_TRANSCFG_TARGET, _COHERENT_SYSMEM) |
         REF_DEF(LW_PGSP_FBIF_TRANSCFG_MEM_TYPE, _PHYSICAL) |
         REF_DEF(LW_PGSP_FBIF_TRANSCFG_ENGINE_ID_FLAG, _BAR2_FN0));
    fbif_write(LW_PGSP_FBIF_TRANSCFG(LIBOS_DMAIDX_PHYS_SYS_COH_FN0), reg);

    return;
}

// @todo Place this array in DMEM
// @note Technically IMEM and WRITE is forbidden
static const LwU32 LIBOS_SECTION_DMEM_PINNED(dma_cmd_by_flag)[] = {
    // !WRITE !IMEM
    (REF_DEF(LW_PGSP_FALCON_DMATRFCMD_SET_DMTAG, _TRUE)),

    // WRITE !IMEM
    (REF_DEF(LW_PGSP_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
     REF_DEF(LW_PGSP_FALCON_DMATRFCMD_WRITE, _TRUE)),

    // !WRITE IMEM
    (REF_DEF(LW_PGSP_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
     REF_DEF(LW_PGSP_FALCON_DMATRFCMD_IMEM, _TRUE)),

    // WRITE IMEM
    (REF_DEF(LW_PGSP_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
     REF_DEF(LW_PGSP_FALCON_DMATRFCMD_IMEM, _TRUE) | REF_DEF(LW_PGSP_FALCON_DMATRFCMD_WRITE, _TRUE)),
};

/*!
 * kernel_dma_start
 *
 * Extended version of kernel_dma_start that handles sysmem and any combination
 * of sizes and alignments.
 *
 * This routine uses the optimal size for each transfer.  For example it will
 * break down a DMA of 0x1C8 bytes from 0x78 to 0x78 into the following xfers:
 *    0x78 to  0x78 for     8 bytes
 *    0x80 to  0x80 for  0x80 bytes
 *   0x100 to 0x100 for 0x100 bytes
 *   0x200 to 0x200 for  0x40 bytes
 *
 * As shown above, when the last two digits of the addresses match, this
 * routine can get to larger xfer sizes in very few steps.
 *
 * This routine will also work when the last two digits don't match, but the
 * DMA can be very inefficient.  In the pathological case, where the addresses
 * are off by 1 (0x34 and 0x35, for example), the DMA is broken down into single
 * byte transfers for the entire buffer.
 *
 * In any case, the addresses and length should be 8-byte aligned.  We have
 * seen overwrites when using smaller alignments.
 *
 * @param[in]   phys_addr     Physical address to DMA to/from
 * @param[out]  tcm_offset    Area in TCM that holds/will hold the data
 * @param[in]   length        Length to DMA in bytes
 * @param[in]   dma_idx       LIBOS_DMAIDX_<xyz>
 * @param[in]   is_write      Indicates if we are writing to fb_phys_addr
 * @param[in]   is_exec       Indicates if transfer ilwolves exelwtable (text) memory
 */
#define LIBOS_DMA_BLOCK_SIZE U(256)

LIBOS_SECTION_IMEM_PINNED void
kernel_dma_start(LwU64 phys_addr, LwU32 tcm_offset, LwU32 length, LwU32 dma_idx, LwU32 dma_flags)
{
    LwU32 fb_offset = (LwU32)(phys_addr & 0xff);
    LwU32 dma_cmd;
    LwU32 dma_cmd_size;
    LwU32 xfer_size;

    phys_addr >>= 8u;
    KernelMmioWrite(LW_PGSP_FALCON_DMATRFBASE, LO32(phys_addr));
    KernelMmioWrite(LW_PGSP_FALCON_DMATRFBASE1, HI32(phys_addr));

    dma_cmd = dma_cmd_by_flag[dma_flags] | REF_NUM(LW_PGSP_FALCON_DMATRFCMD_CTXDMA, dma_idx);

    while (length != 0)
    {
        // Get MSB of size remaining to xfer, but not more than 0x100.
        if (length >= LIBOS_DMA_BLOCK_SIZE)
        {
            xfer_size = LIBOS_DMA_BLOCK_SIZE;
        }
        else
        {
            // Find the top-most bit set in the length (guaranteed to be lower than bit 8)
            xfer_size = length;
            xfer_size |= (xfer_size >> 1);
            xfer_size |= (xfer_size >> 2);
            xfer_size |= (xfer_size >> 4);
            xfer_size = (xfer_size + 1) >> 1;
        }

        // or in the pointers
        xfer_size |= tcm_offset | fb_offset;

        // Find the mask of the bottom most bit -- the length of the maximum copy we're allowed
        xfer_size ^= xfer_size & (xfer_size - 1);

        // Get LW_PGSP_FALCON_DMATRFCMD_SIZE_nnnB from xfer_size.
        dma_cmd_size = Log2OfPowerOf2(xfer_size);

        // Wait if DMA queue is full.
        while ((KernelMmioRead(LW_PGSP_FALCON_DMATRFCMD) & DRF_SHIFTMASK(LW_PGSP_FALCON_DMATRFCMD_FULL)) !=
               0u)
        {
            ;
        }

        KernelMmioWrite(LW_PGSP_FALCON_DMATRFMOFFS, tcm_offset);
        KernelMmioWrite(LW_PGSP_FALCON_DMATRFFBOFFS, fb_offset);

        dma_cmd_size = REF_NUM(LW_PGSP_FALCON_DMATRFCMD_SIZE, (dma_cmd_size - 2));
        KernelMmioWrite(LW_PGSP_FALCON_DMATRFCMD, dma_cmd | dma_cmd_size);

        tcm_offset += xfer_size;
        fb_offset += xfer_size;
        length -= xfer_size;
    }
}

#if LIBOS_FEATURE_PAGING
/*!
 * @brief Initiate dma transfer from dma buffer to/from TCM
 *
 * param[in] tcm_va         virtual address of tcm buffer
 * param[in] dma_va         virtual address of dma buffer
 * param[in] size           size to transfer in bytes; must be 4 byte aligned
 * param[in] from_tcm       copy from TCM to dma buffer
 *
 * Possible status values returned:
 *   LibosErrorArgument
 *   LibosErrorFailed
 *   LibosOk
 */
LibosStatus LIBOS_SECTION_IMEM_PINNED kernel_syscall_dma_copy(LwU64 tcm_va, LwU64 dma_va, LwU64 size, LwU8 from_tcm)
{
    libos_pagetable_entry_t *dma_memory, *tcm_memory;
    LwU64 dma_pa;
    LwU32 dma_idx;
    LwU64 start_page, end_page;
    LwU32 start_size, end_size; // Transfer sizes for start and end pages.
    LwU32 page_size;
    LwU32 tcm_offset;
    LwU64 i;
    LwBool skip_read;

    // GSP DMA requires 4 byte alignment for size and addresses.
    if ((dma_va | tcm_va | size) & 3)
        return LibosErrorAccess;

    // Validate appropriate access and size for DMA memory
    dma_memory = KernelValidateMemoryAccessOrReturn(dma_va, size, from_tcm);

    // Validate appropriate access and size for PAGED_TCM memory
    tcm_memory = KernelValidateMemoryAccessOrReturn(tcm_va, size, !from_tcm);

    // callwlate physical address of dma source buffer
    dma_pa = (dma_va - dma_memory->virtual_address) + dma_memory->physical_address;

    if (!LIBOS_MEMORY_IS_PAGED_TCM(tcm_memory) || LIBOS_MEMORY_IS_EXELWTE(tcm_memory))
        return LibosErrorAccess;

    // determine dma aperture index
    if (dma_memory->physical_address >= LW_RISCV_AMAP_SYSGPA_START)
    {
        dma_idx = LIBOS_DMAIDX_PHYS_SYS_COH_FN0;
    }
    else
    {
        dma_idx = LIBOS_DMAIDX_PHYS_VID_FN0;
    }

    start_page = tcm_va >> 12;
    end_page   = (tcm_va + size - 1) >> 12;

    if (start_page != end_page)
    {
        start_size = 0x1000 - (LwU32)(tcm_va & 0xFFF);
        end_size   = (LwU32)((tcm_va + size - 1) & 0xFFF) + 1;
    }
    else
    {
        start_size = size;
        end_size   = size;
    }

    for (i = start_page; i < end_page + 1; i++)
    {
        if (i == start_page)
        {
            page_size = start_size;
        }
        else if (i == end_page)
        {
            page_size = end_size;
        }
        else
        {
            page_size = 4096;
        }

        // PagingPageIn does not need to read if we are about to overwrite the entire page.
        skip_read = (!from_tcm && (page_size == 4096));

        // @note This operation is guaranteed to succeed because of prior validation of descriptor
        // coverage
        tcm_offset = PagingPageIn(
            KernelSchedulerGetTask()->pasid, i << 12,
            from_tcm ? LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT : LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT,
            skip_read);

        tcm_offset |= (tcm_va & 0xFFF);

        kernel_dma_start(dma_pa, tcm_offset, page_size, dma_idx, from_tcm ? LIBOS_DMAFLAG_WRITE : 0);

        dma_pa += page_size;
        tcm_va += page_size;
    }

    KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_A0] = LibosOk;
    KernelTaskReturn();
}
#endif

/*!
 *  @brief Wait for completion of remaining dma transfers (if any)
 */
LIBOS_SECTION_IMEM_PINNED void kernel_syscall_dma_flush(void)
{
    while ((KernelMmioRead(LW_PGSP_FALCON_DMATRFCMD) & DRF_SHIFTMASK(LW_PGSP_FALCON_DMATRFCMD_IDLE)) ==
           LW_PGSP_FALCON_DMATRFCMD_IDLE_FALSE)
    {
        ;
    }
}
