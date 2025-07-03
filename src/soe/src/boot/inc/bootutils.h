/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOT_UTILS_H
#define BOOT_UTILS_H

//
// The new SVEC field definition is ((num_blks << 24) | (e << 17) | (s << 16) | tag_val)
// assuming size is always multiple of 256, then (size << 16) equals (num_blks << 24)
//
#define SVEC_REG(base, size, s, e)  (((base) >> 8) | (((size) | (s)) << 16) | ((e) << 17))

void memClear(void *pStart, LwU32 sizeInBytes);

void* memCopy(void *pDst, const void *pSrc, LwU32 sizeInBytes);

#endif // BOOT_UTILS_H
