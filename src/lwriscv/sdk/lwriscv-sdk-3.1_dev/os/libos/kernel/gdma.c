/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwmisc.h>
#include <lwtypes.h>
#include <peregrine-headers.h>
#include <stddef.h>

#include "kernel.h"
#include "diagnostics.h"
#include "libos_status.h"
#include "task.h"

// Migrated from liblwriscv

#define GDMA_NUM_SUBCHANNELS           2
#define GDMA_ADDR_ALIGN                (1<<2)
#define GDMA_MAX_XFER_LENGTH           0xFFFFFC

typedef enum
{
    GDMA_L2C_L2_EVICT_FIRST,
    GDMA_L2C_L2_EVICT_NORMAL,
    GDMA_L2C_L2_EVICT_LAST
} GDMA_L2C;

#define LW_PRGNLCL_GDMA_IRQ_CTRL_CHANx_DIS(i)     (i):(i)
#define LW_PRGNLCL_GDMA_IRQ_CTRL_CHANx_DIS_TRUE LW_PRGNLCL_GDMA_IRQ_CTRL_CHAN0_DIS_TRUE
#define LW_PRGNLCL_GDMA_IRQ_CTRL_CHANx_DIS_FALSE LW_PRGNLCL_GDMA_IRQ_CTRL_CHAN0_DIS_FALSE

static LwU8 LIBOS_SECTION_DMEM_COLD(gdmaNumChannels);

LIBOS_SECTION_TEXT_COLD LibosStatus KernelGdmaCfgChannel(LwU8 chanIdx)
{
    LwU32 reg;

    KernelAssert(chanIdx < gdmaNumChannels);

    /*!
    * Round-robin weight of this channel (0-16).
    * Higher values will result in more bandwidth being spent on this channel
    * when it is competing with another channel for a common interface.
    */
    reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _RR_WEIGHT, 0);
    /*!
    * Maximum inflight transaction size (0-7). It is useful when
    * the bandwidth of destination does not match that of global memory.
    *   max = 0 -> unlimited, issue until back-pressured
    *   max > 0 -> (64 << (max-1)), support 64B ~ 4KB
    *   Recommended configurations:
    *   ==============================================================
    *   |  Source    |  Destination |           MAX_INFLIGHT         |
    *   --------------------------------------------------------------
    *   | Global MEM |  Local MEM   |           Unlimited            |
    *   --------------------------------------------------------------
    *   | Global MEM |  IO posted   |               4KB              |
    *   --------------------------------------------------------------
    *   | Global MEM | IO nonposted |               64B              |
    *   --------------------------------------------------------------
    *   |            |              | When using FBIF, the total     |
    *   |            |              | inflight size of all channels  |
    *   | Global MEM |  Global MEM  | MUST be less than FBIF reorder |
    *   |            |              | -buffer size (typical 4KB) to  |
    *   |            |              | avoid deadlock!                |
    *   ==============================================================
    */
    // TODO: Ask HW about the behavior of this register and set it per transfer
    reg |= DRF_NUM(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _MAX_INFLIGHT, 1);
    reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _MEMQ, _FALSE);

    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG(chanIdx), reg);

    reg = KernelMmioRead(LW_PRGNLCL_GDMA_IRQ_CTRL);
    reg = FLD_IDX_SET_DRF(_PRGNLCL_GDMA, _IRQ_CTRL, _CHANx_DIS, chanIdx, _TRUE, reg);

    KernelMmioWrite(LW_PRGNLCL_GDMA_IRQ_CTRL, reg);

    // default address mode: incremental addressing
    reg = DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _SRC_ADDR_MODE, _INC) |
        DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_ADDR_MODE, _INC) |
        DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_POSTED, _FALSE);

    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1(chanIdx), reg);

    // defaults: coherent access, L2C evict normal, WPR ID 0
    reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_COHERENT, 1) |
        DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_L2C_RD, GDMA_L2C_L2_EVICT_NORMAL) |
        DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_COHERENT, 1) |
        DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_L2C_WR, GDMA_L2C_L2_EVICT_NORMAL);

    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2(chanIdx), reg);

    return LibosOk;
}

LIBOS_SECTION_TEXT_COLD LibosStatus
KernelGdmaXferReg (
    LwU64 src,
    LwU64 dest,
    LwU32 length,
    LwU8 chanIdx,
    LwU8 subChanIdx
)
{
    LwU32 reg;

    KernelAssert(chanIdx < gdmaNumChannels);
    KernelAssert(subChanIdx < GDMA_NUM_SUBCHANNELS);
    KernelAssert(LW_IS_ALIGNED(src, GDMA_ADDR_ALIGN));
    KernelAssert(LW_IS_ALIGNED(dest, GDMA_ADDR_ALIGN));
    KernelAssert(LW_IS_ALIGNED(length, GDMA_ADDR_ALIGN));
    KernelAssert(length <= GDMA_MAX_XFER_LENGTH);

    // If HW Request Queue is full, wait for one of the transfers to complete
    do {
        reg = KernelMmioRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chanIdx));
        if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, reg) ||
            FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED, _TRUE, reg))
        {
            // If the channel is in an error state, new requests cannot be queued
            return LibosErrorFailed;
        }
    }
    while (FLD_TEST_DRF_NUM(_PRGNLCL_GDMA, _CHAN_STATUS, _REQ_QUEUE_SPACE, 0, reg));

    // Write the src and dest addresses
    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_SRC_ADDR(chanIdx), LwU64_LO32(src));
    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_SRC_ADDR_HI(chanIdx), LwU64_HI32(src));

    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_DEST_ADDR(chanIdx), LwU64_LO32(dest));
    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_DEST_ADDR_HI(chanIdx), LwU64_HI32(dest));

    reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _LENGTH, (length/GDMA_ADDR_ALIGN));
    reg |= DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _SUBCHAN, subChanIdx);
    reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _COMPLETE_IRQEN, _FALSE);
    reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _LAUNCH, _TRUE);

    // This kicks off the DMA
    KernelMmioWrite(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(chanIdx), reg);

    return LibosOk;
}

LIBOS_SECTION_TEXT_COLD LibosStatus KernelGdmaFlush(LwU8 chanIdx)
{
    LwU32 reg;
    LwU32 reqProduce = KernelMmioRead(LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(chanIdx));
    LwU32 reqComplete;

    KernelAssert(chanIdx < gdmaNumChannels);

    do
    {
        reqComplete = KernelMmioRead(LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(chanIdx));

        reg = KernelMmioRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chanIdx));
        if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, reg) ||
            FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED, _TRUE, reg))
        {
            // Does not recover from failure for now because it's hard to return
            // the error to the initiator.
            return LibosErrorFailed;
        }
    }
    while (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _BUSY, _TRUE, reg) || (reqComplete < reqProduce));

    return LibosOk;
}

LIBOS_SECTION_TEXT_COLD void KernelGdmaLoad(void)
{
    gdmaNumChannels = DRF_VAL(_PRGNLCL_GDMA, _CFG, _CHANS, KernelMmioRead(LW_PRGNLCL_GDMA_CFG));
    for (LwU8 chanIdx = 0; chanIdx < gdmaNumChannels; chanIdx++)
    {
        KernelGdmaCfgChannel(chanIdx);
    }
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelSyscallGdmaTransfer
(
    LwU64 dstVa,
    LwU64 srcVa,
    LwU32 size,
    LwU64 flags
)
{
    static LwU8 LIBOS_SECTION_DMEM_COLD(lwrChanIdx) = 0;
    LwU8 subChanIdx;

    if ((!LW_IS_ALIGNED(dstVa, GDMA_ADDR_ALIGN)) ||
        (!LW_IS_ALIGNED(srcVa, GDMA_ADDR_ALIGN)) ||
        (!LW_IS_ALIGNED(size, GDMA_ADDR_ALIGN)) ||
        (size > GDMA_MAX_XFER_LENGTH))
    {
        KernelSyscallReturn(LibosErrorArgument);
    }

    // Does not translate VA to PA because all mappings are identity mapping for FSP
    // TODO: Security checks on the address

    subChanIdx = (dstVa < LW_RISCV_AMAP_INTIO_START) ? 0 : 1;
    KernelGdmaXferReg(srcVa, dstVa, size, lwrChanIdx, subChanIdx);

    lwrChanIdx = (lwrChanIdx + 1) % gdmaNumChannels;
    KernelSyscallReturn(LibosOk);
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelSyscallGdmaFlush(void)
{
    for (LwU8 chanIdx = 0; chanIdx < gdmaNumChannels; chanIdx++)
    {
        LibosStatus status = KernelGdmaFlush(chanIdx);
        if (status != LibosOk)
        {
            KernelSyscallReturn(status);
        }
    }
    KernelSyscallReturn(LibosOk);
}
