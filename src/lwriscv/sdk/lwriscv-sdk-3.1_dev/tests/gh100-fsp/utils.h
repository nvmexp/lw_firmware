/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef UTILS_H
#define UTILS_H

void    swapEndianness(void* pOutData, void* pInData, const uint32_t size);
uint8_t memCompare(void *a1, void *a2, uint32_t size);

#endif  // UTILS_H
