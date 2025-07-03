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
#include "libos_status.h"
#include "libos_syscalls.h"
#include "memory.h"
#include "mmu.h"
#include "paging.h"
#include "task.h"

#include "riscv-isa.h"
#include "lw_gsp_riscv_address_map.h"
#include "dev_gsp.h"
#include "dev_gsp_prgnlcl.h"

// Ported from //sw/lwriscv/main/os/libos-next/kernel/gdma.c#1

#define GDMA_NUM_SUBCHANNELS           2
#define GDMA_ADDR_ALIGN                (1<<2)
#define GDMA_MAX_XFER_LENGTH           0xFFFFFC

typedef enum
{
    GDMA_L2C_L2_EVICT_FIRST,
    GDMA_L2C_L2_EVICT_NORMAL,
    GDMA_L2C_L2_EVICT_LAST
} GDMA_L2C;

static LwU8 gdma_num_channels;

static LIBOS_SECTION_TEXT_COLD LwU8 gdma_cfg_channel(LwU8 chan_idx)
{
    LwU32 reg;

    LIBOS_ASSERT(chan_idx < gdma_num_channels);

    // Round-robin weight of this channel (0-16).
    // Higher values will result in more bandwidth being spent on this channel
    // when it is competing with another channel for a common interface.
    reg = REF_NUM(LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG_RR_WEIGHT, 0);
    reg |= REF_DEF(LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG_MEMQ, _FALSE);

    // TODO: This is not ideal, but we don't know how to set this register yet.
    //       This setting only applies when Global memory is the source.
    //       This may imply we should not round-robin the channels but instead
    //       assign e.g., global -> TCM to channel 0 and global -> global to
    //       channel 1 (other sources can be round-robin'd?)
    reg |= REF_DEF(LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG_MAX_INFLIGHT, _64B);

    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG(chan_idx), reg);

    // Set the IRQ_CTRL_CHANx_DIS bit for the channel - we don't use interrupts yet
    reg = prgnlcl_read(LW_PRGNLCL_GDMA_IRQ_CTRL);
    reg |= (1 << chan_idx);
    prgnlcl_write(LW_PRGNLCL_GDMA_IRQ_CTRL, reg);

    // default address mode: incremental addressing
    reg = REF_DEF(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_SRC_ADDR_MODE, _INC) |
        REF_DEF(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_DEST_ADDR_MODE, _INC) |
        REF_DEF(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_DEST_POSTED, _FALSE);

    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1(chan_idx), reg);

    return LIBOS_OK;
}

LIBOS_SECTION_TEXT_COLD LwU8 kernel_gdma_xfer_reg (
    LwU64 src_pa,
    LwU64 dst_pa,
    LwU32 length,
    LwU8 chan_idx,
    LwU8 sub_chan_idx
)
{
    LwU32 reg;

    LIBOS_ASSERT(chan_idx < gdma_num_channels);
    LIBOS_ASSERT(sub_chan_idx < GDMA_NUM_SUBCHANNELS);
    LIBOS_ASSERT(IS_ALIGNED(src_pa, GDMA_ADDR_ALIGN));
    LIBOS_ASSERT(IS_ALIGNED(dst_pa, GDMA_ADDR_ALIGN));
    LIBOS_ASSERT(IS_ALIGNED(length, GDMA_ADDR_ALIGN));
    LIBOS_ASSERT(length <= GDMA_MAX_XFER_LENGTH);

    // If HW Request Queue is full, wait for one of the transfers to complete
    do {
        reg = prgnlcl_read(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));
        if ((REF_VAL(LW_PRGNLCL_GDMA_CHAN_STATUS_ERROR_VLD, reg) ==
                LW_PRGNLCL_GDMA_CHAN_STATUS_ERROR_VLD_TRUE) ||
            (REF_VAL(LW_PRGNLCL_GDMA_CHAN_STATUS_FAULTED, reg) ==
                LW_PRGNLCL_GDMA_CHAN_STATUS_FAULTED_TRUE))
        {
            // If the channel is in an error state, new requests cannot be queued
            return LIBOS_ERROR_OPERATION_FAILED;
        }
    }
    while (REF_VAL(LW_PRGNLCL_GDMA_CHAN_STATUS_REQ_QUEUE_SPACE, reg) == 0);

    reg = 0;
    if ((src_pa >= LW_RISCV_AMAP_FBGPA_START) && (src_pa < LW_RISCV_AMAP_FBGPA_END))
    {
        reg |= REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT, 1) |
               REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_L2C_RD, GDMA_L2C_L2_EVICT_NORMAL);
#if LIBOS_CONFIG_WPR
        if (WPR_OVERLAP(src_pa, length))
            reg |= REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_WPR, LIBOS_CONFIG_WPR);
#endif
    }
    
    if ((dst_pa >= LW_RISCV_AMAP_FBGPA_START) && (dst_pa < LW_RISCV_AMAP_FBGPA_END))
    {
        reg |= REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_DEST_FB_COHERENT, 1) |
               REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_DEST_FB_L2C_WR, GDMA_L2C_L2_EVICT_NORMAL);
#if LIBOS_CONFIG_WPR
        if (WPR_OVERLAP(dst_pa, length))
            reg |= REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_DEST_FB_WPR, LIBOS_CONFIG_WPR);
#endif
    }
    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2(chan_idx), reg);

    // Write the src and dest addresses
    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_SRC_ADDR(chan_idx), LwU64_LO32(src_pa));
    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_SRC_ADDR_HI(chan_idx), LwU64_HI32(src_pa));

    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_DEST_ADDR(chan_idx), LwU64_LO32(dst_pa));
    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_DEST_ADDR_HI(chan_idx), LwU64_HI32(dst_pa));

    reg = REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0_LENGTH, (length/GDMA_ADDR_ALIGN));
    reg |= REF_NUM(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0_SUBCHAN, sub_chan_idx);
    reg |= REF_DEF(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0_COMPLETE_IRQEN, _FALSE);
    reg |= REF_DEF(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0_LAUNCH, _TRUE);

    // This kicks off the DMA
    prgnlcl_write(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(chan_idx), reg);

    return LIBOS_OK;
}

static LIBOS_SECTION_TEXT_COLD LwU8 gdma_flush(LwU8 chan_idx)
{
    LwU32 reg;
    LwU32 req_produce = prgnlcl_read(LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(chan_idx));
    LwU32 req_complete;

    LIBOS_ASSERT(chan_idx < gdma_num_channels);

    do
    {
        req_complete = prgnlcl_read(LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(chan_idx));

        reg = prgnlcl_read(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));
        if ((REF_VAL(LW_PRGNLCL_GDMA_CHAN_STATUS_ERROR_VLD, reg) ==
                LW_PRGNLCL_GDMA_CHAN_STATUS_ERROR_VLD_TRUE) ||
            (REF_VAL(LW_PRGNLCL_GDMA_CHAN_STATUS_FAULTED, reg) ==
                LW_PRGNLCL_GDMA_CHAN_STATUS_FAULTED_TRUE))
        {
            // Does not recover from failure for now because it's hard to return
            // the error to the initiator.
            return LIBOS_ERROR_OPERATION_FAILED;
        }
    }
    while ((REF_VAL(LW_PRGNLCL_GDMA_CHAN_STATUS_BUSY, reg) ==
                LW_PRGNLCL_GDMA_CHAN_STATUS_BUSY_TRUE) ||
           (req_complete < req_produce));

    return LIBOS_OK;
}

LIBOS_SECTION_TEXT_COLD void kernel_gdma_load(void)
{
    //! The INTIO range is only used for GDMA access for now
    kernel_mmu_program_mpu_entry(LIBOS_MPU_INDEX_INTIO_PRI,
                                 PEREGRINE_INTIO_PRI_BASE_VA,
                                 LW_RISCV_AMAP_INTIO_START,
                                 LW_RISCV_AMAP_INTIO_SIZE,
                                 REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) |
                                    REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL));

    gdma_num_channels = REF_VAL(LW_PRGNLCL_GDMA_CFG_CHANS, prgnlcl_read(LW_PRGNLCL_GDMA_CFG));
    for (LwU8 chan_idx = 0; chan_idx < gdma_num_channels; chan_idx++)
    {
        gdma_cfg_channel(chan_idx);
    }
}

LIBOS_SECTION_TEXT_COLD void kernel_gdma_flush(void)
{
    for (LwU8 chan_idx = 0; chan_idx < gdma_num_channels; chan_idx++)
    {
        (void)gdma_flush(chan_idx);
    }
}

static LIBOS_SECTION_TEXT_COLD LwU64 dma_va_to_pa
(
    libos_pagetable_entry_t *memory,
    LwU64 va,
    LwBool write,
    LwU32 *max_xfer_length
)
{
    LwU64 pa;

    // We need to page in TCM memory to get its physical address
#if LIBOS_CONFIG_TCM_PAGING
    if (LIBOS_MEMORY_IS_PAGED_TCM(memory))
    {
        LwU32 offset = va & ~LIBOS_MPU_PAGEMASK;
        LwU64 mcause = write ? LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT
                             : LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT;

        // Shrink the transfer to fit within the TCM page, if necessary
        if ((LIBOS_MPU_PAGESIZE - offset) < *max_xfer_length)
            *max_xfer_length = LIBOS_MPU_PAGESIZE - offset;

        // kernel_mmu_page_in() returns a TCM offset, need to add in the PA base
        pa = LIBOS_MEMORY_IS_EXELWTE(memory) ? LW_RISCV_AMAP_IMEM_START
                                             : LW_RISCV_AMAP_DMEM_START;
        pa += kernel_mmu_page_in(memory, va - offset, mcause,
                                 write && (*max_xfer_length == LIBOS_MPU_PAGESIZE));
        pa += offset;
    }
    else
#endif
    {
        pa = (va - memory->virtual_address) + memory->physical_address;
        if (*max_xfer_length > GDMA_MAX_XFER_LENGTH)
            *max_xfer_length = GDMA_MAX_XFER_LENGTH;
    }

    return pa;
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_syscall_gdma_register_tcm
(
    LwU64 va,
    LwU32 size
)
{
    // We don't use the TCM buffer with GDMA, since GDMA is capable of direct
    // global -> global transfers.
    self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_OK;
    kernel_return();
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_syscall_gdma_copy
(
    LwU64 dst_va,
    LwU64 src_va,
    LwU32 size
)
{
    libos_pagetable_entry_t *src_memory, *dst_memory;
    LwU32 remaining = size;
    static LwU8 lwr_chan_idx = 0;
    LwU8 sub_chan_idx;
    LwU8 status = LIBOS_OK;

    if ((!IS_ALIGNED(dst_va, GDMA_ADDR_ALIGN)) ||
        (!IS_ALIGNED(src_va, GDMA_ADDR_ALIGN)) ||
        (!IS_ALIGNED(size, GDMA_ADDR_ALIGN)) || (size == 0u))
    {
        self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_ERROR_ILWALID_ARGUMENT;
        kernel_return();
    }

    // Validate appropriate access and size for destination memory
    dst_memory = validate_memory_access_or_return(dst_va, size, LW_TRUE);

    // Validate appropriate access and size for source memory
    src_memory = validate_memory_access_or_return(src_va, size, LW_FALSE);

    // Check for overlapping buffers, which are not supported for simplicity
    if (((src_va <= dst_va) && (src_va + size > dst_va)) ||
        ((dst_va <= src_va) && (dst_va + size > src_va)))
    {
        self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_ERROR_ILWALID_ARGUMENT;
        kernel_return();
    }

    // TODO: select channel based on source/destination (see comment in gdma_cfg_channel())
    lwr_chan_idx = (lwr_chan_idx + 1) % gdma_num_channels;

    do
    {
        LwU32 xfer_length = remaining;  // dma_va_to_pa will shrink this as needed
        LwU64 dst_pa, src_pa;

        // Pages in TCM src/dst if necessary to get the PA.
        src_pa = dma_va_to_pa(src_memory, src_va, LW_FALSE, &xfer_length);
        dst_pa = dma_va_to_pa(dst_memory, dst_va, LW_TRUE, &xfer_length);

        sub_chan_idx = (dst_pa < LW_RISCV_AMAP_INTIO_START) ? 0 : 1;

        // TCM -> Sysmem
        status = kernel_gdma_xfer_reg(src_pa, dst_pa, xfer_length,
                                      lwr_chan_idx, sub_chan_idx);
        
        remaining -= xfer_length;
        src_va += xfer_length;
        dst_va += xfer_length;
    }
    while ((status == LIBOS_OK) && (remaining > 0));

    self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = status;
    kernel_return();
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_syscall_gdma_flush(void)
{
    kernel_gdma_flush();
    kernel_return();
}
