/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_helper_functions_ga10x.c
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

// Global variables
#ifdef NEW_WPR_BLOBS

#if !defined(BSI_LOCK)
LwU8    g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX]  ATTR_OVLY(".data") ATTR_ALIGNED(LSF_WPR_HEADER_ALIGNMENT);
#endif

#if !defined(BSI_LOCK) && !defined(ACR_UNLOAD) && !defined(ACR_UNLOAD_ON_SEC2)
LwU8    g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE]   ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);
#endif

// external variables
extern ACR_DMA_PROP       g_dmaProp;

/*!
 * @brief Read all WPR header wrappers from FB
 */
#if !defined(BSI_LOCK)
ACR_STATUS
acrReadAllWprHeaderWrappers_GA10X(void)
{
    LwU32  wprSize  = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER) * LSF_FALCON_ID_END),
                                   LSF_WPR_HEADER_ALIGNMENT);

   // Read the WPR header
   if ((acrIssueDma_HAL(pAcr, g_wprHeaderWrappers, LW_FALSE, 0, wprSize,
           ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != wprSize)
   {
       return ACR_ERROR_DMA_FAILURE;
   }

   return ACR_OK;
}
#endif

/*!
 * @brief Writes back all WPR header wrappers to FB
 */
#ifdef AHESASC
ACR_STATUS
acrWriteAllWprHeaderWrappers_GA10X(void)
{
    LwU32  wprSize  = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER) * LSF_FALCON_ID_END),
                                   LSF_WPR_HEADER_ALIGNMENT);

    // Write the WPR header
    if ((acrIssueDma_HAL(pAcr, g_wprHeaderWrappers, LW_FALSE, 0, wprSize,
            ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != wprSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}
#endif

/*!
 * @brief Reads LSB header wrapper into a global buffer then pointed out by pLsbHeaderWarpper
 */
#if !defined(BSI_LOCK) && !defined(ACR_UNLOAD) && !defined(ACR_UNLOAD_ON_SEC2)
ACR_STATUS
acrReadLsbHeaderWrapper_GA10X
(
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS status;
    LwU32      lsbSize = LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE;
    LwU32      lsbOffset;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                        pWprHeaderWrapper, (void *)&lsbOffset));

    // Read the LSB  header
    if ((acrIssueDma_HAL(pAcr, g_lsbHeaderWrapper, LW_FALSE, lsbOffset, lsbSize,
             ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != lsbSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_COPY_WRAPPER,
                                                                        (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper, (void *)pLsbHeaderWrapper));

    return ACR_OK;
}
#endif //!BSI_LOCK && !ACR_UNLOAD && !ACR_UNLOAD_ON_SEC2

#endif // NEW_WPR_BLOBS

