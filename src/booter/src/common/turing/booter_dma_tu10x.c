/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: booter_dma_tu10x.c 
 */
// Includes
//
#include "booter.h"

// Extern Variables 
extern BOOTER_DMA_PROP     g_dmaProp[BOOTER_DMA_CONTEXT_COUNT];

// DMA CTX update macros
#define  DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~(0x7 << 0)) | ((dmaIdx) << 0))
#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~(0x7 << 8)) | ((dmaIdx) << 8))
#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~(0x7 << 12)) | ((dmaIdx) << 12))

/*!
 * @brief Booter routine to initialize DMA
 */
void
booterInitializeDma_TU10X()
{
    // TODO-30952877: dmiyani: @srathod to help review this function

    //
    // General settings
    //

    // Set DMACTL_REQUIRE_CTX_FALSE
    booterSetupDmaCtl_HAL(pBooter, LW_FALSE);
   
    // Set FBIF_CTL_ENABLE_TRUE and FBIF_CTL_ALLOW_PHYS_NO_CTX_ALLOW 
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    LwU32 fbifCtlVal = BOOTER_REG_RD32(CSB, LW_CSEC_FBIF_CTL);
    fbifCtlVal = FLD_SET_DRF(_CSEC, _FBIF_CTL, _ENABLE, _TRUE, fbifCtlVal);
    fbifCtlVal = FLD_SET_DRF(_CSEC, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, fbifCtlVal);
    BOOTER_REG_WR32(CSB, LW_CSEC_FBIF_CTL, fbifCtlVal);
#elif defined(BOOTER_RELOAD)
    LwU32 fbifCtlVal = BOOTER_REG_RD32(CSB, LW_CLWDEC_FBIF_CTL);
    fbifCtlVal = FLD_SET_DRF(_CLWDEC, _FBIF_CTL, _ENABLE, _TRUE, fbifCtlVal);
    fbifCtlVal = FLD_SET_DRF(_CLWDEC, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, fbifCtlVal);
    BOOTER_REG_WR32(CSB, LW_CLWDEC_FBIF_CTL, fbifCtlVal);
#endif

    //
    // Context specific settings
    //

    // Initialize FB Non-WPR DMA context
    g_dmaProp[BOOTER_DMA_FB_NON_WPR_CONTEXT].ctxDma   = BOOTER_DMA_FB_NON_WPR_CONTEXT;
    g_dmaProp[BOOTER_DMA_FB_NON_WPR_CONTEXT].regionID = NON_WPR_REGION_ID;
    booterSetupCtxDma_HAL(pBooter, g_dmaProp[BOOTER_DMA_FB_NON_WPR_CONTEXT].ctxDma, LW_TRUE);
    booterProgramRegionCfg_HAL(pBooter, BOOTER_DMA_FB_NON_WPR_CONTEXT, g_dmaProp[BOOTER_DMA_FB_NON_WPR_CONTEXT].regionID);

    // Initialize FB WPR DMA context
    g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].ctxDma   = BOOTER_DMA_FB_WPR_CONTEXT;
    g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].regionID = WPR_REGION_ID;
    booterSetupCtxDma_HAL(pBooter, g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].ctxDma, LW_TRUE);
    booterProgramRegionCfg_HAL(pBooter, BOOTER_DMA_FB_WPR_CONTEXT, g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].regionID);

    // Initialize Sysmem DMA context
    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].ctxDma   = BOOTER_DMA_SYSMEM_CONTEXT;
    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].regionID = NON_WPR_REGION_ID;
    booterSetupCtxDma_HAL(pBooter, g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].ctxDma, LW_FALSE);
    booterProgramRegionCfg_HAL(pBooter, BOOTER_DMA_SYSMEM_CONTEXT, g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].regionID);
}


/*!
 * @brief Booter routine to do falcon FB DMA to and from IMEM/DMEM.
 *
 * @param[in]  pOff        Offset to be used either as source or destination
 * @param[in]  bIsImem     Is Source/destination IMEM?
 * @param[in]  fbOff       FB offset from DMB
 * @param[in]  sizeInBytes Size of the content to be DMAed
 * @param[in]  bIsCopytoFB True If FB destination.
 * @param[in]  bIsSync     Do dmwait after submitting the DMA request(s)
 * @param[in]  pDmaProp    DMA properties to be used for this DMA operation
 *
 * @return Number of bytes transferred
 */
LwU32
booterIssueDma_TU10X
(
    void                  *pOff, 
    LwBool                bIsImem, 
    LwU32                 fbOff, 
    LwU32                 sizeInBytes,
    BOOTER_DMA_DIRECTION  dmaDirection,
    BOOTER_DMA_SYNC_TYPE  dmaSync,
    PBOOTER_DMA_PROP      pDmaProp
)
{
    LwU32           bytesXfered = 0;
    LwU32           memOff = (LwU32)pOff;
    LwU32           memSrc;
    LwU32           xferSize;
    LwU32           e;
    LwU32           ctx;

    // Set regionCFG
    booterProgramRegionCfg_HAL(pBooter, pDmaProp->ctxDma, pDmaProp->regionID);

    // Set new DMA and CTX
    booterProgramDmaBase_HAL(pBooter, bIsImem, pDmaProp);

    // Update the CTX, this depends on mem type and dir
    falc_rspr(ctx, CTX);
    if (bIsImem)
    {
        ctx = DMA_SET_IMREAD_CTX(ctx, pDmaProp->ctxDma);
        falc_wspr(CTX, ctx);
    }
    else
    {
        ctx = (dmaDirection == BOOTER_DMA_FROM_FB) ? DMA_SET_DMREAD_CTX(ctx, pDmaProp->ctxDma) : DMA_SET_DMWRITE_CTX(ctx, pDmaProp->ctxDma);
        falc_wspr(CTX, ctx);
    }

    e = DMA_XFER_ESIZE_MAX;
    xferSize = DMA_XFER_SIZE_BYTES(e);

    while (bytesXfered != sizeInBytes)
    {
        if (((sizeInBytes - bytesXfered) >= xferSize) &&
              VAL_IS_ALIGNED((LwU32)memOff, xferSize) &&
              VAL_IS_ALIGNED(fbOff, xferSize))
        {
            if ((dmaDirection == BOOTER_DMA_TO_FB) || (dmaDirection == BOOTER_DMA_TO_FB_SCRUB))
            {
                if (dmaDirection != BOOTER_DMA_TO_FB_SCRUB)
                    memSrc = ((LwU32)memOff + bytesXfered);
                else
                    memSrc = (LwU32)memOff;

                if (bIsImem)
                {
                    // Error, no imwrite
                    return bytesXfered;
                }
                else
                {
                    falc_dmwrite(
                        fbOff + bytesXfered,
                        memSrc | (e << 16));
                }
            }
            else
            {
                if (bIsImem)
                {
                    falc_imread(
                        fbOff + bytesXfered,
                        ((LwU32)memOff + bytesXfered) | (e << 16));
                }
                else
                {
                    falc_dmread(
                        fbOff + bytesXfered,
                        ((LwU32)memOff + bytesXfered) | (e << 16));
                }
            }
            bytesXfered += xferSize;

            // Restore e
            e        = DMA_XFER_ESIZE_MAX;
            xferSize = DMA_XFER_SIZE_BYTES(e);
        }
        // try the next largest transfer size
        else
        {
            //
            // Ensure that we have not dropped below the minimum transfer size
            // supported by the HW. Such and event indicates that the requested
            // number of bytes cannot be written either due to mis-alignment of
            // the source and destination pointers or the requested write-size
            // not being a multiple of the minimum transfer size.
            //
            if (e > DMA_XFER_ESIZE_MIN)
            {
                e--;
                xferSize = DMA_XFER_SIZE_BYTES(e);
            }
            else
            {
                break;
            }
        }
    }

    if (dmaSync == BOOTER_DMA_SYNC_AT_END)
    {
        if (bIsImem)
            falc_imwait();
        else
            falc_dmwait();
    }

    return bytesXfered;
}

/*!
 * @brief Booter routine to program DMA base
 *
 * @param[in]  bIsImem     Is Source/destination IMEM?
 * @param[in]  pDmaProp    DMA properties to be used for this DMA operation
 */
void
booterProgramDmaBase_TU10X
(
    LwBool                bIsImem,
    PBOOTER_DMA_PROP      pDmaProp
)
{
    // Set new DMA
    if (bIsImem)
    {
        falc_wspr(IMB,  (LwU32)LwU64_LO32(pDmaProp->baseAddr));
        falc_wspr(IMB1, (LwU32)LwU64_HI32(pDmaProp->baseAddr));
    }
    else
    {
        falc_wspr(DMB,  (LwU32)LwU64_LO32(pDmaProp->baseAddr));
        falc_wspr(DMB1, (LwU32)LwU64_HI32(pDmaProp->baseAddr));
    }
}


#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)

/*!
 * @brief Programs the region CFG in FBFI register.
 */
void
booterProgramRegionCfg_TU10X
(
    LwU32                ctxDma,
    LwU32                regionID
)
{
    LwU32        data = 0;
    LwU32        mask = ~(0xF << (ctxDma*4));

    data = BOOTER_REG_RD32(CSB, LW_CSEC_FBIF_REGIONCFG);
    data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
    BOOTER_REG_WR32(CSB, LW_CSEC_FBIF_REGIONCFG, data);
}

/*!
 * @brief Program FBIF to allow physical transactions.
 */
void
booterSetupCtxDma_TU10X
(
    LwU32            ctxDma,
    LwBool           bIsFbCtx
)
{
    LwU32 regVal = 0;

    if(bIsFbCtx)
    {
        regVal = BOOTER_REG_RD32(CSB, LW_CSEC_FBIF_TRANSCFG(ctxDma));
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, regVal);
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, regVal);
        BOOTER_REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(ctxDma), regVal);
    }
    else
    {
        regVal = BOOTER_REG_RD32(CSB, LW_CSEC_FBIF_TRANSCFG(ctxDma));
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _COHERENT_SYSMEM, regVal);
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, regVal);
        BOOTER_REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(ctxDma), regVal);
    }
}

/*!
 * @brief Setup DMACTL
 */
void
booterSetupDmaCtl_TU10X
(
    LwBool bIsCtxRequired
)
{
    if (bIsCtxRequired)
    {
        BOOTER_REG_WR32(CSB, LW_CSEC_FALCON_DMACTL, 0x1);
    }
    else
    {
        BOOTER_REG_WR32(CSB, LW_CSEC_FALCON_DMACTL, 0x0);
    }
}

#elif defined(BOOTER_RELOAD)

/*!
 * @brief Programs the region CFG in FBFI register.
 */
void
booterProgramRegionCfg_TU10X
(
    LwU32                ctxDma,
    LwU32                regionID
)
{
    LwU32        data = 0;
    LwU32        mask = ~(0xF << (ctxDma*4));

    data = BOOTER_REG_RD32(CSB, LW_CLWDEC_FBIF_REGIONCFG);
    data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
    BOOTER_REG_WR32(CSB, LW_CLWDEC_FBIF_REGIONCFG, data);
}

/*!
 * @brief Program FBIF to allow physical transactions.
 */
void
booterSetupCtxDma_TU10X
(
    LwU32            ctxDma,
    LwBool           bIsFbCtx
)
{
    LwU32 regVal = 0;

    if(bIsFbCtx)
    {
        regVal = BOOTER_REG_RD32(CSB, LW_CLWDEC_FBIF_TRANSCFG(ctxDma));
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, regVal);
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, regVal);
        BOOTER_REG_WR32(CSB, LW_CLWDEC_FBIF_TRANSCFG(ctxDma), regVal);
    }
    else
    {
        regVal = BOOTER_REG_RD32(CSB, LW_CLWDEC_FBIF_TRANSCFG(ctxDma));
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _COHERENT_SYSMEM, regVal);
        regVal = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, regVal);
        BOOTER_REG_WR32(CSB, LW_CLWDEC_FBIF_TRANSCFG(ctxDma), regVal);
    }
}

/*!
 * @brief Setup DMACTL
 */
void
booterSetupDmaCtl_TU10X
(
    LwBool bIsCtxRequired
)
{
    if (bIsCtxRequired)
    {
        BOOTER_REG_WR32(CSB, LW_CLWDEC_FALCON_DMACTL, 0x1);
    }
    else
    {
        BOOTER_REG_WR32(CSB, LW_CLWDEC_FALCON_DMACTL, 0x0);
    }
}

#endif

