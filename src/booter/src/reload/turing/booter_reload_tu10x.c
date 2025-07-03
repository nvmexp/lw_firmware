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
 * @file: booter_reload_tu10x.c
 */
//
// Includes
//
#include "booter.h"

extern BOOTER_DMA_PROP g_dmaProp[BOOTER_DMA_CONTEXT_COUNT];
extern GspFwWprMeta g_gspFwWprMeta;

/*!
 * @brief Main function to do Booter reload process
 */
BOOTER_STATUS booterReload_TU10X(void)
{
    BOOTER_STATUS status;

    // Check handoffs with other Booters / ACR
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckBooterHandoffsInSelwreScratch_HAL(pBooter));

    booterInitializeDma_HAL(pBooter);
    
    // Look for WprMeta structure at the start of WPR2
    LwU64 wpr2Start = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO));
    wpr2Start = (wpr2Start << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);

    LwU64 wpr2End   = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI));
    wpr2End = (wpr2End << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT) + BOOTER_WPR_ALIGNMENT;

    if (wpr2End < wpr2Start)
    {
        // WPR2 is not up!
        return BOOTER_ERROR_NO_WPR;
    }

    GspFwWprMeta *pWprMeta = &g_gspFwWprMeta;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterReadGspRmWprHeader_HAL(pBooter, pWprMeta, wpr2Start, &g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT]));

    // Sanity check WprMeta structure is intact
    if (pWprMeta->magic != GSP_FW_WPR_META_MAGIC ||
        pWprMeta->revision != GSP_FW_WPR_META_REVISION ||
        pWprMeta->verified != GSP_FW_WPR_META_VERIFIED)
    {
        return BOOTER_ERROR_ILWALID_WPRMETA_MAGIC_OR_REVISION;
    }

    // Sanity check WprMeta says WPR2 is where it actually is
    if (pWprMeta->gspFwWprStart != wpr2Start ||
        pWprMeta->gspFwWprEnd != wpr2End)
    {
        return BOOTER_ERROR_ILWALID_WPRMETA_WPR2_REGION;
    }

    // Prepare GSP RISC-V for GSP-RM reload
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterPrepareGspRm_HAL(pBooter, pWprMeta));

    // Set handoff bit for indicating Booter Reload has completed successfully
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterWriteBooterHandoffsToSelwreScratch_HAL(pBooter));

    // Now, start GSP-RM from WPR2
    booterStartGspRm_HAL(pBooter);

    return status;
}

