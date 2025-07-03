/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_dmat234.c
 */

//
// Includes
//
#include "acr.h"
#include "external/lwmisc.h"
#include "acr_objacrlib.h"
#include "g_acr_private.h"
#include "dev_pwr_csb.h"
#include "dev_falcon_v4.h"

/*!
 * @brief  Populates the DMA properties.
 *         Do sanity checks on wprRegionID/wprOffset.
 *         wprBase is populated using region properties.\n
 *         wprRegionID is populated using g_desc.\n
 *         g_scrubState.scrubTrack ACR scrub tracker is also initialized to 0x0.\n
 *         ctxDma is also populated by calling acrFindCtxDma_HAL.
 *
 * @param[in] wprRegIndex Index into pAcrRegions holding the wpr region.
 *
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If there is a size overflow. 
 * @return ACR_ERROR_NO_WPR If wprRegionID is 0x0 or wprOffset is not as expected.
 * @return ACR_OK           If DMA parameters are successfully populated.
 */
ACR_STATUS
acrPopulateDMAParameters_T234
(
    LwU32       wprRegIndex
)
{
    ACR_STATUS               status       = ACR_OK;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    //
    // Do sanity check on region ID and wpr offset.
    // If wprRegionID is not set, return as it shows that RM
    // didnt setup the WPR region. 
    // TODO: Do more sanity checks including read/write masks
    //
    if (g_desc.wprRegionID == 0U)
    {
        return ACR_ERROR_NO_WPR;
    } 

    if (g_desc.wprOffset != LSF_WPR_EXPECTED_OFFSET)
    {
        return ACR_ERROR_NO_WPR;
    }

    pRegionProp         = &(g_desc.regions.regionProps[wprRegIndex]);

    // startAddress is 32 bit integer and wprOffset is 256B aligned. Right shift wprOffset by 8 before adding to wprBase
    g_dmaProp.wprBase   = (((LwU64)pRegionProp->startAddress << 4)+ ((LwU64)g_desc.wprOffset >> 8));
    g_dmaProp.regionID  = g_desc.wprRegionID;

    // Also initialize ACR scrub tracker to 0
    g_scrubState.scrubTrack = 0;
 
    // Find the right ctx dma for us to use
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindCtxDma_HAL(pAcr, &(g_dmaProp.ctxDma)));

    // Setup pmu apertures for dma usage
    ACR_REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_UCODE),
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _TARGET, _NONCOHERENT_SYSMEM) |
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _MEM_TYPE, _PHYSICAL));
    ACR_REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_VIRT),
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _MEM_TYPE, _VIRTUAL));
    ACR_REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_VID),
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _TARGET, _NONCOHERENT_SYSMEM) |
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _MEM_TYPE, _PHYSICAL));
    ACR_REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_SYS_COH),
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _TARGET, _COHERENT_SYSMEM) |
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _MEM_TYPE, _PHYSICAL));
    ACR_REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_SYS_NCOH),
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _TARGET, _NONCOHERENT_SYSMEM) |
						DRF_DEF(_CPWR_FBIF, _TRANSCFG, _MEM_TYPE, _PHYSICAL));
    return ACR_OK;
}


