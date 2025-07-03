/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_dma_tu10x.c 
 */
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "dev_graphics_nobundle.h"
#include "dev_fb.h"
#include "dev_fbif_v4.h"
#include "dev_pwr_csb.h"

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#include "dev_sec_csb.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

// Extern Variables 
extern ACR_SCRUB_STATE  g_scrubState;
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;

#define ACR_DEFAULT_ALIGNMENT           (0x20000)

// DMA CTX update macros
#define  DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~(0x7 << 0)) | ((dmaIdx) << 0))
#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~(0x7 << 8)) | ((dmaIdx) << 8))
#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~(0x7 << 12)) | ((dmaIdx) << 12))

#ifdef ACR_BUILD_ONLY
/*!
 * @brief ACR routine to do falcon FB DMA to and from IMEM/DMEM.
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
acrIssueDma_TU10X
(
    void               *pOff, 
    LwBool             bIsImem, 
    LwU32              fbOff, 
    LwU32              sizeInBytes,
    ACR_DMA_DIRECTION  dmaDirection,
    ACR_DMA_SYNC_TYPE  dmaSync,
    PACR_DMA_PROP      pDmaProp
)
{
    LwU32           bytesXfered = 0;
    LwU32           memOff = (LwU32)pOff;
    LwU32           memSrc = (LwU32)pOff;
    LwU32           xferSize;
    LwU32           e;
    LwU16           ctx;

    // Set regionCFG
    acrlibProgramRegionCfg_HAL(pAcrlib, 0, LW_TRUE, pDmaProp->ctxDma, pDmaProp->regionID);

    // Set new DMA and CTX
    acrProgramDmaBase_HAL(pAcrlib, bIsImem, pDmaProp);

    // Update the CTX, this depends on mem type and dir
    falc_rspr(ctx, CTX);
    if (bIsImem)
    {
        falc_wspr(CTX, DMA_SET_IMREAD_CTX(ctx, pDmaProp->ctxDma));
    }
    else
    {
        falc_wspr(CTX, (dmaDirection == ACR_DMA_FROM_FB) ?
                  DMA_SET_DMREAD_CTX(ctx, pDmaProp->ctxDma) :
                  DMA_SET_DMWRITE_CTX(ctx, pDmaProp->ctxDma));
    }

    e = DMA_XFER_ESIZE_MAX;
    xferSize = DMA_XFER_SIZE_BYTES(e);

    while (bytesXfered != sizeInBytes)
    {
        if (((sizeInBytes - bytesXfered) >= xferSize) &&
              VAL_IS_ALIGNED((LwU32)memOff, xferSize) &&
              VAL_IS_ALIGNED(fbOff, xferSize))
        {
            if ((dmaDirection == ACR_DMA_TO_FB) || (dmaDirection == ACR_DMA_TO_FB_SCRUB))
            {
                if (dmaDirection != ACR_DMA_TO_FB_SCRUB)
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

    if (dmaSync == ACR_DMA_SYNC_AT_END)
    {
        if (bIsImem)
            falc_imwait();
        else
            falc_dmwait();
    }
    return bytesXfered;
}

/*!
 * @brief  Populates the DMA properties including ctxDma and RegionID
 *
 * @param[in] wprRegIndex Index into pAcrRegions holding the wpr region
 */
ACR_STATUS
acrPopulateDMAParameters_TU10X
(
    LwU32       wprRegIndex
)
{
    ACR_STATUS               status       = ACR_OK;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    LwU32                    val;

    //
    // Do sanity check on region ID and wpr offset.
    // If wprRegionID is not set, return as it shows that RM
    // didnt setup the WPR region.
    // TODO: Do more sanity checks including read/write masks
    //
#if defined(AHESASC)
    if (!(g_desc.wprRegionID == LSF_WPR_EXPECTED_REGION_ID))
    {
        return ACR_ERROR_NO_WPR;
    }

    if (g_desc.wprOffset != LSF_WPR_EXPECTED_OFFSET)
    {
        return ACR_ERROR_NO_WPR;
    }
#else
    g_desc.wprRegionID = LSF_WPR_EXPECTED_REGION_ID;
    g_desc.wprOffset = LSF_WPR_EXPECTED_OFFSET;
#endif

    pRegionProp         = &(g_desc.regions.regionProps[wprRegIndex]);

    //
    // Not every ACR variants have valid g_desc.
    // For UNLOAD variant, PopulateDMAParameters_TU10X is still be exelwted but g_desc is invalid.
    // Because we already have programmed the WPR address in MMU initially, we can read WPR settings from register
    // and restore to pRegionProp->startAddress and pRegionProp->endAddress.
    // The WPR address pair is saved in LW_PFB_PRI_MMU_WPR1_ADDR_LO[31:4] and LW_PFB_PRI_MMU_WPR1_ADDR_HI[31:4], and are 4K aligned.
    // In order to get 256 byte aligned WPR address, we need to shift left 4 bits for WPRR address pair.
    // The granularity of WPR is 128KB, so we need to shift right 8 bits for ACR_DEFAULT_ALIGNMENT and add to
    // pRegionProp->endAddress.
    //
    val = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
    val = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, val);
    pRegionProp->startAddress = val << (LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT - 8);

    val = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
    val = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, val);
    pRegionProp->endAddress = val << (LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT - 8);
    pRegionProp->endAddress += (ACR_DEFAULT_ALIGNMENT >> 8);

    // Decreasing pRegionProp->endAddress by 1 byte, make it represent the real end address rather than pRegionProp->startAddress + size.
    pRegionProp->endAddress -= 1;
    //
    // startAddress is 256 byte aligned and save in 32 bit integer and wprOffset is byte aligned address.
    // wprBase needs 256 bytes aligned address, so we need to shift 8 bits right for g_desc.wprOffset.
    //
    g_dmaProp.wprBase   = pRegionProp->startAddress + (g_desc.wprOffset >> 8);
    g_dmaProp.regionID  = g_desc.wprRegionID;

#if defined(AHESASC)
    // Also initialize ACR scrub tracker to 0
    g_scrubState.scrubTrack = 0;
#endif

    // Find the right ctx dma for us to use
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindCtxDma_HAL(pAcr, &(g_dmaProp.ctxDma)));

    return ACR_OK;
}
#endif

/*!
 * @brief ACR routine to program DMA base
 *
 * @param[in]  bIsImem     Is Source/destination IMEM?
 * @param[in]  pDmaProp    DMA properties to be used for this DMA operation
 */
void
acrProgramDmaBase_TU10X
(
    LwBool             bIsImem,
    PACR_DMA_PROP      pDmaProp
)
{
    // Set new DMA
    if (bIsImem)
    {
        falc_wspr(IMB,  (LwU32)LwU64_LO32(pDmaProp->wprBase));
        falc_wspr(IMB1, (LwU32)LwU64_HI32(pDmaProp->wprBase));
    }
    else
    {
        falc_wspr(DMB,  (LwU32)LwU64_LO32(pDmaProp->wprBase));
        falc_wspr(DMB1, (LwU32)LwU64_HI32(pDmaProp->wprBase));
    }
}

/*!
 * @brief Programs the region CFG in FBFI register.
 */
ACR_STATUS
acrlibProgramRegionCfg_TU10X
(
    PACR_FLCN_CONFIG  pFlcnCfg,
    LwBool            bUseCsb,
    LwU32             ctxDma,
    LwU32             regionID
)
{
    LwU32        data = 0;
    FLCN_REG_TGT tgt  = BAR0_FBIF;
    LwU32        addr = LW_PFALCON_FBIF_REGIONCFG;
    LwU32        mask = 0;

    mask   = ~(0xF << (ctxDma*4));

    if (pFlcnCfg && (!(pFlcnCfg->bFbifPresent)))
    {
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
        data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgAddr, data);
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
    }
    else
    {
        if (bUseCsb)
        {
            tgt  = CSB_FLCN;
#ifdef ACR_UNLOAD
            addr = LW_CPWR_FBIF_REGIONCFG;
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
            addr = LW_CSEC_FBIF_REGIONCFG;
#else
            ct_assert(0);
#endif
        }

        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, tgt, addr);
        data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, tgt, addr, data);
    }

    return ACR_OK;
}

/*!
 * @brief Finds CTX dma with TRANCFG setting mapping to 
 *        physical FB access. If not present, it will
 *        program offset 0 with physical FB access.
 *
 * @param[out] pCtxDma Pointer to hold the resulting ctxDma
 *
 * @return ACR_OK if ctxdma is found
 */
ACR_STATUS
acrFindCtxDma_TU10X
(
    LwU32        *pCtxDma
)
{
    //
    // CTX DMA is programmed for PMU but not for SEC2
    //
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    LwU32 data = 0;
    data = FLD_SET_DRF(_CSEC, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, 0);
    data = FLD_SET_DRF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
    ACR_REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER),data);
#endif
    *pCtxDma = LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER;
    return ACR_OK;
}

#ifndef ACR_UNLOAD
/*!
 * @brief Program FBIF to allow physical transactions.
 *        Incase of GR falcons, make appropriate writes
 */
ACR_STATUS
acrlibSetupCtxDma_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32            ctxDma,
    LwBool           bIsPhysical
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;
    LwU32      addr   = LW_CSEC_FBIF_TRANSCFG(ctxDma);;

    if (pFlcnCfg->bFbifPresent)
    {

        if (!(pFlcnCfg->bIsBoOwner))
        {
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma));
        }
        else
        {
            data =  acrlibFlcnRegRead_HAL(pAcrlib, 0, CSB_FLCN, addr);
        }

        data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, data);

        if (bIsPhysical)
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
        }
        else
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL, data);
        }

        if (!(pFlcnCfg->bIsBoOwner))
        {
            acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma), data);
        }
        else
        {
            acrlibFlcnRegWrite_HAL(pAcrlib, 0, CSB_FLCN, addr, data);
        }
    }
    else
    {
        if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
        {
            acrlibSetupFecsDmaThroughArbiter_HAL(pAcrlib, pFlcnCfg, bIsPhysical);
        }
    }

    return status;
}
#endif

/*!
 *
 * @brief Setup DMACTL
 */
void
acrlibSetupDmaCtl_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool bIsCtxRequired
)
{
    if (bIsCtxRequired)
    {
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x1);
    }
    else
    {
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x0);
    }
}

/*!
 * @brief Program ARB to allow physical transactions for FECS as it doesn't have FBIF.
 * Not required for GPCCS as it is priv loaded.
 */
ACR_STATUS
acrlibSetupFecsDmaThroughArbiter_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool           bIsPhysical
)
{
    LwU32 data;

    if (bIsPhysical)
    {
        data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR);
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR, _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
        ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR, data);
    }

    data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE);
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD, _PHYS_VID_MEM, data);

    if (bIsPhysical)
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _ON, data);
    }
    else
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _OFF, data);
    }

    ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, data);

    return ACR_OK;
}

