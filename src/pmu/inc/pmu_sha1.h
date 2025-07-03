/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#ifndef PMU_SHA1_H
#define PMU_SHA1_H

/*!
 * @file    sha1.h
 * @brief   Callwlates the SHA-1 hash value.
 *
 * The SHA-1 algorithm can be found at the National Institute of Standards and
 * Technology's website (http://www.itl.nist.gov/fipspubs/fip180-1.htm).
 */

#include "flcntypes.h"

#define LW_PMU_SHA1_DIGEST_SIZE 20

/*!
 * @brief Pointer to a memory accessor function for use by the SHA-1 hash
 * function.
 *
 * Due to memory constraints in the PMU/DPU, the data that needs to be
 * processed by the SHA-1 hash function may not be readily available. This
 * function is responsible for copying the data into a buffer to be used by
 * the SHA-1 function.
 *
 * @param[out]  pBuff   The buffer to copy the new data to.
 * @param[in]   index   The desired offset to begin copying from.
 * @param[in]   size    The requested number of bytes to be copied.
 * @param[in]   info    Pointer to the data passed into hdcpSha1 as pData.
 *
 * @return The actual number of bytes copied into the buffer.
 *
 * @sa hdcpSha1
 */

#define PmuSha1CopyFunc(fname) LwU32 (fname)(LwU8 *pBuff, LwU32 index, LwU32 size, \
                              void *pInfo)


/* Function Prototype */
void pmuSha1(LwU8 pHash[LW_PMU_SHA1_DIGEST_SIZE], void *pData, LwU32 nBytes,
             PmuSha1CopyFunc (copyFunc));


#endif //PMU_SHA1_H
