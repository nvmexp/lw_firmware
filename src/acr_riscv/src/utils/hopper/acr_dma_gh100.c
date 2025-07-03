/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_dma_gh100.c 
 */
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "dev_graphics_nobundle.h"
#include "dev_smcarb_addendum.h"
#include "hwproject.h"
#include "dev_fb.h"
#include "dev_fbif_v4.h"

#include "config/g_acr_private.h"

#include <liblwriscv/print.h>
#include <liblwriscv/dma.h>
#include <liblwriscv/fbif.h>
#include <lwriscv/fence.h>

// Extern Variables 
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;

#define ACR_DEFAULT_ALIGNMENT           (0x20000)

/*!
 * @brief ACR routine to do riscv FB DMA to and from IMEM/DMEM.
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
ACR_STATUS
acrIssueDma_GH100
(
    void               *pOff,
    LwBool             bIsImem,
    LwU32              fbOffset,
    LwU32              sizeInBytes,
    ACR_DMA_DIRECTION  dmaDirection,
    ACR_DMA_SYNC_TYPE  dmaSync,
    PACR_DMA_PROP      pDmaProp
)
{
    LwU32  dmaRet     = LWRV_ERR_GENERIC;
    LwBool bReadExt   = LW_FALSE;
    LwBool bIsWpr     = LW_TRUE;
    LwU64  tcmOffset  = (LwU64)pOff;
    LwU32  addrSpace  = ADDR_FBMEM;
    ACR_STATUS status = ACR_OK;

    // 
    // [NOTE] : We have skipped the step of Programming the DMA base like it was done previously in 
    // falcon architecture. The DMA functionality in RISCV requires the direct physical address. 
    // So we add the offset (received as the parameter), and the wpr base for the respective DMA_PROP 
    // object and then pass the result to the dmaPa function.
    //

    if (pOff == NULL || pDmaProp == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    
    // Check the required direction of DMA transaction
    switch (dmaDirection)
    {
        case ACR_DMA_TO_FB:
        case ACR_DMA_TO_SYS:
            bReadExt = LW_FALSE;
            break;
        case ACR_DMA_FROM_FB:
        case ACR_DMA_FROM_SYS:
            bReadExt = LW_TRUE;
            break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Check if the region is a non-WPR region
    if (pDmaProp->regionID == LSF_UNPROTECTED_REGION_ID)
    {
        bIsWpr = LW_FALSE;
    }

    if ((dmaDirection == ACR_DMA_FROM_SYS) || (dmaDirection == ACR_DMA_TO_SYS))
    {
        addrSpace = ADDR_SYSMEM;
    }

    // Setup the aperture that is to be used for DMA transaction.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupApertureCfg_HAL(pAcr, (LwU8)pDmaProp->regionID, (LwU8)pDmaProp->ctxDma, addrSpace, bIsWpr));

    //
    // Here the DMA operation is triggered. The wprBase address that is present in the pDmaProp is a 256 Byte
    // aligned address. But the dmaPa function operates on the physical addresses, and thus needs that as
    // an argument. So we Lshift the wprBase address by 8 bits, so that it is colwerted into physical address.
    //

    dmaRet = dmaPa(tcmOffset, (((pDmaProp->wprBase) << 8) + fbOffset) , (LwU64)sizeInBytes, (LwU8)pDmaProp->ctxDma, bReadExt);
    if (dmaRet != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Fence function is ilwoked to wait for all the external transactions to complete.
    riscvFenceRWIO();

    return ACR_OK;
}

/*!
 * @brief ACR routine to setup the FBIF Aperture Configuration.
 *
 * @param[in]  regionID   WPR Region ID
 * @param[in]  bIsWpr     Is the region a valid WPR Region?
 *
 * @return ACR_OK                If Success
 *         ACR_ERROR_DMA_FAILURE If the fbif Configuration fails
 */
ACR_STATUS
acrSetupApertureCfg_GH100
(   
    LwU8   regionID,
    LwU8   ctxDma,
    LwU32  addrSpace,
    LwBool bIsWpr
)
{
    FBIF_APERTURE_CFG acrApertCfg = {0};
    FBIF_TRANSCFG_TARGET  target;
    if (addrSpace == ADDR_SYSMEM)
    {
        if (ctxDma == RM_GSP_DMAIDX_PHYS_SYS_COH_FN0)   
        {    
            target = FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM;
        }
        else
        {
            target = FBIF_TRANSCFG_TARGET_NONCOHERENT_SYSTEM;    
        } 
    }
    else if (addrSpace == ADDR_FBMEM)
    {
       target = FBIF_TRANSCFG_TARGET_LOCAL_FB;
    }
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    acrApertCfg.apertureIdx             = ctxDma;
    acrApertCfg.target                  = target;
    acrApertCfg.bTargetVa               = LW_FALSE;
    acrApertCfg.l2cWr                   = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL;
    acrApertCfg.l2cRd                   = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL;
    acrApertCfg.fbifTranscfWachk0Enable = LW_FALSE;
    acrApertCfg.fbifTranscfWachk1Enable = LW_FALSE;
    acrApertCfg.fbifTranscfRachk0Enable = LW_FALSE;
    acrApertCfg.fbifTranscfRachk1Enable = LW_FALSE;
    acrApertCfg.regionid                = (uint8_t)regionID;

    if (fbifConfigureAperture(&acrApertCfg, 1) != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}



/*!
 * @brief  Populates the DMA properties including ctxDma and RegionID
 *
 * @param[in] wprRegIndex Index into pAcrRegions holding the wpr region
 */
ACR_STATUS
acrPopulateDMAParameters_GH100
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
    if (!(g_desc.wprRegionID == LSF_WPR_EXPECTED_REGION_ID))
    {
        return ACR_ERROR_NO_WPR;
    }

    if (g_desc.wprOffset != LSF_WPR_EXPECTED_OFFSET)
    {
        return ACR_ERROR_NO_WPR;
    }

    pRegionProp         = &(g_desc.regions.regionProps[wprRegIndex]);

    //
    // Not every ACR variants have valid g_desc.
    // For UNLOAD variant, PopulateDMAParameters_GH100 is still be exelwted but g_desc is invalid.
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

    // Find the right ctx dma for us to use
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindCtxDma_HAL(pAcr, &(g_dmaProp.ctxDma)));

    return ACR_OK;
}

/*!
 * @brief Programs the region CFG in FBFI register.
 */
ACR_STATUS
acrProgramRegionCfg_GH100
(
    PACR_FLCN_CONFIG  pFlcnCfg,
    LwU32             ctxDma,
    LwU32             regionID
)
{
    ACR_STATUS   status = ACR_OK;
    LwU32        data   = 0;
    FLCN_REG_TGT tgt    = BAR0_FBIF;
    LwU32        addr   = LW_PFALCON_FBIF_REGIONCFG;
    LwU32        mask   = 0;

    mask   = ~(0xFU << (ctxDma*4));

    if (pFlcnCfg ==  NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (!(pFlcnCfg->bFbifPresent))
    {
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
        data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgAddr, data);
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, tgt, addr, &data));
        data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, tgt, addr, data));
    }

    return status;
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
acrFindCtxDma_GH100
(
    LwU32        *pCtxDma
)
{
    //
    // CTX DMA is programmed for PMU but not for SEC2
    //
    LwU32 data = 0;

    data = FLD_SET_DRF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, 0);
    data = FLD_SET_DRF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FBIF_TRANSCFG(LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER),data);

    *pCtxDma = LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER;
    return ACR_OK;
}



/*!
 * @brief Program FBIF to allow physical transactions.
 *        Incase of GR falcons, make appropriate writes
 */
ACR_STATUS
acrSetupCtxDma_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32            ctxDma,
    LwBool           bIsPhysical
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;
    LwU32      addr   = LW_PRGNLCL_FBIF_TRANSCFG(ctxDma);
    if (pFlcnCfg->bFbifPresent)
    {

        if (!(pFlcnCfg->bIsBoOwner))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma), &data));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, 0, PRGNLCL_RISCV, addr, &data));
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
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma), data));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, 0, PRGNLCL_RISCV, addr, data));
        }
    }
    else
    {
        if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
        {
            acrSetupFecsDmaThroughArbiter_HAL(pAcr, pFlcnCfg, bIsPhysical);
        }
    }

    return status;
}

/*!
 *
 * @brief Setup DMACTL
 */
ACR_STATUS
acrSetupDmaCtl_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool bIsCtxRequired
)
{
    ACR_STATUS status = ACR_OK;
    if (bIsCtxRequired)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x1));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x0));
    }

    return status;
}


/*!
 * @brief Program ARB to allow physical transactions for FECS as it doesn't have FBIF.
 * Not required for GPCCS as it is priv loaded.
 * Ampere also has SMC support so program correct FECS instance if SMC is enabled.
 */
ACR_STATUS
acrSetupFecsDmaThroughArbiter_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool           bIsPhysical
)
{
    LwU32 data;
    LwU32 regFecsArbWpr;
    LwU32 regFecsCmdOvr;

    //
    // SEC always uses extended address space to bootload FECS-0 of a GR. And thus we also skip the check of
    // unprotected pric SMC_INFO which is vulnerable to attacks. Tracked in bug 2565457.
    //
    regFecsArbWpr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_WPR, pFlcnCfg->falconInstance);
    regFecsCmdOvr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, pFlcnCfg->falconInstance);

    if (bIsPhysical)
    {
        data = ACR_REG_RD32(BAR0, regFecsArbWpr);
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR, _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
        ACR_REG_WR32(BAR0, regFecsArbWpr, data);
    }

    data = ACR_REG_RD32(BAR0, regFecsCmdOvr);
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD, _PHYS_VID_MEM, data);

    if (bIsPhysical)
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _ON, data);
    }
    else
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _OFF, data);
    }

    ACR_REG_WR32(BAR0, regFecsCmdOvr, data);

    return ACR_OK;
}
