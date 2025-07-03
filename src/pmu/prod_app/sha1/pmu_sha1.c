/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "pmu_sha1.h"
#include "dbgprintf.h"

//
// The SHA-1 component shares code with CPU-based drivers. The shared code
// contains functions marked as inline that the PMU compiler does not actually
// inline (they aren't tagged with GCC_ATTRIB_ALWAYSINLINE or LW_FORCEINLINE).
// Therefore, we want to disable the following compile options for this file.
//
#pragma GCC diagnostic ignored "-Winline"

/* ------------------------- SHA-1 implementation --------------------------- */

#include "lwSha1.h"

PMU_CT_ASSERT(LW_PMU_SHA1_DIGEST_SIZE == LW_SHA1_DIGEST_LENGTH);

/*!
 * @brief   Generates the SHA-1 hash value on the data provided.
 *
 * The function does not manipulate the source data directly, as it may not
 * have direct access to it. Therefore, it relies upon the copy function to
 * copy segments of the data into a local buffer before any manipulation takes
 * place.
 *
 * @param[out]  pHash
 *          Pointer to store the hash array. The buffer must be 20 bytes in
 *          length, and the result is stored in big endian format.
 *
 * @param[in]   pData
 *          The source data array to transform. The actual values and make-up
 *          of this parameter are dependent on the copy function.
 *
 * @param[in]   nBytes
 *          The size, in bytes, of the source data.
 *
 * @param[in]   copyFunc
 *          The function responsible for copying data from the source for
 *          use by the sha1 function. It is possible for the data to exist
 *          outside the PMU (e.g. system memory), so the function will never
 *          directly manipulate the source data.
 */

void pmuSha1
(
    LwU8            pHash[LW_PMU_SHA1_DIGEST_SIZE],
    void           *pData,
    LwU32           nBytes,
    PmuSha1CopyFunc (copyFunc)
)
{
    sha1Generate(pHash, pData, nBytes, copyFunc);
}
