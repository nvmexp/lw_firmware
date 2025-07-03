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
 * @file   lw_sha.c
 * @brief  File that provides SHA and HMAC-SHA functionality for libspdm,
 *         utilizing SEC2 implementations.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwuproc.h"
#include "lw_utils.h"

/* ------------------------- Application Includes --------------------------- */
#include "library/memlib.h"
#include "library/cryptlib.h"
#include "lib_intfcshahw.h"

/* ------------------------- Cryptlib.h Functions --------------------------- */
/**
  Computes the SHA-256 message digest of a input data buffer.

  This function performs the SHA-256 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]  data        Pointer to the buffer containing the data to be hashed.
  @param[in]  data_size   Size of data buffer in bytes.
  @param[out] hash_value  Pointer to a buffer that receives the SHA-256 digest
                          value (32 bytes).

  @retval TRUE   SHA-256 digest computation succeeded.
  @retval FALSE  SHA-256 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean
sha256_hash_all
(
    IN  const void *data,
    IN  uintn       data_size,
    OUT uint8      *hash_value
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU64       dataAddr   = (LwUPtr)data;
    LwU32       intStatus  = 0;
    LwU32       localDigest[SHA_256_HASH_SIZE_DWORD];
    SHA_CONTEXT shaContext;

    if (data == NULL || data_size == 0 || hash_value == NULL)
    {
        return FALSE;
    }

    zero_mem(localDigest, sizeof(localDigest));
    zero_mem(&shaContext, sizeof(shaContext));
    shaContext.algoId  = SHA_ID_SHA_256;
    shaContext.msgSize = data_size;
    shaContext.mutexId = SHA_MUTEX_ID_SPDM_SHA_256;

    //
    // Return immediately here and before we acquire mutex, as on
    // ErrorExit we will try and release.
    //
    if (shahwSoftReset() != FLCN_OK)
    {
        return FALSE;
    }

    if (shahwAcquireMutex(shaContext.mutexId) != FLCN_OK)
    {
        return FALSE;
    }

    CHECK_FLCN_STATUS(shahwOpInit(&shaContext));
    CHECK_FLCN_STATUS(shahwOpUpdate(&shaContext, data_size, LW_TRUE, dataAddr,
                                    SHA_SRC_TYPE_DMEM, 0, &intStatus, LW_TRUE));
    CHECK_FLCN_STATUS(shahwOpFinal(&shaContext, localDigest, LW_TRUE,
                                   &intStatus));

    copy_mem(hash_value, localDigest, SHA_256_HASH_SIZE_BYTE);

ErrorExit:
    // Release mutex, warn user if we fail.
    if (shahwReleaseMutex(shaContext.mutexId) != FLCN_OK)
    {
        return FALSE;
    }

    return FLCN_STATUS_TO_BOOLEAN(flcnStatus);
}

/**
  Computes the HMAC-SHA256 digest of a input data buffer.

  This function performs the HMAC-SHA256 digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE;.

  @param[in]  data        Pointer to the buffer containing the data to be digested.
  @param[in]  data_size   Size of data buffer in bytes.
  @param[in]  key         Pointer to the user-supplied key.
  @param[in]  key_size    Key size in bytes.
  @param[out] hash_value  Pointer to a buffer that receives the HMAC-SHA256 digest
                          value (32 bytes).

  @retval TRUE   HMAC-SHA256 digest computation succeeded.
  @retval FALSE  HMAC-SHA256 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean
hmac_sha256_all
(
    IN  const void  *data,
    IN  uintn        data_size,
    IN  const uint8 *key,
    IN  uintn        key_size,
    OUT uint8       *hmac_value
)
{
    FLCN_STATUS  flcnStatus = FLCN_OK;
    LwU64        dataAddr   = (LwUPtr)data;
    LwU32        intStatus  = 0;
    LwU32        tag[SHA_256_HASH_SIZE_DWORD];
    HMAC_CONTEXT hmacContext;

    if (data == NULL || data_size == 0 || key == NULL || key_size == 0 ||
        hmac_value == NULL)
    {
        return FALSE;
    }

    //
    // Prepate HMAC context. Wait to store key in context until after SHA HW
    // initialization, since we may need to take hash of key.
    //
    zero_mem(&hmacContext, sizeof(hmacContext));
    hmacContext.algoId  = SHA_ID_SHA_256;
    hmacContext.msgSize = data_size;
    hmacContext.mutexId = SHA_MUTEX_ID_SPDM_HMAC_256;

    //
    // Return immediately here and before we acquire mutex, as on
    // ErrorExit we will try and release.
    //
    if (shahwSoftReset() != FLCN_OK)
    {
        return FALSE;
    }

    if (shahwAcquireMutex(hmacContext.mutexId) != FLCN_OK)
    {
        return FALSE;
    }

    // Use hash of key if larger than block size.
    if (key_size > SHA_256_BLOCK_SIZE_BYTE)
    {
        LwU64       keyAddr = (LwUPtr)key;
        SHA_CONTEXT keyShaContext;

        zero_mem(&keyShaContext, sizeof(keyShaContext));
        keyShaContext.algoId  = SHA_ID_SHA_256;
        keyShaContext.msgSize = key_size;
        keyShaContext.mutexId = SHA_MUTEX_ID_SPDM_HMAC_256;

        CHECK_FLCN_STATUS(shahwOpInit(&keyShaContext));
        CHECK_FLCN_STATUS(shahwOpUpdate(&keyShaContext, key_size, LW_TRUE,
                                        keyAddr, SHA_SRC_TYPE_DMEM, 0,
                                        &intStatus, LW_TRUE));
        // Store the key hash in the HMAC key buffer.
        CHECK_FLCN_STATUS(shahwOpFinal(&keyShaContext,
                                       (LwU32 *)&(hmacContext.keyBuffer),
                                       LW_TRUE, &intStatus));
        hmacContext.keySize = SHA_256_HASH_SIZE_BYTE;
    }
    else
    {
        // Otherwise, copy directly into HMAC key buffer.
        copy_mem(&(hmacContext.keyBuffer), key, key_size);
        hmacContext.keySize = key_size;
    }

    // Callwlate HMAC of input data. Use temporary tag buffer for alignment.
    CHECK_FLCN_STATUS(shahwHmacSha256Init(&hmacContext, &intStatus));
    CHECK_FLCN_STATUS(shahwHmacSha256Update(&hmacContext, data_size, dataAddr,
                                            SHA_SRC_TYPE_DMEM, 0, &intStatus,
                                            LW_TRUE));
    CHECK_FLCN_STATUS(shahwHmacSha256Final(&hmacContext, tag, LW_TRUE,
                                           &intStatus));
    copy_mem(hmac_value, tag, sizeof(tag));

ErrorExit:
    // Release mutex, warn user if we fail.
    if (shahwReleaseMutex(hmacContext.mutexId) != FLCN_OK)
    {
        return FALSE;
    }

    return (flcnStatus == FLCN_OK);
}

/* ------------------------- Lw_crypt.h Functions --------------------------- */
/**
  Computes the SHA-1 message digest of a input data buffer.
  
  IMPORTANT NOTE: SHA-1 is considerered inselwre, and should not be use as a secure
  hash.   It is only included here for simple hashing uses, such as generating fields in
  a x509 certificate as dictated by standard.
  
  data must be not NULL, otherwise FALSE is returned.
  data_size must not be 0, otherwise FALSE is returned.
  hash_value must be not NULL, otherwise FALSE is returned.

  This function performs the SHA-1 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size    size of data buffer in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the SHA-256 digest
                           value (20 bytes).

  @retval TRUE   SHA-1 digest computation succeeded.
  @retval FALSE  SHA-1 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean
sha1_hash_all
(
    IN  const void *data,
    IN  uintn       data_size,
    OUT uint8      *hash_value
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU64       dataAddr   = (LwUPtr)data;
    LwU32       intStatus  = 0;
    LwU32       localDigest[SHA_1_HASH_SIZE_DWORD];
    SHA_CONTEXT shaContext;

    if (data == NULL || data_size == 0 || hash_value == NULL)
    {
        return FALSE;
    }

    zero_mem(localDigest, sizeof(localDigest));
    zero_mem(&shaContext, sizeof(shaContext));
    shaContext.algoId  = SHA_ID_SHA_1;
    shaContext.msgSize = data_size;
    shaContext.mutexId = SHA_MUTEX_ID_SPDM_SHA_256;

    //
    // Return immediately here and before we acquire mutex, as on
    // ErrorExit we will try and release.
    //
    if (shahwSoftReset() != FLCN_OK)
    {
        return FALSE;
    }

    if (shahwAcquireMutex(shaContext.mutexId) != FLCN_OK)
    {
        return FALSE;
    }

    CHECK_FLCN_STATUS(shahwOpInit(&shaContext));
    CHECK_FLCN_STATUS(shahwOpUpdate(&shaContext, data_size, LW_TRUE, dataAddr,
                                    SHA_SRC_TYPE_DMEM, 0, &intStatus, LW_TRUE));
    CHECK_FLCN_STATUS(shahwOpFinal(&shaContext, localDigest, LW_TRUE,
                                   &intStatus));

    copy_mem(hash_value, localDigest, SHA_1_HASH_SIZE_BYTE);

ErrorExit:
    // Release mutex, warn user if we fail.
    if (shahwReleaseMutex(shaContext.mutexId) != FLCN_OK)
    {
        return FALSE;
    }

    return (flcnStatus == FLCN_OK);
}
