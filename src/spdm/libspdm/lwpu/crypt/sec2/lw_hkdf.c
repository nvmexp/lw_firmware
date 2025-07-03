/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored but uses libspdm code.

/*!
 * @file   lw_hkdf.c
 * @brief  File that provides HKDF functionality for libspdm, which wraps around
 *         our HMAC-SHA256 implementation in SEC2.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"

/* ------------------------- Application Includes --------------------------- */
#include "library/cryptlib.h"
#include "lib_intfcshahw.h"
#include "lw_crypt.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define HKDF_HMAC_BUFFER_MAX_SIZE (256)
#define HKDF_MAX_EXPAND_BLOCKS    (255)

/* ------------------------- Cryptlib.h Functions --------------------------- */
/**
  Derive SHA256 HMAC-based Expand key Derivation Function (HKDF).

  @param[in]  prk        Pointer to the user-supplied key.
  @param[in]  prk_size   key size in bytes.
  @param[in]  info       Pointer to the application specific info.
  @param[in]  info_size  info size in bytes.
  @param[out] out        Pointer to buffer to receive hkdf value.
  @param[in]  out_size   size of hkdf bytes to generate.

  @retval TRUE   Hkdf generated successfully.
  @retval FALSE  Hkdf generation failed.

**/
boolean
hkdf_sha256_expand
(
    IN  const uint8 *prk,
    IN  uintn        prk_size,
    IN  const uint8 *info,
    IN  uintn        info_size,
    OUT uint8       *out,
    IN  uintn        out_size
)
{
    LwU32 blocksNum;
    LwU8  tempHash[SHA_256_HASH_SIZE_BYTE];
    LwU8  hmacBuffer[HKDF_HMAC_BUFFER_MAX_SIZE];
    LwU32 hmacBufferSize;
    LwU32 i;

    if (prk == NULL || prk_size < SHA_256_HASH_SIZE_BYTE           ||
        out_size > HKDF_MAX_EXPAND_BLOCKS * SHA_256_HASH_SIZE_BYTE || 
        out == NULL)
    {
        return FALSE;
    }

    // Callwlate how many blocks needed to fit all data.
    blocksNum = (out_size + SHA_256_HASH_SIZE_BYTE - 1)/SHA_256_HASH_SIZE_BYTE;

    //
    // The message M that is passed into HMAC-SHA1(K, M) consists of:
    //
    // M = T(idx-1) || info || byte(idx)
    //
    // - T(idx-1) is the previous HMAC-SHA1 value and T(0) is empty
    // - info is the info data passed into HKDF-Expand
    // - idx is the index of the current block
    //

    // Previous HMAC value T
    hmacBufferSize = SHA_256_HASH_SIZE_BYTE;

    // Info field
    if (info)
    {
        hmacBufferSize += info_size;
    }

    // The index byte
    hmacBufferSize++;

    if (hmacBufferSize > HKDF_HMAC_BUFFER_MAX_SIZE)
    {
        return FALSE;
    }

    // Set the info part of T here because it is unchanged
    if (info)
    {
        copy_mem(hmacBuffer + SHA_256_HASH_SIZE_BYTE, info, info_size);
    }

    // The first block does not have the previous HMAC hash
    if (blocksNum > 0)
    {
        LwU32 block_size = (out_size > SHA_256_HASH_SIZE_BYTE) ?
                           SHA_256_HASH_SIZE_BYTE : out_size;
        out_size -= block_size;

        hmacBuffer[hmacBufferSize - 1] = 0x01;
        if (!hmac_sha256_all(hmacBuffer + SHA_256_HASH_SIZE_BYTE,
                             hmacBufferSize - SHA_256_HASH_SIZE_BYTE,
                             prk, prk_size, tempHash))
        {
            return FALSE;
        }

        copy_mem(out, tempHash, block_size);
        out = out + block_size;
    }

    for (i = 2; i <= blocksNum; i++)
    {
        LwU32 block_size = (out_size > SHA_256_HASH_SIZE_BYTE) ?
                           SHA_256_HASH_SIZE_BYTE : out_size;
        out_size -= block_size;

        copy_mem(hmacBuffer, tempHash, SHA_256_HASH_SIZE_BYTE);

        hmacBuffer[hmacBufferSize - 1] = i;
        if (!hmac_sha256_all(hmacBuffer, hmacBufferSize, prk,
                             prk_size, tempHash))
        {
            return FALSE;
        }

        copy_mem(out, tempHash, block_size);
        out = out + block_size;
    }

    return TRUE;
}
