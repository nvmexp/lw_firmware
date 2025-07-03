/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_command_unload.c
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"

#include "mmu/mmucmn.h"
#include "dev_top.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_pwr_csb.h"
#include "acr.h"
#include "config/g_acr_private.h"
#include "gsp/gspifacr.h"

// Extern Variables
extern LwU8 g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];

ACR_STATUS acrCmdUnlockWpr(void)
{
    ACR_STATUS      status       = ACR_OK;
    LwU32           index        = 0;
    LwU32           falconId;
    LwU32           lsbOffset;
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper   = NULL;

    // Need to reset all LS falcons before tearing down WPR1, as it could be using WPR1 while we opened WPR1
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            pWprHeaderWrapper, (void *)&lsbOffset));

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(falconId, lsbOffset))
        {
            break;
        }

        //
        // Dont reset if falcon id is bootstrap owner
        //
        if (falconId == LSF_FALCON_ID_GSP_RISCV)
        {
            continue;
        }

        // Reset all engine Instances for the engine ID  
        if ((status = acrResetAllEngineInstances_HAL(pAcr, falconId)) != ACR_OK)
        {
            //
            // Safely continue incase particular falcon is not present on this chip
            // To explain further, acrResetFalcon calls acrGetFalconConfig_HAL which returns ACR_ERROR_FLCN_ID_NOT_FOUND
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
                goto Cleanup;
            }
        }
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrUnlockWpr_HAL(pAcr, ACR_WPR1_REGION_IDX));

#ifndef ACR_GSPRM_BUILD
    //
    // TODO: remove this call once other profiles have been ported to use
    //       RM_GSP_ACR_CMD_ID_SHUTDOWN.
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrUnlockWpr_HAL(pAcr, ACR_WPR2_REGION_IDX));
#endif

Cleanup:
    return status;
}

ACR_STATUS acrCmdShutdown(void)
{
#ifdef ACR_GSPRM_BUILD
    //
    // We are unlocking the WPR that the GSP is running out of.
    // All GSP ucode exelwting from here on out should be resident in TCM,
    // and we cannot reset the engine because it us. The core will be halted
    // after this command handling finishes, so there is nothing else to do.
    //
    return acrUnlockWpr_HAL(pAcr, ACR_WPR2_REGION_IDX);
#else
    //
    // WPR2 has already been unlocked during acrCmdUnlockWpr.
    // TODO: remove this once other profiles have been ported to use
    //       RM_GSP_ACR_CMD_ID_SHUTDOWN.
    //
    return ACR_ERROR_NO_WPR;
#endif
}
