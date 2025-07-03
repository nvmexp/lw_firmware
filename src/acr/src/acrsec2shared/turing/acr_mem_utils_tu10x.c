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
 * @file: acr_mem_utils_tu10x.c
 * @brief Implementation for memory utility functions shared across ACR and SEC2 ucodes
 */

//
// Includes
//
#include "acr.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

/*!
 * Memset implementation
 * @param[in] pMem pointer to the memory 
 * @param[in] data value to be filled in the memory (Only 8 lower bits are used)
 * @param[in] sizeInBytes number of bytes to fill in pMem
 * 
 * @return pMem
 */
void *
acrlibMemset_TU10X(void *pMem, LwU32 data, LwU32 sizeInBytes)
{
    LwU8 *pDataC = (LwU8*) pMem;
    while(sizeInBytes--)
    {
        *pDataC++ = (LwU8)data;
    }
    return pMem;
}

/*!
 * Memcpy implementation
 * @param[in] pDst pointer to the destination memory 
 * @param[in] pSrc pointer to the source memory 
 * @param[in] sizeInBytes number of bytes to be copied
 * 
 * @return pDst
 */
void *
acrlibMemcpy_TU10X(void *pDst, void *pSrc, LwU32 sizeInBytes)
{

    LwU8       *pDstC = (LwU8 *) pDst;
    const LwU8 *pSrcC = (const LwU8 *) pSrc;
    while(sizeInBytes--)
    {
        *pDstC++ = *pSrcC++;
    }
    return pDst;
}
