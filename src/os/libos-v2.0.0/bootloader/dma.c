/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dma.h"

#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"
#include "dev_gsp.h"
#include "dev_riscv_pri.h"
#include "lw_gsp_riscv_address_map.h"
#include "lwmisc.h"
#include "bootloader.h"

#define DMA_BLOCK_SIZE 256

static inline void falcon_write(LwU32 addr, LwU32 value)
{
    *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + LW_FALCON_GSP_BASE + addr) = value;
}

static inline LwU32 falcon_read(LwU32 addr)
{
    return *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + LW_FALCON_GSP_BASE + addr);
}

static inline void fbif_write(LwU32 addr, LwU32 value)
{
    *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + LW_PGSP_FBIF_TRANSCFG(0) + addr) = value;
}

static inline LwU32 fbif_read(LwU32 addr)
{
    return *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + LW_PGSP_FBIF_TRANSCFG(0) + addr);
}

void dma_init(void)
{
    LwU32 reg;

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
    fbif_write(LW_PFALCON_FBIF_TRANSCFG(DMAIDX_PHYS_VID_FN0), reg);

    // setup coherent sysmem aperture access
    reg =
        (REF_DEF(LW_PFALCON_FBIF_TRANSCFG_TARGET, _COHERENT_SYSMEM) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE, _PHYSICAL) |
         REF_DEF(LW_PFALCON_FBIF_TRANSCFG_ENGINE_ID_FLAG, _BAR2_FN0));
    fbif_write(LW_PFALCON_FBIF_TRANSCFG(DMAIDX_PHYS_SYS_COH_FN0), reg);
}

void dma_copy
(
    LwU64 phys_addr,
    LwU32 tcm_offset,
    LwU32 length,
    LwU32 dma_idx,
    LwU32 dma_flags
)
{
    LwU32 fb_offset = 0;
    LwU32 dma_cmd;

    // The boatloader dma_copy routine only handles 256-byte aligned addresses.
    if (((phys_addr | tcm_offset | length) & 0xff) != 0)
        halt();

    phys_addr >>= 8u;
    falcon_write(LW_PFALCON_FALCON_DMATRFBASE, LwU64_LO32(phys_addr));
    falcon_write(LW_PFALCON_FALCON_DMATRFBASE1, LwU64_HI32(phys_addr));

    dma_cmd = REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG, _TRUE) |
              REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_SIZE, _256B) |
              REF_NUM(LW_PFALCON_FALCON_DMATRFCMD_CTXDMA, dma_idx);

    if (dma_flags & DMAFLAG_WRITE)
        dma_cmd |= REF_DEF(LW_PFALCON_FALCON_DMATRFCMD_WRITE, _TRUE);

    while (length != 0)
    {
        // Wait if DMA queue is full.
        while ((falcon_read(LW_PFALCON_FALCON_DMATRFCMD) & DRF_SHIFTMASK(LW_PFALCON_FALCON_DMATRFCMD_FULL)) != 0u)
        {
            ;
        }

        falcon_write(LW_PFALCON_FALCON_DMATRFMOFFS, tcm_offset);
        falcon_write(LW_PFALCON_FALCON_DMATRFFBOFFS, fb_offset);
        falcon_write(LW_PFALCON_FALCON_DMATRFCMD, dma_cmd);

        tcm_offset += DMA_BLOCK_SIZE;
        fb_offset += DMA_BLOCK_SIZE;
        length -= DMA_BLOCK_SIZE;
    }
}

void dma_flush(void)
{
    while ((falcon_read(LW_PFALCON_FALCON_DMATRFCMD) & DRF_SHIFTMASK(LW_PFALCON_FALCON_DMATRFCMD_IDLE)) ==
           LW_PFALCON_FALCON_DMATRFCMD_IDLE_FALSE)
    {
        ;
    }

    if (FLD_TEST_DRF(_PFALCON, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _TRUE,
        falcon_read(LW_PFALCON_FALCON_DMAINFO_CTL)))
    {
        halt();
    }
}
