/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- Application Includes --------------------------- */
#include "lib_sha1.h"
#include "lwSha1.h"

/* ------------------------- Prototypes ------------------------------------- */
static void
sha1Generate(LwU8 pHash[LW_SHA1_DIGEST_LENGTH], void *pData, LwU32 nBytes, Sha1CopyFunc copyFunc)
    GCC_ATTRIB_SECTION("imem_libSha1", "sha1Generate");
static void
_sha1Final(LwU8 *pDigest, Sha1Context *pContext)
    GCC_ATTRIB_SECTION("imem_libSha1", "_sha1Final");
 static LW_INLINE void
_sha1MemZero(LwU8 *pData, LwU32 nBytes)
    GCC_ATTRIB_SECTION("imem_libSha1", "_sha1MemZero");
static void
_sha1Update(Sha1Context *pContext, void *pData, LwU32 len, Sha1CopyFunc copyFunc)
    GCC_ATTRIB_SECTION("imem_libSha1", "_sha1Update");
static void
_sha1Initialize(Sha1Context *pContext)
    GCC_ATTRIB_SECTION("imem_libSha1", "_sha1Initialize");
static void
_sha1Transform(LwU32 *pState, LwU8  *pBuffer)
    GCC_ATTRIB_SECTION("imem_libSha1", "_sha1Transform");
/* ------------------------- SHA-1 implementation --------------------------- */

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
 *          outside the FLCN (e.g. system memory), so the function will never
 *          directly manipulate the source data.
 */
void libSha1
(
    LwU8             pHash[LW_FLCN_SHA1_DIGEST_SIZE_BYTES],
    void             *pData,
    LwU32            nBytes,
    FlcnSha1CopyFunc (copyFunc)
)
{
    sha1Generate(pHash, pData, nBytes, copyFunc);
}
