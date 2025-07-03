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
 * @file: acr_command_gsprm_boot.c
 */

#include "acr.h"
#include "acr_objacr.h"
#include "config/g_acr_private.h"
#include "config/g_hal_register.h"
#include <liblwriscv/print.h>
#include <partitions.h>
#include "hwproject.h"
#include <liblwriscv/scp.h>
#include <lwriscv/csr.h>
#include "mmu/mmucmn.h"

#ifdef RTS_ACR_CODE
#include "dev_se_seb.h"
#include <liblwriscv/io.h>
#endif // RTS_ACR_CODE

ACR_GSP_RM_DESC         g_gspRmDesc    ATTR_OVLY(".data")    ATTR_ALIGNED(ACR_DESC_ALIGN);
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;
extern LwU8             g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8             g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];

/*!
 * @brief Top level function to handle BOOT_GSP_RM command
 * This function calls other functions to execute the command.
 */
ACR_STATUS
acrCmdBootGspRm(ACR_GSP_RM_DESC *pGspRmDesc, LwU32 *failingEngine)
{
    ACR_STATUS status            = ACR_OK;
    ACR_STATUS statusCleanup     = ACR_OK;
    LwU32      wprIndex          = 0xFFFFFFFF;

    if ((pGspRmDesc == NULL) || (failingEngine == NULL))
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

    g_gspRmDesc = (ACR_GSP_RM_DESC)*pGspRmDesc;

    // Setup FRTS shared subWpr in MMU
#ifdef FB_FRTS
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSetupFrtsSubWprs_HAL(pAcr));
#endif

    // Read the GspFwWprMeta structure into local memory, to be used for setting up WPR2
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLoadGspRmMetaForWpr2_HAL(pAcr));

    // Allocate WPR2 at the end of FB for GSP-RM
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrAllocateWpr2ForGspRm_HAL(pAcr));

    // Lock WPR2 Region
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLockWpr2_HAL(pAcr, &wprIndex));  

    // Create the WPR2 header and LSB header for WPR2
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSetupWpr2ForGspRm_HAL(pAcr));

    // Copy GSP-RM from sysmem to WPR2
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrCopyGspRmToWpr2_HAL(pAcr));

    // Signature validation 
    // TODO-WD: temporarily disabled until GSP-RM signing is enabled and wired up
    // CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrValidateSignatureAndScrubUnusedWprExt_HAL(pAcr, failingEngine, wprIndex));

    // Create the WprMeta structure for GSP-RM
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSetupWpr2MemMapForGspRm_HAL(pAcr));

    // Setup SubWpr for WPR2
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSetupSubWprForWpr2_HAL(pAcr, failingEngine, wprIndex));    

    // Scrub Global Variables used in BOOT_GSP_RM command
    acrMemset_HAL(pAcr, (void *)&g_gspRmDesc, 0x0, sizeof(g_gspRmDesc));
    acrMemset_HAL(pAcr, (void *)&g_desc,      0x0, sizeof(RM_FLCN_ACR_DESC));
    acrMemset_HAL(pAcr, (void *)&g_dmaProp,   0x0, sizeof(ACR_DMA_PROP));
    acrMemset_HAL(pAcr, (void *)&g_lsbHeaderWrapper,  0x0,  LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE);
    acrMemset_HAL(pAcr, (void *)&g_wprHeaderWrappers, 0x0, LSF_WPR_HEADER_WRAPPER_SIZE_ALIGNED_BYTE);

    // Drop ACR access to WPR2 at end of BOOT_GSP_RM command
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_3_ACR_FULL_WPR2,
                                    ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

Cleanup:
    if (status != ACR_OK)
    {
        *failingEngine = LSF_FALCON_ID_GSP_RISCV;
    }

    // Drop ACR access to WPR2 at end of BOOT_GSP_RM command
    statusCleanup = acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_3_ACR_FULL_WPR2,
                                               ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE);

    return (status != ACR_OK ? status : statusCleanup);  
}

