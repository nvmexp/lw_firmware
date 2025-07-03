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
 * @file: acr_command_gsprm_proxy_boot.c
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

ACR_GSP_RM_PROXY_DESC   g_gspRmProxyDesc    ATTR_OVLY(".dataEntry")    ATTR_ALIGNED(ACR_DESC_ALIGN);
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;
extern LwU8             g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8             g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];

/*!
 * @brief Top level function to handle BOOT_GSP_RM command
 * This function calls other functions to execute the command.
 */
ACR_STATUS
acrCmdBootGspRmProxy(ACR_GSP_RM_PROXY_DESC *pGspRmProxyDesc, LwU32 *failingEngine)
{
    ACR_STATUS status            = ACR_OK;
    ACR_STATUS statusCleanup     = ACR_OK;
    LwU32      wprIndex          = 0xFFFFFFFF;

    if ((pGspRmProxyDesc == NULL) || (failingEngine == NULL))
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

    g_gspRmProxyDesc = *pGspRmProxyDesc;

    // Setup FRTS shared subWpr in MMU
#ifdef FB_FRTS
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSetupFrtsSubWprs_HAL(pAcr));
#endif

    // Allocate WPR2 at the end of FB for GspRm-Proxy, we get sizeOfImage from descriptor of GSP-RM Proxy
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrAllocateWpr2ForGspRm_HAL(pAcr));
    
    // Lock WPR2 Region
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLockWpr2_HAL(pAcr, &wprIndex));  

    // Copy GspRm Proxy Image and Header from sysmem to WPR2
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrCopyGspRmToWpr2_HAL(pAcr));
   
    // Setup SubWprs for WPR2
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSetupSubWprForWpr2_HAL(pAcr, failingEngine, wprIndex));    

    // Signature validation
    // MK TODO: this did not work ever it seems, followup CL will fix sig verification.
    // JIRA COREUCODES-2126
    // CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrValidateSignatureAndScrubUnusedWprExt_HAL(pAcr, failingEngine, wprIndex));

    // Scrub Global Variables used in BOOT_GSP_RM command
    acrMemset_HAL(pAcr, (void *)&g_desc,    0x0, sizeof(RM_FLCN_ACR_DESC));
    acrMemset_HAL(pAcr, (void *)&g_dmaProp, 0x0, sizeof(ACR_DMA_PROP));
    acrMemset_HAL(pAcr, (void *)&g_lsbHeaderWrapper,  0x0,  LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE);
    acrMemset_HAL(pAcr, (void *)&g_wprHeaderWrappers, 0x0, LSF_WPR_HEADER_WRAPPER_SIZE_ALIGNED_BYTE);
    
Cleanup:
    // Drop ACR access to WPR2 at end of BOOT_GSP_RM command
    statusCleanup = acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_3_ACR_FULL_WPR2,
                                               ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE);

    return (status != ACR_OK ? status : statusCleanup);      
}

