/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_unload.c
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "mmu/mmucmn.h"
#include "dev_top.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_pwr_csb.h"
#include "dev_bus.h"
#include "acr.h"
#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

// Extern Variables
extern LwU8 g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX];

#ifdef NEW_WPR_BLOBS
extern LwU8 g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
#endif

/*!
 * @brief Function to De-initialize ACR states
 */
ACR_STATUS acrDeInit(void)
{
    ACR_STATUS      status       = ACR_OK;
    LwU32           index        = 0;
    LwU32           falconId;
    LwU32           lsbOffset;
#ifdef NEW_WPR_BLOBS
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper   = NULL;
#else
    PLSF_WPR_HEADER pWprHeader   = NULL;
#endif

    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPopulateDMAParameters_HAL(pAcr, LSF_WPR_EXPECTED_REGION_ID));

#ifdef NEW_WPR_BLOBS
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadAllWprHeaderWrappers_HAL(pAcr));
#else
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadWprHeader_HAL(pAcr));
#endif

    // Need to reset all LS falcons before tearing down WPR1, as it could be using WPR1 while we opened WPR1
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
#ifdef NEW_WPR_BLOBS
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            pWprHeaderWrapper, (void *)&lsbOffset));
#else
        pWprHeader = GET_WPR_HEADER(index);
        falconId   = pWprHeader->falconId;
        lsbOffset  = pWprHeader->lsbOffset;
#endif

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(falconId, lsbOffset))
        {
            break;
        }

#if ACR_GSP_RM_BUILD
        // Dont reset if falcon id is GSP in Pre-Hopper Authenticated GSP-RM Boot
        if (falconId == LSF_FALCON_ID_GSPLITE)
        {
            continue;
        }
#endif

        // Dont reset if falcon id is bootstrap owner
#if defined(ACR_UNLOAD_ON_SEC2)
        //
        // If ACR_UNLOAD is running on SEC2 then don't reset SEC2
        // We don't have a way yet to distinguish GC6/GCOFF
        // For GC6 entry, PMU is responsible for
        // 1. Wait for PCIE link to be L2
        // 2. If L2 is engaged, put FB into self refresh
        // 3. Trigger SCI/BSI to transfer control to island
        // So given PMU is having functional requirement in GC6 we cannot reset PMU
        // GCOFF behavior has to align as we don't have a way to distinguish GC6/GCOFF
        //
        if ((falconId == LSF_FALCON_ID_SEC2) ||
            (falconId == LSF_FALCON_ID_PMU_RISCV))
        {
            if ((falconId == LSF_FALCON_ID_PMU_RISCV))
            {
                //
                // TODO: since on GA10X+ RISCV PMU shuts down in the non-GC6 case on detach
                // ("We don't have a way yet to distinguish GC6/GCOFF" no longer applies),
                // consider resetting PMU when LW_PPWR_RISCV_CPUCTL._HALTED !
                //

                if ((status = acrLockFalconDmaRegion_HAL(pAcr, falconId)) != ACR_OK)
                {
                    goto cleanup;
                }

                //
                // On GA10X, apply cleanup for the mining WAR.
                // This must be done AFTER acrLockFalconDmaRegion_HAL, since, if this fails,
                // PMU will no longer have access to WPR, and, thus, an attacker launching
                // ACR-unload early would have PMU fail on the next WPR access (e.g. page fault).
                //
                // It's not completely foolproof, since PMU doesn't access WPR 100% of the time,
                // so there are additional checks inside acrCleanupMiningWAR_HAL to make sure that
                // PMU has indeed legitimately detached (or shut down).
                //
                if ((status = acrCleanupMiningWAR_HAL(pAcr)) != ACR_OK)
                {
                    goto cleanup;
                }
            }
            continue;
        }
#else
        if (falconId == LSF_FALCON_ID_PMU)
        {
            continue;
        }
#endif

        // Reset the falcon
        if ((status = acrResetFalcon_HAL(pAcr, falconId)) != ACR_OK)
        {
            //
            // Safely continue incase particular falcon is not present on this chip
            // To explain further, acrResetFalcon calls acrlibGetFalconConfig_HAL which returns ACR_ERROR_FLCN_ID_NOT_FOUND
            // if the falcon is not supported by ACR
            //
            if (status == ACR_ERROR_FLCN_ID_NOT_FOUND)
            {
                // Restore status to ACR_OK for next loop run
                status = ACR_OK;
                continue;
            }
            else
            {
                goto cleanup;
            }
        }
    }

    if ((status = acrUnLockAcrRegions_HAL(pAcr)) != ACR_OK)
    {
        goto cleanup;
    }

    //Restore PLM for PTIMER register to allow access to all levels
    if ((status = acrProtectHostTimerRegisters_HAL(pAcr, LW_FALSE)) != ACR_OK)
    {
        goto cleanup;
    }

    if ((status = acrEnableMemoryLockRangeRowRemapWARBug2968134_HAL()) != ACR_OK)
    {
        goto cleanup;
    }

cleanup:
    return status;
}

