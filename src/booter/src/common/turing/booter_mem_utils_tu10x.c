/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_mem_utils_tu10x.c
 * @brief Implementation for memory utility functions
 */

//
// Includes
//
#include "booter.h"


/*!
 * Memset implementation
 * @param[in] pMem pointer to the memory 
 * @param[in] data value to be filled in the memory (Only 8 lower bits are used)
 * @param[in] sizeInBytes number of bytes to fill in pMem
 * 
 * @return pMem
 */
void *
booterMemset_TU10X(void *pMem, LwU32 data, LwU32 sizeInBytes)
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
booterMemcpy_TU10X(void *pDst, void *pSrc, LwU32 sizeInBytes)
{

    LwU8       *pDstC = (LwU8 *) pDst;
    const LwU8 *pSrcC = (const LwU8 *) pSrc;
    while(sizeInBytes--)
    {
        *pDstC++ = *pSrcC++;
    }
    return pDst;
}
