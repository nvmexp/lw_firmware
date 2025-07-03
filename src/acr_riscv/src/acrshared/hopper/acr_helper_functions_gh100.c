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
 * @file: acr_helper_functions_gh100.c
 */
//
// Includes
//
/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"

/* ------------------------ Application Includes --------------------------- */
#include "mmu/mmucmn.h"
#include "dev_top.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "sec2mutexreservation.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_falcon_v4.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"
#include "config/g_acr_private.h"

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwU8         g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX]  ATTR_OVLY(".data") ATTR_ALIGNED(LSF_WPR_HEADER_ALIGNMENT);
LwU8         g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE] ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);
ACR_DMA_PROP g_dmaProp                                                    ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);


// Skipping this for GP10X BSI_LOCK through this define, as anyway BAR0 is not accessible to read mailbox and debug
/*!
 * @brief Write status into MAILBOX0
 * Pending: Clean-up after ACR_UNLOAD is moved to sec2 from PMU.
 * CL: https://lwcr.lwpu.com/sw/25021609/
 */
void
acrWriteStatusToFalconMailbox_GH100(ACR_STATUS status)
{
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_MAILBOX0, status);
}


/*
* @brief: Returns the HW state of MMU secure lock and if WPR is allowed with secure lock enabled
*/
ACR_STATUS
acrGetMmuSelwreLockStateFromHW_GH100
(
    LwBool *pbIsMmuSelwreLockEnabledInHW, 
    LwBool *pbIsWprAllowedWithSelwreLockInHW
)
{
    if (NULL == pbIsMmuSelwreLockEnabledInHW ||
        NULL == pbIsWprAllowedWithSelwreLockInHW)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    *pbIsMmuSelwreLockEnabledInHW     = FLD_TEST_DRF(_PFB, _PRI_MMU_WPR_MODE, _SELWRE_LOCK, _TRUE,
                                            ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_MODE));

    *pbIsWprAllowedWithSelwreLockInHW = FLD_TEST_DRF(_PFB, _PRI_MMU_WPR_MODE, _WPR_WITH_SELWRE_LOCK, _TRUE,
                                            ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_MODE));

    return ACR_OK;
}

/*!
 * @brief ACR routine to check if falcon is in DEBUG mode or not
 */
LwBool
acrIsDebugModeEnabled_GH100(void)
{
    LwU32   scpCtl = 0;

    scpCtl = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_SCP_CTL_STAT);
    return !FLD_TEST_DRF( _PRGNLCL, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
}

/*!
 * @brief Read all WPR header wrappers from FB
 */
ACR_STATUS
acrReadAllWprHeaderWrappers_GH100(LwU32 wprIndex)
{
    ACR_STATUS status  = ACR_OK;
    LwU32      wprSize;  

    if (wprIndex == ACR_WPR1_REGION_IDX)
    { 
        wprSize = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER) * LSF_FALCON_ID_END),
                               LSF_WPR_HEADER_ALIGNMENT);
    }
    else
    {
        wprSize = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER)),
                               LSF_WPR_HEADER_ALIGNMENT);        
    }

   // Read the WPR header
   CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, g_wprHeaderWrappers, LW_FALSE, 0, wprSize,
                                    ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

   return status;
}

/*!
 * @brief Writes back all WPR header wrappers to FB
 */
ACR_STATUS
acrWriteAllWprHeaderWrappers_GH100(LwU32 wprIndex)
{
    ACR_STATUS status  = ACR_OK;
    LwU32      wprSize;  

    if (wprIndex == ACR_WPR1_REGION_IDX)
    { 
        wprSize = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER) * LSF_FALCON_ID_END),
                               LSF_WPR_HEADER_ALIGNMENT);
    }
    else
    {
        wprSize = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER)),
                               LSF_WPR_HEADER_ALIGNMENT);        
    }

    // Write the WPR header
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, g_wprHeaderWrappers, LW_FALSE, 0, wprSize,
                                      ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

    return status;
}

/*!
 * @brief Reads LSB header wrapper into a global buffer then pointed out by pLsbHeaderWarpper
 */
ACR_STATUS
acrReadLsbHeaderWrapper_GH100
(
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS status  = ACR_OK;
    LwU32      lsbSize = LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE;
    LwU32      lsbOffset;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                        pWprHeaderWrapper, (void *)&lsbOffset));

    // Read the LSB  header
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, pLsbHeaderWrapper, LW_FALSE, lsbOffset, lsbSize,
                                                       ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

    return status;
}

