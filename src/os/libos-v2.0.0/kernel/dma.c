/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"

// TCM paging support is required to use the legacy DMA driver
#if LIBOS_CONFIG_TCM_PAGING
#include "dma.h"
#include "mmu.h"
#include "paging.h"
#include "riscv-isa.h"

#include "../include/libos.h"

#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"
#include "dev_riscv_pri.h"
#include "lw_gsp_riscv_address_map.h"

/*!
 * @brief Initialize dma engine.
 */
void LIBOS_SECTION_TEXT_COLD kernel_dma_init(void)
{
    LwU32 reg;

    // We're mapping peregrine and falcon using the same mapping
    _Static_assert(LW_FALCON_GSP_BASE + PEREGRINE_OFFSET == LW_FALCON2_GSP_BASE, "Invalid register layout");

    //
    // Configure DMA engine
    //
    reg = REF_DEF(LW_PFALCON_FALCON_DMACTL_REQUIRE_CTX, _FALSE);
    falcon_write(LW_PFALCON_FALCON_DMACTL, reg);

    //
    // Pre-program the DMA engine for physical FB transactions
    // (to minimize FB fault time).
    //
    reg =
        (REF_DEF(LW_PFALCON_FBIF_CTL_ENABLE, _TRUE) | REF_DEF(LW_PFALCON_FBIF_CTL_ALLOW_PHYS_NO_CTX, _ALLOW));
    fbif_write(LW_PFALCON_FBIF_CTL, reg);

    // setup vidmem aperture access
    reg =
        (REF_DEF(LW_PFALCON_FBIF_TRANSCFG_TARGET, _LOCAL_FB) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE, _PHYSICAL) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_ENGINE_ID_FLAG, _BAR2_FN0) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_L2C_WR, _L2_EVICT_NORMAL) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_L2C_RD, _L2_EVICT_NORMAL));
    fbif_write(LW_PFALCON_FBIF_TRANSCFG(LIBOS_DMAIDX_PHYS_VID_FN0), reg);

    // setup coherent sysmem aperture access
    reg =
        (REF_DEF(LW_PFALCON_FBIF_TRANSCFG_TARGET, _COHERENT_SYSMEM) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE, _PHYSICAL) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_ENGINE_ID_FLAG, _BAR2_FN0));
    fbif_write(LW_PFALCON_FBIF_TRANSCFG(LIBOS_DMAIDX_PHYS_SYS_COH_FN0), reg);

    return;
}

// @todo Place this array in DMEM
// @note Technically IMEM and WRITE is forbidden
static const LwU32 LIBOS_SECTION_DMEM_PINNED(dma_cmd_by_flag)[] = {
    // !WRITE !IMEM
    (REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG, _TRUE)),

    // WRITE !IMEM
    (REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
     REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_WRITE, _TRUE)),

    // !WRITE IMEM
    (REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
     REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_IMEM, _TRUE)),

    // WRITE IMEM
    (REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
     REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_IMEM, _TRUE) | REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_WRITE, _TRUE)),
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
    falcon_write(LW_PFALCON_FALCON_DMATRFBASE, LO32(phys_addr));
    falcon_write(LW_PFALCON_FALCON_DMATRFBASE1, HI32(phys_addr));

    dma_cmd = dma_cmd_by_flag[dma_flags] | REF_NUM(LW_PFALCON_FALCON_DMATRFCMD_CTXDMA, dma_idx);

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

        // Get LW_PFALCON_FALCON_DMATRFCMD_SIZE_nnnB from xfer_size.
        dma_cmd_size = pow2_to_log(xfer_size);

        // Wait if DMA queue is full.
        while ((falcon_read(LW_PFALCON_FALCON_DMATRFCMD) & REF_SHIFTMASK(LW_PFALCON_FALCON_DMATRFCMD_FULL)) !=
               0u)
        {
            ;
        }

        falcon_write(LW_PFALCON_FALCON_DMATRFMOFFS, tcm_offset);
        falcon_write(LW_PFALCON_FALCON_DMATRFFBOFFS, fb_offset);

        dma_cmd_size = REF_NUM(LW_PFALCON_FALCON_DMATRFCMD_SIZE, (dma_cmd_size - 2));
        falcon_write(LW_PFALCON_FALCON_DMATRFCMD, dma_cmd | dma_cmd_size);

        tcm_offset += xfer_size;
        fb_offset += xfer_size;
        length -= xfer_size;
    }
}

void LIBOS_SECTION_IMEM_PINNED kernel_dma_flush(void)
{
    LwU32 reg;
    do
    {
        reg = falcon_read(LW_PFALCON_FALCON_DMATRFCMD);
    } while (((reg & REF_SHIFTMASK(LW_PFALCON_FALCON_DMATRFCMD_IDLE)) ==
                REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_IDLE, _FALSE)) &&
             ((reg & REF_SHIFTMASK(LW_PFALCON_FALCON_DMATRFCMD_ERROR)) !=
                REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_ERROR, _TRUE)));
}

static void LIBOS_SECTION_IMEM_PINNED
kernel_dma_xfer
(
    libos_pagetable_entry_t *tcm_memory,
    LwU64 tcm_va,
    libos_pagetable_entry_t *dma_memory,
    LwU64 dma_va,
    LwU32 size,
    LwBool from_tcm
)
{
    LwU64 i;
    LwU64 mcause = from_tcm ? LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT : LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT;
    LwU64 write_flag = from_tcm ? LIBOS_DMAFLAG_WRITE : 0;
    LwU64 dma_pa;
    LwU64 start_page, end_page;
    LwU32 start_size, end_size; // Transfer sizes for start and end pages.
    LwU32 page_size;
    LwU32 tcm_offset;
    LwU32 dma_idx;
    LwBool skip_read;

    // callwlate physical address of dma source buffer
    dma_pa = (dma_va - dma_memory->virtual_address) + dma_memory->physical_address;

    // determine dma aperture index
    if (dma_memory->physical_address >= LW_RISCV_AMAP_SYSGPA_START)
    {
        dma_idx = LIBOS_DMAIDX_PHYS_SYS_COH_FN0;
    }
    else
    {
        dma_idx = LIBOS_DMAIDX_PHYS_VID_FN0;
    }

    start_page = tcm_va >> LIBOS_MPU_PAGESHIFT;
    end_page   = (tcm_va + size - 1) >> LIBOS_MPU_PAGESHIFT;

    if (start_page != end_page)
    {
        start_size = LIBOS_MPU_PAGESIZE - (LwU32)(tcm_va & ~LIBOS_MPU_PAGEMASK);
        end_size   = (LwU32)((tcm_va + size - 1) & ~LIBOS_MPU_PAGEMASK) + 1;
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
            page_size = LIBOS_MPU_PAGESIZE;
        }

        // kernel_mmu_page_in does not need to read if we are about to overwrite the entire page.
        skip_read = (!from_tcm && (page_size == LIBOS_MPU_PAGESIZE));

        // @note This operation is guaranteed to succeed because of prior validation of descriptor
        // coverage
        tcm_offset = kernel_mmu_page_in(tcm_memory, i << LIBOS_MPU_PAGESHIFT, mcause, skip_read)
                     | (tcm_va & ~LIBOS_MPU_PAGEMASK);

        kernel_dma_start(dma_pa, tcm_offset, page_size, dma_idx, write_flag);

        dma_pa += page_size;
        tcm_va += page_size;
    }
}

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN
kernel_syscall_dma_register_tcm
(
    LwU64 va,
    LwU32 size
)
{
    libos_pagetable_entry_t *memory;

    // The implementation of kernel_syscall_dma_copy() requires the TCM bounce
    // buffer to be page-aligned.
    if (((va | size) & ~LIBOS_MPU_PAGEMASK) != 0)
        kernel_return_access_errorcode();

    memory = validate_memory_access_or_return(va, size, LW_TRUE);
    if (!LIBOS_MEMORY_IS_PAGED_TCM(memory))
        kernel_return_access_errorcode();

    self->dma.tcm.memory = LIBOS_PTR32_NARROW(memory);
    self->dma.tcm.va = va;
    self->dma.tcm.size = size;

    self->state.registers[LIBOS_REG_IOCTL_A0] = LIBOS_OK;
    kernel_return();
}

/*!
 * @brief Initiate a DMA copy to/from TCM, or through a TCM bounce buffer.
 *
 * param[in] dst_va         virtual address of destination buffer
 * param[in] src_va         virtual address of source buffer
 * param[in] size           size to transfer in bytes; must be 4 byte aligned
 *
 * Possible status values returned:
 *   LIBOS_ERROR_ILWALID_ARGUMENT
 *   LIBOS_ERROR_OPERATION_FAILED
 *   LIBOS_OK
 */
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN
kernel_syscall_dma_copy
(
    LwU64 dst_va,
    LwU64 src_va,
    LwU32 size
)
{
    libos_pagetable_entry_t *dst_memory, *src_memory, *tcm_memory;
    LwU32 remaining = size;
    LwU64 tcm_va;
    LwU32 tcm_size;

    // GSP DMA requires 4 byte alignment for size and addresses.
    if ((dst_va | src_va | size) & 3)
        kernel_return_access_errorcode();

    // Find out which VA is TCM (dst_va, src_va, or tcm_va)
    dst_memory = validate_memory_access_or_return(dst_va, size, LW_TRUE);
    src_memory = validate_memory_access_or_return(src_va, size, LW_FALSE);

    // One and only one should be TCM. If dst_va or src_va are TCM, tcm_va should be 0.
    if (!LIBOS_MEMORY_IS_PAGED_TCM(dst_memory) &&
        !LIBOS_MEMORY_IS_PAGED_TCM(src_memory))
    {
        // We'll need to use a TCM bounce buffer, which should have already
        // been registered and validated in kernel_syscall_dma_register_tcm().
        tcm_memory = LIBOS_PTR32_EXPAND(libos_pagetable_entry_t, self->dma.tcm.memory);
        tcm_va = self->dma.tcm.va;
        tcm_size = self->dma.tcm.size;

        if (!tcm_memory)
            kernel_return_access_errorcode();
    }
    else if (LIBOS_MEMORY_IS_PAGED_TCM(dst_memory))
    {
        tcm_memory = dst_memory;
        tcm_va = dst_va;
        tcm_size = size;

        // Can't DMA TCM -> TCM
        if (LIBOS_MEMORY_IS_PAGED_TCM(src_memory))
            kernel_return_access_errorcode();
    }
    else
    {
        tcm_memory = src_memory;
        tcm_va = src_va;
        tcm_size = size;
    }

    if (!LIBOS_MEMORY_IS_PAGED_TCM(tcm_memory) ||
        LIBOS_MEMORY_IS_EXELWTE(tcm_memory) ||
        LIBOS_MEMORY_IS_MMIO(src_memory) ||
        LIBOS_MEMORY_IS_MMIO(dst_memory))
    {
        kernel_return_access_errorcode();
    }

    while (remaining > 0)
    {
        LwU32 xfer_size = tcm_size;
        LwU32 offset = 0;

        if ((tcm_memory != src_memory) && (tcm_memory != dst_memory))
        {
            // If a TCM bounce buffer smaller than the overall transfer was
            // given, make the first sub-transfer smaller so that the remaining
            // sub-transfers from the source are page-aligned.
            offset = src_va & (tcm_size - 1);
            xfer_size -= offset;
        }

        if (xfer_size > remaining)
            xfer_size = remaining;

        // DMA src -> TCM
        if (src_memory != tcm_memory)
        {
            // Wait for any prior transfers to complete
            kernel_dma_flush();
            kernel_dma_xfer(tcm_memory, tcm_va + offset,
                            src_memory, src_va, xfer_size,
                            LW_FALSE);
            src_va += xfer_size;
            tcm_va += (dst_memory == tcm_memory) ? xfer_size : 0;
        }

        // DMA TCM -> dst
        if (dst_memory != tcm_memory)
        {
            // Wait for any prior transfers to complete
            kernel_dma_flush();
            LIBOS_ASSERT(dst_memory != tcm_memory);
            kernel_dma_xfer(tcm_memory, tcm_va + offset,
                            dst_memory, dst_va, xfer_size,
                            LW_TRUE);
            dst_va += xfer_size;
            tcm_va += (src_memory == tcm_memory) ? xfer_size : 0;
        }

        remaining -= xfer_size;
    }

    self->state.registers[LIBOS_REG_IOCTL_A0] = LIBOS_OK;
    kernel_return();
}

/*!
 *  @brief Wait for completion of remaining dma transfers (if any)
 */
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN
kernel_syscall_dma_flush(void)
{
    kernel_dma_flush();
    kernel_return();
}

#endif // LIBOS_CONFIG_TCM_PAGING
