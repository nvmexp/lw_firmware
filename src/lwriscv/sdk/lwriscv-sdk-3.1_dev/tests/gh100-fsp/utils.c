/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stddef.h>
#include <stdint.h>

#include "utils.h"

void swapEndianness(void* pOutData, void* pInData, const uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size / 2; i++)
    {
        uint8_t b1 = ((uint8_t*)pInData)[i];
        uint8_t b2 = ((uint8_t*)pInData)[size - 1 - i];
        ((uint8_t*)pOutData)[i]            = b2;
        ((uint8_t*)pOutData)[size - 1 - i] = b1;
    }
}

uint8_t memCompare(void *a1, void *a2, uint32_t size)
{
#define SUCCESS 0
#define ERROR   1
    uint8_t status         = SUCCESS;
    uint32_t idx;
    const uint8_t *pSrc1   = (const uint8_t *) a1;
    const uint8_t *pSrc2   = (const uint8_t *) a2;

    if ((pSrc1 != NULL) && (pSrc2 != NULL))
    {
        for(idx=0;idx<size;idx++)
        {
            if( pSrc1[idx] != pSrc2[idx] )
            {
                status = ERROR;
            }
        }
    }
    else
    {
        status = ERROR;
    }

    return status;
}

