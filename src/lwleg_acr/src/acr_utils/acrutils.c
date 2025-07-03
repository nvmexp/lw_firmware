/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrutils.c
 */

//
// Includes
//
#include "acr.h"

/*!
 * @brief ACR routine to memcpy with xor.
 *
 * @param[out] pDst        pointer to destination memory.
 * @param[in]  pSrc        pointer to source memory.
 * @param[in]  sizeInBytes size in bytes to process.
 */ 
void acrMemxor(void *pDst, const void *pSrc, LwU32 sizeInBytes)
{
    LwU8       *pDstC = (LwU8 *) pDst;
    const LwU8 *pSrcC = (const LwU8 *) pSrc;
    LwU32 i;

    for(i = sizeInBytes; i > 0U; i--)
    {
        *pDstC++ ^= ((*pSrcC++) & (0XFFU));
    }
}

/*!
 * @brief ACR memcpy routine
 *
 * @param[out] pDst        pointer to destination memory.
 * @param[in]  pSrc        pointer to source memory.
 * @param[in]  sizeInBytes size in bytes to be copied.
 */ 
void acrMemcpy(void *pDst, const void *pSrc, LwU32 sizeInBytes)
{
    LwU8       *pDstC = (LwU8 *) pDst;
    const LwU8 *pSrcC = (const LwU8 *) pSrc;
    LwU32 i;

    for(i = sizeInBytes; i > 0U; i--)
    {
        *pDstC++ = *pSrcC++;
    }
}

/*!
 * @brief sets all memory locations with value passed.
 *
 * @param[out] pData         Pointer to memory location to be set.
 * @param[in]  val           value to be set in memory.
 * @param[in]  sizeInBytes   size in bytes to be set.
 */
void acrMemset(void *pData, LwU8 val, LwU32 sizeInBytes)
{
    LwU8 *pDataC = (LwU8*) pData;
    LwU32 i;

    for(i = sizeInBytes; i > 0U; i--)
    {
        *pDataC++ = val;
    }
}

/*!
 * @brief Mimics standard memcmp for unaligned memory
 *
 * @param[in] pS1   memory Source1
 * @param[in] pS2   memory Source2
 * @param[in] size  Number of bytes to be compared
 *
 * @return ACR_OK If acrMemcmp passes.
 * @return ACR_ERROR_MEMCMP_FAILED If memory comparison fails.
 */
ACR_STATUS acrMemcmp(const void *pS1, const void *pS2, LwU32 size)
{
    const LwU8 *pSrc1 = (const LwU8 *)pS1;
    const LwU8 *pSrc2 = (const LwU8 *)pS2;
    ACR_STATUS status = ACR_OK;
    LwU32 i;

    for(i = size; i > 0U; i--)
    {
        if (*pSrc1 != *pSrc2)
        {
            status = ACR_ERROR_MEMCMP_FAILED;
        }
        pSrc1++;
        pSrc2++;
    }
    return status;
}


