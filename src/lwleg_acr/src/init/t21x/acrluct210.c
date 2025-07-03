/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrluct210.c
 */

//
// Includes
//
#include "acr.h"
#include "external/pmuifcmn.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "hwproject.h"
#include "acr_objacr.h"

#include "config/g_acr_private.h"

/*!
 * @brief ACR initialization routine
 *
 * Calls acrFindWprRegions to find WPR region with expected configuration and get its region id.\n
 * Calls acrProgramFalconSubWpr to restrict access to only PMU falcon for the full WPR.\n
 * Calls acrPopulateDMAParameters to populate DMA properties wprBase, ctxDma and RegionID\n
 * Calls acrCopyUcodesToWpr to copy non-wpr blob to WPR region in SYSMEM.\n
 * Calls acrBootstrapFalcons to load FECS/GPCCS falcons after successful authentication of their ucodes present in WPR blob.\n
 * Calls acrProgramFalconSubWpr to release access to PMU falcon for the full WPR as LS PMU sub regions are now set and falcon will exit HS next.\n
 *
 *   @global_START
 *   @global_{g_desc.ucodeBlobSize , Size of non-wpr blob as passed by lwgpu-rm}
 *   @global_END
 *
 * @return ACR_OK if all steps in initialization goes successful.\n
 * @return ACR_ERROR_ILWALID_REGION_ID If invalid region index is found i.e. region Id is greater than maximum supported regions.\n
 */ 
ACR_STATUS acrInit_T210(void)
{
    ACR_STATUS   status     = ACR_OK;
    LwU32        wprIndex   = 0;
#ifdef ACR_SUB_WPR
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
#endif // ACR_SUB_WPR

    // Get the falcon debug status
    g_bIsDebug = acrIsDebugModeEnabled_HAL(pAcr);

    if ((status = acrFindWprRegions_HAL(pAcr, &wprIndex)) != ACR_OK)
    {
        goto cleanup;
    }

    //
    // Setup entire WPR region as a PMU SUB WPR. This will restrict access to only PMU.
    // This restriction is later relaxed before setting up of LS falcons by acrSetupLSFalcons.
    //
#ifdef ACR_SUB_WPR
   pRegionProp         = &(g_desc.regions.regionProps[0U]);
#ifdef ACR_SAFE_BUILD
    if ((status = acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_MAX, (pRegionProp->startAddress),
                (pRegionProp->endAddress), ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3)) != ACR_OK)
    {
        goto cleanup;
    }
#else
    if ((status = acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_MAX, (pRegionProp->startAddress),
                (pRegionProp->endAddress), ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_L2_AND_L3)) != ACR_OK)
    {
        goto cleanup;
    }
#endif
#endif // ACR_SUB_WPR

    //
    // Bootstrap the falcon only if it has valid WPR region.
    // A valid region should have a index less than the maximum supported regions.
    //
    if (wprIndex >= LW_MMU_WRITE_PROTECTION_REGIONS)
    {
        status = ACR_ERROR_ILWALID_REGION_ID;
        goto cleanup;
    }

    status = acrPopulateDMAParameters_HAL(pAcr, wprIndex);
    if (status != ACR_OK)
    {
        goto cleanup;
    }

    if((status = acrCopyUcodesToWpr_HAL(pAcr)) != ACR_OK)
    {
        goto cleanup;
    }
#ifndef ACR_SAFE_BUILD
    if (g_desc.ucodeBlobSize == 0U)
    {
        if ((status = acrBootstrapValidatedUcode()) != ACR_OK)
        {
            goto cleanup;
        }
    }
    else
#endif
    {
        if ((status = acrBootstrapFalcons_HAL(pAcr)) != ACR_OK)
        {
            goto cleanup;
        }
    }
#ifdef ACR_SUB_WPR
#ifdef ACR_SAFE_BUILD
    if ((status = acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_MAX, (pRegionProp->startAddress),
                (pRegionProp->endAddress), ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED)) != ACR_OK)
    {
        goto cleanup;
    }
#endif
#endif

    acrSetupFbhubDecodeTrap_HAL(void);

cleanup:
    return status;
}

