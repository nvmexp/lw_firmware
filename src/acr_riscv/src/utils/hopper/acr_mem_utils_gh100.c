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
 * @file: acr_mem_utils_gh100.c
 * @brief Implementation for memory utility functions shared across ACR and SEC2 ucodes
 */

//
// Includes
//
#include "acr.h"

#include "config/g_acr_private.h"

/*!
 * Memset implementation
 * @param[in] pMem pointer to the memory 
 * @param[in] data value to be filled in the memory (Only 8 lower bits are used)
 * @param[in] sizeInBytes number of bytes to fill in pMem
 * 
 * @return pMem
 */
void *
acrMemset_GH100(void *pMem, LwU32 data, LwU32 sizeInBytes)
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
acrMemcpy_GH100(void *pDst, void *pSrc, LwU32 sizeInBytes)
{

    LwU8       *pDstC = (LwU8 *) pDst;
    const LwU8 *pSrcC = (const LwU8 *) pSrc;
    while(sizeInBytes--)
    {
        *pDstC++ = *pSrcC++;
    }
    return pDst;
}

/*!
 * acrMemsetByte
 *
 * @brief    Mimics standard memset for unaligned memory
 *
 * @param[in] pAddr   Memory address to set
 * @param[in] size    Number of bytes to set
 * @param[in] data    byte-value to fill memory
 *
 * @return void
 */
void acrMemsetByte
(
   void *pAddr,
   LwU32 size,
   LwU8  data
)
{
    LwU32  i;
    LwU8  *pBufByte = (LwU8 *)pAddr;

    if (pBufByte != NULL)
    {
        for (i = 0; i < size; i++)
        {
            pBufByte[i] = data;
        }
    }
}

