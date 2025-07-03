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
 * @file: acr_command_wpr.c
 */

#include "acr.h"
#include "acr_objacr.h"
#include "config/g_acr_private.h"
#include "config/g_hal_register.h"
#include <liblwriscv/print.h>
#include <partitions.h>
#include "hwproject.h"
#include <liblwriscv/scp.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>

extern RM_FLCN_ACR_DESC g_desc;

/*!
 * @brief Top level function to handle LOCK_WPR command
 * This function calls other functions to execute the command.
 */
ACR_STATUS
acrCmdLockWpr(LwU64 *pUnsafeAcrDesc, LwU32 *failingEngine)
{
    ACR_STATUS   status        = ACR_OK;
    ACR_STATUS   statusCleanup = ACR_OK;
    LwU32        wprIndex      = 0xFFFFFFFF;

    if (pUnsafeAcrDesc == NULL)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

    // 
    // ACR descriptor received from RM can be changed during ACR exelwtion.
    // So copy it in local buffer before consumption.
    // RM provides physical FB offset. FB is mapped to RISCV Global MEM1 aperture.
    // So to acess the memory from RISCV, LW_RISCV_AMAP_FBGPA_START is added which is FBPA aperture offset.
    //
    acrMemcpy_HAL(pAcr, (void *)&g_desc, (void *)((LwU64)LW_RISCV_AMAP_FBGPA_START + (LwU64)pUnsafeAcrDesc), sizeof(RM_FLCN_ACR_DESC));

    // MMU registers setup to configure WPR and sub-WPRs
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLockWpr1_HAL(pAcr, &wprIndex));

    // Valid WPR region index must be less than maximum supported regions.
    if (wprIndex >= LW_MMU_WRITE_PROTECTION_REGIONS)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_REGION_PROP_ENTRY_NOT_FOUND);
    }

    // DMA configuration
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrPopulateDMAParameters_HAL(pAcr, wprIndex));

    // 
    // Copy WPR blob from non-protected to WPR region so that it 
    // gets encrypted through HUB encryption.
    //
#ifndef ACR_FMODEL_BUILD
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrShadowCopyOfWpr_HAL(pAcr, wprIndex));
#endif

    // LS signature validation
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrValidateSignatureAndScrubUnusedWprExt_HAL(pAcr, failingEngine, wprIndex));

    // Decrypt LS ucodes
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrDecryptAndWriteToWpr_HAL(pAcr, failingEngine));

Cleanup:
    // Drop ACR access to WPR1 at end of LOCK_WPR command
    statusCleanup = acrConfigureSubWprForAcr_HAL(pAcr, LW_FALSE);

    return (status != ACR_OK ? status : statusCleanup);    
}
