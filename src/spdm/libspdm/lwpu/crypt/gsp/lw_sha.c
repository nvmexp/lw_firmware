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

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_sha.c
 * @brief  File that provides SHA and HMAC-SHA functionality for libSPDM,
 *         utilizing SEC2 implementations. Only the SHA functionality which
 *         are required for APM SPDM Responder have been implemented.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwtypes.h"
#include "library/memlib.h"
#include "library/cryptlib.h"
#include "liblwriscv/sha.h"
#include "dev_gsp_prgnlcl.h"


/* ------------------------  and Defines ----------------------------- */
//
// Mutex IDs for various usecases.
// Defined here in an attempt to synchronize and prevent overlap.
//
#define SHA_MUTEX_ID_SPDM_SHA  0x01
#define SHA_MUTEX_ID_SPDM_HMAC 0x02


/**
  Computes the SHA message digest of a input data buffer.

  This function performs the SHA operation on  message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data         Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size    size of data buffer in bytes.
  @param[in]   algoId       SHA algo id
  @param[out]  hash_value   Pointer to a buffer that receives the SHA digest
                            value.

  @retval TRUE   SHA digest computation succeeded.
  @retval FALSE  SHA digest computation failed.
  @retval FALSE  This interface is not supported.

**/
static boolean  GCC_ATTRIB_SECTION("imem_libspdm", "_sha_hash_all")
_sha_hash_all(IN const void *data, IN uintn data_size, IN SHA_ALGO_ID algoId,
            OUT uint8 *hash_value)
{
    LWRV_STATUS     status     = LWRV_OK;
    LwU64           dataAddr   = (LwU64)data;
    SHA_CONTEXT     shaContext;
    SHA_TASK_CONFIG shaTaskCfg;

    if (data == NULL || data_size == 0 || hash_value == NULL)
    {
        return FALSE;
    }

    zero_mem(&shaContext, sizeof(shaContext));
    shaContext.algoId     = algoId;
    shaContext.msgSize    = data_size;
    shaContext.pBufOut    = hash_value;
   
    status =  shaGetHashSizeByte(algoId, &shaContext.bufSize);
    if (status != LWRV_OK)
    {
        return false;
    }
    shaContext.mutexToken = SHA_MUTEX_ID_SPDM_SHA;

    zero_mem(&shaTaskCfg, sizeof(shaTaskCfg));
    shaTaskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    shaTaskCfg.bDefaultHashIv = LW_TRUE;
    shaTaskCfg.dmaIdx         = 0;
    shaTaskCfg.size           = data_size;
    shaTaskCfg.addr           = (LwU64) dataAddr;

    status = shaRunSingleTaskCommon(&shaTaskCfg, &shaContext);
    return (status == LWRV_OK);
}

/**
  Computes the HMAC digest of a input data buffer.

  This function performs the HMAC-SHA digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE;.

  @param[in]   data        Pointer to the buffer containing the data to be digested.
  @param[in]   data_size    size of data buffer in bytes.
  @param[in]   key         Pointer to the user-supplied key.
  @param[in]   key_size     key size in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the HMAC-SHA digest
                           value.

  @retval TRUE   HMAC-SHA digest computation succeeded.
  @retval FALSE  HMAC-SHA digest computation failed.
  @retval FALSE  This interface is not supported.

**/
static boolean GCC_ATTRIB_SECTION("imem_libspdm", "_hmac_sha_all")
_hmac_sha_all(IN const void *data, IN uintn data_size,
            IN const uint8 *key, IN uintn key_size, IN SHA_ALGO_ID algoId,
            OUT uint8 *hmac_value)
{
    LWRV_STATUS     status = LWRV_OK;
    LwU64           dataAddr   = (LwU64)data;
    HMAC_CONTEXT    hmacContext;
    SHA_TASK_CONFIG hmacTaskCfg;
    LwU32           hmacBlockSizeByte;

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
    hmacContext.shaContext.algoId     = algoId;
    hmacContext.shaContext.msgSize    = data_size;
    hmacContext.shaContext.pBufOut    = hmac_value;

    status =  shaGetHashSizeByte(algoId, &hmacContext.shaContext.bufSize);
    if (status != LWRV_OK)
    {
        return false;
    }

    status =  shaGetBlockSizeByte(algoId, &hmacBlockSizeByte);
    if (status != LWRV_OK)
    {
        return false;
    }

    hmacContext.shaContext.mutexToken = SHA_MUTEX_ID_SPDM_HMAC;

    // Use hash of key if larger than block size.
    if (key_size > hmacBlockSizeByte)
    {
        LwU64       keyAddr        = (LwU64)key;
        SHA_CONTEXT keyShaContext  = {0};
        SHA_TASK_CONFIG keyTaskCfg = {0};

        status =  shaGetHashSizeByte(algoId, &hmacContext.keySize);
        if (status != LWRV_OK)
        {
            return false;
        }

        keyShaContext.algoId      = algoId;
        keyShaContext.msgSize     = key_size;
        keyShaContext.pBufOut     = &(hmacContext.keyBuffer[0]);
        status =  shaGetHashSizeByte(algoId, &keyShaContext.bufSize);
        if (status != LWRV_OK)
        {
            return false;
        }
        keyShaContext.mutexToken  = SHA_MUTEX_ID_SPDM_HMAC;

        keyTaskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
        keyTaskCfg.bDefaultHashIv = LW_TRUE;
        keyTaskCfg.dmaIdx         = 0;
        keyTaskCfg.size           = key_size;
        keyTaskCfg.addr           = (LwU64)keyAddr;        
        
        status = shaRunSingleTaskCommon(&keyTaskCfg, &keyShaContext);
        if (status != LWRV_OK)
        {
            return FALSE;
        }
    }
    else
    {
        // Otherwise, copy directly into HMAC key buffer.
        copy_mem(&(hmacContext.keyBuffer), key, key_size);
        hmacContext.keySize = key_size;
    }

    zero_mem(&hmacTaskCfg, sizeof(hmacTaskCfg));
    hmacTaskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    //
    // bDefaultHashIv needs to be LW_FALSE this is requirement from shaHmacRunSingleTaskCommon requirement
    // Please check sha.h for the explanation
    //
    hmacTaskCfg.bDefaultHashIv = LW_FALSE;
    hmacTaskCfg.dmaIdx         = 0;
    hmacTaskCfg.size           = data_size;
    hmacTaskCfg.addr           = (LwU64) dataAddr;

    status = shaHmacRunSingleTaskCommon(&hmacContext, &hmacTaskCfg);
    return (status == LWRV_OK);
}

/* ------------------------- Public Functions ------------------------------ */

/**
  Computes the SHA-256 message digest of a input data buffer.

  This function performs the SHA-256 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size    size of data buffer in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the SHA-256 digest
                           value (32 bytes).

  @retval TRUE   SHA-256 digest computation succeeded.
  @retval FALSE  SHA-256 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean sha256_hash_all(IN const void *data, IN uintn data_size,
            OUT uint8 *hash_value)
{
    return _sha_hash_all(data, data_size, SHA_ALGO_ID_SHA_256, hash_value);
}

/**
  Computes the HMAC-SHA256 digest of a input data buffer.

  This function performs the HMAC-SHA256 digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE;.

  @param[in]   data        Pointer to the buffer containing the data to be digested.
  @param[in]   data_size    size of data buffer in bytes.
  @param[in]   key         Pointer to the user-supplied key.
  @param[in]   key_size     key size in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the HMAC-SHA256 digest
                           value (32 bytes).

  @retval TRUE   HMAC-SHA256 digest computation succeeded.
  @retval FALSE  HMAC-SHA256 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean hmac_sha256_all(IN const void *data, IN uintn data_size,
            IN const uint8 *key, IN uintn key_size,
            OUT uint8 *hmac_value)
{
    return _hmac_sha_all(data, data_size, key, key_size, SHA_ALGO_ID_SHA_256, hmac_value);
}

/**
  Computes the SHA-384 message digest of a input data buffer.

  This function performs the SHA-384 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size    size of data buffer in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the SHA-384 digest
                           value (48 bytes).

  @retval TRUE   SHA-384 digest computation succeeded.
  @retval FALSE  SHA-384 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean sha384_hash_all(IN const void *data, IN uintn data_size,
            OUT uint8 *hash_value)
{
    return _sha_hash_all(data, data_size, SHA_ALGO_ID_SHA_384, hash_value);
}

/**
  Computes the HMAC-SHA384 digest of a input data buffer.

  This function performs the HMAC-SHA384 digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE;.

  @param[in]   data        Pointer to the buffer containing the data to be digested.
  @param[in]   data_size    size of data buffer in bytes.
  @param[in]   key         Pointer to the user-supplied key.
  @param[in]   key_size     key size in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the HMAC-SHA384 digest
                           value (48 bytes).

  @retval TRUE   HMAC-SHA384 digest computation succeeded.
  @retval FALSE  HMAC-SHA384 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean hmac_sha384_all(IN const void *data, IN uintn data_size,
            IN const uint8 *key, IN uintn key_size,
            OUT uint8 *hmac_value)
{

    // TODO: GSP-CC CONFCOMP-575 Implement SHA-384 functionality.
    return _hmac_sha_all(data, data_size, key, key_size, SHA_ALGO_ID_SHA_384, hmac_value);
}

